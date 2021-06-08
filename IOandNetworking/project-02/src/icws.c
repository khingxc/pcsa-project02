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

const char *port;
const char *root;

typedef struct sockaddr SA;

char* mimeT(char* req_obj){ //get mime type from the extension

    char *ext = strchr(req_obj, '.') + 1;
    char *mimeType;
    
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

    return mimeType;

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
    strcat(loc, req_obj);
    printf(" File location is: %s \n", loc);

    return loc;
    
}

void getHeader(int fd, int connFd, char* req_obj){ //get header

    struct stat st;
    fstat(fd, &st);
    size_t filesize = st.st_size;

    char headr[MAXBUF]; // buffer for header

    if (fd < 0){
        sprintf(headr, 
                "HTTP/1.1 404 not found\r\n"
                "Server: icws\r\n"
                "Connection: close\r\n");
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
            "Server: icws\r\n"
            "Connection: close\r\n"
            "Content-length: %lu\r\n"
            "Content-type: %s\r\n\r\n", filesize, mimeT(req_obj));

    write_all(connFd, headr, strlen(headr));

}

void respond_get(int connFd, char* rootFol, char* req_obj) { //running get

    int fd = open(fileLoc(rootFol, req_obj), O_RDONLY);

    // getHeader(fd, connFd, req_obj);

    // // ====================================

    // struct stat st;
    // fstat(fd, &st);

    // // ====================================


    struct stat st;
    fstat(fd, &st);
    size_t filesize = st.st_size;

    char headr[MAXBUF]; // buffer for header

    if (fd < 0){
        sprintf(headr, 
                "HTTP/1.1 404 not found\r\n"
                "Server: icws\r\n"
                "Connection: close\r\n");
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
            "Server: icws\r\n"
            "Connection: close\r\n"
            "Content-length: %lu\r\n"
            "Content-type: %s\r\n\r\n", filesize, mimeT(req_obj));

    write_all(connFd, headr, strlen(headr));

    char buf[filesize];
    // char buf[filesize];
    ssize_t numRead;
    while ((numRead = read(fd, buf, MAXBUF)) > 0) {
        write_all(connFd, buf, numRead);
    }

    if ( (close(fd)) < 0 ){
        printf("Failed to close input file. Meh.\n");
    }    
}

void respond_head(int connFd, char* rootFol, char* req_obj){ //running head

    int fd = open(fileLoc(rootFol, req_obj), O_RDONLY);

    getHeader(fd, connFd, req_obj);

    if ( (close(fd)) < 0 ){
        printf("Failed to close input file. Meh.\n");
    }   

}

void serve_http(int connFd, char* rootFol) {

    char buffer[MAXBUF];
    char line[MAXBUF];
    char headr[MAXBUF];

    // if (!read_line(connFd, buffer, MAXBUF))
    //     return ;  /* Quit if we can't read the first line */
    
    while (read_line(connFd, line, MAXBUF) > 0){
        strcat(buffer, line);
        if (strcmp(line, "\r\n") == 0){
            break;
        }
    } //

    Request *req = parse(buffer, MAXBUF, connFd);

    if (req == NULL){

        printf("LOG: Failling to parse a request");
        sprintf(headr, 
                "HTTP/1.1 400 Request Parsing Failed\r\n"
                "Server: icws\r\n"
                "Connection: close\r\n");
        return;

    }

    else if (strcasecmp(req->http_version, "HTTP/1.1") == 0) {

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
                    "Server: icws\r\n"
                    "Connection: close\r\n");
            write_all(connFd, headr, strlen(headr));
        }

    }

    else{

        printf("LOG: Unsupported HTTTP Version");
        sprintf(headr, 
                "HTTP/1.1 505 HTTP Version Not Supported\r\n"
                "Server: icws\r\n"
                "Connection: close\r\n");

        return;

    } 

    free(req->headers);
    free(req);

}

int main(int argc, char* argv[]) {

    // static struct option long_ops[] =
    // {
    //     {"port", required_argument, NULL, 'p'},
    //     {"root", required_argument, NULL, 'r'}, 
    //     {NULL, 0, NULL, 0}
    // };

    // int ch;
    // while ((ch = getopt_long(argc, argv, "p:r:", long_ops, NULL)) != -1){

    //     switch (ch)
    //     {

    //         case 'p':
    //             strcpy(port,optarg);
    //             break;
 
    //         case 'r':
    //             strcpy(root,optarg);
    //             break;
    //     }
  
    // }

    int listenFd = open_listenfd(argv[1]);

    for (;;) {
        struct sockaddr_storage clientAddr;
        socklen_t clientLen = sizeof(struct sockaddr_storage);

        int connFd = accept(listenFd, (SA *) &clientAddr, &clientLen);
        if (connFd < 0) { fprintf(stderr, "Failed to accept\n"); continue; }

        char hostBuf[MAXBUF], svcBuf[MAXBUF];
        if (getnameinfo((SA *) &clientAddr, clientLen, 
                        hostBuf, MAXBUF, svcBuf, MAXBUF, 0) == 0) 
            printf("Connection from %s:%s\n", hostBuf, svcBuf);
        else
            printf("Connection from ?UNKNOWN?\n");
                        
        serve_http(connFd, argv[2]);
        close(connFd);
    }

    return 0;
}
