cmake_minimum_required(VERSION 3.5)

project(hexandtabler)

set(CMAKE_CXX_STANDARD 11)

# Habilitar el MOC automático para que maneje Q_OBJECT 
set(CMAKE_AUTOMOC ON)

find_package(Qt5Widgets REQUIRED)

# Genera ui_hexandtabler.h a partir del archivo .ui
qt5_wrap_ui(UI_HEADERS hexandtabler.ui)

# Definir el ejecutable
# ¡CORRECCIÓN CLAVE! Añadir hexeditorarea.cpp para que el enlazador encuentre las implementaciones.
add_executable(hexandtabler main.cpp hexandtabler.cpp hexeditorarea.cpp ${UI_HEADERS})

# Agregar el directorio de compilación (build directory) a las rutas de inclusión para encontrar ui_hexandtabler.h
target_include_directories(hexandtabler PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

# Añadir el directorio fuente (donde está hexeditorarea.h) a las rutas de inclusión.
target_include_directories(hexandtabler PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# Enlazar con las librerías de Qt
target_link_libraries(hexandtabler Qt5::Widgets)