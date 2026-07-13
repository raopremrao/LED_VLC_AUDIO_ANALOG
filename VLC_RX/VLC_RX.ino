#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

#define SERVICE_UUID           "6e400001-b5a3-f393-e0a9-e50e24dcca9e"
#define CHARACTERISTIC_UUID_TX "6e400003-b5a3-f393-e0a9-e50e24dcca9e"
#define CHARACTERISTIC_UUID_RX "6e400002-b5a3-f393-e0a9-e50e24dcca9e"

const int ADC_PIN = 34;
const int SAMPLE_RATE = 4000;

hw_timer_t * timer = NULL;
portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;

#define BUF_SIZE 12000 // 3 seconds of audio at 4kHz
uint8_t audioBuf[BUF_SIZE];
volatile uint32_t head = 0;
uint32_t tail = 0;
volatile bool isRecording = false;
bool isSending = false;

BLECharacteristic *pTxCharacteristic;
volatile bool deviceConnected = false;

void IRAM_ATTR onTimer() {
    if (!isRecording) return;
    
    // Read 12-bit ADC and shift down to 8-bit for BLE streaming
    uint16_t adcVal = analogRead(ADC_PIN);
    uint8_t sample = adcVal >> 4;

    portENTER_CRITICAL_ISR(&timerMux);
    if (head < BUF_SIZE) {
        audioBuf[head] = sample;
        head++;
        if (head >= BUF_SIZE) {
            isRecording = false; // Stop recording when buffer is full (3 seconds)
            isSending = true;    // Trigger slow BLE transmission
        }
    }
    portEXIT_CRITICAL_ISR(&timerMux);
}

class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
        deviceConnected = true;
        Serial.println("[INFO] Browser Connected to Analog RX");
    }
    void onDisconnect(BLEServer* pServer) {
        deviceConnected = false;
        isRecording = false;
        isSending = false;
        Serial.println("[WARN] Browser Disconnected");
        BLEDevice::startAdvertising();
    }
};

class MyRxCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
        uint8_t* rxData = pCharacteristic->getData();
        size_t rxLength = pCharacteristic->getLength();
        
        if (rxLength > 4 && strncmp((const char*)rxData, "CMD:", 4) == 0) {
            String cmdStr = String((const char*)rxData).substring(0, rxLength);
            Serial.println("[CMD] " + cmdStr);
            if (cmdStr == "CMD:START") {
                portENTER_CRITICAL(&timerMux);
                head = 0;
                tail = 0;
                isSending = false;
                isRecording = true;
                portEXIT_CRITICAL(&timerMux);
                Serial.println("[INFO] Started 3-second Analog Recording...");
            }
            if (cmdStr == "CMD:STOP") {
                isRecording = false;
            }
        }
    }
};

void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("====================================");
    Serial.println("[INFO] Booting Analog VLC_RX (ADC Pin 34)...");

    analogReadResolution(12);

    BLEDevice::init("VLC_RX_Analog");
    BLEDevice::setMTU(512);

    BLEServer *pServer = BLEDevice::createServer();
    pServer->setCallbacks(new MyServerCallbacks());

    BLEService *pService = pServer->createService(SERVICE_UUID);
    
    pTxCharacteristic = pService->createCharacteristic(
        CHARACTERISTIC_UUID_TX,
        BLECharacteristic::PROPERTY_NOTIFY
    );
    pTxCharacteristic->addDescriptor(new BLE2902());

    BLECharacteristic *pRxCharacteristic = pService->createCharacteristic(
        CHARACTERISTIC_UUID_RX,
        BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_WRITE_NR
    );
    pRxCharacteristic->setCallbacks(new MyRxCallbacks());

    pService->start();
    pServer->getAdvertising()->start();
    Serial.println("[INFO] BLE Advertising...");

    timer = timerBegin(1000000); // 1MHz base clock
    timerAttachInterrupt(timer, &onTimer);
    timerAlarm(timer, 1000000 / SAMPLE_RATE, true, 0);
}

void loop() {
    if (deviceConnected && isSending) {
        uint32_t remaining = head - tail;
        
        if (remaining > 0) {
            uint32_t chunkSize = (remaining > 240) ? 240 : remaining;
            uint8_t txBuf[240];
            
            for (uint32_t i = 0; i < chunkSize; i++) {
                txBuf[i] = audioBuf[tail + i];
            }
            tail += chunkSize;

            pTxCharacteristic->setValue(txBuf, chunkSize);
            pTxCharacteristic->notify();
            
            // Slow down BLE transmission so Linux/Windows Bluetooth stack doesn't drop packets
            delay(40); 
        } else {
            // Finished sending the 3-second snippet
            isSending = false;
            Serial.println("[INFO] Sent 3-second recording to Browser.");
        }
    } else {
        delay(100);
    }
}
