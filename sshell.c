#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <dirent.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>

#define CMDLINE_MAX 512
#define ARGV_MAX 16
#define TOKEN_MAX 32
#define PIPE_MAX 3

#define CMD_SUCCESS 0
#define CMD_FAILURE 1
#define CMD_CONTINUE 2

#define REDIR_NONE 3
#define REDIR_TRUNCATE 4
#define REDIR_APPEND 5

struct redirData {
        int mode;
        char *filename;
};

struct pipeData {
        int status;
        int pipeCount;
        char *cmdv[ARGV_MAX];
        char cmd[CMDLINE_MAX];
};

/* Parse command */
int Parse(char *cmd, char **argv)
{
        if (strlen(cmd) == 0) {
                fprintf(stderr, "Error: missing command\n");
                return CMD_FAILURE;
        }

        int argc = 0;
        char *tok = strtok(cmd, " ");

        while(tok != NULL) {
                if (argc >= ARGV_MAX) {
                        fprintf(stderr, "Error: too many process arguments\n");
                        return CMD_FAILURE;
                }
                char *arg = tok;        // Create new string each time
                argv[argc] = arg;       // to assign to argv member ptr
                tok = strtok(NULL, " ");
                argc++;
        }
        argv[argc] = NULL;

        return CMD_SUCCESS;
}

/* Scan command for redirection */
struct redirData ScanRedirect(char *cmd)
{
        struct redirData data;
        data.mode = REDIR_NONE;
        char *redir = strchr(cmd, '>');
        char *redir2 = strrchr(cmd, '>');

        if (redir == NULL)
                return data;

        if (redir < strrchr(cmd, '|')) {
                fprintf(stderr, "Error: mislocated output redirection\n");
                data.mode = CMD_FAILURE;
                return data;
        }

        if (redir2 == (redir + 1)) {
                data.mode = REDIR_APPEND;
                data.filename = redir2 + 1;
        } else {
                data.mode = REDIR_TRUNCATE;
                data.filename = redir + 1;
        }

        *redir = '\0';
        while (redir-- >= cmd && isspace(*redir))
                *redir = '\0';

        while (isspace(*data.filename))
                data.filename++;

        if (!strcmp(cmd, "")) {
                fprintf(stderr, "Error: missing command\n");
                data.mode = CMD_FAILURE;
        } else if (data.filename == NULL || !strcmp(data.filename, "")) {
                fprintf(stderr, "Error: no output file\n");
                data.mode = CMD_FAILURE;
        }

        return data;
}

/* Scan command for pipes */
struct pipeData ScanPipes(char *cmd)
{
        struct pipeData data;
        data.pipeCount = 0;
        strcpy(data.cmd, cmd);

        for (unsigned i = 0; i < strlen(cmd); i++) {
                if (cmd[i] == '|')
                        data.pipeCount++;
        }

        int cmdcount = 0;
        char *tok = strtok(cmd, "|");

        while(tok != NULL) {
                char *command = tok;
                data.cmdv[cmdcount] = command;
                while (*tok != '\0') {
                        if (!isspace(*tok)) {
                                cmdcount++;
                                break;
                        }
                        tok++;
                }
                tok = strtok(NULL, "|");
        }

        if (cmdcount > PIPE_MAX + 1) {
                fprintf(stderr, "Error: too many process arguments\n");
                data.status = CMD_FAILURE;
                return data;
        }

        if (cmdcount <= data.pipeCount) {
                fprintf(stderr, "Error: missing command\n");
                data.status = CMD_FAILURE;
                return data;
        }

        data.cmdv[cmdcount] = NULL;

        return data;
}

/* Redirct STDOUT to file */
int Redirect(struct redirData data)
{
        int fd;
        if (data.mode == REDIR_TRUNCATE)
                fd = open(data.filename, O_RDWR | O_CREAT | O_TRUNC, S_IRWXU);
        else
                fd = open(data.filename, O_RDWR | O_CREAT | O_APPEND, S_IRWXU);

        if (fd == -1) {
                fprintf(stderr, "Error: cannot open output file\n");
                return CMD_FAILURE;
        }

        dup2(fd, STDOUT_FILENO);
        close(fd);

        return CMD_SUCCESS;
}

/* Reset stdio to default */
void ResetFD(int *fd)
{
        close(STDIN_FILENO);
        close(STDOUT_FILENO);

        dup2(fd[STDIN_FILENO], STDIN_FILENO);
        dup2(fd[STDOUT_FILENO], STDOUT_FILENO);

        close(fd[STDIN_FILENO]);
        close(fd[STDOUT_FILENO]);
}

/* Filter out hidden files for sls */
int Filter(const struct dirent *entry)
{
        return entry->d_name[0] != '.';
}

/* Built-in ls-like command */
int sls(char *name)
{
        struct dirent **entryList;
        int entryCount = scandir(name, &entryList, Filter, alphasort);

        if (entryCount == -1) {
                fprintf(stderr, "Error: cannot open directory\n");
                return CMD_FAILURE;
        }

        while (entryCount--) {
                struct stat st;
                struct dirent *entry = entryList[entryCount];

                if (stat(entry->d_name, &st) == -1) {
                        perror("stat");
                        return CMD_FAILURE;
                }

                printf("%s (%lld bytes)\n",
                       entry->d_name, (long long) st.st_size);
        }

        return CMD_SUCCESS;
}

/* Built-in command */
int BuiltinCMD(char **argv)
{
        if (!strcmp(argv[0], "pwd")) {
                char currentDir[CMDLINE_MAX];
                getcwd(currentDir, CMDLINE_MAX);
                printf("%s\n", currentDir);
                return CMD_SUCCESS;
        }

        if (!strcmp(argv[0], "cd")) {
                if(chdir(argv[1]) == -1) {
                        fprintf(stderr, "Error: cannot cd into directory\n");
                        return CMD_FAILURE;
                } else
                        return CMD_SUCCESS;
        }
        
        if (!strcmp(argv[0], "sls")) {
                char currentDir[CMDLINE_MAX];
                getcwd(currentDir, CMDLINE_MAX);
                return sls(currentDir);
        }

        return CMD_CONTINUE;
}

/* Execute command & wait */
int Exec(char **argv, int infd, int outfd, int lastcmd)
{
        pid_t childpid;
        int wstatus;

        childpid = fork();
        if (childpid == -1) {
                perror("error: fork");
                exit(EXIT_FAILURE);
        }

        if (childpid > 0) {
                wait(&wstatus);
                return WEXITSTATUS(wstatus);
        } else {
                if (infd != STDIN_FILENO) {
                        dup2(infd, STDIN_FILENO);
                        if (!lastcmd)
                                close(infd);
                }
                if (outfd != STDOUT_FILENO && !lastcmd) {
                        dup2(outfd, STDOUT_FILENO);
                        close(outfd);
                }
                execvp(argv[0], argv);
                fprintf(stderr, "Error: command not found\n");
                exit(EXIT_FAILURE);
        }
}

/* Execute pipe */
void Pipe(struct pipeData data, char **argv, int *stdfd)
{
        int retvals[PIPE_MAX + 1];
        int fd[2];
        int infd = STDIN_FILENO;
        int lastcmd = 0;

        int i = 0;
        while (data.cmdv[i] != NULL) {
                pipe(fd);
                Parse(data.cmdv[i], argv);

                if (data.cmdv[i + 1] == NULL)
                        lastcmd++;

                retvals[i] = Exec(argv, infd, fd[STDOUT_FILENO], lastcmd);

                if (!lastcmd)
                        close(fd[STDOUT_FILENO]);

                infd = fd[STDIN_FILENO];
                i++;
        }
        ResetFD(stdfd);

        fprintf(stderr, "+ completed '%s' ", data.cmd);
        for (int j = 0; j < i; j++)
                fprintf(stderr, "[%d]", retvals[j]);
        fprintf(stderr, "\n");
}

/* Implement system() with out-redirect & pipes */
void MySystem(const char *cmd, char **argv)
{
        int retval;
        static int stdfd[2];
        stdfd[STDIN_FILENO] = dup(STDIN_FILENO);
        stdfd[STDOUT_FILENO] = dup(STDOUT_FILENO);

        char cmdcopy[CMDLINE_MAX];
        strcpy(cmdcopy, cmd);

        struct redirData redirdata = ScanRedirect(cmdcopy);
        if (redirdata.mode == CMD_FAILURE)
                return;

        struct pipeData pipedata = ScanPipes(cmdcopy);
        if (pipedata.status == CMD_FAILURE)
                return;

        if (pipedata.pipeCount == 0 && Parse(cmdcopy, argv) == CMD_FAILURE)
                return;
        
        if (redirdata.mode != REDIR_NONE && Redirect(redirdata))
                return;

        if (pipedata.pipeCount > 0) {
                Pipe(pipedata, argv, stdfd);
                return;
        } else {
                retval = BuiltinCMD(argv);
                if (retval == CMD_CONTINUE)
                        retval = Exec(argv, STDIN_FILENO, STDOUT_FILENO, 0);
        }

        ResetFD(stdfd);
        fprintf(stderr, "+ completed '%s' [%d]\n", pipedata.cmd, retval);
}

int main(void)
{
        char cmd[CMDLINE_MAX];

        while (1) {
                char *nl;
                char *argv[ARGV_MAX];

                /* Print prompt */
                printf("sshell@ucd$ ");
                fflush(stdout);

                /* Get command line */
                fgets(cmd, CMDLINE_MAX, stdin);

                /* Print command line if stdin is not provided by terminal */
                if (!isatty(STDIN_FILENO)) {
                        printf("%s", cmd);
                        fflush(stdout);
                }

                /* Remove trailing newline from command line */
                nl = strchr(cmd, '\n');
                if (nl)
                        *nl = '\0';

                /* Builtin exit command */
                if (!strcmp(cmd, "exit")) {
                        fprintf(stderr, "Bye...\n");
                        fprintf(stderr, "+ completed 'exit' [0]\n");
                        break;
                }

                /* Run other command */
                MySystem(cmd, argv);
        }

        return EXIT_SUCCESS;
}
