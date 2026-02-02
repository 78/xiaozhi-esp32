'use client';

import React, { useEffect, useState } from 'react';
import Navbar from '../components/Navbar';
import StatsPanel from '../components/StatsPanel';
import DeviceCard from '../components/DeviceCard';
import AddDeviceModal from '../components/AddDeviceModal';
import OtaModal from '../components/OtaModal';
import { io } from 'socket.io-client';

interface Device {
    id: number;
    mac: string;
    name?: string;
    status: string;
    last_seen?: string;
    current_version?: string;
}

export default function Home() {
    const [devices, setDevices] = useState<Device[]>([]);
    const [loading, setLoading] = useState(true);
    const [socket, setSocket] = useState<any>(null);
    const [connected, setConnected] = useState(false);

    // Modals
    const [isAddOpen, setIsAddOpen] = useState(false);
    const [otaMac, setOtaMac] = useState<string | null>(null);

    useEffect(() => {
        fetchDevices();
        
        // Socket Init
        const socketInstance = io(); // Connects to same host by default, which is our custom server
        setSocket(socketInstance);

        socketInstance.on('connect', () => {
            console.log('Socket Connected');
            setConnected(true);
        });

        socketInstance.on('disconnect', () => {
            console.log('Socket Disconnected');
            setConnected(false);
        });

        socketInstance.on('device_update', (data: any) => {
            console.log('Device Update:', data);
            setDevices(prev => {
                const index = prev.findIndex(d => d.mac === data.mac);
                if (index === -1) {
                    fetchDevices(); // New device? reload
                    return prev;
                }
                const newDevices = [...prev];
                newDevices[index] = { ...newDevices[index], ...data };
                return newDevices;
            });
        });

        return () => {
            socketInstance.disconnect();
        };
    }, []);

    async function fetchDevices() {
        try {
            const res = await fetch('/api/devices');
            const data = await res.json();
            setDevices(data);
            setLoading(false);
        } catch (e) {
            console.error(e);
            setLoading(false);
        }
    }

    async function handleAddDevice(name: string, mac: string) {
        try {
            const res = await fetch('/api/devices', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ name, mac })
            });
            if (res.ok) {
                fetchDevices();
                setIsAddOpen(false);
            } else {
                alert('Failed to add device');
            }
        } catch (e) {
            console.error(e);
            alert('Error adding device');
        }
    }

    async function handleTriggerFota(mac: string, url: string | null, assetsUrl: string | null) {
        await fetch('/api/fota/trigger', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ mac, url, assetsUrl })
        });
    }

    async function handleUpload(file: File, version: string) {
        const formData = new FormData();
        formData.append('file', file);
        formData.append('version', version);
        
        const res = await fetch('/api/upload', {
            method: 'POST',
            body: formData
        });
        
        if (!res.ok) throw new Error('Upload failed');
        const data = await res.json();
        return data.url;
    }

    // Stats
    const total = devices.length;
    const online = devices.filter(d => d.status === 'online').length;
    const outdated = 0; // Logic for outdated could be added later

    return (
        <div className="min-h-screen flex flex-col">
            <Navbar onAddDevice={() => setIsAddOpen(true)} />
            
            <main className="flex-grow max-w-7xl mx-auto px-6 py-8 w-full">
                <StatsPanel total={total} online={online} outdated={outdated} />

                <div className="flex justify-between items-center mb-6">
                    <h2 className="text-xl font-bold text-gray-800">Your Devices</h2>
                    {!connected && <span className="text-red-500 text-sm"><i className="fa-solid fa-wifi"></i> Disconnected</span>}
                </div>

                {loading ? (
                    <div className="col-span-full py-20 text-center text-gray-400">
                        <i className="fa-solid fa-spinner fa-spin text-3xl mb-4"></i>
                        <p>Loading devices...</p>
                    </div>
                ) : devices.length === 0 ? (
                    <div className="col-span-full py-20 text-center text-gray-400 bg-white rounded-2xl border border-gray-100 border-dashed">
                        <i className="fa-regular fa-hard-drive text-4xl mb-4"></i>
                        <p>No devices found. Click "Add Device" to start.</p>
                    </div>
                ) : (
                    <div className="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-3 gap-6">
                        {devices.map(device => (
                            <DeviceCard 
                                key={device.mac} 
                                device={device} 
                                onOta={(mac) => setOtaMac(mac)} 
                                onDelete={() => {}} // TODO: Implement delete
                            />
                        ))}
                    </div>
                )}
            </main>
            
            <footer className="bg-white/50 backdrop-blur border-t border-gray-200 py-6 text-center text-sm text-gray-500">
                <p>&copy; 2026 Xiaozhi Project. All rights reserved.</p>
            </footer>

            <AddDeviceModal 
                isOpen={isAddOpen} 
                onClose={() => setIsAddOpen(false)} 
                onAdd={handleAddDevice} 
            />

            <OtaModal 
                isOpen={!!otaMac} 
                mac={otaMac} 
                onClose={() => setOtaMac(null)} 
                onTrigger={handleTriggerFota} 
                onUpload={handleUpload} 
            />
        </div>
    );
}
