#pragma once
#include <string>
#include <fstream>
#include "../Structs/Structs.h"

// FILESYSTEM — Sistema de archivos EXT2
// Comandos de este módulo:
//   MKFS -> formatea una partición montada como EXT2
// Funciones auxiliares (usadas por FileOperations y UserSession):
//   AllocateInode   -> busca y marca el siguiente inodo libre
//   AllocateBlock   -> busca y marca el siguiente bloque libre
//   WriteInode      -> escribe un inodo en su posición correcta
//   WriteBlock      -> escribe un bloque en su posición correcta
//   ReadInode       -> lee un inodo desde el disco
//   ReadBlock       -> lee un bloque desde el disco
//   ReadSuperBloque -> lee el superbloque de una partición

namespace FileSystem {

// Constante para EXT3 Journaling
const int JOURNALING_SIZE = 50;

//  Comando principal 
// type: "full" o vacío (ambos = formateo completo)
// fs: "2fs" (EXT2, default) o "3fs" (EXT3)
std::string Mkfs(const std::string& id, const std::string& type, const std::string& fs = "2fs");

// Funciones auxiliares 
// Todas reciben el archivo .mia ya abierto y el superbloque
// para saber exactamente dónde leer/escribir en el disco.

// Retorna el número del siguiente inodo libre y lo marca en el bitmap
// Retorna -1 si no hay inodos disponibles
int AllocateInode(std::fstream& file, SuperBloque& sb);

// Retorna el número del siguiente bloque libre y lo marca en el bitmap
// Retorna -1 si no hay bloques disponibles
int AllocateBlock(std::fstream& file, SuperBloque& sb);

// Escribe un inodo en la posición inodeNum de la tabla
void WriteInode(std::fstream& file, const SuperBloque& sb,
                int inodeNum, const Inode& inode);

// Lee un inodo desde la posición inodeNum de la tabla
void ReadInode(std::fstream& file, const SuperBloque& sb,
               int inodeNum, Inode& inode);

// Escribe un FolderBlock en la posición blockNum de la tabla
void WriteFolderBlock(std::fstream& file, const SuperBloque& sb,
                      int blockNum, const FolderBlock& block);

// Escribe un FileBlock en la posición blockNum de la tabla
void WriteFileBlock(std::fstream& file, const SuperBloque& sb,
                    int blockNum, const FileBlock& block);

// Lee el superbloque de una partición (dado su byte de inicio)
void ReadSuperBloque(std::fstream& file, int partStart, SuperBloque& sb);

// Escribe el superbloque actualizado de vuelta al disco
void WriteSuperBloque(std::fstream& file, int partStart, const SuperBloque& sb);

} 