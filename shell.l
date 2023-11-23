%{

#include <cstring>
#include "y.tab.hh"
#include <sys/wait.h>
#include <pwd.h>
#include <unistd.h>

int special_expansion;
extern char* shellpath;
extern "C" char* read_line();

int mygetc(FILE* f) {
  static char* p;
  char ch;

  if (!isatty(0)) {
    return getc(f);
  }

  if (p == NULL || *p == 0) {
    char* s = read_line();
    p = s;
  }

  ch = *p;
  p++;

  return ch;
}

#undef getc
#define getc(f) mygetc(f)

static void yyunput (int c,char *buf_ptr  );

void myunputc(int c) {
  unput(c);
}

%}

%option noyywrap

%%

\n {
  return NEWLINE;
}

[ \t] {
  /* Discard spaces and tabs */
}

">" {
  return GREAT;
}

">&" {
  return GREATAMPERSAND;
}

"2>" {
  return STDERR;
}

">>" {
  return GREATGREAT;
}

">>&" {
  return GREATGREATAMPERSAND;
}

"&" {
  return AMPERSAND;
}

"|" {
  return PIPE;
}

"<" {
  return LESS;
}

"$("[^\t\n]*")" {

  char* clean_str = (char*)malloc(yyleng-1);
  for (int i = 2; yytext[i] != ')'; i++) {
    clean_str[i-2] = yytext[i];
  }
  clean_str[yyleng-3] = '\n';
  clean_str[yyleng-2] = 0;

  int pin[2];
  int pout[2];
  pipe(pin);
  pipe(pout);

  write(pin[1], clean_str, strlen(clean_str));
  write(pin[1], "exit\n", 6);
  close(pin[1]);

  int ret = fork();

  if (ret == 0) {

    dup2(pin[0], 0);
    dup2(pout[1], 1);

    char** argv = new char*[2];
    argv[0] = (char*)"/proc/self/exe";
    argv[1] = NULL;

    execvp("/proc/self/exe", argv);

  } else {

    close(pout[1]);
    close(pin[0]);

    char* buffer = (char*)malloc(YY_BUF_SIZE);

    read(pout[0], buffer, YY_BUF_SIZE);
    buffer[strlen(buffer)-1] = 0;

    for (int i = strlen(buffer)-1; i != -1; i--) {

      if (buffer[i] == '\n') {
        myunputc(' ');
      } else {
        myunputc(buffer[i]);
      }

    }
    close(pout[0]);
  }
}

\$\{[^ \t\n]*\} {

  special_expansion = 0;
  char* clean_str = (char*)malloc(yyleng-2);
  for (int i = 2; yytext[i] != '}'; i++) {
    clean_str[i-2] = yytext[i];
  }
  clean_str[yyleng-3] = 0;
  if (!strcmp(clean_str, "$")) {
    char* pid = (char*)malloc(50);
    sprintf(pid, "%d", getpid());
    yylval.cpp_string = new std::string(pid);
    return WORD;
  } else if (!strcmp(clean_str, "?")) {
    special_expansion = 1;
  } else if (!strcmp(clean_str, "!")) {
    special_expansion = 2;
  } else if (!strcmp(clean_str, "_")) {
    special_expansion = 3;
  } else if (!strcmp(clean_str, "SHELL")) {
    char* real_path = (char*)malloc(50);
    realpath(shellpath, real_path);
    yylval.cpp_string = new std::string(real_path);
    return WORD;
  } else {
    char* value = getenv(clean_str);
    yylval.cpp_string = new std::string(value);
    return WORD;
  }
}

"source "[^ "\t\n]* {
  char* filename = (char*)malloc(yyleng-6);
  int j = 0;
  for (int i = 7; i < yyleng; i++) {
    filename[j] = yytext[i];
    j++;
  }
  filename[j] = 0;

  yyin = fopen(filename, "r");
  yypush_buffer_state(yy_create_buffer(yyin, YY_BUF_SIZE));

  BEGIN(INITIAL);
}

[^ "\t\n][^ "\t\n]*  {
  /* Assume that file names have only alpha chars */

  if (yytext[0] == '~') {

    char* name = (char*)malloc(yyleng+1);
    int i = 1;
    int flag = 0;
    for (i; i < yyleng; i++) {
      if (yytext[i] == '/') {
        flag = 1;
        break;
      }
      name[i-1] = yytext[i];
    }

    char* dir = (char*)malloc(1000);

    struct passwd *pw;
    if (i > 1) {
      pw = getpwnam(name);
    } else {
      pw = getpwnam(getenv("USER"));
    }
    strcpy(dir, pw->pw_dir);

    int j = 0;
    while (dir[j] != 0) {
      j++;
    }
    for (i; i < yyleng; i++) {
      dir[j] = yytext[i];
      j++;
    }

    yylval.cpp_string = new std::string(dir);
    return WORD;

  } else if (strchr(yytext, '>') || strchr(yytext, '|')) {

    int i = 0;
    for (i = yyleng-1; yytext[i] != '>' && yytext[i] != '|'; i--) {
      myunputc(yytext[i]);
    }
    myunputc(' ');
    myunputc(yytext[i]);
    i--;
    myunputc(' ');
    for (int j = i; j >= 0; j--) {
      myunputc(yytext[i]);
      i--;
    }

  } else {

    char* clean_str = (char*)malloc(yyleng+1);
    int j = 0;
    for (int i = 0; i < yyleng; i++) {
      if (yytext[i] == 92) {
        if (yytext[i+1] == 92) {
          clean_str[j] = yytext[i];
          i++;
        } else {
          clean_str[j] = yytext[++i];
        }
      } else {
        clean_str[j] = yytext[i];
      }
      j++;
    }
    clean_str[yyleng] = 0;
    yylval.cpp_string = new std::string(clean_str);
    return WORD;

  }
}

\"[^\n\"]*\" {
  char* clean_str = (char*)malloc(yyleng-1);
  for (int i = 1; yytext[i] != '"'; i++) {
    clean_str[i-1] = yytext[i];
  }
  clean_str[yyleng-2] = 0;
  yylval.cpp_string = new std::string(clean_str);
  return WORD;
}

<<EOF>> {
  yypop_buffer_state();
  if (!YY_CURRENT_BUFFER) {
    yyterminate();
  }
}
