import readline from "node:readline/promises";
import { stdin as input, stdout as output } from "node:process";
import { SerialPort, ReadlineParser } from "serialport";
import Anthropic from "@anthropic-ai/sdk";

const PORT = "/dev/ttyACM0"; // WSL path
const BAUD = 115200;
const MODEL = "claude-sonnet-5-5";

const MOODS = new Set(["HAPPY", "CURIOUS", "SAD", "STARTLED"]);
const MOOD_PLAY_MS = { HAPPY: 4500, CURIOUS: 1500, SAD: 4000, STARTLED: 700 };

const SYSTEM =
  "You are Bubo, a small, curious, slightly dramatic owl animatronic. " +
  "Reply in one or two short sentences. Start every reply with exactly one " +
  "mood tag: [HAPPY], [CURIOUS], [SAD], or [STARTLED].";

// --- Fake mode: used automatically when no API key is set ---
const FAKE = !process.env.ANTHROPIC_API_KEY;
const FAKE_REPLIES = [
  "[HAPPY] Hoo hoo! Nice to meet you!",
  "[CURIOUS] Hmm, what's that?",
  "[SAD] Oh no, that's a shame.",
  "[STARTLED] Whoa! You surprised me!",
];

const sleep = (ms) => new Promise((resolve) => setTimeout(resolve, ms));

const port = new SerialPort({ path: PORT, baudRate: BAUD });
const send = (cmd) => port.write(cmd + "\n");

const parser = port.pipe(new ReadlineParser({ delimiter: "\n" }));
parser.on("data", (line) => {
  if (line.trim()) console.log(`  [board] ${line.trim()}`);
});

function parseReply(text) {
  const m = text.match(/^\s*\[(\w+)\]\s*([\s\S]*)/);
  let mood = m ? m[1].toUpperCase() : "CURIOUS";
  const speech = m ? m[2].trim() : text.trim();
  if (!MOODS.has(mood)) mood = "CURIOUS";
  return { mood, speech };
}

async function main() {
  const client = FAKE ? null : new Anthropic(); // only created when a key exists
  const history = [];
  const rl = readline.createInterface({ input, output });

  async function getReply() {
    if (FAKE) {
      await sleep(800); // pretend to think
      return FAKE_REPLIES[Math.floor(Math.random() * FAKE_REPLIES.length)];
    }
    const resp = await client.messages.create({
      model: MODEL,
      max_tokens: 200,
      system: SYSTEM,
      messages: history.slice(-10),
    });
    return resp.content[0].text;
  }

  rl.on("close", () => {
    port.write("IDLE\n", () => port.close(() => process.exit(0)));
  });

  console.log(FAKE ? "No API key found: running in FAKE mode." : "Using Claude.");

  await sleep(2500); // the board may reset when the port opens
  send("STARTLED");
  await sleep(1000);
  send("IDLE");
  console.log("Bubo is awake. Type a message (Ctrl+C to quit).");

  while (true) {
    send("LISTENING");
    const text = (await rl.question("You: ")).trim();
    if (!text) continue;

    send("THINKING");
    history.push({ role: "user", content: text });

    let reply;
    try {
      reply = await getReply();
    } catch (err) {
      console.error(`API error: ${err.message}`);
      history.pop();
      send("SAD");
      await sleep(MOOD_PLAY_MS.SAD);
      continue;
    }

    history.push({ role: "assistant", content: reply });
    const { mood, speech } = parseReply(reply);
    console.log(`Bubo (${mood}): ${speech}`);

    send("TALKING");
    await sleep(Math.max(1500, speech.length * 60)); // stand-in for text-to-speech
    send(mood);
    await sleep(MOOD_PLAY_MS[mood]);
  }
}

main();