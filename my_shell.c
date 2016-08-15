#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h>
#define BUFFERSIZE 1024
void add_space_between_token(char * buffer);
int recognize_a_token(char * token);
void tokenize_into_commands_with_pipeline(char * buffer);
void set_pipes(int pipes[100], int number_of_commands);
void close_pipes(int pipes[100], int number_of_commands);
void redirect_to_output(char * output, int * output_descriptor);
void redirect_to_input(char * input, int * input_descriptor);
void execute_command_with_argument(char * arguments[]);
void execute_command_with_first_process(int pipes[100], char * input, char * output, int number_of_commands, char * arguments[]);
void execute_command_with_last_process(int pipes[100], char * input, char * output, int current_process, int number_of_commands, char * arguments[]);
void execute_command_with_between_process(int pipes[100], int current_process, int number_of_commands, char * arguments[]);
void read_command(void);
void child_reaper(int ignore);
void parse_commands(char * commands[], int number_of_commands, char * input, char * output, int background);
void split(int pipes[100], char * input, char * output, int i, int number_of_commands, char * one_command[]);

extern char** environ;

void add_space_between_token(char * buffer)
{
    char new_buffer[1024];
    int i = 0, length = 0;
    for(; buffer[i] != '\0'; ++i)
    {
        switch(buffer[i])
        {
            case '|': case '<': case '>': case '&':
                new_buffer[length++] = ' ';//to separate them so that they can be read instead of stick to
                //something
                new_buffer[length++] = buffer[i];
                new_buffer[length++] = ' ';
                break;
            default:        new_buffer[length++] = buffer[i];
                break;
        }
    }
    new_buffer[length] = '\0';
    tokenize_into_commands_with_pipeline(new_buffer);
}

//1 is < ; 2 is > ; | is 3; & is 4; other is 0
int recognize_a_token(char * token)
{
    if (strcmp(token, "<") == 0)
        return 1;
    else if (strcmp(token, ">") == 0)
        return 2;
    else if (strcmp(token, "|") == 0)
        return 3;
    else if (strcmp(token, "&") == 0)
        return 4;
    return 0;
}

void tokenize_into_commands_with_pipeline(char * buffer)
{
    int number_arguments = 0, number_of_commands = 1, background = 0;
    char * commands[100], * input = NULL, * output = NULL;
    char * token = strtok(buffer, " ");//get the first word before space
    while(token)
    {
        switch(recognize_a_token(token))
        {
            case 0: commands[number_arguments++] = token;//storing the words before the sign
                break;
            case 1: input = strtok(NULL, " ");//go throught the rest of the token after the sign
                break;
            case 2: output = strtok(NULL, " ");
                break;
            case 3: commands[number_arguments++] = NULL;//put a null to replace pipe
                ++number_of_commands;//+1 because of pipe
                break;
            case 4: background = 1;//if there's & set backgound to 1
        }
        token = strtok(NULL, " ");//continue storing the words after the space
    }
    commands[number_arguments++] = NULL;//put a null at the end for execvp
    parse_commands(commands, number_of_commands, input, output, background);
}

void set_pipes(int pipes[100], int number_of_commands)
{
    int i;
    for(i = 0; i < number_of_commands - 1; ++i)//one pipe less than the number of commands
        pipe(pipes + 2 * i);//depending on the pipes, it's gonna create 2 * num of commands pipes
}
void close_pipes(int pipes[100], int number_of_commands)
{
    int i;
    for(i = 0; i < 2 * (number_of_commands - 1); ++i)//a pipe has two holes, and so it's always *2
        close(pipes[i]);//we are done using pipe
}

// just when >
void redirect_to_output(char * output, int * output_descriptor)
{
    if (output == NULL) return;
    *output_descriptor = open(output, O_WRONLY | O_CREAT, 0644);//open for write only or create if the file does not exits
    dup2(*output_descriptor, 1);//0644 is the permission bits of a file and these bits are only used when the file is actually created
}

// just when <
void redirect_to_input(char * input, int * input_descriptor)
{
    if (input == NULL) return;
    *input_descriptor = open(input, O_RDONLY);//use the pointer to open a file for read_only
    dup2(*input_descriptor, 0);//redirect the pointer to 0,0 for input , 1 for output
}

void execute_command_with_argument(char * arguments[])
{
    char path[256];
    strcpy(path,"PATH=");//
    char * envp[] = {strcat(path, getenv("PATH")), NULL};//concatinate path and get envnvironment, get the environment under path
    
    if(strcmp(arguments[0], "clr") == 0)
    {
        system("clear");
        return;
    }
    else if (strcmp(arguments[0], "environ") ==0)
    {
        char **var;
        for(var = environ; *var != NULL; ++var){
            printf("%s\n", *var);
        }
        return;
    }
    else if (strcmp(arguments[0], "cd") ==0)
    {
        chdir(arguments[1]);
        return;
    }
    else if(strcmp(arguments[0], "exit") ==0)
        kill(0, SIGKILL);
    // exit(101);
    else if (strcmp(arguments[0], "help") ==0)
    {
        system("help");
        return;
    }
    else if(strcmp(arguments[0], "pause") ==0)
    {
        getchar();
        return;
    }
    
    execvpe(arguments[0], arguments, envp);//arguments[0] is the first command, arguments is the pointer point to
    //the whole array, execvpe reads the whole command till it reachs a null
}

void execute_command_with_first_process(int pipes[100], char * input, char * output, int number_of_commands, char * arguments[])
{
    int output_descriptor, input_descriptor;
    dup2(pipes[1], 1);//let pipes[1] equals to 1
    if (input != NULL) redirect_to_input(input, &input_descriptor);//
    if (number_of_commands == 1 && output != NULL) redirect_to_output(output, &output_descriptor);
    close_pipes(pipes, number_of_commands);
    execute_command_with_argument(arguments);
    if (input != NULL) close(input_descriptor);//close a file descriptor
    if (number_of_commands == 1 && output != NULL) close(output_descriptor);
}

void execute_command_with_last_process(int pipes[100], char * input, char * output, int current_process, int number_of_commands, char * arguments[])
{
    int output_descriptor;
    dup2(pipes[2 * (current_process - 1)], 0);//
    if (output != NULL) redirect_to_output(output, &output_descriptor);
    close_pipes(pipes, number_of_commands);
    execute_command_with_argument(arguments);
    if (output != NULL) close(output_descriptor);
}

void execute_command_with_between_process(int pipes[100], int current_process, int number_of_commands, char * arguments[])
{
    dup2(pipes[2 * (current_process - 1)], 0);//
    dup2(pipes[2 * (current_process - 1) + 3], 1);
    close_pipes(pipes, number_of_commands);
    execute_command_with_argument(arguments);
}

void child_reaper(int ignore)
{
    signal(SIGCHLD, child_reaper);//google it
    wait(NULL);  //farther wait for son
}

void split(int pipes[100], char * input, char * output, int i, int number_of_commands, char * one_command[])
{
    if(fork() != 0) return;//if fork happen or not
    if (i == 0)
        execute_command_with_first_process(pipes, input, output, number_of_commands, one_command);
    else if (i == number_of_commands - 1)//doing the last command
        execute_command_with_last_process(pipes, input, output, i, number_of_commands, one_command);
    else
        execute_command_with_between_process(pipes, i, number_of_commands, one_command);
}

void parse_commands(char * commands[], int number_of_commands, char * input, char * output, int background)
{
    int status, i, index;
    int pipes[100];
    set_pipes(pipes, number_of_commands);//pipe is setted
    for(i = 0, index = 0; i < number_of_commands; ++i)
    {
        char * one_command[20];
        int j;
        for(j = 0; commands[index] != NULL; ++index, ++j)
            one_command[j] = commands[index];
        one_command[j] = commands[index++];//always storing null
        split(pipes, input, output, i, number_of_commands, one_command);//start to fork
    }
    close_pipes(pipes, number_of_commands);
    if (background)
        signal (SIGCHLD, child_reaper);
    else
    {
        for(i = 0; i < number_of_commands; ++i)
            wait(NULL);
    }
}

void read_command(void)
{
    char line[BUFFERSIZE];
    while(1)
    {
        printf("%% ");//prints a percent symbol
        if (gets(line) == EOF) break;
        add_space_between_token(line);
    }
}

int main()
{
    read_command();
    return 0;
}