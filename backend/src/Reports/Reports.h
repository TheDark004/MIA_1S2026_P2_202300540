#pragma once
#include <string>


// REPORTS — Genera reportes visuales con Graphviz
//
// Comando: REP
//   rep -id=401A -ruta=inode.png  -name=inode
//   rep -id=401A -ruta=block.png  -name=block
//   rep -id=401A -ruta=bminode.png -name=bm_inode
//   rep -id=401A -ruta=bmblock.png -name=bm_block
//   rep -id=401A -ruta=tree.png   -name=tree
//   rep -id=401A -ruta=sb.png     -name=sb
//   rep -id=401A -ruta=file.png   -name=file   -path=/home/user/hola.txt
//   rep -id=401A -ruta=ls.png     -name=ls     -path=/home/user
//
// Flujo interno:
//  1. Leer datos del .mia
//  2. Generar archivo .dot
//  3. Ejecutar: dot -Tpng archivo.dot -o archivo.png
//  4. Retornar ruta del .png

namespace Reports {

std::string Rep(const std::string& id,
                const std::string& ruta,
                const std::string& name,
                const std::string& path); 

} 