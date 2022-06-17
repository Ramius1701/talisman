cmake_minimum_required (VERSION 3.0)
project (trinket)


set(CMAKE_CXX_STANDARD 17)
set(CMAKE_MODULE_PATH ${trinket_SOURCE_DIR}/../cmake/)
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -Wall -ggdb -std=c++17")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -Wall -std=c++17")

find_package(SQLite3 REQUIRED)
find_package(OpenSSL REQUIRED)

include_directories(${SQLITE3_INCLUDE_DIRS} ${OPENSSL_INCLUDE_DIR} ../Common)

add_executable(trinket Trinket.cpp)
if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
    target_link_libraries (trinket ${SQLITE3_LIBRARIES} ${OPENSSL_CRYPTO_LIBRARY})
else()
    target_link_libraries (trinket ${SQLITE3_LIBRARIES} ${OPENSSL_CRYPTO_LIBRARY})
endif()
