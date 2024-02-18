#ifndef SERVER_H
#define SERVER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdbool.h>
#include <sys/epoll.h>

#define PORT 8080
#define BUFFER_SIZE 4096
#define MAX_EVENTS 10
#define THREAD_POOL_SIZE 4
#define MAX_OUTPUT_SIZE 4096

// 客户端请求类型
enum RequestType {
    SEND_COMMAND,
    SEND_FILE,
    RECEIVE_FILE,
};

struct Request {
    enum RequestType type;
    char data[BUFFER_SIZE]; // 命令字符串或文件路径
};

struct ThreadPool {
    pthread_t threads[THREAD_POOL_SIZE];
    int epoll_fds[THREAD_POOL_SIZE];
};

void send_data_to_client(int client_socket, const char *data);
void handle_send_file_request(int client_socket, const char *filename);
void handle_receive_file_request(int client_socket, const char *filename);
void *thread_function(void *arg);
char *shell_exec(const char *command);

#endif /* SERVER_H */
