#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

#define SERVICE_UUID           "6e400001-b5a3-f393-e0a9-e50e24dcca9e"
#define CHARACTERISTIC_UUID_RX "6e400002-b5a3-f393-e0a9-e50e24dcca9e"

const int LASER_PIN = 2;
const int SAMPLE_RATE = 1000; // Ultra-slow 1kHz default — transistor-safe!

hw_timer_t * timer = NULL;
portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;

#define BUF_SIZE 16384 // Large buffer for slower speed
volatile uint8_t audioBuf[BUF_SIZE];
volatile uint32_t head = 0;
volatile uint32_t tail = 0;
volatile bool isPlaying = false;

// 128 is mid-point for 8-bit unsigned PCM, gives stable DC bias for the laser
const uint8_t IDLE_BIAS = 128;

void IRAM_ATTR onTimer() {
    portENTER_CRITICAL_ISR(&timerMux);
    if (isPlaying && head != tail) {
        uint8_t sample = audioBuf[tail];
        tail = (tail + 1) % BUF_SIZE;
        ledcWrite(LASER_PIN, sample);
    } else {
        ledcWrite(LASER_PIN, IDLE_BIAS);
    }
    portEXIT_CRITICAL_ISR(&timerMux);
}

class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
        Serial.println("[INFO] Browser Connected to Analog TX");
    }
    void onDisconnect(BLEServer* pServer) {
        isPlaying = false;
        Serial.println("[WARN] Browser Disconnected");
        BLEDevice::startAdvertising();
    }
};

class MyRxCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
        uint8_t* rxData = pCharacteristic->getData();
        size_t rxLength = pCharacteristic->getLength();
        
        if (rxLength > 4 && strncmp((const char*)rxData, "CMD:", 4) == 0) {
            // Safely create a null-terminated copy of the command string
            char cmdBuf[64];
            size_t copyLen = (rxLength < 63) ? rxLength : 63;
            memcpy(cmdBuf, rxData, copyLen);
            cmdBuf[copyLen] = '\0';
            String cmdStr = String(cmdBuf);
            Serial.println("[CMD] " + cmdStr);
            if (cmdStr == "CMD:START") {
                portENTER_CRITICAL(&timerMux);
                head = 0; tail = 0;
                isPlaying = true;
                portEXIT_CRITICAL(&timerMux);
            }
            if (cmdStr == "CMD:STOP") {
                isPlaying = false;
            }
            if (cmdStr.startsWith("CMD:RATE:")) {
                int rate = cmdStr.substring(9).toInt();
                if (rate > 0) {
                    // Update audio playback timer
                    timerAlarm(timer, 1000000 / rate, true, 0);
                    // Update PWM carrier to 2x the audio rate
                    ledcChangeFrequency(LASER_PIN, rate * 2, 8);
                    Serial.printf("[INFO] Audio=%dHz, PWM Carrier=%dHz\n", rate, rate * 2);
                }
            }
            return;
        }

        // Add raw audio chunks to circular buffer
        portENTER_CRITICAL(&timerMux);
        for (size_t i = 0; i < rxLength; i++) {
            uint32_t nextHead = (head + 1) % BUF_SIZE;
            if (nextHead != tail) { // If not full
                audioBuf[head] = rxData[i];
                head = nextHead;
            }
        }
        portEXIT_CRITICAL(&timerMux);
    }
};

void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("====================================");
    Serial.println("[INFO] Booting Analog VLC_TX (PWM on Pin 2)...");
    
    // Setup PWM on the laser pin — ultra-slow 2kHz carrier for basic transistors
    ledcAttach(LASER_PIN, 2000, 8);
    ledcWrite(LASER_PIN, IDLE_BIAS);

    BLEDevice::init("VLC_TX_Analog");
    BLEDevice::setMTU(512);

    BLEServer *pServer = BLEDevice::createServer();
    pServer->setCallbacks(new MyServerCallbacks());

    BLEService *pService = pServer->createService(SERVICE_UUID);
    
    BLECharacteristic *pRxCharacteristic = pService->createCharacteristic(
        CHARACTERISTIC_UUID_RX,
        BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_WRITE_NR
    );
    pRxCharacteristic->setCallbacks(new MyRxCallbacks());

    pService->start();
    pServer->getAdvertising()->start();
    Serial.println("[INFO] BLE Advertising...");

    // Setup 2kHz Timer Interrupt (ESP32 Core v3.0+)
    timer = timerBegin(1000000); // 1MHz base clock
    timerAttachInterrupt(timer, &onTimer);
    timerAlarm(timer, 1000000 / SAMPLE_RATE, true, 0);
}

void loop() {
    vTaskDelay(portMAX_DELAY);
}
