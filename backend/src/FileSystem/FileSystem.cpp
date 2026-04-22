#include "FileSystem.h"
#include "../DiskManagement/DiskManagement.h"
#include "../UserSession/UserSession.h"
#include "../Utilities/Utilities.h"
#include "../Structs/Structs.h"
#include <cstring>
#include <ctime>
#include <sstream>
#include <vector>

namespace FileSystem
{

    // MKFS — Formatea una partición montada como EXT2
    //
    // Parámetros:
    //   id   -> ID de la partición montada
    //   type -> "full" o vacío (ambos hacen formateo completo en EXT2)
    //
    // Lo que hace:
    //   1. Buscar la partición en RAM por su ID
    //   2. Leer el MBR para saber Start y Size de la partición
    //   3. Calcular n (número de inodos y relación con bloques)
    //   4. Escribir todo con ceros (formateo limpio)
    //   5. Calcular y escribir el SuperBloque
    //   6. Inicializar bitmaps (todos en 0 = libres)
    //   7. Inicializar tabla de inodos y bloques (vacíos)
    //   8. Crear directorio raíz "/" (inodo 0, bloque 0)
    //   9. Crear archivo users.txt (inodo 1, bloque 1)

    std::string Mkfs(const std::string &id, const std::string &type, const std::string &fs)
    {
        std::ostringstream out;

        // Determinar tipo de filesystem
        int fsType = 2; // default EXT2
        if (fs == "3fs")
            fsType = 3; // EXT3

        out << "======= MKFS (Tipo: " << (fsType == 2 ? "EXT2" : "EXT3") << ") =======\n";

        // 1. Buscar la partición montada por ID
        // FindMountedById busca en el arreglo RAM de MOUNT
        std::string diskPath;
        int mountIdx = DiskManagement::FindMountedById(id, diskPath);

        if (mountIdx == -1)
        {
            out << "Error: No existe partición montada con ID '" << id << "'\n";
            out << "Usa MOUNT primero y luego MKFS\n";
            return out.str();
        }

        // 2. Leer el MBR para obtener Start y Size
        auto file = Utilities::OpenFile(diskPath);
        if (!file.is_open())
        {
            out << "Error: No se pudo abrir el disco: " << diskPath << "\n";
            return out.str();
        }

        MBR mbr{};
        Utilities::ReadObject(file, mbr, 0);

        // Buscar en las 4 particiones del MBR cuál tiene ese ID
        int partStart = -1;
        int partSize = -1;

        for (int i = 0; i < 4; i++)
        {
            std::string partId(mbr.Partitions[i].Id, 4);
            // Recortar chars nulos del final
            while (!partId.empty() && partId.back() == 0)
                partId.pop_back();
            if (partId == id)
            {
                partStart = mbr.Partitions[i].Start;
                partSize = mbr.Partitions[i].Size;
                break;
            }
        }

        if (partStart == -1)
        {
            out << "Error: No se encontró la partición con ID '" << id << "' en el MBR\n";
            file.close();
            return out.str();
        }

        //  3. Calcular n según el tipo de filesystem
        int n = 0;

        if (fsType == 2)
        {
            // EXT2: n = floor(partSize / (sizeof(SB) + 1 + 3 + 4*sizeof(Inode) + 3*sizeof(FolderBlock)))
            n = partSize / (sizeof(SuperBloque) + 4 +
                            4 * sizeof(Inode) +
                            3 * sizeof(FolderBlock));
        }
        else
        {
            // EXT3: n = floor(partSize / (sizeof(SB) + 50*sizeof(Journal) + 1 + 3 + 4*sizeof(Inode) + 3*sizeof(FolderBlock)))
            // con JOURNALING_SIZE = 50
            n = partSize / (sizeof(SuperBloque) +
                            FileSystem::JOURNALING_SIZE * sizeof(Journal) + 4 +
                            4 * sizeof(Inode) +
                            3 * sizeof(FolderBlock));
        }

        if (n < 2)
        {
            out << "Error: La partición es demasiado pequeña para " << (fsType == 2 ? "EXT2" : "EXT3") << "\n";
            file.close();
            return out.str();
        }

        out << "Calculando estructuras " << (fsType == 2 ? "EXT2" : "EXT3") << ":\n";
        out << "  n (inodos)    = " << n << "\n";
        out << "  Bloques       = " << 3 * n << "\n";

        // 4. Calcular posiciones absolutas en el disco
        int bm_inode_start = 0;
        int bm_block_start = 0;
        int inode_start = 0;
        int block_start = 0;
        int journal_start = 0;

        if (fsType == 2)
        {
            // EXT2 Layout:
            // [partStart + 0] → SuperBloque
            // [partStart + sizeof(SB)] → Bitmap Inodos (n bytes)
            // [partStart + sizeof(SB) + n] → Bitmap Bloques (3n bytes)
            // [partStart + sizeof(SB) + 4n] → Tabla de Inodos
            // [partStart + sizeof(SB) + 4n + n×sizeof(Inode)] → Tabla de Bloques
            bm_inode_start = partStart + sizeof(SuperBloque);
            bm_block_start = bm_inode_start + n;
            inode_start = bm_block_start + 3 * n;
            block_start = inode_start + n * sizeof(Inode);
        }
        else
        {
            // EXT3 Layout:
            // [partStart + 0] → SuperBloque
            // [partStart + sizeof(SB)] → Journaling (50 × sizeof(Journal))
            // [partStart + sizeof(SB) + 50*sizeof(Journal)] → Bitmap Inodos (n bytes)
            // [partStart + sizeof(SB) + 50*sizeof(Journal) + n] → Bitmap Bloques (3n bytes)
            // [...] → Tabla de Inodos
            // [...] → Tabla de Bloques
            journal_start = partStart + sizeof(SuperBloque);
            bm_inode_start = journal_start + FileSystem::JOURNALING_SIZE * sizeof(Journal);
            bm_block_start = bm_inode_start + n;
            inode_start = bm_block_start + 3 * n;
            block_start = inode_start + n * sizeof(Inode);
        }

        //  5. Construir el SuperBloque
        SuperBloque sb{};
        sb.s_filesystem_type = fsType; // 2 = EXT2, 3 = EXT3
        sb.s_inodes_count = n;
        sb.s_blocks_count = 3 * n;
        sb.s_free_inodes_count = n - 2;     // inodo 0 (root) e inodo 1 (users.txt) ya usados
        sb.s_free_blocks_count = 3 * n - 2; // bloque 0 (FolderBlock root) y 1 (FileBlock users) usados
        sb.s_mtime = (int)time(nullptr);
        sb.s_umtime = 0;
        sb.s_mnt_count = 1;
        sb.s_magic = 0xEF53; // número mágico EXT2/EXT3
        sb.s_inode_size = sizeof(Inode);
        sb.s_block_size = sizeof(FolderBlock); // = 64 bytes
        sb.s_first_ino = 2;                    // próximo inodo libre (0 y 1 ya usados)
        sb.s_first_blo = 2;                    // próximo bloque libre
        sb.s_bm_inode_start = bm_inode_start;
        sb.s_bm_block_start = bm_block_start;
        sb.s_inode_start = inode_start;
        sb.s_block_start = block_start;

        // Escribir SuperBloque al inicio de la partición
        Utilities::WriteObject(file, sb, partStart);

        // 5.5 Para EXT3: Inicializar Journaling (todas las entradas vacías)
        if (fsType == 3)
        {
            for (int i = 0; i < FileSystem::JOURNALING_SIZE; i++)
            {
                Journal j_vacio{};
                j_vacio.j_count = i;
                std::memset(&j_vacio.j_content, 0, sizeof(Information)); // Limpiamos con ceros

                Utilities::WriteObject(file, j_vacio, journal_start + (i * sizeof(Journal)));
            }
            out << "Journaling inicializado (" << FileSystem::JOURNALING_SIZE << " entradas vacías)\n";
        }

        //  6. Inicializar Bitmap de Inodos (todo en 0 = libres)
        {
            char zeroByte = 0;
            for (int i = 0; i < n; i++)
            {
                file.seekp(bm_inode_start + i);
                file.write(&zeroByte, 1);
            }
        }

        //  7. Inicializar Bitmap de Bloques (todo en 0 = libres)
        {
            char zeroByte = 0;
            for (int i = 0; i < 3 * n; i++)
            {
                file.seekp(bm_block_start + i);
                file.write(&zeroByte, 1);
            }
        }

        //  8. Inicializar Tabla de Inodos (todos vacíos)
        {
            Inode emptyInode{};
            for (int i = 0; i < 15; i++)
                emptyInode.i_block[i] = -1;
            emptyInode.i_uid = -1;
            emptyInode.i_gid = -1;
            emptyInode.i_size = 0;

            for (int i = 0; i < n; i++)
            {
                Utilities::WriteObject(file, emptyInode,
                                       inode_start + i * sizeof(Inode));
            }
        }

        //  9. Inicializar Tabla de Bloques (todos vacíos)
        {
            FolderBlock emptyBlock{};
            for (int i = 0; i < 4; i++)
            {
                std::memset(emptyBlock.b_content[i].b_name, 0, 12);
                emptyBlock.b_content[i].b_inodo = -1;
            }

            for (int i = 0; i < 3 * n; i++)
            {
                Utilities::WriteObject(file, emptyBlock,
                                       block_start + i * sizeof(FolderBlock));
            }
        }

        // 10. CREAR DIRECTORIO RAÍZ "/"
        //
        // El directorio raíz siempre es el INODO 0 y usa el BLOQUE 0.
        // Su FolderBlock tiene estas entradas obligatorias:
        //   [0] "."        -> inodo 0 (apunta a sí mismo)
        //   [1] ".."       -> inodo 0 (su padre es él mismo, es la raíz)
        //   [2] "users.txt"-> inodo 1 (el archivo de usuarios)
        //   [3] vacío      -> -1

        // Marcar inodo 0 como ocupado en el bitmap
        {
            char usedByte;
            file.seekg(bm_inode_start);
            file.read(&usedByte, 1);
            usedByte |= (1 << 7); // bit 7 = inodo 0 (el bit más significativo)
            file.seekp(bm_inode_start);
            file.write(&usedByte, 1);
        }

        // Marcar bloque 0 como ocupado en el bitmap
        {
            char usedByte;
            file.seekg(bm_block_start);
            file.read(&usedByte, 1);
            usedByte |= (1 << 7); // bit 7 = bloque 0
            file.seekp(bm_block_start);
            file.write(&usedByte, 1);
        }

        // Crear el FolderBlock del directorio raíz
        FolderBlock rootBlock{};

        // Entrada "." → apunta al propio directorio (inodo 0)
        std::memset(rootBlock.b_content[0].b_name, 0, 12);
        std::memcpy(rootBlock.b_content[0].b_name, ".", 1);
        rootBlock.b_content[0].b_inodo = 0;

        // Entrada ".." → padre (en raíz, apunta a sí mismo)
        std::memset(rootBlock.b_content[1].b_name, 0, 12);
        std::memcpy(rootBlock.b_content[1].b_name, "..", 2);
        rootBlock.b_content[1].b_inodo = 0;

        // Entrada "users.txt" → inodo 1
        std::memset(rootBlock.b_content[2].b_name, 0, 12);
        std::memcpy(rootBlock.b_content[2].b_name, "users.txt", 9);
        rootBlock.b_content[2].b_inodo = 1;

        // Entrada vacía
        std::memset(rootBlock.b_content[3].b_name, 0, 12);
        rootBlock.b_content[3].b_inodo = -1;

        // Escribir bloque 0 (FolderBlock del root)
        Utilities::WriteObject(file, rootBlock, block_start + 0 * sizeof(FolderBlock));

        // Crear Inodo 0 (directorio raíz)
        std::string now = Utilities::GetCurrentDateTime();
        Inode rootInode{};
        rootInode.i_uid = 1; // uid de root
        rootInode.i_gid = 1; // gid de root
        rootInode.i_size = sizeof(FolderBlock);
        std::memcpy(rootInode.i_atime, now.c_str(), 19);
        std::memcpy(rootInode.i_ctime, now.c_str(), 19);
        std::memcpy(rootInode.i_mtime, now.c_str(), 19);
        for (int i = 0; i < 15; i++)
            rootInode.i_block[i] = -1;
        rootInode.i_block[0] = 0;  // usa el bloque 0
        rootInode.i_type[0] = '0'; // '0' = carpeta
        std::memcpy(rootInode.i_perm, "664", 3);

        // Escribir inodo 0
        Utilities::WriteObject(file, rootInode, inode_start + 0 * sizeof(Inode));

        // 11. CREAR ARCHIVO users.txt (INODO 1, BLOQUE 1)
        // users.txt es el archivo de usuarios del sistema.
        // Formato de cada línea:
        //   tipo,GID,nombre_grupo        -> para grupos
        //   tipo,UID,usuario,grupo,pass  -> para usuarios
        //
        // Siempre se crea con el usuario root:
        //   1,G,root        -> grupo root con GID 1
        //   1,U,root,root,123 -> usuario root, en grupo root, pass "123"

        // Marcar inodo 1 en bitmap
        {
            char usedByte;
            file.seekg(bm_inode_start);
            file.read(&usedByte, 1);
            usedByte |= (1 << 6); // bit 6 = inodo 1
            file.seekp(bm_inode_start);
            file.write(&usedByte, 1);
        }

        // Marcar bloque 1 en bitmap
        {
            char usedByte;
            file.seekg(bm_block_start);
            file.read(&usedByte, 1);
            usedByte |= (1 << 6); // bit 6 = bloque 1
            file.seekp(bm_block_start);
            file.write(&usedByte, 1);
        }

        // Contenido del archivo users.txt
        std::string usersContent = "1,G,root\n1,U,root,root,123\n";

        FileBlock usersBlock{};
        std::memset(usersBlock.b_content, 0, 64);
        std::memcpy(usersBlock.b_content, usersContent.c_str(),
                    std::min(usersContent.size(), (size_t)64));

        // Escribir bloque 1 (FileBlock de users.txt)
        Utilities::WriteObject(file, usersBlock,
                               block_start + 1 * sizeof(FolderBlock));

        // Crear Inodo 1 (archivo users.txt)
        Inode usersInode{};
        usersInode.i_uid = 1;
        usersInode.i_gid = 1;
        usersInode.i_size = (int)usersContent.size();
        std::memcpy(usersInode.i_atime, now.c_str(), 19);
        std::memcpy(usersInode.i_ctime, now.c_str(), 19);
        std::memcpy(usersInode.i_mtime, now.c_str(), 19);
        for (int i = 0; i < 15; i++)
            usersInode.i_block[i] = -1;
        usersInode.i_block[0] = 1;  // usa el bloque 1
        usersInode.i_type[0] = '1'; // '1' = archivo
        std::memcpy(usersInode.i_perm, "664", 3);

        // Escribir inodo 1
        Utilities::WriteObject(file, usersInode, inode_start + 1 * sizeof(Inode));

        file.close();

        out << "SuperBloque escrito en byte " << partStart << "\n";
        if (fsType == 3)
            out << "Journaling escrito en byte " << journal_start << "\n";
        out << "Bitmap inodos: " << bm_inode_start << "\n";
        out << "Bitmap bloques: " << bm_block_start << "\n";
        out << "Tabla inodos: " << inode_start << "\n";
        out << "Tabla bloques: " << block_start << "\n";
        out << "Directorio raíz '/' creado (inodo 0, bloque 0)\n";
        out << "Archivo users.txt creado (inodo 1, bloque 1)\n";
        out << "Contenido users.txt:\n"
            << usersContent;
        out << "=====================\n";
        return out.str();
    }

    // AllocateInode
    // Busca el próximo inodo libre en el bitmap y lo marca como usado.
    // El bitmap tiene 1 byte por 8 inodos. Bit = 1 -> ocupado.
    //
    // Ejemplo con n=16 inodos (2 bytes de bitmap):
    //   Byte 0: 11000000 -> inodos 0 y 1 usados, 2-7 libres
    //   Byte 1: 00000000 -> inodos 8-15 todos libres
    //
    // Para el inodo 2 (libre): byte=0, bit=5 (de más signif. a menos)

    int AllocateInode(std::fstream &file, SuperBloque &sb)
    {
        if (sb.s_free_inodes_count <= 0)
            return -1;

        for (int i = 0; i < sb.s_inodes_count; i++)
        {
            int bytePos = i / 8;      // qué byte del bitmap
            int bitPos = 7 - (i % 8); // qué bit dentro del byte (MSB primero)

            char byteVal;
            file.seekg(sb.s_bm_inode_start + bytePos);
            file.read(&byteVal, 1);

            if (!((byteVal >> bitPos) & 1))
            {
                // Este bit es 0 -> inodo libre -> marcarlo como usado
                byteVal |= (1 << bitPos);
                file.seekp(sb.s_bm_inode_start + bytePos);
                file.write(&byteVal, 1);
                file.flush();

                // Actualizar contadores del SuperBloque
                sb.s_free_inodes_count--;
                sb.s_first_ino = i + 1;

                return i; // retorna el número del inodo asignado
            }
        }
        return -1; // no hay inodos libres
    }

    // AllocateBlock — igual que AllocateInode pero para bloques
    int AllocateBlock(std::fstream &file, SuperBloque &sb)
    {
        if (sb.s_free_blocks_count <= 0)
            return -1;

        for (int i = 0; i < sb.s_blocks_count; i++)
        {
            int bytePos = i / 8;
            int bitPos = 7 - (i % 8);

            char byteVal;
            file.seekg(sb.s_bm_block_start + bytePos);
            file.read(&byteVal, 1);

            if (!((byteVal >> bitPos) & 1))
            {
                byteVal |= (1 << bitPos);
                file.seekp(sb.s_bm_block_start + bytePos);
                file.write(&byteVal, 1);
                file.flush();

                sb.s_free_blocks_count--;
                sb.s_first_blo = i + 1;

                return i;
            }
        }
        return -1;
    }

    // WriteInode — Escribe un inodo en la posición correcta
    //
    // Posición = inicio_tabla_inodos + (inodeNum × sizeof(Inode))

    void WriteInode(std::fstream &file, const SuperBloque &sb,
                    int inodeNum, const Inode &inode)
    {
        int pos = sb.s_inode_start + inodeNum * sizeof(Inode);
        Utilities::WriteObject(file, inode, pos);
    }

    void ReadInode(std::fstream &file, const SuperBloque &sb,
                   int inodeNum, Inode &inode)
    {
        int pos = sb.s_inode_start + inodeNum * sizeof(Inode);
        Utilities::ReadObject(file, inode, pos);
    }

    // WriteFolderBlock / WriteFileBlock
    //
    // Todos los bloques (carpeta, archivo, apuntador) tienen 64 bytes.
    // La posición en disco es la misma fórmula independiente del tipo:
    //   pos = inicio_tabla_bloques + (blockNum × 64)

    void WriteFolderBlock(std::fstream &file, const SuperBloque &sb,
                          int blockNum, const FolderBlock &block)
    {
        int pos = sb.s_block_start + blockNum * sizeof(FolderBlock);
        Utilities::WriteObject(file, block, pos);
    }

    void WriteFileBlock(std::fstream &file, const SuperBloque &sb,
                        int blockNum, const FileBlock &block)
    {
        // FileBlock y FolderBlock tienen el mismo tamaño (64 bytes)
        // así que el offset es el mismo
        int pos = sb.s_block_start + blockNum * sizeof(FolderBlock);
        Utilities::WriteObject(file, block, pos);
    }

    // ReadSuperBloque / WriteSuperBloque
    // El SuperBloque siempre está al inicio de la partición.
    void ReadSuperBloque(std::fstream &file, int partStart, SuperBloque &sb)
    {
        Utilities::ReadObject(file, sb, partStart);
    }

    void WriteSuperBloque(std::fstream &file, int partStart, const SuperBloque &sb)
    {
        Utilities::WriteObject(file, sb, partStart);
    }

    // Función para registrar operaciones en el Journal
    void RegisterInJournal(const std::string &op, const std::string &path, const std::string &content)
    {
        if (!UserSession::currentSession.active)
            return;

        auto file = Utilities::OpenFile(UserSession ::currentSession.diskPath);
        if (!file.is_open())
            return;

        SuperBloque sb;
        ReadSuperBloque(file, UserSession ::currentSession.partStart, sb);

        // Si NO es EXT3, salimos para no afectar el EXT2
        if (sb.s_filesystem_type != 3)
        {
            file.close();
            return;
        }

        // El Journal empieza justo después del SuperBloque
        int posJournal = UserSession ::currentSession.partStart + sizeof(SuperBloque);

        // Buscamos la primera de las 50 posiciones que esté libre
        for (int i = 0; i < 50; i++)
        {
            Journal j_actual;
            Utilities::ReadObject(file, j_actual, posJournal + (i * sizeof(Journal)));

            // Si la operación está vacía (empieza con \0), aquí guardamos el registro
            if (j_actual.j_content.i_operation[0] == '\0')
            {

                // Llenamos la información asegurándonos de no desbordar los arrays
                std::strncpy(j_actual.j_content.i_operation, op.c_str(), 9);
                std::strncpy(j_actual.j_content.i_path, path.c_str(), 31);
                std::strncpy(j_actual.j_content.i_content, content.c_str(), 63);

                // Guardamos la fecha como Float (Unix Timestamp)
                j_actual.j_content.i_date = static_cast<float>(std::time(nullptr));

                // Escribimos este Journal de vuelta al disco
                Utilities::WriteObject(file, j_actual, posJournal + (i * sizeof(Journal)));
                break;
            }
        }

        file.close();
    }

    int GetInodeFromPath(std::fstream &file, const SuperBloque &sb, std::string path)
    {
        if (path == "/" || path == "")
            return 0; // Raíz siempre es inodo 0

        std::vector<std::string> steps;
        std::stringstream ss(path);
        std::string segment;
        while (std::getline(ss, segment, '/'))
        {
            if (!segment.empty())
                steps.push_back(segment);
        }

        int currentInodeNum = 0; // Empezamos en la raíz
        for (const auto &step : steps)
        {
            Inode inode;
            ReadInode(file, sb, currentInodeNum, inode);

            if (inode.i_type[0] != '0')
                return -1; // No es carpeta, no podemos seguir

            bool found = false;
            // Buscamos en los bloques de la carpeta el nombre del siguiente paso
            for (int i = 0; i < 15; i++)
            {
                if (inode.i_block[i] == -1)
                    continue;

                FolderBlock fb;
                Utilities::ReadObject(file, fb, sb.s_block_start + inode.i_block[i] * sizeof(FolderBlock));

                for (int j = 0; j < 4; j++)
                {
                    if (fb.b_content[j].b_inodo != -1 && std::string(fb.b_content[j].b_name) == step)
                    {
                        currentInodeNum = fb.b_content[j].b_inodo;
                        found = true;
                        break;
                    }
                }
                if (found)
                    break;
            }
            if (!found)
                return -1; // Carpeta o archivo no encontrado
        }
        return currentInodeNum;
    }

    //  OBTIENE LA LISTA DE ARCHIVOS DE UNA CARPETA -
    std::vector<FileEntry> GetDirectoryContent(const std::string &path)
    {
        std::vector<FileEntry> entries;
        if (!UserSession::currentSession.active)
            return entries;

        auto file = Utilities::OpenFile(UserSession::currentSession.diskPath);
        if (!file.is_open())
            return entries;

        SuperBloque sb;
        ReadSuperBloque(file, UserSession::currentSession.partStart, sb);

        int inodeNum = GetInodeFromPath(file, sb, path);
        if (inodeNum == -1)
        {
            file.close();
            return entries;
        }

        Inode dirInode;
        ReadInode(file, sb, inodeNum, dirInode);

        if (dirInode.i_type[0] != '0')
        {
            file.close();
            return entries;
        }

        // Recorremos los bloques de la carpeta para listar sus hijos
        for (int i = 0; i < 15; i++)
        {
            if (dirInode.i_block[i] == -1)
                continue;

            FolderBlock fb;
            Utilities::ReadObject(file, fb, sb.s_block_start + dirInode.i_block[i] * sizeof(FolderBlock));

            for (int j = 0; j < 4; j++)
            {
                if (fb.b_content[j].b_inodo == -1)
                    continue;
                std::string name = fb.b_content[j].b_name;
                if (name == "." || name == "..")
                    continue; // Omitir navegación relativa

                Inode childInode;
                ReadInode(file, sb, fb.b_content[j].b_inodo, childInode);

                FileEntry entry;
                entry.name = name;
                entry.type = childInode.i_type[0]; // '0' carpeta, '1' archivo
                entry.size = childInode.i_size;
                entry.date = childInode.i_mtime;
                entry.perm = std::string(childInode.i_perm, 3);
                entry.owner = "root"; // Se puede buscar el nombre real en users.txt
                entries.push_back(entry);
            }
        }
        file.close();
        return entries;
    }

    // OBTIENE EL CONTENIDO DE UN ARCHIVO
    std::string GetFileContent(const std::string &path)
    {
        if (!UserSession::currentSession.active)
            return "Error: No hay sesión activa";

        auto file = Utilities::OpenFile(UserSession::currentSession.diskPath);
        if (!file.is_open())
            return "Error: Disco no accesible";

        SuperBloque sb;
        ReadSuperBloque(file, UserSession::currentSession.partStart, sb);

        int inodeNum = GetInodeFromPath(file, sb, path);
        if (inodeNum == -1)
            return "Archivo no encontrado";

        Inode inode;
        ReadInode(file, sb, inodeNum, inode);
        if (inode.i_type[0] != '1')
            return "Error: No es un archivo de texto";

        std::string content = "";
        int bytesLeft = inode.i_size;

        for (int i = 0; i < 15; i++)
        {
            if (inode.i_block[i] == -1 || bytesLeft <= 0)
                break;

            FileBlock fb;
            Utilities::ReadObject(file, fb, sb.s_block_start + inode.i_block[i] * sizeof(FolderBlock));

            int toRead = std::min(bytesLeft, 64);
            content.append(fb.b_content, toRead);
            bytesLeft -= toRead;
        }
        file.close();
        return content;
    }

}