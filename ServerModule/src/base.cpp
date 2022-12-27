//
// Created by 远行客 on 2022/12/15.
//

#include <netinet/in.h> // htons, sockaddr_in
#include <arpa/inet.h> // inet_pton
#include <string.h> // basename
#include <stdlib.h> // atoi
#include <iostream>

#include <sys/socket.h> // socket, bind, listen, accept, connect, shutdown, send, recv, sendto, recvfrom
#include <assert.h> // assert
#include <fcntl.h> // fcntl, O_NONBLOCK, F_GETFL, ……
#include <sys/epoll.h> // epoll_event, epoll_ctl

using namespace std;

sockaddr_in addr_hton(const char *ip, const char *port) {
    struct sockaddr_in address{}; // 声明网络地址
    bzero(&address, sizeof(address)); // 置零

    // 确定协议类型
    address.sin_family = AF_INET;
    // 讲字符串的ip地址转换为网络字节序
    inet_pton(AF_INET, ip, &address.sin_addr);
    // 先将字符串的端口号转换为主机字节序，然后讲主机字节序转换为网络字节序
    address.sin_port = htons(atoi(port));
    return address;
}

void print_network_ip_port(sockaddr_in &addr) {
    // INET_ADDRSTRLEN 代表IPv4的地址长度
    char remote[INET_ADDRSTRLEN];
    printf("connected with ip: %s and port: %d\n", inet_ntop(AF_INET, &addr.sin_addr, remote, INET_ADDRSTRLEN),
           ntohs(addr.sin_port));
}

int setup_connect(sockaddr_in &addr) {
    int sockfd = socket(PF_INET, SOCK_STREAM, 0);
    assert(sockfd >= 0);

    int ret = connect(sockfd, (sockaddr *) &addr, sizeof(addr));
    assert(ret >= 0);

    return sockfd;
}

int setup_listen(sockaddr_in &addr, int num, bool is_reuse) {
    int sockfd = socket(PF_INET, SOCK_STREAM, 0);
    assert(sockfd >= 0);
    if (is_reuse) {
        int reuse = 1;
        setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
    }

    int ret = 0;

    // 绑定连接
    ret = bind(sockfd, (sockaddr *) &addr, sizeof(addr));
    assert(ret >= 0);

    // 监听连接，num为监听队列的长度，里面的连接处于完整连接状态
    ret = listen(sockfd, num);
    assert(ret >= 0);

    return sockfd;
}

int accept_connect(int sockfd, sockaddr_in &client) {
    socklen_t client_addrlength = sizeof(client);

    int connfd = accept(sockfd, (sockaddr *) &client, &client_addrlength);
    assert(connfd >= 0);

    return connfd;
}

int setnonblocking(int fd) {
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
    return old_option;
}

void addfd(int epollfd, int fd, bool isServer) {
    epoll_event event;
    event.data.fd = fd;
    event.events = isServer ? EPOLLIN | EPOLLET : EPOLLIN | EPOLLRDHUP | EPOLLET; // 注册可读事件，为边缘触发模式
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
    setnonblocking(fd);
}