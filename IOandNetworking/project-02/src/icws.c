#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <getopt.h>
#include <poll.h>
#include <pthread.h>
#include "parse.h"
#include "pcsa_net.h"

//variables & threads set up

//============================

#define MAXBUF 8192
#define THREAD_POOL_SIZE 100
#define PERSISTENT 1
#define CLOSE 0

//============================

char *port;
char *root;
char *numThreads;
char *timeout;
char *cgi;

//================

int num_thread;

typedef struct sockaddr_in SA_IN;
typedef struct sockaddr SA;

pthread_t thread_pool[THREAD_POOL_SIZE];                   

pthread_mutex_t queue_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t parse_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond_var = PTHREAD_COND_INITIALIZER;

//===========================================================

struct survival_bag {

    struct sockaddr_storage clientAddr;
    int connFd;

};

int req_count = 0;
struct survival_bag req_queue[300];

//===========================================================

char* date(){       //get date function

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

    return strdup(loc);
    
}

char* getFile(char* req_obj){   //get the real file (incase there is a / comes);

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

    printf("before calling get header\n");
    printf("fd: %d\n", fd);
    printf("connFd: %d\n", connFd);
    printf("file: %s\n", file);
    getHeader(fd, connFd, file);

    char buf[st.st_size];

    write_all(connFd, buf, strlen(buf));

    ssize_t numRead;
    while ((numRead = read(fd, buf, MAXBUF)) > 0) {
        write_all(connFd, buf, numRead);
    }

    if ((close(fd)) < 0){
        printf("Failed to close input file. Meh.\n");
    }    

}

void respond_head(int connFd, char* rootFol, char* req_obj){ //running head

    char* fileLocation = fileLoc(rootFol, req_obj);
    int fd = open(fileLocation, O_RDONLY);
    char* file = getFile(req_obj);

    struct stat st;
    fstat(fd, &st);
    char buf[st.st_size];

    getHeader(fd, connFd, file);

    write_all(connFd, buf, strlen(buf));

    if ( (close(fd)) < 0 ){
        printf("Failed to close input file. Meh.\n");
    }   

}

// void respond_post(int connFd, char* rootFol, char* req_obj){

// /*
//    The POST method is used to request the script perform processing and
//    produce a document based on the data in the request message-body, in
//    addition to meta-variable values.  A common use is form submission in
//    HTML [18], intended to initiate processing by the script that has a
//    permanent affect, such a change in a database.

//    The script MUST check the value of the CONTENT_LENGTH variable before
//    reading the attached message-body, and SHOULD check the CONTENT_TYPE
//    value before processing it.
// */


// }

int poll(struct pollfd *fds, nfds_t nfds, int timeout);

int serve_http(int connFd, char* rootFol) {

    int persistantCheck = PERSISTENT;

    printf("**calling serve_http**\n");

    char buffer[MAXBUF];
    memset(buffer, '\0', 8192);
    char line[MAXBUF];
    char headr[MAXBUF];
    char lastFour[5];
    int readLine;

    struct pollfd fds[1];
    int timeOut, pret;
    timeOut = atoi(timeout);

    fds[0].fd = connFd;
    fds[0].events = 0;
    fds[0].events |= POLLIN;

    pret = poll(fds, 1, timeOut);

    if (pret == 0){
        sprintf(headr, 
                "HTTP/1.1 408 Request Timeout Error\r\n"
                "Date: %s \r\n"
                "Server: ICWS\r\n"
                "Connection: close\r\n\r\n", date());

        write_all(connFd, headr, strlen(headr));
        persistantCheck = CLOSE;
    }

    else{

    printf("before read while loop\n");
    while ((readLine = read(connFd, line, 8192)) > 0){

        printf("in while loop\n");

        strcat(buffer, line);
        memset(lastFour, '\0', 5);
        strncpy(lastFour, &buffer[strlen(buffer)-4], 4);

        printf("line: %s\n", line);
        
        if ((strcmp(lastFour, "\r\n\r\n") == 0) || (strcmp(line, "\r\n") == 0)){     
            break;
        }

        memset(line, '\0', MAXBUF);

    }

    printf("done reading, before mutex lock and parsing\n");

    //parser is not thread safe, so we have to lock the process right here. 
    //=====================================================
    pthread_mutex_lock(&parse_mutex);
    Request *req = parse(buffer, MAXBUF, connFd);  
    pthread_mutex_unlock(&parse_mutex); 
    //=====================================================
    
    if (req == NULL){

        printf("LOG: Failling to parse a request");
        sprintf(headr, 
                "HTTP/1.1 400 Request Parsing Failed\r\n"
                "Date: %s \r\n"
                "Server: ICWS\r\n"
                "Connection: close\r\n\r\n", date());

        write_all(connFd, headr, strlen(headr));
        memset(buffer, '\0', MAXBUF);
        memset(headr, '\0', MAXBUF);
        return;

    }

    // else if ((strcasecmp(req->http_version, "HTTP/1.1") == 0) || (strcasecmp(req->http_version, "HTTP/1.0") == 0)) {
    else if (strcasecmp(req->http_version, "HTTP/1.1") == 0) {

        printf("method: %s\n", req->http_method);
        printf("version: %s\n", req->http_version);
        printf("uri: %s\n", req->http_uri);

        printf("connFd: %d\n", connFd);
        printf("rootFol: %s\n", rootFol);

        for(int i = 0; i < req->header_count; i++){

            if (strcmp(req->headers[i].header_name, "Connection") == 0){
                if (strcmp(req->headers[i].header_value, "keep-alive") != 0){
                    persistantCheck = CLOSE;
                    break;
                }
            } 

        }

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
                    "Server: ICWS\r\n"
                    "Connection: close\r\n", date());
            write_all(connFd, headr, strlen(headr));
        
        }

    }

    else{

        printf("LOG: Unsupported HTTP Version\n");
        sprintf(headr, 
                "HTTP/1.1 505 HTTP Version Not Supported\r\n"
                "Server: ICWS\r\n"
                "Connection: close\r\n");
        write_all(connFd, headr, strlen(headr));
        free(req->headers);
        free(req);
        memset(buffer, '\0', MAXBUF);
        memset(headr, '\0', MAXBUF);
        return;

    } 

    free(req->headers);
    free(req);
    memset(buffer, '\0', MAXBUF);
    memset(headr, '\0', MAXBUF);

    }

    return persistantCheck;

}

void* conn_handler(struct survival_bag* req) {

    printf("*calling conn_handler*\n");

    while(serve_http(req->connFd, root) == PERSISTENT){
        
    }  

    close(req->connFd);
    return NULL; 

}

void* runThread(void *args){

    for (;;) {

        struct survival_bag req;
        pthread_mutex_lock(&queue_mutex);
        while (req_count == 0){
            pthread_cond_wait(&cond_var, &queue_mutex);
        }
        req = req_queue[0];
        for (int i = 0; i < req_count - 1; i++){
            req_queue[i] = req_queue[i+1];
        }
        req_count--;
        pthread_mutex_unlock(&queue_mutex);
        conn_handler(&req);

    }

} 
 
int main(int argc, char* argv[]) {

    pthread_mutex_init(&queue_mutex, NULL);
    pthread_mutex_init(&parse_mutex, NULL);
    pthread_cond_init(&cond_var, NULL);

    //=======================================================================================
    //get values of each input; port, root, numThreads, timeout

    static struct option long_ops[] =
    {
        {"port", required_argument, NULL, 'p'},
        {"root", required_argument, NULL, 'r'}, 
        {"numThreads", required_argument, NULL, 'n'}, 
        {"timeout", required_argument, NULL, 't'}, 
        {"cgiHandler", required_argument, NULL, 'c'},
        {NULL, 0, NULL, 0}
    };

    int ch;
    while ((ch = getopt_long(argc, argv, "p:r:n:t:c:", long_ops, NULL)) != -1){

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

            case 'n':
                printf("numThreads: %s\n", optarg);
                numThreads = optarg;
                break;

            case 't':
                printf("timeout: %s\n", optarg);
                timeout = optarg;
                break;

            case 'c':
                printf("cgi program: %s\n", optarg);
                cgi = optarg;
                break;
        }
  
    }

    //=======================================================================

    int listenFd = open_listenfd(port);
    num_thread = atoi(numThreads);

    //=======================================================================

    //Thread Creation
    for (int i = 0; i < num_thread; i++){

        printf("thread %d created\n", i);

        if (pthread_create(&thread_pool[i], NULL, runThread, NULL) != 0){
            printf("Creating Thread failed\n");
        }

    }

    //=======================================================================

    for (;;) {

        struct sockaddr_storage clientAddr;
        socklen_t clientLen = sizeof(struct sockaddr_storage);

        int connFd = accept(listenFd, (SA *) &clientAddr, &clientLen);

        if (connFd < 0) { 

            fprintf(stderr, "Failed to accept\n"); 
            continue; 
            
        }

        struct survival_bag *context = 
                    (struct survival_bag *) malloc(sizeof(struct survival_bag));
        context->connFd = connFd;
        
        memcpy(&context->clientAddr, &clientAddr, sizeof(struct sockaddr_storage));

        char hostBuf[MAXBUF], svcBuf[MAXBUF];
        if (getnameinfo((SA *) &clientAddr, clientLen, 
                        hostBuf, MAXBUF, svcBuf, MAXBUF, 0) == 0) 
            printf("Connection from %s:%s\n", hostBuf, svcBuf);
        else
            printf("Connection from ?UNKNOWN?\n");


        pthread_mutex_lock(&queue_mutex);
        printf("lock and add request to queue\n");
        req_queue[req_count] = *context;
        req_count++;
        pthread_mutex_unlock(&queue_mutex);
        pthread_cond_signal(&cond_var);

    }

    for (int i = 0; i < num_thread; i++) {

        if (pthread_join(thread_pool[i], NULL) != 0) {
            perror("Failed to join the thread");
        }

    }

    pthread_mutex_destroy(&queue_mutex);
    pthread_mutex_destroy(&parse_mutex);
    pthread_cond_destroy(&cond_var);

    return 0;

}

//=====================================================================================

/*
Progress Checking Point

MILESTONE 2

- persistant; check close in request

MILESTONE 3

- CGI Handler: Get Long Opt (get input part)

*/

//=====================================================================================

/*
Bugs:

* it only runs one request and it didn't run others after then. 

//calling exit() in sigInt handler is a SIN ; MUST NOT DO; better put things into a queue, change flag

*/

//=====================================================================================

/*
References

* https://www.techiedelight.com/print-current-date-and-time-in-c/
* https://www.youtube.com/playlist?list=PL9IEJIKnBJjH_zM5LnovnoaKlXML5qh17
* https://www.youtube.com/watch?v=_n2hE2gyPxU
* https://code-vault.net/lesson/j62v2novkv:1609958966824 
* https://www.youtube.com/watch?v=UP6B324Qh5k&t=2s


*/

//=====================================================================================
