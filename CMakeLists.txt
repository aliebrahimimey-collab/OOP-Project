cmake_minimum_required(VERSION 3.20)
project(OOP_Project)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories("C:/MinGW/SDL2-2.28.5/i686-w64-mingw32/include")
link_directories("C:/MinGW/SDL2-2.28.5/i686-w64-mingw32/lib")

add_executable(OOP_Project main.cpp)

# کپی کردن فونت در صورت وجود داشتن
if(EXISTS "${CMAKE_SOURCE_DIR}/font.ttf")
    file(COPY ${CMAKE_SOURCE_DIR}/font.ttf DESTINATION ${CMAKE_BINARY_DIR})
endif()

target_link_libraries(OOP_Project mingw32 SDL2main SDL2 SDL2_ttf)