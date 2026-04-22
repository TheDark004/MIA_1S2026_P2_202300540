import { useState, useEffect } from "react"

export default function ReportGallery({ reports, onClose, onClear }) {
  const [selected, setSelected] = useState(0)

  // Cerrar con Escape
  useEffect(() => {
    const handler = (e) => { if (e.key === "Escape") onClose() }
    window.addEventListener("keydown", handler)
    return () => window.removeEventListener("keydown", handler)
  }, [onClose])

  // Navegar con flechas
  useEffect(() => {
    const handler = (e) => {
      if (e.key === "ArrowRight") setSelected(p => Math.min(p + 1, reports.length - 1))
      if (e.key === "ArrowLeft")  setSelected(p => Math.max(p - 1, 0))
    }
    window.addEventListener("keydown", handler)
    return () => window.removeEventListener("keydown", handler)
  }, [reports.length])

  const current = reports[selected]
  const isTxt = current?.filename?.endsWith(".txt")

  return (
    <div style={styles.overlay} onClick={(e) => e.target === e.currentTarget && onClose()}>
      <div style={styles.modal}>

        {/* Header del modal */}
        <div style={styles.modalHeader}>
          <div style={styles.modalTitle}>
            <span style={styles.modalIcon}>📊</span>
            <span>Reportes Generados</span>
            <span style={styles.modalCount}>{reports.length}</span>
          </div>
          <div style={styles.modalActions}>
            <button style={styles.clearBtn} onClick={onClear}>
              🗑 Limpiar
            </button>
            <button style={styles.closeBtn} onClick={onClose}>✕</button>
          </div>
        </div>

        <div style={styles.modalBody}>

          {/* Sidebar de miniaturas */}
          <div style={styles.sidebar}>
            {reports.map((r, i) => (
              <div
                key={i}
                style={{
                  ...styles.thumb,
                  ...(i === selected ? styles.thumbActive : {})
                }}
                onClick={() => setSelected(i)}
              >
                <span style={styles.thumbIcon}>
                  {r.filename.endsWith(".txt") ? "📄" : "🖼"}
                </span>
                <span style={styles.thumbName}>{r.filename}</span>
              </div>
            ))}
          </div>

          {/* Visor principal */}
          <div style={styles.viewer}>

            {/* Barra de navegación */}
            <div style={styles.navBar}>
              <button
                style={{ ...styles.navBtn, opacity: selected === 0 ? 0.3 : 1 }}
                onClick={() => setSelected(p => Math.max(p - 1, 0))}
                disabled={selected === 0}
              >
                ← Anterior
              </button>
              <span style={styles.navInfo}>
                {current?.filename} · {selected + 1} / {reports.length}
              </span>
              <button
                style={{ ...styles.navBtn, opacity: selected === reports.length - 1 ? 0.3 : 1 }}
                onClick={() => setSelected(p => Math.min(p + 1, reports.length - 1))}
                disabled={selected === reports.length - 1}
              >
                Siguiente →
              </button>
            </div>

            {/* Contenido */}
            <div style={styles.imageContainer}>
              {isTxt ? (
                <TxtViewer url={current.url} />
              ) : (
                <img
                  src={current?.url}
                  alt={current?.filename}
                  style={styles.reportImage}
                  onError={(e) => {
                    e.target.style.display = "none"
                    e.target.nextSibling.style.display = "flex"
                  }}
                />
              )}
              <div style={{ ...styles.errorMsg, display: "none" }}>
                ⚠ No se pudo cargar la imagen
              </div>
            </div>

          </div>
        </div>

      </div>
    </div>
  )
}

// Visor para archivos .txt
function TxtViewer({ url }) {
  const [content, setContent] = useState("Cargando...")

  useEffect(() => {
    fetch(url)
      .then(r => r.text())
      .then(t => setContent(t))
      .catch(() => setContent("Error al cargar el archivo"))
  }, [url])

  return (
    <pre style={styles.txtViewer}>{content}</pre>
  )
}

const styles = {
  overlay: {
    position:        "fixed",
    inset:           0,
    backgroundColor: "rgba(20, 28, 40, 0.85)",
    backdropFilter:  "blur(6px)",
    zIndex:          1000,
    display:         "flex",
    alignItems:      "center",
    justifyContent:  "center",
    padding:         "20px",
  },
  modal: {
    backgroundColor: "#1e2a3a",
    border:          "1px solid rgba(61, 97, 155, 0.4)",
    borderRadius:    "12px",
    width:           "90vw",
    maxWidth:        "1200px",
    height:          "85vh",
    display:         "flex",
    flexDirection:   "column",
    overflow:        "hidden",
    boxShadow:       "0 24px 80px rgba(0,0,0,0.6)",
  },
  modalHeader: {
    display:         "flex",
    justifyContent:  "space-between",
    alignItems:      "center",
    padding:         "16px 20px",
    borderBottom:    "1px solid rgba(61, 97, 155, 0.3)",
    backgroundColor: "rgba(67, 80, 108, 0.4)",
  },
  modalTitle: {
    display:    "flex",
    alignItems: "center",
    gap:        "10px",
    fontSize:   "0.95rem",
    fontWeight: "600",
    color:      "#E9E9EB",
    fontFamily: "'IBM Plex Mono', monospace",
  },
  modalIcon: {
    fontSize: "1.1rem",
  },
  modalCount: {
    backgroundColor: "#EF4B4C",
    color:           "white",
    borderRadius:    "10px",
    padding:         "1px 8px",
    fontSize:        "0.7rem",
    fontWeight:      "700",
  },
  modalActions: {
    display: "flex",
    gap:     "10px",
  },
  clearBtn: {
    backgroundColor: "transparent",
    border:          "1px solid rgba(239, 75, 76, 0.4)",
    color:           "rgba(239, 75, 76, 0.8)",
    borderRadius:    "6px",
    padding:         "6px 12px",
    fontSize:        "0.75rem",
    cursor:          "pointer",
    fontFamily:      "inherit",
  },
  closeBtn: {
    backgroundColor: "rgba(67, 80, 108, 0.6)",
    border:          "1px solid rgba(61, 97, 155, 0.3)",
    color:           "#E9E9EB",
    borderRadius:    "6px",
    padding:         "6px 12px",
    fontSize:        "0.85rem",
    cursor:          "pointer",
    fontFamily:      "inherit",
  },
  modalBody: {
    display:  "flex",
    flex:     1,
    overflow: "hidden",
  },
  sidebar: {
    width:           "200px",
    borderRight:     "1px solid rgba(61, 97, 155, 0.25)",
    overflowY:       "auto",
    padding:         "12px 8px",
    display:         "flex",
    flexDirection:   "column",
    gap:             "4px",
    backgroundColor: "rgba(20, 28, 40, 0.5)",
  },
  thumb: {
    display:         "flex",
    alignItems:      "center",
    gap:             "8px",
    padding:         "8px 10px",
    borderRadius:    "6px",
    cursor:          "pointer",
    transition:      "all 0.15s",
    border:          "1px solid transparent",
  },
  thumbActive: {
    backgroundColor: "rgba(61, 97, 155, 0.3)",
    border:          "1px solid rgba(61, 97, 155, 0.5)",
  },
  thumbIcon: {
    fontSize: "0.9rem",
  },
  thumbName: {
    fontSize:     "0.7rem",
    color:        "rgba(233, 233, 235, 0.7)",
    overflow:     "hidden",
    textOverflow: "ellipsis",
    whiteSpace:   "nowrap",
  },
  viewer: {
    flex:          1,
    display:       "flex",
    flexDirection: "column",
    overflow:      "hidden",
  },
  navBar: {
    display:         "flex",
    justifyContent:  "space-between",
    alignItems:      "center",
    padding:         "10px 20px",
    borderBottom:    "1px solid rgba(61, 97, 155, 0.2)",
    backgroundColor: "rgba(67, 80, 108, 0.2)",
  },
  navBtn: {
    backgroundColor: "rgba(61, 97, 155, 0.3)",
    border:          "1px solid rgba(61, 97, 155, 0.4)",
    color:           "#E9E9EB",
    borderRadius:    "6px",
    padding:         "5px 12px",
    fontSize:        "0.75rem",
    cursor:          "pointer",
    fontFamily:      "inherit",
    transition:      "opacity 0.15s",
  },
  navInfo: {
    fontSize:  "0.75rem",
    color:     "rgba(233, 233, 235, 0.5)",
    fontFamily: "inherit",
  },
  imageContainer: {
    flex:           1,
    overflow:       "auto",
    display:        "flex",
    alignItems:     "flex-start",
    justifyContent: "center",
    padding:        "20px",
    backgroundColor: "rgba(15, 20, 30, 0.6)",
  },
  reportImage: {
    maxWidth:     "100%",
    borderRadius: "8px",
    boxShadow:    "0 4px 24px rgba(0,0,0,0.4)",
    border:       "1px solid rgba(61, 97, 155, 0.2)",
  },
  errorMsg: {
    color:          "rgba(239, 75, 76, 0.7)",
    fontSize:       "0.85rem",
    alignItems:     "center",
    justifyContent: "center",
    padding:        "40px",
  },
  txtViewer: {
    backgroundColor: "rgba(20, 28, 40, 0.8)",
    border:          "1px solid rgba(61, 97, 155, 0.2)",
    borderRadius:    "8px",
    padding:         "20px",
    fontSize:        "0.78rem",
    color:           "#7fba9f",
    fontFamily:      "'IBM Plex Mono', monospace",
    lineHeight:      "1.6",
    whiteSpace:      "pre",
    overflow:        "auto",
    maxWidth:        "100%",
    margin:          "0",
  },
}