#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <signal.h>
#include "locker.h"
#include "threadpool.h"
#include "http_conn.h"

#define MAX_FD 65535 // 最大的文件描述符个数
#define MAX_EVENT_NUMBER 10000 // epoll监听的最大事件数量

// 添加信号捕捉
void addsig(int sig, void (handler)(int)) {
    struct sigaction sa;
    memset(&sa, '\0', sizeof(sa));
    sa.sa_handler = handler;
    sigfillset(&sa.sa_mask);
    sigaction(sig, &sa, NULL);
}

// 添加文件描述符到epoll中
extern void addfd(int epollfd, int fd, bool one_shot);
// 从epoll中删除文件描述符
extern void removefd(int epollfd, int fd);
// 修改epoll文件描述符, 重置socket上EPOLLONESHOT事件，以确保下一次可读时，EPOLLIN事件能被触发
extern void modfd(int epollfd, int fd, int ev);

int main(int argc, char *argv[]) {
    if (argc <= 1) {
        printf("按照如下格式运行： %s port_number\n", basename(argv[0]));
        exit(-1);
    }

    // 获取端口号
    int port = atoi(argv[1]);

    // 对 SIGPIPE 信号进行处理
    addsig(SIGPIPE, SIG_IGN);

    // 创建线程池，初始化线程池
    threadpool<http_conn> *pool = NULL;
    try {
        pool = new threadpool<http_conn>;
    } catch(...) {
        exit(-1);
    }

    // 创建一个数组用于保存所有的客户端信息
    http_conn *users = new http_conn[MAX_FD];

    // 创建监听的套接字
    int lfd = socket(PF_INET, SOCK_STREAM, 0);
    if (lfd == -1) {
        perror("socket");
        exit(-1);
    }

    // 设置端口复用
    int op = 1;
    setsockopt(lfd, SOL_SOCKET, SO_REUSEADDR, &op, sizeof(op));

    // 绑定
    struct sockaddr_in saddr;
    saddr.sin_family = AF_INET;
    saddr.sin_addr.s_addr = INADDR_ANY; // 0.0.0.0
    saddr.sin_port = htons(port);
    int ret = bind(lfd, (struct sockaddr *)&saddr, sizeof(saddr));
    if (ret == -1) {
        perror("bind");
        exit(-1);
    }

    // 监听
    ret = listen(lfd, 5);
    if (ret == -1) {
        perror("listen");
        exit(-1);
    }

    // 创建epoll对象
    int epollfd = epoll_create(5);
    struct epoll_event epevs[MAX_EVENT_NUMBER];

    // 将监听的文件描述符添加到epoll对象中
    addfd(epollfd, lfd, false);

    http_conn::m_epollfd = epollfd;

    while (1) {
        ret = epoll_wait(epollfd, epevs, MAX_EVENT_NUMBER, -1);
        if ((ret == -1) && (errno != EINTR)) {
            perror("epoll_wait");
            break;
        }

        for (int i = 0; i < ret; ++i) {
            int sockfd = epevs[i].data.fd;
            if (sockfd == lfd) {
                struct sockaddr_in clientaddr;
                socklen_t len = sizeof(clientaddr);
                int connfd = accept(lfd, (struct sockaddr *)&clientaddr, &len);

                if (http_conn::m_user_count >= MAX_FD) {
                    // 目前连接数满了
                    // 给客户端写一个http回复报文信息，服务器正在忙
                    close(connfd);
                    continue;
                }

                // 将新的客户的数据初始化，放到数组中
                users[connfd].init(connfd, clientaddr);
            }
            else if (epevs[i].events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR)) {
                // 对方异常断开或者错误等事件
                users[sockfd].close_conn();
            }
            else if (epevs[i].events & EPOLLIN) {
                if (users[sockfd].read()) {
                    // 一次性把所有数据都读完了
                    pool->append(users + sockfd);
                }
                else {
                    users[sockfd].close_conn();
                }
            }
            else if (epevs[i].events & EPOLLOUT) {
                if (!users[sockfd].write()) {
                    users[sockfd].close_conn();
                }
            }
        }
    }

    close(epollfd);
    close(lfd);
    delete [] users;
    delete pool;

    return 0;
}
