const sqlite3 = require('sqlite3');
const { open } = require('sqlite');

let db;

async function initDB() {
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
    return db;
}

function getDB() {
    if (!db) throw new Error('DB not initialized');
    return db;
}

module.exports = { initDB, getDB };
