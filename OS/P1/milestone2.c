#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char* readCommand(char* currentInput, char* lastInput){

    char current[1000];
    char past[1000];
    char cmd[6];
    char value[980];
    int exitNum; 
    char toReturn[1000];

    strcpy(current, currentInput);
    strcpy(past, lastInput);
    strncpy(cmd, current, 5);

    if (strcmp(cmd,"!!\n") == 0){

        if (past[0] != '\0'){
            printf("%s", past);
            readCommand(past, past);
        }   

    }

    else if (strcmp(cmd,"echo ") == 0){

        memset(value, '\0', strlen(value));     
        strncpy(value, &current[5], strlen(current) - 6);
        printf("%s\n", value); 

    }

    else if (strcmp(cmd,"exit ") == 0){

        //variables

        char recentInput[1000];
        char pastInput[1000] = "";
        char val[980];
        char readLine[1000];

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
        exit(atoi(value));

    }

    else if (strcmp(cmd,"\n")==0){

    }

    else {

        printf("bad command\n");

    }

    return cmd;

}

char* readFromFile(char* currentInput, char* lastInput){

    char current[1000];
    char past[1000];
    char cmd[6];
    char value[980];
    int exitNum; 

    strcpy(current, currentInput);
    strcpy(past, lastInput);
    strncpy(cmd, current, 5);

    if (strcmp(cmd,"echo ") == 0){

        memset(value, '\0', strlen(value));     
        strncpy(value, &current[5], strlen(current) - 6);
        printf("%s\n", value);        

    }

    else if (strcmp(cmd,"!!\n") == 0){

        if (past[0] != '\0'){

            readFromFile(past, past);

        }    
    }

    else if (strcmp(cmd,"exit ") == 0){

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

    return cmd;
    
}

int main(int argc, char *argv[]){

    //variables

    char recentInput[1000];
    char pastInput[1000] = "";
    char val[980];
    char readLine[1000];

    if (argc > 1){  
    
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

    else{

        printf("Starting IC shell\n");
        while(1){
            printf("icsh $ ");
            fgets(recentInput,100,stdin);
            if (strcmp(readCommand(recentInput, pastInput),"!!\n")!=0){
                memset(pastInput, '\0', strlen(pastInput));
                strcpy(pastInput, recentInput);
            }
        }

    }


}