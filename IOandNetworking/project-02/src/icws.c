#include<sys/types.h>
#include<sys/socket.h>
#include<sys/stat.h>
#include<netdb.h>
#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<unistd.h>
#include <fcntl.h>
#include <time.h>
#include <getopt.h>
#include "parse.h"
#include "pcsa_net.h"

/* Rather arbitrary. In real life, be careful with buffer overflow */
#define MAXBUF 8192

char *port;
char *root;

typedef struct sockaddr SA;

// struct date dt;
// getdate(&dt);
// time_t tme;
// tme = time(NULL);
// struct tm *tm = localtime(&tme);
// time_t now;

char* date(){
    char d[100];
    time_t t;
    time(&t);

    struct tm *local = localtime(&t);
    sprintf(d, "%d/%d/%d", local->tm_mday, local->tm_mon + 1, local->tm_year + 1900);
    
    return strdup(d);
}

char* mimeT(char* req_obj){ //get mime type from the extension

    char *ext = strchr(req_obj, '.') + 1;   //get extension
    char *mimeType;
    //add while loop checking null
    
    // check extension and get mime type
    if (strcmp(ext, "html") == 0){
        mimeType = "text/html";
    }
    else if (strcmp(ext, "css") == 0){
        mimeType = "text/css";
    }
    else if (strcmp(ext, "txt") == 0){
        mimeType = "text/plain";
    }
    else if (strcmp(ext, "jpg") == 0){
        mimeType = "image/jpeg";
    }
    else if (strcmp(ext, "png") == 0){
        mimeType = "image/png";
    }
    else if (strcmp(ext, "js") == 0){
        mimeType = "application/javascript";
    }    
    else{
        mimeType = "null";
    }

    return strdup(mimeType);

}

char* fileLoc(char* rootFol, char* req_obj){ //get file location

    char loc[MAXBUF]; // full file path
    strcpy(loc, rootFol); // loc = rootFol

    if (strcmp(req_obj, "/") == 0){ // if input == / then req_obj = /index
        req_obj = "/index.html";
    } 
    else if (req_obj[0] != '/'){      // add '/' if first char is not '/'
        strcat(loc, "/");   
    }
    strcat(loc, req_obj);   //add file afterward
    // printf("File location is: %s \n", loc);

    return strdup(loc);
    
}

char* getFile(char* req_obj){

    if (strcmp(req_obj, "/") == 0){ // if input == / then req_obj = /index
        req_obj = "/index.html";
    } 
    return strdup(req_obj);

}

void getHeader(int fd, int connFd, char* req_obj){ //get header

    struct stat st;
    fstat(fd, &st);
    size_t filesize = st.st_size;

    char headr[MAXBUF]; // buffer for header
    char* file = getFile(req_obj);

    // printf("int fd from getHeader: %d\n", fd);

    if (fd < 0){
        sprintf(headr, 
                "HTTP/1.1 404 not found\r\n"
                "Date: %s \r\n"
                "Server: ICWS\r\n"
                "Connection: close\r\n", date());
        write_all(connFd, headr, strlen(headr));
        return;
    }

    if (filesize < 0){
        printf("Filesize Error\n");
        close(fd);
        return;
    }
  
    if (strcmp(mimeT(req_obj), "null") == 0){
        char * msg = "File type not supported\n";
        write_all(connFd, msg , strlen(msg) );
        close(fd);
        return;
    }

    sprintf(headr, 
            "HTTP/1.1 200 OK\r\n"
            "Date: %s \r\n"
            "Server: ICWS\r\n"
            "Connection: close\r\n"
            "Content-length: %lu\r\n"
            "Content-type: %s\r\n"
            "Last-Modified: %s\r\n\r\n", date(), filesize, mimeT(file), ctime(&st.st_mtime), mimeT(req_obj));
            // , filesize, ctime(&now), mimeT(req_obj));
            // tm.tm_mday, tm.tm_mon+1, tm.tm_year+1900, filesize, ctime(&st.st_mtime)), mimeT(req_obj));

    write_all(connFd, headr, strlen(headr));

}

void respond_get(int connFd, char* rootFol, char* req_obj) { //running get

    char* fileLocation = fileLoc(rootFol, req_obj);
    printf("fileLoc from respond_get: %s\n", fileLocation);
    int fd = open(fileLocation, O_RDONLY);
    printf("value of fd from respond_get: %d\n", fd);

     // ====================================

    struct stat st;
    fstat(fd, &st);

    // ====================================

    char* file = getFile(req_obj);

    getHeader(fd, connFd, file);

    char buf[st.st_size];

    ssize_t numRead;
    while ((numRead = read(fd, buf, MAXBUF)) > 0) {
        write_all(connFd, buf, numRead);
    }

    if ( (close(fd)) < 0 ){
        printf("Failed to close input file. Meh.\n");
    }    

}

void respond_head(int connFd, char* rootFol, char* req_obj){ //running head

    char* fileLocation = fileLoc(rootFol, req_obj);
    // printf("fileLoc from respond_head: %s\n", fileLocation);
    int fd = open(fileLocation, O_RDONLY);
    char* file = getFile(req_obj);
    getHeader(fd, connFd, file);
    // printf("value of fd from respond_head: %d\n", fd);

    if ( (close(fd)) < 0 ){
        // printf("value of fd from if close fd in respond_head < 0: %d\n", fd);
        printf("Failed to close input file. Meh.\n");
    }   

}

void serve_http(int connFd, char* rootFol) {

    // printf("in serve_http\n");

    char buffer[MAXBUF];
    memset(buffer, '\0', 8192);
    char line[MAXBUF];
    char headr[MAXBUF];
    int readLine;
    char TwopastLine[MAXBUF];
    
    while ((readLine = read(connFd, line, 8192)) > 0){

        // printf("%d\n", readLine);
        // printf("IN READ LOOP\n");
        // printf("==================================\n");
        strcat(buffer, line);
        char lastTwo[2];
        strncpy(lastTwo, &line[strlen(line)-2], 2);
        // printf("LAST TWO CHARS: %s\n", lastTwo);
        
        if ((strcmp(line, "\r\n") == 0) && (strcmp(TwopastLine, "\r\n") == 0)){        //*  trying compare the last 4 bytes;
            // printf("it reaches the end!!!!\n");
            break;
        }

        // printf("BUFFER: %s\n", buffer);
        memset(TwopastLine, '\0', MAXBUF);
        strcpy(TwopastLine, lastTwo);
        memset(line, '\0', MAXBUF);
    }

    Request *req = parse(buffer, MAXBUF, connFd);    //(buffer, MAXBUF, connFd);
    // memset(buffer, '\0', MAXBUF);
 
    // printf("done parsing\n");
    
    if (req == NULL){

        printf("LOG: Failling to parse a request");
        sprintf(headr, 
                "HTTP/1.1 400 Request Parsing Failed\r\n"
                "Date: %s \r\n"
                // "Date: %d/%d/%d\r\n"
                "Server: ICWS\r\n"
                "Connection: close\r\n\r\n", date());
                // , tm.tm_mday, tm.tm_mon+1, tm.tm_year+1900);

        write_all(connFd, headr, strlen(headr));
        memset(buffer, '\0', MAXBUF);
        memset(headr, '\0', MAXBUF);
        memset(TwopastLine, '\0', MAXBUF);
        return;

    }

    else if (strcasecmp(req->http_version, "HTTP/1.1") == 0) {
          
        // printf("HTTP METHOD: %s\n", req->http_method);
        // printf("HTTP VERSION: %s\n", req->http_version);
        // printf("HTTP URI: %s\n", req->http_uri);

        if (strcasecmp(req->http_method, "GET") == 0) {

            printf("LOG: Running via GET method\n");
            respond_get(connFd, rootFol, req->http_uri);

        }

        else if (strcasecmp(req->http_method, "HEAD") == 0) {

            printf("LOG: Running via HEAD method\n");
            respond_head(connFd, rootFol, req->http_uri);

        }

        else {

            printf("LOG: Unknown request\n");
            sprintf(headr, 
                    "HTTP/1.1 501 Method Unimplemented\r\n"
                    "Date: %s\r\n"
                    // "Date: %d/%d/%d\r\n"
                    "Server: ICWS\r\n"
                    "Connection: close\r\n", date());
                    // , tm.tm_mday, tm.tm_mon+1, tm.tm_year+1900);
            write_all(connFd, headr, strlen(headr));
        
        }

    }

    else{

        printf("LOG: Unsupported HTTTP Version");
        sprintf(headr, 
                "HTTP/1.1 505 HTTP Version Not Supported\r\n"
                // "Date: %d/%d/%d\r\n"
                "Server: ICWS\r\n"
                "Connection: close\r\n");
                // , tm.tm_mday, tm.tm_mon+1, tm.tm_year+1900);
        write_all(connFd, headr, strlen(headr));
        free(req->headers);
        free(req);
        memset(buffer, '\0', MAXBUF);
        memset(headr, '\0', MAXBUF);
        memset(TwopastLine, '\0', MAXBUF);
        return;

    } 

    free(req->headers);
    free(req);
    memset(buffer, '\0', MAXBUF);
    memset(headr, '\0', MAXBUF);
    memset(TwopastLine, '\0', MAXBUF);

}

struct survival_bag {

    struct sockaddr_storage clientAddr;
    int connFd;

};

char* dirName;

// void* conn_handler(void *args) {

//     struct survival_bag *context = (struct survival_bag *) args;
    
//     pthread_detach(pthread_self());
//     serve_http(context->connFd, dirName);

//     close(context->connFd);
    
//     free(context); /* Done, get rid of our survival bag */

//     return NULL; /* Nothing meaningful to return */

// }

int main(int argc, char* argv[]) {

    static struct option long_ops[] =
    {
        {"port", required_argument, NULL, 'p'},
        {"root", required_argument, NULL, 'r'}, 
        {NULL, 0, NULL, 0}
    };

    int ch;
    while ((ch = getopt_long(argc, argv, "p:r:", long_ops, NULL)) != -1){

        switch (ch)
        {

            case 'p':
                printf("port: %s\n", optarg);
                port = optarg;
                break;

            case 'r':
                printf("root: %s\n", optarg);
                root = optarg;
                break;

        }
  
    }

    int listenFd = open_listenfd(port);

    for (;;) {
        struct sockaddr_storage clientAddr;
        socklen_t clientLen = sizeof(struct sockaddr_storage);

        int connFd = accept(listenFd, (SA *) &clientAddr, &clientLen);
        if (connFd < 0) { fprintf(stderr, "Failed to accept\n"); continue; }

        // struct survival_bag *context = 
        //             (struct survival_bag *) malloc(sizeof(struct survival_bag));
        // context->connFd = connFd;
        
        // memcpy(&context->clientAddr, &clientAddr, sizeof(struct sockaddr_storage));

        char hostBuf[MAXBUF], svcBuf[MAXBUF];
        if (getnameinfo((SA *) &clientAddr, clientLen, 
                        hostBuf, MAXBUF, svcBuf, MAXBUF, 0) == 0) 
            printf("Connection from %s:%s\n", hostBuf, svcBuf);
        else
            printf("Connection from ?UNKNOWN?\n");

        //pthread_create(&threadInfo, NULL, conn_handler, (void *) context);

        serve_http(connFd, root);
        close(connFd);


    }

    return 0;

}


/*
Progress Checking Point

MILESTONE 1
- Add Date and Last Modified Time
- Already fix reading part but more bugs to fix ;–;

MILESTONE 2
- Already copied the survival back and thread pull from micro_cc; CHECKED

*/

/*
Bugs:

- After one bad command, it all returns to be bad command/ getting bug. :|
- GET / HTTP/1.1
- HTTP version failed 

*/



/*
References

* https://www.techiedelight.com/print-current-date-and-time-in-c/

*/