#include "Analyzer.h"
#include "../DiskManagement/DiskManagement.h"
#include "../FileSystem/FileSystem.h"
#include "../UserSession/UserSession.h"
#include "../FileOperations/FileOperations.h"
#include "../Reports/Reports.h"
#include <regex>
#include <sstream>
#include <map>
#include <set>
#include <algorithm>

namespace Analyzer
{

    // ─────────────────────────────────────────────────────────────
    // Helper: validar que solo vengan parámetros permitidos
    // Retorna "" si todo bien, o un mensaje de error si hay uno inválido
    // ─────────────────────────────────────────────────────────────
    static std::string ValidateParams(const std::string &command,
                                      const std::map<std::string, std::string> &params,
                                      const std::set<std::string> &valid)
    {
        for (auto &p : params)
        {
            if (!valid.count(p.first))
            {
                return "Error " + command + ": Parámetro desconocido '-" + p.first + "'\n";
            }
        }
        return "";
    }

    // Analyze
    // Recibe:  "mkdisk -size=3 -unit=m -fit=bf -path=/home/A.mia"
    //
    // Pasos:
    //  1. Ignorar líneas vacías y comentarios (#)
    //  2. Extraer el nombre del comando (primera palabra)
    //  3. Usar REGEX para extraer los pares -key=value
    //  4. Buscar el comando y llamar al módulo correcto

    std::string Analyze(const std::string &input)
    {

        // Ignorar vacíos
        if (input.empty())
            return "";

        // Quitar espacios/tabs al inicio
        std::string line = input;
        size_t start = line.find_first_not_of(" \t\r\n");
        if (start == std::string::npos)
            return "";
        line = line.substr(start);

        // Comentarios (#)
        // mostrar comentarios en el área de salida
        if (line[0] == '#')
        {
            return line + "\n";
        }

        // Extraer nombre del comando
        std::istringstream iss(line);
        std::string command;
        iss >> command;
        // Convertir a minúscula: "MKDISK" -> "mkdisk"
        std::transform(command.begin(), command.end(), command.begin(), ::tolower);

        // Patrón: -(\w+)=("[^"]+"|\S+)
        //
        //   -         -> literal, el guión
        //   (\w+)     -> GRUPO 1: letras/dígitos/_ = nombre del param
        //   =         -> literal, el igual
        //   (         -> inicio GRUPO 2: el valor, que puede ser:
        //     "[^"]+" -> entre comillas: permite espacios adentro
        //     |       -> O
        //     \S+     -> sin comillas: cualquier cosa sin espacios
        //   )
        //
        //   -size=3 -> key="size", value="3"
        //   -path=/home/A.mia -> key="path",  value="/home/A.mia"
        //   -path="/mi disco/A.mia" -> key="path", value="/mi disco/A.mia"
        //   -fit=bf -> key="fit", value="bf"

        std::regex re(R"(-(\w+)=("[^"]+"|\S+))");
        auto it = std::sregex_iterator(line.begin(), line.end(), re);
        auto end = std::sregex_iterator();

        std::map<std::string, std::string> params;

        for (; it != end; ++it)
        {
            std::string key = (*it)[1].str();
            std::string value = (*it)[2].str();

            // Quitar comillas del valor si las tiene
            // "/mi disco/A.mia" -> /mi disco/A.mia
            if (value.size() >= 2 && value.front() == '"' && value.back() == '"')
            {
                value = value.substr(1, value.size() - 2);
            }

            // Keys siempre en minúscula para comparar de manera uniforme
            std::transform(key.begin(), key.end(), key.begin(), ::tolower);

            params[key] = value;
        }

        std::regex flagRe(R"((?:^|\s)-([a-zA-Z]+)(?:\s|$))");
        auto fit = std::sregex_iterator(line.begin(), line.end(), flagRe);
        auto fend = std::sregex_iterator();
        std::map<std::string, bool> flags;
        for (; fit != fend; ++fit)
        {
            std::string flag = (*fit)[1].str();
            std::transform(flag.begin(), flag.end(), flag.begin(), ::tolower);
            if (!params.count(flag))
                flags[flag] = true;
        }

        std::string err;

        // ── DISCO ──────────────────────────────────────────────────
        if (command == "mkdisk")
        {
            err = ValidateParams("MKDISK", params, {"size", "path", "fit", "unit"});
            if (!err.empty())
                return err;
            if (!params.count("size") || !params.count("path"))
                return "Error MKDISK: Se requieren -size y -path\n";
            return DiskManagement::Mkdisk(
                std::stoi(params["size"]),
                params.count("fit") ? params["fit"] : "ff",
                params.count("unit") ? params["unit"] : "m",
                params["path"]);
        }
        else if (command == "rmdisk")
        {
            err = ValidateParams("RMDISK", params, {"path"});
            if (!err.empty())
                return err;
            if (!params.count("path"))
                return "Error RMDISK: Se requiere -path\n";
            return DiskManagement::Rmdisk(params["path"]);
        }
        else if (command == "fdisk")
        {
            // FDISK puede tener 3 variantes:
            // 1. Crear partición: -size, -path, -name, -type, -fit, -unit
            // 2. Agregar espacio: -add, -unit, -path, -name
            // 3. Eliminar partición: -delete, -path, -name

            if (params.count("add"))
            {
                // FDISK -add: agregar espacio a partición existente
                err = ValidateParams("FDISK", params, {"add", "unit", "path", "name"});
                if (!err.empty())
                    return err;
                if (!params.count("path") || !params.count("name"))
                    return "Error FDISK -add: Se requieren -path y -name\n";
                return DiskManagement::FdiskAdd(
                    std::stoi(params["add"]),
                    params.count("unit") ? params["unit"] : "k",
                    params["path"], params["name"]);
            }
            else if (params.count("delete"))
            {
                // FDISK -delete: eliminar partición
                err = ValidateParams("FDISK", params, {"delete", "path", "name"});
                if (!err.empty())
                    return err;
                if (!params.count("path") || !params.count("name"))
                    return "Error FDISK -delete: Se requieren -path y -name\n";
                return DiskManagement::FdiskDelete(
                    params["delete"],
                    params["path"], params["name"]);
            }
            else
            {
                // FDISK normal: crear partición
                err = ValidateParams("FDISK", params, {"size", "path", "name", "type", "fit", "unit"});
                if (!err.empty())
                    return err;
                if (!params.count("size") || !params.count("path") || !params.count("name"))
                    return "Error FDISK: Se requieren -size, -path y -name\n";
                return DiskManagement::Fdisk(
                    std::stoi(params["size"]),
                    params["path"], params["name"],
                    params.count("type") ? params["type"] : "p",
                    params.count("fit") ? params["fit"] : "f",
                    params.count("unit") ? params["unit"] : "k");
            }
        }
        else if (command == "mount")
        {
            err = ValidateParams("MOUNT", params, {"path", "name"});
            if (!err.empty())
                return err;
            if (!params.count("path") || !params.count("name"))
                return "Error MOUNT: Se requieren -path y -name\n";
            return DiskManagement::Mount(params["path"], params["name"]);
        }
        else if (command == "unmount")
        {
            err = ValidateParams("UNMOUNT", params, {"id"});
            if (!err.empty())
                return err;
            if (!params.count("id"))
                return "Error UNMOUNT: Se requiere -id\n";
            return DiskManagement::Unmount(params["id"]);
        }
        else if (command == "mounted")
        {
            return DiskManagement::Mounted();

            // ── FILESYSTEM ─────────────────────────────────────────────
        }
        else if (command == "mkfs")
        {
            err = ValidateParams("MKFS", params, {"id", "type", "fs"});
            if (!err.empty())
                return err;
            if (!params.count("id"))
                return "Error MKFS: Se requiere -id\n";

            // Validar que -fs sea válido si se proporciona
            if (params.count("fs"))
            {
                std::string fs = params["fs"];
                if (fs != "2fs" && fs != "3fs")
                {
                    return "Error MKFS: -fs debe ser '2fs' (EXT2) o '3fs' (EXT3)\n";
                }
            }

            return FileSystem::Mkfs(params["id"],
                                    params.count("type") ? params["type"] : "full",
                                    params.count("fs") ? params["fs"] : "2fs");

            // ── SESIÓN ─────────────────────────────────────────────────
        }
        else if (command == "login")
        {
            err = ValidateParams("LOGIN", params, {"user", "pass", "id"});
            if (!err.empty())
                return err;
            if (!params.count("user") || !params.count("pass") || !params.count("id"))
                return "Error LOGIN: Se requieren -user, -pass y -id\n";
            return UserSession::Login(params["user"], params["pass"], params["id"]);
        }
        else if (command == "logout")
        {
            return UserSession::Logout();
        }
        else if (command == "mkgrp")
        {
            err = ValidateParams("MKGRP", params, {"name"});
            if (!err.empty())
                return err;
            if (!params.count("name"))
                return "Error MKGRP: falta -name\n";
            return UserSession::Mkgrp(params["name"]);
        }
        else if (command == "rmgrp")
        {
            err = ValidateParams("RMGRP", params, {"name"});
            if (!err.empty())
                return err;
            if (!params.count("name"))
                return "Error RMGRP: falta -name\n";
            return UserSession::Rmgrp(params["name"]);
        }
        else if (command == "mkusr")
        {
            err = ValidateParams("MKUSR", params, {"user", "pass", "grp"});
            if (!err.empty())
                return err;
            if (!params.count("user") || !params.count("pass") || !params.count("grp"))
                return "Error MKUSR: Se requieren -user, -pass y -grp\n";
            return UserSession::Mkusr(params["user"], params["pass"], params["grp"]);
        }
        else if (command == "rmusr")
        {
            err = ValidateParams("RMUSR", params, {"user"});
            if (!err.empty())
                return err;
            if (!params.count("user"))
                return "Error RMUSR: falta -user\n";
            return UserSession::Rmusr(params["user"]);
        }
        else if (command == "chgrp")
        {
            err = ValidateParams("CHGRP", params, {"user", "grp"});
            if (!err.empty())
                return err;
            if (!params.count("user") || !params.count("grp"))
                return "Error CHGRP: Se requieren -user y -grp\n";
            return UserSession::Chgrp(params["user"], params["grp"]);

            // ── ARCHIVOS ───────────────────────────────────────────────
        }
        else if (command == "mkdir")
        {
            err = ValidateParams("MKDIR", params, {"path", "p"});
            if (!err.empty())
                return err;
            if (!params.count("path"))
                return "Error MKDIR: falta -path\n";
            bool p = flags.count("p") || params.count("p");
            return FileOperations::Mkdir(params["path"], p);
        }
        else if (command == "mkfile")
        {
            err = ValidateParams("MKFILE", params, {"path", "size", "cont", "r"});
            if (!err.empty())
                return err;
            if (!params.count("path"))
                return "Error MKFILE: falta -path\n";
            bool randomFlag = flags.count("r") || params.count("r");
            int size = params.count("size") ? std::stoi(params["size"]) : 0;
            std::string cont = params.count("cont") ? params["cont"] : "";
            return FileOperations::Mkfile(params["path"], randomFlag, size, cont);
        }
        else if (command == "cat")
        {
            // Acepta -file o -file1
            std::string filePath;
            if (params.count("file"))
                filePath = params["file"];
            else if (params.count("file1"))
                filePath = params["file1"];
            else
                return "Error CAT: falta -file o -file1\n";
            return FileOperations::Cat(filePath);
        }
        else if (command == "remove")
        {
            err = ValidateParams("REMOVE", params, {"path"});
            if (!err.empty())
                return err;
            if (!params.count("path"))
                return "Error REMOVE: falta -path\n";
            return FileOperations::Remove(params["path"]);
        }
        else if (command == "rename")
        {
            err = ValidateParams("RENAME", params, {"path", "name"});
            if (!err.empty())
                return err;
            if (!params.count("path") || !params.count("name"))
                return "Error RENAME: se requieren -path y -name\n";
            return FileOperations::Rename(params["path"], params["name"]);
        }
        else if (command == "copy")
        {
            err = ValidateParams("COPY", params, {"path", "path1", "source", "dest", "destino", "destination"});
            if (!err.empty())
                return err;
            std::string source;
            if (params.count("path"))
                source = params["path"];
            else if (params.count("path1"))
                source = params["path1"];
            else if (params.count("source"))
                source = params["source"];
            else
                return "Error COPY: falta -path, -path1 o -source\n";
            std::string dest;
            if (params.count("dest"))
                dest = params["dest"];
            else if (params.count("destino"))
                dest = params["destino"];
            else if (params.count("destination"))
                dest = params["destination"];
            else
                return "Error COPY: falta -dest, -destino o -destination\n";
            return FileOperations::Copy(source, dest);
        }
        else if (command == "move")
        {
            err = ValidateParams("MOVE", params, {"path", "path1", "source", "dest", "destino", "destination"});
            if (!err.empty())
                return err;
            std::string source;
            if (params.count("path"))
                source = params["path"];
            else if (params.count("path1"))
                source = params["path1"];
            else if (params.count("source"))
                source = params["source"];
            else
                return "Error MOVE: falta -path, -path1 o -source\n";
            std::string dest;
            if (params.count("dest"))
                dest = params["dest"];
            else if (params.count("destino"))
                dest = params["destino"];
            else if (params.count("destination"))
                dest = params["destination"];
            else
                return "Error MOVE: falta -dest, -destino o -destination\n";
            return FileOperations::Move(source, dest);
        }
        else if (command == "find")
        {
            err = ValidateParams("FIND", params, {"path", "name"});
            if (!err.empty())
                return err;
            if (!params.count("path") || !params.count("name"))
                return "Error FIND: se requieren -path y -name\n";
            return FileOperations::Find(params["path"], params["name"]);
        }
        else if (command == "chmod")
        {
            err = ValidateParams("CHMOD", params, {"path", "ugo"});
            if (!err.empty())
                return err;
            if (!params.count("path") || !params.count("ugo"))
                return "Error CHMOD: se requieren -path y -ugo\n";
            return FileOperations::Chmod(params["path"], params["ugo"]);
        }
        else if (command == "chown")
        {
            err = ValidateParams("CHOWN", params, {"path", "usr", "grp"});
            if (!err.empty())
                return err;
            if (!params.count("path"))
                return "Error CHOWN: falta -path\n";
            std::string usr = params.count("usr") ? params["usr"] : "";
            std::string grp = params.count("grp") ? params["grp"] : "";
            if (usr.empty() && grp.empty())
                return "Error CHOWN: falta -usr o -grp\n";
            return FileOperations::Chown(params["path"], usr, grp);
        }
        else if (command == "loss")
        {
            err = ValidateParams("LOSS", params, {"id"});
            if (!err.empty())
                return err;
            if (!params.count("id"))
                return "Error LOSS: falta -id\n";
            return FileOperations::Loss(params["id"]);
        }

        // ── REPORTES ───────────────────────────────────────────────
        else if (command == "rep")
        {
            err = ValidateParams("REP", params, {"id", "path", "ruta", "name", "path_file_ls"});
            if (!err.empty())
                return err;
            if (!params.count("id"))
                return "Error REP: falta -id\n";
            if (!params.count("name"))
                return "Error REP: falta -name\n";

            // Acepta -path= o -ruta=
            std::string ruta;
            if (params.count("path"))
                ruta = params["path"];
            else if (params.count("ruta"))
                ruta = params["ruta"];
            else
                return "Error REP: falta -path\n";

            // Para reportes file y ls → acepta -path_file_ls=
            std::string filePath;
            if (params.count("path_file_ls"))
                filePath = params["path_file_ls"];

            return Reports::Rep(params["id"], ruta, params["name"], filePath);
        }
        else
        {
            return "Error: Comando no reconocido -> '" + command + "'\n";
        }
    }

    std::string AnalyzeScript(const std::string &script)
    {
        std::ostringstream total;
        std::istringstream stream(script);
        std::string line;
        while (std::getline(stream, line))
        {
            std::string result = Analyze(line);
            if (!result.empty())
                total << result;
        }
        return total.str();
    }
}