#pragma once
#include <fstream>
#include <string>


// UTILITIES — I/O 
// leer/escribir structs del disco.
// El patrón "template<typename T>" permite una sola función
// que sirve para MBR, Partition, Inode, FolderBlock... cualquiera.
//
// Uso típico:
//   MBR mbr;
//   auto file = Utilities::OpenFile("/ruta/disco.mia");
//   Utilities::ReadObject(file, mbr, 0); -- lee MBR del byte 0
//   mbr.Signature = 999;
//   Utilities::WriteObject(file, mbr, 0); -- lo escribe de vuelta

namespace Utilities {

// Crea el archivo vacío. Crea las carpetas del path si no existen.
bool CreateFile(const std::string& path);

// Abre un archivo existente para leer Y escribir en binario.
std::fstream OpenFile(const std::string& path);

// Retorna la fecha/hora actual 
std::string GetCurrentDateTime();

// WriteObject<T>
// Escribe cualquier struct en el archivo en la posición 'pos'.
// seekp(pos) mueve el cursor de ESCRITURA al byte indicado.
// reinterpret_cast<char*> convierte la struct a bytes crudos.


template<typename T>
bool WriteObject(std::fstream& file, const T& data, std::streampos pos) {
    file.seekp(pos);
    file.write(reinterpret_cast<const char*>(&data), sizeof(T));
    file.flush();          // fuerza escritura inmediata al disco
    return file.good();    // good() = true si no hubo error
}


// ReadObject<T>
// Lee bytes del archivo y los interpreta como la struct T.
// seekg(pos) mueve el cursor de LECTURA al byte indicado.

template<typename T>
bool ReadObject(std::fstream& file, T& data, std::streampos pos) {
    file.seekg(pos);
    file.read(reinterpret_cast<char*>(&data), sizeof(T));
    return file.good();
}

} 