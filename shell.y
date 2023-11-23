%code requires 
{
#include <string>

#if __cplusplus > 199711L
#define register      // Deprecated in C++11 so remove the keyword
#endif
}

%union
{
  char        *string_val;
  // Example of using a c++ type in yacc
  std::string *cpp_string;
}

%token <cpp_string> WORD
%token NOTOKEN GREAT GREATAMPERSAND NEWLINE GREATGREAT GREATGREATAMPERSAND AMPERSAND PIPE LESS STDERR

%{
//#define yylex yylex
#include <cstdio>
#include "shell.hh"
#include <cstring>
#include <regex.h>
#include <dirent.h>
#include <cassert>
#include <algorithm>

#define MAXFILENAME 1024

void yyerror(const char * s);
int yylex();

int nEntries;
int maxEntries;
char** array;
int hasdot;

int compare(const void* a, const void* b) {
  return strcmp(*(const char**)a, *(const char**)b);
}

void expandWildcard(char* prefix, char* suffix) {

  if (suffix[0] == 0) {
    if (nEntries == maxEntries) {
      maxEntries *= 2;
      array = (char**)realloc(array, maxEntries*sizeof(char*));
      assert(array != NULL);
    }
    array[nEntries] = strdup(prefix);
    nEntries++;
    return;
  }

  char* s = strchr(suffix, '/');
  char component[MAXFILENAME];
  if (s != NULL) {
    strncpy(component, suffix, s - suffix);
    suffix = s + 1;
  } else {
    strcpy(component, suffix);
    suffix = suffix + strlen(suffix);
  }

  char newPrefix[MAXFILENAME];

  if (component[0] < 32) {
    expandWildcard(prefix, suffix);
    return;
  }

  if (!strchr(component, '*') && !strchr(component, '?')) {
    sprintf(newPrefix, "%s/%s", prefix, component);
    expandWildcard(newPrefix, suffix);
    return;
  }

  char* reg = (char*)malloc(2*strlen(component)+10);
  char* a = (char*)component;
  char* r = reg;
  *r = '^';
  r++;
  while (*a) {
    if (*a == '*') {
      *r = '.';
      r++;
      *r = '*';
      r++;
    } else if (*a == '?') {
      *r = '.';
      r++;
    } else if (*a == '.') {
      *r = '\\';
      r++;
      *r = '.';
      r++;
    } else {
      *r = *a;
      r++;
    }
    a++;
    }
  *r = '$';
  r++;
  *r = 0;

  regex_t re;
  int expbuf = regcomp(&re, reg, REG_EXTENDED | REG_NOSUB);
  if (expbuf) {
    perror("compile");
    return;
  }

  if (!strcmp(prefix, "")) {
    DIR* d = opendir("/");
    if (!d) {
      return;
    }

    struct dirent* ent;
    regmatch_t match;
    ent = readdir(d);

    while (ent != NULL) {
      if (regexec(&re, ent->d_name, 1, &match, 0) == 0) {
        sprintf(newPrefix, "%s/%s", prefix, ent->d_name);
        expandWildcard(newPrefix, suffix);
      }
      ent = readdir(d);
    }
    closedir(d);

  } else {
    DIR* d = opendir(prefix);
    if (!d) {
      return;
    }

    struct dirent* ent;
    regmatch_t match;
    ent = readdir(d);

    while (ent != NULL) {
      if (regexec(&re, ent->d_name, 1, &match, 0) == 0) {
        sprintf(newPrefix, "%s/%s", prefix, ent->d_name);
        expandWildcard(newPrefix, suffix);
      }
      ent = readdir(d);
    }
    closedir(d);

  }
}

void expandWildcardsIfNecessary(std::string* arg) {
  if (!strchr(arg->c_str(), '*') && !strchr(arg->c_str(), '?')) {
    Command::_currentSimpleCommand->insertArgument(arg);
    return;
  } else {
    hasdot = 0;
    if (arg->c_str()[0] == '.') {
      hasdot = 1;
    }
    if (strchr(arg->c_str(), '/')) {
      nEntries = 0;
      maxEntries = 20;
      array = (char**)malloc(maxEntries*sizeof(char*));
      expandWildcard((char*)"", (char*)arg->c_str());
      qsort(array, nEntries, sizeof(char*), compare);
      if (array[0] == NULL) {
        Command::_currentSimpleCommand->insertArgument(arg);
        return;
      }
      for (int i = 0; i < nEntries; i++) {
        if (array[i][0] == '.' && hasdot) {
          Command::_currentSimpleCommand->insertArgument(new std::string(array[i]));
        } else if (array[i][strlen(array[i])-1] != '.' && array[i][0] != '.') {
          Command::_currentSimpleCommand->insertArgument(new std::string(array[i]));
        }
      }
      free(array);
      array = NULL;
      return;
    }

    char* reg = (char*)malloc(2*strlen(arg->c_str())+10);
    char* a = (char*)arg->c_str();
    char* r = reg;
    *r = '^';
    r++;
    while (*a) {
      if (*a == '*') {
        *r = '.';
        r++;
        *r = '*';
        r++;
      } else if (*a == '?') {
        *r = '.';
        r++;
      } else if (*a == '.') {
        *r = '\\';
        r++;
        *r = '.';
        r++;
      } else {
        *r = *a;
        r++;
      }
      a++;
    }
    *r = '$';
    r++;
    *r = 0;

    regex_t re;
    int expbuf = regcomp(&re, reg, REG_EXTENDED | REG_NOSUB);
    if (expbuf) {
      perror("compile");
      return;
    }

    DIR* dir = opendir(".");
    if (dir == NULL) {
      perror("opendir");
      return;
    }
    struct dirent* ent;
    regmatch_t match;

    maxEntries = 20;
    nEntries = 0;
    array = (char**)malloc(maxEntries*sizeof(char*));

    ent = readdir(dir);
    while (ent != NULL) {
      if (regexec(&re, ent->d_name, 1, &match, 0) == 0) {
        if (nEntries == maxEntries) {
          maxEntries *= 2;
          array = (char**)realloc(array, maxEntries*sizeof(char*));
          assert(array != NULL);
        }
        array[nEntries] = strdup(ent->d_name);
        nEntries++;
      }
      ent = readdir(dir);
    }
    closedir(dir);
    qsort(array, nEntries, sizeof(char*), compare);
    for (int i = 0; i < nEntries; i++) {
      if (array[i][0] == '.' && hasdot) {
        Command::_currentSimpleCommand->insertArgument(new std::string(array[i]));
      } else if (array[i][strlen(array[i])-1] != '.' && array[i][0] != '.') {
        Command::_currentSimpleCommand->insertArgument(new std::string(array[i]));
      }
      free(array[i]);
      array[i] = NULL;
    }
    free(array);
    array = NULL;
    free(reg);
    reg = NULL;
    regfree(&re);
  }
}

%}

%%

goal: command_list;

cmd_and_args:
  WORD {
    //printf("Yacc: insert command \"%s\"\n", $1->c_str());
    Command::_currentSimpleCommand = new SimpleCommand();
    expandWildcardsIfNecessary($1);
  }
  arg_list {
    Shell::_currentCommand.
    insertSimpleCommand(Command::_currentSimpleCommand);
  }
;

arg_list:
  arg_list WORD {
    //printf("Yacc: insert argument \"%s\"\n", $2->c_str());
    //Command::_currentSimpleCommand->insertArgument($2);
    expandWildcardsIfNecessary($2);
  }
  | /* Empty string */
;

pipe_list:
  cmd_and_args
  | pipe_list PIPE cmd_and_args
;

io_modifier:
  GREATGREAT WORD {
    //printf("Yacc: insert output \"%s\"\n", $2->c_str());
    if (!Shell::_currentCommand._outFile) {
      Shell::_currentCommand._outFile = $2;
      Shell::_currentCommand._append = 1;
    } else {
      printf("Ambiguous output redirect.\n");
    }
  }
  | GREAT WORD {
    //printf("Yacc: insert output \"%s\"\n", $2->c_str());
    if (!Shell::_currentCommand._outFile) {
      Shell::_currentCommand._outFile = $2;
    } else {
      printf("Ambiguous output redirect.\n");
    }
  }
  | GREATGREATAMPERSAND WORD {
    //printf("Yacc: insert output \"%s\"\n", $2->c_str());
    if (!Shell::_currentCommand._outFile) {
      Shell::_currentCommand._outFile = $2;
      Shell::_currentCommand._errFile = $2;
      Shell::_currentCommand._append = 1;
    } else {
      printf("Ambiguous output redirect.\n");
    }
  }
  | GREATAMPERSAND WORD {
    //printf("Yacc: insert output \"%s\"\n", $2->c_str());
    if (!Shell::_currentCommand._outFile) {
      Shell::_currentCommand._outFile = $2;
      Shell::_currentCommand._errFile = $2;
    } else {
      printf("Ambiguous output redirect.\n");
    }
  }
  | LESS WORD {
    //printf("Yacc: insert input \"%s\"\n", $2->c_str());
    if (!Shell::_currentCommand._inFile) {
      Shell::_currentCommand._inFile = $2;
    } else {
      printf("Ambiguous output redirect.\n");
    }
  }
  | STDERR WORD {
    if (!Shell::_currentCommand._errFile) {
      Shell::_currentCommand._errFile = $2;
    } else {
      printf("Ambiguous output redirect.\n");
    }
  }
;

io_modifier_list:
  io_modifier_list io_modifier
  | /* Empty */
;

background_optional:
  AMPERSAND {
    Shell::_currentCommand._background = 1;
  }
  | /* Empty */
;

command_line:
  pipe_list io_modifier_list
  background_optional NEWLINE {
    //printf("Yacc: execute command\n");
    Shell::_currentCommand.execute();
  }
  | NEWLINE
  | error NEWLINE{yyerrok;}
;

command_list:
  command_line
  | command_list command_line
;

%%

void
yyerror(const char * s)
{
  fprintf(stderr,"%s", s);
}

#if 0
main()
{
  yyparse();
}
#endif
