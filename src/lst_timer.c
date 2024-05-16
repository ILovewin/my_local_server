#include "lst_timer.h"

struct util_timer *head;
struct util_timer *tail;
const int TIMESLOT = 7;       //最小超时单位
int *u_pipefd = 0;
int u_epollfd = 0;
extern int user_count;

int setnonblocking(int fd)
{
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
    return old_option;
}

void add_timer(struct util_timer *timer)
{
    if (!timer)
        return;

    if (!head)
    {
        head = tail = timer;
        return;
    }

    if (timer->expire < head->expire)
    {
        timer->next = head;
        head->prev = timer;
        head = timer;
        return;
    }

    struct util_timer *tmp = head->next;
    struct util_timer *prev = head;

    while (tmp)
    {
        if (timer->expire < tmp->expire)
        {
            prev->next = timer;
            timer->next = tmp;
            tmp->prev = timer;
            timer->prev = prev;
            break;
        }
        prev = tmp;
        tmp = tmp->next;
    }

    if (!tmp)
    {
        prev->next = timer;
        timer->prev = prev;
        timer->next = NULL;
        tail = timer;
    }
}

void Add_timer(struct util_timer *timer, struct util_timer *lst_head)
{
    struct util_timer *prev = lst_head;
    struct util_timer *tmp = prev->next;
    while (tmp)
    {
        if (timer->expire < tmp->expire)
        {
            prev->next = timer;
            timer->next = tmp;
            tmp->prev = timer;
            timer->prev = prev;
            break;
        }
        prev = tmp;
        tmp = tmp->next;
    }
    if (!tmp)
    {
        prev->next = timer;
        timer->prev = prev;
        timer->next = NULL;
        tail = timer;
    }
}


void adjust_timer(struct util_timer *timer)
{
    if (!timer)
    {
        return;
    }

    struct util_timer *tmp = timer->next;

    // 如果定时器已经是最后一个或者仍然小于下一个定时器的超时时间，则不需要调整
    if (!tmp || (timer->expire < tmp->expire))
    {
        return;
    }

    // 如果定时器是头部节点
    if (timer == head)
    {
        head = head->next;
        if (head)
        {
            head->prev = NULL;
        }
        timer->next = NULL;
        Add_timer(timer, head);
    }
    else
    {
        // 从链表中移除该定时器
        if (timer->prev)
        {
            timer->prev->next = timer->next;
        }
        if (timer->next)
        {
            timer->next->prev = timer->prev;
        }

        // 将定时器插入新的位置
        Add_timer(timer, timer->next);
    }
}

void del_timer(struct util_timer *timer)
{
    if (!timer)
        return;

    if (timer == head && timer == tail)
    {
        free(timer);
        head = NULL;
        tail = NULL;
        return;
    }

    if (timer == head)
    {
        head = head->next;
        head->prev = NULL;
        free(timer);
        return;
    }

    if (timer == tail)
    {
        tail = tail->prev;
        tail->next = NULL;
        free(timer);
        return;
    }

    timer->prev->next = timer->next;
    timer->next->prev = timer->prev;
    free(timer);
}

void tick()
{
    if (!head)
        return;

    time_t cur = time(NULL);
    struct util_timer *tmp = head;

    while (tmp)
    {
        if (cur < tmp->expire)
            break;

        tmp->cb_func(tmp->user_data);
        head = tmp->next;

        if (head)
            head->prev = NULL;

        free(tmp);
        tmp = head;
    }
}

void addfd(int epollfd, int fd, int one_shot)
{
    struct epoll_event event;
    event.data.fd = fd;
    event.events = EPOLLIN | EPOLLRDHUP;

    //对于注册了EPOLLONESHOT事件的文件描述符，操作系统最多触发其上注册的一个事件，且只触发一次，除非我们使用epoll_ctl函数重置该文件描述符上注册的EPOLLONESHOT事件。这样，在一个线程使用socket时，其他线程无法操作socket。同样，只有在该socket被处理完后，须立即重置该socket的EPOLLONESHOT事件，以确保这个socket在下次可读时，其EPOLLIN事件能够被触发，进而让其他线程有机会操作这个socket。
    if (one_shot)
        event.events |= EPOLLONESHOT;

    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
}

void sig_handler(int sig)
{
    int save_errno = errno;
    int msg = sig;
    send(u_pipefd[1], (char *)&msg, 1, 0);
    errno = save_errno;
}

void addsig(int sig, void (*handler)(int), int restart) {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handler;

    if (restart)
        sa.sa_flags |= ERESTART;

    sigfillset(&sa.sa_mask);
    assert(sigaction(sig, &sa, NULL) != -1);
}

void timer_handler()
{
    tick();
    alarm(TIMESLOT);
}

void show_error(int connfd, const char *info)
{
    send(connfd, info, strlen(info), 0);
    close(connfd);
}

void cb_func(struct client_data *user_data)
{
    epoll_ctl(u_epollfd, EPOLL_CTL_DEL, user_data->sockfd, 0);
    assert(user_data);
    close(user_data->sockfd);
    user_count--;
    printf("remove sockfd : %d\n", user_data->sockfd);
}

