#include "server.h"

extern const int TIMESLOT;       //最小超时单位
extern int *u_pipefd;
extern int u_epollfd;

// 创建服务器套接字
int create_server_socket() {
    int server_socket;
    struct sockaddr_in server_address;

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

    return server_socket;
}


// 监听
void eventlisten(int server_socket) {
    epollfd = epoll_create(5);
    assert(epollfd != -1);

    addfd(epollfd, server_socket, 0); // 将监听套接字 server_socket 添加到 epoll 实例中，以便监听连接请求

    int ret = 0;
    ret = socketpair(PF_UNIX, SOCK_STREAM, 0, pipefd); // 创建一个全双工的管道，其中 pipefd 数组保存了两个文件描述符，分别用于读和写。通过管道实现进程间通信。
    assert(ret != -1);
    setnonblocking(pipefd[1]); // 将管道写端设置为非阻塞模式，以便在写入管道时可以立即返回。
    addfd(epollfd, pipefd[0], 0); // 将管道读端添加到 epoll 实例中，以便监听管道的读事件。

    addsig(SIGPIPE, SIG_IGN, 0); // 忽略 SIGPIPE 信号，避免因为写入已关闭的管道而导致进程终止。
    // 为 SIGALRM 和 SIGTERM 信号注册信号处理函数。
    addsig(SIGALRM, sig_handler, 0);
    addsig(SIGTERM, sig_handler, 0);

    alarm(TIMESLOT); // 设置一个周期性定时器，每隔 TIMESLOT 秒触发一次 SIGALRM 信号

    u_epollfd = epollfd;
    u_pipefd = pipefd;
}

void eventLoop(int server_socket)
{
    bool timeout = false;
    bool stop_server = false;
    // epoll创建内核事件表
    struct epoll_event event[MAX_EVENTS];
    event->events = EPOLLIN | EPOLLRDHUP;


    while (!stop_server)
    {
        int number = epoll_wait(u_epollfd, event, MAX_EVENTS, -1);
        if (number < 0 && errno != EINTR)//如果 number 小于 0 且错误码不是 EINTR（表示被中断），则说明发生了错误。
        {
            printf("epoll failure\n");
            break;
        }

        for (int i = 0; i < number; i++)
        {
            int sockfd = event[i].data.fd;

            //处理新到的客户连接
            if (sockfd == server_socket)
            {
                bool flag = dealclientconnet(server_socket);
                if (false == flag)
                    continue;
            }
            else if (event[i].events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR))
            {
                // 处理断开连接或错误事件，移除对应的定时器
                struct util_timer *timer = users_timer[sockfd].timer;
                deal_timer(timer, sockfd);
                printf("Client disconnected or encountered an error\n");
                user_count--;
                close(sockfd);
            }
            //处理信号
            else if ((sockfd == u_pipefd[0]) && (event[i].events & EPOLLIN))
            {
                bool flag = dealwithsignal(&timeout, &stop_server);
                if (false == flag)
                    printf("dealclientdata failure\n");
            }
            //处理客户连接上接收到的数据
            else if (event[i].events & EPOLLIN)
            {
                dealwithread(sockfd);
            }
        }
        if (timeout)
        {
            timer_handler();

            printf("timer tick\n");

            timeout = false;
        }
    }
}

//若有数据传输，则将定时器往后延迟3个单位
//并对新的定时器在链表上的位置进行调整
void adjust(struct util_timer *timer)
{
    time_t cur = time(NULL);
    timer->expire = cur + 5 * TIMESLOT;
    adjust_timer(timer);

    printf("adjust timer once\n");
}

void timer(int connfd, struct sockaddr_in client_address)
{
    addfd(u_epollfd, connfd, 0);
    user_count++;
    
    //初始化client_data数据
    //创建定时器，设置回调函数和超时时间，绑定用户数据，将定时器添加到链表中
    users_timer[connfd].address = client_address;
    users_timer[connfd].sockfd = connfd;
    // 创建一个util_timer对象并分配内存
    struct util_timer *timer = (struct util_timer *)malloc(sizeof(struct util_timer));
    if (timer == NULL) {
        // 处理内存分配失败的情况
        printf("Memory allocation failed\n");
    }
    timer->user_data = &users_timer[connfd];
    timer->cb_func = cb_func;
    time_t cur = time(NULL);
    timer->expire = cur + 3 * TIMESLOT;
    users_timer[connfd].timer = timer;
    add_timer(timer);
}

bool dealclientconnet(int server_socket)
{
    struct sockaddr_in client_address;
    socklen_t client_addrlength = sizeof(client_address);
    int connfd = accept(server_socket, (struct sockaddr *)&client_address, &client_addrlength);
    if (connfd < 0)
    {
        printf("%s: errno is: %d\n", "accept error", errno);
        return false;
    }
    if (user_count >= MAX_FD)
    {
        printf(connfd, "Internal server busy\n");
        return false;
    }
    printf("The connfd : %d is connect\n", connfd);
    timer(connfd, client_address);
    return true;
}

void deal_timer(struct util_timer *timer, int sockfd)
{
    timer->cb_func(&users_timer[sockfd]);
    if (timer)
    {
        del_timer(timer);
    }

    printf("close fd %d\n", users_timer[sockfd].sockfd);
}

bool dealwithsignal(bool *timeout, bool *stop_server)
{
    int ret = 0;
    int sig;
    char signals[1024];
    ret = recv(u_pipefd[0], signals, sizeof(signals), 0);
    if (ret == -1)
    {
        return false;
    }
    else if (ret == 0)
    {
        return false;
    }
    else
    {
        for (int i = 0; i < ret; ++i)
        {
            switch (signals[i])
            {
            case SIGALRM:
            {
                *timeout = true;
                break;
            }
            case SIGTERM:
            {
                *stop_server = true;
                break;
            }
            }
        }
    }
    return true;
}

void dealwithread(int sockfd)
{
    struct util_timer *timer = users_timer[sockfd].timer;
    adjust(timer);
    // 处理读取事件
    struct Request request;
    ssize_t bytes_received = recv(sockfd, &request, sizeof(struct Request), 0);
    if (bytes_received <= 0) {
        // 客户端关闭连接或发生错误
        printf("Client disconnected\n");
        struct util_timer *timer = users_timer[sockfd].timer;
        deal_timer(timer, sockfd);
        close(sockfd);
    }
    else
    {
        threadpool_add(pool, sockfd, request);
    }
}

void requestprocess(int sockfd, struct Request request)
{
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
                send_data_to_client(sockfd, output);
                free(output);
            }
                // 处理 cd命令
            else if (strcmp(token, "cd") == 0){
                char *token = strtok(NULL, " "); // 获取目录参数
                if (token == NULL) {
                    // 如果没有提供目录参数，则发送错误消息给客户端
                    const char *error_message = "Invalid directory";
                    send_data_to_client(sockfd, error_message);
                } else {
                    // 执行 chdir 函数切换目录
                    if (chdir(token) == 0) {
                        // 切换目录成功
                        const char *message = "Directory changed successfully";
                        send_data_to_client(sockfd, message);
                    } else {
                        // 切换目录失败
                        const char *error_message = "Failed to change directory";
                        send_data_to_client(sockfd, error_message);
                    }
                }
            }
            else {
                // 处理 mkdir命令       
                if (system(request.data) == 0) {
                    // 命令执行成功
                    const char *message = "Directory created successfully";
                    send_data_to_client(sockfd, message);
                } else {
                    // 命令执行失败
                    const char *message = "Failed to create directory";
                    send_data_to_client(sockfd, message);
                }
            }
            break;
        case SEND_FILE:
            // 处理发送文件请求
            printf("Client requests file: %s\n", request.data);
            if(token != NULL) {
                token = strtok(NULL, " ");
                printf("token: %s\n", token);
                handle_send_file_request(sockfd, token);
                const char *end_message = "END_OF_FILE";
                send_data_to_client(sockfd, end_message);
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
                handle_receive_file_request(sockfd, filename);
            }
            break;
        default:
            printf("Unknown request type from client\n");
            break;
    }
}

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
            fseek(file, -strlen("END_OF_FILE"), SEEK_END);  // 将文件指针移动到距离文件末尾 "END_OF_FILE" 字符串长度的位置
            int trunc_result = ftruncate(fileno(file), ftell(file)); // 将文件截断到当前文件指针的位置
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

int main() {
    int server_socket;
    users_timer = (struct client_data *)malloc(MAX_FD * sizeof(struct client_data));
    if (users_timer == NULL) {
        // 处理内存分配失败的情况
        printf("Memory allocation failed\n");
    }
    // 创建服务器套接字并获取文件描述符
    server_socket = create_server_socket();

    // 创建线程池
    pool = threadpool_create(8, THREAD_POOL_SIZE);

    //监听
    eventlisten(server_socket);

    eventLoop(server_socket);
    
    // 销毁 epoll 实例
    close(u_epollfd);

    // 关闭服务器套接字
    close(server_socket);

    threadpool_destroy(pool);

    free(users_timer);

    return 0;
}
