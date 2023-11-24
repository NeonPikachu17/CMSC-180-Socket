#include <stdio.h> 
#include <stdlib.h>
#include <string.h> 
#include <netdb.h> 
#include <sys/types.h> 
#include <sys/socket.h>
#include <arpa/inet.h>
#include <math.h>
#include <unistd.h>
#include <pthread.h> 

#define MAX_CONNECTIONS 8
#define BUFFER_SIZE 1024 
#define PORT 8094

/* thread structure */
typedef struct {
    int thread_id;
    int connfd;
    int slaves;
    int tcount;
} thread_data;

// Global Matrix for threads
int** matrix;
int* arr;

/* thread function */
void * thread_function(void *arg) {
    char buffer[BUFFER_SIZE];
    thread_data *data = (thread_data *)arg;
    int thread_id = data->thread_id;
    int connfd = data->connfd;
    int tcount = data->tcount;

    int slaves = data->slaves;

    int thread_start = thread_id * arr[thread_id];
    int thread_end = thread_start + arr[thread_id];

    printf("start = %d", thread_start);
    printf("end = %d", thread_end);
    
    // Create submatrix memory
    int rowcount = thread_end - thread_start;
    int number_to_send = rowcount; // Put your value
    int converted_number = htonl(number_to_send);
    // Write the number to the opened socket
    write(connfd, &converted_number, sizeof(converted_number));

    number_to_send = tcount; // Put your value
    converted_number = htonl(number_to_send);
    // Write the number to the opened socket
    write(connfd, &converted_number, sizeof(converted_number));


    // Write the number to the opened socket
    // printf("%d %d\n", rowcount, tcount);
    for(int row1 = thread_start; row1 < thread_end; row1++){
        if(row1 > tcount) break;
        for(int col1 = 0; col1 < tcount; col1++){
            converted_number = htonl(matrix[row1][col1]);
            write(connfd, &converted_number, sizeof(int));
            // printf("sent\n");
        }
    }
    // 
    // write(connfd, &submatrix, sizeof(submatrix));

    printf("[+] Sent Submatrix\n");
    bzero(buffer, 1024);
    recv(connfd, buffer, sizeof(buffer), 0);
    printf("Thread %d %s\n", thread_id, buffer);
    close(connfd);

    return NULL;
}

int* distribute_arrays (int n, int t){ 
	int quotient = n / t;
	int remainder = n % t;

	int* array = (int*)malloc(t*sizeof(int));

	for (int i = 0; i < t; i++){
		array[i] = quotient;
		if(i < remainder) array[i] += 1;
	}

	return array;

}

int main(int argc, char* argv[]) {
    // For getting info
    time_t t;

    // For random number generator
    srand(time(NULL));

    int status = strtol(argv[3], NULL, 10);
    int tcount = strtol(argv[1], NULL, 10);

    FILE *fp;
    int port = strtol(argv[2], NULL, 10);
    // char ip[] = "127.0.0.1";


    // Count number of slaves
    int slaves = MAX_CONNECTIONS;

    arr = distribute_arrays(tcount, slaves);
    for(int i = 0; i < slaves; i++){
        printf("[%d] length is %d\n", i, arr[i]);
    }
    // fp = fopen("config.conf", "r");
    // fgets(ip, 255, (FILE*)fp);
    // printf("%s\n", ip );
    
    // for debugging
    // int tcount = 10;

    switch(status) {
        // master
        case 0: {
            int sockfd, connfd;
            struct sockaddr_in servaddr;
            pthread_t threads[MAX_CONNECTIONS];

            // Creation of dynamic 2d matrix
            matrix = (int**)malloc(tcount * sizeof(int*));
            for(int count = 0; count < tcount; count++){
                matrix[count] = malloc(tcount * sizeof(int));
            }

            int thread_id = 0;

            // Create random matrix
            for(int i = 0; i < tcount; i++){    
                for(int j = 0; j < tcount; j++) {
                    matrix[i][j] = ((rand() % 100) + 1);
                }
            }

            // For debugging
            // for(int i = 0; i < tcount; i++){
            //     for(int j = 0; j < tcount; j++){
            //         printf("%d ", matrix[i][j]);
            //     }
            //     printf("\n");
            // }
            // printf("\n");

            sockfd = socket(AF_INET, SOCK_STREAM, 0);
            if (sockfd == -1) {
                printf("Socket creation failed\n");
                exit(0);
            } else {
                printf("Socket successfully created\n");
            }

            memset(&servaddr, 0, sizeof servaddr);

            servaddr.sin_family = AF_INET;
            servaddr.sin_addr.s_addr = inet_addr("127.0.0.1");
            servaddr.sin_port = htons(port);


            if (bind(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) != 0) {
                printf("Socket binding failed\n");
                exit(0);
            } else {
                printf("Socket successfully bound\n");
            }
            printf("hi\n");

            if (listen(sockfd, 5) != 0) {
                printf("Listen failed\n");
                exit(0);
            } else {
                printf("Server listening\n");
            }

            // time start
            clock_t tic = clock();
            while (1) {
                connfd = accept(sockfd, (struct sockaddr *)NULL, NULL);
                if (connfd == -1) {
                    printf("Accept failed\n");
                    continue;
                }

                // for slaves
                // thread_data *data = malloc(sizeof(thread_data));
                thread_data *data = malloc(sizeof(thread_data));
                data->thread_id = thread_id;
                data->connfd = connfd;
                data->tcount = tcount;

                double divide = (double) tcount / (double) slaves;
                int ceiling = (int) floor(divide);

                data->slaves = slaves;

                pthread_create(&threads[thread_id], NULL, thread_function, (void *)data);

                thread_id++;
                
                if (thread_id == MAX_CONNECTIONS) {
                    printf("Waiting for Acknowledgement\n");
                    // Join the threads to terminate the program
                    for(int i = 0; i < MAX_CONNECTIONS; i++){
                        printf("Slave %d joined.\n", i);
                        pthread_join(threads[i], NULL);
                    }
                    break;
                }
            }
            clock_t toc = clock();
            double time_taken = (double) (toc - tic) / CLOCKS_PER_SEC; // Calculate the elapsed time
            printf("time elapsed: %f seconds\n", time_taken);
            close(sockfd);
            return 0;
        }
        // client
        case 1: {
            int sockfd;
            char buffer[BUFFER_SIZE];
            struct sockaddr_in servaddr;

            sockfd = socket(AF_INET, SOCK_STREAM, 0);
            if (sockfd == -1) {
                printf("Socket creation failed\n");
                exit(0);
            }

            memset(&servaddr, 0, sizeof(servaddr));

            servaddr.sin_family = AF_INET;
            servaddr.sin_addr.s_addr = inet_addr("127.0.0.1");
            servaddr.sin_port = htons(port);

            // printf("hi\n");
            if (connect(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) != 0) {
                printf("Connection to the server failed\n");
                exit(0);
            }

            int received_row = 0;
            int received_col = 0;

            read(sockfd, &received_row, sizeof(received_row));
            int row = ntohl(received_row);

            read(sockfd, &received_col, sizeof(received_col));
            int col = ntohl(received_col);

            // printf("%d, %d\n", row, col);

            // Goal | Pass the matrix from server to client
            int **received_int = (int**)malloc(tcount * sizeof(int*));
            for(int count = 0; count < tcount; count++){
                received_int[count] = malloc(tcount * sizeof(int));
            }
            // int received_int[row][col];
            int received_number;
            for(int row1 = 0; row1 < row; row1++){
                for(int col1 = 0; col1 < col; col1++){
                    read(sockfd, &received_number, sizeof(received_number));
                    received_int[row1][col1] = ntohl(received_number);
                    // printf("%d\n", ntohl(received_number));
                }
            }
            // read(sockfd, received_int, sizeof(received_int));
            
            // for debugging
            // for(int i = 0; i < row; i++){
            //     for(int j = 0; j < col; j++){
            //         printf("%d ", received_int[i][j]);
            //     }
            //     printf("\n");
            // }

            bzero(buffer, 1024);
            strcpy(buffer, "[+] Acknowledged");
            send(sockfd, buffer, strlen(buffer), 0);

            return 0;
        }
    }
}