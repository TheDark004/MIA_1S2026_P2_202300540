#pragma once
#include <cstdint>

#pragma pack(push, 1)

// PARTITION — Una ranura de partición dentro del MBR
//
// El MBR tiene 4 de estas. Cada una puede estar:
//   - Vacía    -> Start = -1
//   - Ocupada  -> tiene datos reales
//   - Montada  -> Status = '1' y tiene un ID
//
// Tamaño total: 1+1+2+4+4+16+4+4 = 36 bytes

struct Partition {
    char    Status[1];      // '0' = sin montar  |  '1' = montada
    char    Type[1];        // 'P' = Primaria  |  'E' = Extendida  |  'L' = Lógica
    char    Fit[2];         // 'BF' = Best  |  'FF' = First  |  'WF' = Worst Fit
    int32_t Start;          // Byte donde INICIA la partición en el .mia  (-1 si vacía)
    int32_t Size;           // Tamaño en BYTES
    char    Name[16];       // Nombre: "part1", "home"
    int32_t Correlative;    // -1 hasta que se monta, luego 1, 2, 3...
    char    Id[4];          // ID al montar.
};


// MBR — Master Boot Record
//
// Siempre en el byte 0 del archivo .mia.
// Contiene info del disco + 4 ranuras de partición.

struct MBR {
    int32_t   MbrSize;          // Tamaño total del disco en bytes
    char      CreationDate[19]; // Fecha de creación del disco "2026-02-25 14:30:00"
    int32_t   Signature;        // Número random único del disco
    char      Fit[2];           // Tipo de ajuste del disco
    Partition Partitions[4];    // Las 4 ranuras (pueden estar vacías)
};


// EBR — Extended Boot Record
//
// Para particiones LÓGICAS dentro de una Extendida.
// Forman una lista enlazada via el campo Next.


struct EBR {
    char    Mount[1];  // '1' = montada  |  '0' = no montada
    char    Fit[2];    // Tipo de ajuste
    int32_t Start;     // Byte de inicio del contenido de esta partición lógica
    int32_t Size;      // Tamaño en bytes
    int32_t Next;      // Byte del siguiente EBR  (-1 si es el último)
    char    Name[16];  // Nombre de la partición lógica
};

// SUPERBLOQUE — Metadatos del sistema de archivos EXT2
//
// Primero que se escribe al ejecutar MKFS en una partición.
// Guarda contabilidad de inodos, bloques y offsets de cada región.
struct SuperBloque {
    int32_t s_filesystem_type;   // 2 = EXT2  |  3 = EXT3
    int32_t s_inodes_count;      // Total de inodos
    int32_t s_blocks_count;      // Total de bloques
    int32_t s_free_inodes_count; // Inodos libres
    int32_t s_free_blocks_count; // Bloques libres
    int32_t s_mtime;             // Último montaje (Unix timestamp)
    int32_t s_umtime;            // Último desmontaje
    int32_t s_mnt_count;         // Veces montado
    int32_t s_magic;             // 0xEF53 -> identifica EXT2
    int32_t s_inode_size;        // sizeof(Inode)
    int32_t s_block_size;        // sizeof(FolderBlock) = 64
    int32_t s_first_ino;         // Primer inodo libre
    int32_t s_first_blo;         // Primer bloque libre
    int32_t s_bm_inode_start;    // Byte donde inicia bitmap de inodos
    int32_t s_bm_block_start;    // Byte donde inicia bitmap de bloques
    int32_t s_inode_start;       // Byte donde inicia tabla de inodos
    int32_t s_block_start;       // Byte donde inicia tabla de bloques
};


// INODO — Representa UN archivo O UNA carpeta
//
// NO contiene el contenido real, solo metadatos + apuntadores.
//
// i_block[15]:
//   [0..11] -> Directos      -> apuntan a bloques de datos
//   [12]    -> Ind. Simple   ->apunta a PointerBlock (16 ptrs)
//   [13]    ->Ind. Doble    -> ptr -> PointerBlock -> PointerBlock -> datos
//   [14]    -> Ind. Triple   -> tres niveles de PointerBlocks

struct Inode {
    int32_t i_uid;       // UID del propietario
    int32_t i_gid;       // GID del grupo
    int32_t i_size;      // Tamaño en bytes
    char    i_atime[19]; // Último acceso       
    char    i_ctime[19]; // Fecha de creación   
    char    i_mtime[19]; // Última modificación "
    int32_t i_block[15]; // Apuntadores a bloques (-1 = no usado)
    char    i_type[1];   // '0' = carpeta  |  '1' = archivo
    char    i_perm[3];   // "664", "755", etc.
};


// BLOQUES — 3 tipos, todos de 64 bytes
//
// Mismo tamaño -> fácil calcular posición del bloque N:
//   pos = s_block_start + (N * 64)


struct FolderContent {
    char    b_name[12]; // Nombre del entry (".", "..", "users.txt")
    int32_t b_inodo;    // Número de inodo  (-1 si vacío)
};

// Bloque CARPETA: hasta 4 entries × 16 bytes = 64 bytes
struct FolderBlock {
    FolderContent b_content[4];
};

// Bloque ARCHIVO: 64 bytes de contenido crudo
struct FileBlock {
    char b_content[64];
};

// Bloque APUNTADOR: 16 punteros × 4 bytes = 64 bytes
struct PointerBlock {
    int32_t b_pointers[16]; // -1 si no usado
};



// NUEVAS ESTRUCTURAS PARA EXT3 -- JOURNALING --- Proyecto 2
// Contenido de una entrada del journal
// Registra todos los detalles de una operación realizada en el sistema de archivos
// (mkdir, mkfile, remove, rename, copy, move, chown, chmod ...)

struct Information {
    char    i_operation[10]; // Nombre de la operación: "mkdir", "mkfile", "remove"..
    char    i_path[32];      // Ruta donde se realiza la operación: "/home/user/docs"
    char    i_content[64];   // contenido (si es archivo)
    float   i_date;          // Fecha/hora 
};

// JOURNAL — Entrada del journal/bitácora de EXT3
// El journal de EXT3 está formado por un array de 50 entradas (constant JOURNALING_SIZE = 50)
// en la partición EXT3. Cada entrada registra UNA operación.
struct Journal {
    int32_t     j_count;    // Contador del journal 
    Information j_content;  // información de la acción
};

#pragma pack(pop)