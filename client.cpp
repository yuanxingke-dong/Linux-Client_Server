#define _GNU_SOURCE 1

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <errno.h>

#include <iostream>

#include "ServerModule/include/base.h"
#include "ServerModule/include/mytime.h"

#define BUFFER_SIZE 512
#define MAX_EVENT_NUMBER 8

int main(int argc, char *argv[]) {
//    if (argc <= 2) {
//        printf("usage: %s ip_address port_number\n", basename(argv[0]));
//        return 1;
//    }
//    const char *ip = argv[1];
//    int port = atoi(argv[2]);

    const char *ip = "127.0.0.1";
    const char *port = "1234";

    struct sockaddr_in server_address = addr_hton(ip, port);

    int sockfd = setup_connect(server_address);

    int epollfd = epoll_create(5);

    epoll_event event{};

    event.data.fd = 0; // 标准输入
    event.events = EPOLLIN | EPOLLET;
    epoll_ctl(epollfd, EPOLL_CTL_ADD, 0, &event);

    event.data.fd = sockfd;
    event.events = EPOLLIN | EPOLLRDHUP | EPOLLET;
    epoll_ctl(epollfd, EPOLL_CTL_ADD, sockfd, &event);

    int pipefd[2];
    int ret = pipe(pipefd);
    assert(ret != -1);

    bool client_stop = false;
    char read_buf[BUFFER_SIZE];
    epoll_event events[MAX_EVENT_NUMBER];

    // 初次连接
    int _number = epoll_wait(epollfd, events, MAX_EVENT_NUMBER, -1);
    if ((_number < 0) && (errno != EINTR)) {
        printf("epoll failure\n");
        exit(1);
    }
    for (int i = 0; i < _number; ++i) {
        int fd = events[i].data.fd;
        if (fd == sockfd) { // 若是为连接fd
            if (events[i].events & EPOLLRDHUP) {
                printf("server close the connection\n");
                client_stop = true;
            } else if (events[i].events & EPOLLIN) {
                memset(read_buf, '\0', BUFFER_SIZE);
                recv(fd, read_buf, BUFFER_SIZE - 1, 0);
                // printf("%s", read_buf);
                std::cout << read_buf << std::flush; // 这一句不会堵塞
            }
        } else if ((fd == 0) && (events[i].events & EPOLLIN)) {
            // 从命令行发送数据
            ret = splice(0, NULL, pipefd[1], NULL, BUFFER_SIZE, SPLICE_F_MORE | SPLICE_F_MOVE);
            ret = splice(pipefd[0], NULL, sockfd, NULL, BUFFER_SIZE, SPLICE_F_MORE | SPLICE_F_MOVE);
        }
    }

    while (!client_stop) {
        int number = epoll_wait(epollfd, events, MAX_EVENT_NUMBER, -1);
        if ((number < 0) && (errno != EINTR)) {
            printf("epoll failure\n");
            break;
        }

        for (int i = 0; i < number; ++i) {
            int fd = events[i].data.fd;

            if (fd == sockfd) { // 若是为连接fd
                if (events[i].events & EPOLLRDHUP) {
                    printf("server close the connection\n");
                    client_stop = true;
                } else if (events[i].events & EPOLLIN) {
                    print_time();

                    memset(read_buf, '\0', BUFFER_SIZE);
                    recv(fd, read_buf, BUFFER_SIZE - 1, 0);
                    std::cout << read_buf << " " << std::flush;
                }
            } else if ((fd == 0) && (events[i].events & EPOLLIN)) {
                ret = splice(0, NULL, pipefd[1], NULL, BUFFER_SIZE, SPLICE_F_MORE | SPLICE_F_MOVE);
                ret = splice(pipefd[0], NULL, sockfd, NULL, BUFFER_SIZE, SPLICE_F_MORE | SPLICE_F_MOVE);
            }
        }
    }

    close(sockfd);
    return 0;
}
