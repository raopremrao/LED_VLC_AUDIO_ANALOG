#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

#define SERVICE_UUID           "6e400001-b5a3-f393-e0a9-e50e24dcca9e"
#define CHARACTERISTIC_UUID_TX "6e400003-b5a3-f393-e0a9-e50e24dcca9e"
#define CHARACTERISTIC_UUID_RX "6e400002-b5a3-f393-e0a9-e50e24dcca9e"

const int ADC_PIN = 34;
const int SAMPLE_RATE = 8000;

hw_timer_t * timer = NULL;
portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;

#define BUF_SIZE 16384
volatile uint8_t audioBuf[BUF_SIZE];
volatile uint32_t head = 0;
volatile uint32_t tail = 0;
volatile bool isRecording = false;

BLECharacteristic *pTxCharacteristic;
volatile bool deviceConnected = false;

void IRAM_ATTR onTimer() {
    if (!isRecording) return;
    
    // Read 12-bit ADC and shift down to 8-bit for BLE streaming
    uint16_t adcVal = analogRead(ADC_PIN);
    uint8_t sample = adcVal >> 4;

    portENTER_CRITICAL_ISR(&timerMux);
    uint32_t nextHead = (head + 1) % BUF_SIZE;
    if (nextHead != tail) {
        audioBuf[head] = sample;
        head = nextHead;
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
                head = 0; tail = 0;
                portEXIT_CRITICAL(&timerMux);
                isRecording = true;
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

    timer = timerBegin(0, 80, true);
    timerAttachInterrupt(timer, &onTimer, true);
    timerAlarmWrite(timer, 1000000 / SAMPLE_RATE, true);
    timerAlarmEnable(timer);
}

void loop() {
    if (deviceConnected && isRecording) {
        uint32_t currentHead, currentTail;
        portENTER_CRITICAL(&timerMux);
        currentHead = head;
        currentTail = tail;
        portEXIT_CRITICAL(&timerMux);

        uint32_t available = (currentHead >= currentTail) ? 
                             (currentHead - currentTail) : 
                             (BUF_SIZE - currentTail + currentHead);
        
        // Push in chunks of 240 bytes (30ms at 8kHz)
        if (available >= 240) {
            uint8_t txBuf[240];
            for (int i = 0; i < 240; i++) {
                txBuf[i] = audioBuf[currentTail];
                currentTail = (currentTail + 1) % BUF_SIZE;
            }
            
            portENTER_CRITICAL(&timerMux);
            tail = currentTail;
            portEXIT_CRITICAL(&timerMux);

            pTxCharacteristic->setValue(txBuf, 240);
            pTxCharacteristic->notify();
            
            delay(15); // Pace BLE to prevent stack congestion
        } else {
            delay(5); // Wait for more samples
        }
    } else {
        delay(100);
    }
}
