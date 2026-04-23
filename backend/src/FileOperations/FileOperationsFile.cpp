#include "FileOperationsFile.h"
#include "FileOperationsCore.h"
#include "../FileSystem/FileSystem.h"
#include "../UserSession/UserSession.h"
#include "../Utilities/Utilities.h"
#include "../Structs/Structs.h"
#include "../DiskManagement/DiskManagement.h"
#include <sstream>
#include <fstream>
#include <algorithm>
#include <cstring>
#include <filesystem>

namespace FileOperations
{

    std::string Mkfile(const std::string &path, bool random, int size, const std::string &cont)
    {
        std::ostringstream out;
        out << "======= MKFILE =======\n";

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

        int parentInode = TraverseToParent(file, sb, parts);
        if (parentInode == -1)
        {
            if (random && size == 0)
            {
                std::vector<std::string> parentParts(parts.begin(), parts.end() - 1);
                int currentInode = 0;
                for (int i = 0; i < (int)parentParts.size(); i++)
                {
                    int existsInode = FindInDir(file, sb, currentInode, parentParts[i]);
                    if (existsInode != -1)
                    {
                        currentInode = existsInode;
                        continue;
                    }
                    int newInodeNum = FileSystem::AllocateInode(file, sb);
                    int newBlockNum = FileSystem::AllocateBlock(file, sb);
                    if (newInodeNum == -1 || newBlockNum == -1)
                    {
                        file.close();
                        return "Error: No hay espacio disponible\n";
                    }
                    FolderBlock fb{};
                    for (int j = 0; j < 4; j++)
                    {
                        std::memset(fb.b_content[j].b_name, 0, 12);
                        fb.b_content[j].b_inodo = -1;
                    }
                    std::memcpy(fb.b_content[0].b_name, ".", 1);
                    fb.b_content[0].b_inodo = newInodeNum;
                    std::memcpy(fb.b_content[1].b_name, "..", 2);
                    fb.b_content[1].b_inodo = currentInode;
                    FileSystem::WriteFolderBlock(file, sb, newBlockNum, fb);
                    std::string now = Utilities::GetCurrentDateTime();
                    Inode newInode{};
                    newInode.i_uid = UserSession::currentSession.uid;
                    newInode.i_gid = UserSession::GetGid(UserSession::currentSession.group, UserSession::ReadUsersFile(file, sb));
                    newInode.i_size = sizeof(FolderBlock);
                    memcpy(newInode.i_atime, now.c_str(), sizeof(newInode.i_atime));
                    memcpy(newInode.i_ctime, now.c_str(), sizeof(newInode.i_ctime));
                    memcpy(newInode.i_mtime, now.c_str(), sizeof(newInode.i_mtime));
                    for (int j = 0; j < 15; j++)
                        newInode.i_block[j] = -1;
                    newInode.i_block[0] = newBlockNum;
                    newInode.i_type[0] = '0';
                    std::memcpy(newInode.i_perm, "664", 3);
                    FileSystem::WriteInode(file, sb, newInodeNum, newInode);
                    AddEntryToDir(file, sb, UserSession::currentSession.partStart, currentInode, parentParts[i], newInodeNum);
                    currentInode = newInodeNum;
                }
                parentInode = currentInode;
            }
            else
            {
                out << "Error: El directorio padre no existe\n";
                out << "Usa MKDIR -p para crear los directorios intermedios\n";
                file.close();
                return out.str();
            }
        }

        std::string fileName = parts.back();
        if (FindInDir(file, sb, parentInode, fileName) != -1)
        {
            out << "Error: Ya existe '" << fileName << "'\n";
            file.close();
            return out.str();
        }

        if (size < 0)
        {
            out << "Error: -size no puede ser negativo\n";
            file.close();
            return out.str();
        }

        std::string content;
        if (!cont.empty())
        {
            std::ifstream realFile(cont);
            if (realFile.is_open())
            {
                content = std::string((std::istreambuf_iterator<char>(realFile)), std::istreambuf_iterator<char>());
                realFile.close();
            }
            else
            {
                content = cont;
            }
        }
        else if (size > 0)
        {
            std::string chars = "abcdefghijklmnopqrstuvwxyz0123456789\n";
            for (int i = 0; i < size; i++)
            {
                content += chars[rand() % chars.size()];
            }
        }

        int newInodeNum = FileSystem::AllocateInode(file, sb);
        if (newInodeNum == -1)
        {
            out << "Error: No hay inodos disponibles\n";
            file.close();
            return out.str();
        }

        std::string now = Utilities::GetCurrentDateTime();
        Inode newInode{};
        newInode.i_uid = UserSession::currentSession.uid;
        newInode.i_gid = UserSession::GetGid(UserSession::currentSession.group, UserSession::ReadUsersFile(file, sb));
        newInode.i_size = (int)content.size();
        std::memcpy(newInode.i_atime, now.c_str(), 19);
        std::memcpy(newInode.i_ctime, now.c_str(), 19);
        std::memcpy(newInode.i_mtime, now.c_str(), 19);
        for (int j = 0; j < 15; j++)
            newInode.i_block[j] = -1;
        newInode.i_type[0] = '1';
        std::memcpy(newInode.i_perm, "664", 3);

        if (!content.empty())
        {
            int offset = 0;
            int blockSlot = 0;
            int totalBytes = (int)content.size();
            while (offset < totalBytes && blockSlot < 12)
            {
                int newBlockNum = FileSystem::AllocateBlock(file, sb);
                if (newBlockNum == -1)
                {
                    file.close();
                    return "Error: No hay bloques disponibles\n";
                }
                FileBlock fb{};
                std::memset(fb.b_content, 0, 64);
                int toCopy = std::min(totalBytes - offset, (int)sizeof(FileBlock));
                std::memcpy(fb.b_content, content.c_str() + offset, toCopy);
                FileSystem::WriteFileBlock(file, sb, newBlockNum, fb);
                newInode.i_block[blockSlot] = newBlockNum;
                offset += toCopy;
                blockSlot++;
            }
        }

        FileSystem::WriteInode(file, sb, newInodeNum, newInode);
        AddEntryToDir(file, sb, UserSession::currentSession.partStart, parentInode, fileName, newInodeNum);

        if (sb.s_filesystem_type == 3)
        {
            Journal j_actual{};
            memset(&j_actual, 0, sizeof(Journal));

            strncpy(j_actual.j_content.i_operation, "mkfile", 9);
            strncpy(j_actual.j_content.i_path, path.c_str(), 31);

            strncpy(j_actual.j_content.i_content, cont.c_str(), 63);

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

        // Sincronizar con sistema de archivos físico
        std::string mountPoint = DiskManagement::GetMountPoint(UserSession::currentSession.partId);
        if (!mountPoint.empty())
        {
            std::filesystem::create_directories(std::filesystem::path(mountPoint + path).parent_path());
            std::ofstream physFile(mountPoint + path);
            if (physFile.is_open())
            {
                physFile << content;
                physFile.close();
            }
        }

        out << "Archivo creado: " << path << "\n";
        out << "Tamaño: " << content.size() << " bytes\n";
        if (!content.empty() && !random)
        {
            out << "Contenido: " << content << "\n";
        }
        out << "======================\n";
        return out.str();
    }

    std::string Cat(const std::string &filePath)
    {
        std::ostringstream out;
        out << "======= CAT =======\n";

        if (!UserSession::currentSession.active)
        {
            out << "Error: No hay sesión activa\n";
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

        std::vector<std::string> parts = SplitPath(filePath);
        if (parts.empty())
        {
            out << "Error: Path inválido\n";
            file.close();
            return out.str();
        }

        int fileInodeNum = TraversePath(file, sb, parts);
        if (fileInodeNum == -1)
        {
            out << "Error: No existe '" << filePath << "'\n";
            file.close();
            return out.str();
        }

        Inode fileInode{};
        FileSystem::ReadInode(file, sb, fileInodeNum, fileInode);
        if (fileInode.i_type[0] == '0')
        {
            out << "Error: '" << filePath << "' es un directorio, no un archivo\n";
            file.close();
            return out.str();
        }

        std::string content;
        int remaining = fileInode.i_size;
        for (int i = 0; i < 12 && remaining > 0; i++)
        {
            if (fileInode.i_block[i] == -1)
                break;
            FileBlock fb{};
            int pos = sb.s_block_start + fileInode.i_block[i] * sizeof(FolderBlock);
            Utilities::ReadObject(file, fb, pos);
            int toRead = std::min(remaining, (int)sizeof(FileBlock));
            content.append(fb.b_content, toRead);
            remaining -= toRead;
        }

        file.close();
        out << content << "\n";
        out << "===================\n";
        return out.str();
    }

    std::string Remove(const std::string &path)
    {
        std::ostringstream out;
        out << "======= REMOVE =======\n";

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

        int parentInode = TraverseToParent(file, sb, parts);
        if (parentInode == -1)
        {
            out << "Error: El directorio padre no existe\n";
            file.close();
            return out.str();
        }

        std::string name = parts.back();
        int targetInode = FindInDir(file, sb, parentInode, name);
        if (targetInode == -1)
        {
            out << "Error: No existe '" << path << "'\n";
            file.close();
            return out.str();
        }

        if (!RemoveInodeRecursive(file, sb, UserSession::currentSession.partStart, targetInode))
        {
            out << "Error: No se pudo eliminar '" << path << "'\n";
            file.close();
            return out.str();
        }

        if (!RemoveDirEntry(file, sb, parentInode, name))
        {
            out << "Advertencia: objeto eliminado pero no se pudo limpiar la entrada en el padre\n";
            file.close();
            return out.str();
        }

        if (sb.s_filesystem_type == 3)
        {
            Journal j_actual{};
            memset(&j_actual, 0, sizeof(Journal));

            strncpy(j_actual.j_content.i_operation, "remove", 9);
            strncpy(j_actual.j_content.i_path, path.c_str(), 31);
            // El contenido queda vacío porque en un remove no escribimos nada nuevo

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

        // Sincronizar con sistema de archivos físico
        std::string mountPoint = DiskManagement::GetMountPoint(UserSession::currentSession.partId);
        if (!mountPoint.empty())
        {
            std::filesystem::remove_all(mountPoint + path);
        }

        out << "Eliminado: " << path << "\n";
        out << "======================\n";
        return out.str();
    }

    std::string Rename(const std::string &path, const std::string &newName)
    {
        std::ostringstream out;
        out << "======= RENAME =======\n";

        if (!UserSession::currentSession.active)
        {
            out << "Error: No hay sesión activa. Usa LOGIN primero\n";
            return out.str();
        }

        if (newName.empty())
        {
            out << "Error: Nombre de destino inválido\n";
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

        int parentInode = TraverseToParent(file, sb, parts);
        if (parentInode == -1)
        {
            out << "Error: El directorio padre no existe\n";
            file.close();
            return out.str();
        }

        std::string oldName = parts.back();
        int targetInode = FindInDir(file, sb, parentInode, oldName);
        if (targetInode == -1)
        {
            out << "Error: No existe '" << path << "'\n";
            file.close();
            return out.str();
        }

        if (FindInDir(file, sb, parentInode, newName) != -1)
        {
            out << "Error: Ya existe un elemento con el nombre '" << newName << "'\n";
            file.close();
            return out.str();
        }

        Inode parentInodeData{};
        FileSystem::ReadInode(file, sb, parentInode, parentInodeData);

        for (int i = 0; i < 12; i++)
        {
            if (parentInodeData.i_block[i] == -1)
                break;
            int pos = sb.s_block_start + parentInodeData.i_block[i] * sizeof(FolderBlock);
            FolderBlock fb{};
            Utilities::ReadObject(file, fb, pos);
            for (int j = 0; j < 4; j++)
            {
                if (fb.b_content[j].b_inodo == targetInode)
                {
                    std::memset(fb.b_content[j].b_name, 0, 12);
                    std::memcpy(fb.b_content[j].b_name, newName.c_str(), std::min(newName.size(), (size_t)11));
                    Utilities::WriteObject(file, fb, pos);
                    break;
                }
            }
        }

        if (sb.s_filesystem_type == 3)
        {
            Journal j_actual{};
            memset(&j_actual, 0, sizeof(Journal));

            strncpy(j_actual.j_content.i_operation, "rename", 9);
            strncpy(j_actual.j_content.i_path, path.c_str(), 31);
            // El contenido queda vacío porque en un remove no escribimos nada nuevo

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
        out << "Renombrado: " << path << " -> " << newName << "\n";
        out << "======================\n";
        return out.str();
    }

    std::string Copy(const std::string &source, const std::string &destination)
    {
        std::ostringstream out;
        out << "======= COPY =======\n";

        if (!UserSession::currentSession.active)
        {
            out << "Error: No hay sesión activa. Usa LOGIN primero\n";
            return out.str();
        }

        if (source.empty() || destination.empty())
        {
            out << "Error: Source o destino inválido\n";
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

        std::vector<std::string> srcParts = SplitPath(source);
        if (srcParts.empty())
        {
            out << "Error: Source inválido\n";
            file.close();
            return out.str();
        }

        int srcInode = TraversePath(file, sb, srcParts);
        if (srcInode == -1)
        {
            out << "Error: No existe '" << source << "'\n";
            file.close();
            return out.str();
        }

        std::vector<std::string> destParts = SplitPath(destination);
        if (destParts.empty())
        {
            out << "Error: Destination inválido\n";
            file.close();
            return out.str();
        }

        int destParent = -1;
        std::string newName;
        int existingDest = TraversePath(file, sb, destParts);
        if (existingDest != -1)
        {
            Inode destInode{};
            FileSystem::ReadInode(file, sb, existingDest, destInode);
            if (IsDirectory(destInode))
            {
                destParent = existingDest;
                newName = srcParts.back();
            }
            else
            {
                out << "Error: El destino ya existe y no es un directorio\n";
                file.close();
                return out.str();
            }
        }
        else
        {
            destParent = TraverseToParent(file, sb, destParts);
            if (destParent == -1)
            {
                out << "Error: El directorio padre del destino no existe\n";
                file.close();
                return out.str();
            }
            newName = destParts.back();
        }

        if (FindInDir(file, sb, destParent, newName) != -1)
        {
            out << "Error: Ya existe un elemento '" << newName << "' en el destino\n";
            file.close();
            return out.str();
        }

        if (!CopyInodeRecursive(file, sb, UserSession::currentSession.partStart, srcInode, destParent, newName))
        {
            out << "Error: No se pudo copiar '" << source << "'\n";
            file.close();
            return out.str();
        }

        file.close();

        // Sincronizar con sistema de archivos físico
        std::string mountPoint = DiskManagement::GetMountPoint(UserSession::currentSession.partId);
        if (!mountPoint.empty())
        {
            std::filesystem::create_directories(std::filesystem::path(mountPoint + destination).parent_path());
            std::filesystem::copy_file(mountPoint + source, mountPoint + destination, std::filesystem::copy_options::overwrite_existing);
        }

        out << "Copiado: " << source << " -> " << destination << "\n";
        out << "======================\n";
        return out.str();
    }

    std::string Move(const std::string &source, const std::string &destination)
    {
        std::ostringstream out;
        out << "======= MOVE =======\n";

        if (!UserSession::currentSession.active)
        {
            out << "Error: No hay sesión activa. Usa LOGIN primero\n";
            return out.str();
        }

        if (source.empty() || destination.empty())
        {
            out << "Error: Source o destino inválido\n";
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

        std::vector<std::string> srcParts = SplitPath(source);
        if (srcParts.empty())
        {
            out << "Error: Source inválido\n";
            file.close();
            return out.str();
        }

        int srcParent = TraverseToParent(file, sb, srcParts);
        if (srcParent == -1)
        {
            out << "Error: El directorio padre del source no existe\n";
            file.close();
            return out.str();
        }

        std::string srcName = srcParts.back();
        int srcInode = FindInDir(file, sb, srcParent, srcName);
        if (srcInode == -1)
        {
            out << "Error: No existe '" << source << "'\n";
            file.close();
            return out.str();
        }

        std::vector<std::string> destParts = SplitPath(destination);
        if (destParts.empty())
        {
            out << "Error: Destination inválido\n";
            file.close();
            return out.str();
        }

        int destParent = -1;
        std::string newName;
        int existingDest = TraversePath(file, sb, destParts);
        if (existingDest != -1)
        {
            Inode destInode{};
            FileSystem::ReadInode(file, sb, existingDest, destInode);
            if (IsDirectory(destInode))
            {
                destParent = existingDest;
                newName = srcName;
            }
            else
            {
                out << "Error: El destino ya existe y no es un directorio\n";
                file.close();
                return out.str();
            }
        }
        else
        {
            destParent = TraverseToParent(file, sb, destParts);
            if (destParent == -1)
            {
                out << "Error: El directorio padre del destino no existe\n";
                file.close();
                return out.str();
            }
            newName = destParts.back();
        }

        if (srcParent == destParent && newName == srcName)
        {
            out << "Error: Source y destino son iguales\n";
            file.close();
            return out.str();
        }

        if (FindInDir(file, sb, destParent, newName) != -1)
        {
            out << "Error: Ya existe un elemento '" << newName << "' en el destino\n";
            file.close();
            return out.str();
        }

        if (IsInsidePath(source, destination) && source != destination)
        {
            out << "Error: No se puede mover un directorio dentro de sí mismo\n";
            file.close();
            return out.str();
        }

        if (!RemoveDirEntry(file, sb, srcParent, srcName))
        {
            out << "Error: No se pudo quitar la entrada del origen\n";
            file.close();
            return out.str();
        }

        if (!AddEntryToDir(file, sb, UserSession::currentSession.partStart, destParent, newName, srcInode))
        {
            out << "Error: No se pudo crear la entrada en el destino\n";
            file.close();
            return out.str();
        }

        Inode movedInode{};
        FileSystem::ReadInode(file, sb, srcInode, movedInode);
        std::string now = Utilities::GetCurrentDateTime();
        std::memcpy(movedInode.i_mtime, now.c_str(), 19);
        FileSystem::WriteInode(file, sb, srcInode, movedInode);

        if (srcParent != destParent && IsDirectory(movedInode))
        {
            UpdateDirectoryParent(file, sb, srcInode, destParent);
        }

        file.close();

        // Sincronizar con sistema de archivos físico
        std::string mountPoint = DiskManagement::GetMountPoint(UserSession::currentSession.partId);
        if (!mountPoint.empty())
        {
            std::filesystem::create_directories(std::filesystem::path(mountPoint + destination).parent_path());
            std::filesystem::rename(mountPoint + source, mountPoint + destination);
        }

        out << "Movido: " << source << " -> " << destination << "\n";
        out << "======================\n";
        return out.str();
    }

}