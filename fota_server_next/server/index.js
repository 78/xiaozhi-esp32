require('dotenv').config();
const express = require('express');
const http = require('http');
const { Server } = require("socket.io");
const next = require('next');
const path = require('path');
const fs = require('fs');
const multer = require('multer');
const multerS3 = require('multer-s3');
const AWS = require('aws-sdk');

const { initDB, getDB } = require('./db');
const { initMQTT, getMQTT } = require('./mqtt');

const dev = process.env.NODE_ENV !== 'production';
const app = next({ dev });
const handle = app.getRequestHandler();

const PORT = process.env.PORT || 3000;

// AWS Config
const AWS_CONFIG = {
    accessKeyId: process.env.AWS_ACCESS_KEY_ID,
    secretAccessKey: process.env.AWS_SECRET_ACCESS_KEY,
    region: process.env.AWS_REGION || 'ap-southeast-1'
};
AWS.config.update(AWS_CONFIG);
const s3 = new AWS.S3();
const BUCKET_NAME = process.env.AWS_BUCKET_NAME || 'xiaozhi-fota-firmware';
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
            const dir = path.join(process.cwd(), 'public/uploads'); 
            if (!fs.existsSync(dir)){
                fs.mkdirSync(dir, { recursive: true });
            }
            cb(null, dir);
        },
        filename: (req, file, cb) => {
            cb(null, Date.now() + '-' + file.originalname);
        }
    })
});

app.prepare().then(async () => {
    const server = express();
    const httpServer = http.createServer(server);
    const io = new Server(httpServer);

    // Init DB
    try {
        await initDB();
    } catch (err) {
        console.error("Failed to init DB:", err);
    }

    // Init MQTT
    const mqttClient = initMQTT(io);

    server.use(express.json());

    // API Routes
    server.get('/api/devices', async (req, res) => {
        try {
            const db = getDB();
            const devices = await db.all('SELECT * FROM devices');
            res.json(devices);
        } catch (e) {
            res.status(500).json({ error: e.message });
        }
    });

    server.post('/api/devices', async (req, res) => {
        const { mac, name } = req.body;
        try {
            const db = getDB();
            await db.run('INSERT INTO devices (mac, name, status) VALUES (?, ?, ?)', [mac, name, 'offline']);
            res.json({ success: true });
        } catch (e) {
            res.status(400).json({ error: 'Device already exists or invalid data' });
        }
    });

    server.post('/api/upload', upload.single('file'), async (req, res) => {
         if (!req.file) return res.status(400).send('No file uploaded.');
         
         let fileUrl = '';
         if (USE_S3) {
             fileUrl = req.file.location;
         } else {
             // Local file url
             fileUrl = `/uploads/${req.file.filename}`;
         }
     
         const version = req.body.version || 'unknown';
         const db = getDB();
         
         await db.run('INSERT INTO firmwares (filename, version, url) VALUES (?, ?, ?)', 
             [req.file.originalname, version, fileUrl]);
     
         res.json({ success: true, url: fileUrl, version });
    });

    server.post('/api/fota/trigger', async (req, res) => {
        const { mac, url, assetsUrl } = req.body;
        if (!mac || (!url && !assetsUrl)) return res.status(400).json({ error: 'Missing mac or at least one url' });
    
        console.log(`Triggering FOTA for ${mac}`);
        if (url) {
            const payload = JSON.stringify({ type: "ota_url", url: url });
            mqttClient.publish(`device/${mac}/command`, payload);
        }
        if (assetsUrl) {
            const payload = JSON.stringify({ type: "assets_url", url: assetsUrl });
            setTimeout(() => {
                mqttClient.publish(`device/${mac}/command`, payload);
            }, 500); 
        }
    
        res.json({ success: true, message: 'FOTA command(s) sent' });
    });

    // Default Next.js Handler
    server.all('*splat', (req, res) => {
        return handle(req, res);
    });

    httpServer.listen(PORT, (err) => {
        if (err) throw err;
        console.log(`> Ready on http://localhost:${PORT}`);
    });
});
