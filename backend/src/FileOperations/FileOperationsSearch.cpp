#include "FileOperationsSearch.h"
#include "FileOperationsCore.h"
#include "../FileSystem/FileSystem.h"
#include "../UserSession/UserSession.h"
#include "../Utilities/Utilities.h"
#include "../Structs/Structs.h"
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

    std::string Chown(const std::string &path, const std::string &usr, const std::string &grp)
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

        if (UserSession::currentSession.uid != 1)
        {
            out << "Error: Solo root puede cambiar el propietario\n";
            file.close();
            return out.str();
        }

        std::string usersContent = UserSession::ReadUsersFile(file, sb);
        int newUid = -1;
        int newGid = -1;

        if (!usr.empty())
        {
            newUid = UserSession::GetUid(usr, usersContent);
            if (newUid == -1)
            {
                out << "Error: Usuario '" << usr << "' no existe\n";
                file.close();
                return out.str();
            }
        }

        if (!grp.empty())
        {
            newGid = UserSession::GetGid(grp, usersContent);
            if (newGid == -1)
            {
                out << "Error: Grupo '" << grp << "' no existe\n";
                file.close();
                return out.str();
            }
        }

        if (newUid != -1)
            targetInodeData.i_uid = newUid;
        if (newGid != -1)
            targetInodeData.i_gid = newGid;

        std::string now = Utilities::GetCurrentDateTime();
        memcpy(targetInodeData.i_mtime, now.c_str(), 19);
        FileSystem::WriteInode(file, sb, targetInode, targetInodeData);

        file.close();
        out << "Propietario cambiado: " << path;
        if (newUid != -1)
            out << " usuario=" << usr;
        if (newGid != -1)
            out << " grupo=" << grp;
        out << "\n";
        out << "======================\n";
        return out.str();
    }

    std::string Loss(const std::string &id)
    {
        std::ostringstream out;
        out << "======= LOSS =======\n";

        if (!UserSession::currentSession.active)
        {
            out << "Error: No hay sesión activa. Usa LOGIN primero\n";
            return out.str();
        }

        if (UserSession::currentSession.uid != 1)
        {
            out << "Error: Solo root puede ejecutar LOSS\n";
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

        if (sb.s_filesystem_type != 3)
        {
            out << "Error: LOSS solo funciona en EXT3\n";
            file.close();
            return out.str();
        }

        out << "Simulando pérdida de datos...\n";
        out << "Recuperando desde journaling...\n";

        int journalStart = UserSession::currentSession.partStart + sizeof(SuperBloque);
        int recovered = 0;

        for (int i = 0; i < FileSystem::JOURNALING_SIZE; i++)
        {
            Journal journalEntry{};
            Utilities::ReadObject(file, journalEntry, journalStart + i * sizeof(Journal));

            if (journalEntry.j_count > 0)
            {
                out << "Recuperando operación: " << journalEntry.j_content.i_operation << "\n";
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
        out << "======================\n";
        return out.str();
    }

}