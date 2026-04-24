import { useState, useEffect } from "react";

const BACKEND_URL = import.meta.env.VITE_BACKEND_URL ?? "";

export default function JournalViewer({ onClose, partitionId }) {
  const [journal, setJournal] = useState([]);
  const [isLoading, setIsLoading] = useState(false);
  const [error, setError] = useState("");

  useEffect(() => {
    if (partitionId) {
      fetchJournal();
    } else {
      setError("No hay ninguna partición montada y activa.");
    }
  }, [partitionId]);

  const fetchJournal = async () => {
    setIsLoading(true);
    setError("");
    try {
      const res = await fetch(`${BACKEND_URL}/fs/journal?id=${partitionId}`);
      if (!res.ok) throw new Error("Error al obtener Journaling");
      const data = await res.json();
      setJournal(data);
    } catch (err) {
      setError("No se pudo cargar el Journaling o la partición no es EXT3.");
    } finally {
      setIsLoading(false);
    }
  };

  return (
    <div
      style={styles.overlay}
      onClick={(e) => e.target === e.currentTarget && onClose()}
    >
      <div style={styles.container}>
        <div style={styles.header}>
          <h2 style={styles.title}>Reporte de Journaling (EXT3)</h2>
          <button style={styles.closeBtn} onClick={onClose}>
            ✕
          </button>
        </div>

        <div style={styles.content}>
          {error && <div style={styles.errorBox}>{error}</div>}

          {isLoading ? (
            <p style={styles.msg}>Cargando transacciones...</p>
          ) : journal.length === 0 && !error ? (
            <p style={styles.msg}>
              No hay registros de Journaling o la partición es EXT2.
            </p>
          ) : (
            <div style={styles.tableWrapper}>
              <table style={styles.table}>
                <thead>
                  <tr>
                    <th style={styles.th}>Operación</th>
                    <th style={styles.th}>Path / Ruta</th>
                    <th style={styles.th}>Contenido</th>
                    <th style={styles.th}>Fecha y Hora</th>
                  </tr>
                </thead>
                <tbody>
                  {journal.map((item, idx) => (
                    <tr key={idx} style={styles.tr}>
                      <td style={styles.td}>{item.operation}</td>
                      <td style={styles.td}>{item.path}</td>
                      <td style={styles.td}>{item.content || "-"}</td>
                      <td style={styles.td}>{item.date}</td>
                    </tr>
                  ))}
                </tbody>
              </table>
            </div>
          )}
        </div>
      </div>
    </div>
  );
}

const styles = {
  overlay: {
    position: "fixed",
    top: 0,
    left: 0,
    right: 0,
    bottom: 0,
    backgroundColor: "rgba(0,0,0,0.7)",
    zIndex: 1100,
    display: "flex",
    alignItems: "center",
    justifyContent: "center",
  },
  container: {
    width: "900px",
    height: "600px",
    backgroundColor: "#0d1117", // Color oscuro tipo terminal
    borderRadius: "12px",
    display: "flex",
    flexDirection: "column",
    border: "1px solid #30363d",
    boxShadow: "0 25px 50px -12px rgba(0,0,0,0.8)",
  },
  header: {
    padding: "16px 20px",
    backgroundColor: "#161b22",
    borderBottom: "1px solid #30363d",
    display: "flex",
    justifyContent: "space-between",
    alignItems: "center",
    borderTopLeftRadius: "12px",
    borderTopRightRadius: "12px",
  },
  title: {
    margin: 0,
    color: "#58a6ff",
    fontSize: "1.2rem",
    fontFamily: "monospace",
  },
  closeBtn: {
    background: "none",
    border: "none",
    color: "#8b949e",
    fontSize: "1.2rem",
    cursor: "pointer",
  },
  content: {
    padding: "20px",
    flex: 1,
    overflow: "hidden",
    display: "flex",
    flexDirection: "column",
  },
  tableWrapper: {
    overflowY: "auto",
    flex: 1,
    border: "1px solid #30363d",
    borderRadius: "6px",
  },
  table: {
    width: "100%",
    borderCollapse: "collapse",
    fontFamily: "monospace",
    fontSize: "0.9rem",
  },
  th: {
    position: "sticky",
    top: 0,
    backgroundColor: "#21262d",
    color: "#c9d1d9",
    padding: "10px",
    textAlign: "left",
    borderBottom: "1px solid #30363d",
  },
  td: {
    padding: "10px",
    color: "#7ee787", // Verde tipo consola
    borderBottom: "1px solid #30363d",
  },
  tr: { transition: "background-color 0.2s" },
  msg: { color: "#8b949e", fontFamily: "monospace" },
  errorBox: {
    backgroundColor: "rgba(248, 81, 73, 0.1)",
    color: "#ff7b72",
    padding: "12px",
    borderRadius: "6px",
    marginBottom: "16px",
    border: "1px solid rgba(248, 81, 73, 0.4)",
  },
};
