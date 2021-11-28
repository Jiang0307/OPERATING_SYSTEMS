#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <ctype.h>
#include <string.h>
#include <netdb.h>
#include <ctype.h>
#include <stdatomic.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "types.h"
#include "sock.h"

#define SIZE 1024
#define DATABASE "./DATABASE/"
#define FREE_RESOURCE \
close(listenfd);      \
free(server_port);

static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

int check_file_exists(const char *fname)
{
    FILE *file;
    if ((file = fopen(fname, "r")))
    {
        fclose(file);
        return 1;
    }
    else
        return 0;
}

void* thread_handler(void* arg)
{
    int socket_fd = *((int *)arg);   //每個client的IDENTIFIER , 強制轉型成int
    free(arg);
    printf("[THREAD INFO] Thread %lu created\n",pthread_self() );

    char input_line[SIZE], command[SIZE], key[SIZE], value[SIZE], check_input_format[SIZE], return_to_client[SIZE], filename[SIZE];

    //while( (recv_len = recv(socket_fd, command, SIZE, 0)) > 0) // recv command from client
    while( recv(socket_fd, input_line, SIZE, 0) ) // recv command from client
    {
        memset(command,0,SIZE);
        memset(key,0,SIZE);
        memset(value,0,SIZE);
        memset(check_input_format,0,SIZE);
        memset(return_to_client,0,SIZE);
        memset(filename,0,SIZE);

        int argument_count = sscanf(input_line,"%s%s%s%s",command,key,value,check_input_format);

        if(argument_count<1 || argument_count>3)
        {
            strcat(return_to_client,"[ERROR] Wrong command format\n");
        }

        else
        {
            if(command[0]=='S' && command[1]=='E' && command[2]=='T')   //set : recv key and value
            {
                if(argument_count != 3)
                {
                    strcat(return_to_client,"[ERROR] Wrong command format\n");
                }
                else
                {
                    //printf("\nSET state\n");
                    strcat(filename,DATABASE);
                    strcat(filename,key);
                    strcat(filename,".KEY_FILE");

                    pthread_mutex_lock(&mutex);
                    FILE* file_pointer = NULL;
                    file_pointer = fopen(filename, "w");
                    fprintf(file_pointer, "%s", value );
                    fclose(file_pointer);
                    //sleep(5);
                    pthread_mutex_unlock(&mutex);

                    strcat(return_to_client,"[OK] Key-value pair (");
                    strcat(return_to_client,key);
                    strcat(return_to_client,",");
                    strcat(return_to_client,value);
                    strcat(return_to_client,") is stored!\n");
                }
            }
            else if(command[0]=='G' && command[1]=='E' && command[2]=='T')
            {
                if(argument_count != 2)
                {
                    strcat(return_to_client,"[ERROR] Wrong command format\n");
                }
                else
                {
                    //printf("\nGET state\n");
                    strcpy(filename,DATABASE);
                    strcat(filename,key);
                    strcat(filename,".KEY_FILE");

                    pthread_mutex_lock(&mutex); //LOCK
                    if( check_file_exists(filename)==0 )
                    {
                        //sleep(5);
                        pthread_mutex_unlock(&mutex); //UNLOCK
                        strcat(return_to_client,"[ERROR] Key does not exist!\n");
                    }
                    else
                    {
                        FILE* file_pointer = NULL;
                        file_pointer = fopen(filename,"r");
                        fscanf(file_pointer,"%s",value);
                        fclose(file_pointer);
                        //sleep(5);
                        pthread_mutex_unlock(&mutex); //UNLOCK

                        strcat(return_to_client,"[OK] The value of ");
                        strcat(return_to_client,key);
                        strcat(return_to_client," is ");
                        strcat(return_to_client,value);
                        strcat(return_to_client,"\n");
                    }
                }
            }
            else if(command[0]=='D' && command[1]=='E' && command[2]=='L' && command[3]=='E' && command[4]=='T' && command[5]=='E' )
            {
                if(argument_count != 2)
                {
                    strcat(return_to_client,"[ERROR] Wrong command format\n");
                }
                else
                {
                    //printf("\nDELETE state\n");
                    strcpy(filename,DATABASE);
                    strcat(filename,key);
                    strcat(filename,".KEY_FILE");

                    pthread_mutex_lock(&mutex); //LOCK
                    if( check_file_exists(filename)==0 )
                    {
                        //sleep(5);
                        pthread_mutex_unlock(&mutex); //UNLOCK
                        strcat(return_to_client,"[ERROR] Key does not exist!\n");
                    }
                    else
                    {
                        remove(filename);
                        //sleep(5);
                        pthread_mutex_unlock(&mutex); //UNLOCK

                        strcat(return_to_client,"[OK] Key \"");
                        strcat(return_to_client,key);
                        strcat(return_to_client,"\" is removed!\n");
                    }
                }
            }
            else
            {
                strcat(return_to_client,"[ERROR] Wrong command format\n");
            }
        }
        send(socket_fd, return_to_client, SIZE, 0);
    }
    printf("exiting thread\n");   //some client is finished
    close(socket_fd);
    return NULL;
}

int main(int argc, char **argv)
{
    char *server_port = 0;
    int opt = 0;
//=======================================================
    int *socket_fd;
    struct sockaddr_in client_address;
    socklen_t client_address_len = sizeof(struct sockaddr_in);
    pthread_t thread_id;
    pthread_attr_t attr;
    pthread_attr_init(&attr);
//=======================================================
    /* Parsing args */
    while( (opt = getopt(argc, argv, "p:")) != -1 )
    {
        switch(opt)
        {
        case 'p':
            server_port = malloc(strlen(optarg) + 1);
            strncpy( server_port, optarg, strlen(optarg) );
            break;
        case '?':
            fprintf( stderr, "Unknown option \"-%c\"\n", isprint(optopt) ? optopt : '#' );
            return 0;
        }
    }
    if (!server_port)
    {
        fprintf(stderr, "Error! No port number provided!\n");
        exit(1);
    }
    /* Open a listen socket fd */
    int listenfd __attribute__((unused)) = open_listenfd(server_port);
//================REMOVE OLD DATABASE================
    struct stat st = {0};
    if(stat(DATABASE, &st) == 0)
    {
        char cmd[32];
        sprintf(cmd, "rm -rf %s", DATABASE);
        int ret = system(cmd);
        if(ret == 0)
            printf("[INFO] Start with a clean database...\n");
    }
    else
        printf("[INFO] Start with a clean database...\n");
//================CREATE NEW DATABASE================
    printf("[INFO] Initializing the server...\n");
    struct stat st2 = {0};
    if(stat(DATABASE, &st2) == -1)
    {
        mkdir(DATABASE, 0700);
    }
    printf("[INFO] Server initialized!\n");
    printf("[INFO] Listening on the port %s...\n",server_port);
//================START CODING================
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    while(1)
    {
        //只要有thread(client)就會有新的 所以需要用指標
        socket_fd = malloc(sizeof(int));
        *socket_fd = accept(listenfd, (struct sockaddr*)&client_address, &client_address_len); //accept代表client server建立連線  停在這直到有client request

        if(*socket_fd == -1)
        {
            perror("accept()");
            FREE_RESOURCE
            return -1;
        }
        else
        {
            int host_id = *(socket_fd);
            printf("\n[CLIENT CONNECTED] Connected to client (localhost,%d)\n",host_id);
        }

        if( pthread_create(&thread_id, &attr, &thread_handler, socket_fd)>0 )   //pthread_create(&thread_id, &attr, &thread_handler, socket_fd)
        {
            perror("pthread_create()");
            FREE_RESOURCE
            return -1;
        }
    }
    return 0;
}