#define _GNU_SOURCE
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/wait.h>
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

//define for CGI

static char *inferiorCmd = "./pipe-demo/chatty_cli.py";

//============================


char *port;
char *root;
char *numThreads;
char *timeout;
char *cgi;

char* acpt;
char* referer;
char* acpt_encoding;
char* acpt_lang;
char* acpt_chrset;
char* host;
char* cookie;
char* usr_agent;
char* cont;
char* cont_length;
unsigned char* remote_addr;

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

void getHeader(int fd, int connFd, char* req_obj, char* connection){ //get header

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
            "Connection: %s\r\n"
            "Content-length: %lu\r\n"
            "Content-type: %s\r\n"
            "Last-Modified: %s\r\n\r\n", date(), connection, filesize, mimeT(file), ctime(&st.st_mtime), mimeT(req_obj));

    write_all(connFd, headr, strlen(headr));

}

void respond_get(int connFd, char* rootFol, char* req_obj, char* connection) { //running get

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
    // if ()
    getHeader(fd, connFd, file, connection);

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

void respond_head(int connFd, char* rootFol, char* req_obj, char* connection){ //running head

    char* fileLocation = fileLoc(rootFol, req_obj);
    int fd = open(fileLocation, O_RDONLY);
    char* file = getFile(req_obj);

    struct stat st;
    fstat(fd, &st);
    char buf[st.st_size];

    getHeader(fd, connFd, file, connection);

    write_all(connFd, buf, strlen(buf));

    if ( (close(fd)) < 0 ){
        printf("Failed to close input file. Meh.\n");
    }   

}

int poll(struct pollfd *fds, nfds_t nfds, int timeout);

void fail_exit(char *msg) { fprintf(stderr, "%s\n", msg); exit(-1); }

int piper(int connFd, char* rootFol, char* buffer, char* connection) {

    int c2pFds[2]; /* Child to parent pipe */
    int p2cFds[2]; /* Parent to child pipe */

    if (pipe(c2pFds) < 0) fail_exit("c2p pipe failed.");
    if (pipe(p2cFds) < 0) fail_exit("p2c pipe failed.");

    int pid = fork();

    if (pid < 0) fail_exit("Fork failed.");
    if (pid == 0) { /* Child - set up the conduit & run inferior cmd */

        pthread_mutex_lock(&parse_mutex);
        Request *req = parse(buffer, MAXBUF, connFd);  
        pthread_mutex_unlock(&parse_mutex); 

        // for content length and type
        char* fileLocation = fileLoc(rootFol, req->http_uri);
        char* file = getFile(req->http_uri);
        char* path_info = strtok(req->http_uri, "?");
        char* query_str = req->http_uri;
        char* final_query_str;
        while(query_str != NULL){
            final_query_str = query_str;
            query_str = strtok(NULL, "?");
    
        }

        int fd = open(fileLocation, O_RDONLY);
        struct stat st;
        fstat(fd, &st);
        size_t filesize = st.st_size;
        int fSize = filesize;
        char* ext = mimeT(fileLocation);

        setenv("GATEWAY_INTERFACE", "CGI/1.1", 1);
        setenv("PATH_INFO", path_info, 1);
        setenv("QUERY_STRING", final_query_str, 1);
        // printf("remote_addr: %d %d %d %d\n", remote_addr[0], remote_addr[1], remote_addr[2], remote_addr[3]);
        setenv("REMOTE_ADDR", remote_addr, 1);
        // printf("request med: %s\n", req->http_method);
        setenv("REQUEST_METHOD", req->http_method, 1);
        // printf("request uri: %s\n", req->http_uri);
        setenv("REQUEST_URI", req->http_uri, 1);
        // printf("script name: %s\n", cgi);
        setenv("SCRIPT_NAME", cgi, 1);
        // printf("server port: %s\n", port);
        setenv("SERVER_PORT", port, 1);   //**please check the type needed**
        setenv("SERVER_PROTOCOL", "HTTP/1.1", 1);
        setenv("SERVER_SOFTWARE", "KHINGXC_SERVER", 1);
        setenv("HTTP_CONNECTION", connection, 1);
        // printf("HTTP connection: %s\n", connection, 1);
        if (strcasecmp(ext, "null") != 0){
            setenv("CONTENT_TYPE", ext, 1);
        }
        // printf("req header counts: %d\n", req->header_count);

        for(int i = 0; i < req->header_count; i++){

            if (strcmp(req->headers[i].header_name, "CONTENT_LENGTH") == 0){
                setenv("CONTENT_LENGTH", req->headers[i].header_value, 1);
            }

            if (strcmp(req->headers[i].header_name, "Accept") == 0){
                setenv("HTTP_ACCEPT", req->headers[i].header_value, 1);
            }

            if (strcmp(req->headers[i].header_name, "Referer") == 0){
                setenv("HTTP_REFERER", req->headers[i].header_value, 1);
            }

            if (strcmp(req->headers[i].header_name, "Accept-Encoding") == 0){
                setenv("HTTP_ACCEPT_ENCODING", req->headers[i].header_value, 1);
            }

            if (strcmp(req->headers[i].header_name, "Accept-Language") == 0){
                setenv("HTTP_ACCEPT_LANGUAGE", req->headers[i].header_value, 1);
            }

            if (strcmp(req->headers[i].header_name, "Accept-Charset") == 0){
                setenv("HTTP_ACCEPT_CHARSET", req->headers[i].header_value, 1);
            }

            if (strcmp(req->headers[i].header_name, "Host") == 0){
                setenv("HOST", req->headers[i].header_value, 1);
            }

            if (strcmp(req->headers[i].header_name, "Cookie") == 0){
                setenv("HTTP_COOKIE", req->headers[i].header_value, 1);
            }

            if (strcmp(req->headers[i].header_name, "User-Agent") == 0){
                setenv("HTTP_USER_AGENT", req->headers[i].header_value, 1);
            }
        }

        /* Wire pipe's incoming to child's stdin */
        /* First, close the unused direction. */
        if (close(p2cFds[1]) < 0) fail_exit("failed to close p2c[1]");
        if (p2cFds[0] != STDIN_FILENO) {
            if (dup2(p2cFds[0], STDIN_FILENO) < 0)
                fail_exit("dup2 stdin failed.");
            if (close(p2cFds[0]) < 0)
                fail_exit("close p2c[0] failed.");
        }

        /* Wire child's stdout to pipe's outgoing */
        /* But first, close the unused direction */
        if (close(c2pFds[0]) < 0) fail_exit("failed to close c2p[0]");
        if (c2pFds[1] != STDOUT_FILENO) {
            if (dup2(c2pFds[1], STDOUT_FILENO) < 0)
                fail_exit("dup2 stdin failed.");
            if (close(c2pFds[1]) < 0)
                fail_exit("close pipeFd[0] failed.");
        }

        
        char* inferiorArgv[] = {cgi, NULL};
        //fixing inferiorCmd
        if (execvpe(inferiorArgv[0], inferiorArgv, environ) < 0)
            fail_exit("exec failed.");
    }
    else { /* Parent - send a random message */
        /* Close the write direction in parent's incoming */
        if (close(c2pFds[1]) < 0) fail_exit("failed to close c2p[1]");

        /* Close the read direction in parent's outgoing */
        if (close(p2cFds[0]) < 0) fail_exit("failed to close p2c[0]");

        char *message = "OMGWTFBBQ\n";
        /* Write a message to the child - replace with write_all as necessary */
        write(p2cFds[1], message, strlen(message));
        /* Close this end, done writing. */
        if (close(p2cFds[1]) < 0) fail_exit("close p2c[01] failed.");

        char buf[MAXBUF+1];
        ssize_t numRead;
        /* Begin reading from the child */
        while ((numRead = read(c2pFds[0], buf, BUFSIZE))>0) {
            printf("Parent saw %ld bytes from child...\n", numRead);
            // printf("Parent saw %ld bytes from child...\n", numRead);
            // buf[numRead] = '\x0'; /* Printing hack; won't work with binary data */
            write_all(connFd, buf, strlen(buf));
            // printf("-------\n");
            // printf("%s", buf);
            // printf("-------\n");
        }
        /* Close this end, done reading. */
        if (close(c2pFds[0]) < 0) fail_exit("close c2p[01] failed.");

        /* Wait for child termination & reap */
        int status;

        if (waitpid(pid, &status, 0) < 0) fail_exit("waitpid failed.");
        printf("Child exited... parent's terminating as well.\n");
    }
}

int serve_http(int connFd, char* rootFol) {

    int persistantCheck = PERSISTENT;

    char buffer[MAXBUF];
    memset(buffer, '\0', 8192);
    char line[MAXBUF];
    char headr[MAXBUF];
    char lastFour[5];
    int readLine;
    char* connection;

    struct pollfd fds[1];
    int timeOut, pret;
    timeOut = atoi(timeout);

    fds[0].fd = connFd;
    fds[0].events = 0;
    fds[0].events |= POLLIN;

    pret = poll(fds, 1, timeOut);

    if (pret == 0){
        persistantCheck = CLOSE;
        connection = "close";
        sprintf(headr, 
                "HTTP/1.1 408 Request Timeout Error\r\n"
                "Date: %s \r\n"
                "Server: ICWS\r\n"
                "Connection: %s\r\n\r\n", date(), connection);

        write_all(connFd, headr, strlen(headr));
    }

    else{

    while ((readLine = read(connFd, line, 8192)) > 0){

        strcat(buffer, line);
        memset(lastFour, '\0', 5);
        strncpy(lastFour, &buffer[strlen(buffer)-4], 4);
        
        if ((strcmp(lastFour, "\r\n\r\n") == 0) || (strcmp(line, "\r\n") == 0)){     
            break;
        }

        memset(line, '\0', MAXBUF);

    }

    //parser is not thread safe, so we have to lock the process right here. 
    //=====================================================
    pthread_mutex_lock(&parse_mutex);
    Request *req = parse(buffer, MAXBUF, connFd);  
    pthread_mutex_unlock(&parse_mutex); 
    //=====================================================
    
    if (req == NULL){

        persistantCheck = CLOSE;
        connection = "close";
        printf("LOG: Failling to parse a request");
        sprintf(headr, 
                "HTTP/1.1 400 Request Parsing Failed\r\n"
                "Date: %s \r\n"
                "Server: ICWS\r\n"
                "Connection: %s\r\n\r\n", date(), connection);

        write_all(connFd, headr, strlen(headr));
        memset(buffer, '\0', MAXBUF);
        memset(headr, '\0', MAXBUF);
        return;

    }

    else if (strcasecmp(req->http_version, "HTTP/1.1") == 0) {

        // printf("method: %s\n", req->http_method);
        // printf("version: %s\n", req->http_version);
        // printf("uri: %s\n", req->http_uri);

        // printf("connFd: %d\n", connFd);
        // printf("rootFol: %s\n", rootFol);

        connection = "keep-alive";

        for(int i = 0; i < req->header_count; i++){

            if (strcasecmp(req->headers[i].header_name, "Connection") == 0){
                if (strcmp(req->headers[i].header_value, "keep-alive") != 0){
                    printf("**connection is closed**\n");
                    persistantCheck = CLOSE;
                    connection = "close";
                    break;
                }
            }

        }

        //==============================================
        char* toCheck = fileLoc(root, req->http_uri);

        // if (strcasecmp(starter, "/cgi/") == 0){
        if (strstr(toCheck, "/cgi/")){
            if ((strcasecmp(req->http_method, "GET") == 0)||(strcasecmp(req->http_method, "HEAD") == 0)||(strcasecmp(req->http_method, "POST") == 0)){
                piper(connFd, rootFol, buffer, connection);
                return;
            }
            
        }

        printf("start checking methods\n");

        if (strcasecmp(req->http_method, "GET") == 0) {

            printf("LOG: Running via GET method\n");
            respond_get(connFd, rootFol, req->http_uri, strdup(connection));

        }

        else if (strcasecmp(req->http_method, "HEAD") == 0) {

            printf("LOG: Running via HEAD method\n");
            respond_head(connFd, rootFol, req->http_uri, strdup(connection));

        }

        else {

            persistantCheck = CLOSE;
            connection = "close";
            printf("LOG: Unknown request\n");
            sprintf(headr, 
                    "HTTP/1.1 501 Method Unimplemented\r\n"
                    "Date: %s\r\n"
                    "Server: ICWS\r\n"
                    "Connection: %s\r\n", date(), connection);
            write_all(connFd, headr, strlen(headr));
        
        }

    }

    else{

        persistantCheck = CLOSE;
        connection = "close";
        printf("LOG: Unsupported HTTP Version\n");
        sprintf(headr, 
                "HTTP/1.1 505 HTTP Version Not Supported\r\n"
                "Server: ICWS\r\n"
                "Connection: %s\r\n", connection);
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

    signal(SIGPIPE, SIG_IGN);
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


        struct sockaddr_in *sin = (struct sockaddr_in *)&clientAddr;
        unsigned char *ip = (unsigned char *)&sin->sin_addr.s_addr;
        // printf("%d %d %d %d\n", ip[0], ip[1], ip[2], ip[3]);
        remote_addr = ip;

        pthread_mutex_lock(&queue_mutex); 
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

/*

=====================================================================================

Bugs:

* M3 doesn't work; getting exec failed, Child exited... parent's terminating as well.

=====================================================================================

References

* https://www.techiedelight.com/print-current-date-and-time-in-c/
* https://www.youtube.com/playlist?list=PL9IEJIKnBJjH_zM5LnovnoaKlXML5qh17
* https://www.youtube.com/watch?v=_n2hE2gyPxU
* https://code-vault.net/lesson/j62v2novkv:1609958966824 
* https://www.youtube.com/watch?v=UP6B324Qh5k&t=2s

=====================================================================================

Collaborators

* Maylin Catherine Cerf 6180039
* Thanthong Chim-Ong 6280026

=====================================================================================

*/
