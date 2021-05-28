#include <stdio.h>
#include <stdlib.h>
#include <string.h>

//issue: save !!, didn't go through if conditions

//need to fix bang2

    // for command only: bang bang goes to the bad commands only
    // for the read file: it doesn't save (will edit later)

int readCommand(char* currentInput, char* lastInput, int boolean){

    char current[1000];
    char past[1000];
    char cmd[20];
    char value[980];
    int exitNum; 
    char toReturn[1000];

    strcpy(current, currentInput);
    strcpy(past, lastInput);
    strncpy(cmd, current, 5);

    if (strcmp(cmd,"echo ") == 0){
        memset(value, '\0', strlen(value));     
        strncpy(value, &current[5], strlen(current) - 6);
        printf("%s\n", value);                                               
    }

    else if (strcmp(cmd,"!!\n") == 0){

        boolean = 1;

        if (past[0] == '\0'){

        }

        else{
            printf("%s", past);
            readCommand(past, past, boolean);
        }        
    }

    else if (strcmp(cmd,"exit ") == 0){
        boolean = 1;
        strncpy(value, &current[5], strlen(current) - 6);
        exitNum = atoi(value);
        if (exitNum < 0){
            exitNum = 0;
        }
        else if (exitNum > 255){
            exitNum = 255;
        }
        printf("-bye-\n");        
        printf("exit code: %d\n", exitNum);
        exit(exitNum);
    }

    else if (strcmp(cmd,"\n")==0){

    }

    else{
        printf("bad command\n");
    }

    return boolean;
}

int readFromFile(char* currentInput, char* lastInput, int boolean){

    char current[1000];
    char past[1000];
    char cmd[20];
    char value[980];
    int exitNum; 

    strcpy(current, currentInput);
    strcpy(past, lastInput);
    strncpy(cmd, current, 5);

    if (strcmp(cmd,"echo ") == 0){
        memset(value, '\0', strlen(value));     
        strncpy(value, &current[5], strlen(current) - 6);
        printf("%s\n", value);  
        // strcpy(returnCmd, current);                                                     
    }

    else if (strcmp(cmd,"!!\n") == 0){

        boolean = 1;

        if (past[0] == '\0'){

        }

        else{
            readFromFile(past, past, boolean);
        }        
    }

    else if (strcmp(cmd,"exit ") == 0){
        boolean = 1;
        strncpy(value, &current[5], strlen(current) - 6);
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
        printf("bad command\n");
    }

    return boolean;

}

int main(int argc, char *argv[]){

    //variables

    char recentInput[1000];
    char pastInput[1000] = "";
    char val[980];
    char readLine[1000];
    int boolean = 0;


    //if there is input after ./icsh [read file]
    if (argc > 1){  
        FILE* fileOpen = fopen(argv[1],"r");
        if (fileOpen == NULL){
            exit(-1);
        }
        while (fgets(readLine, 1000, fileOpen) > 0){
            //fputs(readLine, stdout);
            strcpy(recentInput, readLine);
            readFromFile(recentInput, pastInput, boolean);

            ///FIX HERE PLS///
            // memset(pastInput, '\0', strlen(pastInput));
            // strcpy(pastInput, recentInput);
            // if (readFromFile(recentInput, pastInput, boolean) == 0){
            //     memset(pastInput, '\0', strlen(pastInput));
            //     strcpy(pastInput, recentInput);
            // }     
        }
        fclose(fileOpen);
    }

    //if it asks to run only a command  
    else{
        printf("Starting IC shell\n");
        while(1){
            printf("icsh $ ");
            fgets(recentInput,100,stdin);
            //readCommand(recentInput, pastInput, boolean);  
            if (readCommand(recentInput, pastInput, boolean) == 0){
                memset(pastInput, '\0', strlen(pastInput));
                strcpy(pastInput, recentInput);
            }
        }
    }

    return 0;
    
}