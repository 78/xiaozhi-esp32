const mqtt = require('mqtt');
const { getDB } = require('./db');

const MQTT_BROKER = process.env.MQTT_BROKER || 'mqtt://broker.hivemq.com';
const MQTT_USER = process.env.MQTT_USER || '';
const MQTT_PASS = process.env.MQTT_PASS || '';

let mqttClient;

function initMQTT(io) {
    mqttClient = mqtt.connect(MQTT_BROKER, {
        username: MQTT_USER,
        password: MQTT_PASS
    });

    mqttClient.on('connect', () => {
        console.log('Connected to MQTT Broker');
        mqttClient.subscribe('device/+/status');
        mqttClient.subscribe('device/+/version');
    });

    mqttClient.on('message', async (topic, message) => {
        const parts = topic.split('/');
        if (parts.length < 3) return;
        const mac = parts[1];
        const type = parts[2];

        const msgStr = message.toString();
        
        try {
            const db = getDB();
            
            // Auto-discovery: Check if device exists, if not insert it
            const device = await db.get('SELECT id FROM devices WHERE mac = ?', [mac]);
            if (!device) {
                console.log(`New device discovered: ${mac}`);
                await db.run('INSERT INTO devices (mac, name, status, last_seen) VALUES (?, ?, ?, CURRENT_TIMESTAMP)', 
                    [mac, `Device-${mac.slice(-5)}`, 'online']);
                // Notify frontend of new device immediately
                const newDevice = await db.get('SELECT * FROM devices WHERE mac = ?', [mac]);
                io.emit('device_update', newDevice);
            }

            if (type === 'status') {
                const status = msgStr;
                await db.run('UPDATE devices SET status = ?, last_seen = CURRENT_TIMESTAMP WHERE mac = ?', [status, mac]);
                io.emit('device_update', { mac, status });
            } else if (type === 'version') {
                await db.run('UPDATE devices SET current_version = ? WHERE mac = ?', [msgStr, mac]);
                io.emit('device_update', { mac, current_version: msgStr });
            }
        } catch (err) {
            console.error("MQTT Message Error:", err);
        }
    });

    return mqttClient;
}

function getMQTT() {
    return mqttClient;
}

module.exports = { initMQTT, getMQTT };
