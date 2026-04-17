#pragma once
#include <string>
#include <fstream>
#include "../Structs/Structs.h"

namespace UserSession {

// Estado de la sesión activa (en RAM)
struct Session {
    bool        active   = false; // se pregunta si esta activa la sesion
    std::string username;         // usuario logueado
    std::string group;            // grupo del usuario
    int         uid      = -1;    // UID del usuario
    std::string partId;           // ID de la partición donde está logueado
    std::string diskPath;         // path del .mia
    int         partStart = -1;   // byte de inicio de la partición
};

// Sesión global (accesible por FileOperations y Reports)
extern Session currentSession;

// ── Comandos 
std::string Login (const std::string& user, const std::string& pass,
                   const std::string& id);
std::string Logout();

std::string Mkgrp(const std::string& name);
std::string Rmgrp(const std::string& name);
std::string Mkusr(const std::string& user, const std::string& pass,
                  const std::string& grp);
std::string Rmusr(const std::string& user);
std::string Chgrp(const std::string& user, const std::string& grp);

// ── Helpers internos (usados por FileOperations) 

// Lee el contenido completo de users.txt desde el .mia
// Retorna el texto plano del archivo
std::string ReadUsersFile(std::fstream& file, const SuperBloque& sb);

// Escribe el contenido completo de users.txt en el .mia
// Maneja automáticamente múltiples bloques si el texto es largo
bool WriteUsersFile(std::fstream& file, SuperBloque& sb,
                    int partStart, const std::string& content);

// Retorna el UID de un usuario (o -1 si no existe)
int GetUid(const std::string& username, const std::string& usersContent);

// Retorna el GID de un grupo (o -1 si no existe)
int GetGid(const std::string& groupname, const std::string& usersContent);

} 
