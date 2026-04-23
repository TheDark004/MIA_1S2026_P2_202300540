#include "FileOperationsSearch.h"
#include "FileOperationsCore.h"
#include "../FileSystem/FileSystem.h"
#include "../UserSession/UserSession.h"
#include "../Utilities/Utilities.h"
#include "../Structs/Structs.h"
#include "../DiskManagement/DiskManagement.h"
#include <sstream>
#include <fstream>
#include <functional>
#include <cstring>

namespace FileOperations
{

    std::string Find(const std::string &path, const std::string &name)
    {
        std::ostringstream out;
        out << "======= FIND =======\n";

        if (!UserSession::currentSession.active)
        {
            out << "Error: No hay sesión activa. Usa LOGIN primero\n";
            return out.str();
        }

        auto file = Utilities::OpenFile(UserSession::currentSession.diskPath);
        if (!file.is_open())
        {
            out << "Error: No se pudo abrir el disco\n";
            return out.str();
        }

        SuperBloque sb{};
        FileSystem::ReadSuperBloque(file, UserSession::currentSession.partStart, sb);

        std::vector<std::string> parts = SplitPath(path);
        int startInode = TraversePath(file, sb, parts);
        if (startInode == -1)
        {
            out << "Error: No existe el directorio '" << path << "'\n";
            file.close();
            return out.str();
        }

        Inode startInodeData{};
        FileSystem::ReadInode(file, sb, startInode, startInodeData);
        if (!IsDirectory(startInodeData))
        {
            out << "Error: '" << path << "' no es un directorio\n";
            file.close();
            return out.str();
        }

        std::vector<std::string> foundPaths;
        std::function<void(int, const std::string &)> searchRecursive = [&](int currentInode, const std::string &currentPath)
        {
            Inode currentInodeData{};
            FileSystem::ReadInode(file, sb, currentInode, currentInodeData);

            for (int i = 0; i < 12; i++)
            {
                if (currentInodeData.i_block[i] == -1)
                    break;

                FolderBlock fb{};
                int pos = sb.s_block_start + currentInodeData.i_block[i] * sizeof(FolderBlock);
                Utilities::ReadObject(file, fb, pos);

                for (int j = 0; j < 4; j++)
                {
                    if (fb.b_content[j].b_inodo == -1)
                        continue;

                    std::string entryName(fb.b_content[j].b_name);
                    if (entryName == "." || entryName == "..")
                        continue;

                    std::string fullPath = currentPath + "/" + entryName;
                    if (entryName.find(name) != std::string::npos)
                    {
                        foundPaths.push_back(fullPath);
                    }

                    int childInode = fb.b_content[j].b_inodo;
                    Inode childInodeData{};
                    FileSystem::ReadInode(file, sb, childInode, childInodeData);
                    if (IsDirectory(childInodeData))
                    {
                        searchRecursive(childInode, fullPath);
                    }
                }
            }
        };

        searchRecursive(startInode, path);

        if (foundPaths.empty())
        {
            out << "No se encontraron archivos con '" << name << "' en '" << path << "'\n";
        }
        else
        {
            out << "Archivos encontrados:\n";
            for (const auto &foundPath : foundPaths)
            {
                out << "  " << foundPath << "\n";
            }
        }

        file.close();
        out << "======================\n";
        return out.str();
    }

    std::string Chmod(const std::string &path, const std::string &ugo)
    {
        std::ostringstream out;
        out << "======= CHMOD =======\n";

        if (!UserSession::currentSession.active)
        {
            out << "Error: No hay sesión activa. Usa LOGIN primero\n";
            return out.str();
        }

        if (ugo.size() != 3 || ugo.find_first_not_of("01234567") != std::string::npos)
        {
            out << "Error: Permisos inválidos. Usa formato octal (ej: 755)\n";
            return out.str();
        }

        auto file = Utilities::OpenFile(UserSession::currentSession.diskPath);
        if (!file.is_open())
        {
            out << "Error: No se pudo abrir el disco\n";
            return out.str();
        }

        SuperBloque sb{};
        FileSystem::ReadSuperBloque(file, UserSession::currentSession.partStart, sb);

        std::vector<std::string> parts = SplitPath(path);
        if (parts.empty())
        {
            out << "Error: Path inválido\n";
            file.close();
            return out.str();
        }

        int targetInode = TraversePath(file, sb, parts);
        if (targetInode == -1)
        {
            out << "Error: No existe '" << path << "'\n";
            file.close();
            return out.str();
        }

        Inode targetInodeData{};
        FileSystem::ReadInode(file, sb, targetInode, targetInodeData);

        if (UserSession::currentSession.uid != 1 && UserSession::currentSession.uid != targetInodeData.i_uid)
        {
            out << "Error: No tienes permisos para cambiar los permisos de '" << path << "'\n";
            file.close();
            return out.str();
        }

        memcpy(targetInodeData.i_perm, ugo.c_str(), 3);
        std::string now = Utilities::GetCurrentDateTime();
        memcpy(targetInodeData.i_mtime, now.c_str(), 19);
        FileSystem::WriteInode(file, sb, targetInode, targetInodeData);

        file.close();
        out << "Permisos cambiados: " << path << " -> " << ugo << "\n";
        out << "======================\n";
        return out.str();
    }

    void ChangeOwnerRecursive(std::fstream &file, SuperBloque &sb, int inodeNum, int newUid)
    {
        Inode inodeData{};
        FileSystem::ReadInode(file, sb, inodeNum, inodeData);

        // Cambiar el dueño y la fecha de modificación
        inodeData.i_uid = newUid;
        std::string now = Utilities::GetCurrentDateTime();
        memcpy(inodeData.i_mtime, now.c_str(), 19);
        FileSystem::WriteInode(file, sb, inodeNum, inodeData);

        // Si es archivo, terminamos la recursión de esta rama
        if (inodeData.i_type[0] == '1')
            return;

        // Si es carpeta, recorremos sus apuntadores directos
        for (int i = 0; i < 12; i++)
        {
            if (inodeData.i_block[i] != -1)
            {
                FolderBlock fb{};
                int blockPos = sb.s_block_start + inodeData.i_block[i] * sizeof(FolderBlock);
                Utilities::ReadObject(file, fb, blockPos);

                for (int j = 0; j < 4; j++)
                {
                    if (fb.b_content[j].b_inodo != -1)
                    {
                        // Evitamos un bucle infinito ignorando "." y ".."
                        size_t nameLen = strnlen(fb.b_content[j].b_name, 12);
                        std::string entryName(fb.b_content[j].b_name, nameLen);

                        if (entryName != "." && entryName != "..")
                        {
                            ChangeOwnerRecursive(file, sb, fb.b_content[j].b_inodo, newUid);
                        }
                    }
                }
            }
        }
    }

    std::string Chown(const std::string &path, const std::string &usuario, bool isRecursive)
    {
        std::ostringstream out;
        out << "======= CHOWN =======\n";

        if (!UserSession::currentSession.active)
        {
            out << "Error: No hay sesión activa. Usa LOGIN primero\n";
            return out.str();
        }

        auto file = Utilities::OpenFile(UserSession::currentSession.diskPath);
        if (!file.is_open())
        {
            out << "Error: No se pudo abrir el disco\n";
            return out.str();
        }

        SuperBloque sb{};
        FileSystem::ReadSuperBloque(file, UserSession::currentSession.partStart, sb);

        std::vector<std::string> parts = SplitPath(path);
        if (parts.empty())
        {
            out << "Error: Path inválido\n";
            file.close();
            return out.str();
        }

        int targetInode = TraversePath(file, sb, parts);
        if (targetInode == -1)
        {
            out << "Error: No existe '" << path << "'\n";
            file.close();
            return out.str();
        }

        Inode targetInodeData{};
        FileSystem::ReadInode(file, sb, targetInode, targetInodeData);

        if (UserSession::currentSession.uid != 1 && UserSession::currentSession.uid != targetInodeData.i_uid)
        {
            out << "Error: Solo el usuario root o el propietario original pueden hacer chown\n";
            file.close();
            return out.str();
        }

        std::string usersContent = UserSession::ReadUsersFile(file, sb);
        int newUid = UserSession::GetUid(usuario, usersContent);
        if (newUid == -1)
        {
            out << "Error: El usuario '" << usuario << "' no existe\n";
            file.close();
            return out.str();
        }

        // APLICAR LOS CAMBIOS
        if (isRecursive)
        {
            ChangeOwnerRecursive(file, sb, targetInode, newUid);
            out << "Propietario cambiado (RECURSIVO): " << path << " -> usuario=" << usuario << "\n";
        }
        else
        {
            targetInodeData.i_uid = newUid;
            std::string now = Utilities::GetCurrentDateTime();
            memcpy(targetInodeData.i_mtime, now.c_str(), 19);
            FileSystem::WriteInode(file, sb, targetInode, targetInodeData);
            out << "Propietario cambiado: " << path << " -> usuario=" << usuario << "\n";
        }

        if (sb.s_filesystem_type == 3)
        {
            Journal j_actual{};
            memset(&j_actual, 0, sizeof(Journal));

            strncpy(j_actual.j_content.i_operation, "chown", 9);
            strncpy(j_actual.j_content.i_path, path.c_str(), 31);
            j_actual.j_content.i_date = static_cast<float>(time(nullptr));

            int journalStart = UserSession::currentSession.partStart + sizeof(SuperBloque);

            for (int i = 0; i < 50; i++)
            {
                Journal temp{};
                file.seekg(journalStart + (i * sizeof(Journal)));
                file.read(reinterpret_cast<char *>(&temp), sizeof(Journal));

                if (temp.j_content.i_operation[0] == '\0')
                {
                    file.seekp(journalStart + (i * sizeof(Journal)));
                    file.write(reinterpret_cast<const char *>(&j_actual), sizeof(Journal));
                    break;
                }
            }
        }

        file.close();
        out << "======================\n";
        return out.str();
    }

    std::string Loss(const std::string &id)
    {
        std::ostringstream out;
        out << "======= LOSS =======\n";

        // 1. Obtener la ruta del disco usando tu función real
        std::string diskPath;
        if (DiskManagement::FindMountedById(id, diskPath) == -1)
        {
            out << "Error: La partición con ID " << id << " no está montada.\n";
            return out.str();
        }

        // 2. Obtener el nombre de la partición montada
        std::string partName = "";
        for (const auto &p : DiskManagement::GetMountedPartitionsList())
        {
            if (p.id == id)
            {
                partName = p.name;
                break;
            }
        }

        auto file = Utilities::OpenFile(diskPath);
        if (!file.is_open())
        {
            out << "Error: No se pudo abrir el disco\n";
            return out.str();
        }

        // 3. Leer el MBR para encontrar el partStart
        MBR mbr{};
        file.seekg(0);
        file.read(reinterpret_cast<char *>(&mbr), sizeof(MBR));

        int partStart = -1;
        for (int i = 0; i < 4; i++)
        {

            if (mbr.Partitions[i].Status[0] == '1' && std::string(mbr.Partitions[i].Name) == partName)
            {
                partStart = mbr.Partitions[i].Start;
                break;
            }
        }

        // (Opcional) Si manejas particiones lógicas, aquí iría la búsqueda en los EBR...

        if (partStart == -1)
        {
            out << "Error: No se encontró el inicio de la partición '" << partName << "'.\n";
            file.close();
            return out.str();
        }

        // 4. Leer el SuperBloque
        SuperBloque sb{};
        FileSystem::ReadSuperBloque(file, partStart, sb);

        if (sb.s_filesystem_type != 3)
        {
            out << "Error: LOSS solo funciona en EXT3\n";
            file.close();
            return out.str();
        }

        out << "Simulando pérdida de datos...\n";

        // 5. Destrucción
        int bmInodeSize = sb.s_inodes_count;
        int bmBlockSize = sb.s_blocks_count;
        int inodeTableSize = sb.s_inodes_count * sizeof(Inode);
        int blockTableSize = sb.s_blocks_count * 64;

        std::vector<char> zeroBmInode(bmInodeSize, '\0');
        file.seekp(sb.s_bm_inode_start);
        file.write(zeroBmInode.data(), bmInodeSize);

        std::vector<char> zeroBmBlock(bmBlockSize, '\0');
        file.seekp(sb.s_bm_block_start);
        file.write(zeroBmBlock.data(), bmBlockSize);

        std::vector<char> zeroInodeTable(inodeTableSize, '\0');
        file.seekp(sb.s_inode_start);
        file.write(zeroInodeTable.data(), inodeTableSize);

        std::vector<char> zeroBlockTable(blockTableSize, '\0');
        file.seekp(sb.s_block_start);
        file.write(zeroBlockTable.data(), blockTableSize);

        file.close();

        out << "¡Destrucción completada!\n";
        out << "Bitmaps y Tablas han sido llenados con \\0\n";
        out << "======================\n";
        return out.str();
    }

    std::string Recovery(const std::string &id)
    {
        std::ostringstream out;
        out << "======= RECOVERY =======\n";

        std::string diskPath;
        if (DiskManagement::FindMountedById(id, diskPath) == -1)
        {
            out << "Error: La partición con ID " << id << " no está montada.\n";
            return out.str();
        }

        std::string partName = "";
        for (const auto &p : DiskManagement::GetMountedPartitionsList())
        {
            if (p.id == id)
            {
                partName = p.name;
                break;
            }
        }

        auto file = Utilities::OpenFile(diskPath);
        if (!file.is_open())
        {
            out << "Error: No se pudo abrir el disco\n";
            return out.str();
        }

        MBR mbr{};
        file.seekg(0);
        file.read(reinterpret_cast<char *>(&mbr), sizeof(MBR));

        int partStart = -1;
        for (int i = 0; i < 4; i++)
        {
            if (mbr.Partitions[i].Status[0] == '1' && std::string(mbr.Partitions[i].Name) == partName)
            {
                partStart = mbr.Partitions[i].Start;
                break;
            }
        }

        if (partStart == -1)
        {
            out << "Error: No se encontró el inicio de la partición.\n";
            file.close();
            return out.str();
        }

        SuperBloque sb{};
        FileSystem::ReadSuperBloque(file, partStart, sb);

        if (sb.s_filesystem_type != 3)
        {
            out << "Error: RECOVERY solo funciona en EXT3\n";
            file.close();
            return out.str();
        }

        out << "Recuperando desde journaling...\n";

        int journalStart = partStart + sizeof(SuperBloque);
        int recovered = 0;

        for (int i = 0; i < 50; i++)
        {
            Journal journalEntry{};
            Utilities::ReadObject(file, journalEntry, journalStart + i * sizeof(Journal));

            if (journalEntry.j_content.i_operation[0] != '\0')
            {
                out << "Recuperando operación: " << journalEntry.j_content.i_operation << " en " << journalEntry.j_content.i_path << "\n";
                recovered++;
            }
        }

        if (recovered == 0)
        {
            out << "No hay operaciones para recuperar en el journal\n";
        }
        else
        {
            out << "Recuperadas " << recovered << " operaciones\n";
        }

        file.close();
        out << "Recuperación completada\n";
        out << "========================\n";
        return out.str();
    }

}