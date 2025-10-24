require("dotenv").config();
const express = require("express");
const cors = require("cors");
const mqtt = require("mqtt");
const http = require("http");
const { Server } = require("socket.io");

const app = express();
app.use(cors());
app.use(express.json());

const server = http.createServer(app);
const io = new Server(server, {
  cors: { origin: "*" },
});

// MQTT config (from user's HTML)
const MQTT_HOST = process.env.MQTT_HOST || "10.249.240.135";
const MQTT_PORT = process.env.MQTT_PORT || "1883"; // TCP port for backend
const TOPIC_UNLOCK = "door/unlock";
const TOPIC_STATUS = "door/status";
const TOPIC_LIGHT = "light/control";

const mqttUrl = `mqtt://${MQTT_HOST}:${MQTT_PORT}`;
const mqttClient = mqtt.connect(mqttUrl);

mqttClient.on("connect", () => {
  console.log("Connected to MQTT broker:", mqttUrl);
  mqttClient.subscribe(TOPIC_STATUS, (err) => {
    if (err) console.error("Subscribe error", err);
  });
});

mqttClient.on("message", (topic, message) => {
  const payload = message.toString();
  console.log("MQTT message", topic, payload);
  if (topic === TOPIC_STATUS) {
    io.emit("status", payload);
  }
});

app.post("/api/unlock", (req, res) => {
  mqttClient.publish(TOPIC_UNLOCK, "unlock", {}, (err) => {
    if (err) return res.status(500).json({ error: "MQTT publish failed" });
    res.json({ ok: true, message: "unlock sent" });
  });
});

app.post("/api/light", (req, res) => {
  const { action } = req.body;
  if (!action || (action !== "on" && action !== "off")) {
    return res.status(400).json({ error: 'action must be "on" or "off"' });
  }
  mqttClient.publish(TOPIC_LIGHT, action, {}, (err) => {
    if (err) return res.status(500).json({ error: "MQTT publish failed" });
    res.json({ ok: true, message: `light ${action} sent` });
  });
});

// Simple health
app.get("/api/health", (req, res) => res.json({ ok: true }));

// Socket.IO connection logging
io.on("connection", (socket) => {
  console.log("Client connected", socket.id);
  socket.on("disconnect", () => console.log("Client disconnected", socket.id));
});

const PORT = process.env.PORT || 4000;
server.listen(PORT, () => console.log(`Backend listening on ${PORT}`));
