//
// Created by 远行客 on 2022/12/24.
//

#include <ctime>
#include <iostream>

using namespace std;

void print_time() {
    // 基于当前系统的当前日期/时间
    time_t now = time(0);

    tm *ltm = localtime(&now);

    // 输出 tm 结构的各个组成部分
    cout << 1900 + ltm->tm_year << "年" << 1 + ltm->tm_mon << "月" << ltm->tm_mday << "日" << ltm->tm_hour << "时"
         << ltm->tm_min << "分" << ltm->tm_sec << "秒 " << flush;
}
