const express = require('express');
const bodyParser = require('body-parser');
const cors = require('cors');
const { v4: uuidv4 } = require('uuid');
const multer = require('multer');
const path = require('path');
const bcrypt = require('bcryptjs');
const jwt = require('jsonwebtoken');

// Logic encapsulated in DB Manager
const db = require('./db_manager');

// Default questions if none uploaded
const defaultQuestions = require('./questions.json');

const app = express();
const PORT = process.env.PORT || 3000;
const JWT_SECRET = process.env.JWT_SECRET || 'xiaozhi-secret-key-change-me';

// Middleware
app.use(cors());
app.use(bodyParser.json({ limit: '10mb' }));
app.use(express.static('public')); 

// Helper to log
function log(msg) {
    console.log(`${new Date().toISOString()} ${msg}`);
}

app.use((req, res, next) => {
    log(`[${req.method}] ${req.url}`);
    next();
});

// Setup Multer
const upload = multer({ storage: multer.memoryStorage() });

// Active Sessions (In-Memory for speed)
const sessions = new Map();

// --- Auth Middleware ---
function authenticateToken(req, res, next) {
    const authHeader = req.headers['authorization'];
    const token = authHeader && authHeader.split(' ')[1]; // Bearer TOKEN

    if (!token) return res.sendStatus(401);

    jwt.verify(token, JWT_SECRET, (err, user) => {
        if (err) return res.sendStatus(403);
        req.user = user;
        next();
    });
}

// --- Auth Routes ---

app.post('/api/auth/register', async (req, res) => {
    const { username, password } = req.body;
    if (!username || !password) return res.status(400).send("Missing fields");

    try {
        const hashedPassword = await bcrypt.hash(password, 10);
        const user = await db.createUser(username, hashedPassword);
        res.json({ message: "User created", userId: user.id });
    } catch (e) {
        res.status(400).json({ error: e.message });
    }
});

app.post('/api/auth/login', async (req, res) => {
    const { username, password } = req.body;
    const user = db.findUser(username);
    
    if (!user) return res.status(400).json({ error: "User not found" });

    // Check if this user is the Admin
    // Default admin name is 'admin' if not set in ENV, but better to enforce ENV for security
    const adminUser = process.env.ADMIN_USERNAME || 'admin';
    const isAdmin = (user.username === adminUser);

    if (await bcrypt.compare(password, user.passwordHash)) {
        const token = jwt.sign({ 
            id: user.id, 
            username: user.username,
            isAdmin: isAdmin 
        }, JWT_SECRET, { expiresIn: '7d' });
        
        res.json({ token, username: user.username, isAdmin });
    } else {
        res.status(403).json({ error: "Invalid password" });
    }
});

// --- Management Routes (Protected) ---

// 1. Get My Devices (Or ALL if Admin)
app.get('/api/devices', authenticateToken, (req, res) => {
    if (req.user.isAdmin) {
        // Admin sees ALL devices
        const allDevices = db.getAllDevices();
        // Enrich with user info if needed, or return raw
        res.json(allDevices.map(d => ({
            ...d,
            name: `${d.name} [Owner: ${db.findUserById(d.ownerId)?.username || 'Unknown'}]` // Append Owner for visibility
        })));
    } else {
        // Normal user
        const devices = db.getDevicesForUser(req.user.id);
        res.json(devices);
    }
});

// 2. Link a Device
app.post('/api/devices', authenticateToken, async (req, res) => {
    const { deviceId, name } = req.body;
    if (!deviceId) return res.status(400).json({ error: "Device ID required" });

    try {
        const finalId = await db.linkDevice(req.user.id, deviceId, name);
        res.json({ message: "Device Linked", deviceId: finalId });
    } catch (e) {
        res.status(400).json({ error: e.message });
    }
});

// 3. Delete/Unlink Device
app.delete('/api/devices/:deviceId', authenticateToken, async (req, res) => {
    await db.removeDevice(req.user.id, req.params.deviceId);
    res.json({ message: "Device Removed" });
});

// 4. Get Quiz for Device
app.get('/api/quiz/:deviceId', authenticateToken, async (req, res) => {
    const questions = await db.loadQuiz(req.params.deviceId);
    if (!questions) return res.json(defaultQuestions); // Return default if custom doesn't exist
    res.json(questions);
});

// 5. Save Quiz (JSON format from Dashboard)
app.post('/api/quiz/json', authenticateToken, async (req, res) => {
    const { deviceId, questions } = req.body;
    
    // Verify ownership
    const devices = db.getDevicesForUser(req.user.id);
    if (!devices.find(d => d.deviceId === deviceId.toLowerCase())) {
        return res.status(403).json({ error: "You do not own this device" });
    }

    await db.saveQuiz(deviceId, questions);
    res.json({ message: "Quiz Saved", count: questions.length });
});

// 6. Get History
app.get('/api/history/:deviceId', authenticateToken, async (req, res) => {
     // Verify ownership
     const devices = db.getDevicesForUser(req.user.id);
     if (!devices.find(d => d.deviceId === req.params.deviceId.toLowerCase())) {
         return res.status(403).json({ error: "You do not own this device" });
     }
     
     const history = await db.getHistory(req.params.deviceId);
     res.json(history);
});

// --- Public Device API (Used by ESP32) ---

// Start Quiz (Public, but logic checks if device has custom quiz)
app.post('/api/quiz/start', async (req, res) => {
    const deviceId = req.body.deviceId ? req.body.deviceId.trim().toLowerCase() : null;
    log(`Start request from device: ${deviceId || 'Unknown'}`);

    let questionsToUse = defaultQuestions;
    
    if (deviceId) {
        // Try to load custom quiz from DB (Cached or S3)
        const custom = await db.loadQuiz(deviceId);
        if (custom) {
            questionsToUse = custom;
            log(`Using custom quiz for ${deviceId}`);
        } else {
            log(`Using default quiz`);
        }
    }

    const sessionId = uuidv4();
    sessions.set(sessionId, {
        score: 0,
        currentQuestionIndex: 0,
        questions: questionsToUse,
        originalDeviceId: deviceId,
        created: Date.now(),
        answers: []
    });

    // Cleanup old sessions
    if (sessions.size > 200) {
        const now = Date.now();
        for (const [id, s] of sessions) {
            if (now - s.created > 3600000) sessions.delete(id);
        }
    }

    const firstQuestion = questionsToUse[0];
    res.json({
        sessionId: sessionId,
        total: questionsToUse.length,
        question: {
            index: 0,
            id: firstQuestion.id,
            text: firstQuestion.question,
            options: firstQuestion.options
        }
    });
});

// Submit Answer
app.post('/api/quiz/answer', async (req, res) => {
    const { sessionId, answer } = req.body;

    if (!sessions.has(sessionId)) {
        return res.status(404).json({ error: "Session not found or expired" });
    }

    const session = sessions.get(sessionId);
    const questionsList = session.questions;
    const currentQ = questionsList[session.currentQuestionIndex];

    const isCorrect = answer.toUpperCase() === currentQ.correct;
    if (isCorrect) session.score++;

    session.answers.push({
        questionId: currentQ.id,
        userAnswer: answer,
        isCorrect: isCorrect
    });

    session.currentQuestionIndex++;
    
    const response = {
        correct: isCorrect,
        correctOption: currentQ.correct,
        currentScore: session.score
    };

    if (session.currentQuestionIndex < questionsList.length) {
        const nextQ = questionsList[session.currentQuestionIndex];
        response.nextQuestion = {
            index: session.currentQuestionIndex,
            id: nextQ.id,
            text: nextQ.question,
            options: nextQ.options
        };
        response.finished = false;
    } else {
        response.finished = true;
        response.finalScore = session.score;
        response.totalQuestions = questionsList.length;
        
        // Save history if device ID is known
        if (session.originalDeviceId) {
            await db.addHistory(session.originalDeviceId, {
                date: new Date().toISOString(),
                score: session.score,
                total: questionsList.length,
                durationSeconds: Math.floor((Date.now() - session.created) / 1000)
            });
        }
        
        sessions.delete(sessionId);
    }

    res.json(response);
});

// Health check / Root
// app.get('/', ...) is not needed because express.static('public') serves index.html by default.

// Init DB then Start Server
db.init().then(() => {
    const server = app.listen(PORT, () => {
        console.log(`Server is running on port ${PORT}`);
        server.keepAliveTimeout = 61000;
        server.headersTimeout = 65000;
    });
});
