cmake_minimum_required(VERSION 3.16)
# Set PROJECT_VER so ESP-IDF skips git-describe (repo has no commits yet).
set(PROJECT_VER "0.1.0")
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(yuktrivia)
