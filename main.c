#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/wait.h>
#include <string.h>
#include <ctype.h>

#define MAX_LINE 80 /* 80 chars per line, per command, should be enough. */
#define MAX_BACKGROUND_PROCESS 20

int pathLength = 0;
char *paths[MAX_LINE];

/////////////////////////////////////

//For background processes
typedef struct{

    int process_pids[MAX_BACKGROUND_PROCESS];
    char process_names[MAX_BACKGROUND_PROCESS][50];
    int background_process_count;

}bg;

bg background_processes;

//For foreground process
int fg_process_pid = 0;
char fg_process_name[50];

//Others
int main_process_pid;

/////////////////////////////////////

void setup(char *, char **, int *);
void sigExitHandler();
void sigtstpHandler();
void createNewProcess(char **, int);
void checkBackgroundProcesses();
void fgCommand();
void parsePath();
void freePath();

char *splitText(char *, char, int);

int commands(char **);
int hashCodeForCommands(char *);

/////////////////////////////////////

int main(void){

    char inputBuffer[MAX_LINE]; /*buffer to hold command entered */
    int background; /* equals 1 if a command is followed by '&' */
    char *args[MAX_LINE / 2 + 1]; /*command line arguments */
    int commandsReturn;

    //FIRST THING FIRST
    main_process_pid = getpid();
    system("clear");

    //In the beginning, lets fill the path array.
    parsePath();

    //Also we need to handle some signals.
    signal(SIGINT, sigExitHandler);
    signal(SIGTSTP, sigtstpHandler);

    //Lets initialize background processes.
    background_processes.background_process_count = 0;
    for(int i = 0; i < MAX_BACKGROUND_PROCESS; i++) background_processes.process_pids[i] = 0;

    while(1){
        background = 0;
        printf("myshell: ");
        fflush(stdout);

        /*setup() calls exit() when Control-D is entered */
        setup(inputBuffer, args, &background);

        //If the user didn't enter anything, or entered a signal we need to continue the loop.
        if(args[0] == NULL) continue;

        // 0 for a command, 1 for exit, 2 for NOT A COMMAND
        commandsReturn = commands(args);

        if(commandsReturn == 1) //Means exit the loop.
            break;
        else if(commandsReturn == 2) //Which means the first argument is not an internal command.
            createNewProcess(args, background);

        //In each iteration, we need to check the states of background processes.
        checkBackgroundProcesses();
    }

    freePath();
}

/**
 * The setup function below will not return any value, but it will just: read
 * in the next command line; separate it into distinct arguments (using blanks as
 * delimiters), and set the args array entries to point to the beginning of what
 * will become null-terminated, C-style strings.
 */
void setup(char inputBuffer[], char *args[], int *background){

    int length,     /* # of characters in the command line */
            i,      /* loop index for accessing inputBuffer array */
            start,  /* index where beginning of next command parameter is */
            ct;     /* index of where to place the next parameter into args[] */

    ct = 0;

    /* read what the user enters on the command line */
    length = (int) read(STDIN_FILENO, inputBuffer, MAX_LINE);

    /* 0 is the system predefined file descriptor for stdin (standard input),
       which is the user's screen in this case. inputBuffer by itself is the
       same as &inputBuffer[0], i.e. the starting address of where to store
       the command that is read, and length holds the number of characters
       read in. inputBuffer is not a null terminated C-string. */

    start = -1;
    if(length == 0)
        exit(0);            /* ^d was entered, end of user command stream */

/* the signal interrupted the read system call */
/* if the process is in the read() system call, read returns -1
  However, if this occurs, errno is set to EINTR. We can check this  value
  and disregard the -1 value */
    if((length < 0) && (errno != EINTR)){
        perror("error reading the command");
        exit(-1);           /* terminate with error code of -1 */
    }

//    printf(">>%s<<", inputBuffer); //TO PRINT THE COMMAND BACK...

    for(i = 0; i < length; i++){ /* examine every character in the inputBuffer */

        switch(inputBuffer[i]){
            case ' ':
            case '\t' :               /* argument separators */
                if(start != -1){
                    args[ct] = &inputBuffer[start];    /* set up pointer */
                    ct++;
                }
                inputBuffer[i] = '\0'; /* add a null char; make a C string */
                start = -1;
                break;

            case '\n':                 /* should be the final char examined */
                if(start != -1){
                    args[ct] = &inputBuffer[start];
                    ct++;
                }
                inputBuffer[i] = '\0';
                args[ct] = NULL; /* no more arguments to this command */
                break;

            default :             /* some other character */
                if(start == -1)
                    start = i;
                if(inputBuffer[i] == '&'){
                    *background = 1;
                    inputBuffer[i - 1] = '\0';

                    //NOT TO INCLUDE & CHARACTER.
                    i++;
                    start = i;
                }
        } /* end of switch */
    }    /* end of for */
    args[ct] = NULL; /* just in case the input line was > 80 */

    // PRINTING ALL ARGUMENTS
//    for(i = 0; i <= ct; i++)
//        printf("args %d = %s\n", i, args[i]);

} /* end of setup routine */

/**
 * This function parses the path variable into workable array.
 */
void parsePath(){

    char *pathVar = getenv("PATH");

    // Until we encounter a NULL value in the path, we continue to parse.
    while(1){
        paths[pathLength] = splitText(pathVar, ':', pathLength);

        if(paths[pathLength] != NULL)
            pathLength++;
        else
            break;
    }

    //Now, we also need to add the location of myshell program to be able to execute programs in the same folder.
    char cwd[MAX_LINE];
    getcwd(cwd, sizeof(cwd));

    paths[pathLength] = strdup(cwd);
    pathLength++;
}

/**
  * This is an actual implementation of STRTOK function in string.h library.
  * We did it because using original strtok gave us error.
 */
char *splitText(char *text, char regex, int index){

    // We need four variable.
    char *indexPointer = text;
    int arrayIndex = 0;
    int indexRepetition = 0;

    char *returnValue = malloc(MAX_LINE * sizeof(char));

    /////////////////////////////////////////

    // While we're not on NULL...
    while(*indexPointer){

        //If we found a regex(: for ex) and we have the copy we need, we exit the loop.
        if(*indexPointer == regex && indexRepetition == index)
            break;
            //If we found a regex(: for ex) and we still haven't reach to the index we need, we iterate.
        else if(*indexPointer == regex && indexRepetition < index)
            indexRepetition++;

        //We do this not to copy the regex character.
        if(indexRepetition == index && *indexPointer != regex)
            returnValue[arrayIndex++] = *indexPointer;

        indexPointer++;
    }

    // If the return value is empty, we make it NULL.
    if(returnValue[0] == '\0')
        returnValue = NULL;

    return returnValue;
}

/**
 * This function checks the signal for Ctrl+C.
 */
void sigExitHandler(){

    if(fg_process_pid != 0){ //If a background process is running...

        //We will check if the process has exited.
        int status;
        pid_t exited_process = waitpid(fg_process_pid, &status, WNOHANG);

        if(exited_process < 0){

            //Means that process is already terminated.
            fg_process_pid = 0;

        }else{

            //Means that process is running now.
            kill(fg_process_pid, SIGKILL);

            printf("\n");
            return;
        }
    }

    //We won't do anything, just go to the newline.
    printf("\nmyshell: ");
    fflush(stdout);
}

/**
 * This function checks the signal for Ctrl+Z.
 */
void sigtstpHandler(){

    if(fg_process_pid != 0){ //If a background process is running...

        //We will check if the process has exited.
        int status;
        pid_t exited_process = waitpid(fg_process_pid, &status, WNOHANG);

        if(exited_process < 0){

            //Means that process is already terminated.
            fg_process_pid = 0;

        }else{

            //There is a foreground job running now.

            //We will move the foreground process to background. But there are a couple conditions.
            //First, lets stop the process for now.
            kill(fg_process_pid, SIGTSTP);

            //CONDITIONS
            //Did we reach the maximum background process count???
            if(background_processes.background_process_count == MAX_BACKGROUND_PROCESS){
                fprintf(stderr,
                        "There is no space for this process to run in the background!!! Running in the foreground...\n");
                kill(fg_process_pid, SIGCONT);
            }else{

                //Means, there is still room for background process. We will search for an empty slot.
                for(int i = 0; i < MAX_BACKGROUND_PROCESS; i++){

                    if(background_processes.process_pids[i] == 0){

                        // We need to add it to background_processes.
                        background_processes.process_pids[i] = fg_process_pid;
                        strcpy(background_processes.process_names[i], fg_process_name);
                        background_processes.background_process_count++;

                        //Lets switch back io ports to the main shell.
                        tcsetpgrp(STDIN_FILENO, getpgid(main_process_pid));
                        tcsetpgrp(STDOUT_FILENO, getpgid(main_process_pid));

                        printf("\nStopped the process with the name \"%s\"...\n", fg_process_name);

                        fg_process_pid = 0;
                        break;
                    }
                }
            }

            printf("\n");
            return;
        }
    }

    //We won't do anything, just go to the newline.
    printf("\nmyshell: ");
    fflush(stdout);
}

/**
 * This function creates a new process and run the program given in the argument.
 */
void createNewProcess(char *args[], int background){

    pid_t childpid;

    childpid = fork();

    if(childpid == -1){

        perror("There was an error while forking the child!!!");
        return;

    }else if(childpid != 0){                        // PARENT PART

        if(background == 1){

            //If this is the case, we are in the parent process and the child process wants to run in background. But is there any empty slot???
            if(background_processes.background_process_count == MAX_BACKGROUND_PROCESS){
                fprintf(stderr,
                        "There is no space for this process to run in the background!!! Running in the foreground...\n");
                waitpid(childpid, NULL, WUNTRACED);
            }else{

                //Else, we will move its process into its own process group. Also we need to find an empty place in background_processes array.
                for(int i = 0; i < MAX_BACKGROUND_PROCESS; i++){

                    if(background_processes.process_pids[i] == 0){

                        setpgid(childpid, childpid);
                        // We need to add it to background_processes.
                        background_processes.process_pids[i] = childpid;
                        strcpy(background_processes.process_names[i], args[0]);
                        background_processes.background_process_count++;
                        break;
                    }
                }
            }
        }else{

            //Means background is 0. We will set the variables.
            fg_process_pid = childpid;
            strcpy(fg_process_name, args[0]);

            // We will wait the process.
            if(childpid != waitpid(childpid, NULL, WUNTRACED))
                perror("Parent failed while waiting the child due to a signal or error!!!");
        }
    }else{                                              // CHILD PART

        //We need to execute execl for every path.
        char path[MAX_LINE];

        for(int i = 0; i < pathLength; i++){

            strcpy(path, paths[i]);

            //First we need to append '/';
            strcat(path, "/");

            //Now, we append the file name into the path which is the first argument.
            strcat(path, args[0]);

            //Now, we execute.
            execl(path, args[0], args[1], args[2], args[3], args[4], args[5], args[6], args[7], args[8], args[9],
                        args[10], args[11], args[12], args[13], args[14], args[15], args[16], args[17], args[18], args[19], NULL);
        }

        //Error control part.
        perror("Child failed to execute execl command");
        exit(errno);
    }
}

/**
 * This function checks all background processes and if they are terminated, we delete them from the list.
 */
void checkBackgroundProcesses(){

    if(background_processes.background_process_count == 0)
        return;

    //Variables
    int status;
    pid_t exited_process;

    for(int i = 0; background_processes.background_process_count > 0 && i < MAX_BACKGROUND_PROCESS; i++){

        if(background_processes.process_pids[i] != 0){

            exited_process = waitpid(background_processes.process_pids[i], &status, WNOHANG);

            if(exited_process > 0){

                //Means that process is terminated.
                fprintf(stderr, "\nThe process with the p_id = %d and the name = \"%s\" is terminated!!\n",
                            background_processes.process_pids[i], background_processes.process_names[i]);

                background_processes.process_pids[i] = 0;
                background_processes.background_process_count--;
            }
        }
    }
}

/**
 * This function interprets all commands.
 */
int commands(char *args[]){

    // Firstly, lets convert the first argument into full lower-case format.
    for(int i = 0; i < strlen(args[0]); i++) args[0][i] = (char) tolower(args[0][i]);

    switch(hashCodeForCommands(args[0])){
        case 0: // ALIAS
            return 0;
        case 1: // UNALIAS
            return 0;
        case 2: // CLR

            system("clear");

            return 0;
        case 3: // FG

            fgCommand();

            return 0;
        case 4: // EXIT

            //Before exiting, we need to check if there is any process running in the background. If so, we will not let the user exit.
            checkBackgroundProcesses();

            if(background_processes.background_process_count > 0){
                fprintf(stderr, "%d process(es) running in the background. Please make sure it is close before exiting the shell.\n", background_processes.background_process_count);
                return 0;
            }

            printf("There is no running background process\n");
            return 1;
        default:
            break;
    }

    return 2;
}

/**
 * This function represents the behaviour of fg command which moves all background processes into foreground processes.
 */
void fgCommand(){

    //We exit the function if there are no background processes.
    if(background_processes.background_process_count == 0){
        fprintf(stderr, "There is no process running in the background!!\n");
        return;
    }

    //We need to move all background processes to foreground.
    for(int i = 0; background_processes.background_process_count > 0 && i < MAX_BACKGROUND_PROCESS; i++){

        //If that slot is not assigned to a process, we jump over.
        if(background_processes.process_pids[i] == 0)
            continue;

        //First of all, until this point the user might have closed the process, so we need to check it.
        checkBackgroundProcesses();

        //If that process is closed, we can skip over.
        if(background_processes.process_pids[i] == 0) continue;

        //Printing the process name.
        printf("The process with the p_id = %d and the name \"%s\" is running in the foreground now...\n",
                    background_processes.process_pids[i], background_processes.process_names[i]);

        //Before moving the process to the foreground, we stop it.
        kill(background_processes.process_pids[i], SIGTSTP);

        //Setting the io_ports to the same as the main process.
        tcsetpgrp(main_process_pid, background_processes.process_pids[i]);

        //After moving the process to the foreground, we continue it.
        kill(background_processes.process_pids[i], SIGCONT);

        //And waiting the process to end. Also we need to set the variables because it is a foreground process now.
        fg_process_pid = background_processes.process_pids[i];
        strcpy(fg_process_name, background_processes.process_names[i]);
        waitpid(fg_process_pid, NULL, WUNTRACED);

        printf("\nProcess terminated!!!\n");

        //Setting the variables back.
        background_processes.process_pids[i] = 0;
        background_processes.background_process_count--;
        fg_process_pid = 0;
    }

    //And lastly, we need to set the shell back.
    tcsetpgrp(STDIN_FILENO, main_process_pid);
    tcsetpgrp(STDOUT_FILENO, main_process_pid);
}

/**
 * This function is for comparing string commands.
 */
int hashCodeForCommands(char *arg){

    if(strcmp(arg, "alias") == 0)
        return 0;
    else if(strcmp(arg, "unalias") == 0)
        return 1;
    else if(strcmp(arg, "clr") == 0)
        return 2;
    else if(strcmp(arg, "fg") == 0)
        return 3;
    else if(strcmp(arg, "exit") == 0)
        return 4;

    return -1;
}

/**
 * This function frees the allocated memory for the path variables.
 */
void freePath(){

    for(int i = 0; i < pathLength; i++){
        free(paths[i]);
    }
}
