const { S3Client, PutObjectCommand, GetObjectCommand, ListObjectsV2Command, DeleteObjectCommand } = require('@aws-sdk/client-s3');
const { v4: uuidv4 } = require('uuid');

// In-Memory Cache to avoid hitting S3 constantly
let USERS_CACHE = [];
let DEVICE_MAP_CACHE = []; // [{ deviceId: '...', ownerId: '...', name: '...' }]

class DatabaseManager {
    constructor() {
        this.s3Client = null;
        this.bucket = process.env.S3_BUCKET_NAME;
        this.enabled = false;

        if (process.env.AWS_ACCESS_KEY_ID && process.env.AWS_SECRET_ACCESS_KEY && this.bucket) {
            this.s3Client = new S3Client({
                region: process.env.AWS_REGION || 'us-east-1',
                credentials: {
                    accessKeyId: process.env.AWS_ACCESS_KEY_ID,
                    secretAccessKey: process.env.AWS_SECRET_ACCESS_KEY
                }
            });
            this.enabled = true;
            console.log('[DB] S3 Storage Enabled');
        } else {
            console.warn('[DB] S3 Credentials missing. Data will be lost on restart.');
        }
    }

    async init() {
        if (!this.enabled) return;
        
        // Load Users
        try {
            const data = await this._readJSON('users.json');
            USERS_CACHE = data || [];
            console.log(`[DB] Loaded ${USERS_CACHE.length} users`);
        } catch (e) {
            console.log('[DB] No users.json found, starting fresh.');
            USERS_CACHE = [];
        }

        // Load Device Map
        try {
            const data = await this._readJSON('device_map.json');
            DEVICE_MAP_CACHE = data || [];
            console.log(`[DB] Loaded ${DEVICE_MAP_CACHE.length} devices`);
        } catch (e) {
            console.log('[DB] No device_map.json found, starting fresh.');
            DEVICE_MAP_CACHE = [];
        }
    }

    // --- User Management ---
    
    async createUser(username, passwordHash) {
        if (USERS_CACHE.find(u => u.username === username)) {
            throw new Error('User already exists');
        }
        const newUser = { id: uuidv4(), username, passwordHash, created: Date.now() };
        USERS_CACHE.push(newUser);
        await this._saveJSON('users.json', USERS_CACHE);
        return newUser;
    }

    findUser(username) {
        return USERS_CACHE.find(u => u.username === username);
    }
    
    findUserById(id) {
        return USERS_CACHE.find(u => u.id === id);
    }

    // --- Device Management ---

    async linkDevice(userId, deviceId, name) {
        const normalizedId = deviceId.trim().toLowerCase();
        
        // Check if already linked to SOMEONE ELSE
        const existing = DEVICE_MAP_CACHE.find(d => d.deviceId === normalizedId);
        if (existing && existing.ownerId !== userId) {
            throw new Error('Device is already owned by another user.');
        }

        if (existing) {
            // Update name
            existing.name = name || existing.name;
        } else {
            // Create new link
            DEVICE_MAP_CACHE.push({ 
                deviceId: normalizedId, 
                ownerId: userId, 
                name: name || `Device ${normalizedId}`,
                linkedAt: Date.now()
            });
        }
        
        await this._saveJSON('device_map.json', DEVICE_MAP_CACHE);
        return normalizedId;
    }

    getDevicesForUser(userId) {
        return DEVICE_MAP_CACHE.filter(d => d.ownerId === userId);
    }

    getAllDevices() {
        return DEVICE_MAP_CACHE;
    }

    async removeDevice(userId, deviceId) {
        const normalizedId = deviceId.trim().toLowerCase();
        const initialLen = DEVICE_MAP_CACHE.length;
        
        // Custom logic: If userId is "ADMIN", allow removal of any device? 
        // For now, keep strict ownership for removal, or handle in index.js
        DEVICE_MAP_CACHE = DEVICE_MAP_CACHE.filter(d => !(d.deviceId === normalizedId && d.ownerId === userId));
        
        if (DEVICE_MAP_CACHE.length !== initialLen) {
            await this._saveJSON('device_map.json', DEVICE_MAP_CACHE);
            // Optional: We do NOT delete the quiz file immediately on unlink, 
            // incase they want to re-link. Or we could. Let's keep data simple for now.
            return true;
        }
        return false;
    }

    // --- Quiz Data ---

    async saveQuiz(deviceId, questions) {
        const key = `quizzes/${deviceId.toLowerCase()}.json`;
        await this._saveJSON(key, questions);
    }

    async loadQuiz(deviceId) {
        const key = `quizzes/${deviceId.toLowerCase()}.json`;
        return await this._readJSON(key);
    }

    // --- History ---

    async addHistory(deviceId, sessionData) {
        const key = `history/${deviceId.toLowerCase()}.json`;
        let history = [];
        try {
            history = await this._readJSON(key) || [];
        } catch (e) {
            // New history file
        }
        
        // Keep last 50 sessions
        history.unshift(sessionData);
        if (history.length > 50) history = history.slice(0, 50);

        await this._saveJSON(key, history);
    }

    async getHistory(deviceId) {
        const key = `history/${deviceId.toLowerCase()}.json`;
        try {
            return await this._readJSON(key) || [];
        } catch (e) {
            return [];
        }
    }

    // --- S3 Helpers ---

    async _readJSON(key) {
        if (!this.enabled) return null;
        try {
            const command = new GetObjectCommand({ Bucket: this.bucket, Key: key });
            const response = await this.s3Client.send(command);
            const str = await response.Body.transformToString();
            return JSON.parse(str);
        } catch (e) {
            if (e.name !== 'NoSuchKey') console.error(`[DB] Read Error ${key}:`, e);
            throw e; 
        }
    }

    async _saveJSON(key, data) {
        if (!this.enabled) return;
        try {
            const command = new PutObjectCommand({ 
                Bucket: this.bucket, 
                Key: key, 
                Body: JSON.stringify(data), 
                ContentType: 'application/json' 
            });
            await this.s3Client.send(command);
        } catch (e) {
            console.error(`[DB] Save Error ${key}:`, e);
            throw e;
        }
    }
}

module.exports = new DatabaseManager();
