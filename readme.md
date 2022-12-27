# 遇到的问题
## Segmentation fault (core dumped)
编译可以通过，分段错误，此时程序崩溃，由数组下标越界引起。
## 文件不存在
使用如下程序查看当前程序工作的路径，多半是文件的目录路径没对应上。
```c++
char *buffer;
if ((buffer = getcwd(nullptr, 0)) == nullptr) {
    perror("getcwd error"); 
} else {     
    printf("%s\n", buffer); 
}
```
## goto注意事项
在goto和跳转的语句之间，不能含有任何变量的定义
```c++
// 错误的写法
goto label;
int a[5];
label:
cout<<"error"<<endl;
```
```c++
int a[5]; // 要拿出来才行
goto label;
label:
cout<<"right"<<endl;
```
## 父进程中使用new开辟出来的内存与子进程中的是相互独立的
不要妄图通过这种方式完成进程之间的通信，这也就意味着全局变量什么的，在父子进程之间都是相互独立的，进程之间传递消息只有那几种方式！！！
另外，程序打印的都是虚拟内存的地址