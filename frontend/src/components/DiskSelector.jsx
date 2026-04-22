import { useState, useEffect } from "react";

export default function DiskSelector({
  onClose,
  onPartitionSelect,
  currentPartitionId,
}) {
  const [disks, setDisks] = useState([]);
  const [selectedDisk, setSelectedDisk] = useState(null);
  const [partitions, setPartitions] = useState([]);
  const [loadingDisks, setLoadingDisks] = useState(false);
  const [loadingParts, setLoadingParts] = useState(false);
  const [error, setError] = useState("");

  useEffect(() => {
    fetchDisks();
  }, []);

  useEffect(() => {
    if (selectedDisk) fetchPartitions(selectedDisk.name);
  }, [selectedDisk]);

  const fetchDisks = async () => {
    setLoadingDisks(true);
    setError("");
    try {
      const res = await fetch("/get_disks");
      if (!res.ok) throw new Error("Error " + res.status);
      const data = await res.json();
      setDisks(data);
      if (data.length > 0) setSelectedDisk(data[0]);
    } catch (err) {
      setError(
        "No se pudo obtener la lista de discos. ¿El backend está corriendo?",
      );
    } finally {
      setLoadingDisks(false);
    }
  };

  const fetchPartitions = async (diskName) => {
    setLoadingParts(true);
    setPartitions([]);
    try {
      const res = await fetch("/get_partitions", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ name: diskName }),
      });
      if (!res.ok) throw new Error("Error " + res.status);
      const data = await res.json();
      setPartitions(data);
    } catch (err) {
      setError("Error al cargar particiones.");
    } finally {
      setLoadingParts(false);
    }
  };

  // Función auxiliar para mostrar el tamaño de forma legible
  const formatBytes = (bytes) => {
    if (bytes === 0) return "0 B";
    const k = 1024;
    const sizes = ["B", "KB", "MB", "GB"];
    const i = Math.floor(Math.log(bytes) / Math.log(k));
    return parseFloat((bytes / Math.pow(k, i)).toFixed(2)) + " " + sizes[i];
  };

  return (
    <div
      style={styles.overlay}
      onClick={(e) => e.target === e.currentTarget && onClose()}
    >
      <div style={styles.container}>
        {/* HEADER */}
        <div style={styles.header}>
          <h2 style={styles.title}>Selección de Disco</h2>
          <button style={styles.closeBtn} onClick={onClose}>
            ✕
          </button>
        </div>

        <div style={styles.layout}>
          {/* SIDEBAR - DISCOS */}
          <div style={styles.sidebar}>
            {loadingDisks ? (
              <span style={styles.statusMsg}>Cargando discos...</span>
            ) : disks.length === 0 ? (
              <span style={styles.statusMsg}>No hay discos montados</span>
            ) : (
              disks.map((d) => (
                <button
                  key={d.path}
                  style={{
                    ...styles.diskTab,
                    ...(selectedDisk?.path === d.path
                      ? styles.diskTabActive
                      : {}),
                  }}
                  onClick={() => setSelectedDisk(d)}
                >
                  <span style={styles.diskIcon}>🖴</span>
                  <div style={styles.diskInfo}>
                    <span style={styles.diskName}>{d.name}</span>
                  </div>
                </button>
              ))
            )}
            <button style={styles.refreshBtn} onClick={fetchDisks}>
              ↻ Actualizar
            </button>
          </div>

          {/* MAIN PANEL - PARTICIONES Y ESTADÍSTICAS */}
          <div style={styles.mainPanel}>
            {error && <div style={styles.errorBox}>{error}</div>}

            {selectedDisk && (
              <div style={styles.diskStatsCard}>
                <div style={styles.statsHeader}>
                  <h3>Estadísticas de {selectedDisk.name}</h3>
                  <span style={styles.fitBadge}>Fit: {selectedDisk.fit}</span>
                </div>

                {/* Barra de progreso de espacio */}
                <div style={styles.progressContainer}>
                  <div
                    style={{
                      ...styles.progressBar,
                      width: `${((selectedDisk.size - selectedDisk.free) / selectedDisk.size) * 100}%`,
                    }}
                  ></div>
                </div>

                <div style={styles.statsLabels}>
                  <span>
                    Usado: {formatBytes(selectedDisk.size - selectedDisk.free)}
                  </span>
                  <span>Libre: {formatBytes(selectedDisk.free)}</span>
                  <span>Total: {formatBytes(selectedDisk.size)}</span>
                </div>
              </div>
            )}

            <h3 style={{ color: "white", marginTop: "10px" }}>Particiones</h3>

            {loadingParts ? (
              <span style={styles.statusMsg}>Cargando particiones...</span>
            ) : partitions.length === 0 ? (
              <span style={styles.statusMsg}>
                Selecciona un disco para ver particiones
              </span>
            ) : (
              <div style={styles.partGrid}>
                {partitions.map((p, idx) => {
                  const isActive =
                    currentPartitionId === p.id && p.id !== "N/A";
                  return (
                    <div
                      key={idx}
                      style={{
                        ...styles.partCard,
                        ...(isActive ? styles.partCardActive : {}),
                      }}
                    >
                      <div style={styles.partHeader}>
                        <span style={styles.partName}>{p.name}</span>
                        {isActive && (
                          <span style={styles.currentBadge}>Activa</span>
                        )}
                      </div>

                      <div style={styles.partDetails}>
                        <span>
                          <strong>ID:</strong> {p.id}
                        </span>
                        <span>
                          <strong>Estado:</strong> {p.status}
                        </span>
                        <span>
                          <strong>Tipo:</strong> {p.type}
                        </span>
                        <span>
                          <strong>Fit:</strong> {p.fit}
                        </span>
                        <span>
                          <strong>Tamaño:</strong> {formatBytes(p.size)}
                        </span>
                      </div>

                      <button
                        style={styles.selectBtn}
                        onClick={() => {
                          if (p.id !== "N/A") {
                            onPartitionSelect(p.id);
                            onClose();
                          } else {
                            alert("Esta partición no está montada.");
                          }
                        }}
                        disabled={p.id === "N/A"}
                      >
                        {p.id === "N/A" ? "No montada" : "Seleccionar"}
                      </button>
                    </div>
                  );
                })}
              </div>
            )}
          </div>
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
    backgroundColor: "rgba(0,0,0,0.6)",
    zIndex: 1000,
    display: "flex",
    alignItems: "center",
    justifyContent: "center",
  },
  container: {
    width: "800px",
    height: "550px",
    backgroundColor: "#0f172a",
    borderRadius: "16px",
    display: "flex",
    flexDirection: "column",
    boxShadow: "0 25px 50px -12px rgba(0,0,0,0.5)",
    overflow: "hidden",
    border: "1px solid rgba(148,163,184,0.1)",
  },
  header: {
    padding: "16px 24px",
    backgroundColor: "#1e293b",
    borderBottom: "1px solid rgba(148,163,184,0.1)",
    display: "flex",
    justifyContent: "space-between",
    alignItems: "center",
  },
  title: { margin: 0, color: "#f8fafc", fontSize: "1.2rem" },
  closeBtn: {
    background: "none",
    border: "none",
    color: "#94a3b8",
    fontSize: "1.2rem",
    cursor: "pointer",
  },
  layout: { display: "flex", flex: 1, overflow: "hidden" },
  sidebar: {
    width: "220px",
    backgroundColor: "#1e293b",
    borderRight: "1px solid rgba(148,163,184,0.1)",
    display: "flex",
    flexDirection: "column",
    padding: "12px",
    gap: "8px",
    overflowY: "auto",
  },
  diskTab: {
    display: "flex",
    alignItems: "center",
    gap: "10px",
    padding: "12px",
    backgroundColor: "transparent",
    border: "none",
    borderRadius: "8px",
    color: "#94a3b8",
    cursor: "pointer",
    textAlign: "left",
    transition: "all 0.2s",
  },
  diskTabActive: { backgroundColor: "rgba(56,189,248,0.1)", color: "#38bdf8" },
  diskIcon: { fontSize: "1.2rem" },
  diskName: { fontWeight: 600, fontSize: "0.9rem" },
  refreshBtn: {
    marginTop: "auto",
    padding: "10px",
    borderRadius: "10px",
    border: "1px solid rgba(56,189,248,0.3)",
    color: "#e2e8f0",
    backgroundColor: "transparent",
    cursor: "pointer",
  },
  mainPanel: {
    flex: 1,
    padding: "20px",
    overflowY: "auto",
    display: "flex",
    flexDirection: "column",
    gap: "16px",
  },
  diskStatsCard: {
    backgroundColor: "rgba(30, 41, 59, 0.6)",
    padding: "16px",
    borderRadius: "12px",
    border: "1px solid rgba(148, 163, 184, 0.2)",
  },
  statsHeader: {
    display: "flex",
    justifyContent: "space-between",
    color: "white",
    marginBottom: "12px",
  },
  fitBadge: {
    backgroundColor: "#38bdf8",
    color: "#0f172a",
    padding: "2px 8px",
    borderRadius: "6px",
    fontSize: "0.8rem",
    fontWeight: "bold",
  },
  progressContainer: {
    width: "100%",
    height: "12px",
    backgroundColor: "#334155",
    borderRadius: "6px",
    overflow: "hidden",
    marginBottom: "8px",
  },
  progressBar: { height: "100%", backgroundColor: "#34d399" },
  statsLabels: {
    display: "flex",
    justifyContent: "space-between",
    color: "#94a3b8",
    fontSize: "0.8rem",
  },
  partGrid: { display: "grid", gridTemplateColumns: "1fr 1fr", gap: "16px" },
  partCard: {
    backgroundColor: "rgba(15,23,42,0.9)",
    border: "1px solid rgba(148,163,184,0.1)",
    borderRadius: "12px",
    padding: "16px",
    display: "flex",
    flexDirection: "column",
    gap: "10px",
  },
  partCardActive: {
    borderColor: "rgba(34,197,94,0.4)",
    backgroundColor: "rgba(34,197,94,0.06)",
  },
  partHeader: { display: "flex", justifyContent: "space-between" },
  partName: { color: "#f8fafc", fontWeight: 600, fontSize: "1rem" },
  currentBadge: {
    backgroundColor: "rgba(34,197,94,0.2)",
    color: "#4ade80",
    padding: "2px 8px",
    borderRadius: "12px",
    fontSize: "0.7rem",
  },
  partDetails: {
    display: "flex",
    flexDirection: "column",
    gap: "4px",
    color: "#94a3b8",
    fontSize: "0.85rem",
  },
  selectBtn: {
    marginTop: "auto",
    padding: "8px",
    backgroundColor: "#38bdf8",
    color: "#0f172a",
    border: "none",
    borderRadius: "6px",
    fontWeight: 600,
    cursor: "pointer",
  },
  statusMsg: { color: "#94a3b8", padding: "16px" },
  errorBox: {
    backgroundColor: "#7f1d1d",
    color: "#fca5a5",
    padding: "12px",
    borderRadius: "8px",
  },
};
