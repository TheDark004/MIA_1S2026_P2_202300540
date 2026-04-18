#ifndef FILEOPERATIONS_DIR_H
#define FILEOPERATIONS_DIR_H

#include <string>

namespace FileOperations
{

    // MKDIR — Crea un directorio en el sistema de archivos
    std::string Mkdir(const std::string &path, bool createParents);

} // namespace FileOperations

#endif // FILEOPERATIONS_DIR_H