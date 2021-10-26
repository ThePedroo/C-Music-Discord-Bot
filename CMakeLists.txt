cmake_minimum_required(VERSION 3.16)

project(OrcaBot VERSION 0.1)

add_executable(OrcaBot main.c)
add_definitions("-Wall -Wextra -Werror -pthread")

target_link_libraries(OrcaBot discord curl crypto pthread m sqlite3)
