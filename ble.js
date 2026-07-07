import { CONFIG } from './config.js';
import { Logger } from './logger.js';

export class BLEManager {
    constructor(role) {
        this.role = role;
        this.device = null;
        this.server = null;
        this.service = null;
        this.rxCharacteristic = null;
        this.txCharacteristic = null;
        this.onDisconnected = null;
        this.onDataReceived = null;
        this.writeQueue = [];
        this.isWriting = false;
        this.stats = { totalWrites: 0, failedWrites: 0, retries: 0, bytesWritten: 0, droppedPackets: 0, notifications: 0, bytesReceived: 0 };
    }

    async connect() {
        try {
            Logger.info(`BLE_${this.role}`, `Requesting device (Prefix: VLC_${this.role}_Analog)...`);
            this.device = await navigator.bluetooth.requestDevice({
                filters: [{ namePrefix: `VLC_${this.role}` }],
                optionalServices: [CONFIG.BLE.SERVICE_UUID]
            });
            this.device.addEventListener('gattserverdisconnected', this.handleDisconnect.bind(this));
            this.server = await this.device.gatt.connect();
            this.service = await this.server.getPrimaryService(CONFIG.BLE.SERVICE_UUID);

            if (this.role === 'TX') {
                this.txCharacteristic = await this.service.getCharacteristic(CONFIG.BLE.TX_CHARACTERISTIC);
                Logger.info(`BLE_${this.role}`, `TX Characteristic acquired.`);
            } else {
                this.rxCharacteristic = await this.service.getCharacteristic(CONFIG.BLE.RX_CHARACTERISTIC);
                await this.rxCharacteristic.startNotifications();
                this.rxCharacteristic.addEventListener('characteristicvaluechanged', this.handleCharValue.bind(this));
                try { this.txCharacteristic = await this.service.getCharacteristic(CONFIG.BLE.TX_CHARACTERISTIC); } catch(e) {}
                Logger.info(`BLE_${this.role}`, `RX Notifications started.`);
            }
            return true;
        } catch (error) {
            Logger.error(`BLE_${this.role}`, `Connection failed: ${error.message}`);
            return false;
        }
    }

    handleDisconnect() {
        Logger.warn(`BLE_${this.role}`, `Device disconnected.`);
        if (this.onDisconnected) this.onDisconnected();
    }

    handleCharValue(event) {
        const value = new Uint8Array(event.target.value.buffer);
        this.stats.notifications++;
        this.stats.bytesReceived += value.length;
        if (this.onDataReceived) this.onDataReceived(value);
    }

    async write(data) {
        if (!this.txCharacteristic) return false;
        if (this.writeQueue.length >= CONFIG.TRANSFER.MAX_WRITE_QUEUE) {
            await this._waitForQueueDrain(Math.floor(CONFIG.TRANSFER.MAX_WRITE_QUEUE / 2));
        }
        this.writeQueue.push(data);
        this._processWriteQueue();
        return true;
    }

    async _waitForQueueDrain(targetSize) {
        while (this.writeQueue.length > targetSize) {
            await new Promise(resolve => setTimeout(resolve, 5));
        }
    }

    async _processWriteQueue() {
        if (this.isWriting || this.writeQueue.length === 0) return;
        this.isWriting = true;
        try {
            const data = this.writeQueue[0];
            let success = false;
            for (let attempt = 0; attempt < CONFIG.TRANSFER.MAX_RETRIES; attempt++) {
                try {
                    await this.txCharacteristic.writeValueWithoutResponse(data);
                    success = true;
                    this.stats.totalWrites++;
                    this.stats.bytesWritten += data.length;
                    break;
                } catch (error) {
                    this.stats.retries++;
                    if (attempt < CONFIG.TRANSFER.MAX_RETRIES - 1) {
                        await new Promise(resolve => setTimeout(resolve, 10));
                    }
                }
            }
            if (success) {
                this.writeQueue.shift();
            } else {
                this.writeQueue.shift();
                this.stats.failedWrites++;
                this.stats.droppedPackets++;
            }
        } finally {
            if (this.writeQueue.length > 0) {
                setTimeout(() => {
                    this.isWriting = false;
                    this._processWriteQueue();
                }, CONFIG.TRANSFER.BASE_TX_DELAY_MS);
            } else {
                this.isWriting = false;
            }
        }
    }

    getStats() { return { ...this.stats, queueLength: this.writeQueue.length }; }
    resetStats() { for (const key in this.stats) this.stats[key] = 0; }
    disconnect() { if (this.device && this.device.gatt.connected) this.device.gatt.disconnect(); }
}
