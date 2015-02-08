#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <strings.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/socket.h>
#include <pthread.h>
/* CONSTANTS ============================================================ */
#define BUFFER_SIZE     1024
#define SOCKET_ERROR    -1
#define FILENAME        "/1mb-sample-file-1.txt"

/* pthread var ========================================================== */
pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t bossCond = PTHREAD_COND_INITIALIZER;
pthread_cond_t workerCond = PTHREAD_COND_INITIALIZER;
/* Serverinfo ============================================================ */
struct hostent *he;
/* Options =============================================================== */
const char *sHostAddress = "0.0.0.0";
int request = 10;
int portNum = 8888, threadNum = 1;
char *workLoad = "workload.txt";
int writer_check = 0;
int running_thread;
/* Worker args =========================================================== */
struct arg_struct {
    int each;
    int tid;
};

/* Worker args =========================================================== */
int debug_lvl;
/* Queue constructor ====================================================== */
struct linked_list {
    char *val;
    struct linked_list *next;
};

struct linked_list *head = NULL;
struct linked_list *curr = NULL;

struct linked_list* create_list(char *input) {
    //printf("\n Creating list with headnode as [%d]\n",val);
    struct linked_list *ptr = malloc(sizeof(struct linked_list)+1);
    if(NULL == ptr) {
        printf("\n Node creation failed \n");
        return NULL;
    }
    char *val = malloc(strlen(input)+1);
    strcpy(val, input);
    ptr->val = val;
    ptr->next = NULL;

    head = curr = ptr;
    return ptr;
}

struct linked_list* add_to_list(char *input) {
    if(NULL == head) {
        return (create_list(input));
    }    
    struct linked_list *ptr = malloc(sizeof(struct linked_list)+1);
    if(NULL == ptr) {
        printf("\n Node creation failed \n");
        return NULL;
    }    
    char *val = malloc(strlen(input)+1);
    strcpy(val, input);
    ptr->val = val;
    ptr->next = NULL;
    
    curr->next = ptr;
    curr = ptr;

    return ptr;
}

int is_empty (void){
    return (NULL == head)? 1 : 0;
}

char* delete_from_list(void) {
    struct linked_list *ptr = head;
    if(NULL == head)
        return NULL;
    char *val = head->val;
    ptr = head->next;
    head = ptr;
    return val;
}
/* Usage ================================================================== */
void print_usage() {
    printf("Usage: webclient [options]\n");
    printf("-s server address (Default: 0.0.0.0)\n");
    printf("-p server port (Default: 8888)\n");
    printf("-t number of worker threads (Default: 1, Range: 1-100)\n");
    printf("-w path to workload file (Default: workload.txt)\n");
    printf("-d path to downloaded file directory (Default: null)\n");
    printf("-r number of total requests (Default: 10, Range: 1-1000)\n");
    printf("-m path to metrics file (Default: metrics.txt)\n");
    printf("-h show help message\n");
}

void *worker (void *threadarg) {
    struct arg_struct *args = threadarg;
    int each = args->each;
    int tid = args->tid;
    int i;
    int work_done = 0;
    if(debug_lvl == 1) { printf("DEBUG: Thread %d started\n", tid); }
    for(i = 0; i < each; i++) {
        char buffer[BUFFER_SIZE];
        if(debug_lvl == 1) { printf("DEBUG: Thread %d locking\n", tid); }
        pthread_mutex_lock(&mtx);
            // writer_check = 0;
            if(debug_lvl == 1) printf("DEBUG: Thread %d locked, request: %d, list is empty? : %d\n", tid, request, is_empty());
            while( is_empty() == 1 ){                
                if(request == 0){
                    running_thread--;
                    pthread_mutex_unlock(&mtx);
                    pthread_cond_signal(&bossCond);
                    if(debug_lvl == 1) { printf("DEBUG: Thread %d closed, finished %d requests\n", tid, work_done); }
                    return;
                }
                if(debug_lvl == 1) { printf("DEBUG: Thread %d waiting, current load: %d\n", tid, each); }
                pthread_cond_wait (&workerCond, &mtx);
            }
            if(debug_lvl == 1) printf("DEBUG: Thread %d working\n", tid);
            request--;
            work_done++;
            char *filename = malloc(50);
            char *read_name = delete_from_list();
            strcpy (filename, read_name);
            if (filename[strlen(filename)-1] == '\n')
                filename[strlen(filename)-1] = '\0';
            if(debug_lvl == 1) { printf("DEBUG: Thread got %s!\n", filename); }

            struct sockaddr_in server_socket_addr;

            if(debug_lvl == 1) { printf("Host name : %s\n", he->h_name); }

            unsigned long server_addr_nbo = *(unsigned long *)(he->h_addr_list[0]);

            int socket_fd;
            if ( (socket_fd = socket(AF_INET, SOCK_STREAM, 0)) == SOCKET_ERROR ) {
                fprintf(stderr, "client failed to create socket\n");
                exit(1);
            } 

            if(debug_lvl == 1) {  printf("DEBUG: client created socket\n"); }


            bzero(&server_socket_addr, sizeof(server_socket_addr));
            server_socket_addr.sin_family = AF_INET;
            server_socket_addr.sin_port = htons(portNum);
            server_socket_addr.sin_addr.s_addr = server_addr_nbo;
            if ( connect(socket_fd, (struct sockaddr *)&server_socket_addr, sizeof(server_socket_addr)) == SOCKET_ERROR) {
                fprintf(stderr, "client failed to connect to server!\n");
                close(socket_fd);
                exit(1);
            } 

            if(debug_lvl == 1) { fprintf(stdout, "INFO: client connected to to server!\n"); }

            char fullname[50];
            sprintf(fullname, "%s%s", "GetFile Get ", filename);

            if(debug_lvl == 1) { printf("Request is %s\n", fullname); }

            if (send(socket_fd, fullname, sizeof(fullname), 0) == SOCKET_ERROR) {
                fprintf(stderr, "client failed to send %s\n", filename);
                close(socket_fd);
                exit(1);
            } 

            if(debug_lvl == 1) {  puts("DEBUG: Sent requests"); }

            char response[100];
            if(read(socket_fd, response, sizeof(response)+1) == SOCKET_ERROR){
                fprintf(stderr, "client failed to read message\n");
                exit(1);
            } 

            if(debug_lvl == 1) { puts("DEBUG: received response"); }
            
            strtok(response, " ");
            char *status = strdup(strtok(NULL, " "));
            char *size = strdup(strtok(NULL, " "));
            int file_size = atoi(size);

            /* GetFile response */
            if(debug_lvl == 1) { printf("status:%s, bytes: %s\n", status, size); }

            
        pthread_mutex_unlock(&mtx);
        if(debug_lvl == 1) { puts("DEBUG: Thread unlocked"); }
        pthread_cond_signal(&bossCond);


        if(strcmp(status, "OK") != 0){
            fprintf(stderr, "Received bad response!!!\n");
            continue;
        }

        int remain_data = file_size;
        int len;

        int total = 0;
        char open_name[50];
        sprintf(open_name, "%s%s", "/home/adminuser/temp", filename);
        //FILE *received_file;
        //received_file = fopen(open_name, "w");
        // if (received_file == NULL) {
        //     fprintf(stderr, "Failed to open file --> %s\n", strerror(errno));
        //     exit(EXIT_FAILURE);
        // }
        while (((len = recv(socket_fd, buffer, BUFFER_SIZE, 0)) > 0) && (remain_data > 0)) {
            total += len;
            //fwrite(buffer, sizeof(char), len, received_file);
            remain_data -= len;
        }
        if(debug_lvl == 1) { fprintf(stdout, "DEBUG: Thread %d received file %s in %d bytes\n", tid, filename, total); }
        //fclose(received_file);
        /* Close the socket and return the response length (in bytes) */
        close(socket_fd);        
    }
    if(debug_lvl == 0) { printf("DEBUG: Thread %d closed, finished %d requests\n", tid, work_done); }
    running_thread--;
    return;
}


int random_num(int limit) {
    int divisor = RAND_MAX/(limit+1);
    int result;
    do { 
        result = rand() / divisor;
    } while (result > limit);

    return result;
}


int main(int argc, char **argv) {

    printf("\nHello, I am the web client\n");
    int option = 0;
    while ((option = getopt(argc, argv,"hp:t:s:r:d:")) != -1) {
        switch (option) {
            case 'p': portNum = atoi(optarg);
                printf ("ARG: Port number is '%s'\n", optarg);
                break;
            case 't' : threadNum = atoi(optarg);
                printf ("ARG: Thread number is '%s'\n", optarg);
                break;
            case 's' : sHostAddress = optarg;
                printf ("ARG: Server address is '%s'\n", optarg);
                break;
            case 'r' : request = atoi(optarg);
                printf ("ARG: Request number is '%s'\n", optarg);
                break;
            case 'd' : debug_lvl = atoi(optarg);
                printf ("ARG: Debug level is '%s'\n", optarg);
                break;
            case 'h': print_usage();
                exit (0);
            default: print_usage();
        }
    }    

    he = gethostbyname(sHostAddress);

    /* Thread initiation */
    int each = (threadNum + request -1) / threadNum;

    pthread_t threads[threadNum]; 
    int i;
    for(i = 0; i < threadNum; i++) {
        struct arg_struct args;
        int tid = i;
        args.each = each;
        args.tid = tid;
        pthread_create(&threads[i],NULL,worker,(void *)&args);
        running_thread++;
    }
    if(debug_lvl == 1) printf("DEBUG: each thread has %d jobs\n", each);
    //usleep(1000);

    FILE *fr;
    char line[256];
    fr = fopen ("./workload.txt", "rt");
    if (fr == NULL) {
        fprintf(stderr, "Failed to open file --> %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    char *files[100];
    int file_number = 0;
    while( fgets(line, sizeof(line), fr ) ){
        if (line[strlen(line)-1] == '\n')
            line[strlen(line)-1] = '\0';
        files[file_number] = malloc(strlen(line)+1);
        strcpy(files[file_number], line);
        file_number++;
    }

    int index;
    for (index = 0; index < request; index++){
        int random = random_num(file_number - 1);
        add_to_list(files[random]);
    }
    if(debug_lvl == 1)
        puts("DEBUG: STARTING!!!");

    while( running_thread > 0 || request > 0 ){
        if(debug_lvl == 1) { printf("DEBUG: Boss locking, request: %d, threads: %d\n", request, running_thread); }
        pthread_mutex_lock(&mtx);
            if(debug_lvl > 1) { puts("DEBUG: Boss unlocking and broadcasting"); }
        pthread_mutex_unlock(&mtx);
        pthread_cond_broadcast(&workerCond);
        if(debug_lvl > 1){ puts("DEBUG: Boss unlocked"); }
    }

    for(i = 0; i < threadNum; i++) { /* wait/join threads */
        if(debug_lvl == 1) printf("Thread %d joined\n", i);
        pthread_join(threads[i], NULL);
    }
    exit(0);
}

