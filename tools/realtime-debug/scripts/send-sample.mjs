import net from 'node:net';
import { randomUUID } from 'node:crypto';

const host = process.argv[2] || '127.0.0.1';
const port = Number(process.argv[3] || 7319);
if (!Number.isInteger(port) || port < 1 || port > 65535) {
  throw new Error('port must be between 1 and 65535');
}

const bootId = randomUUID();
const traceSessionId = randomUUID();
let sequence = 0;
const started = process.hrtime.bigint();

function frame(kind, metadata, payload = Buffer.alloc(0)) {
  const meta = Buffer.from(JSON.stringify(metadata));
  const bytes = Buffer.alloc(12 + meta.length + payload.length);
  bytes.writeUInt32BE(bytes.length - 4, 0);
  bytes.writeUInt16BE(1, 4);
  bytes.writeUInt16BE(kind, 6);
  bytes.writeUInt32BE(meta.length, 8);
  meta.copy(bytes, 12);
  payload.copy(bytes, 12 + meta.length);
  return bytes;
}

function metadata(extra = {}) {
  return {
    boot_id: bootId,
    trace_session_id: traceSessionId,
    seq: String(++sequence),
    mono_us: Number((process.hrtime.bigint() - started) / 1000n),
    ...extra,
  };
}

const pcm = Buffer.alloc(16000 * 2);
for (let sample = 0; sample < pcm.length / 2; sample++) {
  pcm.writeInt16LE(Math.round(7000 * Math.sin(2 * Math.PI * 440 * sample / 16000)), sample * 2);
}

const message = Buffer.from(JSON.stringify({ type: 'sample', text: 'Realtime debug sample' }));
const output = [
  frame(1, { boot_id: bootId, device_id: 'sample-device', firmware_version: 'manual-sample' }),
  frame(2, metadata({ mode: 'manual' })),
  frame(4, metadata({ sample_rate: 16000, channels: 1, bits_per_sample: 16, sample_format: 'signed_int', byte_order: 'little' }), pcm),
  frame(5, metadata({ message_id: 'sample-message', content_type: 'application/json', event_type: 'sample' }), message),
  frame(6, metadata({ message_id: 'sample-message', detail: 'send call succeeded' })),
  frame(7, metadata({ content_type: 'application/json', event_type: 'sample.reply' }), Buffer.from(JSON.stringify({ type: 'sample.reply', text: 'Hello' }))),
  frame(3, metadata({ detail: 'sample finished' })),
];

const socket = net.createConnection({ host, port });
socket.on('error', error => { throw error; });
socket.on('connect', () => {
  for (const bytes of output) socket.write(bytes);
  socket.end();
});
socket.on('close', () => console.log(`sample trace: ${traceSessionId}`));
