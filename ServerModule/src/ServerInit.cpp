//
// Created by 远行客 on 2022/12/25.
//

#include <map>
#include <string>
#include <fstream>
#include <sstream>
#include <iostream>

#include <sys/socket.h>
#include <sys/epoll.h>
#include <cstring>
#include <unistd.h>

#include "../include/base.h"

using namespace std;

void Init(const string &path, map<string, string> &user2passwd) {
    ifstream infile;
    infile.open(path, ios::in);
    if (!infile.is_open()) {
        cout << "path not exist." << endl;
        exit(1);
    }
    string line;
    string user, passwd;
    while (std::getline(infile, line)) {
        istringstream text(line);
        text >> user;
        text >> passwd;
        user2passwd[user] = passwd;
    }
}

bool judgeUser(int connfd, std::map<std::string, std::string> &user2passwd, string &right_user) {
    bool result = true; // 结果
    const char *info = "please input username and password split by empty number: ";
    send(connfd, info, strlen(info), 0);

    int child_epollfd = epoll_create(5); // 创造事件表
    addfd(child_epollfd, connfd);

    int event_number = 5;
    epoll_event events[event_number];

    int buffer_size = 128;
    char buffer[buffer_size];

    int number = epoll_wait(child_epollfd, events, event_number, -1);
    if ((number < 0) && (errno != EINTR)) {
        printf("epoll failure\n");
        result = false;
        goto label;
    }

    for (int i = 0; i < number; i++) {
        int sockfd = events[i].data.fd;
        if ((sockfd == connfd) && (events[i].events & EPOLLIN)) {
            memset(buffer, '\0', buffer_size);
            auto ret = recv(connfd, buffer, buffer_size - 1, 0);
            if (ret < 0) {
                if (errno != EAGAIN) {
                    result = false;
                    goto label;
                }
            } else if (ret == 0) {
                result = false;
                goto label;
            } else {
                // 在这里完成用户登陆数据的匹配
                istringstream text(buffer);
                string user, passwd;
                text >> user;
                text >> passwd;
                auto it = user2passwd.find(user);
                if (it != user2passwd.end() && it->second == passwd) {

                    const char *success = "Successful login.\n";
                    send(connfd, success, strlen(success), 0);

                    right_user = user;
                    result = true;
                    goto label;
                } else {
                    result = true;
                    goto label;
                }
            }
        }
    }

    label:
    // close(connfd); // 这里不能关闭，因为其引用计数没有增加，不然该子进程就被全关了
    close(child_epollfd);
    return result;
}