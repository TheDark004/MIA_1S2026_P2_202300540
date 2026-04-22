import { useEffect, useRef } from "react";

export default function Terminal({
  inputText,
  outputText,
  isLoading,
  onInputChange,
  onExecute,
  onLoadScript,
  onClearOutput,
  onClearInput,
}) {
  const outputRef = useRef(null);

  useEffect(() => {
    if (outputRef.current) {
      outputRef.current.scrollTop = outputRef.current.scrollHeight;
    }
  }, [outputText]);

  const handleKeyDown = (e) => {
    if (e.ctrlKey && e.key === "Enter") onExecute();
  };

  return (
    <div style={styles.container}>
      {/* ── Panel izquierdo: ENTRADA ── */}
      <div style={styles.panel}>
        <div style={styles.panelHeader}>
          <div style={styles.panelTitle}>
            <span style={styles.dot} />
            <span style={styles.panelLabel}>Entrada</span>
          </div>
          <span style={styles.hint}>Ctrl+Enter para ejecutar</span>
        </div>

        <textarea
          style={styles.textarea}
          value={inputText}
          onChange={(e) => onInputChange(e.target.value)}
          onKeyDown={handleKeyDown}
          placeholder={
            "# Escribe comandos o carga un script .smia\n\nmkdisk -size=50 -unit=M -fit=FF -path=/home/user/Disco1.mia\nfdisk -type=P -unit=M -name=Part1 -size=10 -path=...\nmount -path=... -name=Part1\nmkfs -id=401A\nlogin -user=root -pass=123 -id=401A"
          }
          spellCheck={false}
        />

        <div style={styles.buttonRow}>
          <button
            style={{
              ...styles.btn,
              ...styles.btnPrimary,
              opacity: isLoading ? 0.6 : 1,
            }}
            onClick={onExecute}
            disabled={isLoading}
          >
            {isLoading ? (
              <>
                <span style={styles.spinner} /> Ejecutando...
              </>
            ) : (
              <>▶ Ejecutar</>
            )}
          </button>

          <button
            style={{ ...styles.btn, ...styles.btnSecondary }}
            onClick={onLoadScript}
          >
            Cargar Script
          </button>

          <button
            style={{ ...styles.btn, ...styles.btnGhost }}
            onClick={onClearInput}
          >
            Limpiar
          </button>
        </div>
      </div>

      {/* ── Panel derecho: SALIDA ── */}
      <div style={styles.panel}>
        <div style={styles.panelHeader}>
          <div style={styles.panelTitle}>
            <span style={{ ...styles.dot, backgroundColor: "#EF4B4C" }} />
            <span style={styles.panelLabel}>Salida</span>
          </div>
          <button
            style={{ ...styles.btn, ...styles.btnTiny }}
            onClick={onClearOutput}
          >
            Limpiar
          </button>
        </div>

        <textarea
          ref={outputRef}
          style={{ ...styles.textarea, ...styles.outputArea }}
          value={outputText}
          readOnly={true}
          spellCheck={false}
        />
      </div>
    </div>
  );
}

const styles = {
  container: {
    display: "grid",
    gridTemplateColumns: "1fr 1fr",
    gap: "20px",
    height: "calc(100vh - 140px)",
  },
  panel: {
    display: "flex",
    flexDirection: "column",
    gap: "10px",
  },
  panelHeader: {
    display: "flex",
    justifyContent: "space-between",
    alignItems: "center",
    padding: "10px 14px",
    backgroundColor: "rgba(67, 80, 108, 0.6)",
    backdropFilter: "blur(8px)",
    borderRadius: "8px",
    border: "1px solid rgba(61, 97, 155, 0.3)",
  },
  panelTitle: {
    display: "flex",
    alignItems: "center",
    gap: "8px",
  },
  dot: {
    width: "8px",
    height: "8px",
    borderRadius: "50%",
    backgroundColor: "#3D619B",
    display: "inline-block",
  },
  panelLabel: {
    color: "#E9E9EB",
    fontWeight: "600",
    fontSize: "0.8rem",
    letterSpacing: "0.06em",
    textTransform: "uppercase",
  },
  hint: {
    color: "rgba(233, 233, 235, 0.3)",
    fontSize: "0.7rem",
  },
  textarea: {
    flex: 1,
    backgroundColor: "rgba(30, 42, 58, 0.8)",
    color: "#E9E9EB",
    border: "1px solid rgba(61, 97, 155, 0.25)",
    borderRadius: "8px",
    padding: "16px",
    fontFamily: "'IBM Plex Mono', 'Fira Code', monospace",
    fontSize: "0.82rem",
    resize: "none",
    outline: "none",
    lineHeight: "1.7",
    caretColor: "#EF4B4C",
  },
  outputArea: {
    backgroundColor: "rgba(20, 28, 40, 0.9)",
    color: "#7fba9f",
    border: "1px solid rgba(61, 97, 155, 0.2)",
  },
  buttonRow: {
    display: "flex",
    gap: "8px",
  },
  btn: {
    cursor: "pointer",
    border: "none",
    borderRadius: "6px",
    padding: "8px 16px",
    fontFamily: "'IBM Plex Mono', monospace",
    fontSize: "0.8rem",
    fontWeight: "600",
    transition: "all 0.15s",
    display: "flex",
    alignItems: "center",
    gap: "6px",
    letterSpacing: "0.02em",
  },
  btnPrimary: {
    backgroundColor: "#EF4B4C",
    color: "#fff",
    flex: 1,
    justifyContent: "center",
    boxShadow: "0 2px 12px rgba(239, 75, 76, 0.3)",
  },
  btnSecondary: {
    backgroundColor: "rgba(61, 97, 155, 0.4)",
    color: "#E9E9EB",
    border: "1px solid rgba(61, 97, 155, 0.5)",
  },
  btnGhost: {
    backgroundColor: "rgba(67, 80, 108, 0.4)",
    color: "rgba(233, 233, 235, 0.5)",
    border: "1px solid rgba(67, 80, 108, 0.5)",
  },
  btnTiny: {
    backgroundColor: "transparent",
    color: "rgba(233, 233, 235, 0.35)",
    border: "1px solid rgba(67, 80, 108, 0.4)",
    padding: "4px 10px",
    fontSize: "0.7rem",
  },
  spinner: {
    width: "10px",
    height: "10px",
    border: "2px solid rgba(255,255,255,0.3)",
    borderTopColor: "white",
    borderRadius: "50%",
    display: "inline-block",
    animation: "spin 0.7s linear infinite",
  },
};
