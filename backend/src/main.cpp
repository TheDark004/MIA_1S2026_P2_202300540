#include <iostream>
#include <string>
#include <fstream>
#include <filesystem>
#include <vector>

#define CROW_MAIN
#include "../../external/crow_all.h"
#include <nlohmann/json.hpp>
#include "Analyzer/Analyzer.h"

using json = nlohmann::json;

// Decodificar URL: %2F -> /, %20 -> espacio, etc.
static std::string UrlDecode(const std::string& str) {
    std::string result;
    for (size_t i = 0; i < str.size(); i++) {
        if (str[i] == '%' && i + 2 < str.size()) {
            int val = std::stoi(str.substr(i + 1, 2), nullptr, 16);
            result += (char)val;
            i += 2;
        } else if (str[i] == '+') {
            result += ' ';
        } else {
            result += str[i];
        }
    }
    return result;
}

int main() {
    std::cout << "   ExtreamFS — Backend C++ + Crow HTTP\n";
    std::cout << "   Puerto : 18080\n";
    std::cout << "========================================\n";

    crow::SimpleApp app;

    // POST /execute
    CROW_ROUTE(app, "/execute").methods(crow::HTTPMethod::POST)
    ([](const crow::request& req) {
        auto body = json::parse(req.body, nullptr, false);
        if (body.is_discarded() || !body.contains("commands")) {
            json err = {{"output", "Error: JSON inválido o falta campo 'commands'"}};
            auto res = crow::response(400, err.dump());
            res.add_header("Content-Type", "application/json");
            res.add_header("Access-Control-Allow-Origin", "*");
            return res;
        }
        std::string commands = body["commands"].get<std::string>();
        std::string output   = Analyzer::AnalyzeScript(commands);
        json response = {{"output", output}};
        auto res = crow::response(200, response.dump());
        res.add_header("Content-Type", "application/json");
        res.add_header("Access-Control-Allow-Origin", "*");
        return res;
    });

    // OPTIONS /execute — preflight CORS
    CROW_ROUTE(app, "/execute").methods(crow::HTTPMethod::OPTIONS)
    ([]() {
        auto res = crow::response(200);
        res.add_header("Access-Control-Allow-Origin",  "*");
        res.add_header("Access-Control-Allow-Methods", "POST, OPTIONS");
        res.add_header("Access-Control-Allow-Headers", "Content-Type");
        return res;
    });

    // GET /health
    CROW_ROUTE(app, "/health")
    ([]() {
        json r = {{"status", "ok"}, {"backend", "ExtreamFS C++"}};
        auto res = crow::response(200, r.dump());
        res.add_header("Content-Type", "application/json");
        res.add_header("Access-Control-Allow-Origin", "*");
        return res;
    });

    // GET /reports/<path> — sirve imágenes y reportes
    CROW_ROUTE(app, "/reports/<path>")
    ([](const std::string& filepath) {
        // Decodificar %2F -> / etc.
        std::string fullPath = UrlDecode(filepath);

        // Si no es ruta absoluta, buscar en ubicaciones comunes
        if (fullPath.empty() || fullPath[0] != '/') {
            std::vector<std::string> dirs = {
                "../../reports/",
                "../reports/",
                "./reports/",
            };
            for (auto& dir : dirs) {
                if (std::filesystem::exists(dir + fullPath)) {
                    fullPath = dir + fullPath;
                    break;
                }
            }
        }

        if (!std::filesystem::exists(fullPath)) {
            auto res = crow::response(404, "No encontrado: " + fullPath);
            res.add_header("Access-Control-Allow-Origin", "*");
            return res;
        }

        // Leer archivo
        std::ifstream imgFile(fullPath, std::ios::binary);
        std::string imgData((std::istreambuf_iterator<char>(imgFile)),
                             std::istreambuf_iterator<char>());
        imgFile.close();

        // Tipo de contenido según extensión
        std::string ext = fullPath.substr(fullPath.rfind('.') + 1);
        std::string contentType = "image/png";
        if      (ext == "jpg" || ext == "jpeg") contentType = "image/jpeg";
        else if (ext == "txt")                  contentType = "text/plain; charset=utf-8";
        else if (ext == "pdf")                  contentType = "application/pdf";

        auto res = crow::response(200, imgData);
        res.add_header("Content-Type", contentType);
        res.add_header("Access-Control-Allow-Origin", "*");
        return res;
    });

    std::cout << "Servidor iniciado en http://localhost:18080\n";
    std::cout << "POST http://localhost:18080/execute\n";
    std::cout << "GET  http://localhost:18080/health\n";
    std::cout << "GET  http://localhost:18080/reports/<ruta>\n\n";
    app.port(18080).multithreaded().run();

    return 0;
}