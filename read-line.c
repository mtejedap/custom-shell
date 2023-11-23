#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <termios.h>
#include <assert.h>

#define MAX_BUFFER_LINE 2048

extern void tty_raw_mode(void);

// Buffer where line is stored
int line_length;
char line_buffer[MAX_BUFFER_LINE];
char copy[MAX_BUFFER_LINE];

int history_index = 0;
int history_length = 0;
char** history;

int cursor;

void read_line_print_usage()
{
  char * usage = "\n"
    " ctrl-?       Print usage\n"
    " Backspace    Deletes last character\n"
    " up arrow     See last command in the history\n";

  write(1, usage, strlen(usage));
}

/* 
 * Input a line with some basic editing.
 */
char * read_line() {

  // Set terminal in raw mode
  struct termios tty_attr;
  struct termios old;
  tcgetattr(0,&tty_attr);
  old = tty_attr;

  /* Set raw mode. */
  tty_attr.c_lflag &= (~(ICANON|ECHO));
  tty_attr.c_cc[VTIME] = 0;
  tty_attr.c_cc[VMIN] = 1;

  tcsetattr(0,TCSANOW,&tty_attr);

  line_length = 0;
  cursor = 0;
  history_index = history_length;

  if (history == NULL) {
    history = (char**)malloc(sizeof(char*));
  }

  // Read one line until enter is typed
  while (1) {

    // Read one character in raw mode.
    char ch;
    read(0, &ch, 1);

    if (ch>=32 && ch != 127) {
      // It is a printable character.

      if (line_length == cursor) {
        // Do echo
        write(1,&ch,1);

        // If max number of character reached return.
        if (line_length==MAX_BUFFER_LINE-2) break;

        // add char to buffer.
        line_buffer[line_length]=ch;
        line_length++;
        cursor++;
      } else if (cursor < line_length) {

        int i = 0;
        if (line_length==MAX_BUFFER_LINE-2) break;
        for (i = 0; i < cursor; i++) {
          copy[i] = line_buffer[i];
        }
        copy[i] = ch;
        i++;
        for (int j = cursor; j < line_length; j++) {
          copy[i] = line_buffer[j];
          i++;
        }
        for (int k = 0; k < i; k++) {
          line_buffer[k] = copy[k];
        }
        //
        for (i=cursor; i < line_length; i++) {
          write(1, "\033[C", 3);
        }
        for (i =0; i < line_length; i++) {
          ch = 8;
          write(1,&ch,1);
        }
        for (i =0; i < line_length; i++) {
          ch = ' ';
          write(1,&ch,1);
        }
        for (i =0; i < line_length; i++) {
          ch = 8;
          write(1,&ch,1);
        }
        //
        line_length++;
        cursor++;
        write(1, line_buffer, line_length);
        for (i=cursor; i < line_length; i++) {
          write(1, "\033[D", 3);
        }
      }
    }
    else if (ch==10) {
      // <Enter> was typed. Return line

      // Add to history

      history[history_length] = (char*)malloc(line_length+1);
      strncpy(history[history_length], line_buffer, line_length);
      history[history_length][line_length] = 0;
      history_length++;
      history = (char**)realloc(history, (history_length+1)*sizeof(char*));
      assert(history != NULL);

      // Print newline
      write(1,&ch,1);

      break;
    }
    else if (ch == 31) {
      // ctrl-?
      read_line_print_usage();
      line_buffer[0]=0;
      break;
    }
    else if (ch == 8 || ch == 127) {
      // <backspace> was typed. Remove previous character read.
      if (cursor == line_length) {
        if (line_length > 0) {
          // Go back one character
          ch = 8;
          write(1,&ch,1);

          // Write a space to erase the last character read
          ch = 32;
          write(1,&ch,1);

          // Go back one character
          ch = 8;
          write(1,&ch,1);

          // Remove one character from buffer
          line_length--;
          cursor--;
        }
      } else if (cursor < line_length) {

        if (cursor > 0) {
          int i = 0;
          for (i = 0; i < cursor-1; i++) {
            copy[i] = line_buffer[i];
          }
          for (int j = cursor; j < line_length; j++) {
            copy[i] = line_buffer[j];
            i++;
          }
          for (int k = 0; k < i; k++) {
            line_buffer[k] = copy[k];
          }

          //
          for (i=cursor; i < line_length; i++) {
            write(1, "\033[C", 3);
          }
          for (i =0; i < line_length; i++) {
            ch = 8;
            write(1,&ch,1);
          }
          for (i =0; i < line_length; i++) {
            ch = ' ';
            write(1,&ch,1);
          }
          for (i =0; i < line_length; i++) {
            ch = 8;
            write(1,&ch,1);
          }
          //
          line_length--;
          cursor--;
          write(1, line_buffer, line_length);
          for (i=cursor; i < line_length; i++) {
            write(1, "\033[D", 3);
          }
        }
      }
    }
    else if (ch==1) {
      // Home key
      if (cursor != 0) {
        for (int i = 0; i < cursor; i++) {
          write(1, "\033[D", 3);
        }
        cursor = 0;
      }
    }
    else if (ch==5) {
      // End key
      for (int i = cursor; i < line_length; i++) {
        write(1, "\033[C", 3);
      }
      cursor = line_length;
    }
    else if (ch==4) {

      if (cursor < line_length && line_length > 0) {

        int i = 0;
        for (i = 0; i < cursor; i++) {
          copy[i] = line_buffer[i];
        }
        for (int j = cursor+1; j < line_length; j++) {
          copy[i] = line_buffer[j];
          i++;
        }
        for (int k = 0; k < i; k++) {
          line_buffer[k] = copy[k];
        }
        //
        for (i=cursor; i < line_length; i++) {
          write(1, "\033[C", 3);
        }
        for (i =0; i < line_length; i++) {
          ch = 8;
          write(1,&ch,1);
        }
        for (i =0; i < line_length; i++) {
          ch = ' ';
          write(1,&ch,1);
        }
        for (i =0; i < line_length; i++) {
          ch = 8;
          write(1,&ch,1);
        }
        //
        line_length--;
        write(1, line_buffer, line_length);
        for (i=cursor; i < line_length; i++) {
          write(1, "\033[D", 3);
        }
      }
    }
    else if (ch==27) {
      // Escape sequence. Read two chars more
      //
      // HINT: Use the program "keyboard-example" to
      // see the ascii code for the different chars typed.
      //
      char ch1;
      char ch2;
      read(0, &ch1, 1);
      read(0, &ch2, 1);
      if (ch1==91 && ch2==65) {
        // Up arrow. Print next line in history.

        if (history_index-1 >= 0) {
          history_index--;
        }
        // Erase old line
        // Print backspaces
        cursor = 0;
        int i = 0;
        for (i =0; i < line_length; i++) {
          ch = 8;
          write(1,&ch,1);
        }

        // Print spaces on top
        for (i =0; i < line_length; i++) {
          ch = ' ';
          write(1,&ch,1);
        }

        // Print backspaces
        for (i =0; i < line_length; i++) {
          ch = 8;
          write(1,&ch,1);
        }

        // Copy line from history

        strcpy(line_buffer, history[history_index]);
        line_length = strlen(line_buffer);

        // echo line
        write(1, line_buffer, line_length);
        cursor = line_length;
      } else if (ch1==91 && ch2==66) {
        // Down arrow
        // Erase old line
        // Print backspaces

        if (history_index < history_length-1) {
          history_index++;
        }

        cursor = 0;
        int i = 0;
        for (i =0; i < line_length; i++) {
          ch = 8;
          write(1,&ch,1);
        }

        // Print spaces on top
        for (i =0; i < line_length; i++) {
          ch = ' ';
          write(1,&ch,1);
        }

        // Print backspaces
        for (i =0; i < line_length; i++) {
          ch = 8;
          write(1,&ch,1);
        }

        // Copy line from history

        strcpy(line_buffer, history[history_index]);
        line_length = strlen(line_buffer);

        // echo line
        write(1, line_buffer, line_length);
        cursor = line_length;
      } else if (ch1==91 && ch2==68) {
        // Left arrow
        if (cursor > 0) {
          write(1, "\033[D", 3);
          cursor--;
        }
      } else if (ch1==91 && ch2==67) {
        // Right arrow
        if (cursor < line_length) {
          write(0, "\033[C", 3);
          cursor++;
        }
      }
    }
  }

  // Add eol and null char at the end of string
  line_buffer[line_length]=10;
  line_length++;
  line_buffer[line_length]=0;

  tcsetattr(0, TCSANOW, &old);
  return line_buffer;
}

