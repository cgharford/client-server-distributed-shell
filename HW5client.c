/* Christina Harford
 *
 * In accordance with the UNC Honor pledge, I certify that I have neither
 * given nor recieved unauthorized aid on this assignment.

 * This program is the client portion of a client-server distributed command
 * shell interpreter. The client has two positional parameters: (1) the DNS host
 * name where the server program is running, and (2) the port number of the
 * server's "welcoming" socket. This program reads input strings from stdin and
 * sends them to a server which parses the input and execs the file. The stdout
 * from the server is then returned to the client. The client then puts the
 * converted string to stdout. When an EOF is reached, the program terminates.
 */

// Standard includes
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>

// Custom includes
#include "Socket.h"

#define INPUT_LENGTH 1002

int main(int argc, char* argv[]) {
  int i, c, returnedValue;
  int count = 0;
  char inputString[INPUT_LENGTH];
  Socket connect_socket;

  // If there is not a valid number of arguments, print error and terminate
  if (argc < 3) {
      printf("No host and port\n");
      return -1;
  }
  // Connect to server at the host and port & creates a new data transfer socket
  connect_socket = Socket_new(argv[1], atoi(argv[2]));
  // If unable to connect to server, print error and terminate
  if (connect_socket < 0) {
     printf("Failed to connect to server\n");
     return -1;
  }

  // Get a string from stdin, service request and print out relevant output
  // Loop executes until EOF is reached
   while (1) {
      // Outputs '% ' and resets variables from previou proccess
      fprintf(stdout, "%% ");
      memset(inputString, 0, sizeof(inputString));

      // Reads in user input; If EOF is reached, terminate program
      if (fgets(inputString, INPUT_LENGTH, stdin) == NULL) {
        return 0;
      }

      // If input exeeds max, print error message and restart prompt
      if (strlen(inputString) > (INPUT_LENGTH - 2)) {
        printf("Cannot process more than 1000 characters of input\n");
        // Manually flush buffer via getchar
        while (getchar() != '\n');
        continue;
      }
      // count includes '\0'
      count = strlen(inputString) + 1;

      // Send input line to the server using the data transfer socket
      for (i = 0; i < count; i++) {
        c = inputString[i];
        returnedValue = Socket_putc(c, connect_socket);
        // If error occurs putting char in socket, terminate client
        if (returnedValue == EOF) {
          printf("Socket_putc EOF or error\n");
          Socket_close(connect_socket);
          return -1;
        }
      }

      // Get characters from the server using the data transfer socket
      // Expect server to return exactly the same number of characters sent in
      int errorChar = 0;
      while (1) {
        c = Socket_getc(connect_socket);
        // If error occurs getting charaacters from socket, terminate client
        if (c == EOF) {
          printf("Socket_getc EOF or error\n");
          Socket_close(connect_socket);
          return -1;
        }
        if (errorChar == 1) {
          break;
        }
        // If no error occurs, print output until NULL character is read
        else {
          if (c == '\0') {
            errorChar = 1;
          }
          else {
            printf("%c", c);
          }
        }
      }
  } // end of while loop; when ready for next prompt
  Socket_close(connect_socket);
  return 0;
}
