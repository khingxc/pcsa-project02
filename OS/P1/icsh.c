#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <signal.h>
#include <errno.h>
#include <fcntl.h>

// Redirection
int inRedirectPresentAt(char** args);
int outRedirectPresentAt(char** args);

// redirection stuff
int outRedirectPresentAt(char** args){

    for (int i = 0; args[i] != NULL; i++) {
        if (strcmp(args[i], ">") == 0) {
            return i;
        }
    }

    return -1;

}

int inRedirectPresentAt(char** args){
    
    for (int i = 0; args[i] != NULL; i++) {

        if (strcmp(args[i], "<") == 0) {

            return i;

        }

    }

    return -1;
}

struct jobs{

    int jobID;
    int pid;
    char command[1000];

};

struct jobs jobsList[100];


char* readCommand(char* currentInput, char* lastInput){

    char current[1000];
    char past[1000];
    char cmd[6];
    char value[980];
    int exitNum; 
    char toReturn[1000];
    char* inFile;
    char* outFile;
    FILE* infp;
    FILE* outfp;
    int inRedirect;
    int outRedirect; 
    char temp[6];

    strcpy(current, currentInput);
    strcpy(past, lastInput);
    strncpy(cmd, current, 5);
    strcpy(temp, cmd);
    //printf("%s\n", cmd);

    if (strcmp(cmd,"!!\n") == 0){

        if (past[0] != '\0'){

            printf("%s", past);
            readCommand(past, past);

        }

    }

    else if (strcmp(cmd,"echo ") == 0){

	printf("command from temp: %s\n", temp); 
        printf("command from cmd: %s\n", cmd);	
        memset(value, '\0', strlen(value));     
        strncpy(value, &current[5], strlen(current) - 5);
        printf("%s\n", value);
	printf("command from temp after print val: %s\n", temp);
	printf("command from cmd after print val: %s\n", cmd);
       // printf("command in Echo if: %s", cmd);	

    }

    else if (strcmp(cmd,"exit ") == 0){

        strncpy(value, &current[5], strlen(current) - 5);
        exitNum = atoi(value);
        if (exitNum < 0){
            exitNum = 0;
        }
        else if (exitNum > 255){
            exitNum = 255;
        }
        printf("-bye-\n");        
        exit(exitNum);

    }

    else if (strcmp(cmd,"\n")==0){

    }

    else{

        int status;
        int pid;
        char *argumentLst[300];
        int i = 0;
        char *argument = strtok(strtok(current, "\n"), " ");
        int backgroundCheck = 0;    //foreground

        while (argument != NULL){

            if (strcmp(argument,"&")==0){
                backgroundCheck = 1;       //background
                break;
            }
            argumentLst[i] = argument;
            i++;
            argument = strtok(NULL, " ");

        }
        
        argumentLst[i] = NULL;

        if ((pid=fork()) < 0){

            perror("Fork failed");
            exit(-1);

        }
        if (!pid){                              //child

            struct sigaction sigtstp_default;
            struct sigaction sigint_default;

            sigtstp_default.sa_handler = SIG_DFL;
            sigint_default.sa_handler = SIG_DFL;
            sigtstp_default.sa_flags = 0;
            sigint_default.sa_flags = 0;
            sigemptyset(&sigtstp_default.sa_mask);
            sigemptyset(&sigint_default.sa_mask);
            sigaction(SIGTSTP, &sigtstp_default, NULL);
            sigaction(SIGINT, &sigint_default, NULL);

            setpgid(pid, pid);

            //if foreground, call tcsetpgrp(0,pid)
            if (backgroundCheck == 0){
                tcsetpgrp(0, pid);  
            }
            
            IORedirection(argumentLst);

            execvp(argumentLst[0], argumentLst);
            printf("bad command\n");
            exit(0);

        }

        if (pid){                                   //parent

            setpgid(pid, pid);
            if (backgroundCheck == 0){      //foreground
                tcsetpgrp(0, pid);
                int stat;
                waitpid(pid, &stat, WUNTRACED); 
                tcsetpgrp(0, getpid()); 
            }
            else{

                int k;

                for(int d = 1; d < 100; d++){

                    if (jobsList[d].jobID == 0){

                        jobsList[d].jobID = d;
                        k = d;
                        jobsList[d].pid = pid;
                        strcpy(jobsList[d].command, current);
                        break;
                 
                    }
                   
                }

                printf("[%d] %d\n", jobsList[k].jobID, jobsList[k].pid); 

            }
           

        }

    }

    printf("before return: %s", temp);
    return temp;
}

char* readFromFile(char* currentInput, char* lastInput){

    char current[1000];
    char past[1000];
    char cmd[6];
    char value[980];
    int exitNum; 
    char* inFile;
    char* outFile;
    FILE* infp;
    FILE* outfp;
    int inRedirect;
    int outRedirect;
    char temp[6];

    strcpy(current, currentInput);
    strcpy(past, lastInput);
    strncpy(cmd, current, 5);
    strcpy(temp, cmd);

    if (strcmp(cmd,"echo ") == 0){

        printf("echo successed with cmd: %s\n", temp);
        memset(value, '\0', strlen(value));     
        strncpy(value, &current[5], strlen(current) - 5);
        printf("%s\n", value);        
	//printf("echo successed with cmd: %s\n", temp);

    }

    else if (strcmp(cmd,"!!\n") == 0){

        if (past[0] != '\0'){

            readFromFile(past, past);

        }    
    }

    else if (strcmp(cmd,"exit ") == 0){

        strncpy(value, &current[5], strlen(current) - 5);
        exitNum = atoi(value);
        if (exitNum < 0){

            exitNum = 0;

        }
        else if (exitNum > 255){

            exitNum = 255;

        }
        exit(exitNum);
    }

    else if (strcmp(cmd,"\n")==0){

    }

    else{

        int status;
        int pid;
        char *argumentLst[300];
        int i = 0;
        char *argument = strtok(strtok(current, "\n"), " ");

        while (argument != NULL){

            argumentLst[i] = argument;
            i++;
            argument = strtok(NULL, " ");

        }
        
        argumentLst[i] = NULL;

        if ((pid=fork()) < 0){

            perror("Fork failed");
            exit(-1);

        }
        if (!pid){

            struct sigaction sigtstp_default;
            struct sigaction sigint_default;

            sigtstp_default.sa_handler = SIG_DFL;
            sigint_default.sa_handler = SIG_DFL;
            sigtstp_default.sa_flags = 0;
            sigint_default.sa_flags = 0;
            sigemptyset(&sigtstp_default.sa_mask);
            sigemptyset(&sigint_default.sa_mask);
            sigaction(SIGTSTP, &sigtstp_default, NULL);
            sigaction(SIGINT, &sigint_default, NULL);

            setpgid(pid, pid);
            tcsetpgrp(0, pid);

            IORedirection(currentInput);

            execvp(argumentLst[0], argumentLst);
            printf("bad command\n");
            exit(EXIT_FAILURE);

        }

        if (pid){

            setpgid(pid, pid);
            tcsetpgrp(0, pid);
            int stat;
            waitpid(pid, &stat, WUNTRACED);
            tcsetpgrp(0, getpid());

        }
    }

    return temp;
    
}

void child_handler(int sig){

    int ccount = 0;
    int child_status;
    pid_t pid = wait(&child_status);
    ccount--;

}

int IORedirection(char** inputLst){

    char* inFile;
    char* outFile;
    FILE* infp;
    FILE* outfp;
    int inRedirect;
    int outRedirect;

    // check for redirection
    inRedirect = inRedirectPresentAt(inputLst);
    outRedirect = outRedirectPresentAt(inputLst);

    // redirection
    if (inRedirect >= 0) {

        inFile = inputLst[inRedirect + 1];
        inputLst[inRedirect] = NULL;
        infp = freopen(inFile, "r", stdin);

    }

    if (outRedirect >= 0) {

        outFile = inputLst[outRedirect + 1];
        inputLst[outRedirect] = NULL;
        outfp = freopen(outFile, "w", stdout);

    }

}

int main(int argc, char *argv[]){

    struct sigaction sigtstp_default;
    struct sigaction sigint_default;
    struct sigaction sa;
    struct sigaction childAction;

    sigtstp_default.sa_handler = SIG_IGN;
    sigint_default.sa_handler = SIG_IGN;
    sa.sa_handler = SIG_IGN;
    childAction.sa_handler = child_handler;

    sigtstp_default.sa_flags = 0;
    sigint_default.sa_flags = 0;
    sa.sa_flags = 0;
    childAction.sa_flags = 0;

    sigemptyset(&sigtstp_default.sa_mask);
    sigemptyset(&sigint_default.sa_mask);
    sigemptyset(&sa.sa_mask);
    sigemptyset(&childAction.sa_mask);

    sigaction(SIGTSTP, &sigtstp_default, NULL);
    sigaction(SIGINT, &sigint_default, NULL);
    sigaction(SIGTTOU, &sa, NULL);
    sigaction(SIGCHLD, &childAction, NULL);

    //variables

    char recentInput[1000];
    char pastInput[1000] = "";
    char val[980];
    char readLine[1000];

    //if there is input after ./icsh [read file]
    if (argc > 1 && (strcmp(argv[0], "./icsh") == 0)){  
        
        FILE* fileOpen = fopen(argv[1],"r");
        if (fileOpen == NULL){
            exit(-1);
        }
        while (fgets(readLine, 1000, fileOpen) > 0){
            strcpy(recentInput, readLine);
            if (strcmp(readFromFile(recentInput, pastInput),"!!\n")!=0){
                memset(pastInput, '\0', strlen(pastInput));
                strcpy(pastInput, recentInput);
            }  
        }
        fclose(fileOpen);

    }

    //if it asks to run only a command  
    else{

        char temp[1000];
        printf("Starting IC shell\n");
        while(1){
            printf("icsh $ ");
            //recentInput[0] = '\0';
            fgets(recentInput,100,stdin);
            // printf("%s\n", recentInput);
            // printf("\0");
            //strcpy(temp, readCommand(recentInput, pastInput));
	    printf("come here\n");
	    //readCommand(recentInput, pastInput);
            if (strcmp(readCommand(recentInput, pastInput),"!!\n")!=0){
		printf("no bang if");
                memset(pastInput, '\0', strlen(pastInput));
		printf("before cp");
                strcpy(pastInput, recentInput);

   	    }
	}
    }

    return 0;

}

/*
reference(s):
- IO Redirection: https://github.com/linuxartisan/basicshell/blob/master/basicshell.c
*/
