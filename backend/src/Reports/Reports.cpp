#include "Reports.h"
#include "../DiskManagement/DiskManagement.h"
#include "../FileSystem/FileSystem.h"
#include "../FileOperations/FileOperations.h"
#include "../Utilities/Utilities.h"
#include "../Structs/Structs.h"
#include <sstream>
#include <fstream>
#include <cstring>
#include <cstdlib>
#include <filesystem>
#include <algorithm>
#include <set>

namespace Reports {


// Helper: detectar extensión del archivo de salida
static std::string GetExtension(const std::string& path) {
    auto pos = path.rfind('.');
    if (pos == std::string::npos) return "";
    std::string ext = path.substr(pos + 1);
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    return ext;
}


// Helper: escribir texto plano (para .txt)
static bool WriteText(const std::string& content, const std::string& outputPath) {
    std::filesystem::create_directories(
        std::filesystem::path(outputPath).parent_path()
    );
    std::ofstream f(outputPath);
    if (!f.is_open()) return false;
    f << content;
    f.close();
    return true;
}

// ejecutar Graphviz y generar imagen
static bool GenerateImage(const std::string& dotContent,
                          const std::string& outputPath) {
    std::filesystem::create_directories(
        std::filesystem::path(outputPath).parent_path()
    );

    std::string ext = GetExtension(outputPath);
    std::string format = "png";
    if (ext == "jpg" || ext == "jpeg") format = "jpg";
    else if (ext == "pdf") format = "pdf";
    else if (ext == "svg") format = "svg";

    std::string dotPath = outputPath + ".dot";
    std::ofstream dotFile(dotPath);
    if (!dotFile.is_open()) return false;
    dotFile << dotContent;
    dotFile.close();

    std::string cmd = "dot -T" + format + " \"" + dotPath + "\" -o \"" + outputPath + "\" 2>/dev/null";
    int result = std::system(cmd.c_str());
    std::filesystem::remove(dotPath);
    return result == 0;
}


// obtiene partStart desde ID

static int GetPartStart(std::fstream& file, const std::string& id) {
    MBR mbr{};
    Utilities::ReadObject(file, mbr, 0);
    for (int i = 0; i < 4; i++) {
        std::string partId(mbr.Partitions[i].Id, 4);
        while (!partId.empty() && partId.back() == 0) partId.pop_back();
        if (partId == id) return mbr.Partitions[i].Start;
    }
    return -1;
}


// REPORTE MBR
// Muestra el MBR del disco y sus particiones - Si hay partición extendida, también muestra los EBR.

static std::string ReportMbr(std::fstream& file, const std::string& diskPath,
                              const std::string& ruta) {
    MBR mbr{};
    Utilities::ReadObject(file, mbr, 0);

    std::ostringstream dot;
    dot << "digraph G {\n";
    dot << "  node [shape=none fontname=\"Helvetica\" fontsize=11]\n";
    dot << "  rankdir=TB\n";

    // Tabla principal MBR
    dot << "  mbr [label=<\n";
    dot << "    <TABLE BORDER='1' CELLBORDER='1' CELLSPACING='0' BGCOLOR='white'>\n";
    dot << "    <TR><TD COLSPAN='2' BGCOLOR='#4a235a'><FONT COLOR='white'><B> REPORTE DE MBR </B></FONT></TD></TR>\n";
    dot << "    <TR><TD BGCOLOR='#d7bde2'>mbr_tamano</TD><TD>" << mbr.MbrSize << "</TD></TR>\n";

    std::string date(mbr.CreationDate, 19);
    dot << "    <TR><TD BGCOLOR='#d7bde2'>mbr_fecha_creacion</TD><TD>" << date << "</TD></TR>\n";
    dot << "    <TR><TD BGCOLOR='#d7bde2'>mbr_disk_signature</TD><TD>" << mbr.Signature << "</TD></TR>\n";

    for (int i = 0; i < 4; i++) {
        if (mbr.Partitions[i].Start == -1) continue;

        std::string tipo = std::string(1, mbr.Partitions[i].Type[0]);
        bool isExtended = (tipo == "e" || tipo == "E");

        std::string headerColor = isExtended ? "#c0392b" : "#4a235a";
        std::string rowColor    = isExtended ? "#f9ebea" : "#e8daef";
        std::string headerLabel = isExtended ? "Particion Extendida" : "Particion";

        dot << "    <TR><TD COLSPAN='2' BGCOLOR='" << headerColor << "'>"
            << "<FONT COLOR='white'><B> " << headerLabel << " </B></FONT></TD></TR>\n";
        dot << "    <TR><TD BGCOLOR='" << rowColor << "'>part_status</TD>"
            << "<TD>" << mbr.Partitions[i].Status[0] << "</TD></TR>\n";
        dot << "    <TR><TD BGCOLOR='" << rowColor << "'>part_type</TD>"
            << "<TD>" << tipo << "</TD></TR>\n";
        dot << "    <TR><TD BGCOLOR='" << rowColor << "'>part_fit</TD>"
            << "<TD>" << std::string(mbr.Partitions[i].Fit, 1) << "</TD></TR>\n";
        dot << "    <TR><TD BGCOLOR='" << rowColor << "'>part_start</TD>"
            << "<TD>" << mbr.Partitions[i].Start << "</TD></TR>\n";
        dot << "    <TR><TD BGCOLOR='" << rowColor << "'>part_size</TD>"
            << "<TD>" << mbr.Partitions[i].Size << "</TD></TR>\n";
        dot << "    <TR><TD BGCOLOR='" << rowColor << "'>part_name</TD>"
            << "<TD>" << std::string(mbr.Partitions[i].Name) << "</TD></TR>\n";

        // Si es extendida lee EBRs
        if (isExtended) {
            int ebrPos = mbr.Partitions[i].Start;
            while (ebrPos != -1) {
                EBR ebr{};
                Utilities::ReadObject(file, ebr, ebrPos);
                if (ebr.Size <= 0) break;

                dot << "    <TR><TD COLSPAN='2' BGCOLOR='#e74c3c'>"
                    << "<FONT COLOR='white'><B> Particion Logica </B></FONT></TD></TR>\n";
                dot << "    <TR><TD BGCOLOR='#fadbd8'>part_status</TD>"
                    << "<TD>" << ebr.Mount[0] << "</TD></TR>\n";
                dot << "    <TR><TD BGCOLOR='#fadbd8'>part_next</TD>"
                    << "<TD>" << ebr.Next << "</TD></TR>\n";
                dot << "    <TR><TD BGCOLOR='#fadbd8'>part_fit</TD>"
                    << "<TD>" << std::string(ebr.Fit, 1) << "</TD></TR>\n";
                dot << "    <TR><TD BGCOLOR='#fadbd8'>part_start</TD>"
                    << "<TD>" << ebr.Start << "</TD></TR>\n";
                dot << "    <TR><TD BGCOLOR='#fadbd8'>part_size</TD>"
                    << "<TD>" << ebr.Size << "</TD></TR>\n";
                dot << "    <TR><TD BGCOLOR='#fadbd8'>part_name</TD>"
                    << "<TD>" << std::string(ebr.Name) << "</TD></TR>\n";

                ebrPos = ebr.Next;
            }
        }
    }

    dot << "    </TABLE>>]\n";
    dot << "}\n";

    GenerateImage(dot.str(), ruta);
    return ruta;
}


// REPORTE DISK : Diagrama proporcional del disco
// Muestra MBR | particiones | espacio libre con % del total

static std::string ReportDisk(std::fstream& file, const std::string& diskPath,
                               const std::string& ruta) {
    MBR mbr{};
    Utilities::ReadObject(file, mbr, 0);
 
    // Nombre del disco
    std::string diskName = std::filesystem::path(diskPath).filename().string();
 
    // Calcular espacio usado y libre
    int totalSize = mbr.MbrSize;
    int mbrBytes  = sizeof(MBR);
 
    // Recolectar segmentos: [start, size, label, type]
    struct Segment {
        int start, size;
        std::string label, color;
    };
    std::vector<Segment> segments;
 
    // MBR siempre primero
    segments.push_back({0, mbrBytes, "MBR", "#aed6f1"});
 
    // Particiones del MBR
    for (int i = 0; i < 4; i++) {
        if (mbr.Partitions[i].Start == -1) continue;
        std::string tipo(1, mbr.Partitions[i].Type[0]);
        std::string name(mbr.Partitions[i].Name);
        std::string color = (tipo == "p" || tipo == "P") ? "#a9dfbf" :
                            (tipo == "e" || tipo == "E") ? "#f9e79f" : "#f1948a";
        double pct = 100.0 * mbr.Partitions[i].Size / totalSize;
        char buf[64];
        std::snprintf(buf, sizeof(buf), "%.1f%%", pct);
        segments.push_back({mbr.Partitions[i].Start, mbr.Partitions[i].Size,
                             name + "<BR/>" + std::string(buf), color});
    }
 
    // Ordenar por start
    std::sort(segments.begin(), segments.end(),
              [](const Segment& a, const Segment& b){ return a.start < b.start; });
 
    // Agregar segmentos libres entre particiones
    std::vector<Segment> all;
    int cursor = 0;
    for (auto& seg : segments) {
        if (seg.start > cursor) {
            double pct = 100.0 * (seg.start - cursor) / totalSize;
            char buf[64]; std::snprintf(buf, sizeof(buf), "%.1f%%", pct);
            all.push_back({cursor, seg.start - cursor, "Libre<BR/>" + std::string(buf), "#d5d8dc"});
        }
        all.push_back(seg);
        cursor = seg.start + seg.size;
    }
    if (cursor < totalSize) {
        double pct = 100.0 * (totalSize - cursor) / totalSize;
        char buf[64]; std::snprintf(buf, sizeof(buf), "%.1f%%", pct);
        all.push_back({cursor, totalSize - cursor, "Libre<BR/>" + std::string(buf), "#d5d8dc"});
    }
 
    // Generar SVG-like con Graphviz como tabla horizontal
    std::ostringstream dot;
    dot << "digraph G {\n";
    dot << "  rankdir=LR\n";
    dot << "  node [shape=none margin=0]\n";
    dot << "  disk [label=<\n";
    dot << "    <TABLE BORDER='1' CELLBORDER='1' CELLSPACING='0'>\n";
    dot << "    <TR><TD COLSPAN='" << all.size() << "' BGCOLOR='#2c3e50'>"
        << "<FONT COLOR='white'><B> " << diskName << " </B></FONT></TD></TR>\n";
    dot << "    <TR>\n";
    for (auto& seg : all) {
        dot << "      <TD BGCOLOR='" << seg.color << "' WIDTH='"
            << std::max(40, seg.size * 800 / totalSize) << "'>"
            << "<FONT POINT-SIZE='9'>" << seg.label << "</FONT></TD>\n";
    }
    dot << "    </TR></TABLE>>]\n";
    dot << "}\n";
 
    GenerateImage(dot.str(), ruta);
    return ruta;
}


// REPORTE SB : SuperBloque

static std::string ReportSb(std::fstream& file, const SuperBloque& sb,
                             const std::string& diskPath, const std::string& ruta) {
    std::string diskName = std::filesystem::path(diskPath).filename().string();

    std::ostringstream dot;
    dot << "digraph G {\n";
    dot << "  node [shape=none fontname=\"Helvetica\" fontsize=11]\n";
    dot << "  sb [label=<\n";
    dot << "    <TABLE BORDER='1' CELLBORDER='1' CELLSPACING='0' BGCOLOR='white'>\n";
    dot << "    <TR><TD COLSPAN='2' BGCOLOR='#1e8449'>"
        << "<FONT COLOR='white'><B> Reporte de SUPERBLOQUE </B></FONT></TD></TR>\n";

    auto row = [&](const std::string& k, const std::string& v, bool alt) {
        std::string bg = alt ? "#a9dfbf" : "#d5f5e3";
        dot << "    <TR><TD BGCOLOR='" << bg << "' ALIGN='RIGHT'>" << k << "</TD>"
            << "<TD BGCOLOR='white'>" << v << "</TD></TR>\n";
    };

    row("sb_nombre_hd",      diskName,                          true);
    row("sb_inodos_count",   std::to_string(sb.s_inodes_count), false);
    row("sb_bloques_count",  std::to_string(sb.s_blocks_count), true);
    row("sb_inodos_free",    std::to_string(sb.s_free_inodes_count), false);
    row("sb_bloques_free",   std::to_string(sb.s_free_blocks_count), true);
    row("sb_magic_num",      "0xEF53",                          false);
    row("sb_inode_size",     std::to_string(sb.s_inode_size),   true);
    row("sb_block_size",     std::to_string(sb.s_block_size),   false);
    row("sb_first_ino",      std::to_string(sb.s_first_ino),    true);
    row("sb_first_blo",      std::to_string(sb.s_first_blo),    false);
    row("sb_montajes_count", std::to_string(sb.s_mnt_count),    true);
    row("sb_ap_bitmap_inodos",  std::to_string(sb.s_bm_inode_start), false);
    row("sb_ap_bitmap_bloques", std::to_string(sb.s_bm_block_start), true);
    row("sb_ap_inodos",      std::to_string(sb.s_inode_start),  false);
    row("sb_ap_bloques",     std::to_string(sb.s_block_start),  true);

    dot << "    </TABLE>>]\n}\n";
    GenerateImage(dot.str(), ruta);
    return ruta;
}


// REPORTE INODE : Inodos usados conectados en cadena

static std::string ReportInode(std::fstream& file, const SuperBloque& sb,
                                const std::string& ruta) {
    std::ostringstream dot;
    dot << "digraph G {\n";
    dot << "  rankdir=LR\n";
    dot << "  node [shape=none fontname=\"Helvetica\" fontsize=10]\n";
    dot << "  graph [splines=ortho ranksep=1.5 nodesep=0.5]\n";

    std::vector<int> usedInodes;

    for (int i = 0; i < sb.s_inodes_count; i++) {
        int bytePos = i / 8, bitPos = 7 - (i % 8);
        char bitmapByte;
        file.seekg(sb.s_bm_inode_start + bytePos);
        file.read(&bitmapByte, 1);
        if (!((bitmapByte >> bitPos) & 1)) continue;

        usedInodes.push_back(i);
        Inode inode{};
        FileSystem::ReadInode(file, sb, i, inode);

        bool isDir = (inode.i_type[0] == '0');
        std::string tipo = isDir ? "0 (carpeta)" : "1 (archivo)";
        std::string hdrBg = isDir ? "#2E75B6" : "#3A8A3A";
        std::string bg    = isDir ? "#aed6f1"  : "#a9dfbf";
        std::string atime(inode.i_atime, 19);
        std::string mtime(inode.i_mtime, 19);
        std::string perm(inode.i_perm, 3);

        dot << "  inode" << i << " [label=<\n";
        dot << "    <TABLE BORDER='1' CELLBORDER='1' CELLSPACING='0' BGCOLOR='white'>\n";
        dot << "    <TR><TD COLSPAN='2' BGCOLOR='" << hdrBg << "'>"
            << "<FONT COLOR='white'><B>I-nodo " << i << "</B></FONT></TD></TR>\n";
        dot << "    <TR><TD BGCOLOR='" << bg << "'>I_TYPE</TD><TD>" << tipo << "</TD></TR>\n";
        dot << "    <TR><TD BGCOLOR='" << bg << "'>I_UID</TD><TD>" << inode.i_uid << "</TD></TR>\n";
        dot << "    <TR><TD BGCOLOR='" << bg << "'>I_GID</TD><TD>" << inode.i_gid << "</TD></TR>\n";
        dot << "    <TR><TD BGCOLOR='" << bg << "'>I_SIZE</TD><TD>" << inode.i_size << "</TD></TR>\n";
        dot << "    <TR><TD BGCOLOR='" << bg << "'>I_ATIME</TD><TD>" << atime << "</TD></TR>\n";
        dot << "    <TR><TD BGCOLOR='" << bg << "'>I_MTIME</TD><TD>" << mtime << "</TD></TR>\n";
        dot << "    <TR><TD BGCOLOR='" << bg << "'>I_PERM</TD><TD>" << perm << "</TD></TR>\n";
        // Mostrar todos los bloques con PORT para hacer flechas
        for (int j = 0; j < 15; j++) {
            std::string label = (j < 12) ? "ap" + std::to_string(j) :
                                (j == 12) ? "ap_ind" :
                                (j == 13) ? "ap_dbl" : "ap_tri";
            dot << "    <TR><TD BGCOLOR='" << bg << "'>" << label << "</TD>"
                << "<TD PORT='b" << j << "'>" << inode.i_block[j] << "</TD></TR>\n";
        }
        dot << "    </TABLE>>]\n\n";

        // Dibuja los bloques apuntados y conectarlO
        for (int j = 0; j < 12; j++) {
            if (inode.i_block[j] == -1) continue;
            int blkIdx = inode.i_block[j];
            int pos = sb.s_block_start + blkIdx * sizeof(FolderBlock);

            // Dibuja el bloque
            if (isDir) {
                FolderBlock fb{};
                Utilities::ReadObject(file, fb, pos);
                dot << "  blk_" << i << "_" << j << " [label=<\n";
                dot << "    <TABLE BORDER='1' CELLBORDER='1' CELLSPACING='0' BGCOLOR='white'>\n";
                dot << "    <TR><TD COLSPAN='2' BGCOLOR='#E67E22'>"
                    << "<FONT COLOR='white'><B>Bloque Carpeta " << blkIdx << "</B></FONT></TD></TR>\n";
                dot << "    <TR><TD><B>b_name</B></TD><TD><B>b_inodo</B></TD></TR>\n";
                for (int k = 0; k < 4; k++) {
                    std::string nm(fb.b_content[k].b_name);
                    std::string inStr = std::to_string(fb.b_content[k].b_inodo);
                    if (fb.b_content[k].b_inodo == -1) { nm = "-"; inStr = "-1"; }
                    dot << "    <TR><TD>" << nm << "</TD><TD>" << inStr << "</TD></TR>\n";
                }
                dot << "    </TABLE>>]\n";
            } else {
                FileBlock fb{};
                Utilities::ReadObject(file, fb, pos);
                std::string ct(fb.b_content, 64);
                ct.erase(std::find(ct.begin(), ct.end(), '\0'), ct.end());
                std::string esc;
                for (char c : ct) {
                    if (c == '<') esc += "&lt;";
                    else if (c == '>') esc += "&gt;";
                    else if (c == '&') esc += "&amp;";
                    else if (c == '\n') esc += "\\n";
                    else esc += c;
                }
                if (esc.size() > 32) esc = esc.substr(0, 32) + "...";
                dot << "  blk_" << i << "_" << j << " [label=<\n";
                dot << "    <TABLE BORDER='1' CELLBORDER='1' CELLSPACING='0' BGCOLOR='white'>\n";
                dot << "    <TR><TD BGCOLOR='#27AE60'>"
                    << "<FONT COLOR='white'><B>Bloque Archivo " << blkIdx << "</B></FONT></TD></TR>\n";
                dot << "    <TR><TD>" << esc << "</TD></TR>\n";
                dot << "    </TABLE>>]\n";
            }
            dot << "  inode" << i << ":b" << j << " -> blk_" << i << "_" << j
                << " [color=\"#e67e22\"]\n";
        }
    }

    // Conectar inodos en cadena
    for (int i = 0; i + 1 < (int)usedInodes.size(); i++) {
        dot << "  inode" << usedInodes[i] << " -> inode" << usedInodes[i+1]
            << " [color=\"#2980b9\" style=dashed]\n";
    }
    dot << "}\n";
    GenerateImage(dot.str(), ruta);
    return ruta;
}


// REPORTE BLOCK : Bloques usados (carpeta, archivo, apuntador)

static std::string ReportBlock(std::fstream& file, const SuperBloque& sb,
                                const std::string& ruta) {
    std::ostringstream dot;
    dot << "digraph G {\n";
    dot << "  rankdir=LR\n";
    dot << "  node [shape=none fontname=\"Helvetica\" fontsize=10]\n";

    std::vector<int> usedBlocks;

    for (int i = 0; i < sb.s_blocks_count; i++) {
        int bytePos = i / 8, bitPos = 7 - (i % 8);
        char bitmapByte;
        file.seekg(sb.s_bm_block_start + bytePos);
        file.read(&bitmapByte, 1);
        if (!((bitmapByte >> bitPos) & 1)) continue;
        usedBlocks.push_back(i);

        int pos = sb.s_block_start + i * sizeof(FolderBlock);
        FolderBlock fb{};
        Utilities::ReadObject(file, fb, pos);

        // Detectar tipo por contenido
        bool isFolder = false;
        for (int j = 0; j < 4; j++) {
            std::string n(fb.b_content[j].b_name);
            if (n == "." || n == "..") { isFolder = true; break; }
        }

        dot << "  block" << i << " [label=<\n";
        dot << "    <TABLE BORDER='1' CELLBORDER='1' CELLSPACING='0' BGCOLOR='white'>\n";

        if (isFolder) {
            dot << "    <TR><TD COLSPAN='2' BGCOLOR='#aed6f1'>"
                << "<B>Bloque Carpeta " << i << "</B></TD></TR>\n";
            dot << "    <TR><TD><B>b_name</B></TD><TD><B>b_inodo</B></TD></TR>\n";
            for (int j = 0; j < 4; j++) {
                std::string nm(fb.b_content[j].b_name);
                std::string inodeStr = std::to_string(fb.b_content[j].b_inodo);
                if (fb.b_content[j].b_inodo == -1) { nm = "-"; inodeStr = "-1"; }
                dot << "    <TR><TD>" << nm << "</TD><TD>" << inodeStr << "</TD></TR>\n";
            }
        } else {
            // FileBlock
            FileBlock fileBlk{};
            Utilities::ReadObject(file, fileBlk, pos);
            std::string content(fileBlk.b_content, 64);
            content.erase(std::find(content.begin(), content.end(), '\0'), content.end());
            std::string escaped;
            for (char c : content) {
                if (c == '<') escaped += "&lt;";
                else if (c == '>') escaped += "&gt;";
                else if (c == '&') escaped += "&amp;";
                else if (c == '\n') escaped += "\\n";
                else escaped += c;
            }
            dot << "    <TR><TD BGCOLOR='#a9dfbf'>"
                << "<B>Bloque Archivo " << i << "</B></TD></TR>\n";
            dot << "    <TR><TD>" << escaped << "</TD></TR>\n";
        }
        dot << "    </TABLE>>]\n";
    }

    for (int i = 0; i + 1 < (int)usedBlocks.size(); i++) {
        dot << "  block" << usedBlocks[i] << " -> block" << usedBlocks[i+1]
            << " [color=\"#2980b9\"]\n";
    }
    dot << "}\n";
    GenerateImage(dot.str(), ruta);
    return ruta;
}


// REPORTE BM_INODE : Bitmap de inodos como texto plano

static std::string ReportBmInode(std::fstream& file, const SuperBloque& sb,
                                  const std::string& ruta) {
    std::ostringstream txt;
    int cols = 20;
    for (int i = 0; i < sb.s_inodes_count; i++) {
        int bytePos = i / 8, bitPos = 7 - (i % 8);
        char bitmapByte;
        file.seekg(sb.s_bm_inode_start + bytePos);
        file.read(&bitmapByte, 1);
        txt << ((bitmapByte >> bitPos) & 1);
        if ((i + 1) % cols == 0) txt << "\n";
        else txt << " ";
    }
    WriteText(txt.str(), ruta);
    return ruta;
}


// REPORTE BM_BLOCK : Bitmap de bloques como texto plano

static std::string ReportBmBlock(std::fstream& file, const SuperBloque& sb,
                                  const std::string& ruta) {
    std::ostringstream txt;
    int cols = 20;
    for (int i = 0; i < sb.s_blocks_count; i++) {
        int bytePos = i / 8, bitPos = 7 - (i % 8);
        char bitmapByte;
        file.seekg(sb.s_bm_block_start + bytePos);
        file.read(&bitmapByte, 1);
        txt << ((bitmapByte >> bitPos) & 1);
        if ((i + 1) % cols == 0) txt << "\n";
        else txt << " ";
    }
    WriteText(txt.str(), ruta);
    return ruta;
}


// REPORTE TREE : EXT2

static std::set<int> visitedNodes;

static void TreeNodeFull(std::fstream& file, const SuperBloque& sb,
                         int inodeNum, const std::string& name,
                         std::ostringstream& dot) {

    if (visitedNodes.count(inodeNum)) return;
    visitedNodes.insert(inodeNum);

    Inode inode{};
    FileSystem::ReadInode(file, sb, inodeNum, inode);
    bool isDir = (inode.i_type[0] == '0');

    std::string hdrBg = isDir ? "#2E75B6" : "#3A8A3A";
    std::string bg    = isDir ? "#D6EAF8" : "#D5F5E3";
    std::string tipo  = isDir ? "0 (carpeta)" : "1 (archivo)";
    std::string display = name.empty() ? "/" : name;
    std::string perm(inode.i_perm, 3);

    // Nodo inodo
    dot << "  node" << inodeNum << " [label=<\n";
    dot << "    <TABLE BORDER='1' CELLBORDER='1' CELLSPACING='0' BGCOLOR='white'>\n";
    dot << "    <TR><TD COLSPAN='2' BGCOLOR='" << hdrBg << "'>"
        << "<FONT COLOR='white'><B>" << display << " (inodo " << inodeNum << ")</B></FONT></TD></TR>\n";
    dot << "    <TR><TD BGCOLOR='" << bg << "'>I_TYPE</TD><TD>" << tipo << "</TD></TR>\n";
    dot << "    <TR><TD BGCOLOR='" << bg << "'>I_UID</TD><TD>" << inode.i_uid << "</TD></TR>\n";
    dot << "    <TR><TD BGCOLOR='" << bg << "'>I_GID</TD><TD>" << inode.i_gid << "</TD></TR>\n";
    dot << "    <TR><TD BGCOLOR='" << bg << "'>I_SIZE</TD><TD>" << inode.i_size << "</TD></TR>\n";
    dot << "    <TR><TD BGCOLOR='" << bg << "'>I_PERM</TD><TD>" << perm << "</TD></TR>\n";
    for (int j = 0; j < 15; j++) {
        if (inode.i_block[j] == -1) continue;
        std::string lbl = (j < 12) ? "ap" + std::to_string(j) :
                          (j == 12) ? "ap_ind" :
                          (j == 13) ? "ap_dbl" : "ap_tri";
        dot << "    <TR><TD BGCOLOR='" << bg << "'>" << lbl << "</TD>"
            << "<TD PORT='p" << j << "'>" << inode.i_block[j] << "</TD></TR>\n";
    }
    dot << "    </TABLE>> shape=none]\n\n";

    // Dibujar bloques y conectarlos al inodo
    for (int j = 0; j < 12; j++) {
        if (inode.i_block[j] == -1) continue;
        int blkIdx = inode.i_block[j];
        int pos = sb.s_block_start + blkIdx * sizeof(FolderBlock);
        std::string blkNodeId = "blk" + std::to_string(blkIdx);

        if (isDir) {
            FolderBlock fb{};
            Utilities::ReadObject(file, fb, pos);

            dot << "  " << blkNodeId << " [label=<\n";
            dot << "    <TABLE BORDER='1' CELLBORDER='1' CELLSPACING='0' BGCOLOR='white'>\n";
            dot << "    <TR><TD COLSPAN='2' BGCOLOR='#E67E22'>"
                << "<FONT COLOR='white'><B>Bloque Carpeta " << blkIdx << "</B></FONT></TD></TR>\n";
            dot << "    <TR><TD><B>b_name</B></TD><TD><B>b_inodo</B></TD></TR>\n";

            std::vector<std::pair<int,std::string>> children;
            for (int k = 0; k < 4; k++) {
                std::string nm(fb.b_content[k].b_name);
                int bIno = fb.b_content[k].b_inodo;
                std::string inStr = (bIno == -1) ? "-1" : std::to_string(bIno);
                if (bIno == -1) nm = "-";
                dot << "    <TR><TD>" << (nm.empty() ? "-" : nm)
                    << "</TD><TD PORT='c" << k << "'>" << inStr << "</TD></TR>\n";
                if (bIno != -1 && nm != "." && nm != ".." && !nm.empty())
                    children.emplace_back(bIno, nm);
            }
            dot << "    </TABLE>> shape=none]\n\n";
            dot << "  node" << inodeNum << ":p" << j << " -> " << blkNodeId
                << " [color=\"#e67e22\"]\n";

            // Recursión sobre hijos
            for (int ci = 0; ci < (int)children.size(); ci++) {
                auto& [childIno, childName] = children[ci];
                dot << "  " << blkNodeId << ":c" << ci << " -> node" << childIno
                    << " [color=\"#3498db\"]\n";
                TreeNodeFull(file, sb, childIno, childName, dot);
            }
        } else {
            // Archivo
            FileBlock fb{};
            Utilities::ReadObject(file, fb, pos);
            std::string ct(fb.b_content, 64);
            ct.erase(std::find(ct.begin(), ct.end(), '\0'), ct.end());
            std::string esc;
            for (char c : ct) {
                if (c == '<') esc += "&lt;";
                else if (c == '>') esc += "&gt;";
                else if (c == '&') esc += "&amp;";
                else if (c == '\n') esc += "\\n";
                else esc += c;
            }
            if (esc.size() > 40) esc = esc.substr(0, 40) + "...";

            dot << "  " << blkNodeId << " [label=<\n";
            dot << "    <TABLE BORDER='1' CELLBORDER='1' CELLSPACING='0' BGCOLOR='white'>\n";
            dot << "    <TR><TD BGCOLOR='#27AE60'>"
                << "<FONT COLOR='white'><B>Bloque Archivo " << blkIdx << "</B></FONT></TD></TR>\n";
            dot << "    <TR><TD>" << esc << "</TD></TR>\n";
            dot << "    </TABLE>> shape=none]\n\n";
            dot << "  node" << inodeNum << ":p" << j << " -> " << blkNodeId
                << " [color=\"#27ae60\"]\n";
        }
    }
}

static std::string ReportTree(std::fstream& file, const SuperBloque& sb,
                               const std::string& ruta) {
    std::ostringstream dot;
    dot << "digraph TREE {\n";
    dot << "  rankdir=LR\n";
    dot << "  node [fontname=\"Helvetica\" fontsize=10]\n";
    dot << "  graph [splines=ortho ranksep=1.5 nodesep=0.5 bgcolor=\"#f9f9f9\"]\n";
    dot << "  edge [arrowsize=0.7]\n\n";

    visitedNodes.clear();
    TreeNodeFull(file, sb, 0, "/", dot);

    dot << "}\n";
    GenerateImage(dot.str(), ruta);
    return ruta;
}


// REPORTE FILE : Contenido de un archivo (texto plano o imagen)

static std::string ReportFile(std::fstream& file, const SuperBloque& sb,
                               const std::string& filePath,
                               const std::string& ruta) {
    auto parts = FileOperations::SplitPath(filePath);
    int fileInodeNum = FileOperations::TraversePath(file, sb, parts);
 
    if (fileInodeNum == -1) {
        WriteText("Error: No existe " + filePath + "\n", ruta);
        return ruta;
    }
 
    Inode fileInode{};
    FileSystem::ReadInode(file, sb, fileInodeNum, fileInode);
 
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
 
    std::string ext = GetExtension(ruta);
    if (ext == "txt") {
        WriteText(content, ruta);
    } else {
        // Mostrar contenido línea por línea en tabla Graphviz
        std::ostringstream dot;
        dot << "digraph G {\n";
        dot << "  node [shape=none fontname=\"Courier\" fontsize=10]\n";
        dot << "  f [label=<\n";
        dot << "    <TABLE BORDER='1' CELLBORDER='1' CELLSPACING='0'>\n";
        dot << "    <TR><TD BGCOLOR='#2c3e50'><FONT COLOR='white'><B>"
            << filePath << "</B></FONT></TD></TR>\n";
 
        // Dividir por líneas
        std::istringstream ss(content);
        std::string cline;
        bool hasContent = false;
        while (std::getline(ss, cline)) {
            hasContent = true;
            // Escapar caracteres especiales
            std::string escaped;
            for (char c : cline) {
                if (c == '<') escaped += "&lt;";
                else if (c == '>') escaped += "&gt;";
                else if (c == '&') escaped += "&amp;";
                else escaped += c;
            }
            if (escaped.empty()) escaped = " ";
            dot << "    <TR><TD ALIGN='LEFT' BGCOLOR='#f9f9f9'>"
                << "<FONT FACE=\"Courier\">" << escaped << "</FONT></TD></TR>\n";
        }
        if (!hasContent) {
            dot << "    <TR><TD BGCOLOR='#f9f9f9'><FONT COLOR='gray'>(archivo vacío)</FONT></TD></TR>\n";
        }
        dot << "    </TABLE>>]\n}\n";
        GenerateImage(dot.str(), ruta);
    }
    return ruta;
}

// REPORTE LS : Permisos | Owner | Grupo | Size | Fecha | Hora | Tipo | Name
static std::string ReportLs(std::fstream& file, const SuperBloque& sb,
                              const std::string& dirPath, const std::string& ruta) {
    auto parts = FileOperations::SplitPath(dirPath);
    int dirInodeNum = parts.empty() ? 0 : FileOperations::TraversePath(file, sb, parts);

    if (dirInodeNum == -1) {
        WriteText("Error: No existe " + dirPath + "\n", ruta);
        return ruta;
    }

    Inode dirInode{};
    FileSystem::ReadInode(file, sb, dirInodeNum, dirInode);

    std::ostringstream dot;
    dot << "digraph G {\n";
    dot << "  node [shape=none fontname=\"Helvetica\" fontsize=10]\n";
    dot << "  ls [label=<\n";
    dot << "    <TABLE BORDER='1' CELLBORDER='1' CELLSPACING='0' BGCOLOR='white'>\n";
    dot << "    <TR>"
        << "<TD BGCOLOR='#2c3e50'><FONT COLOR='white'><B>Permisos</B></FONT></TD>"
        << "<TD BGCOLOR='#2c3e50'><FONT COLOR='white'><B>Owner</B></FONT></TD>"
        << "<TD BGCOLOR='#2c3e50'><FONT COLOR='white'><B>Grupo</B></FONT></TD>"
        << "<TD BGCOLOR='#2c3e50'><FONT COLOR='white'><B>Size (en Bytes)</B></FONT></TD>"
        << "<TD BGCOLOR='#2c3e50'><FONT COLOR='white'><B>Fecha</B></FONT></TD>"
        << "<TD BGCOLOR='#2c3e50'><FONT COLOR='white'><B>Hora</B></FONT></TD>"
        << "<TD BGCOLOR='#2c3e50'><FONT COLOR='white'><B>Tipo</B></FONT></TD>"
        << "<TD BGCOLOR='#2c3e50'><FONT COLOR='white'><B>Name</B></FONT></TD>"
        << "</TR>\n";

    bool alt = false;
    for (int i = 0; i < 12; i++) {
        if (dirInode.i_block[i] == -1) break;
        FolderBlock fb{};
        int pos = sb.s_block_start + dirInode.i_block[i] * sizeof(FolderBlock);
        Utilities::ReadObject(file, fb, pos);
        for (int j = 0; j < 4; j++) {
            if (fb.b_content[j].b_inodo == -1) continue;
            std::string entryName(fb.b_content[j].b_name);
            int entryInodeNum = fb.b_content[j].b_inodo;

            Inode child{};
            FileSystem::ReadInode(file, sb, entryInodeNum, child);

            bool isDir = (child.i_type[0] == '0');
            std::string tipo = isDir ? "Carpeta" : "Archivo";
            std::string perm = "-rw-rw-r--"; // default 664
            std::string perms(child.i_perm, 3);

            // Parsear permisos reales
            std::string permStr = "-";
            for (char c : perms) {
                int v = c - '0';
                permStr += (v & 4) ? "r" : "-";
                permStr += (v & 2) ? "w" : "-";
                permStr += (v & 1) ? "x" : "-";
            }

            // Fecha y hora del atime
            std::string atime(child.i_atime, 19);
            std::string fecha = atime.substr(0, 10);
            std::string hora  = atime.substr(11, 5);

            std::string bg = alt ? "#eaf2ff" : "white";
            alt = !alt;

            dot << "    <TR BGCOLOR='" << bg << "'>"
                << "<TD>" << permStr << "</TD>"
                << "<TD>" << child.i_uid << "</TD>"
                << "<TD>" << child.i_gid << "</TD>"
                << "<TD>" << child.i_size << "</TD>"
                << "<TD>" << fecha << "</TD>"
                << "<TD>" << hora << "</TD>"
                << "<TD>" << tipo << "</TD>"
                << "<TD>" << entryName << "</TD>"
                << "</TR>\n";
        }
    }
    dot << "    </TABLE>>]\n}\n";
    GenerateImage(dot.str(), ruta);
    return ruta;
}


// REP — Función principal

std::string Rep(const std::string& id, const std::string& ruta,
                const std::string& name, const std::string& path) {
    std::ostringstream out;
    out << "======= REP =======\n";

    std::string diskPath;
    int idx = DiskManagement::FindMountedById(id, diskPath);
    if (idx == -1) {
        out << "Error: No existe partición montada con ID '" << id << "'\n";
        return out.str();
    }

    auto file = Utilities::OpenFile(diskPath);
    if (!file.is_open()) {
        out << "Error: No se pudo abrir el disco\n";
        return out.str();
    }

    std::string nameLower = name;
    std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), ::tolower);

    // mbr y disk
    if (nameLower == "mbr") {
        ReportMbr(file, diskPath, ruta);
        file.close();
        out << "Reporte MBR generado: " << ruta << "\n";
        out << "REPORTE:" << ruta << "\n";
        out << "===================\n";
        return out.str();
    }
    if (nameLower == "disk") {
        ReportDisk(file, diskPath, ruta);
        file.close();
        out << "Reporte DISK generado: " << ruta << "\n";
        out << "REPORTE:" << ruta << "\n";
        out << "===================\n";
        return out.str();
    }

    
    int partStart = GetPartStart(file, id);
    if (partStart == -1) {
        out << "Error: Partición no encontrada en MBR\n";
        file.close();
        return out.str();
    }

    SuperBloque sb{};
    FileSystem::ReadSuperBloque(file, partStart, sb);
    if (sb.s_magic != 0xEF53) {
        out << "Error: Partición no formateada\n";
        file.close();
        return out.str();
    }

    std::string resultPath;
    if      (nameLower == "sb")       resultPath = ReportSb      (file, sb, diskPath, ruta);
    else if (nameLower == "inode")    resultPath = ReportInode   (file, sb, ruta);
    else if (nameLower == "block")    resultPath = ReportBlock   (file, sb, ruta);
    else if (nameLower == "bm_inode") resultPath = ReportBmInode (file, sb, ruta);
    else if (nameLower == "bm_block"  || nameLower == "bm_bloc")
                                      resultPath = ReportBmBlock (file, sb, ruta);
    else if (nameLower == "tree")     resultPath = ReportTree    (file, sb, ruta);
    else if (nameLower == "file")     resultPath = ReportFile    (file, sb, path, ruta);
    else if (nameLower == "ls")       resultPath = ReportLs      (file, sb, path, ruta);
    else {
        out << "Error: Reporte '" << name << "' no reconocido\n";
        file.close();
        return out.str();
    }

    file.close();
    out << "Reporte generado: " << resultPath << "\n";
    out << "REPORTE:" << resultPath << "\n";
    out << "===================\n";
    return out.str();
}

} 