#ifndef FILEOPERATIONS_SEARCH_H
#define FILEOPERATIONS_SEARCH_H

#include <string>

namespace FileOperations
{

    // FIND — Busca archivos por nombre
    std::string Find(const std::string &path, const std::string &name);

    // CHMOD — Cambia permisos de un archivo o directorio
    std::string Chmod(const std::string &path, const std::string &ugo);

    // CHOWN — Cambia propietario de un archivo o directorio
    std::string Chown(const std::string &path, const std::string &usuario, bool isRecursive);

    // LOSS — Simula recuperación de datos en EXT3
    std::string Loss(const std::string &id);

}

#endif