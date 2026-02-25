cmake_minimum_required(VERSION 3.20)
project(hexandtabler)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 COMPONENTS Widgets Concurrent REQUIRED)

add_executable(hexandtabler 
    main.cpp 
    hexandtabler.cpp 
    hexeditorarea.cpp
    hexandtabler.ui
)

target_link_libraries(hexandtabler PRIVATE 
    Qt6::Widgets 
    Qt6::Concurrent
)