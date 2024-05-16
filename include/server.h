#ifndef SERVER_H
#define SERVER_H

#include "lst_timer.h"
#include "threadpool.h"

#define PORT 8080
#define MAX_EVENTS 100
#define THREAD_POOL_SIZE 10
#define MAX_OUTPUT_SIZE 4096

ThreadPool *pool;
const int MAX_FD = 65536;           //最大文件描述符
int user_count = 0;
int pipefd[2];
int epollfd;

void send_data_to_client(int client_socket, const char *data);
void handle_send_file_request(int client_socket, const char *filename);
void handle_receive_file_request(int client_socket, const char *filename);
char *shell_exec(const char *command);

void timer(int connfd, struct sockaddr_in client_address);
void deal_timer(struct util_timer *timer, int sockfd);
bool dealwithsignal(bool *timeout, bool *stop_server);
bool dealclientconnet(int server_socket);
void dealwithread(int sockfd);
void eventLoop(int server_socket);
void eventlisten(int server_socket);
void adjust(struct util_timer *timer);

#endif /* SERVER_H */
