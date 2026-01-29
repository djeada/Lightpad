cmake_minimum_required(VERSION 3.10.0 FATAL_ERROR)

project(Lightpad VERSION 0.1 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Add the App subdirectory
add_subdirectory(App)
