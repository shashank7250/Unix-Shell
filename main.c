#define _XOPEN_SOURCE 700
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>		
#include <stdbool.h>
#include <string.h>
#include <malloc.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <unistd.h>
#include <dirent.h>
#include <ftw.h>


/*
	Function Declarations for builtin shell commands:
*/
static char ** commands;
static int commandCount=0;
int sh_cd(char **args);
int sh_help(char **args);
int sh_history(char** args);
int sh_issue(char **args);
int sh_rm(char **args);
int sh_ls(char **args);
int sh_rmexcept(char **args);
int sh_exit(char **args);

/*
	Function Declaration for helper functions:
*/
int sh_num_builtins();
char **sh_split_line(char *line);
char *sh_read_line(void);
int sh_launch(char **args);
int sh_execute(char **args);
void sh_loop(void);

/*
	List of builtin commands, followed by their corresponding functions.
*/
char *builtin_str[] = {
	"help",
	"cd",
	"history",
	"issue",
	"ls",
	"rm",
	"rmexcept",
	"exit"
};

int (*builtin_func[]) (char **) = {
	&sh_help,
	&sh_cd,
	&sh_history,
	&sh_issue,
	&sh_ls,
	&sh_rm,
	&sh_rmexcept,
	&sh_exit
};


/*
	Functions:
*/
int sh_num_builtins() {
	return sizeof(builtin_str) / sizeof(char *);
}

/*
	Builtin function implementations.
*/
int sh_help(char **args) {
	int i;
	printf("\nType program names and arguments, and hit enter.\n");
	printf("The following are built in commands:\n");

	for (i = 0; i < sh_num_builtins(); i++) {
		printf("  %s\n", builtin_str[i]);
	}
	printf("\n");
	return 1;
}

int sh_cd(char **args) {
	if (args[1] == NULL) {
		fprintf(stderr, "sh: expected argument to \"cd\"\n");
	} else {
		if (chdir(args[1]) != 0) {
			perror("sh");
		}
	}
	return 1;
}

int sh_history(char**args) {
	int j,i;
	if (args[1] == NULL) {			// print complete history
		i = 0;
	} else {
		i = commandCount - atoi(args[1]); //printing most recent args[1] no. of commands
		if(i<0)i=0;
	}
	for(j = i; j < commandCount; j++)
		printf("\t%d\t%s\n", j+1, commands[j]);
	return 1;
}

int sh_issue(char **args) {
	if (args[1] == NULL) {
		fprintf(stderr, "sh: expected argument to \"issue\"\n");
	} else {
		int n = atoi(args[1]);
		if(n <= 0 || n > commandCount){
			fprintf(stderr, "sh: invalid argument to \"issue\". Argument out of range\n");
			return 1;
		}

		n--;
		printf("%s\n\n", commands[n]);
		char **commandargs = sh_split_line(commands[n]);
		int status = sh_execute(commandargs);
		free(commandargs);
		return status;
	}
	return 1;
}

int sh_ls(char **args) {
	struct dirent **namelist;
	int n = scandir(".", &namelist, NULL, alphasort);
	if(n < 0)
	{
		perror("sh");
	}
	else
	{
		while (n--)
		{
			printf("%s\n", namelist[n]->d_name);
			free(namelist[n]);
		}
		free(namelist);
	}
	return 1;
}

int unlink_cb(const char *fpath, const struct stat *sb, int typeflag, struct FTW *ftwbuf) {		
	int rv = remove(fpath);

	if (rv)
		perror(fpath);
		
	return rv;
}
		
int unlink_cb_verb(const char *fpath, const struct stat *sb, int typeflag, struct FTW *ftwbuf) {
	int rv = remove(fpath);
	if (rv)
		perror(fpath);
	else	printf("%s\n", fpath);
	return rv;
 }

int sh_rm(char **args) {
	/* Check for options used. */
	int argc = 1;
	bool RECURSIVE_FLAG = false,
		VERBOSE_FLAG = false,
		FORCE_FLAG = false;
	char filename[2048] = "";

	while(args[argc] != NULL)
	{
		if (strcmp(args[argc], "-r") == 0) {
			RECURSIVE_FLAG = true;
		} else if (strcmp(args[argc], "-f") == 0) {
			FORCE_FLAG = true;
		} else if (strcmp(args[argc], "-v") == 0) {
			VERBOSE_FLAG = true;
		} else {
			// absolute address
			if (args[argc][0] != '/') {
				// relative address
				getcwd(filename, sizeof(filename));
				strcat(filename, "/");
			}
			strcat(filename, args[argc]);
		}
		argc++;
	}

	if (filename == NULL) {
		fprintf(stderr, "sh: expected a file name to \"rm\"\n");
	} else if (!RECURSIVE_FLAG) {						// deleting a single file
		if (unlink(filename) != 0) {
			perror("sh");
			return 1;
		}
		if (VERBOSE_FLAG)	printf("%s\n", filename);
	} else {											// deleting a directory
		if(VERBOSE_FLAG)	nftw(filename, unlink_cb_verb, 64, FTW_DEPTH | FTW_PHYS);
		else	nftw(filename, unlink_cb, 64, FTW_DEPTH | FTW_PHYS);
	}
	return 1;
}

int sh_rmexcept(char **args) {
	//int size = sizeof(args)/sizeof(args[0]);
	int j, i = 0, flag = 0;

	while(args[i] != NULL)	i++;

	DIR *d;
	struct dirent *dir;
	
	d = opendir(".");
	if (d) {
		while ((dir = readdir(d)) != NULL) {

			if (strcmp(dir->d_name, ".") == 0 || strcmp(dir->d_name, "..") == 0) {
				continue;
			}

			for(j = 1; j < i; j++) {
				if( strcmp(args[j], dir->d_name) == 0 ) {
					flag = 0;
					break;
				} else {
					flag = 1;
				}
			}
			
			if (flag == 1) {
				char filename[2048] = "";
				getcwd(filename, sizeof(filename));
				strcat(filename, "/"); 
				strcat(filename, dir->d_name); 
				if (unlink(filename) != 0) {
					perror("sh");
				}	
				printf("%s\n", filename);
			}
		}

		closedir(d);
	}
	return 1;
}

int sh_exit(char **args) {
	return 0;
}

/*
	Signal Handler for killing a process after n seconds.
*/
static void sig_handler(int signo) {
}

/*
	For executing programs
*/
int sh_launch(char **args) {
	pid_t pid, wpid;
	int status;

	pid = fork();
	if (pid == 0) {
		// Child process

		int argc = 1;
		bool KILL_FLAG = false;
		int killTime = 0;

		while(args[argc] != NULL) {
			if(strcmp(args[argc], "--tkill") == 0) {
				KILL_FLAG = true;
				killTime = (args[argc+1] == NULL) ? 0 : atoi(args[argc+1]);			// if invalid conversion, then killTime = 0
				// printf("%s\t:\t%d\n", "KillTime", killTime);
				break;
			}
			argc++;
		}

		if (KILL_FLAG) {
			signal(SIGALRM, sig_handler);

			// Set a timer to send the SIGALRM after specified time(in seconds)
			alarm(killTime);
		}

		if (execvp(args[0], args) == -1) {
			perror("sh");
		}
		exit(EXIT_FAILURE);
	} else if (pid < 0) {
		// Error forking
		perror("sh");
	} else {
		// Parent process
		do {
			wpid = waitpid(pid, &status, WUNTRACED);
		} while (!WIFEXITED(status) && !WIFSIGNALED(status));
	}

	return 1;
}

int sh_execute(char **args) {
	int i;

	if (args[0] == NULL) {
		// An empty command was entered.
		return 1;
	}

	if (strcmp(args[0], "cd") == 0) {
		return (*builtin_func[1])(args);
	} else if (strcmp(args[0], "rm") == 0) {
		return (*builtin_func[5])(args);	
	} else if (strcmp(args[0], "rmexcept") == 0) {
		return (*builtin_func[6])(args);
	}
	for (i = 0; i < sh_num_builtins(); i++) {
		if (strcmp(args[0], builtin_str[i]) == 0) {

			pid_t pid, wpid;
			int status;

			pid = fork();

			if (pid == 0) {						// Child process
				// printf("%s : %d\n", "Child Process", getpid());
				status = (*builtin_func[i])(args);
				exit(status);
			} else if (pid < 0) {				// Error forking
				perror("sh");
			} else {							// Parent process
				waitpid(-1, &status, 0);
				return WEXITSTATUS(status);
			}
		}
	}

	// if not a builtin function, then run as a linux shell command
	return sh_launch(args);
}

/*
	Split a line into different words
*/
#define sh_TOK_BUFSIZE 64
#define sh_TOK_DELIM " \t\r\n\a"
char **sh_split_line(char *line) {
	int bufsize = sh_TOK_BUFSIZE, position = 0;
	char **tokens = malloc(bufsize * sizeof(char*));
	char *token;

	if (!tokens) {
		fprintf(stderr, "sh: allocation error\n");
		exit(EXIT_FAILURE);
	}

	token = strtok(line, sh_TOK_DELIM);
	while (token != NULL) {
		tokens[position] = token;
		position++;

		if (position >= bufsize) {
			bufsize += sh_TOK_BUFSIZE;
			tokens = realloc(tokens, bufsize * sizeof(char*));
			if (!tokens) {
				fprintf(stderr, "sh: allocation error\n");
				exit(EXIT_FAILURE);
			}
		}

		token = strtok(NULL, sh_TOK_DELIM);
	}
	tokens[position] = NULL;
	return tokens;
}

#define sh_RL_BUFSIZE 1024
char *sh_read_line(void) {
	int bufsize = sh_RL_BUFSIZE;
	int position = 0;
	char *buffer = malloc(sizeof(char) * bufsize);
	int c;

	if (!buffer) {
		fprintf(stderr, "sh: allocation error\n");
		exit(EXIT_FAILURE);
	}

	while (1) {
		// Read a character
		c = getchar();

		// If we hit EOF, replace it with a null character and return.
		if (c == EOF || c == '\n') {
			buffer[position] = '\0';
			return buffer;
		} else {
			buffer[position] = c;
		}
		position++;

		// If we have exceeded the buffer, reallocate.
		if (position >= bufsize) {
			bufsize += sh_RL_BUFSIZE;
			buffer = realloc(buffer, bufsize);
			if (!buffer) {
				fprintf(stderr, "sh: allocation error\n");
				exit(EXIT_FAILURE);
			}
		}
	}
}

#define sh_HIST_BUFFER 10
void sh_loop(void) {
	char *line;
	char **args;
	int status;
	int histSize = sh_HIST_BUFFER;
	commands = (char **)malloc(sizeof(char *)*histSize);
	commandCount = 0;
	char *command;
	do {
		// printf("%s : %d\n", "Parent", getpid());
		printf("> ");
		command = sh_read_line();
		commands[commandCount] = (char *)malloc(sizeof(*command));
		strcpy(commands[commandCount], command);
		args = sh_split_line(command);
		status = sh_execute(args);
		free(args);
		free(command);
		commandCount++;

		if(commandCount >= histSize){
			histSize += sh_HIST_BUFFER;
			commands = realloc(commands, sizeof(char *) * histSize);
			if(!commands){
				fprintf(stderr, "%s\n", "sh: allocation error");
				exit(EXIT_FAILURE);
			}
		}
	} while (status);
}

int main(int argc, char **argv)
{
	system("clear");
	sh_loop();

	return EXIT_SUCCESS;
}
