# Manual Técnico — C++Disk 2.0 (ExtreamFS P2)

### Simulador de Sistema de Archivos EXT2 / EXT3

**MIA 1S2026 | Carnet: 202300540 | Universidad San Carlos de Guatemala**

---

## Tabla de Contenidos

1. [Descripción General](#1-descripción-general)
2. [Arquitectura del Sistema](#2-arquitectura-del-sistema)
3. [Estructuras de Datos](#3-estructuras-de-datos)
4. [Comandos Implementados](#4-comandos-implementados)
5. [Reportes](#5-reportes)
6. [Despliegue en AWS](#6-despliegue-en-aws)

---

## 1. Descripción General

C++Disk 2.0 es la evolución del sistema de archivos simulado ExtreamFS (Proyecto 1). El objetivo central fue mejorar la accesibilidad del sistema mediante una Interfaz Gráfica de Usuario (GUI) basada en web, incorporar soporte para EXT3 con journaling, añadir nuevos comandos de administración avanzada, y desplegar la solución en la nube con Amazon Web Services (AWS).

### Cambios respecto al Proyecto 1

| Categoría           | P1                  | P2                                                                        |
| ------------------- | ------------------- | ------------------------------------------------------------------------- |
| Sistema de archivos | Solo EXT2           | EXT2 + EXT3 con Journaling                                                |
| Login               | Por comando `login` | Por comando O por interfaz gráfica                                        |
| Visualizador        | No existía          | Explorador visual de discos, particiones, carpetas y archivos             |
| FDISK               | Solo crear          | También add/delete                                                        |
| Comandos nuevos     | —                   | UNMOUNT, REMOVE, RENAME, COPY, MOVE, FIND, CHOWN, CHMOD, LOSS, JOURNALING |
| Despliegue          | Local               | AWS EC2 (backend) + S3 (frontend)                                         |

---

## 2. Arquitectura del Sistema

### 2.1 Infraestructura en AWS

![Infraestructura en AWS](/Documentacion/aws_infraestructura.png)

```
┌─────────────────────────────────────────────────────────────────┐
│                          AWS Cloud                              │
│                                                                 │
│   ┌───────────────────────┐      ┌────────────────────────────┐ │
│   │    S3 Bucket          │      │       EC2 Instance         │ │
│   │  (Sitio Web Estático) │      │    Ubuntu 22.04 LTS        │ │
│   │                       │      │                            │ │
│   │  React + Vite         │─────►│  C++17 Backend             │ │
│   │  index.html           │ HTTP │  Crow HTTP Server          │ │
│   │  assets/              │:18080│  Puerto 18080              │ │
│   │                       │      │  Archivos .mia             │ │
│   └───────────────────────┘      └────────────────────────────┘ │
│            ▲                                ▲                   │
│            │                                │                   │
└────────────┼────────────────────────────────┼───────────────────┘
             │                                │
         Usuario                       Security Group:
         (Browser)                     TCP 18080 abierto
```

### 2.2 Flujo de Comunicación

```
Usuario
  │
  │  Abre URL del bucket S3
  ▼
Frontend (React)
  │
  ├── POST /execute        → Ejecutar comandos
  ├── GET  /get_disks      → Listar discos montados
  ├── POST /get_partitions → Listar particiones de un disco
  ├── GET  /fs/browse      → Listar carpeta con metadatos
  ├── GET  /fs/read        → Leer contenido de archivo
  ├── GET  /fs/journal     → Ver entradas del journal
  ├── GET  /fs/session     → Estado de sesión activa
  └── GET  /reports/<ruta> → Obtener imagen de reporte
  │
  ▼
Backend C++ (EC2 :18080)
  │
  ├── Analyzer::AnalyzeScript()
  │     └── despacha al módulo correcto
  ├── Lee / escribe en archivos .mia
  └── Retorna JSON con output y reportes
```

![Infraestructura en AWS](/Documentacion/aws_flujo_comunicacion.png)

### 2.3 Módulos del Backend

```
backend/src/
├── main.cpp                     ← Servidor Crow HTTP + todos los endpoints REST
├── Analyzer/
│   └── Analyzer.cpp             ← Parsea comandos (-key=value), despacha módulos
├── DiskManagement/
│   └── DiskManagement.cpp       ← MKDISK, RMDISK, FDISK (add/delete), MOUNT, UNMOUNT
├── FileSystem/
│   └── FileSystem.cpp           ← MKFS (EXT2 y EXT3), inodos, bloques, journaling
├── UserSession/
│   └── UserSession.cpp          ← LOGIN, LOGOUT, MKGRP, RMGRP, MKUSR, RMUSR, CHGRP
├── FileOperations/
│   ├── FileOperationsCore.cpp   ← TraversePath, FindInDir, AddEntryToDir, SplitPath
│   ├── FileOperationsDir.cpp    ← MKDIR
│   ├── FileOperationsFile.cpp   ← MKFILE, CAT, REMOVE, RENAME, COPY, MOVE
│   └── FileOperationsSearch.cpp ← FIND, CHMOD, CHOWN, LOSS
├── Reports/
│   └── Reports.cpp              ← Generación de reportes con Graphviz
├── Utilities/
│   └── Utilities.cpp            ← I/O binario (ReadObject/WriteObject), fechas
└── Structs/
    └── Structs.h                ← Todas las estructuras con #pragma pack(push,1)
```

![Infraestructura en AWS](/Documentacion/backend_modulos.png)

### 2.4 API REST — Endpoints

| Endpoint                   | Método | Descripción                                         |
| -------------------------- | ------ | --------------------------------------------------- |
| `POST /execute`            | POST   | Ejecuta comandos. Retorna `{output, reports[]}`     |
| `GET /health`              | GET    | Verifica que el backend esté activo                 |
| `GET /fs/session`          | GET    | Estado de sesión: `{active, username, uid, partId}` |
| `GET /get_disks`           | GET    | Lista discos con particiones montadas               |
| `POST /get_partitions`     | POST   | Particiones de un disco `{name: "Disco1.mia"}`      |
| `GET /fs/browse?path=/`    | GET    | Lista directorio con metadatos completos            |
| `GET /fs/read?path=/f.txt` | GET    | Contenido de un archivo virtual                     |
| `GET /fs/journal`          | GET    | Entradas del journal (solo EXT3, sesión activa)     |
| `GET /reports/<ruta>`      | GET    | Sirve imagen o .txt de reporte                      |

### 2.5 Componentes del Frontend

```
frontend/src/
├── App.jsx              ← Estado global, coordina sesión y todos los modales
├── components/
│   ├── Terminal.jsx     ← Área entrada/salida, Ctrl+Enter, carga de scripts .smia
│   ├── LoginModal.jsx   ← Login por GUI (ID Partición + Usuario + Contraseña)
│   ├── DiskSelector.jsx ← Visualizador de discos y particiones montadas
│   ├── FileExplorer.jsx ← Explorador de archivos con breadcrumbs y metadatos
│   ├── JournalViewer.jsx← Tabla de transacciones del journal EXT3
│   └── ReportGallery.jsx← Galería de reportes con visor de imágenes y .txt
└── main.jsx             ← Entry point de React
```

---

## 3. Estructuras de Datos

Todas las estructuras usan `#pragma pack(push, 1)` para eliminar el padding entre campos. Esto es crítico para que la lectura/escritura binaria en el `.mia` sea exacta.

### 3.1 Partition (36 bytes)

Representa una ranura de partición dentro del MBR. El MBR tiene exactamente 4 de estas.

```
┌────────┬────────┬────────┬───────────┬──────────┬──────────────────┬─────────────┬────────┐
│Status  │ Type   │  Fit   │   Start   │   Size   │      Name        │ Correlative │   Id   │
│ 1 byte │ 1 byte │ 2 bytes│  4 bytes  │  4 bytes │    16 bytes      │   4 bytes   │ 4 bytes│
└────────┴────────┴────────┴───────────┴──────────┴──────────────────┴─────────────┴────────┘
```

| Campo         | Tipo      | Descripción                                                   |
| ------------- | --------- | ------------------------------------------------------------- |
| `Status[1]`   | `char`    | `'0'` = sin montar \| `'1'` = montada                         |
| `Type[1]`     | `char`    | `'P'` = Primaria \| `'E'` = Extendida \| `'L'` = Lógica       |
| `Fit[2]`      | `char`    | `'bf'` = Best Fit \| `'ff'` = First Fit \| `'wf'` = Worst Fit |
| `Start`       | `int32_t` | Byte donde INICIA la partición en el `.mia` (-1 si vacía)     |
| `Size`        | `int32_t` | Tamaño en bytes                                               |
| `Name[16]`    | `char[]`  | Nombre de la partición (ej: `"Part1"`)                        |
| `Correlative` | `int32_t` | -1 hasta que se monta, luego 1, 2, 3...                       |
| `Id[4]`       | `char[]`  | ID al montar (ej: `"401A"`)                                   |

### 3.2 MBR — Master Boot Record (173 bytes)

Siempre ubicado en el **byte 0** del archivo `.mia`.

```
Byte 0 del .mia:
┌──────────┬───────────────┬───────────┬──────┬───────────────────────────────────────┐
│ MbrSize  │ CreationDate  │ Signature │ Fit  │         Partitions[4]                 │
│  4 bytes │   19 bytes    │  4 bytes  │2 byte│          4 × 36 = 144 bytes           │
└──────────┴───────────────┴───────────┴──────┴───────────────────────────────────────┘
```

| Campo              | Tipo          | Descripción                      |
| ------------------ | ------------- | -------------------------------- |
| `MbrSize`          | `int32_t`     | Tamaño total del disco en bytes  |
| `CreationDate[19]` | `char[]`      | Fecha `"2026-04-23 14:30:00"`    |
| `Signature`        | `int32_t`     | Número aleatorio único del disco |
| `Fit[2]`           | `char[]`      | Tipo de ajuste del disco         |
| `Partitions[4]`    | `Partition[]` | Las 4 ranuras de partición       |

### 3.3 EBR — Extended Boot Record (27 bytes)

Para particiones **lógicas** dentro de una Extendida. Forman una lista enlazada.

| Campo      | Tipo      | Descripción                                 |
| ---------- | --------- | ------------------------------------------- |
| `Mount[1]` | `char`    | `'1'` = montada \| `'0'` = no montada       |
| `Fit[2]`   | `char[]`  | Tipo de ajuste                              |
| `Start`    | `int32_t` | Byte de inicio del contenido lógico         |
| `Size`     | `int32_t` | Tamaño en bytes                             |
| `Next`     | `int32_t` | Byte del siguiente EBR (-1 si es el último) |
| `Name[16]` | `char[]`  | Nombre de la partición lógica               |

### 3.4 SuperBloque

Primero que se escribe al ejecutar MKFS. Contiene la contabilidad completa del filesystem.

```
Byte 0 de la partición:
┌─────────────────────────────────────────────────────────┐
│                      SuperBloque                        │
│  s_filesystem_type  │ s_inodes_count │ s_blocks_count   │
│  s_free_inodes_count│ s_free_blocks  │ s_mtime          │
│  s_umtime           │ s_mnt_count    │ s_magic(0xEF53)  │
│  s_inode_size       │ s_block_size   │ s_first_ino      │
│  s_first_blo        │ s_bm_inode_start│s_bm_block_start │
│  s_inode_start      │ s_block_start                     │
└─────────────────────────────────────────────────────────┘
```

| Campo                 | Descripción                            |
| --------------------- | -------------------------------------- |
| `s_filesystem_type`   | `2` = EXT2 \| `3` = EXT3               |
| `s_inodes_count`      | Total de inodos                        |
| `s_blocks_count`      | Total de bloques                       |
| `s_free_inodes_count` | Inodos libres                          |
| `s_free_blocks_count` | Bloques libres                         |
| `s_magic`             | `0xEF53` — identifica EXT2/EXT3        |
| `s_inode_size`        | `sizeof(Inode)` = 133 bytes            |
| `s_block_size`        | `sizeof(FolderBlock)` = 64 bytes       |
| `s_bm_inode_start`    | Byte donde inicia el bitmap de inodos  |
| `s_bm_block_start`    | Byte donde inicia el bitmap de bloques |
| `s_inode_start`       | Byte donde inicia la tabla de inodos   |
| `s_block_start`       | Byte donde inicia la tabla de bloques  |

### 3.5 Layout del disco — EXT2 vs EXT3

**EXT2:**

```
┌──────────────┬───────────────┬───────────────┬──────────────┬──────────────┐
│  SuperBloque │ Bitmap Inodos │ Bitmap Bloques│    Inodos    │    Bloques   │
└──────────────┴───────────────┴───────────────┴──────────────┴──────────────┘
```

**EXT3 (nuevo en P2) — Journaling insertado después del SuperBloque:**

```
┌──────────────┬──────────────┬───────────────┬───────────────┬──────────────┬──────────────┐
│  SuperBloque │  Journaling  │ Bitmap Inodos │ Bitmap Bloques│    Inodos    │    Bloques   │
│              │  (50 entradas│               │               │              │              │
│              │  × sizeof(J))│               │               │              │              │
└──────────────┴──────────────┴───────────────┴───────────────┴──────────────┴──────────────┘
```

**Fórmula para calcular `n` (número de estructuras):**

```
EXT2:
  tamaño_particion = sizeof(SB) + n + 3*n + n*sizeof(Inode) + 3*n*sizeof(Block)

EXT3:
  tamaño_particion = sizeof(SB) + n*sizeof(Journal) + n + 3*n + n*sizeof(Inode) + 3*n*sizeof(Block)

  n = floor( (tamaño - sizeof(SB)) / (sizeof(Journal) + 4 + n*sizeof(Inode) + 3*64) )
```

> `JOURNALING_SIZE = 50` → constante fija. El array de Journal siempre ocupa `50 * sizeof(Journal)` bytes.

### 3.6 Inode (133 bytes)

Representa **un archivo O una carpeta**. No contiene el contenido real, solo metadatos y punteros.

```
┌───────┬───────┬────────┬──────────┬──────────┬──────────┬────────────────┬────────┬────────┐
│ i_uid │ i_gid │ i_size │ i_atime  │ i_ctime  │ i_mtime  │   i_block[15]  │ i_type │ i_perm │
│4 bytes│4 bytes│4 bytes │ 19 bytes │ 19 bytes │ 19 bytes │   60 bytes     │ 1 byte │ 3 bytes│
└───────┴───────┴────────┴──────────┴──────────┴──────────┴────────────────┴────────┴────────┘
```

| Campo                 | Descripción                                               |
| --------------------- | --------------------------------------------------------- |
| `i_uid`               | UID del propietario                                       |
| `i_gid`               | GID del grupo                                             |
| `i_size`              | Tamaño en bytes                                           |
| `i_atime/ctime/mtime` | Fechas `"YYYY-MM-DD HH:MM:SS"`                            |
| `i_block[0..11]`      | Punteros **directos** a bloques de datos                  |
| `i_block[12]`         | Puntero **indirecto simple** → PointerBlock               |
| `i_block[13]`         | Puntero **indirecto doble** → PointerBlock → PointerBlock |
| `i_block[14]`         | Puntero **indirecto triple**                              |
| `i_type[1]`           | `'0'` = carpeta \| `'1'` = archivo                        |
| `i_perm[3]`           | Permisos en octal `"664"`, `"755"`                        |

### 3.7 Bloques (64 bytes cada uno)

Todos los bloques tienen el mismo tamaño. Posición del bloque N:

```
pos = s_block_start + (N × 64)
```

**FolderBlock** — hasta 4 entradas de directorio:

```
┌────────────────────┬────────────────────┬────────────────────┬────────────────────┐
│  FolderContent[0]  │  FolderContent[1]  │  FolderContent[2]  │  FolderContent[3]  │
│  name[12]+inodo[4] │  name[12]+inodo[4] │  name[12]+inodo[4] │  name[12]+inodo[4] │
└────────────────────┴────────────────────┴────────────────────┴────────────────────┘
```

**FileBlock** — 64 bytes de contenido crudo de archivo.

**PointerBlock** — 16 punteros `int32_t` a otros bloques para indirecciones.

### 3.8 Information y Journal (NUEVAS — EXT3)

Estructuras del **Proyecto 2** que implementan el journaling.

**Information** — contenido de una entrada:

| Campo             | Tipo     | Descripción                                                           |
| ----------------- | -------- | --------------------------------------------------------------------- |
| `i_operation[10]` | `char[]` | Operación realizada: `"mkdir"`, `"mkfile"`, `"remove"`, `"rename"`... |
| `i_path[32]`      | `char[]` | Ruta donde se realizó: `"/home/user/docs"`                            |
| `i_content[64]`   | `char[]` | Contenido del archivo (si aplica)                                     |
| `i_date`          | `float`  | Timestamp Unix de la operación                                        |

**Journal** — entrada del journal:

| Campo       | Tipo          | Descripción                                  |
| ----------- | ------------- | -------------------------------------------- |
| `j_count`   | `int32_t`     | Contador/índice. `> 0` indica entrada activa |
| `j_content` | `Information` | Datos de la acción registrada                |

### 3.9 users.txt

Archivo especial creado por MKFS en el inodo 1. Formato CSV:

```
GID,G,nombre_grupo
UID,U,nombre_usuario,grupo,contraseña
```

Ejemplo real:

```
1,G,root
1,U,root,root,123
2,G,devs
2,U,alice,devs,abc
```

> `GID = 0` o `UID = 0` indica que fue eliminado con RMGRP/RMUSR.

---

## 4. Comandos Implementados

### 4.1 MKDISK

Crea un disco virtual `.mia` de tamaño fijo.

| Parámetro | Tipo        | Descripción                      |
| --------- | ----------- | -------------------------------- |
| `-size`   | Obligatorio | Tamaño del disco. Debe ser > 0   |
| `-path`   | Obligatorio | Ruta absoluta del archivo `.mia` |
| `-unit`   | Opcional    | `k` = KB, `m` = MB. Default: `m` |
| `-fit`    | Opcional    | `bf`, `ff`, `wf`. Default: `ff`  |

```bash
mkdisk -size=50 -unit=M -fit=FF -path=/home/user/Discos/Disco1.mia
```

**Efecto:** Crea el archivo `.mia` llenándolo con ceros y escribe el MBR en el byte 0.

---

### 4.2 RMDISK

Elimina un disco virtual del sistema.

| Parámetro | Tipo        | Descripción                |
| --------- | ----------- | -------------------------- |
| `-path`   | Obligatorio | Ruta del `.mia` a eliminar |

```bash
rmdisk -path=/home/user/Discos/Disco1.mia
```

---

### 4.3 FDISK

Crea, modifica o elimina particiones. En P2 se agregaron `-add` y `-delete`.

| Parámetro | Tipo                | Descripción                                               |
| --------- | ------------------- | --------------------------------------------------------- |
| `-size`   | Obligatorio (crear) | Tamaño de la partición                                    |
| `-path`   | Obligatorio         | Ruta del disco `.mia`                                     |
| `-name`   | Obligatorio         | Nombre único en el disco                                  |
| `-type`   | Opcional            | `P` Primaria \| `E` Extendida \| `L` Lógica. Default: `P` |
| `-fit`    | Opcional            | `BF`, `FF`, `WF`. Default: `WF`                           |
| `-unit`   | Opcional            | `b`, `k`, `m`. Default: `k`                               |
| `-add`    | Opcional (**P2**)   | Agrega (+) o quita (-) espacio. Ej: `-add=500`            |
| `-delete` | Opcional (**P2**)   | `fast` = marca vacío \| `full` = limpia con `\0`          |

```bash
# Crear partición primaria
fdisk -type=P -unit=M -name=Part1 -size=10 -path=/home/user/Disco1.mia

# Agregar 1MB a la partición
fdisk -add=1 -unit=M -path=/home/user/Disco1.mia -name=Part1

# Quitar 500KB
fdisk -add=-500 -unit=K -path=/home/user/Disco1.mia -name=Part1

# Eliminar rápido
fdisk -delete=fast -name=Part1 -path=/home/user/Disco1.mia

# Eliminar completo (limpia con \0)
fdisk -delete=full -name=Part1 -path=/home/user/Disco1.mia
```

---

### 4.4 MOUNT

Monta una partición y le asigna un ID único.

**Formato del ID:** `[2 dígitos carnet][correlativo][letra disco]`

- Carnet `202300540` → 2 últimos antes del guión = `40`
- Primer disco, primera partición → `401A`

| Parámetro | Tipo        | Descripción            |
| --------- | ----------- | ---------------------- |
| `-path`   | Obligatorio | Ruta del disco         |
| `-name`   | Obligatorio | Nombre de la partición |

```bash
mount -path=/home/user/Disco1.mia -name=Part1
# Resultado: Partición montada con ID 401A
```

---

### 4.5 MOUNTED

Lista todas las particiones actualmente montadas en RAM.

```bash
mounted
# Salida:
# 401A  |  Part1  |  /home/user/Disco1.mia
# 402A  |  Part2  |  /home/user/Disco1.mia
# 401B  |  Part1  |  /home/user/Disco2.mia
```

---

### 4.6 UNMOUNT _(nuevo P2)_

Desmonta una partición por su ID. Resetea el correlativo a 0.

| Parámetro | Tipo        | Descripción                                         |
| --------- | ----------- | --------------------------------------------------- |
| `-id`     | Obligatorio | ID de la partición (ej: `401A`). Error si no existe |

```bash
unmount -id=401A
```

---

### 4.7 MKFS

Formatea una partición como EXT2 o EXT3. En P2 se agrega el parámetro `-fs`.

| Parámetro | Tipo              | Descripción                                  |
| --------- | ----------------- | -------------------------------------------- |
| `-id`     | Obligatorio       | ID de la partición montada                   |
| `-type`   | Opcional          | `full` = formateo completo. Default: `full`  |
| `-fs`     | Opcional (**P2**) | `2fs` = EXT2 \| `3fs` = EXT3. Default: `2fs` |

```bash
mkfs -type=full -id=401A          # EXT2 (default)
mkfs -id=401A -fs=3fs             # EXT3 con journaling
```

**Efecto en EXT3:** Además de los pasos normales de EXT2, reserva espacio para 50 entradas `Journal` entre el SuperBloque y el Bitmap de Inodos.

---

### 4.8 LOGIN

Inicia sesión en el sistema de archivos. En P2 también se puede hacer desde la GUI.

| Parámetro | Tipo        | Descripción        |
| --------- | ----------- | ------------------ |
| `-user`   | Obligatorio | Nombre de usuario  |
| `-pass`   | Obligatorio | Contraseña         |
| `-id`     | Obligatorio | ID de la partición |

```bash
login -user=root -pass=123 -id=401A
```

**Restricción:** Solo puede haber una sesión activa a la vez.

---

### 4.9 LOGOUT

Cierra la sesión activa.

```bash
logout
```

---

### 4.10 MKGRP

Crea un nuevo grupo de usuarios. Solo puede ejecutarlo `root`.

```bash
mkgrp -name=devs
```

---

### 4.11 RMGRP

Elimina un grupo (marca su GID como `0`). Solo `root`.

```bash
rmgrp -name=devs
```

---

### 4.12 MKUSR

Crea un nuevo usuario. Solo `root`.

| Parámetro | Tipo        | Descripción            |
| --------- | ----------- | ---------------------- |
| `-user`   | Obligatorio | Nombre del usuario     |
| `-pass`   | Obligatorio | Contraseña             |
| `-grp`    | Obligatorio | Grupo al que pertenece |

```bash
mkusr -user=alice -pass=abc -grp=devs
```

---

### 4.13 RMUSR

Elimina un usuario (marca su UID como `0`). Solo `root`.

```bash
rmusr -user=alice
```

---

### 4.14 CHGRP

Cambia el grupo de un usuario. Solo `root`.

```bash
chgrp -user=alice -grp=admin
```

---

### 4.15 MKDIR

Crea un directorio en el sistema de archivos virtual.

| Parámetro | Tipo        | Descripción                           |
| --------- | ----------- | ------------------------------------- |
| `-path`   | Obligatorio | Ruta del directorio a crear           |
| `-p`      | Opcional    | Crea directorios padres si no existen |

```bash
mkdir -path=/bin
mkdir -p -path=/home/user/docs/proyectos/usac
```

**Efecto interno:**

1. Asigna un inodo nuevo (tipo `'0'` = carpeta)
2. Asigna un bloque con entradas `.` y `..`
3. Agrega la entrada al directorio padre

En particiones **EXT3**, escribe una entrada en el Journal con `i_operation = "mkdir"`.

---

### 4.16 MKFILE

Crea un archivo en el sistema de archivos virtual.

| Parámetro | Tipo        | Descripción                                  |
| --------- | ----------- | -------------------------------------------- |
| `-path`   | Obligatorio | Ruta del archivo                             |
| `-size`   | Opcional    | Genera N bytes de contenido aleatorio        |
| `-cont`   | Opcional    | Copia contenido desde un archivo real del SO |
| `-r`      | Opcional    | Crea directorios padres si no existen        |

```bash
mkfile -path=/home/user/Tarea.txt -size=75
mkfile -path=/home/user/Tarea3.txt -cont=/home/thedark004/NAME.txt
mkfile -r -path=/home/user/docs/proyectos/fase1.txt
```

En particiones **EXT3**, escribe una entrada en el Journal con `i_operation = "mkfile"` y el contenido del archivo en `i_content`.

---

### 4.17 CAT

Muestra el contenido de un archivo del sistema de archivos virtual.

```bash
cat -file=/home/user/Tarea.txt
cat -file1=/users.txt
```

---

### 4.18 REMOVE _(nuevo P2)_

Elimina un archivo o carpeta y todo su contenido. Valida permisos de escritura del usuario activo. Si no puede eliminar un subelemento por permisos, **no elimina los padres**.

| Parámetro | Tipo        | Descripción                           |
| --------- | ----------- | ------------------------------------- |
| `-path`   | Obligatorio | Ruta del archivo o carpeta a eliminar |

```bash
remove -path=/home/user/docs/a.txt
remove -path=/home/user        # elimina carpeta recursivamente
```

---

### 4.19 RENAME _(nuevo P2)_

Cambia el nombre de un archivo o carpeta. Requiere permiso de escritura. Verifica que el nuevo nombre no exista en el mismo directorio.

| Parámetro | Tipo        | Descripción                      |
| --------- | ----------- | -------------------------------- |
| `-path`   | Obligatorio | Ruta actual                      |
| `-name`   | Obligatorio | Nuevo nombre. Error si ya existe |

```bash
rename -path=/home/user/docs/a.txt -name=b1.txt
```

---

### 4.20 COPY _(nuevo P2)_

Copia un archivo o carpeta con todo su contenido hacia un destino. Solo copia lo que tenga permiso de lectura. Requiere permiso de escritura en el destino.

| Parámetro  | Tipo        | Descripción                                |
| ---------- | ----------- | ------------------------------------------ |
| `-path`    | Obligatorio | Ruta origen                                |
| `-destino` | Obligatorio | Ruta del directorio destino (debe existir) |

```bash
copy -path=/home/user/documents -destino=/home/images
# b.txt con permisos 224 no se copia (sin permiso de lectura)
```

---

### 4.21 MOVE _(nuevo P2)_

Mueve un archivo o carpeta hacia otro directorio. Si origen y destino están en la **misma partición**, solo cambia las referencias del inodo padre sin copiar datos físicamente.

| Parámetro  | Tipo        | Descripción  |
| ---------- | ----------- | ------------ |
| `-path`    | Obligatorio | Ruta origen  |
| `-destino` | Obligatorio | Ruta destino |

```bash
move -path=/home/user/documents -destino=/home/images
```

---

### 4.22 FIND _(nuevo P2)_

Búsqueda recursiva de archivos/carpetas por nombre. Soporta wildcards.

| Carácter | Significado                 |
| -------- | --------------------------- |
| `?`      | Un solo carácter cualquiera |
| `*`      | Uno o más caracteres        |

| Parámetro | Tipo        | Descripción                         |
| --------- | ----------- | ----------------------------------- |
| `-path`   | Obligatorio | Directorio donde inicia la búsqueda |
| `-name`   | Obligatorio | Nombre o patrón con wildcards       |

```bash
find -path=/ -name=*              # todos los archivos
find -path=/home -name=*.txt      # todos los .txt
find -path=/ -name=?.jpg          # archivos con nombre de 1 char + .jpg
```

---

### 4.23 CHOWN _(nuevo P2)_

Cambia el propietario de un archivo o carpeta. `root` puede hacerlo en cualquier archivo. Otros usuarios solo en los suyos propios.

| Parámetro  | Tipo        | Descripción                                      |
| ---------- | ----------- | ------------------------------------------------ |
| `-path`    | Obligatorio | Ruta del archivo o carpeta                       |
| `-usuario` | Obligatorio | Nombre del nuevo propietario. Error si no existe |
| `-r`       | Opcional    | Aplica recursivamente en subdirectorios          |

```bash
chown -path=/home -r -usuario=user2
chown -path=/home/user/docs/a.txt -usuario=root
```

---

### 4.24 CHMOD _(nuevo P2)_

Cambia los permisos UGO en notación octal. Cada dígito es 0-7 (bits: read=4, write=2, exec=1).

| Parámetro | Tipo        | Descripción                                           |
| --------- | ----------- | ----------------------------------------------------- |
| `-path`   | Obligatorio | Ruta del archivo o carpeta                            |
| `-ugo`    | Obligatorio | Tres dígitos `[0-7][0-7][0-7]`. Ej: `764` = rwxrw-r-- |
| `-r`      | Opcional    | Aplica recursivamente en archivos propios             |

```bash
chmod -path=/home -r -ugo=764
chmod -path=/home/user/docs/a.txt -ugo=777
```

---

### 4.25 LOSS _(nuevo P2 — EXT3)_

Simula una pérdida de datos en una partición EXT3. Limpia con `\0`:

- Bitmap de inodos
- Bitmap de bloques
- Área de inodos
- Área de bloques

Solo funciona en **EXT3**. Solo lo puede ejecutar **root**.

| Parámetro | Tipo        | Descripción             |
| --------- | ----------- | ----------------------- |
| `-id`     | Obligatorio | ID de la partición EXT3 |

```bash
loss -id=401A
```

> El Journal permanece intacto. Después de LOSS la partición puede recuperarse leyendo el journal con `journaling -id=401A`.

---

### 4.26 JOURNALING _(nuevo P2 — EXT3)_

Muestra en la interfaz gráfica todas las transacciones registradas. Se visualiza en la tabla del componente `JournalViewer`. Internally llama al endpoint `GET /fs/journal`.

| Parámetro | Tipo        | Descripción             |
| --------- | ----------- | ----------------------- |
| `-id`     | Obligatorio | ID de la partición EXT3 |

```bash
journaling -id=401A
```

Campos mostrados en la tabla de la GUI:

| Operación | Ruta             | Contenido    | Fecha              |
| --------- | ---------------- | ------------ | ------------------ |
| `mkdir`   | `/home`          | —            | `07/04/2026 19:07` |
| `mkfile`  | `/home/user.txt` | `thedark004` | `07/04/2026 19:08` |

---

## 5. Reportes

Todos los reportes se generan con el comando `rep` y se visualizan en la galería del frontend.

```bash
rep -id=<ID> -path=<ruta_salida> -name=<tipo> [-ruta=<path_interno>]
```

| Reporte    | Descripción                               | Requiere `-ruta` |
| ---------- | ----------------------------------------- | ---------------- |
| `mbr`      | Estructura del MBR con particiones y EBRs | No               |
| `disk`     | Diagrama proporcional del disco           | No               |
| `sb`       | Campos del SuperBloque                    | No               |
| `inode`    | Todos los inodos usados con sus bloques   | No               |
| `block`    | Todos los bloques usados con su contenido | No               |
| `bm_inode` | Bitmap de inodos en texto plano           | No               |
| `bm_block` | Bitmap de bloques en texto plano          | No               |
| `tree`     | Árbol completo del sistema de archivos    | No               |
| `file`     | Contenido de un archivo específico        | Sí               |
| `ls`       | Lista de archivos de un directorio        | Sí               |

```bash
rep -id=401A -path=/home/Reportes/mbr.jpg -name=mbr
rep -id=401A -path=/home/Reportes/tree.png -name=tree
rep -id=401A -path=/home/Reportes/bm_inode.txt -name=bm_inode
rep -id=401A -path=/home/Reportes/file.jpg -name=file -ruta=/home/user/Tarea.txt
rep -id=401A -path=/home/Reportes/ls.jpg -name=ls -ruta=/home/user
```

Los reportes `.jpg`/`.png` se generan con **Graphviz** (`dot -Tpng`). Los `.txt` se muestran directamente en el visor de texto de la galería.

---

## 6. Despliegue en AWS

### 6.1 Instancia EC2 — Backend

```
Tipo:       t2.micro (o t3.small para mejor rendimiento)
SO:         Ubuntu 22.04 LTS
Región:     us-east-1
Almacen.:   EBS gp2 20GB
```

**Security Group:**

| Tipo       | Puerto | Origen    | Descripción     |
| ---------- | ------ | --------- | --------------- |
| SSH        | 22     | Mi IP     | Administración  |
| TCP Custom | 18080  | 0.0.0.0/0 | API del backend |

**Comandos de configuración en EC2:**

```bash
# 1. Conectarse
ssh -i key.pem ubuntu@<IP_PUBLICA>

# 2. Instalar dependencias
sudo apt update
sudo apt install -y g++ cmake graphviz nlohmann-json3-dev

# 3. Clonar repositorio
git clone https://github.com/thedark004/MIA_1S2026_P2_202300540.git
cd MIA_1S2026_P2_202300540/backend

# 4. Compilar
chmod +x compile.sh
./compile.sh

# 5. Ejecutar como servicio en background
nohup ./ExtreamFS > /var/log/extreamfs.log 2>&1 &

# 6. Verificar que levantó
curl http://localhost:18080/health
```

**compile.sh:**

```bash
g++ -std=c++17 -O2 \
    -Isrc \
    src/main.cpp \
    src/Analyzer/Analyzer.cpp \
    src/DiskManagement/DiskManagement.cpp \
    src/FileSystem/FileSystem.cpp \
    src/UserSession/UserSession.cpp \
    src/FileOperations/FileOperations.cpp \
    src/FileOperations/FileOperationsCore.cpp \
    src/FileOperations/FileOperationsDir.cpp \
    src/FileOperations/FileOperationsFile.cpp \
    src/FileOperations/FileOperationsSearch.cpp \
    src/Reports/Reports.cpp \
    src/Utilities/Utilities.cpp \
    -lpthread \
    -o ExtreamFS
```

---

### 6.2 Bucket S3 — Frontend

**Pasos:**

```bash
# 1. Actualizar URL del backend en el frontend (antes de compilar)
# En App.jsx y componentes: cambiar "" por "http://<IP_EC2>:18080"
# O mejor: usar variable de entorno en vite.config.js

# 2. Compilar el frontend
cd frontend
npm install
npm run build          # genera carpeta dist/

# 3. Crear bucket S3 (una sola vez)
aws s3 mb s3://extreamfs-p2-202300540 --region us-east-1

# 4. Habilitar hosting estático
aws s3 website s3://extreamfs-p2-202300540 \
    --index-document index.html \
    --error-document index.html

# 5. Subir build
aws s3 sync dist/ s3://extreamfs-p2-202300540 --acl public-read

# 6. URL de acceso
echo "http://extreamfs-p2-202300540.s3-website-us-east-1.amazonaws.com"
```

**Bucket Policy para acceso público:**

```json
{
  "Version": "2012-10-17",
  "Statement": [
    {
      "Sid": "PublicReadGetObject",
      "Effect": "Allow",
      "Principal": "*",
      "Action": "s3:GetObject",
      "Resource": "arn:aws:s3:::extreamfs-p2-202300540/*"
    }
  ]
}
```

---

### 6.3 Diagrama de conexión final

```
Browser del usuario
        │
        │  HTTPS
        ▼
S3 Bucket (React App)
http://extreamfs-p2-202300540.s3-website-us-east-1.amazonaws.com
        │
        │  HTTP :18080 (CORS habilitado en Crow)
        ▼
EC2 Instance (Ubuntu)
http://<IP_PUBLICA>:18080
        │
        ├── POST /execute   → Analyzer → módulos C++
        ├── GET  /fs/*      → FileSystem + FileOperations
        └── GET  /reports/  → archivos .jpg/.png generados por Graphviz
                │
                ▼
        Archivos .mia en EBS
        /home/ubuntu/Discos/
```

---

_Manual Técnico — C++Disk 2.0 (ExtreamFS P2) | MIA 1S2026 | Universidad de San Carlos de Guatemala_
