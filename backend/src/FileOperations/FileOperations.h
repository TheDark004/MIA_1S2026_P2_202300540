#ifndef FILEOPERATIONS_H
#define FILEOPERATIONS_H

#include "FileOperationsCore.h"
#include "FileOperationsDir.h"
#include "FileOperationsFile.h"
#include "FileOperationsSearch.h"

#endif
// CAT     -> muestra el contenido de un archivo

namespace FileOperations
{

    // ── Comandos principales ──────────────────────────────────────
    std::string Mkdir(const std::string &path, bool createParents);
    std::string Mkfile(const std::string &path, bool random, int size, const std::string &cont);
    std::string Cat(const std::string &filePath);
    std::string Remove(const std::string &path);
    std::string Rename(const std::string &path, const std::string &newName);
    std::string Copy(const std::string &source, const std::string &destination);
    std::string Move(const std::string &source, const std::string &destination);
    std::string Find(const std::string &path, const std::string &name);
    std::string Chmod(const std::string &path, const std::string &ugo);
    std::string Chown(const std::string &path, const std::string &usr, const std::string &grp);
    std::string Loss(const std::string &id);

    // Divide un path en partes
    // "/home/user/docs" -> ["home", "user", "docs"]
    std::vector<std::string> SplitPath(const std::string &path);

    // Busca una entrada por nombre dentro de un FolderBlock
    // Retorna el número de inodo o -1 si no existe
    int FindInDir(std::fstream &file, const SuperBloque &sb,
                  int dirInodeNum, const std::string &name);

    // Agrega una entrada (name -> inodeNum) a un directorio
    // Retorna true si tuvo éxito
    bool AddEntryToDir(std::fstream &file, SuperBloque &sb,
                       int partStart, int dirInodeNum,
                       const std::string &name, int newInodeNum);

    // Navega el arbol hasta el PADRE del path y retorna su inodo
    // path="/home/user/docs" -> retorna inodo de "/home/user"
    // Retorna -1 si el padre no existe
    int TraverseToParent(std::fstream &file, const SuperBloque &sb,
                         const std::vector<std::string> &parts);

    // Navega el árbol completo y retorna el inodo del path
    // Retorna -1 si no existe
    int TraversePath(std::fstream &file, const SuperBloque &sb,
                     const std::vector<std::string> &parts);

}
