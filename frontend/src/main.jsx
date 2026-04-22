import { StrictMode } from "react"
import { createRoot } from "react-dom/client"
import App from "./App.jsx"

// Reset global
document.body.style.margin  = "0"
document.body.style.padding = "0"
document.body.style.backgroundColor = "#1e2a3a"

// Animación spinner global
const style = document.createElement("style")
style.textContent = `
  @import url('https://fonts.googleapis.com/css2?family=IBM+Plex+Mono:wght@400;600;700&display=swap');
  @keyframes spin { to { transform: rotate(360deg); } }
  * { box-sizing: border-box; }
  ::-webkit-scrollbar { width: 6px; height: 6px; }
  ::-webkit-scrollbar-track { background: rgba(30,42,58,0.5); }
  ::-webkit-scrollbar-thumb { background: rgba(61,97,155,0.5); border-radius: 3px; }
  ::-webkit-scrollbar-thumb:hover { background: rgba(61,97,155,0.8); }
`
document.head.appendChild(style)

createRoot(document.getElementById("root")).render(
  <StrictMode>
    <App />
  </StrictMode>
)