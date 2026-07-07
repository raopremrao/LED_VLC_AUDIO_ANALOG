import { CONFIG } from './config.js';
import { Logger } from './logger.js';
import { Utils } from './utils.js';
import { BLEManager } from './ble.js';

class TransferManager {
    constructor() {
        this.txBle = new BLEManager('TX');
        this.rxBle = new BLEManager('RX');

        this.fileBuffer = null;
        this.isTransmitting = false;

        this.rxRawPCM = new Uint8Array(0);
        this.isRecording = false;

        this.setupUI();
        Logger.info('System', 'Analog VLC Data Link initialized.');
    }

    setupUI() {
        document.getElementById('tab-tx').addEventListener('click', () => this.switchTab('tx'));
        document.getElementById('tab-rx').addEventListener('click', () => this.switchTab('rx'));

        document.getElementById('btn-conn-tx').addEventListener('click', () => this.connectTX());
        document.getElementById('btn-conn-rx').addEventListener('click', () => this.connectRX());

        document.getElementById('file-input').addEventListener('change', () => this.handleFileSelect());
        document.getElementById('btn-stream').addEventListener('click', () => this.startTransmission());

        document.getElementById('btn-start-rx').addEventListener('click', () => this.startRxRecording());
        document.getElementById('btn-stop-rx').addEventListener('click', () => this.stopRxRecording());
        
        document.getElementById('btn-clear-logs').addEventListener('click', () => Logger.clear());
    }

    switchTab(tab) {
        document.querySelectorAll('.tab').forEach(t => t.classList.remove('active'));
        document.querySelectorAll('.panel').forEach(p => p.classList.add('hidden'));
        document.querySelectorAll('.panel').forEach(p => p.classList.remove('active'));
        document.getElementById(`tab-${tab}`).classList.add('active');
        document.getElementById(`panel-${tab}`).classList.remove('hidden');
        document.getElementById(`panel-${tab}`).classList.add('active');
    }

    updateUI(elementId, text) {
        const el = document.getElementById(elementId);
        if (el) el.innerText = text;
    }

    async connectTX() {
        Logger.info('TX', 'Connecting to TX ESP32...');
        this.txBle.onDisconnected = () => {
            this.updateUI('status-tx', 'Status: Disconnected');
            document.getElementById('btn-conn-tx').disabled = false;
        };
        const connected = await this.txBle.connect();
        if (connected) {
            this.updateUI('status-tx', 'Status: Connected');
            document.getElementById('btn-conn-tx').disabled = true;
            if (this.fileBuffer) document.getElementById('btn-stream').disabled = false;
        }
    }

    async connectRX() {
        Logger.info('RX', 'Connecting to RX ESP32...');
        this.rxBle.onDisconnected = () => {
            this.updateUI('status-rx', 'Status: Disconnected');
            document.getElementById('btn-conn-rx').disabled = false;
            document.getElementById('btn-start-rx').disabled = true;
            document.getElementById('btn-stop-rx').disabled = true;
        };
        this.rxBle.onDataReceived = (data) => {
            if (this.isRecording) {
                const newArr = new Uint8Array(this.rxRawPCM.length + data.length);
                newArr.set(this.rxRawPCM);
                newArr.set(data, this.rxRawPCM.length);
                this.rxRawPCM = newArr;
                this.updateUI('rx-buffer-status', `Recorded: ${Utils.formatBytes(this.rxRawPCM.length)}`);
            }
        };
        const connected = await this.rxBle.connect();
        if (connected) {
            this.updateUI('status-rx', 'Status: Connected');
            document.getElementById('btn-conn-rx').disabled = true;
            document.getElementById('btn-start-rx').disabled = false;
        }
    }

    async handleFileSelect() {
        const file = document.getElementById('file-input').files[0];
        if (!file) return;

        Logger.info('TX', `Processing audio: ${file.name}. Converting to 8kHz 8-bit RAW PCM...`);
        document.getElementById('btn-stream').disabled = true;

        try {
            const audioCtx = new (window.AudioContext || window.webkitAudioContext)();
            const arrayBuffer = await file.arrayBuffer();
            const audioBuffer = await audioCtx.decodeAudioData(arrayBuffer);
            
            const offlineCtx = new OfflineAudioContext(1, (audioBuffer.duration * 8000) + 1, 8000);
            const source = offlineCtx.createBufferSource();
            source.buffer = audioBuffer;
            source.connect(offlineCtx.destination);
            source.start();
            
            const renderedBuffer = await offlineCtx.startRendering();
            
            // Convert to 8-bit Unsigned PCM
            const length = renderedBuffer.length;
            const pcmData = new Uint8Array(length);
            const channels = renderedBuffer.getChannelData(0);
            for(let i = 0; i < length; i++) {
                let sample = Math.max(-1, Math.min(1, channels[i]));
                pcmData[i] = Math.round((sample + 1) * 127.5);
            }
            
            this.fileBuffer = pcmData;
            Logger.info('TX', `Audio ready. Size: ${Utils.formatBytes(pcmData.length)}`);
            if (this.txBle.txCharacteristic) {
                document.getElementById('btn-stream').disabled = false;
            }
        } catch (e) {
            Logger.error('TX', `Failed: ${e.message}`);
        }
    }

    async startTransmission() {
        if (!this.fileBuffer || this.isTransmitting) return;
        this.isTransmitting = true;
        document.getElementById('btn-stream').disabled = true;
        this.txBle.resetStats();

        const CHUNK_SIZE = CONFIG.TRANSFER.CHUNK_SIZE;
        const totalChunks = Math.ceil(this.fileBuffer.length / CHUNK_SIZE);
        Logger.info('TX', `Starting analog stream. Chunks: ${totalChunks}`);

        // Send start command
        await this.txBle.txCharacteristic.writeValueWithoutResponse(new TextEncoder().encode("CMD:START"));

        for (let i = 0; i < totalChunks; i++) {
            const chunk = this.fileBuffer.slice(i * CHUNK_SIZE, (i + 1) * CHUNK_SIZE);
            await this.txBle.write(chunk);
            if (i % 20 === 0) {
                this.updateUI('stream-progress', `Streaming: ${((i/totalChunks)*100).toFixed(1)}%`);
                await Utils.sleep(1);
            }
        }

        while (this.txBle.writeQueue.length > 0) {
            await Utils.sleep(50);
        }

        await this.txBle.txCharacteristic.writeValueWithoutResponse(new TextEncoder().encode("CMD:STOP"));
        Logger.info('TX', `Stream Complete.`);
        this.updateUI('stream-progress', `Stream Complete.`);
        this.isTransmitting = false;
        document.getElementById('btn-stream').disabled = false;
    }

    async startRxRecording() {
        this.rxRawPCM = new Uint8Array(0);
        this.isRecording = true;
        document.getElementById('btn-start-rx').disabled = true;
        document.getElementById('btn-stop-rx').disabled = false;
        this.updateUI('rx-buffer-status', `Recording Started...`);
        Logger.info('RX', 'Sending CMD:START to Receiver...');
        await this.rxBle.txCharacteristic.writeValueWithoutResponse(new TextEncoder().encode("CMD:START"));
    }

    async stopRxRecording() {
        this.isRecording = false;
        document.getElementById('btn-stop-rx').disabled = true;
        document.getElementById('btn-start-rx').disabled = false;
        Logger.info('RX', 'Sending CMD:STOP to Receiver...');
        await this.rxBle.txCharacteristic.writeValueWithoutResponse(new TextEncoder().encode("CMD:STOP"));
        
        Logger.info('RX', `Recording complete. Total: ${Utils.formatBytes(this.rxRawPCM.length)}`);
        if (this.rxRawPCM.length > 0) {
            const wavData = this.wrapWav(this.rxRawPCM, 8000);
            const blob = new Blob([wavData], { type: 'audio/wav' });
            Utils.downloadBlob(blob, 'analog_rx_recording.wav');
            Logger.info('RX', 'WAV file downloaded.');
        }
    }

    wrapWav(pcmData, sampleRate) {
        const length = pcmData.length + 44;
        const buffer = new ArrayBuffer(length);
        const view = new DataView(buffer);
        
        const setUint32 = (pos, val) => view.setUint32(pos, val, true);
        const setUint16 = (pos, val) => view.setUint16(pos, val, true);

        setUint32(0, 0x46464952); // "RIFF"
        setUint32(4, length - 8);
        setUint32(8, 0x45564157); // "WAVE"
        setUint32(12, 0x20746d66); // "fmt "
        setUint32(16, 16);
        setUint16(20, 1); // PCM
        setUint16(22, 1); // Channels (Mono)
        setUint32(24, sampleRate);
        setUint32(28, sampleRate); // Byte rate
        setUint16(32, 1); // Block align
        setUint16(34, 8); // Bits per sample
        setUint32(36, 0x61746164); // "data"
        setUint32(40, pcmData.length);
        
        new Uint8Array(buffer).set(pcmData, 44);
        return buffer;
    }
}

window.addEventListener('DOMContentLoaded', () => { window.app = new TransferManager(); });