cmake_minimum_required(VERSION 3.0)

set (This EchoServer)

set(Sources
    main.cpp
)
add_executable(${This} ${Sources})

target_include_directories(${This} PRIVATE ..)
target_link_libraries(${This} PUBLIC
    Sockets
)

add_test(
    NAME ${This}
    COMMAND ${This}
)