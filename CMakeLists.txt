cmake_minimum_required(VERSION 3.16)

project(edge_relay C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(edge_relay
  src/main.c
  src/network.c
  src/protocol.c
  src/database.c
)

target_include_directories(edge_relay PRIVATE include)

target_link_libraries(edge_relay
  PRIVATE
  sqlite3
  cjson
)
