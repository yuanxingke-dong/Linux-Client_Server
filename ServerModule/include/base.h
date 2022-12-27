//
// Created by 远行客 on 2022/12/15.
//

#ifndef LINUXSERVER_BASE_H
#define LINUXSERVER_BASE_H

#include <netinet/in.h> // htons, sockaddr_in

// 将主机字节序表示的IPv4地址和端口号转换为网络字节序
sockaddr_in addr_hton(const char *ip, const char *port);

// 以主机字节序打印网络字节序的IPv4地址和端口号
void print_network_ip_port(sockaddr_in &addr);

// 客户端建立与addr地址的TCP连接
int setup_connect(sockaddr_in &addr);

// 服务器监听addr端口的连接信息，最多允许(num+1)个监听队列，返回监听fd
int setup_listen(sockaddr_in &addr, int num, bool is_reuse = true); // is_reuse表示是否立即重用该addr，不必进入TIME_WAIT状态

// 服务器接受连接，返回接受连接的fd
int accept_connect(int sockfd, sockaddr_in &client);

// 将文件描述符设置为非阻塞
int setnonblocking(int fd);

// 往事件表epollfd上注册fd上的事件, 默认为服务器端
void addfd(int epollfd, int fd, bool isServer = true);

#endif //LINUXSERVER_BASE_H
