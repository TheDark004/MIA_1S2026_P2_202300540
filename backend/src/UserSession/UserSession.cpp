#include "UserSession.h"
#include "../DiskManagement/DiskManagement.h"
#include "../FileSystem/FileSystem.h"
#include "../Utilities/Utilities.h"
#include "../Structs/Structs.h"
#include <sstream>
#include <vector>
#include <cstring>
#include <algorithm>

namespace UserSession {

// Sesión activa global
// Todos los módulos pueden leer currentSession para saber quién está logueado y en qué partición trabaja.
 
Session currentSession;

// Helper: dividir un string por un delimitador
// Ej: split("1,U,root,root,123", ',') → ["1","U","root","root","123"]

static std::vector<std::string> split(const std::string& s, char delim) {
    std::vector<std::string> tokens;
    std::istringstream ss(s);
    std::string token;
    while (std::getline(ss, token, delim)) {
        tokens.push_back(token);
    }
    return tokens;
}

// ReadUsersFile
// El archivo users.txt está guardado en el EXT2 como:
//   inodo 1 → i_block[0] = bloque 1 (y más bloques si el archivo creció)
//
// Leemos el inodo 1, luego cada bloque apuntado, y concatenamos
// el contenido hasta llegar al tamaño real (i_size).

std::string ReadUsersFile(std::fstream& file, const SuperBloque& sb) {
    // Leer el inodo 1 (users.txt siempre es inodo 1)
    Inode usersInode{};
    FileSystem::ReadInode(file, sb, 1, usersInode);

    std::string content;
    int remaining = usersInode.i_size; // bytes reales que tiene el archivo

    // Recorrer los bloques directos [0..11]
    for (int i = 0; i < 12 && remaining > 0; i++) {
        if (usersInode.i_block[i] == -1) break;

        FileBlock fb{};
        int pos = sb.s_block_start + usersInode.i_block[i] * sizeof(FolderBlock);
        Utilities::ReadObject(file, fb, pos);

        // Agregar solo los bytes reales (no el relleno de ceros)
        int toRead = std::min(remaining, (int)sizeof(FileBlock));
        content.append(fb.b_content, toRead);
        remaining -= toRead;
    }

    return content;
}

// WriteUsersFile
// Escribe un string en el archivo users.txt del EXT2.
// Si el contenido cabe en un bloque (<=64B), usa solo el bloque 1.
// Si crece, asigna nuevos bloques con AllocateBlock.
//
// Pasos:
//   1. Leer inodo 1 para saber qué bloques ya tiene
//   2. Si el contenido cabe en los bloques existentes -> reescribir
//   3. Si necesita más bloques -> asignar con AllocateBlock
//   4. Actualizar i_size del inodo

bool WriteUsersFile(std::fstream& file, SuperBloque& sb,
                    int partStart, const std::string& content) {
    Inode usersInode{};
    FileSystem::ReadInode(file, sb, 1, usersInode);

    int totalBytes = (int)content.size();
    int offset     = 0;
    int blockIndex = 0; // índice en i_block[0..11]

    while (offset < totalBytes && blockIndex < 12) {
        int blockNum;

        if (usersInode.i_block[blockIndex] == -1) {
            // No hay bloque aquí → asignar uno nuevo
            blockNum = FileSystem::AllocateBlock(file, sb);
            if (blockNum == -1) return false; // sin espacio
            usersInode.i_block[blockIndex] = blockNum;
        } else {
            blockNum = usersInode.i_block[blockIndex];
        }

        // Preparar el FileBlock con hasta 64 bytes del contenido
        FileBlock fb{};
        std::memset(fb.b_content, 0, 64);
        int toCopy = std::min(totalBytes - offset, (int)sizeof(FileBlock));
        std::memcpy(fb.b_content, content.c_str() + offset, toCopy);

        // Escribir el bloque
        int pos = sb.s_block_start + blockNum * sizeof(FolderBlock);
        Utilities::WriteObject(file, fb, pos);

        offset     += toCopy;
        blockIndex++;
    }

    // Actualizar tamaño real del archivo
    usersInode.i_size = totalBytes;
    std::string now = Utilities::GetCurrentDateTime();
    std::memcpy(usersInode.i_mtime, now.c_str(), 19);

    FileSystem::WriteInode(file, sb, 1, usersInode);

    // Guardar el SuperBloque actualizado (AllocateBlock lo modificó)
    FileSystem::WriteSuperBloque(file, partStart, sb);

    return true;
}


// GetUid — Busca el UID de un usuario en el texto de users.txt

int GetUid(const std::string& username, const std::string& usersContent) {
    std::istringstream ss(usersContent);
    std::string line;
    while (std::getline(ss, line)) {
        auto tokens = split(line, ',');
        // Línea de usuario: UID,U,nombre,grupo,pass
        if (tokens.size() >= 5 && tokens[1] == "U" && tokens[2] == username) {
            return std::stoi(tokens[0]);
        }
    }
    return -1;
}


// GetGid — Busca el GID de un grupo en el texto de users.txt

int GetGid(const std::string& groupname, const std::string& usersContent) {
    std::istringstream ss(usersContent);
    std::string line;
    while (std::getline(ss, line)) {
        auto tokens = split(line, ',');
        // Línea de grupo: GID,G,nombre
        if (tokens.size() >= 3 && tokens[1] == "G" && tokens[2] == groupname) {
            return std::stoi(tokens[0]);
        }
    }
    return -1;
}


// LOGIN
// Pasos:
//   1. Buscar la partición montada por ID
//   2. Leer el MBR para obtener partStart
//   3. Leer SuperBloque de la partición
//   4. Leer users.txt
//   5. Buscar usuario y verificar contraseña
//   6. Si OK -> activar sesión en RAM

std::string Login(const std::string& user, const std::string& pass,
                  const std::string& id) {
    std::ostringstream out;
    out << "======= LOGIN =======\n";

    if (currentSession.active) {
        out << "Error: Ya existe una sesión activa de '" 
            << currentSession.username << "'\n";
        out << "Usa LOGOUT primero\n";
        return out.str();
    }

    // Buscar partición montada
    std::string diskPath;
    int idx = DiskManagement::FindMountedById(id, diskPath);
    if (idx == -1) {
        out << "Error: No existe partición montada con ID '" << id << "'\n";
        return out.str();
    }

    // Leer MBR para obtener partStart
    auto file = Utilities::OpenFile(diskPath);
    if (!file.is_open()) {
        out << "Error: No se pudo abrir el disco\n";
        return out.str();
    }

    MBR mbr{};
    Utilities::ReadObject(file, mbr, 0);

    int partStart = -1;
    for (int i = 0; i < 4; i++) {
        std::string partId(mbr.Partitions[i].Id, 4);
        while (!partId.empty() && partId.back() == 0) partId.pop_back();
        if (partId == id) {
            partStart = mbr.Partitions[i].Start;
            break;
        }
    }

    if (partStart == -1) {
        out << "Error: Partición no encontrada en MBR\n";
        file.close();
        return out.str();
    }

    // Leer SuperBloque
    SuperBloque sb{};
    FileSystem::ReadSuperBloque(file, partStart, sb);

    if (sb.s_magic != 0xEF53) {
        out << "Error: La partición no está formateada (ejecuta MKFS primero)\n";
        file.close();
        return out.str();
    }

    // Leer users.txt
    std::string usersContent = ReadUsersFile(file, sb);
    file.close();

    // Buscar usuario y verificar contraseña
    std::istringstream ss(usersContent);
    std::string line;
    bool found = false;
    std::string userGroup;
    int userUid = -1;

    while (std::getline(ss, line)) {
        // Ignorar líneas de grupos 
        // Solo procesar líneas de usuarios: UID,U,nombre,grupo,pass
        auto tokens = split(line, ',');
        if (tokens.size() < 5) continue;
        if (tokens[1] != "U") continue;

        // UID = tokens[0] marcado con 0 significa usuario eliminado
        if (tokens[0] == "0") continue;

        if (tokens[2] == user && tokens[4] == pass) {
            found     = true;
            userUid   = std::stoi(tokens[0]);
            userGroup = tokens[3];
            break;
        }
    }

    if (!found) {
        out << "Error: Usuario o contraseña incorrectos\n";
        return out.str();
    }

    // Activar sesión en RAM
    currentSession.active    = true;
    currentSession.username  = user;
    currentSession.group     = userGroup;
    currentSession.uid       = userUid;
    currentSession.partId    = id;
    currentSession.diskPath  = diskPath;
    currentSession.partStart = partStart;

    out << "Sesión iniciada\n";
    out << "Usuario: " << user << "\n";
    out << "Grupo: "   << userGroup << "\n";
    out << "UID: "     << userUid << "\n";
    out << "Partición: " << id << "\n";
    out << "=====================\n";
    return out.str();
}


// LOGOUT — Cierra la sesión activa

std::string Logout() {
    std::ostringstream out;
    out << "======= LOGOUT =======\n";

    if (!currentSession.active) {
        out << "Error: No hay sesión activa\n";
        return out.str();
    }

    std::string name = currentSession.username;

    // Limpiar sesión
    currentSession = Session{};

    out << "Sesión cerrada: " << name << "\n";
    out << "======================\n";
    return out.str();
}


// MKGRP — Crea un grupo (solo root puede hacerlo)
// Agrega una línea "GID,G,nombre" al users.txt

std::string Mkgrp(const std::string& name) {
    std::ostringstream out;
    out << "======= MKGRP =======\n";

    if (!currentSession.active) {
        out << "Error: No hay sesión activa\n";
        return out.str();
    }
    if (currentSession.username != "root") {
        out << "Error: Solo root puede crear grupos\n";
        return out.str();
    }

    auto file = Utilities::OpenFile(currentSession.diskPath);
    SuperBloque sb{};
    FileSystem::ReadSuperBloque(file, currentSession.partStart, sb);
    std::string content = ReadUsersFile(file, sb);

    // Verificar que el grupo no exista ya
    if (GetGid(name, content) != -1) {
        out << "Error: El grupo '" << name << "' ya existe\n";
        file.close();
        return out.str();
    }

    // Calcular el próximo GID disponible
    // Recorrer todas las líneas de grupo y encontrar el GID máximo
    int maxGid = 0;
    std::istringstream ss(content);
    std::string line;
    while (std::getline(ss, line)) {
        auto tokens = split(line, ',');
        if (tokens.size() >= 3 && tokens[1] == "G" && tokens[0] != "0") {
            int gid = std::stoi(tokens[0]);
            if (gid > maxGid) maxGid = gid;
        }
    }

    std::string newLine = std::to_string(maxGid + 1) + ",G," + name + "\n";
    content += newLine;

    WriteUsersFile(file, sb, currentSession.partStart, content);
    file.close();

    out << "Grupo creado: " << name << " (GID=" << maxGid + 1 << ")\n";
    out << "=====================\n";
    return out.str();
}


// RMGRP — Elimina un grupo marcando su GID como 0
std::string Rmgrp(const std::string& name) {
    std::ostringstream out;
    out << "======= RMGRP =======\n";

    if (!currentSession.active) { out << "Error: No hay sesión activa\n"; return out.str(); }
    if (currentSession.username != "root") { out << "Error: Solo root\n"; return out.str(); }
    if (name == "root") { out << "Error: No se puede eliminar el grupo root\n"; return out.str(); }

    auto file = Utilities::OpenFile(currentSession.diskPath);
    SuperBloque sb{};
    FileSystem::ReadSuperBloque(file, currentSession.partStart, sb);
    std::string content = ReadUsersFile(file, sb);

    // Marcar el GID como 0 en la línea del grupo
    std::string newContent;
    std::istringstream ss(content);
    std::string line;
    bool found = false;

    while (std::getline(ss, line)) {
        auto tokens = split(line, ',');
        if (tokens.size() >= 3 && tokens[1] == "G" && tokens[2] == name) {
            newContent += "0,G," + name + "\n"; // GID=0 = eliminado
            found = true;
        } else {
            newContent += line + "\n";
        }
    }

    if (!found) {
        out << "Error: El grupo '" << name << "' no existe\n";
        file.close();
        return out.str();
    }

    WriteUsersFile(file, sb, currentSession.partStart, newContent);
    file.close();

    out << "Grupo eliminado: " << name << "\n";
    out << "=====================\n";
    return out.str();
}


// MKUSR — Crea un usuario (solo root)
std::string Mkusr(const std::string& user, const std::string& pass,
                  const std::string& grp) {
    std::ostringstream out;
    out << "======= MKUSR =======\n";

    if (!currentSession.active) { out << "Error: No hay sesión activa\n"; return out.str(); }
    if (currentSession.username != "root") { out << "Error: Solo root\n"; return out.str(); }

    auto file = Utilities::OpenFile(currentSession.diskPath);
    SuperBloque sb{};
    FileSystem::ReadSuperBloque(file, currentSession.partStart, sb);
    std::string content = ReadUsersFile(file, sb);

    if (GetUid(user, content) != -1) {
        out << "Error: El usuario '" << user << "' ya existe\n";
        file.close();
        return out.str();
    }
    if (GetGid(grp, content) == -1) {
        out << "Error: El grupo '" << grp << "' no existe\n";
        file.close();
        return out.str();
    }

    // Calcular el próximo UID
    int maxUid = 0;
    std::istringstream ss(content);
    std::string line;
    while (std::getline(ss, line)) {
        auto tokens = split(line, ',');
        if (tokens.size() >= 5 && tokens[1] == "U" && tokens[0] != "0") {
            int uid = std::stoi(tokens[0]);
            if (uid > maxUid) maxUid = uid;
        }
    }

    std::string newLine = std::to_string(maxUid + 1) + ",U," + user + "," + grp + "," + pass + "\n";
    content += newLine;

    WriteUsersFile(file, sb, currentSession.partStart, content);
    file.close();

    out << "Usuario creado: " << user << " (UID=" << maxUid + 1 << ", grupo=" << grp << ")\n";
    out << "=====================\n";
    return out.str();
}


// RMUSR — Elimina un usuario (marca UID como 0)
std::string Rmusr(const std::string& user) {
    std::ostringstream out;
    out << "======= RMUSR =======\n";

    if (!currentSession.active) { out << "Error: No hay sesión activa\n"; return out.str(); }
    if (currentSession.username != "root") { out << "Error: Solo root\n"; return out.str(); }
    if (user == "root") { out << "Error: No se puede eliminar root\n"; return out.str(); }

    auto file = Utilities::OpenFile(currentSession.diskPath);
    SuperBloque sb{};
    FileSystem::ReadSuperBloque(file, currentSession.partStart, sb);
    std::string content = ReadUsersFile(file, sb);

    std::string newContent;
    std::istringstream ss(content);
    std::string line;
    bool found = false;

    while (std::getline(ss, line)) {
        auto tokens = split(line, ',');
        if (tokens.size() >= 5 && tokens[1] == "U" && tokens[2] == user) {
            // Marcar UID=0
            newContent += "0,U," + tokens[2] + "," + tokens[3] + "," + tokens[4] + "\n";
            found = true;
        } else {
            newContent += line + "\n";
        }
    }

    if (!found) {
        out << "Error: Usuario '" << user << "' no existe\n";
        file.close();
        return out.str();
    }

    WriteUsersFile(file, sb, currentSession.partStart, newContent);
    file.close();

    out << "Usuario eliminado: " << user << "\n";
    out << "=====================\n";
    return out.str();
}


// CHGRP — Cambia el grupo de un usuario (solo root)

std::string Chgrp(const std::string& user, const std::string& grp) {
    std::ostringstream out;
    out << "======= CHGRP =======\n";

    if (!currentSession.active) { out << "Error: No hay sesión activa\n"; return out.str(); }
    if (currentSession.username != "root") { out << "Error: Solo root\n"; return out.str(); }

    auto file = Utilities::OpenFile(currentSession.diskPath);
    SuperBloque sb{};
    FileSystem::ReadSuperBloque(file, currentSession.partStart, sb);
    std::string content = ReadUsersFile(file, sb);

    if (GetGid(grp, content) == -1) {
        out << "Error: El grupo '" << grp << "' no existe\n";
        file.close();
        return out.str();
    }

    std::string newContent;
    std::istringstream ss(content);
    std::string line;
    bool found = false;

    while (std::getline(ss, line)) {
        auto tokens = split(line, ',');
        if (tokens.size() >= 5 && tokens[1] == "U" && tokens[2] == user) {
            // Reemplazar el grupo (tokens[3])
            newContent += tokens[0] + ",U," + tokens[2] + "," + grp + "," + tokens[4] + "\n";
            found = true;
        } else {
            newContent += line + "\n";
        }
    }

    if (!found) {
        out << "Error: Usuario '" << user << "' no existe\n";
        file.close();
        return out.str();
    }

    WriteUsersFile(file, sb, currentSession.partStart, newContent);
    file.close();

    out << "Usuario '" << user << "' movido al grupo '" << grp << "'\n";
    out << "=====================\n";
    return out.str();
}

} 
