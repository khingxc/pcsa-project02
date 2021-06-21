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

//define items
//============================
#define MAXBUF 8192
#define THREAD_POOL_SIZE 100
#define PERSISTENT 1
#define CLOSE 0
//============================

//argument variables
//==================
char *port;
char *root;
char *numThreads;
char *timeout;
//==================


//thread part
//============================================
int num_thread;

typedef struct sockaddr_in SA_IN;
typedef struct sockaddr SA;

pthread_t thread_pool[THREAD_POOL_SIZE];
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t parse_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond_var = PTHREAD_COND_INITIALIZER;
//============================================


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
    int fd = open(fileLocation, O_RDONLY);
    char* file = getFile(req_obj);
    getHeader(fd, connFd, file);

    if ( (close(fd)) < 0 ){
        printf("Failed to close input file. Meh.\n");
    }   

}

void serve_http(int connFd, char* rootFol) {

    printf("**calling serve_http**\n");

    char buffer[MAXBUF];
    memset(buffer, '\0', 8192);
    char line[MAXBUF];
    char headr[MAXBUF];
    int readLine;
    char TwopastLine[MAXBUF];
    
    while ((readLine = read(connFd, line, 8192)) > 0){

        strcat(buffer, line);
        char lastTwo[2];
        strncpy(lastTwo, &line[strlen(line)-2], 2);
        
        if ((strcmp(line, "\r\n") == 0) && (strcmp(TwopastLine, "\r\n") == 0)){       
            break;
        }

        memset(TwopastLine, '\0', MAXBUF);
        strcpy(TwopastLine, lastTwo);
        memset(line, '\0', MAXBUF);

    }

    pthread_mutex_lock(&parse_mutex);

    //parser is not thread safe, so we have to lock the process right here. 
    Request *req = parse(buffer, MAXBUF, connFd);  
    printf("After calling parse\n");

    pthread_mutex_unlock(&parse_mutex); 
    
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
        memset(TwopastLine, '\0', MAXBUF);
        return;

    }

    else if (strcasecmp(req->http_version, "HTTP/1.1") == 0) {

        printf("method: %s\n", req->http_method);
        printf("version: %s\n", req->http_version);
        printf("uri: %s\n", req->http_uri);

        printf("connFd: %d\n", connFd);
        printf("rootFol: %s\n", rootFol);

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

void* conn_handler(void *args) {

    printf("*calling conn_handler*\n");
    printf("agrs: %x\n", args);

    struct survival_bag *context = (struct survival_bag *) args;
    // int connection = PERSISTENT;
    
    // while(connection == PERSISTENT){
    //     connection = serve_http(context->connFd, dirName);
    // }   

    // pthread_detach(pthread_self());
    serve_http(context->connFd, root);

    close(context->connFd);
    
    free(context); /* Done, get rid of our survival bag */

    return NULL; /* Nothing meaningful to return */

}

//NODE Part
// =======================================================

struct node{
    struct node* next;
    struct survival_bag *context;
};
typedef struct node node_t;

node_t* head = NULL;
node_t* tail = NULL;

void enqueue(struct survival_bag *context) { 
   node_t *newNode = malloc(sizeof(node_t));
   newNode->context = context;
   newNode->next = NULL;

   if (tail == NULL){
       head = newNode;
   } 

   else{
       tail->next = newNode;
   }
   tail = newNode;
}

struct survival_bag* dequeue() {

    if (head == NULL){
        return NULL;
    }

    else{

        struct survival_bag *result = head->context;
        // node_t *tmp = head;
        if (head == NULL){
            tail = NULL;
        }
        // free(tmp);
        return result;

    }

}

// =======================================================

void* runThread(void *args){

    for (;;) {
        

        int *pclient = dequeue();
        int toCheck = 0;

        pthread_mutex_lock(&mutex);

        //**********************************

        if ((pclient) == NULL){
            pthread_cond_wait(&cond_var, &mutex);
            //****************************
            //how to make sure that it gets what it wants????

            //why need this kinda flag?
            toCheck = 1;
        }

        pthread_mutex_unlock(&mutex);

        if (toCheck == 0) {
            conn_handler(pclient);
        }

    }

} 
 
int poll(struct pollfd *fds, nfds_t nfds, int timeout);
 
int main(int argc, char* argv[]) {

    pthread_mutex_init(&mutex, NULL);
    pthread_mutex_init(&parse_mutex, NULL);
    pthread_cond_init(&cond_var, NULL);

    static struct option long_ops[] =
    {
        {"port", required_argument, NULL, 'p'},
        {"root", required_argument, NULL, 'r'}, 
        {"numThreads", required_argument, NULL, 'n'}, 
        {"timeout", required_argument, NULL, 't'}, 
        {NULL, 0, NULL, 0}
    };

    int ch;
    while ((ch = getopt_long(argc, argv, "p:r:n:t:", long_ops, NULL)) != -1){

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

        }
  
    }

    int listenFd = open_listenfd(port);
    num_thread = atoi(numThreads);

    //Thread Creation
    //=======================================================================
    for (int i = 0; i < num_thread; i++){

        if (pthread_create(&thread_pool[i], NULL, runThread, NULL) != 0){
            printf("Creating Thread failed\n");
        }

    }
    //=======================================================================

    for (;;) {

        struct sockaddr_storage clientAddr;
        socklen_t clientLen = sizeof(struct sockaddr_storage);
        pthread_t threadInfo;

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

        // int *pclient = malloc(sizeof(int));
        // *pclient = client_socket;

        pthread_mutex_lock(&mutex);
        enqueue(context);
        pthread_cond_signal(&cond_var);
        pthread_mutex_unlock(&mutex);

        // pthread_create(&threadInfo, NULL, conn_handler, (void *) context);

        // serve_http(connFd, root);
        // close(connFd);

    }

    return 0;

}


/*
Progress Checking Point

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
* https://www.youtube.com/playlist?list=PL9IEJIKnBJjH_zM5LnovnoaKlXML5qh17


*/