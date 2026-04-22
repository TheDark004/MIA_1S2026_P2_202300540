#pragma once
#include <string>
#include <nlohmann/json.hpp>

namespace SystemInfo
{
    // Obtiene los discos montados pero leyendo el MBR para sacar tamaño y espacio libre
    nlohmann::json GetEnrichedDisks();

    // Obtiene las particiones de un disco leyendo el MBR para sacar fit, tamaño y estado
    nlohmann::json GetEnrichedPartitions(const std::string &diskName);

    // Lee el bloque de Journaling de una partición montada (EXT3)
    nlohmann::json GetJournaling(const std::string &id);
}