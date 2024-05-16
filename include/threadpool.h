#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

#define MAX_TASKS 1000
#define BUFFER_SIZE 4096

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

void *worker(void *arg);
void requestprocess(int sockfd, struct Request request);

// 互斥锁结构体
typedef struct {
    pthread_mutex_t mutex;
} locker;

// 信号量结构体
typedef struct {
    pthread_mutex_t mutex;// 互斥锁成员变量，用于实现对共享资源的互斥访问。互斥锁用于保护临界区，确保在同一时间只有一个线程可以访问临界区，防止出现竞争条件
    pthread_cond_t cond; // 条件变量成员变量，用于实现线程间的条件同步。条件变量通常与互斥锁配合使用，当一个线程需要等待某个条件满足时，它会调用 pthread_cond_wait 函数来阻塞自己，同时释放互斥锁；当另一个线程满足了条件时，它会调用 pthread_cond_signal 或 pthread_cond_broadcast 函数来唤醒等待的线程
    int count;
} sem;

// 任务结构体
typedef struct Task {
    struct Task *next;                  // 指向下一个任务的指针
    //void (*process)(struct Task *task, void *data); // 任务处理函数指针
    int sockfd;
    struct Request request;                        // 任务请求
    // 具体任务数据
    // ...
} Task;

// 线程池结构体
typedef struct ThreadPool {
    int m_thread_number;                // 线程池中的线程数
    pthread_t *m_threads;               // 描述线程池的数组，其大小为m_thread_number
    Task *m_workqueue;                  // 请求队列
    Task *m_tail;                       // 请求队列尾部
    locker m_queuelocker;               // 保护请求队列的互斥锁
    sem m_queuestat;                    // 是否有任务需要处理的信号量
} ThreadPool;

// 初始化互斥锁
void locker_init(locker *lock) {
    pthread_mutex_init(&lock->mutex, NULL);
}

// 初始化信号量
void sem_init(sem *s, int count) {
    pthread_mutex_init(&s->mutex, NULL);
    pthread_cond_init(&s->cond, NULL);
    s->count = count;
}

// 等待信号量
void sem_wait(sem *s) {
    pthread_mutex_lock(&s->mutex);
    while (s->count <= 0) {
        pthread_cond_wait(&s->cond, &s->mutex);
    }
    s->count--;
    pthread_mutex_unlock(&s->mutex);
}

// 发送信号量
void sem_post(sem *s) {
    pthread_mutex_lock(&s->mutex);
    s->count++;
    pthread_cond_signal(&s->cond);
    pthread_mutex_unlock(&s->mutex);
}

// 创建线程池
ThreadPool *threadpool_create(int thread_number, int max_requests) {
    ThreadPool *pool = (ThreadPool *)malloc(sizeof(ThreadPool));
    if (!pool) {
        return NULL;
    }

    pool->m_thread_number = thread_number;
    pool->m_threads = (pthread_t *)malloc(sizeof(pthread_t) * thread_number);
    if (!pool->m_threads) {
        free(pool);
        return NULL;
    }

    pool->m_workqueue = NULL;
    pool->m_tail = NULL;

    locker_init(&pool->m_queuelocker);
    sem_init(&pool->m_queuestat, 0);

    for (int i = 0; i < thread_number; ++i) {
        if (pthread_create(&pool->m_threads[i], NULL, worker, (void *)pool) != 0) {
            free(pool->m_threads);
            free(pool);
            return NULL;
        }
        if (pthread_detach(pool->m_threads[i])) { // 分离一个线程。被分离的线程在终止的时候，会自动释放资源返回给系统
            free(pool->m_threads);
            free(pool);
            return NULL;
        }
    }

    return pool;
}

// 销毁线程池
void threadpool_destroy(ThreadPool *pool) {
    if (!pool) {
        return;
    }

    if (pool->m_threads) {
        free(pool->m_threads);
    }

    Task *tmp = NULL;
    while (pool->m_workqueue) {
        tmp = pool->m_workqueue;
        pool->m_workqueue = pool->m_workqueue->next;
        free(tmp);
    }

    free(pool);
}

// 添加任务到线程池
int threadpool_add(ThreadPool *pool, int sockfd, struct Request request) {
    Task *new_task = (Task *)malloc(sizeof(Task));
    if (!new_task) {
        return -1;
    }
    new_task->next = NULL;
    new_task->request = request;
    new_task->sockfd = sockfd;

    locker_init(&pool->m_queuelocker);// 初始化线程池中的互斥锁，用于保护对任务队列的访问
    if (!pool->m_workqueue) {//判断任务队列是否为空，如果为空，则将新任务设置为队列的第一个任务，并将 m_tail 指向新任务
        pool->m_workqueue = new_task;
        pool->m_tail = new_task;
    } else {//如果队列不为空，则将新任务添加到队列尾部，并更新 m_tail 指针
        pool->m_tail->next = new_task;
        pool->m_tail = new_task;
    }

    sem_post(&pool->m_queuestat); // 发送信号量，通知工作线程有新的任务可以执行

    return 0; // 返回 0，表示添加任务成功
}

// 线程函数
void *worker(void *arg) {
    ThreadPool *pool = (ThreadPool *)arg;
    while (1) {
        sem_wait(&pool->m_queuestat);
        locker_init(&pool->m_queuelocker);
        if (!pool->m_workqueue) {
            continue;
        }
        Task *task = pool->m_workqueue;
        pool->m_workqueue = pool->m_workqueue->next;
        sem_post(&pool->m_queuestat);
        if (!task) {
            continue;
        }
        requestprocess(task->sockfd, task->request);
        free(task);
    }
}
