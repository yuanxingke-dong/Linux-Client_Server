#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/epoll.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/uio.h> // writev
#include <map>
#include <iostream>

#include "ServerModule/include/base.h"
#include "ServerModule/include/mytime.h"
#include "ServerModule/include/ServerInit.h"

#define USER_LIMIT 5
#define BUFFER_SIZE 1024
#define MAX_EVENT_NUMBER 1024
#define PROCESS_LIMIT 65536

struct client_data {
    sockaddr_in address; // 客户端socket地址
    int connfd; // socket文件描述符
    pid_t pid; // 处理这个连接的子进程pid
    int pipefd[2]; // 和父进程通信用的管道
    char name[20]; // 该用户的昵称
    size_t name_length;
};

std::map<std::string, std::string> user2passwd;

int sig_pipefd[2];

// 信号处理函数
void sig_handler(int sig) {
    int save_errno = errno; // 保留原来的errno，在函数最后恢复，以保证信号的可重入性
    int msg = sig;
    send(sig_pipefd[1], (char *) &msg, 1, 0); // 将信号写入管道，以通知主循环
    errno = save_errno;
}

// 根据信号sig设置相应的信号处理函数
void addsig(int sig, void(*handler)(int), bool restart = true) {
    struct sigaction sa;
    memset(&sa, '\0', sizeof(sa)); // 将sa所在的内存都设为'\0'
    sa.sa_handler = handler; // 指定信号处理函数
    if (restart) {
        sa.sa_flags |= SA_RESTART; // 重新调用被该信号终止的系统调用
    }
    sigfillset(&sa.sa_mask); // 在信号集中设置所有信号
    assert(sigaction(sig, &sa, NULL) != -1); // sig要捕获的信号类型，sa指定信号处理方式
}

bool stop_child = false;

// 终止子进程的信号处理函数
void child_term_handler(int sig) {
    stop_child = true;
}

// 子进程主要运行的函数
// idx表示当前客户端连接的索引，share_mem为各进程共享内存的起始地址
int run_child(int idx, client_data *users, char *share_mem) {
    int child_epollfd = epoll_create(5); // 创造事件表
    assert(child_epollfd != -1);
    int connfd = users[idx].connfd;
    addfd(child_epollfd, connfd);
    int pipefd = users[idx].pipefd[1]; // 这个其实是双向管道，每一端都可读可写
    addfd(child_epollfd, pipefd);

    int ret;

    // SIGTERM终止进程，kill命令默认发送的信号就是SIGTERM
    addsig(SIGTERM, child_term_handler, false);

    epoll_event events[MAX_EVENT_NUMBER];

    // 登陆
    std::string username;
    if (judgeUser(connfd, user2passwd, username)) {
        username += ": ";
        memset(users[idx].name, '\0', 20);
        strcpy(users[idx].name, username.c_str());
        strcpy(share_mem + idx * BUFFER_SIZE, users[idx].name); // 只有通过将名字写入共享内存的方式，才可以让其余进程也读取
        users[idx].name_length = strlen(users[idx].name);
    } else {
        stop_child = true;
    }

    const char *oob_data = " | "; // 用来分割消息，以带外数据的形式发送

    // 当收到SIGTERM时，调用child_term_handler函数，以下循环就退出了
    while (!stop_child) {
        // timeout设为-1，表示运行到这会一直阻塞，直到有事件发生
        // 事件发生后，会将就绪的事件从内核事件表中复制到events指向的数组中
        int number = epoll_wait(child_epollfd, events, MAX_EVENT_NUMBER, -1);
        if ((number < 0) && (errno != EINTR)) {
            printf("epoll failure\n");
            break;
        }

        for (int i = 0; i < number; i++) {
            int sockfd = events[i].data.fd;
            if ((sockfd == connfd) && (events[i].events & EPOLLIN)) {
                // memset(share_mem + idx * BUFFER_SIZE + users[idx].name_length, '\0', BUFFER_SIZE); 这个第三个参数BUFFER_SIZE越界了
                memset(share_mem + idx * BUFFER_SIZE + users[idx].name_length, '\0',
                       BUFFER_SIZE - users[idx].name_length);
                ret = recv(connfd, share_mem + idx * BUFFER_SIZE + users[idx].name_length,
                           BUFFER_SIZE - users[idx].name_length - 1, 0);
                if (ret < 0) {
                    if (errno != EAGAIN) {
                        stop_child = true;
                    }
                } else if (ret == 0) {
                    stop_child = true;
                } else {
                    print_time();
                    std::cout << share_mem + idx * BUFFER_SIZE << std::flush;
                    // 通过管道pipefd告知父进程有数据需要读取，注意，这里写的只是该进程对应的idx
                    send(pipefd, (char *) &idx, sizeof(idx), 0);
                }
            } else if ((sockfd == pipefd) && (events[i].events & EPOLLIN)) { // 管道pipefd上有数据可读，这意味着有其他客户端发送了信息
                int client = 0; // 用来保存是哪个客户端，只是初始化为0
                // 读取sockfd上的数据，得知是哪个客户端在发消息
                ret = recv(sockfd, (char *) &client, sizeof(client), 0); // 此时client保存的就是发送了消息的客户端的索引
                if (ret < 0) {
                    if (errno != EAGAIN) {
                        stop_child = true;
                    }
                } else if (ret == 0) {
                    stop_child = true;
                } else {
//                    struct iovec iv[3];
//                    iv[0].iov_base = users[client].name; // 由于name变量是在子进程中确定的，所以所有子进程压根看不到彼此的name
//                    std::cout << "name: " << users[client].name << std::endl;
//                    iv[0].iov_len = strlen(users[client].name);
//                    iv[1].iov_base = (void *) oob_data;
//                    iv[1].iov_len = strlen(oob_data);
//                    iv[2].iov_base = share_mem + client * BUFFER_SIZE;
//                    iv[2].iov_len = strlen(share_mem + client * BUFFER_SIZE);
//                    writev(connfd, iv, 3);
//                    send(connfd, users[client].name, strlen(users[client].name), 0);
//                    send(connfd, oob_data, strlen(oob_data), 0);
//                    // 由于每个客户端固定了一块共享内存，就可以直接通过客户端的索引找到那块内存，再发出去
                    send(connfd, share_mem + client * BUFFER_SIZE, strlen(share_mem + client * BUFFER_SIZE), 0);
                }
            } else {
                continue;
            }
        }
    }

    close(connfd);
    close(pipefd);
    close(child_epollfd);
    return 0;
}

int main(int argc, char *argv[]) {
//    if (argc <= 2) {
//        printf("usage: %s ip_address port_number\n", basename(argv[0]));
//        return 1;
//    }
//    const char *ip = argv[1];
//    int port = atoi(argv[2]);

    const char *ip = "127.0.0.1";
    const char *port = "1234";

    int ret = 0;
    struct sockaddr_in address = addr_hton(ip, port);

    int listenfd = setup_listen(address, USER_LIMIT);

//    char *buffer;
//    if ((buffer = getcwd(nullptr, 0)) == nullptr) {
//        perror("getcwd error");
//    } else {
//        printf("%s\n", buffer);
//    }

    Init("../passwd.txt", user2passwd);

    int user_count = 0;

    auto *users = new client_data[USER_LIMIT + 1];

    std::map<int, int> sub_process; // 这样就不会数组越界了

    int epollfd = epoll_create(5); // 创建事件表
    assert(epollfd != -1);
    addfd(epollfd, listenfd); // 将监听fd注册到事件表里面

    ret = socketpair(PF_UNIX, SOCK_STREAM, 0, sig_pipefd); // 在本地创建双向管道
    assert(ret != -1);
    setnonblocking(sig_pipefd[1]); // 将双向管道的1端设为非阻塞
    addfd(epollfd, sig_pipefd[0]); // 将双向管道的0端加入事件表

    // 以下信号都会告知主进程
    addsig(SIGCHLD, sig_handler); // 子进程状态发生变化（退出或者暂停）
    addsig(SIGTERM, sig_handler); // 终止进程
    addsig(SIGINT, sig_handler); // 特指键盘输入以中断进程（Ctrl+C）

    // 忽略该条信号
    addsig(SIGPIPE, SIG_IGN); // 往读端被关闭的管道中写数据？不太明白 TODO

    static const char *shm_name = "/my_shm";
    int shmfd = shm_open(shm_name, O_CREAT | O_RDWR, 0666);
    assert(shmfd != -1);
    ret = ftruncate(shmfd, USER_LIMIT * BUFFER_SIZE);
    assert(ret != -1);

    char *share_mem = (char *) mmap(NULL, USER_LIMIT * BUFFER_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shmfd, 0);
    assert(share_mem != MAP_FAILED);
    close(shmfd);

    bool stop_server = false;
    bool terminate = false;
    epoll_event events[MAX_EVENT_NUMBER];

    while (!stop_server) {
        int number = epoll_wait(epollfd, events, MAX_EVENT_NUMBER, -1);
        if ((number < 0) && (errno != EINTR)) {
            printf("epoll failure\n");
            break;
        }

        for (int i = 0; i < number; i++) {
            int sockfd = events[i].data.fd;

            // 新的客户连接到来
            if (sockfd == listenfd) {
                struct sockaddr_in client_address; // 用于保存客户端的地址
                int connfd = accept_connect(listenfd, client_address); // 接受连接

                if (connfd < 0) {
                    printf("errno is: %d\n", errno);
                    continue;
                }
                if (user_count >= USER_LIMIT) {
                    const char *info = "too many users\n";
                    printf("%s", info);
                    send(connfd, info, strlen(info), 0);
                    close(connfd);
                    continue;
                }
                users[user_count].address = client_address;
                users[user_count].connfd = connfd;
                ret = socketpair(PF_UNIX, SOCK_STREAM, 0, users[user_count].pipefd); // 创建了双向管道
                assert(ret != -1);
                pid_t pid = fork(); // 创建了子进程
                if (pid < 0) {
                    close(connfd);
                    continue;
                } else if (pid == 0) { // 代表此时为子进程
                    // 由于会拷贝父进程中的文件描述符，所以以下文件描述符需要关闭
                    close(epollfd);
                    close(listenfd);
                    close(users[user_count].pipefd[0]); // 这个0端是给父进程使用的，所以也需要关闭
                    close(sig_pipefd[0]); // 关闭了传递信号的管道
                    close(sig_pipefd[1]);

                    run_child(user_count, users, share_mem); // 开始运行子进程的主要逻辑
                    munmap((void *) share_mem, USER_LIMIT * BUFFER_SIZE); // 退出上一个函数之后，需要释放该子进程拷贝的共享内存，也就是所有共享内存
                    exit(0);
                } else { // 代表为父进程
                    close(connfd); // connfd在子进程中使用，所以在父进程中关闭
                    close(users[user_count].pipefd[1]); // 同理
                    addfd(epollfd, users[user_count].pipefd[0]); // 需要添加父进程使用的管道到事件表中
                    users[user_count].pid = pid;
                    sub_process[pid] = user_count;
                    ++user_count; // 注意这里，该值表示的是一共有多少个客户端连接
                }
            }
                // 有异常发生，处理信号事件
            else if ((sockfd == sig_pipefd[0]) && (events[i].events & EPOLLIN)) {
                int sig;
                char signals[1024];
                ret = recv(sig_pipefd[0], signals, sizeof(signals), 0);
                if (ret == -1) {
                    continue;
                } else if (ret == 0) {
                    continue;
                } else {
                    for (int i = 0; i < ret; ++i) {
                        switch (signals[i]) {
                            case SIGCHLD: {
                                pid_t pid;
                                int stat;
                                while ((pid = waitpid(-1, &stat, WNOHANG)) > 0) { // 等待任意一个子进程结束，并返回其pid号
                                    int del_user = sub_process[pid];
                                    sub_process[pid] = -1;
                                    if ((del_user < 0) || (del_user > USER_LIMIT)) {
                                        printf("the deleted user was not change\n");
                                        continue;
                                    }
                                    epoll_ctl(epollfd, EPOLL_CTL_DEL, users[del_user].pipefd[0], 0); // 删除其在事件表中注册的事件
                                    close(users[del_user].pipefd[0]);

                                    // 由于user_count表示的是连接的客户端总数，所以自减一表示当前连接的客户端的最大索引值
                                    // 把最后一个数组的值赋给当前要删去的数组元素，即表示删除
                                    users[del_user] = users[--user_count];

                                    sub_process[users[del_user].pid] = del_user; // 修改对应的id
                                    printf("child %d exit, now we have %d users\n", del_user, user_count);
                                }
                                if (terminate && user_count == 0) {
                                    stop_server = true;
                                }
                                break;
                            }
                            case SIGTERM:
                            case SIGINT: {
                                printf("\nkill all the clild now\n");
                                //addsig( SIGTERM, SIG_IGN );
                                //addsig( SIGINT, SIG_IGN );
                                if (user_count == 0) {
                                    stop_server = true;
                                    break;
                                }
                                for (int i = 0; i < user_count; ++i) {
                                    int pid = users[i].pid;
                                    kill(pid, SIGTERM);
                                }
                                terminate = true;
                                break;
                            }
                            default: {
                                break;
                            }
                        }
                    }
                }
            }
                // 某个子进程向父进程写入了数据， 注意！这一个判断只能写在最后！！！
            else if (events[i].events & EPOLLIN) {
                int child = 0;
                ret = recv(sockfd, (char *) &child, sizeof(child), 0); // 是哪个子进程在写数据
                printf("read data from child accross pipe\n");
                if (ret <= 0) continue;
                else {
                    for (int j = 0; j < user_count; ++j) {
                        if (users[j].pipefd[0] != sockfd) {
                            printf("send data to child accross pipe\n");
                            // 将该子进程的idx发送给其他子进程
                            // 其他子进程就可以通过该idx找到他对应的共享内存范围，并读取之
                            send(users[j].pipefd[0], (char *) &child, sizeof(child), 0);
                        }
                    }
                }
            }
        }
    }

    close(sig_pipefd[0]);
    close(sig_pipefd[1]);
    close(listenfd);
    close(epollfd);
    shm_unlink(shm_name);
    delete[] users;
    sub_process.clear();

    return 0;
}
