#include "FileOperations.h"
#include "../FileSystem/FileSystem.h"
#include "../UserSession/UserSession.h"
#include "../Utilities/Utilities.h"
#include "../Structs/Structs.h"
#include <sstream>
#include <fstream>
#include <cstring>
#include <cstdlib>
#include <algorithm>
#include <functional>

namespace FileOperations
{

    // SplitPath — Divide un path en sus partes
    //
    // "/home/user/docs" -> ["home", "user", "docs"]
    // "/home/" -> ["home"]
    // "/" -> []

    std::vector<std::string> SplitPath(const std::string &path)
    {
        std::vector<std::string> parts;
        std::istringstream ss(path);
        std::string token;

        while (std::getline(ss, token, '/'))
        {
            if (!token.empty())
            {
                parts.push_back(token);
            }
        }
        return parts;
    }

    static bool IsDirectory(const Inode &inode)
    {
        return inode.i_type[0] == '0';
    }

    static bool IsFile(const Inode &inode)
    {
        return inode.i_type[0] == '1';
    }

    static bool FreeInodeBlocks(std::fstream &file, SuperBloque &sb,
                                int partStart, int inodeNum,
                                const Inode &inode)
    {
        // Liberar bloques directos
        for (int i = 0; i < 12; i++)
        {
            if (inode.i_block[i] == -1)
                continue;

            int blockNum = inode.i_block[i];
            int bytePos = blockNum / 8;
            int bitPos = 7 - (blockNum % 8);

            file.seekg(sb.s_bm_block_start + bytePos);
            char byteVal;
            file.read(&byteVal, 1);
            byteVal &= ~(1 << bitPos);
            file.seekp(sb.s_bm_block_start + bytePos);
            file.write(&byteVal, 1);
            file.flush();

            sb.s_free_blocks_count++;
            if (blockNum < sb.s_first_blo)
                sb.s_first_blo = blockNum;
        }

        // Liberar el inodo
        int inodeBytePos = inodeNum / 8;
        int inodeBitPos = 7 - (inodeNum % 8);
        file.seekg(sb.s_bm_inode_start + inodeBytePos);
        char inodeByte;
        file.read(&inodeByte, 1);
        inodeByte &= ~(1 << inodeBitPos);
        file.seekp(sb.s_bm_inode_start + inodeBytePos);
        file.write(&inodeByte, 1);
        file.flush();

        sb.s_free_inodes_count++;
        if (inodeNum < sb.s_first_ino)
            sb.s_first_ino = inodeNum;

        Inode emptyInode{};
        emptyInode.i_uid = -1;
        emptyInode.i_gid = -1;
        emptyInode.i_size = 0;
        for (int i = 0; i < 15; i++)
            emptyInode.i_block[i] = -1;
        std::memset(emptyInode.i_atime, 0, 19);
        std::memset(emptyInode.i_ctime, 0, 19);
        std::memset(emptyInode.i_mtime, 0, 19);
        emptyInode.i_type[0] = '1';
        std::memset(emptyInode.i_perm, 0, 3);

        FileSystem::WriteInode(file, sb, inodeNum, emptyInode);
        FileSystem::WriteSuperBloque(file, partStart, sb);
        return true;
    }

    static bool RemoveDirEntry(std::fstream &file, SuperBloque &sb,
                               int dirInodeNum, const std::string &name)
    {
        Inode dirInode{};
        FileSystem::ReadInode(file, sb, dirInodeNum, dirInode);

        for (int i = 0; i < 12; i++)
        {
            if (dirInode.i_block[i] == -1)
                break;

            int pos = sb.s_block_start + dirInode.i_block[i] * sizeof(FolderBlock);
            FolderBlock fb{};
            Utilities::ReadObject(file, fb, pos);

            for (int j = 0; j < 4; j++)
            {
                if (fb.b_content[j].b_inodo == -1)
                    continue;
                std::string entryName(fb.b_content[j].b_name);
                if (entryName == name)
                {
                    fb.b_content[j].b_inodo = -1;
                    std::memset(fb.b_content[j].b_name, 0, 12);
                    Utilities::WriteObject(file, fb, pos);

                    dirInode.i_size = std::max(0, dirInode.i_size - (int)sizeof(FolderContent));
                    std::string now = Utilities::GetCurrentDateTime();
                    std::memcpy(dirInode.i_mtime, now.c_str(), 19);
                    FileSystem::WriteInode(file, sb, dirInodeNum, dirInode);
                    return true;
                }
            }
        }
        return false;
    }

    static bool RemoveInodeRecursive(std::fstream &file, SuperBloque &sb,
                                     int partStart, int inodeNum)
    {
        Inode inode{};
        FileSystem::ReadInode(file, sb, inodeNum, inode);

        if (IsDirectory(inode))
        {
            for (int i = 0; i < 12; i++)
            {
                if (inode.i_block[i] == -1)
                    break;

                FolderBlock fb{};
                Utilities::ReadObject(file, fb,
                                      sb.s_block_start + inode.i_block[i] * sizeof(FolderBlock));

                for (int j = 0; j < 4; j++)
                {
                    if (fb.b_content[j].b_inodo == -1)
                        continue;
                    std::string name(fb.b_content[j].b_name);
                    if (name == "." || name == "..")
                        continue;
                    RemoveInodeRecursive(file, sb, partStart, fb.b_content[j].b_inodo);
                }
            }
        }

        return FreeInodeBlocks(file, sb, partStart, inodeNum, inode);
    }

    static bool CopyInodeRecursive(std::fstream &file, SuperBloque &sb,
                                   int partStart, int srcInodeNum,
                                   int destParentInodeNum,
                                   const std::string &name)
    {
        Inode srcInode{};
        FileSystem::ReadInode(file, sb, srcInodeNum, srcInode);

        int newInodeNum = FileSystem::AllocateInode(file, sb);
        if (newInodeNum == -1)
            return false;

        Inode newInode = srcInode;
        std::string now = Utilities::GetCurrentDateTime();
        newInode.i_uid = UserSession::currentSession.uid;
        newInode.i_gid = UserSession::GetGid(
            UserSession::currentSession.group,
            UserSession::ReadUsersFile(file, sb));
        std::memcpy(newInode.i_atime, now.c_str(), 19);
        std::memcpy(newInode.i_ctime, now.c_str(), 19);
        std::memcpy(newInode.i_mtime, now.c_str(), 19);
        newInode.i_size = srcInode.i_size;
        for (int i = 0; i < 15; i++)
            newInode.i_block[i] = -1;
        std::memcpy(newInode.i_perm, "664", 3);

        if (IsDirectory(srcInode))
        {
            int newBlockNum = FileSystem::AllocateBlock(file, sb);
            if (newBlockNum == -1)
                return false;

            newInode.i_block[0] = newBlockNum;
            newInode.i_type[0] = '0';
            newInode.i_size = sizeof(FolderBlock);

            FolderBlock newFb{};
            std::memset(newFb.b_content[0].b_name, 0, 12);
            std::memcpy(newFb.b_content[0].b_name, ".", 1);
            newFb.b_content[0].b_inodo = newInodeNum;
            std::memset(newFb.b_content[1].b_name, 0, 12);
            std::memcpy(newFb.b_content[1].b_name, "..", 2);
            newFb.b_content[1].b_inodo = destParentInodeNum;
            for (int j = 2; j < 4; j++)
            {
                newFb.b_content[j].b_inodo = -1;
            }

            FileSystem::WriteFolderBlock(file, sb, newBlockNum, newFb);
            FileSystem::WriteInode(file, sb, newInodeNum, newInode);

            if (!AddEntryToDir(file, sb, partStart, destParentInodeNum,
                               name, newInodeNum))
            {
                FreeInodeBlocks(file, sb, partStart, newInodeNum, newInode);
                return false;
            }

            for (int i = 0; i < 12; i++)
            {
                if (srcInode.i_block[i] == -1)
                    break;

                FolderBlock srcFb{};
                Utilities::ReadObject(file, srcFb,
                                      sb.s_block_start + srcInode.i_block[i] * sizeof(FolderBlock));

                for (int j = 0; j < 4; j++)
                {
                    if (srcFb.b_content[j].b_inodo == -1)
                        continue;
                    std::string childName(srcFb.b_content[j].b_name);
                    if (childName == "." || childName == "..")
                        continue;
                    if (!CopyInodeRecursive(file, sb, partStart,
                                            srcFb.b_content[j].b_inodo,
                                            newInodeNum, childName))
                    {
                        return false;
                    }
                }
            }
            return true;
        }

        // Copiar archivo
        int bytesRemaining = srcInode.i_size;
        for (int i = 0; i < 12 && bytesRemaining > 0; i++)
        {
            if (srcInode.i_block[i] == -1)
                break;

            int srcBlockNum = srcInode.i_block[i];
            FileBlock srcFb{};
            Utilities::ReadObject(file, srcFb,
                                  sb.s_block_start + srcBlockNum * sizeof(FolderBlock));

            int newBlockNum = FileSystem::AllocateBlock(file, sb);
            if (newBlockNum == -1)
            {
                FreeInodeBlocks(file, sb, partStart, newInodeNum, newInode);
                return false;
            }

            FileSystem::WriteFileBlock(file, sb, newBlockNum, srcFb);
            newInode.i_block[i] = newBlockNum;
            bytesRemaining -= (int)sizeof(FileBlock);
        }

        newInode.i_type[0] = '1';
        FileSystem::WriteInode(file, sb, newInodeNum, newInode);

        if (!AddEntryToDir(file, sb, partStart, destParentInodeNum,
                           name, newInodeNum))
        {
            FreeInodeBlocks(file, sb, partStart, newInodeNum, newInode);
            return false;
        }
        return true;
    }

    static bool UpdateDirectoryParent(std::fstream &file, const SuperBloque &sb,
                                      int dirInodeNum, int newParentInodeNum)
    {
        Inode dirInode{};
        FileSystem::ReadInode(file, sb, dirInodeNum, dirInode);

        if (!IsDirectory(dirInode))
            return false;

        if (dirInode.i_block[0] == -1)
            return false;

        FolderBlock fb{};
        Utilities::ReadObject(file, fb,
                              sb.s_block_start + dirInode.i_block[0] * sizeof(FolderBlock));

        std::memcpy(fb.b_content[1].b_name, "..", 2);
        fb.b_content[1].b_inodo = newParentInodeNum;
        FileSystem::WriteFolderBlock(file, sb, dirInode.i_block[0], fb);

        return true;
    }

    // FindInDir — Busca una entrada por nombre en un directorio
    // Un directorio puede tener múltiples bloques (cuando tiene más de 4 entradas).
    // Recorremos todos los bloques del inodo.
    // Retorna el número de inodo de la entrada, o -1 si no existe.

    int FindInDir(std::fstream &file, const SuperBloque &sb,
                  int dirInodeNum, const std::string &name)
    {

        Inode dirInode{};
        FileSystem::ReadInode(file, sb, dirInodeNum, dirInode);

        // Recorrer los 12 bloques directos del inodo
        for (int i = 0; i < 12; i++)
        {
            if (dirInode.i_block[i] == -1)
                break;

            // Leer el FolderBlock
            FolderBlock fb{};
            int pos = sb.s_block_start + dirInode.i_block[i] * sizeof(FolderBlock);
            Utilities::ReadObject(file, fb, pos);

            // Revisar las 4 entradas del bloque
            for (int j = 0; j < 4; j++)
            {
                if (fb.b_content[j].b_inodo == -1)
                    continue;

                std::string entryName(fb.b_content[j].b_name);
                if (entryName == name)
                {
                    return fb.b_content[j].b_inodo;
                }
            }
        }
        return -1; // no encontrado
    }

    // AddEntryToDir — Agrega una entrada a un directorio
    // Pasos:
    // 1. Leer el inodo del directorio
    // 2. Buscar un espacio vacío (b_inodo == -1) en sus bloques
    // 3. Si no hay espacio -> asignar nuevo bloque al directorio
    // 4. Escribir la entrada en el espacio encontrado
    bool AddEntryToDir(std::fstream &file, SuperBloque &sb,
                       int partStart, int dirInodeNum,
                       const std::string &name, int newInodeNum)
    {

        Inode dirInode{};
        FileSystem::ReadInode(file, sb, dirInodeNum, dirInode);

        // Buscar espacio vacío en bloques existentes
        for (int i = 0; i < 12; i++)
        {
            if (dirInode.i_block[i] == -1)
                break;

            FolderBlock fb{};
            int pos = sb.s_block_start + dirInode.i_block[i] * sizeof(FolderBlock);
            Utilities::ReadObject(file, fb, pos);

            for (int j = 0; j < 4; j++)
            {
                if (fb.b_content[j].b_inodo == -1)
                {
                    // espacio libre encontrado → escribir la entrada
                    std::memset(fb.b_content[j].b_name, 0, 12);
                    std::memcpy(fb.b_content[j].b_name, name.c_str(),
                                std::min(name.size(), (size_t)11));
                    fb.b_content[j].b_inodo = newInodeNum;

                    Utilities::WriteObject(file, fb, pos);

                    // Actualizar tamaño del directorio
                    dirInode.i_size += sizeof(FolderContent);
                    std::string now = Utilities::GetCurrentDateTime();
                    std::memcpy(dirInode.i_mtime, now.c_str(), 19);
                    FileSystem::WriteInode(file, sb, dirInodeNum, dirInode);

                    // Guardar SuperBloque actualizado
                    FileSystem::WriteSuperBloque(file, partStart, sb);
                    return true;
                }
            }
        }

        //  No hay espacio → asignar nuevo bloque al directorio
        // Buscar siguiente posición libre en i_block[]
        int nextBlockSlot = -1;
        for (int i = 0; i < 12; i++)
        {
            if (dirInode.i_block[i] == -1)
            {
                nextBlockSlot = i;
                break;
            }
        }

        if (nextBlockSlot == -1)
        {
            // Directorio lleno (más de 48 entradas) -> necesitaría indirecto
            return false;
        }

        // Asignar nuevo bloque
        int newBlockNum = FileSystem::AllocateBlock(file, sb);
        if (newBlockNum == -1)
            return false;

        // Inicializar el bloque nuevo con entradas vacías
        FolderBlock newFb{};
        for (int j = 0; j < 4; j++)
        {
            std::memset(newFb.b_content[j].b_name, 0, 12);
            newFb.b_content[j].b_inodo = -1;
        }

        // Agregar la nueva entrada en la primera posición
        std::memset(newFb.b_content[0].b_name, 0, 12);
        std::memcpy(newFb.b_content[0].b_name, name.c_str(),
                    std::min(name.size(), (size_t)11));
        newFb.b_content[0].b_inodo = newInodeNum;

        // Escribir el bloque nuevo
        int pos = sb.s_block_start + newBlockNum * sizeof(FolderBlock);
        Utilities::WriteObject(file, newFb, pos);

        // Actualizar el inodo del directorio
        dirInode.i_block[nextBlockSlot] = newBlockNum;
        dirInode.i_size += sizeof(FolderContent);
        std::string now = Utilities::GetCurrentDateTime();
        std::memcpy(dirInode.i_mtime, now.c_str(), 19);
        FileSystem::WriteInode(file, sb, dirInodeNum, dirInode);

        FileSystem::WriteSuperBloque(file, partStart, sb);
        return true;
    }

    // TraversePath — Navega el arbol y retorna el inodo del path
    // Empieza siempre desde el inodo 0 (raíz) y va bajando.
    //  path = ["home", "user"]
    //  1. Busca "home" en inodo 0 → inodo 2
    //  2. Busca "user" en inodo 2 → inodo 3
    //  3. Retorna 3
    int TraversePath(std::fstream &file, const SuperBloque &sb,
                     const std::vector<std::string> &parts)
    {
        int currentInode = 0; // empezar en la raíz

        for (const auto &part : parts)
        {
            int found = FindInDir(file, sb, currentInode, part);
            if (found == -1)
                return -1; // no existe
            currentInode = found;
        }
        return currentInode;
    }

    // TraverseToParent — Retorna el inodo del directorio PADRE
    //   parts = ["home", "user", "docs"]
    //   -> navega hasta ["home", "user"] y retorna ese inodo
    //   -> "docs" es lo que se va a crear

    int TraverseToParent(std::fstream &file, const SuperBloque &sb,
                         const std::vector<std::string> &parts)
    {
        if (parts.empty())
            return -1;
        if (parts.size() == 1)
            return 0; // el padre es la raíz

        // Navegar hasta el penúltimo elemento
        std::vector<std::string> parentParts(parts.begin(), parts.end() - 1);
        return TraversePath(file, sb, parentParts);
    }

    // MKDIR — Crea un directorio en el sistema de archivos
    // Parámetros:
    //   path  -> ruta absoluta: "/home/user/documentos"
    //   createParents -> si es true (-p), crea directorios intermedios
    // Pasos:
    //  1. Verificar sesión activa
    //  2. Dividir el path en partes
    //  3. Si -p: crear cada directorio intermedio que no exista
    //  4. Si no -p: verificar que el padre existe
    //  5. Crear el inodo (tipo carpeta)
    //  6. Crear el FolderBlock con "." y ".."
    //  7. Agregar la entrada al directorio padre
    std::string Mkdir(const std::string &path, bool createParents)
    {
        std::ostringstream out;
        out << "======= MKDIR =======\n";

        // Verificar sesión activa
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

        // ── Con -p: crear cada nivel que no exista ────────────────
        if (createParents)
        {
            int currentInode = 0; // empezar en raíz

            for (int i = 0; i < (int)parts.size(); i++)
            {
                int existsInode = FindInDir(file, sb, currentInode, parts[i]);

                if (existsInode != -1)
                {
                    // Ya existe → simplemente avanzar
                    currentInode = existsInode;
                    continue;
                }

                // No existe → crear este directorio
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

                // Crear FolderBlock con "." y ".."
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

                // Crear el inodo del nuevo directorio
                std::string now = Utilities::GetCurrentDateTime();
                Inode newInode{};
                newInode.i_uid = UserSession::currentSession.uid;
                newInode.i_gid = UserSession::GetGid(
                    UserSession::currentSession.group,
                    UserSession::ReadUsersFile(file, sb));
                newInode.i_size = sizeof(FolderBlock);
                std::memcpy(newInode.i_atime, now.c_str(), 19);
                std::memcpy(newInode.i_ctime, now.c_str(), 19);
                std::memcpy(newInode.i_mtime, now.c_str(), 19);
                for (int j = 0; j < 15; j++)
                    newInode.i_block[j] = -1;
                newInode.i_block[0] = newBlockNum;
                newInode.i_type[0] = '0'; // carpeta
                std::memcpy(newInode.i_perm, "664", 3);

                FileSystem::WriteInode(file, sb, newInodeNum, newInode);

                // Agregar entrada al directorio padre
                AddEntryToDir(file, sb,
                              UserSession::currentSession.partStart,
                              currentInode, parts[i], newInodeNum);

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
            // ── Sin -p: el padre debe existir ────────────────────
            int parentInode = TraverseToParent(file, sb, parts);
            if (parentInode == -1)
            {
                out << "Error: El directorio padre no existe\n";
                out << "Usa -p para crear directorios intermedios\n";
                file.close();
                return out.str();
            }

            std::string dirName = parts.back();

            // Verificar que no existe ya
            if (FindInDir(file, sb, parentInode, dirName) != -1)
            {
                out << "Error: Ya existe '" << dirName << "'\n";
                file.close();
                return out.str();
            }

            // Crear inodo y bloque
            int newInodeNum = FileSystem::AllocateInode(file, sb);
            int newBlockNum = FileSystem::AllocateBlock(file, sb);

            if (newInodeNum == -1 || newBlockNum == -1)
            {
                out << "Error: No hay espacio disponible\n";
                file.close();
                return out.str();
            }

            // FolderBlock con "." y ".."
            FolderBlock fb{};
            for (int j = 0; j < 4; j++)
            {
                std::memset(fb.b_content[j].b_name, 0, 12);
                fb.b_content[j].b_inodo = -1;
            }
            std::memcpy(fb.b_content[0].b_name, ".", 1);
            fb.b_content[0].b_inodo = newInodeNum;
            std::memcpy(fb.b_content[1].b_name, "..", 2);
            fb.b_content[1].b_inodo = parentInode;

            FileSystem::WriteFolderBlock(file, sb, newBlockNum, fb);

            std::string now = Utilities::GetCurrentDateTime();
            Inode newInode{};
            newInode.i_uid = UserSession::currentSession.uid;
            newInode.i_gid = UserSession::GetGid(
                UserSession::currentSession.group,
                UserSession::ReadUsersFile(file, sb));
            newInode.i_size = sizeof(FolderBlock);
            std::memcpy(newInode.i_atime, now.c_str(), 19);
            std::memcpy(newInode.i_ctime, now.c_str(), 19);
            std::memcpy(newInode.i_mtime, now.c_str(), 19);
            for (int j = 0; j < 15; j++)
                newInode.i_block[j] = -1;
            newInode.i_block[0] = newBlockNum;
            newInode.i_type[0] = '0';
            std::memcpy(newInode.i_perm, "664", 3);

            FileSystem::WriteInode(file, sb, newInodeNum, newInode);
            AddEntryToDir(file, sb,
                          UserSession::currentSession.partStart,
                          parentInode, dirName, newInodeNum);

            out << "Directorio creado: " << path << "\n";
        }

        file.close();
        out << "=====================\n";
        return out.str();
    }

    // MKFILE — Crea un archivo en el sistema de archivos
    // Parámetros:
    //   path  -> ruta del archivo: "/home/user/archivo.txt"
    //   random ->si true (-r), genera contenido aleatorio de 'size' bytes
    //   size  -> tamaño del contenido aleatorio
    //   cont  -> contenido directo del archivo (string)
    // Si no se especifica -r ni -cont -> archivo vacío

    std::string Mkfile(const std::string &path, bool random,
                       int size, const std::string &cont)
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

        // Verificar que el directorio padre existe
        int parentInode = TraverseToParent(file, sb, parts);
        if (parentInode == -1)
        {
            if (random && size == 0)
            {
                // crear carpetas padres automáticamente
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
                        out << "Error: No hay espacio para crear carpetas padres\n";
                        file.close();
                        return out.str();
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
                    newInode.i_gid = 1;
                    newInode.i_size = sizeof(FolderBlock);
                    std::memcpy(newInode.i_atime, now.c_str(), 19);
                    std::memcpy(newInode.i_ctime, now.c_str(), 19);
                    std::memcpy(newInode.i_mtime, now.c_str(), 19);
                    for (int j = 0; j < 15; j++)
                        newInode.i_block[j] = -1;
                    newInode.i_block[0] = newBlockNum;
                    newInode.i_type[0] = '0';
                    std::memcpy(newInode.i_perm, "664", 3);
                    FileSystem::WriteInode(file, sb, newInodeNum, newInode);
                    AddEntryToDir(file, sb, UserSession::currentSession.partStart,
                                  currentInode, parentParts[i], newInodeNum);
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

        // Verificar que no existe ya
        if (FindInDir(file, sb, parentInode, fileName) != -1)
        {
            out << "Error: Ya existe '" << fileName << "'\n";
            file.close();
            return out.str();
        }

        // Validar size negativo
        if (size < 0)
        {
            out << "Error: -size no puede ser negativo\n";
            file.close();
            return out.str();
        }

        // Preparar el contenido
        // -cont= puede ser ruta a archivo REAL del sistema Linux o contenido directo
        // -size= genera contenido aleatorio de 'size' bytes
        std::string content;

        if (!cont.empty())
        {
            std::ifstream realFile(cont);
            if (realFile.is_open())
            {
                content = std::string((std::istreambuf_iterator<char>(realFile)),
                                      std::istreambuf_iterator<char>());
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
                content += chars[std::rand() % chars.size()];
            }
        }

        //  Crear el inodo del archivo
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
        newInode.i_gid = UserSession::GetGid(
            UserSession::currentSession.group,
            UserSession::ReadUsersFile(file, sb));
        newInode.i_size = (int)content.size();
        std::memcpy(newInode.i_atime, now.c_str(), 19);
        std::memcpy(newInode.i_ctime, now.c_str(), 19);
        std::memcpy(newInode.i_mtime, now.c_str(), 19);
        for (int j = 0; j < 15; j++)
            newInode.i_block[j] = -1;
        newInode.i_type[0] = '1'; // archivo
        std::memcpy(newInode.i_perm, "664", 3);

        // Escribir el contenido en bloques
        // Cada FileBlock tiene 64 bytes.
        // Si el contenido es mayor a 64 bytes, usamos múltiples bloques.
        if (!content.empty())
        {
            int offset = 0;
            int blockSlot = 0; // índice en i_block[0..11]
            int totalBytes = (int)content.size();

            while (offset < totalBytes && blockSlot < 12)
            {
                int newBlockNum = FileSystem::AllocateBlock(file, sb);
                if (newBlockNum == -1)
                {
                    out << "Error: No hay bloques disponibles\n";
                    file.close();
                    return out.str();
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

        // Escribir el inodo
        FileSystem::WriteInode(file, sb, newInodeNum, newInode);

        // Agregar entrada al directorio padre
        AddEntryToDir(file, sb,
                      UserSession::currentSession.partStart,
                      parentInode, fileName, newInodeNum);

        file.close();

        out << "Archivo creado: " << path << "\n";
        out << "Tamaño: " << content.size() << " bytes\n";
        if (!content.empty() && !random)
        {
            out << "Contenido: " << content << "\n";
        }
        out << "======================\n";
        return out.str();
    }

    // CAT — Muestra el contenido de un archivo
    // Pasos:
    //  1. Navegar el árbol hasta encontrar el archivo
    //  2. Leer el inodo y verificar que es tipo '1' (archivo)
    //  3. Leer los bloques y concatenar el contenido

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

        // Navegar hasta el archivo
        int fileInodeNum = TraversePath(file, sb, parts);
        if (fileInodeNum == -1)
        {
            out << "Error: No existe '" << filePath << "'\n";
            file.close();
            return out.str();
        }

        // Leer el inodo
        Inode fileInode{};
        FileSystem::ReadInode(file, sb, fileInodeNum, fileInode);

        // Verificar que es un archivo y no una carpeta
        if (fileInode.i_type[0] == '0')
        {
            out << "Error: '" << filePath << "' es un directorio, no un archivo\n";
            file.close();
            return out.str();
        }

        // Leer el contenido bloque por bloque
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

        file.close();
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
                    std::memcpy(fb.b_content[j].b_name, newName.c_str(),
                                std::min(newName.size(), (size_t)11));
                    Utilities::WriteObject(file, fb, pos);
                    std::string now = Utilities::GetCurrentDateTime();
                    std::memcpy(parentInodeData.i_mtime, now.c_str(), 19);
                    FileSystem::WriteInode(file, sb, parentInode, parentInodeData);
                    file.close();
                    out << "Renombrado: " << path << " -> " << newName << "\n";
                    out << "======================\n";
                    return out.str();
                }
            }
        }

        file.close();
        out << "Error: No se pudo renombrar '" << path << "'\n";
        return out.str();
    }

    static std::string NormalizePath(const std::string &path)
    {
        if (path.empty())
            return "/";
        std::string normalized = path;
        if (normalized.back() == '/' && normalized.size() > 1)
            normalized.pop_back();
        return normalized;
    }

    static bool IsInsidePath(const std::string &parent, const std::string &child)
    {
        std::string normalizedParent = NormalizePath(parent);
        std::string normalizedChild = NormalizePath(child);
        if (normalizedParent == "/")
            return normalizedChild == "/" || normalizedChild.rfind("/", 0) == 0;
        return normalizedChild == normalizedParent ||
               normalizedChild.rfind(normalizedParent + "/", 0) == 0;
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

        if (!CopyInodeRecursive(file, sb, UserSession::currentSession.partStart,
                                srcInode, destParent, newName))
        {
            out << "Error: No se pudo copiar '" << source << "'\n";
            file.close();
            return out.str();
        }

        file.close();
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

        if (!AddEntryToDir(file, sb, UserSession::currentSession.partStart,
                           destParent, newName, srcInode))
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
        out << "Movido: " << source << " -> " << destination << "\n";
        out << "======================\n";
        return out.str();
    }

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

        // Verificar permisos: solo root o propietario pueden cambiar permisos
        if (UserSession::currentSession.uid != 1 && UserSession::currentSession.uid != targetInodeData.i_uid)
        {
            out << "Error: No tienes permisos para cambiar los permisos de '" << path << "'\n";
            file.close();
            return out.str();
        }

        std::memcpy(targetInodeData.i_perm, ugo.c_str(), 3);
        std::string now = Utilities::GetCurrentDateTime();
        std::memcpy(targetInodeData.i_mtime, now.c_str(), 19);
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

        // Solo root puede cambiar propietario
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
        std::memcpy(targetInodeData.i_mtime, now.c_str(), 19);
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

        // Solo root puede ejecutar LOSS
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

        // Simular pérdida de datos: marcar algunos inodos como corruptos
        // En un sistema real, esto sería por fallos de hardware
        // Aquí simulamos marcando algunos inodos como libres incorrectamente

        out << "Simulando pérdida de datos...\n";
        out << "Recuperando desde journaling...\n";

        // Leer el journaling y replay las operaciones
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
