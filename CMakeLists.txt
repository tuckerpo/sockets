cmake_minimum_required(VERSION 3.0)

set (This Sockets)

set(Headers
    include/INetConnection.hpp
    include/NetConnection.hpp
    include/NetServer.hpp
)

set (Sources

)

if (MSVC)
    list (APPEND Sources
        src/win/NetConnectionWin32.cpp
        src/win/NetServerWin32.cpp
        src/win/NetUtilsWin32.cpp
    )
endif()

add_subdirectory(googletest)

add_library(${This} STATIC ${Sources} ${Headers})
target_include_directories(${This} PUBLIC include)

add_subdirectory(test)
add_subdirectory(src/examples)
