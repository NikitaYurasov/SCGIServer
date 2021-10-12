cmake_minimum_required(VERSION 3.5)

project(SCGIServer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Threads REQUIRED)

add_executable(${PROJECT_NAME} server.h main.cpp)
target_link_libraries(${PROJECT_NAME} Threads::Threads)
