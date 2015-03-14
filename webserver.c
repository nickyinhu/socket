#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <strings.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <arpa/inet.h>
#include <ctype.h>
#include <string.h>
#include <sys/socket.h>
#include <pthread.h>
#include <sys/stat.h>
#include <sys/sendfile.h>
#include <fcntl.h>
#include <errno.h>
/* CONSTANTS ============================================================ */
#define SOCKET_ERROR    -1
#define BUFFER_SIZE     1024

/* pthread var ========================================================== */
pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t bossCond = PTHREAD_COND_INITIALIZER;
pthread_cond_t workerCond = PTHREAD_COND_INITIALIZER;
/* Options =============================================================== */
char *filePath = ".";
int portNum = 8888;
int threadNum = 1;
/* Linked list constructor =============================================== */
struct linked_list {
    int val;
    struct linked_list *next;
};

struct linked_list *head = NULL;
struct linked_list *curr = NULL;

struct linked_list* create_list(int val) {
    //printf("\n Creating list with headnode as [%d]\n",val);
    struct linked_list *ptr = (struct linked_list*)malloc(sizeof(struct linked_list));
    if(NULL == ptr) {
        printf("\n Node creation failed \n");
        return NULL;
    }
    ptr->val = val;
    ptr->next = NULL;

    head = ptr;
    curr = ptr;
    return ptr;
}

struct linked_list* add_to_list(int val) {
    if(NULL == head) {
        return (create_list(val));
    }
    //printf("\n Adding list with headnode as [%d]\n",val);
    struct linked_list *ptr = (struct linked_list*)malloc(sizeof(struct linked_list));
    if(NULL == ptr) {
        printf("\n Node creation failed \n");
        return NULL;
    }
    ptr->val = val;
    ptr->next = NULL;
    
    curr->next = ptr;
    curr = ptr;

    return ptr;
}

int is_empty (void){
    if(NULL == head) {
        return 1;
    } else {
        return 0;   
    }
}

int delete_from_list(void) {
    struct linked_list *ptr = head;
    if(NULL == head) {
        return -1;
    }
    int result = head->val;
    ptr = head->next;
    head = ptr;
    return result;
}

/************************************
            Worker function
***********************************/
void print_usage() {
    printf("Usage: webserver [options]\n");
    printf("-p path to file (Default: .)\n");
    printf("-p server port (Default: 8888)\n");
    printf("-t number of worker threads (Default: 1, Range: 1-1000)\n");
    printf("-h show help message\n");
}

void *worker(void *threadarg){
    while(1){        
        /* get the file name from the client */
        struct stat stat_buf;      /* argument to fstat */
        off_t offset = 0;          /* file offset */
        char request[50];   /* filename to send */
        int received;                    /* holds return code of system calls */

        pthread_mutex_lock(&mtx);

            while(is_empty()) {
                pthread_cond_wait(&workerCond, &mtx);
            }

            int client_socket_fd = delete_from_list();

        pthread_mutex_unlock(&mtx);
        pthread_cond_signal(&bossCond);

        received = recv(client_socket_fd, request, sizeof(request), 0);
        if (received == -1) {
            fprintf(stderr, "ERROR: recv failed: %s\n", strerror(errno));
            exit(1);
        }
        printf("INFO: %s\n", request);

        char *temp_name = malloc(sizeof(temp_name));
        strtok(request, " ");
        strtok(NULL, " ");
        temp_name = strdup(strtok(NULL, " "));

        /* null terminate and strip any \r and \n from temp_name */
        if (temp_name[strlen(temp_name)-1] == '\n')
            temp_name[strlen(temp_name)-1] = '\0';
        if (temp_name[strlen(temp_name)-1] == '\r')
            temp_name[strlen(temp_name)-1] = '\0';

        char * filename = malloc(snprintf(NULL, 0, "%s%s", filePath, temp_name) + 1);
        sprintf(filename, "%s%s", filePath, temp_name);

        printf("INFO: received request to send file %s\n", filename);

        /* open the file to be sent */
        int fd = open(filename, O_RDONLY);
        char status[100];

        if (fd == -1) {
            snprintf(status, sizeof(status), "GetFile FILE_NOT_FOUND 0 0");
            write(client_socket_fd, status, sizeof(status)+1);
            fprintf(stderr, "ERROR: Cannot open requested file %s\n", filename);
            fprintf(stderr, "ERROR: Closing the socket\n");
        } else {
            /* get the size of the file to be sent */
            fstat(fd, &stat_buf); 

            sprintf(status, "GetFile OK %d",(int)stat_buf.st_size);
            write(client_socket_fd, status, sizeof(status)+1);

            int remain_data = (int)stat_buf.st_size;
            int sent_bytes = 0;
            int total = 0;
            while (((sent_bytes = sendfile(client_socket_fd, fd, &offset, BUFFER_SIZE)) > 0) && (remain_data > 0)) {
                remain_data -= sent_bytes;
                total += sent_bytes;
                //fprintf(stdout, "Server has sent %d bytes from file's data, remaining data = %d\n", total, remain_data);
            }
            /* close socket */
            printf("INFO: Sent %d bytes, Closing the socket\n", total);
        }
        close(fd);
        if(close(client_socket_fd) == SOCKET_ERROR)  {
            printf("INFO: Could not close socket\n");
            exit(1);
        }
    }          
}

int main(int argc, char *argv[]) {
    printf("===== Hello, I am the web server =====\n");
    int option = 0;
    while ((option = getopt(argc, argv,"hp:t:f:")) != -1) {
        switch (option) {
            case 'p': portNum = atoi(optarg);
                printf ("ARG: Port number is '%s'\n", optarg);
                break;
            case 't' : threadNum = atoi(optarg);            
                threadNum = threadNum > 100 ? 100 : threadNum;
                threadNum = threadNum < 1 ? 1 : threadNum;
                printf ("ARG: Thread number is '%d'\n", threadNum);
                break;
            case 'f' : filePath = optarg;
                printf ("ARG: File path is '%s'\n", optarg);
                break;
            case 'h': print_usage();
                exit (0);
            default: print_usage();
        }
    }

    /* Thread initiation */
    pthread_t threads[threadNum]; 
    int i;   
    for(i = 0; i < threadNum; i++) {
        pthread_create(&threads[i],NULL,worker,NULL);
    }

    /* Server initiation */
    int socket_fd = 0;
    struct sockaddr_in server;
    struct sockaddr_in client;
    int client_addr_len = sizeof(client); 

    int client_socket_fd;

    printf("INFO: Starting server\n");
    printf("INFO: Making socket\n");

    /* make a server socket */
    ;

    if( (socket_fd=socket(AF_INET,SOCK_STREAM,0)) == SOCKET_ERROR ) {
        fprintf(stderr, "ERROR: Could not make a socket\n");
        exit(1);
    }

    server.sin_addr.s_addr=htonl(INADDR_ANY);
    server.sin_port=htons(portNum);
    server.sin_family=AF_INET;

    /* Binding to port */
    printf("INFO: Binding to port %d\n",portNum);
    if(bind(socket_fd,(struct sockaddr*)&server,sizeof(server)) == SOCKET_ERROR) {
        fprintf(stderr, "ERROR: Could not connect to host\n");
        exit(1);
    }

    /* Listening to port */
    if ( listen(socket_fd, threadNum) == SOCKET_ERROR ) {
        fprintf(stderr, "ERROR: Server could not listen to port %d\n", portNum);
        exit(1);
    } else {
        printf("INFO: server listening for a connection on port %d\n", portNum);
    }

    while (1){
        pthread_mutex_lock(&mtx);
            printf("INFO: Waiting for a connection\n");
            while(is_empty() == 0) {
                pthread_cond_wait (&bossCond, &mtx);
            }
            if( (client_socket_fd=accept(socket_fd,(struct sockaddr*)&client,(socklen_t *)&client_addr_len)) == SOCKET_ERROR ){
                fprintf(stderr, "ERROR: Connection cannot be accepted\n");
            } else {
                printf("INFO: Got a connection from %s on port %d\n", inet_ntoa(client.sin_addr), htons(client.sin_port));
            }
            add_to_list(client_socket_fd);
        pthread_mutex_unlock(&mtx);
        pthread_cond_broadcast(&workerCond);
    }
}