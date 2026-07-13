export const CONFIG = {
    BLE: {
        SERVICE_UUID: "6e400001-b5a3-f393-e0a9-e50e24dcca9e",
        TX_CHARACTERISTIC: "6e400002-b5a3-f393-e0a9-e50e24dcca9e", // Browser writes here
        RX_CHARACTERISTIC: "6e400003-b5a3-f393-e0a9-e50e24dcca9e", // Browser reads from here
        MAX_MTU: 512,
    },
    TRANSFER: {
        CHUNK_SIZE: 60, // 30ms of audio at 2000Hz 8-bit
        BASE_TX_DELAY_MS: 30, // Pacing delay to match 2000Hz real-time
        MAX_WRITE_QUEUE: 5,
        MAX_RETRIES: 2,
    },
    LOGGING: {
        LEVELS: { DEBUG: 0, INFO: 1, WARN: 2, ERROR: 3 },
        CURRENT_LEVEL: 0,
        MAX_LOG_ENTRIES: 500,
    }
};
