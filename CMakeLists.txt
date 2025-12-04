cmake_minimum_required(VERSION 3.15) # Actualizado para quitar la advertencia
project(hexandtabler)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
find_package(Qt5Widgets REQUIRED)

qt5_wrap_ui(UI_HEADERS hexandtabler.ui)
add_executable(hexandtabler 
    main.cpp 
    hexandtabler.cpp 
    hexeditorarea.cpp 
    ${UI_HEADERS}
)

target_include_directories(hexandtabler PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
target_include_directories(hexandtabler PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(hexandtabler Qt5::Widgets)
install(TARGETS hexandtabler
    RUNTIME DESTINATION bin
)