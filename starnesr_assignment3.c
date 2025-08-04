#include <stdio.h>
#include <stdlib.h> 
#include <string.h>
#include <stdbool.h>
#include <unistd.h>

#include <sys/types.h> //for fork
#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h>

//constants:
#define INPUT_LENGTH 2048
#define MAX_ARGS 512
#define MAX_BG_PROCESS 100

//globals       sowwy :(
pid_t bg_pids[MAX_BG_PROCESS];
int bg_count = 0;
volatile bool foreground_only_mode = false;

//structs
//struct implimentation & partse_input() provided by OSU CS374_400_U2025 "sample_parser.c"
struct command_line
{
	char *argv[MAX_ARGS + 1];
	int argc;
	char *input_file;
	char *output_file;
	bool is_bg;
};


struct command_line *parse_input()
{
	char input[INPUT_LENGTH];
	struct command_line *curr_command = (struct command_line *) calloc(1, sizeof(struct command_line));

	// Get input
	printf(": ");
	fflush(stdout);
	fgets(input, INPUT_LENGTH, stdin);

	// Tokenize the input
	char *token = strtok(input, " \n");
	while(token){
		if(!strcmp(token,"<")){
			curr_command->input_file = strdup(strtok(NULL," \n")); //remember to free, strdup dyanmically allocates.
		} else if(!strcmp(token,">")){
			curr_command->output_file = strdup(strtok(NULL," \n")); //remember to free, strdup dyanmically allocates.
		} else if(!strcmp(token,"&")){
			//curr_command->is_bg = true;
            if (!foreground_only_mode){ curr_command->is_bg = true; }
		} else{
			curr_command->argv[curr_command->argc++] = strdup(token); //remember to free, strdup dyanmically allocates.
		}
		token=strtok(NULL," \n");
	}
	return curr_command;
}

void free_command(struct command_line *command){
    //free each argument
    for (int i = 0; i < command->argc; i++)
    {
        free(command->argv[i]); 
    }

    //free the input file if it has been allocated (meaning it exists)
    if (command->input_file != NULL)
    {
        free(command->input_file);
    }

    //similar idea here
    if (command->output_file != NULL)
    {
        free(command->output_file);
    }

    //now that everything in the struct has been freed, free the struct itself
    free(command);
}

void change_directory(struct command_line *command, const char *home){
    const char *directory;

    //if there is only one argument its "cd" so just return to home
    if (command->argc == 1)
    {
        directory = home;
    }
    //if there is another argument its "cd {some directory}" so go {some directory}
    else{
        directory = command->argv[1];
    }
    //now that we have determined the directory 
    if (chdir(directory) != 0)
    {
        //there has been error
        printf("cannot change directory to %s\n", directory);
    }
    



}

void status(int lastStatus, bool signaled){
    if (signaled)
    {
       printf("terminated by signal %d\n", lastStatus);
    }
    else
    {
        printf("exit value %d\n", lastStatus);
    }
    //recall to use fflush() everytime we print
    fflush(stdout);
    
}

void handle_SIGTSTP(int signo){
    char* m;
    if (foreground_only_mode)
    {
        foreground_only_mode = false;
        m = "\nExiting foreground-only mode";
    }
    else
    {
        foreground_only_mode = true;
        m = "\nEntering foreground-only mode (& is now ignored)";
    }
    write(STDOUT_FILENO, m, strlen(m));
    

}
    


void input_redirect(struct command_line *command){
    /*
    Recall:
    a file descriptor is an ID for open file or I/O steam (i.e. stdin & stdout). open() gets a file descriptor for a given file.
    dup2(new, old) tells the OS, make the old file descriptor now refer to the same file as the new. To my understanding this means
    now the traffic to the old descriptor is routed to the new descriptor, so a printf() could be routed to a file.  
    */

    int inputFileDescriptor = open(command->input_file, O_RDONLY);
    //open() returns -1 if file couldn't open so lets check that
    if (inputFileDescriptor == -1)
    {
        perror("cannot open input file");
        exit(1);
    }

    dup2(inputFileDescriptor, 0); //0 because stdin file descriptor = 0
    close(inputFileDescriptor);
}

void output_redirect(struct command_line *command){
    int outputFileDescriptor = open(command->output_file, O_WRONLY | O_CREAT | O_TRUNC, 0644); //write mode, creates file if it doesnt exist, and erases contents with every write. 
    //open() returns -1 if file couldn't open so lets check that
    if (outputFileDescriptor == -1)
    {
        perror("cannot open input file");
        exit(1);
    }

    dup2(outputFileDescriptor, 1); //1 because stdout file descriptor = 1
    close(outputFileDescriptor);

}

void bg_input_redirect_null(){
    int nullFileDescriptor = open("/dev/null", O_RDONLY);
    if (nullFileDescriptor == -1)
    {
        perror("cannot open /dev/null for input");
        exit(1);
    }
    dup2(nullFileDescriptor, 0); //stdin File Descriptor == 1
    close(nullFileDescriptor);
}

void bg_output_redirect_null(){
    int nullFileDescriptor = open("/dev/null", O_WRONLY);
    if (nullFileDescriptor == -1)
    {
        perror("cannot open /dev/null for output");
        exit(1);
    }
    dup2(nullFileDescriptor, 1); //stdout File Descriptor == 1
    close(nullFileDescriptor);

}

//if I were smart I would combine all 4 of the above helper functions, as there is a LOT of repeated code. It is an eyesore but will due. I mostly wanted to cut fat in other_commands().

void rotate_array_left(int* arr, int size) {
    if (size <= 1){
        return;
    }

    int first = arr[0];
    for (int i = 0; i < size - 1; i++) 
    {
        arr[i] = arr[i + 1];
    }
    arr[size - 1] = first;
}

void bg_check(){
    int i = 0;
    int status;
    pid_t x;

    while (bg_count > i)
    {
        x = waitpid(bg_pids[i], &status, WNOHANG); //WNOHNAG "Demands status information immediately. -IBM https://www.ibm.com/docs/en/zvm/7.4.0?topic=descriptions-waitpid-wait-specific-child-process-end"

        //check if proccess in done
        if (x > 0)
        {
            printf("background pid %d is done: ", bg_pids[i]);
            if (WIFEXITED(status)){
                printf("exit value %d\n", WEXITSTATUS(status));
            }
            else if (WIFSIGNALED(status))
            {
                 printf("terminated by signal %d\n", WTERMSIG(status));
            }
            fflush(stdout);

            rotate_array_left(bg_pids,bg_count);
            bg_count--;
        }
        else{
            i++; //go to next bg (if there is one)

        }
        
    }
    






}

int other_commands(struct command_line *command, int *lastStatus, bool *signaled){
    pid_t spawnpid = -5;
    int childStatus;
    // int child;

    spawnpid = fork();

    //check if fork failed
    if (spawnpid == -1)
    {
        perror("fork() failed!");
        //return error code
        return 1;
    }

    /*Recall: fork returns different things to the fork and the child.
    To the child it return returns 0, and to the parent it returns the process ID of the child*/ 

    //check if child process is running
    if (spawnpid == 0)
    {
        //we are running commands in the child process here. 
        if (!(command->is_bg))
        {
            //bc its child thats not background proccess we want to un-ignore SIGINT.
            //recall the child had inherited ignoring SIGINT from its parent. 
            struct sigaction default_SIGINT = {0};
            default_SIGINT.sa_handler = SIG_DFL;
            sigfillset(&default_SIGINT.sa_mask);
            default_SIGINT.sa_flags = 0;
            sigaction(SIGINT, &default_SIGINT, NULL);
        }

\

        //recall file redirection is only scoped to this command as it only occurs for this child proccess
        //INPUT
        if (command->input_file != NULL)
        {
            input_redirect(command); 
        }
        else if (command->is_bg)
        {
            bg_input_redirect_null();
        }
        
        //OUTPUT
        if (command->output_file != NULL)
        {
            output_redirect(command);
        }
        else if (command->is_bg)
        {
            bg_output_redirect_null();
        }
        
        execvp(command->argv[0], command->argv);
        //if the child process is active here, execvp() must of failed
        perror("execvp() failed");
        free_command(command); 
        exit(1);
    }
    
    /*
    personal notes about execvp():
    we fork to run execvp() because execvp() replaces the current process by running a compiled program based on its arguments.
    By calling execvp in the child, the main process "the parent", stays alive. 

    personal notes about waitpid():
    waits for a specific child process (identified by its Process ID) and returns a stat value analyzed with "Analysis Macros"

    */

   //go to parent process
   else{
        //check if command should be ran in background or not
        if (command->is_bg)
        {
            //print the background process ID as done in the example
            printf("background pid %d\n", spawnpid);
            fflush(stdout);

            if (bg_count < MAX_BG_PROCESS)
            {
                bg_pids[bg_count] = spawnpid; //append to our magical array where we track background proccess's by their PIDS
                bg_count++;
            }
            //if we exceed acceptable number of background proccess's we are forced to wait until its done
            else
            {
                waitpid(spawnpid, &childStatus, 0);
            }  

        }
        else{
                
            //wait for child process to finish
            waitpid(spawnpid, &childStatus, 0);

            if (WIFEXITED(childStatus))
            {
                *lastStatus = WEXITSTATUS(childStatus);
                *signaled = false;
            } 
            else if (WIFSIGNALED(childStatus))
            {
                *lastStatus = WTERMSIG(childStatus);
                *signaled = true;

                printf("terminated by signal %d\n", *lastStatus);
                fflush(stdout);
            }

        }   
   }
    return 0;
}

//initially called to set up the shiell, i.e. the parent process to ignore SIGINT
void setup_ignore_sigint(){
    //initilize SIGINT_action struct to be empty.
    struct sigaction SIGINT_action = {0};
    //set struct's sa_handler member variable to SIG_IGN or SIG_DFL will ignore it
    SIGINT_action.sa_handler = SIG_IGN;
    //block all signals while handle_SIGINT is running
    sigfillset(&SIGINT_action.sa_mask);
    SIGINT_action.sa_flags = 0;
    //setup signal handling function for when SIGINT is called
    sigaction(SIGINT, &SIGINT_action, NULL);


}

void setup_sigstp(){
    struct sigaction SIGTSTP_action = {0};
    SIGTSTP_action.sa_handler = handle_SIGTSTP; //link our handler we made
    sigfillset(&SIGTSTP_action.sa_mask);
    SIGTSTP_action.sa_flags = SA_RESTART; //we use this flag when using a signal handle.
    sigaction(SIGTSTP, &SIGTSTP_action, NULL);

}

int main(){
    setup_ignore_sigint();
    setup_sigstp();
    //variables
    struct command_line *currCommand;
    const char *homeDirectory = getenv("HOME"); 
    int lastStatus = 0; //exit code value of last FOREGROUND process
    bool x = false; //true if last FOREGROUND process killed by signal

    while(1){
        bg_check();
        //struct command_line *currCommand = parse_input(); //get user input
        currCommand = parse_input();


        //Comment Command:
        //receall that argv[0] goes to the first argument (char*), then derefferencing gives value stored by this pointer. 
        //in laymans terms, the first character entered. Also if nothing was entered just do nothing. 
        if (currCommand->argc == 0 || *(currCommand->argv[0]) == '#' )
        {
            //do nothing honestly & restart
            free_command(currCommand);
            continue;
        }
        //Exit Command
        else if ( strcmp(currCommand->argv[0], "exit") == 0) //if strings match strcmp returns 0
        {
            free_command(currCommand);
            exit(0);
        }
        
        //Change Directory Command
        else if (strcmp(currCommand->argv[0], "cd") == 0) 
        {
            change_directory(currCommand, homeDirectory);
            free_command(currCommand);
            continue;
        }

        else if (strcmp(currCommand->argv[0], "status") == 0) 
        {
            status(lastStatus,x);
            free_command(currCommand);
            continue;
        }

        else{
            other_commands(currCommand, &lastStatus, &x);

        }




        //free this currCommand
        free_command(currCommand);
        //off to the next one!

    }


    return 0;
}

/*
educational sources:

https://www.geeksforgeeks.org/c/fork-system-call/ //for forking
Canvas "Exploration: Process API - Monitoring Child Processes" //for forking
https://www.ibm.com/docs/en/zvm/7.4.0?topic=descriptions-waitpid-wait-specific-child-process-end //for waitpid()

//https://www.geeksforgeeks.org/cpp/cpp-getenv-function/ //for acquiring home directory

//https://www.geeksforgeeks.org/c/dup-dup2-linux-system-call/

Canvas "Exploration: Signal Handling API"
*/