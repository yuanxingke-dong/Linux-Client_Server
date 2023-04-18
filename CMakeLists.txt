cmake_minimum_required(VERSION 3.22)
project(Linux-Client&Server)

set(CMAKE_CXX_STANDARD 17)

add_executable(Server server.cpp ServerModule/include/base.h ServerModule/src/base.cpp ServerModule/include/mytime.h ServerModule/src/mytime.cpp ServerModule/include/ServerInit.h ServerModule/src/ServerInit.cpp)

add_executable(Client client.cpp ServerModule/include/base.h ServerModule/src/base.cpp ServerModule/include/mytime.h ServerModule/src/mytime.cpp ServerModule/include/base.h)
