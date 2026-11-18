import fs from "node:fs";
import http from "node:http";
import { SerialPort, ReadlineParser } from "serialport";
import { WebSocketServer } from "ws";
import { GoogleGenAI } from "@google/genai";

const PORT = "/dev/ttyACM0"; // WSL path
const BAUD = 115200;
const MODEL = "gemini-3.5-flash-lite";
const HTTP_PORT = 3000;

const MOODS = new Set(["HAPPY", "CURIOUS", "SAD", "STARTLED"]);
const MOOD_PLAY_MS = { HAPPY: 4500, CURIOUS: 1500, SAD: 4000, STARTLED: 700 };

const SYSTEM =
  "You are Bubo, a small, curious, slightly dramatic owl animatronic. " +
  "Reply in one or two short sentences. Start every reply with exactly one " +
  "mood tag: [HAPPY], [CURIOUS], [SAD], or [STARTLED].";

// --- Fake mode: used automatically when no API key is set ---
const FAKE = !process.env.GEMINI_API_KEY;
const FAKE_REPLIES = [
  "[HAPPY] Hoo hoo! Nice to meet you!",
  "[CURIOUS] Hmm, what's that?",
  "[SAD] Oh no, that's a shame.",
  "[STARTLED] Whoa! You surprised me!",
];

const sleep = (ms) => new Promise((resolve) => setTimeout(resolve, ms));

// --- Serial ---
const serial = new SerialPort({ path: PORT, baudRate: BAUD });
const send = (cmd) => serial.write(cmd + "\n");
serial.on("error", (e) => console.error(`Serial error: ${e.message}`));
serial
  .pipe(new ReadlineParser({ delimiter: "\n" }))
  .on("data", (l) => l.trim() && console.log(`  [board] ${l.trim()}`));

// --- Gemini ---
const client = FAKE ? null : new GoogleGenAI({}); // reads GEMINI_API_KEY

async function getReply(history) {
  if (FAKE) {
    await sleep(800);
    return FAKE_REPLIES[Math.floor(Math.random() * FAKE_REPLIES.length)];
  }
  const resp = await client.models.generateContent({
    model: MODEL,
    contents: history.slice(-10).map((m) => ({
      role: m.role === "assistant" ? "model" : "user",
      parts: [{ text: m.content }],
    })),
    config: {
      systemInstruction: SYSTEM,
      maxOutputTokens: 500,
      abortSignal: AbortSignal.timeout(15000),
    },
  });
  if (!resp.text) {
    throw new Error(`Empty reply (finish reason: ${resp.candidates?.[0]?.finishReason})`);
  }
  return resp.text;
}

function parseReply(text) {
  const m = text.match(/^\s*\[(\w+)\]\s*([\s\S]*)/);
  let mood = m ? m[1].toUpperCase() : "CURIOUS";
  const speech = m ? m[2].trim() : text.trim();
  if (!MOODS.has(mood)) mood = "CURIOUS";
  return { mood, speech };
}

// --- HTTP: serves the page ---
const server = http.createServer((req, res) => {
  res.writeHead(200, { "Content-Type": "text/html; charset=utf-8" });
  res.end(fs.readFileSync(new URL("./index.html", import.meta.url)));
});

// --- WebSocket: events from the browser ---
const wss = new WebSocketServer({ server });
let activeWs = null;

wss.on("connection", (ws) => {
  // Only one tab may control the board at a time
  if (activeWs && activeWs.readyState === 1) activeWs.close();
  activeWs = ws;

  const history = [];
  let pendingMood = "CURIOUS";
  const reply = (obj) => ws.readyState === 1 && ws.send(JSON.stringify(obj));

  console.log(FAKE ? "Browser connected (FAKE mode)" : "Browser connected (Gemini)");
  send("STARTLED");
  setTimeout(() => send("IDLE"), 1000);

  ws.on("message", async (raw) => {
    const msg = JSON.parse(raw);

    if (msg.type === "listening") {
      send("LISTENING");
    } else if (msg.type === "speaking") {
      send("TALKING");
    } else if (msg.type === "spoken") {
      send(pendingMood);
      await sleep(MOOD_PLAY_MS[pendingMood]);
      reply({ type: "ready" });
    } else if (msg.type === "user_text") {
      send("THINKING");
      history.push({ role: "user", content: msg.text });
      try {
        const text = await getReply(history);
        history.push({ role: "assistant", content: text });
        const { mood, speech } = parseReply(text);
        pendingMood = mood;
        console.log(`You: ${msg.text}\nBubo (${mood}): ${speech}`);
        reply({ type: "reply", mood, speech });
      } catch (err) {
        console.error(`API error: ${err.message}`);
        history.pop();
        send("SAD");
        await sleep(MOOD_PLAY_MS.SAD);
        reply({ type: "ready" });
      }
    }
  });

  ws.on("close", () => {
    if (activeWs === ws) {
      activeWs = null;
      send("IDLE");
    }
  });
});

server.listen(HTTP_PORT, () =>
  console.log(`Open http://localhost:${HTTP_PORT} in Chrome or Edge`)
);

// Ctrl+C: leave Bubo limp, then exit cleanly
process.on("SIGINT", () => {
  serial.write("STOPPED\n", () => serial.close(() => process.exit(0)));
});