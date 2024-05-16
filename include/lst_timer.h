#ifndef TIMER_UTILS_H
#define TIMER_UTILS_H

#include <unistd.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <sys/stat.h>
#include <string.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sys/mman.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/wait.h>
#include <sys/uio.h>
#include <signal.h>
#include <time.h>

// 用户数据结构体
struct client_data
{
    struct sockaddr_in address;
    int sockfd;
    struct util_timer *timer;
};
struct client_data *users_timer; //定时器相关

// 定时器结构体
struct util_timer
{
    time_t expire;
    void (*cb_func)(struct client_data *);
    struct client_data *user_data;
    struct util_timer *prev;
    struct util_timer *next;
};

// 回调函数原型
void cb_func(struct client_data *user_data);

// 添加定时器
void add_timer(struct util_timer *timer);

void Add_timer(struct util_timer *timer, struct util_timer *lst_head);

// 调整定时器
void adjust_timer(struct util_timer *timer);

// 删除定时器
void del_timer(struct util_timer *timer);

// 定时器触发函数
void tick();

// 将文件描述符注册到内核事件表
void addfd(int epollfd, int fd, int one_shot);

// 信号处理函数
void sig_handler(int sig);

// 设置信号处理函数
void addsig(int sig, void(*handler)(int), int restart);

// 定时处理任务函数
void timer_handler();

// 显示错误信息并关闭连接
void show_error(int connfd, const char *info);

// 初始化定时器模块
void Utils_init(int timeslot);

int setnonblocking(int fd);

#endif
