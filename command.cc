#include <cstdio>
#include <cstdlib>

#include <fcntl.h>
#include <cstring>

#include <iostream>
#include <unistd.h>
#include <sys/wait.h>

#include "command.hh"
#include "shell.hh"

char* last_arg;
extern int special_expansion;
int last_process;
int last_return;

Command::Command() {
    // Initialize a new vector of Simple Commands
    _simpleCommands = std::vector<SimpleCommand *>();

    _outFile = NULL;
    _inFile = NULL;
    _errFile = NULL;
    _background = false;
    _append = false;
}

void Command::insertSimpleCommand( SimpleCommand * simpleCommand ) {
    // add the simple command to the vector
    _simpleCommands.push_back(simpleCommand);
}

void Command::clear() {
    // deallocate all the simple commands in the command vector
    for (auto simpleCommand : _simpleCommands) {
        delete simpleCommand;
    }

    // remove all references to the simple commands we've deallocated
    // (basically just sets the size to 0)
    _simpleCommands.clear();

    if ( _outFile ) {
        if (_outFile != _errFile) {
          delete _outFile;
        }
    }
    _outFile = NULL;

    if ( _inFile ) {
        delete _inFile;
    }
    _inFile = NULL;

    if ( _errFile ) {
        delete _errFile;
    }
    _errFile = NULL;

    _background = false;
}

void Command::print() {
    printf("\n\n");
    printf("              COMMAND TABLE                \n");
    printf("\n");
    printf("  #   Simple Commands\n");
    printf("  --- ----------------------------------------------------------\n");

    int i = 0;
    // iterate over the simple commands and print them nicely
    for ( auto & simpleCommand : _simpleCommands ) {
        printf("  %-3d ", i++ );
        simpleCommand->print();
    }

    printf( "\n\n" );
    printf( "  Output       Input        Error        Background\n" );
    printf( "  ------------ ------------ ------------ ------------\n" );
    printf( "  %-12s %-12s %-12s %-12s\n",
            _outFile?_outFile->c_str():"default",
            _inFile?_inFile->c_str():"default",
            _errFile?_errFile->c_str():"default",
            _background?"YES":"NO");
    printf( "\n\n" );
}

void Command::execute() {
    // Don't do anything if there are no simple commands
    if ( _simpleCommands.size() == 0 ) {
        Shell::prompt();
        return;
    }

    // Print contents of Command data structure
    /*if (isatty(0)) {
      print();
    }*/

    // Add execution here
    // For every simple command fork a new process
    // Setup i/o redirection
    // and call exec

    if (strcmp(_simpleCommands[0]->_arguments[0]->c_str(), "exit") == 0) {
      if (isatty(0)) {
        printf("\nGood bye!!\n\n");
      }
      exit(0);
    }

    int tmpin = dup(0);
    int tmpout = dup(1);
    int tmperr = dup(2);

    int fdin;
    if (_inFile) {
      fdin = open(_inFile->c_str(), O_RDONLY);
    }
    else {
      fdin = dup(tmpin);
    }

    int ret;
    int fdout;
    int fderr;

    for (int i = 0; i < (int)_simpleCommands.size(); i++) {
      dup2(fdin, 0);
      close(fdin);

      if (i == (int)_simpleCommands.size() - 1) {
        if (_outFile) {
          if (!_append) {
            fdout = open(_outFile->c_str(), O_CREAT | O_RDWR, 00777);
          } else {
            fdout = open(_outFile->c_str(), O_APPEND | O_RDWR, 00777);
          }
        }
        else {
          fdout = dup(tmpout);
        }
        if (_errFile) {
          if (!_append) {
            fderr = open(_errFile->c_str(), O_CREAT | O_RDWR, 00777);
          } else {
            fderr = open(_errFile->c_str(), O_APPEND | O_RDWR, 00777);
          }
        }
        else {
          fderr = dup(tmpout);
        }

      }
      else {
        int fdpipe[2];
        pipe(fdpipe);
        fdout = fdpipe[1];
        fdin = fdpipe[0];
      }
      dup2(fdout, 1);
      dup2(fderr, 2);
      close(fdout);
      close(fderr);

      if (special_expansion == 1) {
        _simpleCommands[i]->insertArgument(new std::string(std::to_string(last_return)));
        special_expansion = 0;
      } else if (special_expansion == 2) {
        _simpleCommands[i]->insertArgument(new std::string(std::to_string(last_process)));
        special_expansion = 0;
      } else if (special_expansion == 3) {
        _simpleCommands[i]->insertArgument(new std::string(last_arg));
        special_expansion = 0;
      }

      if (strcmp(_simpleCommands[i]->_arguments[0]->c_str(), "setenv") == 0) {
        setenv(_simpleCommands[i]->_arguments[1]->c_str(), _simpleCommands[i]->_arguments[2]->c_str(), 1);
      } else if (strcmp(_simpleCommands[i]->_arguments[0]->c_str(), "unsetenv") == 0) {
        unsetenv(_simpleCommands[i]->_arguments[1]->c_str());
      } else if (strcmp(_simpleCommands[i]->_arguments[0]->c_str(), "cd") == 0) {
        if (_simpleCommands[i]->_arguments[1] != NULL) {
          if (chdir(_simpleCommands[i]->_arguments[1]->c_str()) != 0) {
            fprintf(stderr, "cd: can't cd to %s \n", _simpleCommands[i]->_arguments[1]->c_str());
          }
        } else {
          chdir(getenv("HOME"));
        }
      } else {

        ret = fork();
        if (ret == 0) {

          if (strcmp(_simpleCommands[i]->_arguments[0]->c_str(), "printenv") == 0) {
            for (int i = 0; environ[i] != 0; i++) {
              printf("%s\n", environ[i]);
            }
            exit(1);
          } else {

            const char* command = (_simpleCommands[i]->_arguments[0])->c_str();

            char** argv = new char*[(int)_simpleCommands[i]->_arguments.size() + 1];

            for (int j = 0; j < (int)_simpleCommands[i]->_arguments.size(); j++) {

              argv[j] = (char*)(_simpleCommands[i]->_arguments[j])->c_str();

            }

            argv[(int)_simpleCommands[i]->_arguments.size()] = NULL;

            execvp(command, argv);
            perror("execvp");
            exit(1);

          }
        }
      }
    }
    dup2(tmpin, 0);
    dup2(tmpout, 1);
    dup2(tmperr, 2);
    close(tmpin);
    close(tmpout);
    close(tmperr);

    int wstatus;
    if (!_background) {
      waitpid(ret, &wstatus, 0);
      if (WIFEXITED(wstatus)) {
        last_return = WEXITSTATUS(wstatus);
      }
    } else {
      last_process = ret;
    }

    free(last_arg);
    last_arg = NULL;
    last_arg = strdup(((_simpleCommands.back())->_arguments.back())->c_str());

    // Clear to prepare for next command
    clear();

    // Print new prompt
    Shell::prompt();
}

SimpleCommand * Command::_currentSimpleCommand;
