import { useState, useEffect, useCallback } from "react";

const BACKEND = import.meta.env.VITE_BACKEND_URL ?? "";

export default function FileExplorer({ onClose }) {
  const [entries, setEntries] = useState([]);
  const [currentPath, setCurrentPath] = useState("/");
  const [breadcrumbs, setBreadcrumbs] = useState(["/"]);
  const [isLoading, setIsLoading] = useState(false);
  const [error, setError] = useState("");
  const [selectedFile, setSelectedFile] = useState(null);
  const [fileContent, setFileContent] = useState("");
  const [fileLoading, setFileLoading] = useState(false);

  // Cerrar con Escape
  useEffect(() => {
    const handler = (e) => {
      if (e.key === "Escape") onClose();
    };
    window.addEventListener("keydown", handler);
    return () => window.removeEventListener("keydown", handler);
  }, [onClose]);

  // Cargar directorio
  const loadDir = useCallback(async (path) => {
    setIsLoading(true);
    setError("");
    setSelectedFile(null);
    setFileContent("");
    try {
      const res = await fetch(
        `${BACKEND}/fs/browse?path=${encodeURIComponent(path)}`,
      );
      const data = await res.json();
      if (!res.ok) {
        setError(data.error || "Error al cargar directorio");
        setEntries([]);
        return;
      }
      setEntries(data.entries || []);
      setCurrentPath(path);
      // Construir breadcrumbs
      if (path === "/") {
        setBreadcrumbs(["/"]);
      } else {
        const parts = path.split("/").filter(Boolean);
        setBreadcrumbs(["/", ...parts]);
      }
    } catch (err) {
      setError("No se pudo conectar al backend.");
      setEntries([]);
    } finally {
      setIsLoading(false);
    }
  }, []);

  useEffect(() => {
    loadDir("/");
  }, [loadDir]);

  // Leer archivo
  const openFile = async (entry) => {
    setSelectedFile(entry);
    setFileContent("");
    setFileLoading(true);
    try {
      const res = await fetch(
        `${BACKEND}/fs/read?path=${encodeURIComponent(entry.path)}`,
      );
      const data = await res.json();
      if (!res.ok) {
        setFileContent("Error: " + (data.error || "No se pudo leer"));
        return;
      }
      setFileContent(data.content || "(archivo vacío)");
    } catch {
      setFileContent("Error al conectar con el backend.");
    } finally {
      setFileLoading(false);
    }
  };

  // Navegar a breadcrumb
  const navigateBreadcrumb = (idx) => {
    if (idx === 0) {
      loadDir("/");
      return;
    }
    const parts = breadcrumbs.slice(1, idx + 1);
    loadDir("/" + parts.join("/"));
  };

  // Volver al padre
  const goUp = () => {
    if (currentPath === "/") return;
    const parent =
      currentPath.substring(0, currentPath.lastIndexOf("/")) || "/";
    loadDir(parent);
  };

  return (
    <div
      style={S.overlay}
      onClick={(e) => e.target === e.currentTarget && onClose()}
    >
      <div style={S.modal}>
        {/* Header */}
        <div style={S.header}>
          <div style={S.headerLeft}>
            <span style={S.headerIcon}></span>
            <span style={S.headerTitle}>Explorador de Archivos</span>
          </div>
          <button style={S.closeBtn} onClick={onClose}>
            ✕
          </button>
        </div>

        {/* Breadcrumb + acciones */}
        <div style={S.toolbar}>
          <div style={S.breadcrumbs}>
            {breadcrumbs.map((crumb, i) => (
              <span key={i} style={S.breadcrumbWrap}>
                {i > 0 && <span style={S.breadcrumbSep}>/</span>}
                <button
                  style={{
                    ...S.breadcrumbBtn,
                    ...(i === breadcrumbs.length - 1 ? S.breadcrumbActive : {}),
                  }}
                  onClick={() => navigateBreadcrumb(i)}
                >
                  {crumb}
                </button>
              </span>
            ))}
          </div>
          <div style={S.toolbarActions}>
            <button
              style={S.toolBtn}
              onClick={goUp}
              disabled={currentPath === "/"}
            >
              Subir
            </button>
            <button style={S.toolBtn} onClick={() => loadDir(currentPath)}>
              Recargar
            </button>
          </div>
        </div>

        {/* Cuerpo: lista + visor */}
        <div style={S.body}>
          {/* Lista de archivos */}
          <div style={S.fileList}>
            {/* Cabecera de columnas */}
            <div style={S.colHeader}>
              <span style={{ ...S.col, flex: 3 }}>Nombre</span>
              <span style={{ ...S.col, flex: 1 }}>Permisos</span>
              <span style={{ ...S.col, flex: 1 }}>UID</span>
              <span style={{ ...S.col, flex: 1 }}>GID</span>
              <span style={{ ...S.col, flex: 1 }}>Tamaño</span>
              <span style={{ ...S.col, flex: 2 }}>Modificado</span>
              <span style={{ ...S.col, flex: 1 }}></span>
            </div>

            {isLoading ? (
              <div style={S.statusMsg}> Cargando...</div>
            ) : error ? (
              <div style={S.errorMsg}>{error}</div>
            ) : entries.length === 0 ? (
              <div style={S.statusMsg}>Carpeta vacía...</div>
            ) : (
              entries.map((entry) => {
                const isSelected = selectedFile?.path === entry.path;
                return (
                  <div
                    key={entry.path}
                    style={{
                      ...S.fileRow,
                      ...(isSelected ? S.fileRowSelected : {}),
                    }}
                    onClick={() => entry.type === "file" && openFile(entry)}
                  >
                    <span
                      style={{
                        ...S.col,
                        flex: 3,
                        gap: "8px",
                        display: "flex",
                        alignItems: "center",
                      }}
                    >
                      <span>{entry.type === "directory" ? "📁" : "📄"}</span>
                      <span style={S.fileName}>{entry.name}</span>
                    </span>
                    <span
                      style={{
                        ...S.col,
                        flex: 1,
                        fontFamily: "monospace",
                        fontSize: "0.75rem",
                        color: "#7fba9f",
                      }}
                    >
                      {entry.permissions}
                    </span>
                    <span
                      style={{
                        ...S.col,
                        flex: 1,
                        color: "rgba(233,233,235,0.6)",
                      }}
                    >
                      {entry.uid}
                    </span>
                    <span
                      style={{
                        ...S.col,
                        flex: 1,
                        color: "rgba(233,233,235,0.6)",
                      }}
                    >
                      {entry.gid}
                    </span>
                    <span
                      style={{
                        ...S.col,
                        flex: 1,
                        color: "rgba(233,233,235,0.6)",
                      }}
                    >
                      {entry.type === "file" ? `${entry.size}B` : "—"}
                    </span>
                    <span
                      style={{
                        ...S.col,
                        flex: 2,
                        fontSize: "0.72rem",
                        color: "rgba(233,233,235,0.5)",
                      }}
                    >
                      {entry.mtime?.slice(0, 16) || "—"}
                    </span>
                    <span style={{ ...S.col, flex: 1 }}>
                      {entry.type === "directory" ? (
                        <button
                          style={S.enterBtn}
                          onClick={(e) => {
                            e.stopPropagation();
                            loadDir(entry.path);
                          }}
                        >
                          Entrar →
                        </button>
                      ) : (
                        <button
                          style={S.viewBtn}
                          onClick={(e) => {
                            e.stopPropagation();
                            openFile(entry);
                          }}
                        >
                          Ver
                        </button>
                      )}
                    </span>
                  </div>
                );
              })
            )}
          </div>

          {/* Visor de archivo */}
          <div style={S.preview}>
            <div style={S.previewHeader}>
              <span style={S.previewTitle}>Vista previa</span>
              {selectedFile && (
                <span style={S.previewPath}>{selectedFile.path}</span>
              )}
            </div>
            <div style={S.previewBody}>
              {!selectedFile ? (
                <div style={S.previewEmpty}>
                  Selecciona un archivo para ver su contenido
                </div>
              ) : fileLoading ? (
                <div style={S.previewEmpty}>⏳ Cargando...</div>
              ) : (
                <>
                  {selectedFile && (
                    <div style={S.fileMeta}>
                      <span> {selectedFile.permissions}</span>
                      <span> UID: {selectedFile.uid}</span>
                      <span>GID: {selectedFile.gid}</span>
                      <span>{selectedFile.size} bytes</span>
                    </div>
                  )}
                  <pre style={S.previewContent}>{fileContent}</pre>
                </>
              )}
            </div>
          </div>
        </div>
      </div>
    </div>
  );
}

const S = {
  overlay: {
    position: "fixed",
    inset: 0,
    backgroundColor: "rgba(10,15,25,0.8)",
    backdropFilter: "blur(6px)",
    display: "flex",
    alignItems: "center",
    justifyContent: "center",
    zIndex: 1000,
    padding: "20px",
  },
  modal: {
    backgroundColor: "#1a2332",
    border: "1px solid rgba(61,97,155,0.35)",
    borderRadius: "12px",
    width: "92vw",
    maxWidth: "1200px",
    height: "85vh",
    display: "flex",
    flexDirection: "column",
    overflow: "hidden",
    boxShadow: "0 24px 80px rgba(0,0,0,0.6)",
  },
  header: {
    display: "flex",
    justifyContent: "space-between",
    alignItems: "center",
    padding: "14px 20px",
    borderBottom: "1px solid rgba(61,97,155,0.25)",
    backgroundColor: "rgba(67,80,108,0.4)",
  },
  headerLeft: { display: "flex", alignItems: "center", gap: "10px" },
  headerIcon: { fontSize: "1.2rem" },
  headerTitle: { fontWeight: 700, color: "#E9E9EB", fontSize: "0.95rem" },
  closeBtn: {
    background: "none",
    border: "none",
    color: "#E9E9EB",
    fontSize: "1.1rem",
    cursor: "pointer",
    padding: "4px 8px",
  },
  toolbar: {
    display: "flex",
    justifyContent: "space-between",
    alignItems: "center",
    padding: "10px 16px",
    borderBottom: "1px solid rgba(61,97,155,0.2)",
    backgroundColor: "rgba(20,28,40,0.5)",
    gap: "12px",
  },
  breadcrumbs: {
    display: "flex",
    alignItems: "center",
    gap: "2px",
    flexWrap: "wrap",
  },
  breadcrumbWrap: { display: "flex", alignItems: "center" },
  breadcrumbSep: {
    color: "rgba(233,233,235,0.3)",
    margin: "0 2px",
    fontSize: "0.8rem",
  },
  breadcrumbBtn: {
    background: "none",
    border: "none",
    color: "rgba(233,233,235,0.6)",
    fontSize: "0.82rem",
    cursor: "pointer",
    padding: "2px 6px",
    borderRadius: "4px",
    fontFamily: "'IBM Plex Mono', monospace",
  },
  breadcrumbActive: { color: "#E9E9EB", fontWeight: 700 },
  toolbarActions: { display: "flex", gap: "8px" },
  toolBtn: {
    backgroundColor: "rgba(61,97,155,0.25)",
    border: "1px solid rgba(61,97,155,0.4)",
    color: "#E9E9EB",
    borderRadius: "6px",
    padding: "5px 10px",
    fontSize: "0.78rem",
    cursor: "pointer",
    fontFamily: "inherit",
  },
  body: {
    display: "grid",
    gridTemplateColumns: "1fr 380px",
    flex: 1,
    overflow: "hidden",
  },
  fileList: {
    display: "flex",
    flexDirection: "column",
    overflow: "hidden",
    borderRight: "1px solid rgba(61,97,155,0.2)",
  },
  colHeader: {
    display: "flex",
    alignItems: "center",
    padding: "8px 16px",
    backgroundColor: "rgba(30,42,58,0.8)",
    borderBottom: "1px solid rgba(61,97,155,0.2)",
    fontSize: "0.72rem",
    fontWeight: 700,
    color: "rgba(233,233,235,0.5)",
    letterSpacing: "0.05em",
    textTransform: "uppercase",
  },
  col: { overflow: "hidden", textOverflow: "ellipsis", whiteSpace: "nowrap" },
  fileRow: {
    display: "flex",
    alignItems: "center",
    padding: "8px 16px",
    borderBottom: "1px solid rgba(61,97,155,0.1)",
    color: "#E9E9EB",
    fontSize: "0.83rem",
    cursor: "pointer",
    transition: "background 0.1s",
  },
  fileRowSelected: { backgroundColor: "rgba(61,97,155,0.2)" },
  fileName: {
    overflow: "hidden",
    textOverflow: "ellipsis",
    whiteSpace: "nowrap",
  },
  statusMsg: {
    padding: "24px",
    color: "rgba(233,233,235,0.5)",
    textAlign: "center",
    fontSize: "0.85rem",
  },
  errorMsg: { padding: "16px", color: "#EF4B4C", fontSize: "0.85rem" },
  enterBtn: {
    backgroundColor: "rgba(61,97,155,0.35)",
    border: "1px solid rgba(61,97,155,0.5)",
    color: "#E9E9EB",
    borderRadius: "5px",
    padding: "3px 8px",
    fontSize: "0.73rem",
    cursor: "pointer",
    fontFamily: "inherit",
  },
  viewBtn: {
    backgroundColor: "rgba(127,186,159,0.2)",
    border: "1px solid rgba(127,186,159,0.4)",
    color: "#7fba9f",
    borderRadius: "5px",
    padding: "3px 8px",
    fontSize: "0.73rem",
    cursor: "pointer",
    fontFamily: "inherit",
  },
  preview: {
    display: "flex",
    flexDirection: "column",
    overflow: "hidden",
    backgroundColor: "rgba(15,20,30,0.5)",
  },
  previewHeader: {
    padding: "10px 16px",
    borderBottom: "1px solid rgba(61,97,155,0.2)",
    display: "flex",
    flexDirection: "column",
    gap: "4px",
  },
  previewTitle: {
    fontWeight: 700,
    fontSize: "0.8rem",
    color: "rgba(233,233,235,0.7)",
    textTransform: "uppercase",
    letterSpacing: "0.05em",
  },
  previewPath: {
    fontSize: "0.72rem",
    color: "rgba(233,233,235,0.45)",
    wordBreak: "break-all",
  },
  previewBody: {
    flex: 1,
    overflow: "hidden",
    display: "flex",
    flexDirection: "column",
  },
  previewEmpty: {
    padding: "24px",
    color: "rgba(233,233,235,0.35)",
    fontSize: "0.82rem",
    textAlign: "center",
  },
  fileMeta: {
    display: "flex",
    flexWrap: "wrap",
    gap: "12px",
    padding: "10px 14px",
    borderBottom: "1px solid rgba(61,97,155,0.15)",
    fontSize: "0.72rem",
    color: "rgba(233,233,235,0.6)",
    fontFamily: "'IBM Plex Mono', monospace",
  },
  previewContent: {
    flex: 1,
    overflow: "auto",
    margin: 0,
    padding: "14px",
    fontSize: "0.78rem",
    color: "#7fba9f",
    fontFamily: "'IBM Plex Mono', monospace",
    lineHeight: 1.6,
    whiteSpace: "pre-wrap",
    wordBreak: "break-all",
  },
};
