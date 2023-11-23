#include <cstdio>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>

#include "shell.hh"

int yyparse(void);
char* shellpath;

void Shell::prompt() {
  if (isatty(0)) {
    printf("myshell>");
    fflush(stdout);
  }
}

void sighandler(int sig) {
  if (sig == 2) {
    printf("\nmyshell>");
    fflush(stdout);
  }
  if (sig == 17) {
    while(waitpid(-1, NULL, WNOHANG) > 0);
  }
}

int main(int argc, char** argv) {
  shellpath = argv[0];
  struct sigaction sa;
  sa.sa_handler = sighandler;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = SA_RESTART;

  if (sigaction(SIGINT, &sa, NULL)) {
  }
  if (sigaction(SIGCHLD, &sa, NULL)) {
    perror("sigaction");
    exit(-1);
  }

  Shell::prompt();
  yyparse();
}

Command Shell::_currentCommand;
