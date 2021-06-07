cmake_minimum_required (VERSION 2.6)
project (servo)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_MODULE_PATH ${servo_SOURCE_DIR}/../cmake/)
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -Wall -ggdb -std=c++17")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -Wall -std=c++17")

add_executable(servo Servo.cpp IPBlockItem.cpp EventMgr.cpp)
target_link_libraries (servo util pthread)
