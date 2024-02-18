#ifndef CLIENT_H
#define CLIENT_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define SERVER_IP "127.0.0.1"
#define SERVER_PORT 8080
#define MAX_COMMAND_SIZE 256
#define MAX_FILE_SIZE 1024

// 客户端请求类型
enum RequestType {
    SEND_COMMAND,
    SEND_FILE,
    RECEIVE_FILE,
};

struct Request {
    enum RequestType type;
    char data[MAX_COMMAND_SIZE];  // 命令字符串或文件路径
};

void print_help();
void handle_signal(int signal);
void save_file(const char* filename, const char* data, size_t size);

#endif /* CLIENT_H */
