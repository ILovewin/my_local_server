#include "client.h"

void print_help() {
    printf("Available commands:\n");
    printf("1. ls\n");
    printf("2. mkdir <directory_name>\n");
    printf("3. cd <directory_path>\n");
    printf("4. sendfile <file_name>\n");
    printf("5. receivefile <file_path>\n");
    printf("6. help\n");
}

void handle_signal(int signal) {
    if (signal == SIGINT) {
        printf("\nExiting...\n");
        exit(0);
    }
}

void save_file(const char* filename, const char* data, size_t size) {
    FILE* file = fopen(filename, "wb");
    if (file == NULL) {
        perror("Error opening file for writing");
        return;
    }
    if (fwrite(data, 1, size, file) != size) {
        perror("Error writing to file");
        fclose(file);
        return;
    }
    fclose(file);
    printf("File saved as %s\n", filename);
}


int main() {
    // 设置 Ctrl+C 信号处理函数
    signal(SIGINT, handle_signal);

    int client_socket;
    struct sockaddr_in server_addr;
    struct Request request;
    char file_data[MAX_FILE_SIZE];

    // 创建套接字
    if ((client_socket = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("Error creating socket");
        exit(EXIT_FAILURE);
    }

    // 设置服务端地址信息
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    if (inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr) <= 0) {
        perror("Invalid address or address not supported");
        exit(EXIT_FAILURE);
    }

    // 连接到服务端
    if (connect(client_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("Error connecting to server");
        exit(EXIT_FAILURE);
    }

    while (1) {
        printf("\nEnter command: ");
        fgets(request.data, sizeof(request.data), stdin);
        request.data[strcspn(request.data, "\n")] = '\0';  // 去除末尾的换行符

        char new_data[100];
        strcpy(new_data, request.data);


        char *token = strtok(new_data, " ");
        if (token == NULL) {
            printf("Invalid command\n");
            continue;
        }

        // 检查命令类型
        if (strcmp(token, "mkdir") == 0 || strcmp(token, "cd") == 0 || strcmp(token, "ls") == 0) {
            // ls, mkdir和cd命令
            request.type = SEND_COMMAND;
        } else if (strcmp(token, "sendfile") == 0) {
            // 发送文件命令
            request.type = SEND_FILE;
        } else if (strcmp(token, "receivefile") == 0) {
            // 接收文件命令
            request.type = RECEIVE_FILE;
        } else if (strcmp(token, "help") == 0) {
            // 帮助命令
            print_help(); // 打印帮助信息
            continue;
        } else {
            printf("Unknown command\n");
            continue;
        }

        // 检查参数数量
        int num_args = 0;
        while (token != NULL) {
            token = strtok(NULL, " ");
            num_args++;
        }

        if (num_args > 2) {
            printf("Error: Too many arguments. Please re-enter the command.\n");
            continue;
        }

        printf("request.data:%s\n", request.data);

        // 根据请求类型发送请求...
        if (send(client_socket, &request, sizeof(struct Request), 0) == -1) {
            perror("Error sending request");
            exit(EXIT_FAILURE);
        }

        if (request.type == SEND_FILE) {
            // 初始化缓冲区大小
            size_t allocated_size = MAX_FILE_SIZE;
            char* file_data = (char*)malloc(allocated_size);
            if (!file_data) {
                perror("Error allocating memory for file data");
                // 继续等待用户输入
                continue;
            }

            ssize_t total_bytes_received = 0;
            size_t end_marker_length = strlen("END_OF_FILE");
            while (1) {
                ssize_t bytes_received = recv(client_socket, file_data + total_bytes_received, allocated_size - total_bytes_received, 0);
                if (bytes_received == -1) {
                    perror("Error receiving file data");
                    free(file_data); // 释放内存
                    // 继续等待服务端输入
                    break;
                } else if (bytes_received == 0) {
                    printf("Server closed connection\n");
                    break; // 关闭套接字退出循环
                }
                total_bytes_received += bytes_received;

                // 检查是否需要扩展缓冲区
                if (total_bytes_received == allocated_size) {
                    allocated_size *= 2; // 扩展已分配的内存
                    char* new_file_data = (char*)realloc(file_data, allocated_size);
                    if (!new_file_data) {
                        perror("Error reallocating memory for file data");
                        free(file_data); // 释放内存
                        // 继续等待服务端输入
                        break;
                    }
                    file_data = new_file_data;
                }

                // 检查接收到的数据是否以 "1" 开头，如果是，则为错误消息
                if (file_data[0] == '1') {
                    printf("Error: %s\n", file_data + 1); // 打印错误消息，跳过 "1"
                    free(file_data); // 释放内存
                    break; // 退出循环
                }

                // 检查是否收到了结束标志
                if (bytes_received >= end_marker_length && memcmp(file_data + total_bytes_received - end_marker_length, "END_OF_FILE", end_marker_length) == 0) {
                    printf("File transfer complete\n");
                    // 截断文件数据，去除结束标志
                    total_bytes_received -= end_marker_length;
                    break;
                }

            }

            // 如果没有接收到错误消息，则将数据写入文件
            if (file_data[0] != '1') {
                // 获取文件名
                char* token = strtok(request.data, " ");
                token = strtok(NULL, " ");
                if (token == NULL) {
                    printf("Invalid file name\n");
                    free(file_data); // 释放内存
                    // 继续等待用户输入
                    break;
                }

                // 将文件数据保存为本地文件
                save_file(token, file_data, total_bytes_received);
                printf("File received and saved as %s\n", token);
            }

            // 释放内存
            free(file_data);
        }


        else if (request.type == RECEIVE_FILE) {
            // 打开要发送的文件
            // 获取文件路径
            token = strtok(request.data, " ");
            token = strtok(NULL, " ");
            FILE *file = fopen(token, "rb");
            if (file == NULL) {
                perror("Error opening file");
                // 继续等待用户输入
                continue;
            }

            // 动态分配缓冲区
            char *file_data = (char *)malloc(MAX_FILE_SIZE);
            if (file_data == NULL) {
                perror("Error allocating memory for file data");
                exit(EXIT_FAILURE);
            }

            // 读取文件数据并发送给服务器
            while (1) {
                size_t bytes_read = fread(file_data, 1, MAX_FILE_SIZE, file);
                if (bytes_read > 0) {
                    ssize_t bytes_sent = send(client_socket, file_data, bytes_read, 0);
                    if (bytes_sent == -1) {
                        perror("Error sending file data");
                        free(file_data);
                        exit(EXIT_FAILURE);
                    }
                }
                
                if (bytes_read < MAX_FILE_SIZE) {
                    if (feof(file)) {
                        // 文件读取完成，发送结束标志
                        ssize_t bytes_sent = send(client_socket, "END_OF_FILE", strlen("END_OF_FILE"), 0);
                        if (bytes_sent == -1) {
                            perror("Error sending end of file marker");
                        }
                        printf("File sent successfully\n");
                        break;
                    } else if (ferror(file)) {
                        // 读取文件出错
                        perror("Error reading file");
                        free(file_data);
                        exit(EXIT_FAILURE);
                    }
                }

                // 清空缓冲区
                memset(file_data, 0, MAX_FILE_SIZE);
            }

            // 释放缓冲区
            free(file_data);

        }


        else {
            char buffer[MAX_FILE_SIZE];
            ssize_t bytes_received = recv(client_socket, buffer, MAX_FILE_SIZE, 0);
            if (bytes_received == -1) {
                perror("Error receiving data from server");
                exit(EXIT_FAILURE);
            } else if (bytes_received == 0) {
                printf("Server closed connection\n");
                exit(EXIT_FAILURE);
            } else {
                // 在接收到的数据末尾添加 null 终止字符，以便打印
                buffer[bytes_received] = '\0';
                printf("Received from server: %s\n", buffer);
            }
        }

    }    
    return 0;
}
