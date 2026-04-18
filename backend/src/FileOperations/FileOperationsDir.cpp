#include "FileOperationsDir.h"
#include "FileOperationsCore.h"
#include "../FileSystem/FileSystem.h"
#include "../UserSession/UserSession.h"
#include "../Utilities/Utilities.h"
#include "../Structs/Structs.h"
#include <sstream>
#include <fstream>
#include <cstring>

namespace FileOperations
{

    std::string Mkdir(const std::string &path, bool createParents)
    {
        std::ostringstream out;
        out << "======= MKDIR =======\n";

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

        if (createParents)
        {
            int currentInode = 0;
            for (int i = 0; i < (int)parts.size(); i++)
            {
                int existsInode = FindInDir(file, sb, currentInode, parts[i]);
                if (existsInode != -1)
                {
                    currentInode = existsInode;
                    continue;
                }
                int newInodeNum = FileSystem::AllocateInode(file, sb);
                if (newInodeNum == -1)
                {
                    out << "Error: No hay inodos disponibles\n";
                    file.close();
                    return out.str();
                }
                int newBlockNum = FileSystem::AllocateBlock(file, sb);
                if (newBlockNum == -1)
                {
                    out << "Error: No hay bloques disponibles\n";
                    file.close();
                    return out.str();
                }
                FolderBlock fb{};
                for (int j = 0; j < 4; j++)
                {
                    memset(fb.b_content[j].b_name, 0, 12);
                    fb.b_content[j].b_inodo = -1;
                }
                memcpy(fb.b_content[0].b_name, ".", 1);
                fb.b_content[0].b_inodo = newInodeNum;
                memcpy(fb.b_content[1].b_name, "..", 2);
                fb.b_content[1].b_inodo = currentInode;
                FileSystem::WriteFolderBlock(file, sb, newBlockNum, fb);
                std::string now = Utilities::GetCurrentDateTime();
                Inode newInode{};
                newInode.i_uid = UserSession::currentSession.uid;
                newInode.i_gid = UserSession::GetGid(UserSession::currentSession.group, UserSession::ReadUsersFile(file, sb));
                newInode.i_size = sizeof(FolderBlock);
                memcpy(newInode.i_atime, now.c_str(), 19);
                memcpy(newInode.i_ctime, now.c_str(), 19);
                memcpy(newInode.i_mtime, now.c_str(), 19);
                for (int j = 0; j < 15; j++)
                    newInode.i_block[j] = -1;
                newInode.i_block[0] = newBlockNum;
                newInode.i_type[0] = '0';
                memcpy(newInode.i_perm, "664", 3);
                FileSystem::WriteInode(file, sb, newInodeNum, newInode);
                AddEntryToDir(file, sb, UserSession::currentSession.partStart, currentInode, parts[i], newInodeNum);
                out << "Directorio creado: /";
                for (int j = 0; j <= i; j++)
                {
                    out << parts[j];
                    if (j < i)
                        out << "/";
                }
                out << "\n";
                currentInode = newInodeNum;
            }
        }
        else
        {
            int parentInode = TraverseToParent(file, sb, parts);
            if (parentInode == -1)
            {
                out << "Error: El directorio padre no existe\n";
                out << "Usa -p para crear directorios intermedios\n";
                file.close();
                return out.str();
            }
            std::string dirName = parts.back();
            if (FindInDir(file, sb, parentInode, dirName) != -1)
            {
                out << "Error: Ya existe '" << dirName << "'\n";
                file.close();
                return out.str();
            }
            int newInodeNum = FileSystem::AllocateInode(file, sb);
            int newBlockNum = FileSystem::AllocateBlock(file, sb);
            if (newInodeNum == -1 || newBlockNum == -1)
            {
                out << "Error: No hay espacio disponible\n";
                file.close();
                return out.str();
            }
            FolderBlock fb{};
            for (int j = 0; j < 4; j++)
            {
                memset(fb.b_content[j].b_name, 0, 12);
                fb.b_content[j].b_inodo = -1;
            }
            memcpy(fb.b_content[0].b_name, ".", 1);
            fb.b_content[0].b_inodo = newInodeNum;
            memcpy(fb.b_content[1].b_name, "..", 2);
            fb.b_content[1].b_inodo = parentInode;
            FileSystem::WriteFolderBlock(file, sb, newBlockNum, fb);
            std::string now = Utilities::GetCurrentDateTime();
            Inode newInode{};
            newInode.i_uid = UserSession::currentSession.uid;
            newInode.i_gid = UserSession::GetGid(UserSession::currentSession.group, UserSession::ReadUsersFile(file, sb));
            newInode.i_size = sizeof(FolderBlock);
            memcpy(newInode.i_atime, now.c_str(), 19);
            memcpy(newInode.i_ctime, now.c_str(), 19);
            memcpy(newInode.i_mtime, now.c_str(), 19);
            for (int j = 0; j < 15; j++)
                newInode.i_block[j] = -1;
            newInode.i_block[0] = newBlockNum;
            newInode.i_type[0] = '0';
            memcpy(newInode.i_perm, "664", 3);
            FileSystem::WriteInode(file, sb, newInodeNum, newInode);
            AddEntryToDir(file, sb, UserSession::currentSession.partStart, parentInode, dirName, newInodeNum);
            out << "Directorio creado: " << path << "\n";
        }

        file.close();
        out << "=====================\n";
        return out.str();
    }

}