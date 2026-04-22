#include "FileOperationsCore.h"
#include "../FileSystem/FileSystem.h"
#include "../UserSession/UserSession.h"
#include "../Utilities/Utilities.h"
#include <algorithm>
#include <cstring>
#include <sstream>

namespace FileOperations
{

    // SplitPath — Divide un path en sus partes
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

    bool IsDirectory(const Inode &inode)
    {
        return inode.i_type[0] == '0';
    }

    bool IsFile(const Inode &inode)
    {
        return inode.i_type[0] == '1';
    }

    bool FreeInodeBlocks(std::fstream &file, SuperBloque &sb, int partStart, int inodeNum, const Inode &inode)
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

    bool RemoveDirEntry(std::fstream &file, SuperBloque &sb, int dirInodeNum, const std::string &name)
    {
        Inode dirInode{};
        FileSystem::ReadInode(file, sb, dirInodeNum, dirInode);
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

    bool RemoveInodeRecursive(std::fstream &file, SuperBloque &sb, int partStart, int inodeNum)
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
                Utilities::ReadObject(file, fb, sb.s_block_start + inode.i_block[i] * sizeof(FolderBlock));
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

    bool CopyInodeRecursive(std::fstream &file, SuperBloque &sb, int partStart, int srcInodeNum, int destParentInodeNum, const std::string &name)
    {
        Inode srcInode{};
        FileSystem::ReadInode(file, sb, srcInodeNum, srcInode);
        int newInodeNum = FileSystem::AllocateInode(file, sb);
        if (newInodeNum == -1)
            return false;
        Inode newInode = srcInode;
        std::string now = Utilities::GetCurrentDateTime();
        newInode.i_uid = UserSession::currentSession.uid;
        newInode.i_gid = UserSession::GetGid(UserSession::currentSession.group, UserSession::ReadUsersFile(file, sb));
        newInode.i_size = srcInode.i_size;
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
            if (!AddEntryToDir(file, sb, partStart, destParentInodeNum, name, newInodeNum))
            {
                FreeInodeBlocks(file, sb, partStart, newInodeNum, newInode);
                return false;
            }
            for (int i = 0; i < 12; i++)
            {
                if (srcInode.i_block[i] == -1)
                    break;
                FolderBlock srcFb{};
                Utilities::ReadObject(file, srcFb, sb.s_block_start + srcInode.i_block[i] * sizeof(FolderBlock));
                for (int j = 0; j < 4; j++)
                {
                    if (srcFb.b_content[j].b_inodo == -1)
                        continue;
                    std::string childName(srcFb.b_content[j].b_name);
                    if (childName == "." || childName == "..")
                        continue;
                    if (!CopyInodeRecursive(file, sb, partStart, srcFb.b_content[j].b_inodo, newInodeNum, childName))
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
            Utilities::ReadObject(file, srcFb, sb.s_block_start + srcBlockNum * sizeof(FolderBlock));
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
        if (!AddEntryToDir(file, sb, partStart, destParentInodeNum, name, newInodeNum))
        {
            FreeInodeBlocks(file, sb, partStart, newInodeNum, newInode);
            return false;
        }
        return true;
    }

    bool UpdateDirectoryParent(std::fstream &file, const SuperBloque &sb, int dirInodeNum, int newParentInodeNum)
    {
        Inode dirInode{};
        FileSystem::ReadInode(file, sb, dirInodeNum, dirInode);
        if (!IsDirectory(dirInode))
            return false;
        if (dirInode.i_block[0] == -1)
            return false;
        FolderBlock fb{};
        Utilities::ReadObject(file, fb, sb.s_block_start + dirInode.i_block[0] * sizeof(FolderBlock));
        std::memcpy(fb.b_content[1].b_name, "..", 2);
        fb.b_content[1].b_inodo = newParentInodeNum;
        FileSystem::WriteFolderBlock(file, sb, dirInode.i_block[0], fb);
        return true;
    }

    int FindInDir(std::fstream &file, const SuperBloque &sb, int dirInodeNum, const std::string &name)
    {
        Inode dirInode{};
        FileSystem::ReadInode(file, sb, dirInodeNum, dirInode);
        for (int i = 0; i < 12; i++)
        {
            if (dirInode.i_block[i] == -1)
                break; // Ojo: a veces los bloques no están en orden, podría ser mejor 'continue' si en el futuro borras bloques.

            FolderBlock fb{};
            int pos = sb.s_block_start + dirInode.i_block[i] * sizeof(FolderBlock);
            Utilities::ReadObject(file, fb, pos);

            for (int j = 0; j < 4; j++)
            {
                if (fb.b_content[j].b_inodo == -1)
                    continue;

                // === LA SOLUCIÓN MÁGICA AQUÍ ===
                // Calculamos el tamaño real (máximo 12) para evitar leer basura de memoria
                size_t nameLen = strnlen(fb.b_content[j].b_name, 12);
                std::string entryName(fb.b_content[j].b_name, nameLen);

                if (entryName == name)
                {
                    return fb.b_content[j].b_inodo;
                }
            }
        }
        return -1;
    }

    bool AddEntryToDir(std::fstream &file, SuperBloque &sb, int partStart, int dirInodeNum, const std::string &name, int newInodeNum)
    {
        Inode dirInode{};
        FileSystem::ReadInode(file, sb, dirInodeNum, dirInode);
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
                    std::memset(fb.b_content[j].b_name, 0, 12);
                    std::memcpy(fb.b_content[j].b_name, name.c_str(), std::min(name.size(), (size_t)12));
                    fb.b_content[j].b_inodo = newInodeNum;
                    Utilities::WriteObject(file, fb, pos);
                    dirInode.i_size += sizeof(FolderContent);
                    std::string now = Utilities::GetCurrentDateTime();
                    std::memcpy(dirInode.i_mtime, now.c_str(), sizeof(dirInode.i_mtime));
                    FileSystem::WriteInode(file, sb, dirInodeNum, dirInode);
                    FileSystem::WriteSuperBloque(file, partStart, sb);
                    return true;
                }
            }
        }
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
            return false;
        int newBlockNum = FileSystem::AllocateBlock(file, sb);
        if (newBlockNum == -1)
            return false;
        FolderBlock newFb{};
        for (int j = 0; j < 4; j++)
        {
            std::memset(newFb.b_content[j].b_name, 0, 12);
            newFb.b_content[j].b_inodo = -1;
        }
        std::memset(newFb.b_content[0].b_name, 0, 12);
        std::memcpy(newFb.b_content[0].b_name, name.c_str(), std::min(name.size(), (size_t)12));
        newFb.b_content[0].b_inodo = newInodeNum;
        int pos = sb.s_block_start + newBlockNum * sizeof(FolderBlock);
        Utilities::WriteObject(file, newFb, pos);
        dirInode.i_block[nextBlockSlot] = newBlockNum;
        dirInode.i_size += sizeof(FolderContent);
        std::string now = Utilities::GetCurrentDateTime();
        std::memcpy(dirInode.i_mtime, now.c_str(), sizeof(dirInode.i_mtime));
        FileSystem::WriteInode(file, sb, dirInodeNum, dirInode);
        FileSystem::WriteSuperBloque(file, partStart, sb);
        return true;
    }

    int TraversePath(std::fstream &file, const SuperBloque &sb, const std::vector<std::string> &parts)
    {
        int currentInode = 0;
        for (const auto &part : parts)
        {
            int found = FindInDir(file, sb, currentInode, part);
            if (found == -1)
                return -1;
            currentInode = found;
        }
        return currentInode;
    }

    int TraverseToParent(std::fstream &file, const SuperBloque &sb, const std::vector<std::string> &parts)
    {
        if (parts.empty())
            return -1;
        if (parts.size() == 1)
            return 0;
        std::vector<std::string> parentParts(parts.begin(), parts.end() - 1);
        return TraversePath(file, sb, parentParts);
    }

    std::string NormalizePath(const std::string &path)
    {
        if (path.empty())
            return "/";
        std::string normalized = path;
        if (normalized.back() == '/' && normalized.size() > 1)
            normalized.pop_back();
        return normalized;
    }

    bool IsInsidePath(const std::string &parent, const std::string &child)
    {
        std::string normalizedParent = NormalizePath(parent);
        std::string normalizedChild = NormalizePath(child);
        if (normalizedParent == "/")
            return normalizedChild == "/" || normalizedChild.rfind("/", 0) == 0;
        return normalizedChild == normalizedParent || normalizedChild.rfind(normalizedParent + "/", 0) == 0;
    }

}