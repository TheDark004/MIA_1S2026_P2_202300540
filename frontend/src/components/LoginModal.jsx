import { useState } from "react";

export default function LoginModal({ onClose, onLogin, loginError }) {
  const [partitionId, setPartitionId] = useState("");
  const [username, setUsername] = useState("");
  const [password, setPassword] = useState("");

  const handleSubmit = async (e) => {
    e.preventDefault();
    if (!partitionId.trim() || !username.trim() || !password.trim()) {
      return;
    }
    await onLogin({ partitionId, username, password });
  };

  return (
    <div style={styles.overlay} onClick={(e) => e.target === e.currentTarget && onClose()}>
      <div style={styles.modal}>
        <div style={styles.titleSection}>
          <span style={styles.title}>Iniciar Sesión</span>
          <button style={styles.closeBtn} onClick={onClose}>✕</button>
        </div>

        <form style={styles.form} onSubmit={handleSubmit}>
          <label style={styles.label}>
            ID Partición
            <input
              style={styles.input}
              value={partitionId}
              onChange={(e) => setPartitionId(e.target.value)}
              placeholder="401A"
              autoFocus
            />
          </label>
          <label style={styles.label}>
            Usuario
            <input
              style={styles.input}
              value={username}
              onChange={(e) => setUsername(e.target.value)}
              placeholder="root"
            />
          </label>
          <label style={styles.label}>
            Contraseña
            <input
              style={styles.input}
              type="password"
              value={password}
              onChange={(e) => setPassword(e.target.value)}
              placeholder="123"
            />
          </label>

          {loginError && <div style={styles.error}>{loginError}</div>}

          <button type="submit" style={styles.loginBtn}>
            Iniciar Sesión
          </button>
        </form>
      </div>
    </div>
  );
}

const styles = {
  overlay: {
    position: "fixed",
    top: 0,
    left: 0,
    width: "100vw",
    height: "100vh",
    backgroundColor: "rgba(0,0,0,0.65)",
    display: "flex",
    alignItems: "center",
    justifyContent: "center",
    zIndex: 1100,
  },
  modal: {
    width: "360px",
    backgroundColor: "#1f2937",
    borderRadius: "16px",
    padding: "24px",
    boxShadow: "0 20px 60px rgba(0,0,0,0.4)",
    color: "#f8fafc",
  },
  titleSection: {
    display: "flex",
    justifyContent: "space-between",
    alignItems: "center",
    marginBottom: "18px",
  },
  title: {
    fontSize: "1.1rem",
    fontWeight: 700,
  },
  closeBtn: {
    background: "none",
    border: "none",
    color: "#f8fafc",
    fontSize: "1.1rem",
    cursor: "pointer",
  },
  form: {
    display: "flex",
    flexDirection: "column",
    gap: "14px",
  },
  label: {
    display: "flex",
    flexDirection: "column",
    gap: "6px",
    fontSize: "0.85rem",
    color: "rgba(241, 245, 249, 0.85)",
  },
  input: {
    width: "100%",
    padding: "12px 14px",
    borderRadius: "10px",
    border: "1px solid rgba(255,255,255,0.15)",
    backgroundColor: "rgba(15, 23, 42, 0.95)",
    color: "#f8fafc",
    outline: "none",
  },
  loginBtn: {
    marginTop: "8px",
    padding: "12px 16px",
    borderRadius: "12px",
    border: "none",
    backgroundColor: "#2563eb",
    color: "#fff",
    cursor: "pointer",
    fontWeight: 700,
  },
  error: {
    padding: "10px 12px",
    borderRadius: "10px",
    backgroundColor: "#b91c1c",
    color: "#fff",
    fontSize: "0.85rem",
  },
};
