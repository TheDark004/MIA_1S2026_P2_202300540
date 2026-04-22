#!/bin/bash
echo "Compilando ExtreamFS..."

g++ -std=c++17 \
    -Isrc \
    -I../external \
    -I/usr/include \
    src/main.cpp \
    src/Analyzer/Analyzer.cpp \
    src/DiskManagement/DiskManagement.cpp \
    src/FileSystem/FileSystem.cpp \
    src/UserSession/UserSession.cpp \
    src/FileOperations/FileOperationsCore.cpp \
    src/FileOperations/FileOperationsDir.cpp \
    src/FileOperations/FileOperationsFile.cpp \
    src/FileOperations/FileOperationsSearch.cpp \
    src/Utilities/Utilities.cpp \
    src/Reports/Reports.h  \
    src/Reports/Reports.cpp  \
    src/Information/Information.cpp \
    -lpthread \
    -o ExtreamFS

if [ $? -eq 0 ]; then
    echo "Listo ahora ejecuta ./ExtreamFS"
else
    echo "Error de compilación"
fi