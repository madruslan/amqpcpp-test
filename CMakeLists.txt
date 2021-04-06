cmake_minimum_required(VERSION 3.17)
project(amqp)

set(CMAKE_CXX_STANDARD 17)

add_executable(${PROJECT_NAME} main.cpp)

target_include_directories(${PROJECT_NAME} PRIVATE  ${AMQPCPP_INCLUDE_DIR})

target_link_libraries(${PROJECT_NAME} amqpcpp ev pthread)
