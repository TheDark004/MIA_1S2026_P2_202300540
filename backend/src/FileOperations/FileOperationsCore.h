#ifndef FILEOPERATIONS_CORE_H
#define FILEOPERATIONS_CORE_H

#include <vector>
#include <string>
#include <fstream>
#include "../Structs/Structs.h"

namespace FileOperations
{

    // SplitPath — Divide un path en sus partes
    std::vector<std::string> SplitPath(const std::string &path);

    // IsDirectory — Verifica si un inodo es un directorio
    bool IsDirectory(const Inode &inode);

    // IsFile — Verifica si un inodo es un archivo
    bool IsFile(const Inode &inode);

    // FreeInodeBlocks — Libera bloques de un inodo
    bool FreeInodeBlocks(std::fstream &file, SuperBloque &sb, int partStart, int inodeNum, const Inode &inode);

    // RemoveDirEntry — Remueve una entrada de directorio
    bool RemoveDirEntry(std::fstream &file, SuperBloque &sb, int dirInodeNum, const std::string &name);

    // RemoveInodeRecursive — Remueve un inodo recursivamente
    bool RemoveInodeRecursive(std::fstream &file, SuperBloque &sb, int partStart, int inodeNum);

    // CopyInodeRecursive — Copia un inodo recursivamente
    bool CopyInodeRecursive(std::fstream &file, SuperBloque &sb, int partStart, int srcInodeNum, int destParentInodeNum, const std::string &name);

    // UpdateDirectoryParent — Actualiza el padre de un directorio
    bool UpdateDirectoryParent(std::fstream &file, const SuperBloque &sb, int dirInodeNum, int newParentInodeNum);

    // FindInDir — Busca una entrada en un directorio
    int FindInDir(std::fstream &file, const SuperBloque &sb, int dirInodeNum, const std::string &name);

    // AddEntryToDir — Agrega una entrada a un directorio
    bool AddEntryToDir(std::fstream &file, SuperBloque &sb, int partStart, int dirInodeNum, const std::string &name, int newInodeNum);

    // TraversePath — Navega el árbol de directorios
    int TraversePath(std::fstream &file, const SuperBloque &sb, const std::vector<std::string> &parts);

    // TraverseToParent — Navega hasta el directorio padre
    int TraverseToParent(std::fstream &file, const SuperBloque &sb, const std::vector<std::string> &parts);

    // NormalizePath — Normaliza un path
    std::string NormalizePath(const std::string &path);

    // IsInsidePath — Verifica si un path está dentro de otro
    bool IsInsidePath(const std::string &parent, const std::string &child);

}

#endif