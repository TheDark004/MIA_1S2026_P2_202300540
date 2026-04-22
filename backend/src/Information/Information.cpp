#include "Information.h"
#include "../DiskManagement/DiskManagement.h"
#include "../Utilities/Utilities.h"
#include "../Structs/Structs.h"
#include "../FileSystem/FileSystem.h"
#include "../UserSession/UserSession.h"
#include <filesystem>
#include <set>
#include <ctime>
#include <algorithm>
#include <ctime>

namespace SystemInfo
{

    // Helper para limpiar strings de arreglos de char de tamaño fijo
    std::string cleanString(const char *buffer, size_t size)
    {
        std::string str(buffer, size);
        str.erase(std::find(str.begin(), str.end(), '\0'), str.end());
        return str;
    }

    //  ESTADÍSTICAS DEL DISCO
    nlohmann::json GetEnrichedDisks()
    {
        nlohmann::json disks = nlohmann::json::array();
        std::set<std::string> uniquePaths;
        auto montajes = DiskManagement::GetMountedPartitionsList();

        for (const auto &p : montajes)
        {
            if (uniquePaths.find(p.path) != uniquePaths.end())
                continue;
            uniquePaths.insert(p.path);

            std::string fileName = std::filesystem::path(p.path).filename().string();

            auto file = Utilities::OpenFile(p.path);
            if (!file.is_open())
                continue;

            MBR mbr{};
            Utilities::ReadObject(file, mbr, 0);
            file.close();

            int usedSpace = 0;
            for (int i = 0; i < 4; i++)
            {
                if (mbr.Partitions[i].Size > 0)
                {
                    usedSpace += mbr.Partitions[i].Size;
                }
            }

            std::string fitStr = cleanString(mbr.Fit, 2);

            disks.push_back({{"name", fileName},
                             {"path", p.path},
                             {"size", mbr.MbrSize},
                             {"free", mbr.MbrSize - usedSpace - (int)sizeof(MBR)},
                             {"fit", fitStr}});
        }
        return disks;
    }

    // ESTADÍSTICAS DE PARTICIONES
    nlohmann::json GetEnrichedPartitions(const std::string &diskName)
    {
        nlohmann::json partitions = nlohmann::json::array();
        auto montajes = DiskManagement::GetMountedPartitionsList();

        std::string diskPath = "";

        for (const auto &p : montajes)
        {
            if (std::filesystem::path(p.path).filename().string() == diskName)
            {
                diskPath = p.path;
                break;
            }
        }

        if (diskPath.empty())
            return partitions;

        auto file = Utilities::OpenFile(diskPath);
        if (!file.is_open())
            return partitions;

        MBR mbr{};
        Utilities::ReadObject(file, mbr, 0);
        file.close();

        for (int i = 0; i < 4; i++)
        {
            if (mbr.Partitions[i].Size > 0)
            {
                // Usamos la función limpiadora en lugar de c_str() directo
                std::string partName = cleanString(mbr.Partitions[i].Name, 16);

                bool isMounted = false;
                std::string mountId = "";
                for (const auto &m : montajes)
                {
                    if (m.path == diskPath && m.name == partName)
                    {
                        isMounted = true;
                        mountId = m.id;
                        break;
                    }
                }

                partitions.push_back({{"id", mountId.empty() ? "N/A" : mountId},
                                      {"name", partName},
                                      {"size", mbr.Partitions[i].Size},
                                      {"fit", cleanString(mbr.Partitions[i].Fit, 2)},
                                      {"type", cleanString(mbr.Partitions[i].Type, 1)},
                                      {"status", isMounted ? "Montada" : "Desmontada"}});
            }
        }
        return partitions;
    }

    //  VISUALIZADOR DE JOURNALING (EXT3)
    nlohmann::json GetJournaling(const std::string &id)
    {
        nlohmann::json journalArray = nlohmann::json::array();

        std::string diskPath = "";
        int partStart = -1;

        // Validamos si el ID solicitado coincide con la sesión iniciada
        if (UserSession::currentSession.active && UserSession::currentSession.partId == id)
        {
            // ¡Usamos los datos que la sesión ya calculó por nosotros!
            diskPath = UserSession::currentSession.diskPath;
            partStart = UserSession::currentSession.partStart;
        }
        else
        {
            // Si el usuario pide el Journaling de un disco en el que no está logueado,
            // devolvemos vacío por seguridad.
            return journalArray;
        }

        auto file = Utilities::OpenFile(diskPath);
        if (!file.is_open())
            return journalArray;

        SuperBloque sb{};
        FileSystem::ReadSuperBloque(file, partStart, sb);

        // Si no es EXT3, devolvemos el array vacío
        if (sb.s_filesystem_type != 3)
        {
            file.close();
            return journalArray;
        }

        int journalStart = partStart + sizeof(SuperBloque);

        for (int i = 0; i < 50; i++)
        {
            // USAMOS TU ESTRUCTURA JOURNAL EXACTAMENTE COMO LA GUARDAS
            Journal j_actual{};
            file.seekg(journalStart + (i * sizeof(Journal)));
            file.read(reinterpret_cast<char *>(&j_actual), sizeof(Journal));

            // Si está vacío, lo saltamos
            if (j_actual.j_content.i_operation[0] == '\0')
            {
                continue;
            }

            // EXTRACCION DE STRINGS ---
            char opBuf[16] = {0};
            char pathBuf[35] = {0};
            char contBuf[70] = {0};

            strncpy(opBuf, j_actual.j_content.i_operation, sizeof(j_actual.j_content.i_operation));
            strncpy(pathBuf, j_actual.j_content.i_path, sizeof(j_actual.j_content.i_path));
            strncpy(contBuf, j_actual.j_content.i_content, sizeof(j_actual.j_content.i_content));

            // CONVERSIÓN DE FECHA
            time_t t = static_cast<time_t>(j_actual.j_content.i_date);
            struct tm *tm_info = localtime(&t);
            char dateBuf[30] = "Fecha desconocida";
            if (tm_info != nullptr)
            {
                strftime(dateBuf, sizeof(dateBuf), "%Y-%m-%d %H:%M:%S", tm_info);
            }

            nlohmann::json item;
            item["operation"] = std::string(opBuf);
            item["path"] = std::string(pathBuf);
            item["content"] = std::string(contBuf);
            item["date"] = std::string(dateBuf);

            journalArray.push_back(item);
        }

        file.close();
        return journalArray;
    }
}