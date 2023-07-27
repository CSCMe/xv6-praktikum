// Parsed command representation
#define EXEC 1
#define REDIR 2
#define PIPE 3
#define LIST 4
#define BACK 5

#define MAXARGS 10

struct cmd {
  int type;
};

struct execcmd {
  int type;
  char *argv[MAXARGS];
  char *eargv[MAXARGS];
};

struct redircmd {
  int type;
  struct cmd *cmd;
  char *file;
  char *efile;
  int mode;
  int fd;
};

struct pipecmd {
  int type;
  struct cmd *left;
  struct cmd *right;
};

struct listcmd {
  int type;
  struct cmd *left;
  struct cmd *right;
};

struct backcmd {
  int type;
  struct cmd *cmd;
};

void runcmd(struct cmd *cmd) __attribute__((noreturn));

void panic(char *s);

int fork1(void);

//PAGEBREAK!
// Constructors

struct cmd *execcmd(void);

struct cmd *redircmd(struct cmd *subcmd, char *file, char *efile, int mode, int fd);

struct cmd *pipecmd(struct cmd *left, struct cmd *right);

struct cmd *listcmd(struct cmd *left, struct cmd *right);

struct cmd *backcmd(struct cmd *subcmd);
//PAGEBREAK!
// Parsing

int gettoken(char **ps, char *es, char **q, char **eq);

int peek(char **ps, char *es, char *toks);

struct cmd *parseline(char **, char *);
struct cmd *parsepipe(char **, char *);
struct cmd *parseexec(char **, char *);
struct cmd *nulterminate(struct cmd *);

struct cmd *parsecmd(char *s);

struct cmd *parseline(char **ps, char *es);

struct cmd *parsepipe(char **ps, char *es);

struct cmd *parseredirs(struct cmd *cmd, char **ps, char *es);

struct cmd *parseblock(char **ps, char *es);

struct cmd *parseexec(char **ps, char *es);

// NUL-terminate all the counted strings.
struct cmd *nulterminate(struct cmd *cmd);