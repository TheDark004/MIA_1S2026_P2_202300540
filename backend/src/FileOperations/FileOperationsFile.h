#ifndef FILEOPERATIONS_FILE_H
#define FILEOPERATIONS_FILE_H

#include <string>

namespace FileOperations
{

    // MKFILE — Crea un archivo en el sistema de archivos
    std::string Mkfile(const std::string &path, bool random, int size, const std::string &cont);

    // CAT — Muestra el contenido de un archivo
    std::string Cat(const std::string &filePath);

    // REMOVE — Elimina un archivo o directorio
    std::string Remove(const std::string &path);

    // RENAME — Renombra un archivo o directorio
    std::string Rename(const std::string &path, const std::string &newName);

    // COPY — Copia un archivo o directorio
    std::string Copy(const std::string &source, const std::string &destination);

    // MOVE — Mueve un archivo o directorio
    std::string Move(const std::string &source, const std::string &destination);

}
#endif