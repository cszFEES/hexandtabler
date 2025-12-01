cmake_minimum_required(VERSION 3.5)

project(hexandtabler)

set(CMAKE_CXX_STANDARD 11)

# Enable automatic MOC handling for Q_OBJECT
set(CMAKE_AUTOMOC ON)

find_package(Qt5Widgets REQUIRED)

# Generate ui_hexandtabler.h from the .ui file
qt5_wrap_ui(UI_HEADERS hexandtabler.ui)

# Define the executable
add_executable(hexandtabler main.cpp hexandtabler.cpp hexeditorarea.cpp ${UI_HEADERS})

# Add the build directory to include paths to find ui_hexandtabler.h
target_include_directories(hexandtabler PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

# Add the source directory (where hexeditorarea.h is) to include paths
target_include_directories(hexandtabler PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# Link with Qt libraries
target_link_libraries(hexandtabler Qt5::Widgets)