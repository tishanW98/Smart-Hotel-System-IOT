import React, { useEffect, useState } from "react";
import io from "socket.io-client";
import axios from "axios";

const BACKEND = import.meta.env.VITE_BACKEND_URL || "http://localhost:4000";

export default function App() {
  const [status, setStatus] = useState("Ready");

  useEffect(() => {
    const socket = io(BACKEND);
    socket.on("connect", () => console.log("socket connected"));
    socket.on("status", (s) => setStatus(s));
    return () => socket.disconnect();
  }, []);

  async function unlock() {
    setStatus("Sending unlock...");
    try {
      await axios.post(`${BACKEND}/api/unlock`);
      setStatus("Unlock sent");
    } catch (err) {
      setStatus("Error sending unlock");
    }
  }

  async function light(action) {
    setStatus(`Lights ${action.toUpperCase()}...`);
    try {
      await axios.post(`${BACKEND}/api/light`, { action });
      setStatus(`Lights ${action.toUpperCase()}`);
    } catch (err) {
      setStatus("Error controlling lights");
    }
  }

  return (
    <div className="container">
      <h1>Remote Door Unlock</h1>
      <button onClick={unlock}>Unlock Door</button>

      <h2>LED Strip Control</h2>
      <button onClick={() => light("on")}>Light ON</button>
      <button onClick={() => light("off")}>Light OFF</button>

      <div id="status">Status: {status}</div>
    </div>
  );
}
