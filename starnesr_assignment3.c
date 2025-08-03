#include <stdio.h>
#include <stdlib.h> 
#include <string.h>
#include <stdbool.h>
#include <unistd.h>

#include <sys/types.h> //for fork
#include <sys/wait.h>


//constants:
#define INPUT_LENGTH 2048
#define MAX_ARGS 512

//structs
//struct implimentation provided by OSU CS374_400_U2025 "sample_parser.c"
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
			curr_command->is_bg = true;
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

int other_commands(struct command_line *command, int *lastStatus){
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
    
    //wait for child process to finish
    waitpid(spawnpid, &childStatus, 0);

    if (WIFEXITED(childStatus)){
        *lastStatus = WEXITSTATUS(childStatus);
    } else if (WIFSIGNALED(childStatus)){
        *lastStatus = WTERMSIG(childStatus);
    }

    return 0;
    



}

void status(){

}


int main(){

    //variables
    struct command_line *currCommand;
    const char *homeDirectory = getenv("HOME"); 
    int lastStatus = 0;

    while(1){
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
            status(lastStatus);
            free_command(currCommand);
            continue;
        }

        else{
            other_commands(currCommand, &lastStatus);

        }




        //free this currCommand
        free_command(currCommand);
        //off to the next one!

    }


    return 0;
}

/*
sources:

https://www.geeksforgeeks.org/c/fork-system-call/ //for forking

Canvas "Exploration: Process API - Monitoring Child Processes" //for forking

https://www.ibm.com/docs/en/zvm/7.4.0?topic=descriptions-waitpid-wait-specific-child-process-end //for waitpid()

//https://www.geeksforgeeks.org/cpp/cpp-getenv-function/ //for acquiring home directory




*/