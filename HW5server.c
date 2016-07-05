/* Christina Harford
 *
 * In accordance with the UNC Honor pledge, I certify that I have neither
 * given nor recieved unauthorized aid on this assignment.

 * This program is the server portion of a client-server distributed command
 * shell interpreter. It is implemented using the "daemon-service" model. In
 * this model, multiple clients can be serviced concurrently by the service.
 * The server main process is a simple loop that accepts incoming socket
 * connections, and for each new connection established, uses fork() to create
 * a child process that is a new instance of the service process.  This child
 * process will provide the service for the single client program that
 * established the socket connection.
 *
 * Each new instance of the server process accepts input strings from its client
 * program, forks a new process, and redirects output to a file. The child
 * process parses the input string into arguments based on whitespace. The child
 * then validates the file by searching through all of the directories from the
 * PATH environment variable and calling stat() on each individual path. Once
 * stat() returns successfully, the child proccess then passes the file path
 * and arguments to execv(), which in turn executes the given file. Once the
 * child process is finished, it terminates. The parent (which was previously
 * waiting for the child process to terminate) then reads what was written by
 * the child from the file and puts it into the socket to be returned to the
 * client.
 *
 * Since the main process (the daemon) is intended to be continuously
 * available, it has no defined termination condition and must be
 * terminated by an external action (Ctrl-C or kill command).
 *
 * The server has one parameter: the port number that will be used
 * for the "welcoming" socket where the client makes a connection.
 */

// Standard includes
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <errno.h>
#include <stdarg.h>

// Custom includes
#include "Socket.h"

#define INPUT_LENGTH 1002
#define ARGUMENT_LENGTH 1001
#define MAX_TMP 100

// Global variables to hold socket descriptors
ServerSocket welcome_socket;
Socket connect_socket;

// Service method that exectes file and returns output to client
void commandLineService(void);

int main(int argc, char* argv[]) {
  pid_t spid, term_pid;
  int childStatus;
  bool forever = true;

  // If there is not a valid number of arguments, print error and terminate
  if (argc < 2) {
    printf("No port specified\n");
    return -1;
  }

  // Create a "welcoming" socket at the specified port
  welcome_socket = ServerSocket_new(atoi(argv[1]));
  if (welcome_socket < 0) {
    printf("Failed new server socket\n");
    return -1;
  }

  // The daemon infinite loop begins here; terminate with external action
  while (forever) {
    // Accept an incoming client connection & creates a new data transfer socket
    connect_socket = ServerSocket_accept(welcome_socket);
    // If unable to connect to client, print error and terminate
    if (connect_socket < 0) {
      printf("Failed accept on server socket\n");
      return -1;
    }
    // Create a child that will behave as the service process
    spid = fork();
    // If error on fork, print error message and terminate
    if (spid == -1) {
      perror("fork");
      return -1;
    }
    // Child process; services client
    if (spid == 0) {
      commandLineService();
      Socket_close(connect_socket);
      return 0;
    }
    // Parent/daemon process closes its connect socket
    else {
      Socket_close(connect_socket);
      // Reap a zombie every time through the loop, avoid blocking
      term_pid = waitpid(-1, &childStatus, WNOHANG);
    }
  }
}

// Server method that receives input from client, parses arguments and execs
// a file, and returns results to client via a socket
void commandLineService(void) {
  int i, j, k, c;
  int count = 0;
  int childPid, id;
  int status, errno;
  int argCounter, pathCounter;
  int sysCallReturn;
  const char whitespace[8] = " \t\v\n\f\n\r";
  const char colon[2] = ":";
  const char* path = getenv("PATH");
  char lineData[INPUT_LENGTH];
  char inputString[INPUT_LENGTH];
  char tempPath[strlen(path)];
  char *argv[ARGUMENT_LENGTH];
  char *inputPieces;
  char *pathPieces;
  char *pathArgs[strlen(path)];
  char *finalPathArgs[strlen(path)];
  unsigned char tmpName[MAX_TMP];
  unsigned char idStr[MAX_TMP];
  struct stat sb;
  bool forever = true;
  FILE *tmpFP;
  FILE *fp;

  // Will not use the server socket
  Socket_close(welcome_socket);

  // Loops until EOF on connect socket get a null-terminated string from the
  // client on the data transfer socket up to the maximim input line size.
  // Continue getting strings from the client until EOF on the socket.
  while (forever) {
    // Get characters from the client and put into string to parse
    for (i = 0; i < INPUT_LENGTH; i++) {
      c = Socket_getc(connect_socket);
      // If EOF, print out error and terminate
      if (c == EOF) {
        printf("Socket_getc EOF or error\n");
        return;
      }
      else {
        // Finish once we reach the null character
        if (c == '\0') {
          lineData[i] = c;
          break;
        }
        lineData[i] = c;
      }
    }
    // Be sure the string is terminated if max size reached
    if (i == INPUT_LENGTH)
    lineData[i-1] = '\0';

    // Get the parent process ID and use it to create a file name for the
    // temporary file with the format "tmpxxxxx" where xxxxx is the ID
    id = (int) getpid();
    sprintf(idStr, "%d", id);
    strcpy(tmpName,"tmp");
    strcat(tmpName, idStr);

    // Fork a child process
    childPid = fork();

    // Child process executes following code
    if (childPid == 0) {
      // Dynamically redirect stdout to named temporary file open for writing
      fp = freopen(tmpName, "w", stdout);
      fp = freopen(tmpName, "a", stderr);

      // Resets counters from previous child process
      argCounter = 0;
      pathCounter = 0;

      // Parses the input string into arguments based on whitespace
      inputPieces = strtok(lineData, whitespace);
      argv[argCounter] = inputPieces;

      // Parses the remainder of the string based on whitespace
      while(inputPieces != NULL ) {
        argCounter++;
        inputPieces = strtok(NULL, whitespace);
        argv[argCounter] = inputPieces;
      }

      // Convert PATH variable to a string to be parsed
      char pathString[strlen(path)];
      for (i = 0; i < strlen(path); i++) {
        pathString[i] = path[i];
      }

      // Parses the PATH environment variable based on colons
      pathPieces = strtok(pathString, colon);
      pathArgs[pathCounter] = pathPieces;

      // Parses the remainder of the PATH environment variable based on colons
      while(pathPieces != NULL ) {
        pathCounter++;
        pathPieces = strtok(NULL, colon);
        pathArgs[pathCounter] = pathPieces;
      }

      // Appends file to end of each path and calls stat() to validate file
      for (i = 0; i < pathCounter; i++) {
        memset(tempPath, 0, sizeof(tempPath));
        strcpy (tempPath, pathArgs[i]);
        strcat (tempPath, "/");
        strcat (tempPath, argv[0]);
        finalPathArgs[i] = tempPath;
        sysCallReturn = stat(finalPathArgs[i], &sb);
        // If stat() call successful, execv() with proper path and break out
        if (sysCallReturn == 0) {
          sysCallReturn = execv(finalPathArgs[i], argv);
          break;
        }
      }

      // If last system call is unsuccessful, print out error message
      if (sysCallReturn == -1) {
        printf("%s\n", strerror(errno));
      }
      // Return from child process
      return;
    }
    // If fork() error occurs, print out appropriate message to alert user
    else if (childPid == -1) {
      printf("%s\n", strerror(errno));
      break;
    }
    // Parent process executes following code
    else {
      // Waits for child process to terminate before executing
      // If wait() error occurs, print out appropriate message to alert user
      if ( waitpid(childPid, &status, 0) == -1) {
        printf("%s\n", strerror(errno));
      }
      // Open temporary file to read from; if error occurs, terminate
      if ((tmpFP = fopen (tmpName, "r")) == NULL) {
        fprintf (stderr, "error opening tmp file\n");
        exit -1;
      }
      // Loops until the file reaches an EOF
      while (!feof (tmpFP)) {
        int c;
        // Get characters from file and put them into the socket for client
        c = fgetc(tmpFP);
        // If we've reached EOF, signal this to child by passing null character
        if(feof(tmpFP)) {
          Socket_putc('\0', connect_socket);
          Socket_putc(WEXITSTATUS(status), connect_socket);
          // delete the temporary file
          remove(tmpName);
          break;
        }
        Socket_putc(c, connect_socket);
      }
    }
  }
  return;
}
