#pragma once
#include <string>
#include <vector>

namespace DiskManagement
{

    // Partición montada en RAM
    // MOUNT guarda las particiones en MEMORIA,
    // no las busca en disco cada vez. Usamos un arreglo estático.

    struct MountedPartition
    {
        std::string path;       // Ruta del .mia
        std::string name;       // Nombre de la partición
        std::string id;         // ID asignado: "341A"
        int correlative;        // Número correlativo de montaje
        std::string mountPoint; // Ruta física  donde se refleja la estructura
    };

    std::vector<MountedPartition> GetMountedPartitionsList();

    // Todas las funciones retornan string para enviarlo al frontend
    std::string Mkdisk(int size, const std::string &fit,
                       const std::string &unit, const std::string &path);

    std::string Rmdisk(const std::string &path);

    std::string Fdisk(int size, const std::string &path, const std::string &name,
                      const std::string &type, const std::string &fit,
                      const std::string &unit);

    // FUNCIONES PARA P2
    std::string FdiskAdd(int addSize, const std::string &unit, const std::string &path, const std::string &name);

    std::string FdiskDelete(const std::string &deleteType, const std::string &path, const std::string &name);

    std::string Mount(const std::string &path, const std::string &name);

    std::string Unmount(const std::string &id);

    std::string Mounted();

    // Retorna el índice encontrado o -1 si no existe
    int FindMountedById(const std::string &id, std::string &outPath);

    // Retorna el mountPoint físico o "" si no existe
    std::string GetMountPoint(const std::string &id);

}