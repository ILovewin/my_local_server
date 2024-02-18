#include "server.h"

char *shell_exec(const char *command) {
    // 执行命令并获取输出
    FILE *fp = popen(command, "r");
    if (fp == NULL) {
        perror("Error executing command");
        exit(EXIT_FAILURE);
    }

    // 读取命令的输出
    char *output = (char *)malloc(MAX_OUTPUT_SIZE);
    if (output == NULL) {
        perror("Error allocating memory for output");
        exit(EXIT_FAILURE);
    }
    memset(output, 0, MAX_OUTPUT_SIZE);
    size_t bytes_read = fread(output, 1, MAX_OUTPUT_SIZE - 1, fp);
    if (bytes_read == 0) {
        perror("Error reading output");
        exit(EXIT_FAILURE);
    }

    // 关闭文件指针
    pclose(fp);

    return output;
}

void send_data_to_client(int client_socket, const char *data) {
    ssize_t bytes_sent = send(client_socket, data, strlen(data), 0);
    if (bytes_sent == -1) {
        perror("Error sending data to client");
        exit(EXIT_FAILURE);
    }
}

void handle_send_file_request(int client_socket, const char *filename) {
    char current_directory[BUFFER_SIZE];
    char filepath[BUFFER_SIZE];
    getcwd(current_directory, sizeof(current_directory)); // 获取当前工作目录
    // 将current_directory和filename连接起来，形成一个完整的文件路径字符串, 并将结果存储在filepath变量中
    snprintf(filepath, sizeof(filepath), "%s/%s", current_directory, filename);

    // 检查文件是否存在
    if (access(filepath, F_OK) != -1) {
        // 文件存在，尝试打开文件
        FILE *file = fopen(filepath, "rb");
        if (file == NULL) {
            perror("Error opening file");
            // 发送错误消息给客户端
            const char *error_message = "1Error opening file";
            send_data_to_client(client_socket, error_message);
        } else {
            // 文件打开成功，读取文件内容并发送给客户端
            char buffer[BUFFER_SIZE];
            size_t bytes_read;
            while ((bytes_read = fread(buffer, 1, sizeof(buffer), file)) > 0) {
                if (write(client_socket, buffer, bytes_read) == -1) {
                    perror("Error sending file");
                    // 发送错误消息给客户端
                    const char *error_message = "1Error sending file";
                    send_data_to_client(client_socket, error_message);
                    break;
                }
                memset(buffer, 0, sizeof(buffer));
            }
            fclose(file);
        }
    } else {
        // 文件不存在，发送提示消息给客户端
        const char *error_message = "1File does not exist";
        send_data_to_client(client_socket, error_message);
    }
}

void handle_receive_file_request(int client_socket, const char *filename) {
    char current_directory[BUFFER_SIZE];
    char filepath[BUFFER_SIZE];
    getcwd(current_directory, sizeof(current_directory)); // 获取当前工作目录
    // 将current_directory和filename连接起来，形成一个完整的文件路径字符串, 并将结果存储在filepath变量中
    snprintf(filepath, sizeof(filepath), "%s/%s", current_directory, filename);

    // 打开要保存的文件
    FILE *file = fopen(filepath, "wb");
    if (file == NULL) {
        perror("Error opening file");
        // 发送错误消息给客户端
        const char *error_message = "Error opening file";
        send_data_to_client(client_socket, error_message);
        return;
    }

    // 接收并保存文件数据
    ssize_t total_bytes_received = 0;
    while (1) {
        char buffer[BUFFER_SIZE];
        ssize_t bytes_received = recv(client_socket, buffer, sizeof(buffer), 0);
        if (bytes_received == -1) {
            perror("Error receiving file data");
            // 发送错误消息给客户端
            const char *error_message = "Error receiving file data";
            send_data_to_client(client_socket, error_message);
            fclose(file);
            remove(filepath); // 删除部分接收的文件
            return;
        } else if (bytes_received == 0) {
            // 客户端关闭连接
            printf("Client closed connection unexpectedly\n");
            fclose(file);
            remove(filepath); // 删除部分接收的文件
            return;
        }
        total_bytes_received += bytes_received;

        // 将接收到的数据写入文件
        size_t bytes_written = fwrite(buffer, 1, bytes_received, file);
        if (bytes_written < bytes_received) {
            perror("Error writing file");
            // 发送错误消息给客户端
            const char *error_message = "Error writing file";
            send_data_to_client(client_socket, error_message);
            fclose(file);
            remove(filepath); // 删除部分接收的文件
            return;
        }

        // 检查是否接收到结束标志
        if (strstr(buffer, "END_OF_FILE") != NULL) {
            // 删除结束标志
            fseek(file, -strlen("END_OF_FILE"), SEEK_END);
            int trunc_result = ftruncate(fileno(file), ftell(file));
            if (trunc_result != 0) {
                perror("Error truncating file");
                // 发送错误消息给客户端
                const char *error_message = "Error truncating file";
                send_data_to_client(client_socket, error_message);
                fclose(file);
                remove(filepath); // 删除部分接收的文件
                return;
            }
            // 文件接收完成
            printf("File received successfully\n");
            break;
        }
    }

    // 关闭文件
    fclose(file);
}

void *thread_function(void *arg) {
    int epoll_fd = *((int *)arg);
    struct epoll_event events[MAX_EVENTS];

    for (int i = 0; i < MAX_EVENTS; ++i) {
        events[i].events = 0;  // 清空事件
        events[i].data.fd = -1; // 设置文件描述符为无效值
    }

    printf("epoll_fd :%d\n", epoll_fd);

    while (1) {
        int num_events = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
        if (num_events == -1) {
            perror("Error waiting for events");
            exit(EXIT_FAILURE);
        }

        // 处理每个事件
        for (int i = 0; i < num_events; ++i) {
            int client_socket = events[i].data.fd;

            if (events[i].events & EPOLLIN) {
                // 处理读取事件
                struct Request request;
                ssize_t bytes_received = recv(client_socket, &request, sizeof(struct Request), 0);
                if (bytes_received <= 0) {
                    // 客户端关闭连接或发生错误
                    printf("Client disconnected\n");
                    close(client_socket);
                } else {
                    char new_data[100];
                    strcpy(new_data, request.data);
                    char *token = strtok(new_data, " ");
                    // 根据请求类型处理请求
                    switch (request.type) {
                        case SEND_COMMAND:
                            // 执行相应的命令处理逻辑
                            printf("Received command from client: %s\n", request.data);
                            if (strcmp(token, "ls") == 0) {
                                char *command = request.data;
                                char *output = shell_exec(command);
                                printf("Command output:\n%s\n", output);
                                send_data_to_client(client_socket, output);
                                free(output);
                            }
                             // 处理 cd命令
                            else if (strcmp(token, "cd") == 0){
                                char *token = strtok(NULL, " "); // 获取目录参数
                                if (token == NULL) {
                                    // 如果没有提供目录参数，则发送错误消息给客户端
                                    const char *error_message = "Invalid directory";
                                    send_data_to_client(client_socket, error_message);
                                } else {
                                    // 执行 chdir 函数切换目录
                                    if (chdir(token) == 0) {
                                        // 切换目录成功
                                        const char *message = "Directory changed successfully";
                                        send_data_to_client(client_socket, message);
                                    } else {
                                        // 切换目录失败
                                        const char *error_message = "Failed to change directory";
                                        send_data_to_client(client_socket, error_message);
                                    }
                                }
                            }
                            else {
                                // 处理 mkdir命令       
                                if (system(request.data) == 0) {
                                    // 命令执行成功
                                    const char *message = "Directory created successfully";
                                    send_data_to_client(client_socket, message);
                                } else {
                                    // 命令执行失败
                                    const char *message = "Failed to create directory";
                                    send_data_to_client(client_socket, message);
                                }
                            }
                            break;
                        case SEND_FILE:
                            // 处理发送文件请求
                            printf("Client requests file: %s\n", request.data);
                            if(token != NULL) {
                                token = strtok(NULL, " ");
                                printf("token: %s\n", token);
                                handle_send_file_request(client_socket, token);
                                const char *end_message = "END_OF_FILE";
                                send_data_to_client(client_socket, end_message);
                            }
                            break;
                        case RECEIVE_FILE:
                            // 处理接收文件请求
                            printf("Received file from client: %s\n", request.data);
                            if(token != NULL) {
                                token = strtok(NULL, " ");
                                char *filename = NULL;

                                // 找到路径中最后一个斜杠的位置
                                char *last_slash = strrchr(token, '/');
                                if (last_slash != NULL) {
                                    // 获取文件名的起始位置（即斜杠后面的字符）
                                    filename = last_slash + 1;
                                } else {
                                    // 如果没有斜杠，则整个路径就是文件名
                                    filename = token;
                                }

                                printf("文件名：%s\n", filename);
                                handle_receive_file_request(client_socket, filename);
                            }
                            break;
                        default:
                            printf("Unknown request type from client\n");
                            break;
                    }
                }
            } else if (events[i].events & (EPOLLHUP | EPOLLERR)) {
                // 处理断开连接或错误事件
                printf("Client disconnected or encountered an error\n");
                close(client_socket);
            }
        }
    }
}

int main() {
    int server_socket;
    struct sockaddr_in server_address;
    socklen_t server_address_len = sizeof(server_address);
    struct ThreadPool thread_pool;

    // 创建服务器套接字
    if ((server_socket = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("Error creating socket");
        exit(EXIT_FAILURE);
    }

    // 设置服务器地址
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(PORT);
    server_address.sin_addr.s_addr = INADDR_ANY;

    // 绑定套接字
    if (bind(server_socket, (struct sockaddr *)&server_address, sizeof(server_address)) == -1) {
        perror("Error binding socket");
        exit(EXIT_FAILURE);
    }

    // 监听连接
    if (listen(server_socket, 10) == -1) {
        perror("Error listening for connections");
        exit(EXIT_FAILURE);
    }

    printf("Server listening on port %d...\n", PORT);

    // 创建线程池
    for (int i = 0; i < THREAD_POOL_SIZE; ++i) {
        // 创建epoll实例
        if ((thread_pool.epoll_fds[i] = epoll_create(1)) == -1) {
            perror("Error creating epoll instance");
            exit(EXIT_FAILURE);
        }

        // 创建线程
        if (pthread_create(&thread_pool.threads[i], NULL, thread_function, &thread_pool.epoll_fds[i]) != 0) {
            perror("Error creating thread");
            exit(EXIT_FAILURE);
        }
    }

    while (1) {
        // 接受新的连接
        int client_socket;
        struct sockaddr_in client_address;
        socklen_t client_address_len = sizeof(client_address);
        if ((client_socket = accept(server_socket, (struct sockaddr *)&client_address, &client_address_len)) == -1) {
            perror("Error accepting connection");
            continue;
        }

        printf("Connection accepted\n");

        // 将新连接的客户端套接字添加到某个线程的epoll监听中
        struct epoll_event event;
        event.events = EPOLLIN | EPOLLOUT | EPOLLHUP | EPOLLERR; // 表示我们对读写事件感兴趣
        event.data.fd = client_socket;  
        int thread_index = client_socket % THREAD_POOL_SIZE;  // 使用客户端套接字作为哈希函数，将客户端套接字分发到不同的线程
        if (epoll_ctl(thread_pool.epoll_fds[thread_index], EPOLL_CTL_ADD, client_socket, &event) == -1) {
            perror("Error adding client socket to epoll instance");
            exit(EXIT_FAILURE);
        }
    }

    // 销毁 epoll 实例
    for (int i = 0; i < THREAD_POOL_SIZE; ++i) {
        close(thread_pool.epoll_fds[i]);
    }

    // 关闭服务器套接字
    close(server_socket);

    return 0;
}
