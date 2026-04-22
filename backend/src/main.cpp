#include <iostream>
#include <string>
#include <fstream>
#include <filesystem>
#include <vector>
#include <sstream>
#include <set>
#include <algorithm>

#define CROW_MAIN
#include "../../external/crow_all.h"
#include <nlohmann/json.hpp>

#include "Analyzer/Analyzer.h"
#include "UserSession/UserSession.h"
#include "FileSystem/FileSystem.h"
#include "FileOperations/FileOperationsCore.h"
#include "DiskManagement/DiskManagement.h"
#include "Utilities/Utilities.h"
#include "Structs/Structs.h"
#include "Information/Information.h"

using json = nlohmann::json;

// ── Helpers ──────────────────────────────────────────────────

static std::string UrlDecode(const std::string &str)
{
    std::string result;
    for (size_t i = 0; i < str.size(); i++)
    {
        if (str[i] == '%' && i + 2 < str.size())
        {
            int val = std::stoi(str.substr(i + 1, 2), nullptr, 16);
            result += (char)val;
            i += 2;
        }
        else if (str[i] == '+')
        {
            result += ' ';
        }
        else
        {
            result += str[i];
        }
    }
    return result;
}

static void AddCors(crow::response &res)
{
    res.add_header("Access-Control-Allow-Origin", "*");
    res.add_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS, PUT, DELETE");
    res.add_header("Access-Control-Allow-Headers", "Content-Type, Authorization, Accept");
}

static std::string FormatPerms(const char perm[3], char type)
{
    std::string s;
    s += (type == '0') ? 'd' : '-';
    for (int i = 0; i < 3; i++)
    {
        int v = perm[i] - '0';
        s += (v & 4) ? 'r' : '-';
        s += (v & 2) ? 'w' : '-';
        s += (v & 1) ? 'x' : '-';
    }
    return s;
}

// ─────────────────────────────────────────────────────────────
int main()
{
    std::cout << "   ExtreamFS P2 - Backend C++ + Crow HTTP\n";
    std::cout << "   Puerto : 18080\n";
    std::cout << "========================================\n";

    crow::SimpleApp app;

    CROW_CATCHALL_ROUTE(app)
    ([](const crow::request &req)
     {
        crow::response res;
        AddCors(res);
        if (req.method == crow::HTTPMethod::OPTIONS) {
            res.code = 204;
            return res;
        }
        res.code = 404;
        res.body = "{\"error\":\"ruta no encontrada\"}";
        res.add_header("Content-Type", "application/json");
        return res; });

    // ── POST /execute ────────────────────────────────────────
    CROW_ROUTE(app, "/execute").methods(crow::HTTPMethod::POST, crow::HTTPMethod::OPTIONS)([](const crow::request &req)
                                                                                           {
        crow::response res;
        AddCors(res);
        res.add_header("Content-Type", "application/json");

        if (req.method == crow::HTTPMethod::OPTIONS) {
            res.code = 204;
            return res;
        }

        try {
            auto body = json::parse(req.body);
            std::string commands = body["commands"];
            std::string output   = Analyzer::AnalyzeScript(commands);

            
            std::istringstream ss(output);
            std::string line, cleanOutput;
            json reports = json::array();
            while (std::getline(ss, line)) {
                if (line.rfind("REPORTE:", 0) == 0) {
                    std::string ruta = line.substr(8);
                    while (!ruta.empty() && ruta[0] == ' ') ruta.erase(0,1);
                    std::string filename = std::filesystem::path(ruta).filename().string();
                    reports.push_back({{"filename", filename}, {"ruta", ruta}});
                } else {
                    cleanOutput += line + "\n";
                }
            }
            res.code = 200;
            res.body = json({{"output", cleanOutput}, {"reports", reports}}).dump();
        } catch (const std::exception& e) {
            res.code = 400;
            res.body = json({{"error", e.what()}}).dump();
        }
        return res; });

    // ── GET /health ──────────────────────────────────────────
    CROW_ROUTE(app, "/health")
    ([]()
     {
        json r = {{"status", "ok"}, {"backend", "ExtreamFS P2 C++"}};
        auto res = crow::response(200, r.dump());
        res.add_header("Content-Type", "application/json");
        AddCors(res);
        return res; });

    // ── GET /fs/session ──────────────────────────────────────
    CROW_ROUTE(app, "/fs/session")
    ([]()
     {
        auto& s = UserSession::currentSession;
        json r = {
            {"active",    s.active},
            {"username",  s.username},
            {"group",     s.group},
            {"uid",       s.uid},
            {"partId",    s.partId},
            {"diskPath",  s.diskPath},
            {"partStart", s.partStart}
        };
        auto res = crow::response(200, r.dump());
        res.add_header("Content-Type", "application/json");
        AddCors(res);
        return res; });

    // ── GET /get_disks ───────────────────────────────────────
    // Lista los discos con particiones montadas (para DiskSelector)
    CROW_ROUTE(app, "/get_disks").methods(crow::HTTPMethod::GET, crow::HTTPMethod::OPTIONS)([](const crow::request &req)
                                                                                            {
        crow::response res;
        AddCors(res);
        res.add_header("Content-Type", "application/json");
        if (req.method == crow::HTTPMethod::OPTIONS) { res.code = 204; return res; }

        // Llamamos a la nueva función que lee el MBR y devuelve la metadata completa
        json disks = SystemInfo::GetEnrichedDisks();
        
        res.code = 200;
        res.body = disks.dump();
        return res; });

    // ── POST /get_partitions ─────────────────────────────────
    // Retorna particiones de un disco específico (para DiskSelector)
    CROW_ROUTE(app, "/get_partitions").methods(crow::HTTPMethod::POST, crow::HTTPMethod::OPTIONS)([](const crow::request &req)
                                                                                                  {
        crow::response res;
        AddCors(res);
        res.add_header("Content-Type", "application/json");
        if (req.method == crow::HTTPMethod::OPTIONS) { res.code = 204; return res; }

        auto body = json::parse(req.body);
        std::string diskName = body.value("name", "");

        // Llamamos a la función que extrae las particiones desde el MBR
        json partitions = SystemInfo::GetEnrichedPartitions(diskName);

        res.code = 200;
        res.body = partitions.dump();
        return res; });

    // VISUALIZADOR DE JOURNALING
    CROW_ROUTE(app, "/fs/journal").methods(crow::HTTPMethod::GET, crow::HTTPMethod::OPTIONS)([](const crow::request &req)
                                                                                             {
        crow::response res;
        AddCors(res);
        res.add_header("Content-Type", "application/json");
        if (req.method == crow::HTTPMethod::OPTIONS) { res.code = 204; return res; }

        // Obtenemos el ID de la partición desde la URL (ej: /fs/journal?id=401A)
        std::string id = req.url_params.get("id") ? req.url_params.get("id") : "";

        // Extraemos las operaciones de la bitácora EXT3
        json journalList = SystemInfo::GetJournaling(id);

        res.code = 200;
        res.body = journalList.dump();
        return res; });

    // ── GET /fs/browse ───────────────────────────────────────
    // Lista contenido de un directorio con metadatos completos
    // Query: ?path=/home/user
    CROW_ROUTE(app, "/fs/browse").methods(crow::HTTPMethod::GET, crow::HTTPMethod::OPTIONS)([](const crow::request &req)
                                                                                            {
        crow::response res;
        AddCors(res);
        res.add_header("Content-Type", "application/json");
        if (req.method == crow::HTTPMethod::OPTIONS) { res.code = 204; return res; }

        auto& s = UserSession::currentSession;
        if (!s.active) {
            res.code = 401;
            res.body = json({{"error", "No hay sesion activa"}}).dump();
            return res;
        }

        std::string path = "/";
        auto pathParam = req.url_params.get("path");
        if (pathParam) path = UrlDecode(std::string(pathParam));
        if (path.empty()) path = "/";

        auto file = Utilities::OpenFile(s.diskPath);
        if (!file.is_open()) {
            res.code = 500;
            res.body = json({{"error", "No se pudo abrir el disco"}}).dump();
            return res;
        }

        SuperBloque sb{};
        FileSystem::ReadSuperBloque(file, s.partStart, sb);

        auto parts = FileOperations::SplitPath(path);
        int dirInode = (path == "/") ? 0 : FileOperations::TraversePath(file, sb, parts);
        if (dirInode == -1) {
            file.close();
            res.code = 404;
            res.body = json({{"error", "Directorio no encontrado: " + path}}).dump();
            return res;
        }

        Inode dirInodeData{};
        FileSystem::ReadInode(file, sb, dirInode, dirInodeData);

        json entries = json::array();
        for (int i = 0; i < 12; i++) {
            if (dirInodeData.i_block[i] == -1) break;
            FolderBlock fb{};
            int pos = sb.s_block_start + dirInodeData.i_block[i] * sizeof(FolderBlock);
            Utilities::ReadObject(file, fb, pos);

            for (int j = 0; j < 4; j++) {
                if (fb.b_content[j].b_inodo == -1) continue;
                std::string name(fb.b_content[j].b_name,
                    strnlen(fb.b_content[j].b_name, 12));
                if (name.empty() || name == "." || name == "..") continue;

                int childIdx = fb.b_content[j].b_inodo;
                Inode childInode{};
                FileSystem::ReadInode(file, sb, childIdx, childInode);

                bool isDir = (childInode.i_type[0] == '0');
                std::string mtime(childInode.i_mtime, 19);
                std::string entryPath = (path == "/") ? "/" + name : path + "/" + name;

                entries.push_back({
                    {"name",        name},
                    {"path",        entryPath},
                    {"type",        isDir ? "directory" : "file"},
                    {"size",        childInode.i_size},
                    {"uid",         childInode.i_uid},
                    {"gid",         childInode.i_gid},
                    {"permissions", FormatPerms(childInode.i_perm, childInode.i_type[0])},
                    {"perm_octal",  std::string(childInode.i_perm, 3)},
                    {"mtime",       mtime},
                    {"inode",       childIdx}
                });
            }
        }
        file.close();

        // Carpetas primero, luego por nombre
        std::sort(entries.begin(), entries.end(), [](const json& a, const json& b) {
            bool aDir = a["type"] == "directory";
            bool bDir = b["type"] == "directory";
            if (aDir != bDir) return aDir > bDir;
            return a["name"].get<std::string>() < b["name"].get<std::string>();
        });

        res.code = 200;
        res.body = json({{"path", path}, {"entries", entries}, {"count", (int)entries.size()}}).dump();
        return res; });

    // ── GET /fs/read ─────────────────────────────────────────
    // Retorna contenido de un archivo
    CROW_ROUTE(app, "/fs/read").methods(crow::HTTPMethod::GET, crow::HTTPMethod::OPTIONS)([](const crow::request &req)
                                                                                          {
        crow::response res;
        AddCors(res);
        res.add_header("Content-Type", "application/json");
        if (req.method == crow::HTTPMethod::OPTIONS) { res.code = 204; return res; }

        auto& s = UserSession::currentSession;
        if (!s.active) {
            res.code = 401;
            res.body = json({{"error", "No hay sesion activa"}}).dump();
            return res;
        }

        auto pathParam = req.url_params.get("path");
        if (!pathParam) {
            res.code = 400;
            res.body = json({{"error", "Falta parametro path"}}).dump();
            return res;
        }
        std::string path = UrlDecode(std::string(pathParam));

        auto file = Utilities::OpenFile(s.diskPath);
        if (!file.is_open()) {
            res.code = 500;
            res.body = json({{"error", "No se pudo abrir el disco"}}).dump();
            return res;
        }

        SuperBloque sb{};
        FileSystem::ReadSuperBloque(file, s.partStart, sb);

        auto parts = FileOperations::SplitPath(path);
        int fileInodeIdx = FileOperations::TraversePath(file, sb, parts);
        if (fileInodeIdx == -1) {
            file.close();
            res.code = 404;
            res.body = json({{"error", "Archivo no encontrado: " + path}}).dump();
            return res;
        }

        Inode fileInode{};
        FileSystem::ReadInode(file, sb, fileInodeIdx, fileInode);

        if (fileInode.i_type[0] == '0') {
            file.close();
            res.code = 400;
            res.body = json({{"error", "La ruta es un directorio"}}).dump();
            return res;
        }

        std::string content;
        int remaining = fileInode.i_size;
        for (int i = 0; i < 12 && remaining > 0; i++) {
            if (fileInode.i_block[i] == -1) break;
            FileBlock fb{};
            int pos = sb.s_block_start + fileInode.i_block[i] * sizeof(FolderBlock);
            Utilities::ReadObject(file, fb, pos);
            int toRead = std::min(remaining, (int)sizeof(FileBlock));
            content.append(fb.b_content, toRead);
            remaining -= toRead;
        }
        file.close();

        std::string mtime(fileInode.i_mtime, 19);
        res.code = 200;
        res.body = json({
            {"path",        path},
            {"content",     content},
            {"size",        fileInode.i_size},
            {"uid",         fileInode.i_uid},
            {"gid",         fileInode.i_gid},
            {"permissions", FormatPerms(fileInode.i_perm, fileInode.i_type[0])},
            {"mtime",       mtime}
        }).dump();
        return res; });

    // ── GET /reports/<path> ───────────────────────────────────
    CROW_ROUTE(app, "/reports/<path>")
    ([](const std::string &filepath)
     {
        std::string fullPath = UrlDecode(filepath);
        if (fullPath.empty() || fullPath[0] != '/') {
            for (auto& dir : {"../../reports/", "../reports/", "./reports/"}) {
                if (std::filesystem::exists(dir + fullPath)) {
                    fullPath = dir + fullPath; break;
                }
            }
        }
        if (!std::filesystem::exists(fullPath)) {
            auto res = crow::response(404, "No encontrado: " + fullPath);
            AddCors(res);
            return res;
        }
        std::ifstream imgFile(fullPath, std::ios::binary);
        std::string imgData((std::istreambuf_iterator<char>(imgFile)),
                             std::istreambuf_iterator<char>());
        imgFile.close();
        std::string ext = fullPath.substr(fullPath.rfind('.') + 1);
        std::string ct  = "image/png";
        if (ext == "jpg" || ext == "jpeg") ct = "image/jpeg";
        else if (ext == "txt")             ct = "text/plain; charset=utf-8";
        else if (ext == "pdf")             ct = "application/pdf";
        auto res = crow::response(200, imgData);
        res.add_header("Content-Type", ct);
        AddCors(res);
        return res; });

    std::cout << "Servidor en http://localhost:18080\n";
    std::cout << "POST /execute\n";
    std::cout << "GET  /health\n";
    std::cout << "GET  /fs/session\n";
    std::cout << "GET  /get_disks\n";
    std::cout << "POST /get_partitions\n";
    std::cout << "GET  /fs/browse?path=/\n";
    std::cout << "GET  /fs/read?path=/file.txt\n";
    std::cout << "GET  /fs/journal\n\n";
    app.port(18080).multithreaded().run();
    return 0;
}