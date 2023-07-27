#include "user/user.h"
#include "user/shell/shell.h"
#include "kernel/fcntl.h"

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(2, "Expected exactly one argument, got %d\n", argc - 1);
        exit(-1);
    }

    int handle = atoi(argv[1]);

    static char buffer[100];
    int empty_lines_in_a_row = 0;
    for (;;) {
      memset(buffer, 0, sizeof(buffer));
      gets(buffer, sizeof(buffer));
      if (strlen(buffer) != 0) {
        for (int i = 0; i < empty_lines_in_a_row; i++) {
            printf("\n");
        }
        empty_lines_in_a_row = 0;

        net_send_listen(handle, buffer, sizeof(buffer), NULL, 0);
      } else {
        empty_lines_in_a_row += 1;
        if (empty_lines_in_a_row > 100) {
            // There probably won't be more
            exit(0);
        }
      }
    }
    exit(0);
}