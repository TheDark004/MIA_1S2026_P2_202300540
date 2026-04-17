#include "Utilities.h"
#include <filesystem>
#include <ctime>
#include <iomanip>
#include <sstream>

namespace Utilities {


// CreateFile
// 1. Extrae la carpeta del path: "/home/user/discos/A.mia"
//    -> carpeta = "/home/user/discos"
// 2. Crea esa carpeta (y todas las intermedias) si no existen.
// 3. Crea el archivo vacío en modo binario.
//    ios::trunc = si ya existe, lo borra y empieza de cero.

bool CreateFile(const std::string& path) {
    std::filesystem::path p(path);

    // Crear directorios intermedios si no existen
    if (p.has_parent_path()) {
        std::filesystem::create_directories(p.parent_path());
    }

    // Crear el archivo vacío
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    return file.good();
}


// OpenFile
// Abre el archivo para leer Y escribir simultáneamente.
//   ios::in     -> habilita lectura  (seekg + read)
//   ios::out    -> habilita escritura (seekp + write)
//   ios::binary -> no transforma caracteres especiales
// NOTA: el archivo ya debe existir. Si no existe, el
// fstream queda en estado de error (is_open() = false).

std::fstream OpenFile(const std::string& path) {
    return std::fstream(path, std::ios::in | std::ios::out | std::ios::binary);
}


// GetCurrentDateTime
// time(nullptr)     -> segundos (Unix timestamp)
// localtime(...)    -> convierte a struct tm con año/mes/día/etc.
// put_time(...)     -> formatea según el patrón dado

std::string GetCurrentDateTime() {
    std::time_t now = std::time(nullptr);
    std::tm* lt = std::localtime(&now);
    std::ostringstream oss;
    oss << std::put_time(lt, "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

} 