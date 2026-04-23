#include "DiskManagement.h"
#include "../Utilities/Utilities.h"
#include "../Structs/Structs.h"
#include <filesystem>
#include <cstring>
#include <cstdlib>
#include <vector>
#include <sstream>
#include <algorithm>

namespace DiskManagement
{

    // Estado global en RAM (no persiste entre reinicios del backend)
    // mountedPartitions[] -> arreglo de particiones actualmente montadas
    // mountedCount        -> cuántas hay en el arreglo
    // globalCorrelative   -> número que sube con cada MOUNT (1, 2, 3...)

    static MountedPartition mountedPartitions[100];
    static int mountedCount = 0;
    static int globalCorrelative = 1;

    // MKDISK — Crea un disco virtual .mia
    // Pasos:
    //  1. Validar parámetros
    //  2. Calcular tamaño en bytes según -unit
    //  3. Crear archivo vacío
    //  4. Llenar con ceros (buffer de 1024 para eficiencia)
    //  5. Construir MBR y escribirlo en el byte 0

    std::string Mkdisk(int size, const std::string &fit,
                       const std::string &unit, const std::string &path)
    {
        std::ostringstream out;
        out << "======= MKDISK =======\n";

        // Normalizar a minúsculas para comparar sin importar cómo lo escribió el usuario
        std::string f = fit, u = unit;
        for (auto &c : f)
            c = tolower(c);
        for (auto &c : u)
            c = tolower(c);

        //  Validaciones
        if (size <= 0)
        {
            out << "Error: -size debe ser mayor a 0\n";
            return out.str();
        }
        if (f != "bf" && f != "ff" && f != "wf")
        {
            out << "Error: -fit debe ser 'bf', 'ff' o 'wf'\n";
            return out.str();
        }
        if (u != "k" && u != "m")
        {
            out << "Error: -unit debe ser 'k' o 'm'\n";
            return out.str();
        }
        if (path.empty())
        {
            out << "Error: -path es obligatorio\n";
            return out.str();
        }

        //  Convertir a bytes
        int sizeBytes = size;
        if (u == "k")
            sizeBytes *= 1024;
        else
            sizeBytes *= 1024 * 1024;

        //  Crear el archivo vacío
        if (!Utilities::CreateFile(path))
        {
            out << "Error: No se pudo crear el archivo en: " << path << "\n";
            return out.str();
        }

        auto file = Utilities::OpenFile(path);
        if (!file.is_open())
        {
            out << "Error: No se pudo abrir el archivo\n";
            return out.str();
        }

        //  Llenar con ceros
        // Usamos buffer de 1024 bytes -> mucho más rápido que byte a byte.
        std::vector<char> zeros(1024, 0);
        int chunks = sizeBytes / 1024;
        int remainder = sizeBytes % 1024;

        for (int i = 0; i < chunks; i++)
        {
            file.seekp(i * 1024);
            file.write(zeros.data(), 1024);
        }
        if (remainder > 0)
        {
            file.seekp(chunks * 1024);
            file.write(zeros.data(), remainder);
        }

        // Construir y escribir el MBR
        MBR mbr{};
        mbr.MbrSize = sizeBytes;
        mbr.Signature = rand(); // número random para identificar el disco

        std::string date = Utilities::GetCurrentDateTime();
        std::memcpy(mbr.CreationDate, date.c_str(), 19);
        std::memcpy(mbr.Fit, f.c_str(), 2);

        // Inicializar las 4 ranuras como vacías
        for (int i = 0; i < 4; i++)
        {
            mbr.Partitions[i].Start = -1;
            mbr.Partitions[i].Size = -1;
            mbr.Partitions[i].Correlative = -1;
            mbr.Partitions[i].Status[0] = '0';
        }

        // Escribir MBR en el byte 0
        Utilities::WriteObject(file, mbr, 0);
        file.close();

        out << "Disco creado: " << path << "\n";
        out << "Tamaño: " << sizeBytes << " bytes\n";
        out << "Fit: " << f << "\n";
        out << "======================\n";
        return out.str();
    }

    // RMDISK — Elimina el archivo .mia del sistema
    std::string Rmdisk(const std::string &path)
    {
        std::ostringstream out;
        out << "======= RMDISK =======\n";

        if (!std::filesystem::exists(path))
        {
            out << "Error: No existe el disco: " << path << "\n";
            return out.str();
        }

        std::filesystem::remove(path);
        out << "Disco eliminado: " << path << "\n";
        out << "======================\n";
        return out.str();
    }

    // FDISK — Crea una partición dentro de un disco existente
    // Pasos:
    //  1. Validar parámetros
    //  2. Leer el MBR del disco
    //  3. Verificar restricciones (máx 4 particiones, 1 extendida)
    //  4. Calcular byte de inicio (justo después de la última partición)
    //  5. Llenar la ranura libre y escribir MBR actualizado

    std::string Fdisk(int size, const std::string &path, const std::string &name,
                      const std::string &type, const std::string &fit,
                      const std::string &unit)
    {
        std::ostringstream out;
        out << "======= FDISK =======\n";

        std::string t = type, f = fit, u = unit;
        for (auto &c : t)
            c = tolower(c);
        for (auto &c : f)
            c = tolower(c);
        for (auto &c : u)
            c = tolower(c);

        //  Validaciones
        if (size <= 0)
        {
            out << "Error: -size debe ser mayor a 0\n";
            return out.str();
        }
        if (t == "p" || t == "P")
            t = "p";
        else if (t == "e" || t == "E")
            t = "e";
        else if (t == "l" || t == "L")
            t = "l";
        else
        {
            out << "Error: -type debe ser 'p', 'e' o 'l'\n";
            return out.str();
        }

        // Normalizar fit: acepta una letra o dos (BF, FF, WF)
        if (f == "bf")
            f = "b";
        else if (f == "ff")
            f = "f";
        else if (f == "wf")
            f = "w";
        if (f != "b" && f != "f" && f != "w")
        {
            out << "Error: -fit debe ser 'bf/b', 'ff/f' o 'wf/w'\n";
            return out.str();
        }
        if (!std::filesystem::exists(path))
        {
            out << "Error: No existe el disco: " << path << "\n";
            return out.str();
        }

        //  Convertir a bytes
        int sizeBytes = size;
        if (u == "k")
            sizeBytes *= 1024;
        else if (u == "m")
            sizeBytes *= 1024 * 1024;
        // u == "b" → ya está en bytes

        //  Leer MBR
        auto file = Utilities::OpenFile(path);
        if (!file.is_open())
        {
            out << "Error: No se pudo abrir el disco\n";
            return out.str();
        }

        MBR mbr{};
        Utilities::ReadObject(file, mbr, 0);

        //  Verificar restricciones
        int freeIndex = -1;    // índice de ranura libre en Partitions[4]
        int extendedCount = 0; // cuántas extendidas hay (máx 1)

        for (int i = 0; i < 4; i++)
        {
            if (mbr.Partitions[i].Start == -1)
            {
                // ranura vacía
                if (freeIndex == -1)
                    freeIndex = i;
            }
            else
            {
                if (mbr.Partitions[i].Type[0] == 'e')
                    extendedCount++;
            }
        }

        if (freeIndex == -1)
        {
            out << "Error: El disco ya tiene 4 particiones (máximo)\n";
            file.close();
            return out.str();
        }
        if (t == "e" && extendedCount >= 1)
        {
            out << "Error: Solo puede haber UNA partición extendida por disco\n";
            file.close();
            return out.str();
        }

        // PARTICIÓN LÓGICA (type=L)
        // usando EBR (Extended Boot Record) en lista enlazada.
        if (t == "l")
        {
            // Buscar la partición extendida
            int extStart = -1, extSize = -1;
            for (int i = 0; i < 4; i++)
            {
                if (mbr.Partitions[i].Start != -1 &&
                    (mbr.Partitions[i].Type[0] == 'e'))
                {
                    extStart = mbr.Partitions[i].Start;
                    extSize = mbr.Partitions[i].Size;
                    break;
                }
            }
            if (extStart == -1)
            {
                out << "Error: No existe partición extendida para crear lógicas\n";
                file.close();
                return out.str();
            }

            // Recorrer la lista de EBRs para encontrar el último
            int ebrPos = extStart;
            int lastEbr = extStart;
            EBR ebr{};
            bool firstEbr = true;

            // Leer el primer EBR
            Utilities::ReadObject(file, ebr, ebrPos);

            if (ebr.Size <= 0)
            {
                firstEbr = true;
            }
            else
            {
                firstEbr = false;
                while (ebr.Next != -1 && ebr.Next != 0)
                {
                    lastEbr = ebr.Next;
                    ebrPos = ebr.Next;
                    file.clear(); // resetear estado del stream
                    Utilities::ReadObject(file, ebr, ebrPos);
                }
                lastEbr = ebrPos;
            }

            // Calcular dónde empieza el nuevo EBR
            int newEbrPos;
            if (firstEbr)
            {
                newEbrPos = extStart;
            }
            else
            {
                // El nuevo EBR va justo después del espacio del último
                newEbrPos = ebrPos + sizeof(EBR) + ebr.Size;
            }

            // Verificar que cabe dentro de la extendida
            int spaceUsed = newEbrPos - extStart;
            if (spaceUsed + (int)sizeof(EBR) + sizeBytes > extSize)
            {
                out << "Error: No hay espacio en la partición extendida\n";
                out << "  Disponible: " << (extSize - spaceUsed) << " bytes\n";
                out << "  Requerido:  " << sizeBytes << " bytes\n";
                file.close();
                return out.str();
            }

            // Crear el nuevo EBR
            EBR newEbr{};
            newEbr.Mount[0] = '0';
            newEbr.Fit[0] = f[0];
            newEbr.Start = newEbrPos + sizeof(EBR); // datos empiezan después del EBR
            newEbr.Size = sizeBytes;
            newEbr.Next = -1;
            std::memset(newEbr.Name, 0, 16);
            std::memcpy(newEbr.Name, name.c_str(), std::min(name.size(), (size_t)15));

            // Escribir el nuevo EBR
            Utilities::WriteObject(file, newEbr, newEbrPos);

            // Si no es el primero, actualizar el Next del anterior
            if (!firstEbr)
            {
                EBR prevEbr{};
                file.clear(); // resetear antes de leer
                Utilities::ReadObject(file, prevEbr, lastEbr);
                prevEbr.Next = newEbrPos;
                file.clear(); // resetear antes de escribir
                Utilities::WriteObject(file, prevEbr, lastEbr);
            }

            file.close();
            out << "Partición lógica creada: " << name << "\n";
            out << "Tipo: l  |  Fit: " << f << "\n";
            out << "Inicio dato: byte " << newEbr.Start << "\n";
            out << "Tamaño: " << sizeBytes << " bytes\n";
            out << "=====================\n";
            return out.str();
        }

        struct PartitionInfo
        {
            int start;
            int size;
        };
        std::vector<PartitionInfo> usedSpaces;

        // 1. Recopilar particiones ya existentes
        for (int i = 0; i < 4; i++)
        {
            if (mbr.Partitions[i].Start != -1)
            {
                usedSpaces.push_back({mbr.Partitions[i].Start, mbr.Partitions[i].Size});
            }
        }

        // 2. Ordenarlas de izquierda a derecha en el disco
        std::sort(usedSpaces.begin(), usedSpaces.end(), [](const PartitionInfo &a, const PartitionInfo &b)
                  { return a.start < b.start; });

        // 3. Buscar "huecos" libres (gaps)
        struct FreeGap
        {
            int start;
            int size;
        };
        std::vector<FreeGap> gaps;
        int currentPos = sizeof(MBR);

        for (const auto &p : usedSpaces)
        {
            if (p.start > currentPos)
            {
                gaps.push_back({currentPos, p.start - currentPos});
            }
            currentPos = p.start + p.size;
        }

        // Revisar el último gran hueco hasta el final del disco
        if (mbr.MbrSize > currentPos)
        {
            gaps.push_back({currentPos, mbr.MbrSize - currentPos});
        }

        // 4. Filtrar solo los huecos donde sí cabe nuestra partición
        std::vector<FreeGap> validGaps;
        for (const auto &g : gaps)
        {
            if (g.size >= sizeBytes)
            {
                validGaps.push_back(g);
            }
        }

        if (validGaps.empty())
        {
            out << "Error: No hay espacio suficiente o contiguo en el disco\n";
            out << "  Requerido:  " << sizeBytes << " bytes\n";
            file.close();
            return out.str();
        }

        // 5. Aplicar el algoritmo seleccionado (fit)
        FreeGap selectedGap = validGaps[0]; // First Fit (FF) por defecto

        if (f == "b" || f == "bf")
        { // Best Fit (BF)
            for (const auto &g : validGaps)
            {
                if (g.size < selectedGap.size)
                {
                    selectedGap = g;
                }
            }
        }
        else if (f == "w" || f == "wf")
        { // Worst Fit (WF)
            for (const auto &g : validGaps)
            {
                if (g.size > selectedGap.size)
                {
                    selectedGap = g;
                }
            }
        }

        int nextStart = selectedGap.start;
        // ----------------------------------------------------------------------------------

        //  Llenar la ranura libre
        Partition &p = mbr.Partitions[freeIndex];
        p.Status[0] = '0'; // sin montar todavía
        p.Type[0] = t[0];  // 'p', 'e' o 'l'
        p.Fit[0] = f[0];   // 'b', 'f' o 'w'
        p.Start = nextStart;
        p.Size = sizeBytes;
        p.Correlative = -1;

        // Copiar nombre (máx 15 chars + null terminator)
        std::memset(p.Name, 0, 16);
        std::memcpy(p.Name, name.c_str(), std::min(name.size(), (size_t)15));

        //  Escribir MBR actualizado
        Utilities::WriteObject(file, mbr, 0);
        file.close();

        out << "Partición creada: " << name << "\n";
        out << "Tipo: " << t << "  |  Fit: " << f << "\n";
        out << "Inicio: byte " << nextStart << "\n";
        out << "Tamaño: " << sizeBytes << " bytes\n";
        out << "=====================\n";
        return out.str();
    }

    // MOUNT — Monta una partición y le asigna un ID
    // El ID tiene formato: [2 dígitos carnet][correlativo][letra disco]
    // carnet=202012345, disco="DiscoA.mia"
    //   --> toma últimos 2 dígitos = "45"
    //   --> correlativo = 1
    //   --> letra = 'A'
    //   --> ID = "451A"

    std::string Mount(const std::string &path, const std::string &name)
    {
        std::ostringstream out;
        out << "======= MOUNT =======\n";

        if (!std::filesystem::exists(path))
        {
            out << "Error: No existe el disco: " << path << "\n";
            return out.str();
        }

        auto file = Utilities::OpenFile(path);
        if (!file.is_open())
        {
            out << "Error: No se pudo abrir el disco\n";
            return out.str();
        }

        MBR mbr{};
        Utilities::ReadObject(file, mbr, 0);

        // Buscar partición por nombre
        int found = -1;
        for (int i = 0; i < 4; i++)
        {
            if (mbr.Partitions[i].Start != -1)
            {
                std::string partName(mbr.Partitions[i].Name);
                if (partName == name)
                {
                    found = i;
                    break;
                }
            }
        }

        if (found == -1)
        {
            out << "Error: No existe la partición '" << name << "' en el disco\n";
            file.close();
            return out.str();
        }

        // Verificar que no esté ya montada
        for (int i = 0; i < mountedCount; i++)
        {
            if (mountedPartitions[i].path == path &&
                mountedPartitions[i].name == name)
            {
                out << "Error: La partición '" << name << "' ya está montada"
                    << " con ID " << mountedPartitions[i].id << "\n";
                file.close();
                return out.str();
            }
        }

        //  Generar ID
        // stem() extrae el nombre del archivo sin extensión
        // "/home/user/DiscoA.mia" -> stem = "DiscoA" -> last char = 'A'
        std::filesystem::path p(path);
        char diskLetter = 'A';
        std::vector<std::string> uniqueDisks;
        for (int i = 0; i < mountedCount; i++)
        {
            bool found = false;
            for (auto &d : uniqueDisks)
            {
                if (d == mountedPartitions[i].path)
                {
                    found = true;
                    break;
                }
            }
            if (!found)
                uniqueDisks.push_back(mountedPartitions[i].path);
        }

        // Verificar si este disco ya tiene letra asignada
        bool diskExists = false;
        for (int i = 0; i < (int)uniqueDisks.size(); i++)
        {
            if (uniqueDisks[i] == path)
            {
                diskLetter = 'A' + i;
                diskExists = true;
                break;
            }
        }
        // Si es un disco nuevo, asignarle la siguiente letra
        if (!diskExists)
        {
            diskLetter = 'A' + (int)uniqueDisks.size();
        }

        std::string carnet = "40";

        int discCorrelative = 1;
        for (int i = 0; i < mountedCount; i++)
        {
            if (mountedPartitions[i].path == path)
            {
                discCorrelative++;
            }
        }

        std::string id = carnet + std::to_string(discCorrelative) + diskLetter;

        //  Actualizar partición en MBR
        mbr.Partitions[found].Status[0] = '1'; // montada
        mbr.Partitions[found].Correlative = discCorrelative;
        std::memset(mbr.Partitions[found].Id, 0, 4);
        std::memcpy(mbr.Partitions[found].Id, id.c_str(),
                    std::min(id.size(), (size_t)4));

        Utilities::WriteObject(file, mbr, 0);
        file.close();

        // Crear punto de montaje físico
        std::string physicalRoot = "/tmp/extreamfs_mounts/" + id;
        std::filesystem::create_directories(physicalRoot);

        //  Guardar en arreglo RAM
        mountedPartitions[mountedCount].path = path;
        mountedPartitions[mountedCount].name = name;
        mountedPartitions[mountedCount].id = id;
        mountedPartitions[mountedCount].correlative = discCorrelative;
        mountedPartitions[mountedCount].mountPoint = physicalRoot;
        mountedCount++;
        globalCorrelative++;

        out << "Partición montada: " << name << "\n";
        out << "ID asignado: " << id << "\n";
        out << "=====================\n";
        return out.str();
    }

    // MOUNTED — Lista todas las particiones montadas

    std::string Mounted()
    {
        std::ostringstream out;
        out << "====== MOUNTED ======\n";

        if (mountedCount == 0)
        {
            out << "No hay particiones montadas\n";
        }
        else
        {
            for (int i = 0; i < mountedCount; i++)
            {
                out << "  " << mountedPartitions[i].id
                    << "  |  " << mountedPartitions[i].name
                    << "  |  " << mountedPartitions[i].path << "\n";
            }
        }

        out << "=====================\n";
        return out.str();
    }

    // FindMountedById
    // Busca una partición montada por su ID en el arreglo RAM

    int FindMountedById(const std::string &id, std::string &outPath)
    {
        for (int i = 0; i < mountedCount; i++)
        {
            if (mountedPartitions[i].id == id)
            {
                outPath = mountedPartitions[i].path;
                return i;
            }
        }
        return -1;
    }

    // GetMountPoint
    // Retorna el punto de montaje físico de una partición montada

    std::string GetMountPoint(const std::string &id)
    {
        for (int i = 0; i < mountedCount; i++)
        {
            if (mountedPartitions[i].id == id)
            {
                return mountedPartitions[i].mountPoint;
            }
        }
        return "";
    }

    std::string Unmount(const std::string &id)
    {
        std::ostringstream out;
        out << "======= UNMOUNT =======\n";

        int mountedIndex = -1;
        for (int i = 0; i < mountedCount; i++)
        {
            if (mountedPartitions[i].id == id)
            {
                mountedIndex = i;
                break;
            }
        }

        if (mountedIndex == -1)
        {
            out << "Error: No existe una partición montada con ID '" << id << "'\n";
            return out.str();
        }

        std::string path = mountedPartitions[mountedIndex].path;
        std::string name = mountedPartitions[mountedIndex].name;

        if (!std::filesystem::exists(path))
        {
            out << "Error: No existe el disco montado en la ruta: " << path << "\n";
            return out.str();
        }

        auto file = Utilities::OpenFile(path);
        if (!file.is_open())
        {
            out << "Error: No se pudo abrir el disco: " << path << "\n";
            return out.str();
        }

        MBR mbr{};
        Utilities::ReadObject(file, mbr, 0);

        bool found = false;
        for (int i = 0; i < 4; i++)
        {
            if (mbr.Partitions[i].Start != -1)
            {
                std::string partName(mbr.Partitions[i].Name);
                if (partName == name && std::string(mbr.Partitions[i].Id, 4) == id)
                {
                    mbr.Partitions[i].Status[0] = '0';
                    mbr.Partitions[i].Correlative = -1;
                    std::memset(mbr.Partitions[i].Id, 0, 4);
                    found = true;
                    break;
                }
            }
        }

        if (!found)
        {
            file.close();
            out << "Error: No se pudo encontrar la partición montada en el MBR\n";
            return out.str();
        }

        Utilities::WriteObject(file, mbr, 0);
        file.close();

        for (int i = mountedIndex; i < mountedCount - 1; i++)
        {
            mountedPartitions[i] = mountedPartitions[i + 1];
        }
        mountedCount--;

        out << "Partición desmontada: " << name << "\n";
        out << "ID liberado: " << id << "\n";
        out << "=======================\n";
        return out.str();
    }

    // FUNCIONES NUEVAS  PARA  EL PROYECTO 2
    // ══════════════════════════════════════
    // FDISK -add: Agregar espacio a una partición existente
    std::string FdiskAdd(int addSize, const std::string &unit, const std::string &path, const std::string &name)
    {
        std::ostringstream out;
        out << "======= FDISK -add =======\n";

        // Validar parámetros
        if (addSize == 0)
        {
            out << "Error: -add no puede ser 0\n";
            return out.str();
        }

        std::string u = unit;
        for (auto &c : u)
            c = tolower(c);

        if (u != "k" && u != "m" && u != "b")
        {
            out << "Error: -unit debe ser 'k', 'm' o 'b'\n";
            return out.str();
        }

        if (!std::filesystem::exists(path))
        {
            out << "Error: No existe el disco: " << path << "\n";
            return out.str();
        }

        // Convertir addSize a bytes
        int addBytes = addSize;
        if (u == "k")
            addBytes *= 1024;
        else if (u == "m")
            addBytes *= 1024 * 1024;

        // Leer MBR
        auto file = Utilities::OpenFile(path);
        if (!file.is_open())
        {
            out << "Error: No se pudo abrir el disco\n";
            return out.str();
        }

        MBR mbr{};
        Utilities::ReadObject(file, mbr, 0);

        // Buscar la partición por nombre
        int partIndex = -1;
        for (int i = 0; i < 4; i++)
        {
            if (mbr.Partitions[i].Start != -1)
            {
                std::string partName(mbr.Partitions[i].Name);
                if (partName == name)
                {
                    partIndex = i;
                    break;
                }
            }
        }

        if (partIndex == -1)
        {
            out << "Error: No existe la partición '" << name << "' en el disco\n";
            file.close();
            return out.str();
        }

        Partition &p = mbr.Partitions[partIndex];

        // Verificar que no esté montada
        if (p.Status[0] == '1')
        {
            out << "Error: No se puede modificar una partición montada\n";
            file.close();
            return out.str();
        }

        // Calcular el espacio disponible después de esta partición
        int spaceAfter = mbr.MbrSize;
        for (int i = 0; i < 4; i++)
        {
            if (mbr.Partitions[i].Start != -1 && i != partIndex)
            {
                int endOfOther = mbr.Partitions[i].Start + mbr.Partitions[i].Size;
                if (endOfOther > p.Start + p.Size && endOfOther < spaceAfter)
                {
                    spaceAfter = endOfOther;
                }
            }
        }

        int availableSpace = spaceAfter - (p.Start + p.Size);

        if (addBytes > availableSpace)
        {
            out << "Error: No hay suficiente espacio disponible\n";
            out << "  Disponible: " << availableSpace << " bytes\n";
            out << "  Solicitado: " << addBytes << " bytes\n";
            file.close();
            return out.str();
        }

        // Agregar el espacio
        p.Size += addBytes;

        // Escribir MBR actualizado
        Utilities::WriteObject(file, mbr, 0);
        file.close();

        out << "Espacio agregado a partición '" << name << "'\n";
        out << "Tamaño anterior: " << (p.Size - addBytes) << " bytes\n";
        out << "Nuevo tamaño: " << p.Size << " bytes\n";
        out << "Espacio agregado: " << addBytes << " bytes\n";
        out << "==========================\n";
        return out.str();
    }

    // FDISK -delete: Eliminar una partición (fast o full)
    std::string FdiskDelete(const std::string &deleteType, const std::string &path, const std::string &name)
    {
        std::ostringstream out;
        out << "======= FDISK -delete =======\n";

        std::string dt = deleteType;
        for (auto &c : dt)
            c = tolower(c);

        if (dt != "fast" && dt != "full")
        {
            out << "Error: -delete debe ser 'fast' o 'full'\n";
            return out.str();
        }

        if (!std::filesystem::exists(path))
        {
            out << "Error: No existe el disco: " << path << "\n";
            return out.str();
        }

        // Leer MBR
        auto file = Utilities::OpenFile(path);
        if (!file.is_open())
        {
            out << "Error: No se pudo abrir el disco\n";
            return out.str();
        }

        MBR mbr{};
        Utilities::ReadObject(file, mbr, 0);

        // Buscar la partición por nombre
        int partIndex = -1;
        for (int i = 0; i < 4; i++)
        {
            if (mbr.Partitions[i].Start != -1)
            {
                std::string partName(mbr.Partitions[i].Name);
                if (partName == name)
                {
                    partIndex = i;
                    break;
                }
            }
        }

        if (partIndex == -1)
        {
            out << "Error: No existe la partición '" << name << "' en el disco\n";
            file.close();
            return out.str();
        }

        Partition &p = mbr.Partitions[partIndex];

        // Verificar que no esté montada
        if (p.Status[0] == '1')
        {
            out << "Error: No se puede eliminar una partición montada\n";
            file.close();
            return out.str();
        }

        // Para particiones lógicas, necesitamos manejar los EBRs
        if (p.Type[0] == 'L' || p.Type[0] == 'l')
        {
            // Buscar la partición extendida
            int extIndex = -1;
            for (int i = 0; i < 4; i++)
            {
                if (mbr.Partitions[i].Start != -1 && (mbr.Partitions[i].Type[0] == 'E' || mbr.Partitions[i].Type[0] == 'e'))
                {
                    extIndex = i;
                    break;
                }
            }

            if (extIndex != -1)
            {
                // TODO: Implementar eliminación de EBRs para particiones lógicas
                // Por ahora, solo limpiamos la entrada del MBR
                out << "Advertencia: Eliminación de particiones lógicas no completamente implementada\n";
            }
        }

        // Si es delete=full, llenar el espacio con ceros
        if (dt == "full")
        {
            out << "Eliminando partición con 'full' (llenando con ceros)...\n";
            std::vector<char> zeros(1024, 0);
            int bytesToWrite = p.Size;
            int pos = p.Start;

            while (bytesToWrite > 0)
            {
                int chunkSize = std::min(1024, bytesToWrite);
                file.seekp(pos);
                file.write(zeros.data(), chunkSize);
                pos += chunkSize;
                bytesToWrite -= chunkSize;
            }
        }

        // Limpiar la entrada del MBR
        p.Start = -1;
        p.Size = 0;
        p.Correlative = -1;
        p.Status[0] = '0';
        std::memset(p.Name, 0, 16);
        std::memset(p.Id, 0, 4);

        // Escribir MBR actualizado
        Utilities::WriteObject(file, mbr, 0);
        file.close();

        out << "Partición '" << name << "' eliminada (" << dt << ")\n";
        out << "=============================\n";
        return out.str();
    }

    std::vector<MountedPartition> GetMountedPartitionsList()
    {
        std::vector<MountedPartition> lista;
        // Recorremos el arreglo estático hasta el límite de particiones montadas
        for (int i = 0; i < mountedCount; i++)
        {
            lista.push_back(mountedPartitions[i]);
        }
        return lista;
    }

}