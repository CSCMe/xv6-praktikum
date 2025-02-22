// Webshell.

#include "user/user.h"
#include "user/shell/shell.h"
#include "kernel/fcntl.h"

int main(void) {
  static char buf[100];

  // Start a tcp server on port 23 (telnet)
  int port   = 23;
  printf("Starting telnet server on port %d...\n", port);
  int handle = net_bind(port);
  printf("Established connection\n");

  // Send welcome message
  char* welcome =
  "=============================\n"
  " ________________            \n"
  " < Happy Hacking! >          \n"
  " ----------------            \n"
  "       \\   ^__^             \n"
  "        \\  (oo)\\_______    \n"
  "           (__)\\       )\\/\\\n"
  "               ||----w |     \n"
  "               ||     ||     \n"
  "=============================\n";
  char* currentLineStart = welcome;
  char* currentCharacter = welcome;
  int lineLength = 0;
  while(*currentCharacter) {
    lineLength++;
    if (*currentCharacter == '\n') {
      net_send_listen(handle, currentLineStart, lineLength, NULL, 0);
      currentLineStart = currentCharacter + 1;
      lineLength = 0;
    }
    currentCharacter++;
  }

  // Read and run input commands.
  for(;;) {
    memset(buf, 0, sizeof(buf));

    // Receive a command
    char prompt[] = "$ ";
    net_send_listen(handle, prompt, sizeof(prompt), buf, sizeof(buf));

    printf("Got command: %s\n", buf);

    if (buf[0] == 'c' && buf[1] == 'd' && buf[2] == ' ') {
      // Chdir must be called by the parent, not the child.
      buf[strlen(buf) - 1] = 0; // chop \n
      if (chdir(buf + 3) < 0) fprintf(2, "cannot cd %s\n", buf + 3);
      continue;
    }

    // Telnet-shell specific: "quit" exits the webshell
    if (buf[0] == 'q' && buf[1] == 'u' && buf[2] == 'i' && buf[3] == 't') {
        net_unbind(handle);
        printf("Closed webshell\n");
        exit(0);
    }

    // Telnet hack: We don't want to print to stdout but our
    // tcp connection is not a file descriptor that we can pipe into.
    // Instead, we pipe into a utility program that writes all lines it receives
    // into a given net handle.
    uint buffer_len = strlen(buf);
    char* insert_location = (char*)buf + buffer_len;
    strcpy(insert_location, " | telnet_print 0");
    *(insert_location + 17) += handle; // oof

    if (fork1() == 0) runcmd(parsecmd(buf));
    wait(0);
  }
  exit(0);
}