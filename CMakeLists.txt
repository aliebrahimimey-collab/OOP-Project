cmake_minimum_required(VERSION 3.25)
project(OOP_Project)

set(CMAKE_CXX_STANDARD 17)

add_executable(OOP_Project main.cpp)

# پیدا کردن کتابخانه SDL2 روی سیستم
find_package(SDL2 REQUIRED)

# لینک کردن SDL2 به فایل اجرایی پروژه (نام پروژه پیش‌فرض شما معمولاً نام پوشه است)
target_link_libraries(${PROJECT_NAME} PRIVATE SDL2::SDL2)
