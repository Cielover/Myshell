#include <stdio.h>		/* for convenience */
#include <stdlib.h>		/* for convenience */
#include <string.h>		/* for convenience */
#include <unistd.h>		/* for convenience */
#include <errno.h>		/* for definition of errno */
#include <stdarg.h>		/* ISO C variable aruments */
#include <sys/types.h>		/* some systems still require this */
#include <sys/signal.h>
#include <sys/wait.h>

#define BUF_MAX 1024

const char* COMMAND_EXIT = "exit";
const char* COMMAND_HELP = "help";
const char* COMMAND_BGRUN = "bg";
const char* COMMAND_IN = "<";
const char* COMMAND_OUT = ">";

char* COMMAND_PROMPT = "%";     /* can be changed by env */
const char* ENV_NAME ="MYSHELL_COMMAND_PROMPT";  /* use to get shell prompt */

char 	commands[BUF_MAX][BUF_MAX];	/* just like argvs but each argv is a char[] end with '/0' */


int	parseCommand(int argvNum);	/* Process command with redirect */ 
int 	callCommand(int commandNum);	/* Execute commands entered by the user */
int	splitCommandString(char command[BUF_MAX]);	/* Split the command input buffer and return argc (the number of agrs +1) */
int	print_prompt();		/* Print command prompt */
static void	normal_exit();  /* Call exit() to normal exit */
static void	sig_int(int);	/* our signal-catching function */
static void	err_doit(int, int, const char *, va_list);
static void     err_ret(const char *fmt, ...);
static void     err_sys(const char *fmt, ...);

/* status of all errors may product */
enum {
	RESULT_NORMAL,
	ERROR_OUTPUT,
	ERROR_FORK,
	ERROR_EXECUTE,
	ERROR_WAITPID,
	ERROR_DUP,
	ERROR_MISS_PARAMETER,
	ERROR_EXIT,

	/* Error message of redirect */
	ERROR_REPEAT_INPUT_REDIRECTION,
	ERROR_REPEAT_OUTPUT_REDIRECTION,
	ERROR_FILE_NOT_EXIST,
};

int
main(void)
{
	char	buf[BUF_MAX];	
	char*   env_prompt_val;
	int 	result;
    	
	/* Update command prompt */
	if((env_prompt_val = getenv(ENV_NAME)) != NULL)
	{
		/* Count number of command prompt */
		int env_prompt_val_num = 0;
		while(env_prompt_val[env_prompt_val_num]!='\0')
			env_prompt_val_num++;

		/* If env is set to "" do not update */
		if(env_prompt_val_num != 0)
			COMMAND_PROMPT = env_prompt_val;
		else
			printf("Tip: Note that you have set the env for command prompt, but it's empty, please check.\n");
	}
	
	/* Regist signal to exit when user put down CTRL+C */
	if (signal(SIGINT, sig_int) == SIG_ERR)
		err_sys("Error: Signal error.");

	if (print_prompt() < 0) /* Print prompt */
		err_sys("Error: Print prompt error.");
		
	while (fgets(buf, BUF_MAX, stdin) != NULL) {

		if (buf[strlen(buf) - 1] == '\n')
			buf[strlen(buf) - 1] = 0; /* Replace newline with null */

		int commandNum = splitCommandString(buf);
		
		if (commandNum != 0) {
			if (strcmp(commands[0], COMMAND_EXIT) == 0) { /* User input exit command */
				normal_exit();
			}else if (strcmp(commands[0], COMMAND_HELP) == 0) { /* User input help command */
				printf("\e[1;32mWelcome! You are now using a custom shell tool programmed by Yidian.\n\e[0m");				
				printf("\e[1;34mUsage: <command> [arg1] [arg2]...\n\e[0m");
				printf("\e[1;34mTips: Input 'exit' to quit.\n\e[0m");
				fflush(stdout);
			}else{ /* User input other command */
				result = callCommand(commandNum);
				switch (result) {
					case ERROR_FORK:
						err_sys("Error: Fork error.");
						break;
					case ERROR_EXECUTE:
						err_ret("Error: Couldn't execute: %s", buf);
						break;
					case ERROR_WAITPID:
						err_sys("Error: Waitpid error.");
						break;
					case ERROR_DUP:
						err_sys("Error: Dup error.");
						break;				
					case ERROR_MISS_PARAMETER:
						err_ret("Error: Miss redirect parameters.");
						break;

					/* Error message of redirect */
					case ERROR_REPEAT_INPUT_REDIRECTION:
						err_ret("Error: Too many input redirection symbol.");
						break;
					case ERROR_REPEAT_OUTPUT_REDIRECTION:
						err_ret("Error: Too many output redirection symbol.");
						break;
					case ERROR_FILE_NOT_EXIST:
						err_ret("Error: Input redirection file not exist.");
						break;
				}
			}
			if (print_prompt() < 0) /* Print prompt */
				err_sys("Error: Print prompt error.");
		}
		
	}
	exit(0);
}

/* Process command with redirect */
int parseCommand(int argvNum) { 
	
	pid_t pid; /* Child pid */
	int status; /* Child status */
	int result = RESULT_NORMAL; /* Return code */
	int inNum = 0, outNum = 0; /* Flag to judge whether contain redirect */
	int input_reIdx, output_reIdx; /* Redirect symbol location */
	char *inFile = NULL, *outFile = NULL; /* Redirect file */
	int is_bg = 0;

	if (strcmp(commands[0], COMMAND_BGRUN) == 0) {
		is_bg = 1;
		int i,j; /* loop flag */

		/* Clear command flag 'COMMAND_BGRUN' on first location */
		for (i = 0; i < argvNum; i++)		  
			/* printf("argv[%d]: %s\n", i, commands[i]); */
			for (j = 0; j < BUF_MAX; j++)	{
				commands[i][j]=commands[i+1][j];
			}
		commands[argvNum][0]='\0';
		argvNum--;
	}
	
	int i; /* loop flag */
	
	/* Determine whether there is redirection */
	for (i=0; i<argvNum; ++i) {
		if (strcmp(commands[i], COMMAND_IN) == 0) { /* Redirect input */
			inNum = inNum + 1;
			if (i+1 < argvNum)
				inFile = commands[i+1];
			else return ERROR_MISS_PARAMETER; /* Missing filename after redirection symbol */

			input_reIdx = i;
		} else if (strcmp(commands[i], COMMAND_OUT) == 0) { /* Redirect output */
			outNum = outNum + 1;
			if (i+1 < argvNum)
				outFile = commands[i+1];
			else return ERROR_MISS_PARAMETER; /* Missing filename after redirection symbol */
				
			output_reIdx = i;
		}
	}

	if (inNum > 1) { /* More than one input redirector */
		return ERROR_REPEAT_INPUT_REDIRECTION;
	} else if (outNum > 1) { /* More than one output redirector */
		return ERROR_REPEAT_OUTPUT_REDIRECTION;
	}
	
	/* Try to open input redirection */
	if (inNum == 1) {
		FILE* fp = fopen(inFile, "r");

		if (fp == NULL) /* Input redirection file does not exist */
			return ERROR_FILE_NOT_EXIST;
		
		fclose(fp);
	}

	if ((pid = vfork()) < 0) {
		result = ERROR_FORK;
	} else if (pid == 0) {	/* child */
		
		/* Open redirect */
		if (inNum == 1)
			freopen(inFile, "r", stdin);
		if (outNum == 1)
			freopen(outFile, "w", stdout);

		/* do command */
		char* comm[BUF_MAX];
		int commNum = 0;
		for (i=0; i<argvNum; ++i){
			/* printf("argv[%d]: %s\n", i, commands[i]); */
			if(inNum == 1 && ( i==input_reIdx || i==(input_reIdx+1)) )
				continue;
			if(outNum == 1 && ( i==output_reIdx || i==(output_reIdx+1)) )
				continue;
			comm[commNum++] = commands[i];
		}
		comm[commNum++] = 0;
		comm[commNum] = NULL;

		/* echo all command-line args */		
		/*		
		int i;		
		for (i = 0; i < commNum; i++)		  
			printf("argv[%d]: %s\n", commNum, comm[i]);
		*/
		
		errno = 0; /* Clear errno */
		execvp(comm[0], comm);
		exit(errno);	/* child terminates with return errno */
	} else {
		if(is_bg == 0){
			waitpid(pid, &status, 0);
			int err = WEXITSTATUS(status); /* Read return code of subprocess */

			if (err) { /* Judge whether the return value is executed successfully */
				printf("\e[31;1mError: %s\n\e[0m", strerror(err));
				result = ERROR_EXECUTE;
			}
		}else{
			printf("[process id %d] is in background running...\n",pid);
			fflush(stdout);
		}
	}

	return result;
}

/* Execute commands entered by the user */
int
callCommand(int commandNum) {
	pid_t	pid;
	int	status;
	int	inFds,outFds;
 
	if ((pid = fork()) < 0) {
		return ERROR_FORK;
	} else if (pid == 0) {	/* child */

		/* Backup */
		if((inFds = dup(STDIN_FILENO))<0)
			exit(ERROR_DUP);
		if((outFds = dup(STDOUT_FILENO))<0)
			exit(ERROR_DUP);

		int result = parseCommand(commandNum);

		/* printf("123"); */
		
		/* Restore standard input and output redirection */
		if(dup2(inFds, STDIN_FILENO)<0)
			exit(ERROR_DUP);
		if(dup2(outFds, STDOUT_FILENO)<0)
			exit(ERROR_DUP);

		exit(result);
	}

	/* parent */
	if ((pid = waitpid(pid, &status, 0)) < 0)
		return ERROR_WAITPID;
	return WEXITSTATUS(status);
}

/* Split the command input buffer and return argc (the number of agrs +1) */
int 
splitCommandString(char command[BUF_MAX]) {
	int num = 0;
	int i, j;
	int len = strlen(command);

	for (i=0, j=0; i<len; ++i) {
		if (command[i] != ' ') {
			commands[num][j++] = command[i];
		} else {
			if (j != 0) {
				commands[num][j] = '\0';
				++num;
				j = 0;
			}
		}
	}
	if (j != 0) {
		commands[num][j] = '\0';
		++num;
	}

	return num;
}

/* Process interrupt */ 
void
sig_int(int signo)
{
	/* printf("interrupt\n%% "); 
        fflush(stdout); */
	normal_exit();
}

/* Call exit() to normal exit */
void normal_exit() { 
	if (printf("EXIT Bye~\n")) {
		fflush(stdout);
		exit(0);
	}
	else exit(ERROR_OUTPUT);
}

/*
 * Print command prompt
 */
int 
print_prompt()
{
	int out_num = printf("%s", COMMAND_PROMPT);
	int out_space = printf(" ");
	fflush(stdout);
	/* printf("%d", out_num); */
	/* printf("%d", out_space); */
	if( out_num <= 0 || out_space != 1) 
		return -1;
	return 0;
}

/*
 * Print a message and return to caller.
 * Caller specifies "errnoflag".
 */
static void
err_doit(int errnoflag, int error_type, const char *fmt, va_list ap)
{
	char	buf[BUF_MAX];

	vsnprintf(buf, BUF_MAX-1, fmt, ap);
	if (errnoflag){
		if(error_type == 1)
			snprintf(buf+strlen(buf), BUF_MAX-strlen(buf)-1, " %s", "(System Internal Error)");
		else
			printf(buf+strlen(buf), BUF_MAX-strlen(buf)-1, " ");
	}
	strcat(buf, "\n");
	fflush(stdout);		/* in case stdout and stderr are the same */
	fputs(buf, stderr);
	fflush(NULL);		/* flushes all stdio output streams */
}
/*
 * Nonfatal error related to a system call.
 * Print a message and return.
 */
void
err_ret(const char *fmt, ...)
{
	va_list		ap;

	va_start(ap, fmt);
	err_doit(1, 0, fmt, ap);
	va_end(ap);
}

/*
 * Fatal error related to a system call.
 * Print a message and terminate.
 */
void
err_sys(const char *fmt, ...)
{
	va_list		ap;

	va_start(ap, fmt);
	err_doit(1, 1, fmt, ap);
	va_end(ap);
	exit(1);
}


