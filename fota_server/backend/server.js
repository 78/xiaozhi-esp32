require('dotenv').config();
const express = require('express');
const http = require('http');
const cors = require('cors');
const { Server } = require("socket.io");
const mqtt = require('mqtt');
const sqlite3 = require('sqlite3');
const { open } = require('sqlite');
const AWS = require('aws-sdk');
const multer = require('multer');
const multerS3 = require('multer-s3');
const path = require('path');
const fs = require('fs');

// --- Configuration ---
const PORT = process.env.PORT || 4001;
const MQTT_BROKER = process.env.MQTT_BROKER || 'mqtt://broker.hivemq.com'; // Change to your broker
const MQTT_USER = process.env.MQTT_USER || '';
const MQTT_PASS = process.env.MQTT_PASS || '';

// AWS Config
AWS.config.update({
    accessKeyId: process.env.AWS_ACCESS_KEY_ID,
    secretAccessKey: process.env.AWS_SECRET_ACCESS_KEY,
    region: process.env.AWS_REGION || 'ap-southeast-1'
});

const s3 = new AWS.S3();
const BUCKET_NAME = process.env.AWS_BUCKET_NAME || 'xiaozhi-fota-firmware';

// --- App Setup ---
const app = express();
app.use(cors());
app.use(express.json());
const server = http.createServer(app);
const io = new Server(server, {
    cors: { origin: "*" }
});

// --- Database ---
let db;
(async () => {
    db = await open({
        filename: './fota.db',
        driver: sqlite3.Database
    });
    await db.exec(`
        CREATE TABLE IF NOT EXISTS devices (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            mac TEXT UNIQUE NOT NULL,
            name TEXT,
            status TEXT DEFAULT 'offline',
            last_seen DATETIME,
            current_version TEXT
        );
        CREATE TABLE IF NOT EXISTS firmwares (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            filename TEXT,
            version TEXT,
            url TEXT,
            created_at DATETIME DEFAULT CURRENT_TIMESTAMP
        );
    `);
    console.log('Database initialized');
})();

// --- MQTT Client ---
const mqttClient = mqtt.connect(MQTT_BROKER, {
    username: MQTT_USER,
    password: MQTT_PASS
});

mqttClient.on('connect', () => {
    console.log('Connected to MQTT Broker');
    // Subscribe to all device status topics
    // Convention: device/{mac}/status
    mqttClient.subscribe('device/+/status');
    mqttClient.subscribe('device/+/version');
});

mqttClient.on('message', async (topic, message) => {
    // Expected topic: device/{mac}/status
    const parts = topic.split('/');
    if (parts.length < 3) return;
    const mac = parts[1];
    const type = parts[2];

    const msgStr = message.toString();

    if (type === 'status') {
        const status = msgStr; // 'online' or 'offline'
        await db.run('UPDATE devices SET status = ?, last_seen = CURRENT_TIMESTAMP WHERE mac = ?', [status, mac]);
        io.emit('device_update', { mac, status });
    } else if (type === 'version') {
        await db.run('UPDATE devices SET current_version = ? WHERE mac = ?', [msgStr, mac]);
        io.emit('device_update', { mac, current_version: msgStr });
    }
});

// --- Upload Middleware ---
// Strategy: Check if using S3 or Local based on env
const USE_S3 = process.env.USE_S3 === 'true';

const upload = multer({
    storage: USE_S3 ? multerS3({
        s3: s3,
        bucket: BUCKET_NAME,
        acl: 'public-read',
        key: function (req, file, cb) {
            cb(null, 'firmware/' + Date.now().toString() + '-' + file.originalname);
        }
    }) : multer.diskStorage({
        destination: (req, file, cb) => {
            const dir = './uploads';
            if (!fs.existsSync(dir)){
                fs.mkdirSync(dir);
            }
            cb(null, dir);
        },
        filename: (req, file, cb) => {
            cb(null, Date.now() + '-' + file.originalname);
        }
    })
});

// --- API Endpoints ---

// Get all devices
app.get('/api/devices', async (req, res) => {
    try {
        const devices = await db.all('SELECT * FROM devices');
        res.json(devices);
    } catch (e) {
        res.status(500).json({ error: e.message });
    }
});

// Add device
app.post('/api/devices', async (req, res) => {
    const { mac, name } = req.body;
    try {
        await db.run('INSERT INTO devices (mac, name, status) VALUES (?, ?, ?)', [mac, name, 'offline']);
        res.json({ success: true });
    } catch (e) {
        // Assume failure is duplicate MAC
        res.status(400).json({ error: 'Device already exists or invalid data' });
    }
});

// Upload Firmware (S3 or Local)
app.post('/api/upload', upload.single('file'), async (req, res) => {
    if (!req.file) return res.status(400).send('No file uploaded.');
    
    let fileUrl = '';
    if (USE_S3) {
        fileUrl = req.file.location;
    } else {
        // Local file url (assuming static serve)
        fileUrl = `http://localhost:${PORT}/uploads/${req.file.filename}`;
    }

    const version = req.body.version || 'unknown';
    
    await db.run('INSERT INTO firmwares (filename, version, url) VALUES (?, ?, ?)', 
        [req.file.originalname, version, fileUrl]);

    res.json({ success: true, url: fileUrl, version });
});

// Trigger FOTA
app.post('/api/fota/trigger', async (req, res) => {
    const { mac, url, assetsUrl } = req.body;
    
    if (!mac || (!url && !assetsUrl)) return res.status(400).json({ error: 'Missing mac or at least one url (firmware or assets)' });

    console.log(`Triggering FOTA for ${mac}`);

    if (url) {
        console.log(`- Firmware URL: ${url}`);
        const payload = JSON.stringify({
            type: "ota_url",
            url: url
        });
        mqttClient.publish(`device/${mac}/command`, payload);
    }

    if (assetsUrl) {
        console.log(`- Assets URL: ${assetsUrl}`);
        const payload = JSON.stringify({
            type: "assets_url",
            url: assetsUrl
        });
        // Small delay to ensure order if sending both
        setTimeout(() => {
            mqttClient.publish(`device/${mac}/command`, payload);
        }, 500); 
    }

    res.json({ success: true, message: 'FOTA command(s) sent' });
});

// Serve uploads for local test
app.use('/uploads', express.static('uploads'));

// Serve Frontend (Dashboard)
app.use(express.static(path.join(__dirname, '../frontend')));

app.get('/', (req, res) => {
    res.sendFile(path.join(__dirname, '../frontend/dashboard.html'));
});

server.listen(PORT, () => {
    console.log(`FOTA Server running on port ${PORT}`);
});
