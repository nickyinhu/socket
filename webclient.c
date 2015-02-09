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
#include <time.h>
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
char *download_path = NULL;
char *metrics_path = "metrics.txt";

/* Metrics =============================================================== */
long total_bytes = 0;
long total_time = 0;

/* Worker args =========================================================== */
typedef struct thread_data {
    int each;
    int tid;
} arg_struct;

int debug_lvl = 0;
/* Linked list constructor =============================================== */
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
    if(NULL == head)
        return NULL;
    struct linked_list *ptr = head;
    char *val = head->val;
    ptr = head->next;
    head = ptr;
    return val;
}

int get_number() {
    int list_num = 0;
    if (NULL == head)
        return list_num;
    struct linked_list *ptr = head;
    while(ptr != NULL){
        list_num++;
        ptr = ptr->next;
    }
    return list_num;
}

/* Worker =========================================================== */
void *worker (void *threadarg) {
    int each, tid;
    arg_struct *args;
    args = (arg_struct *) threadarg;
    each = args->each;
    tid = args->tid;
    int i;
    int work_done = 0;
    if(debug_lvl == 1) { printf("DEBUG: Thread %d started, with %d jobs, %d jobs in the Q\n", tid, each, get_number()); }
    for(i = 0; i < each; i++) {
        char buffer[BUFFER_SIZE];
        if(debug_lvl == 1) { printf("DEBUG: Thread %d locking, %d jobs in market, %d jobs in the Q\n", tid, request, get_number()); }
        pthread_mutex_lock(&mtx);
            // writer_check = 0;
            if(debug_lvl == 1) printf("DEBUG: Thread %d locked, request: %d, jobs in the Q: %d\n", tid, request, get_number());
            while( is_empty() == 1 ){                
                if(request == 0){
                    running_thread--;
                    pthread_mutex_unlock(&mtx);
                    pthread_cond_signal(&bossCond);
                    if(debug_lvl == 1) { printf("DEBUG: Thread %d closed, finished %d requests, , %d jobs not requested\n", tid, work_done, (each - work_done)); }
                    return;
                }
                if(debug_lvl == 1) { printf("DEBUG: Thread %d waiting, current load: %d\n", tid, (each-i)); }
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
            if(debug_lvl == 1) { printf("DEBUG: Thread %d got %s!\n", tid, filename); }

            struct sockaddr_in server_socket_addr;

            if(debug_lvl == 1) { printf("Host name : %s\n", he->h_name); }

            unsigned long server_addr_nbo = *(unsigned long *)(he->h_addr_list[0]);

            int socket_fd;
            if ( (socket_fd = socket(AF_INET, SOCK_STREAM, 0)) == SOCKET_ERROR ) {
                fprintf(stderr, "ERROR: client failed to create socket\n");
                exit(1);
            } 

            if(debug_lvl == 1) {  printf("DEBUG: client created socket\n"); }


            bzero(&server_socket_addr, sizeof(server_socket_addr));
            server_socket_addr.sin_family = AF_INET;
            server_socket_addr.sin_port = htons(portNum);
            server_socket_addr.sin_addr.s_addr = server_addr_nbo;

            struct timeval start, end;

            long mtime, seconds, useconds;    

            gettimeofday(&start, NULL);

            if ( connect(socket_fd, (struct sockaddr *)&server_socket_addr, sizeof(server_socket_addr)) == SOCKET_ERROR) {
                fprintf(stderr, "ERROR: client failed to connect to server!\n");
                close(socket_fd);
                exit(1);
            } 

            if(debug_lvl == 1) { fprintf(stdout, "INFO: client connected to to server!\n"); }

            char fullname[50];
            sprintf(fullname, "%s%s", "GetFile Get ", filename);

            if(debug_lvl == 1) { printf("DEBUG: Request is %s\n", fullname); }

            if (send(socket_fd, fullname, sizeof(fullname), 0) == SOCKET_ERROR) {
                fprintf(stderr, "ERROR: client failed to send %s\n", filename);
                close(socket_fd);
                exit(1);
            } 

            if(debug_lvl == 1) {  puts("DEBUG: Sent requests"); }

            char response[100];
            if(read(socket_fd, response, sizeof(response)+1) == SOCKET_ERROR){
                fprintf(stderr, "ERROR: client failed to read message\n");
                exit(1);
            } 

            if(debug_lvl == 1) { printf("DEBUG: received response: %s\n", response); }
            
            strtok(response, " ");
            char *status = strdup(strtok(NULL, " "));
            char *size = strdup(strtok(NULL, " "));
            int file_size = atoi(size);

            /* GetFile response */
            if(debug_lvl == 1) { printf("status:%s, bytes: %s\n", status, size); }

            
        pthread_mutex_unlock(&mtx);
        if(debug_lvl == 2) { printf("DEBUG: Thread %d unlocked\n", tid); }
        pthread_cond_signal(&bossCond);


        if(strcmp(status, "OK") != 0){
            fprintf(stderr, "ERROR: Received bad response!!!\n");
            continue;
        }

        int remain_data = file_size;
        int len;

        int total = 0;
        char open_name[50];
        FILE *received_file;
        if(debug_lvl == 1) { printf("DEBUG: Thread %d receiving data\n", tid); }
        if(download_path != NULL) {
            sprintf(open_name, "%s%s", download_path, filename);
            received_file = fopen(open_name, "w");
            if (received_file == NULL) {
                fprintf(stderr, "ERROR: Failed to open file: %s --> %s\n", open_name, strerror(errno));
                exit(EXIT_FAILURE);
            }
        }
        while (((len = recv(socket_fd, buffer, BUFFER_SIZE, 0)) > 0) && (remain_data > 0)) {
            total += len;
            if(received_file != NULL) {
                fwrite(buffer, sizeof(char), len, received_file);
            }
            remain_data -= len;
        }
        if(debug_lvl == 1) { fprintf(stdout, "DEBUG: Thread %d received file %s in %d bytes\n", tid, filename, total); }
        if (received_file != NULL) 
            fclose(received_file);
        /* Close the socket and return the response length (in bytes) */
        close(socket_fd);

            
        gettimeofday(&end, NULL);

        seconds  = end.tv_sec  - start.tv_sec;
        useconds = end.tv_usec - start.tv_usec;

        mtime = ((seconds) * 1000 + useconds/1000.0) + 0.5;

        pthread_mutex_lock(&mtx);
        total_time += mtime;
        total_bytes += (long)total;
        pthread_mutex_unlock(&mtx);
    }
    if(debug_lvl == 1) { printf("DEBUG: Thread %d closed, finished %d requests\n", tid, work_done); }

    pthread_mutex_lock(&mtx);
    running_thread--;
    pthread_mutex_unlock(&mtx);
    return;
}

/* random number generator ================================================ */
int random_num(int limit) {
    int divisor = RAND_MAX/(limit+1);
    int result;
    do { 
        result = rand() / divisor;
    } while (result > limit);

    return result;
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

int main(int argc, char **argv) {
    printf("\n======= Hello, I am the web client ======\n");
    int option = 0;
    while ((option = getopt(argc, argv,"hp:t:s:r:d:w:d:m:z:")) != -1) {
        switch (option) {
            case 'p': portNum = atoi(optarg);
                printf ("ARG: Port number is set: '%d'\n", portNum);
                break;
            case 't' : threadNum = atoi(optarg);
                threadNum = threadNum > 100 ? 100 : threadNum;
                threadNum = threadNum < 1 ? 1 : threadNum;
                printf ("ARG: Thread number is set: '%d'\n", threadNum);
                break;
            case 's' : sHostAddress = optarg;
                printf ("ARG: Server address is set: '%s'\n", optarg);
                break;
            case 'r' : request = atoi(optarg);
                request = request > 1000 ? 1000 : request;
                request = request < 1 ? 1 : request;
                printf ("ARG: Request number is set: '%d'\n", request);
                break;
            case 'w' : workLoad = optarg;
                printf ("ARG: workload file path is set: '%s'\n", optarg);
                break;
            case 'd' : download_path = optarg;
                printf ("ARG: Download path is set: '%s'\n", optarg);
                break;
            case 'm' : metrics_path = optarg;
                printf ("ARG: Metrics file path is set: '%s'\n", optarg);
                break;
            case 'z' : debug_lvl = atoi(optarg);
                printf ("ARG: Debug level is set: '%s'\n", optarg);
                break;
            case 'h': print_usage();
                exit (0);
            default: print_usage();
        }
    }    

    he = gethostbyname(sHostAddress);

    /* Thread initiation */
    int each = (threadNum + request -1) / threadNum;
    int total_request = request;
    pthread_t threads[threadNum]; 
    int i;
    for(i = 0; i < threadNum; i++) {
        arg_struct *args_t = (arg_struct *)malloc(sizeof(arg_struct));
        int tid = i;
        args_t->each = each;
        args_t->tid = tid + 1;
        pthread_create(&threads[i],NULL,worker,(void *)args_t);
        running_thread++;
    }
    usleep(500);

    if(debug_lvl == 1) printf("DEBUG: each thread has %d jobs\n", each);

    FILE *fr;
    char line[256];
    fr = fopen (workLoad, "rt");
    if (fr == NULL) {
        fprintf(stderr, "ERROR: Failed to open file: %s --> %s\n", workLoad, strerror(errno));
        exit(EXIT_FAILURE);
    }
    if(debug_lvl == 1) printf("DEBUG: Opened workload file %s\n", workLoad);
    /* Workload generation */
    char *files[100];
    int file_number = 0;
    while( fgets(line, sizeof(line), fr ) ){
        if (line[strlen(line)-1] == '\n')
            line[strlen(line)-1] = '\0';
        if (line[strlen(line)-1] == '\r')
            line[strlen(line)-1] = '\0';
        files[file_number] = malloc(sizeof(line)+1);
        strcpy(files[file_number], line);
        file_number++;
        if(debug_lvl == 2) printf("DEBUG: File name%s\n", line);
    }
    if(debug_lvl == 1) printf("DEBUG: Total file number%d\n", file_number);
    pthread_mutex_lock(&mtx);
    int index;
    for (index = 0; index < request; index++){
        int random = random_num(file_number - 1);
        add_to_list(files[random]);
    }
    pthread_mutex_unlock(&mtx);

    if(debug_lvl == 1)
        puts("DEBUG: STARTING!!!");

    /* Threads wake up */
    while( running_thread > 0 || request > 0 ){
        if(debug_lvl == 1) { printf("DEBUG: Boss locking, request: %d, threads: %d\n", request, running_thread); }
        pthread_mutex_lock(&mtx);
            if(debug_lvl > 1) { puts("DEBUG: Boss unlocking and broadcasting"); }
        pthread_mutex_unlock(&mtx);
        pthread_cond_broadcast(&workerCond);
        if(debug_lvl > 1){ puts("DEBUG: Boss unlocked"); }
    }

    /* wait/join threads */
    for(i = 0; i < threadNum; i++) { 
        if(debug_lvl == 1) printf("Thread %d joined\n", i + 1 );        
        pthread_cond_broadcast(&workerCond);
        pthread_join(threads[i], NULL);
    }


    FILE *metrics_file;
    metrics_file = fopen(metrics_path, "w");
    char *metrics_content = malloc(sizeof(metrics_content));

    sprintf(metrics_content, "Webclient stats report:\n\tTotal bytes received\t%ld B\n\tAverage response time\t%f S\n\tAverage troughput   \t%.1f B/S", total_bytes, (double)total_time/(total_request*1000), (double)total_bytes*1000/total_time);
    printf("\n%s\n", metrics_content);
    fputs(metrics_content, metrics_file);

    printf("\n===== All client jobs are completed =====\n");
    exit(0);
}

