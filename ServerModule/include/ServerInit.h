//
// Created by 远行客 on 2022/12/25.
//

#ifndef LINUX_CLIENT_SERVER_SERVERINIT_H
#define LINUX_CLIENT_SERVER_SERVERINIT_H

#include <map>
#include <string>

// 服务器的初始化
// 先完成读取账号密码的功能
void Init(const std::string &path, std::map<std::string, std::string> &user2passwd);

// 判断用户
bool judgeUser(int connfd, std::map<std::string, std::string> &user2passwd, std::string &right_user);

#endif //LINUX_CLIENT_SERVER_SERVERINIT_H
