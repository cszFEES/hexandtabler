# Hexandtabler

Hexandtabler is a hexadecimal editor created with C++ and Qt, primarily aimed at translation purposes. Therefore, it supports relative character searches, relative character tables (`.tbl` files), and non-Roman fonts. It was created with multiplatform in mind.

## Requirements

Before compiling the project, make sure you have the following programs installed:

1. **Qt5 Libraries**: Necessary for the graphical interface.
2. **CMake**: Project configuration tool.
3. **g++**: C++ compiler.

## Debug

make 2>&1 | tee error_log.txt | xsel --clipboard