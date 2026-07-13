#include <Arduino.h>
#include <WiFi.h>
#include <WebSocketsServer.h>

const char* ssid = "Valar Morghulis";
const char* password = "Hello World";

WebSocketsServer webSocket = WebSocketsServer(81);

const int ADC_PIN = 34;
const int SAMPLE_RATE = 4000;

hw_timer_t * timer = NULL;
portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;

#define BUF_SIZE 16000 // Circular buffer
uint8_t audioBuf[BUF_SIZE];
volatile uint32_t head = 0;
uint32_t tail = 0;
volatile bool isRecording = false;
uint8_t connectedClientId = 255;

void IRAM_ATTR onTimer() {
    if (!isRecording) return;
    
    // Read ADC once. (We removed the 4x oversampling loop because it takes too long inside an ISR and crashes the ESP32!)
    uint16_t adcVal = analogRead(ADC_PIN);
    
    // Shift 12-bit down to 8-bit
    uint8_t sample = adcVal >> 4;

    portENTER_CRITICAL_ISR(&timerMux);
    uint32_t nextHead = (head + 1) % BUF_SIZE;
    if (nextHead != tail) { // If not full
        audioBuf[head] = sample;
        head = nextHead;
    }
    portEXIT_CRITICAL_ISR(&timerMux);
}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
    switch(type) {
        case WStype_DISCONNECTED:
            Serial.printf("[%u] Disconnected!\n", num);
            isRecording = false;
            connectedClientId = 255;
            break;
        case WStype_CONNECTED:
            {
                IPAddress ip = webSocket.remoteIP(num);
                Serial.printf("[%u] Connected from %d.%d.%d.%d\n", num, ip[0], ip[1], ip[2], ip[3]);
                connectedClientId = num;
            }
            break;
        case WStype_TEXT:
            if (strncmp((const char*)payload, "CMD:START", 9) == 0) {
                portENTER_CRITICAL(&timerMux);
                head = 0;
                tail = 0;
                isRecording = true;
                portEXIT_CRITICAL(&timerMux);
                Serial.println("[INFO] Started continuous Wi-Fi streaming.");
            }
            else if (strncmp((const char*)payload, "CMD:STOP", 8) == 0) {
                isRecording = false;
                Serial.println("[INFO] Stopped Wi-Fi streaming.");
            }
            break;
        case WStype_BIN:
            break;
    }
}

void setup() {
    Serial.begin(115200);
    delay(1000);
    
    Serial.println();
    Serial.print("[INFO] Connecting to WiFi: ");
    Serial.println(ssid);

    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }

    Serial.println("");
    Serial.println("[INFO] WiFi connected successfully!");
    Serial.print("[INFO] ESP32 IP Address: ");
    Serial.println(WiFi.localIP());
    Serial.println("[INFO] Enter this IP in the Web App to connect!");

    webSocket.begin();
    webSocket.onEvent(webSocketEvent);
    
    // Setup 4kHz Timer Interrupt
    timer = timerBegin(1000000); 
    timerAttachInterrupt(timer, &onTimer);
    timerAlarm(timer, 1000000 / SAMPLE_RATE, true, 0);
}

void loop() {
    webSocket.loop();
    
    if (isRecording && connectedClientId != 255) {
        uint32_t currentHead, currentTail;
        portENTER_CRITICAL(&timerMux);
        currentHead = head;
        currentTail = tail;
        portEXIT_CRITICAL(&timerMux);

        uint32_t available = (currentHead >= currentTail) ? 
                             (currentHead - currentTail) : 
                             (BUF_SIZE - currentTail + currentHead);
                             
        // Send chunks of audio data when we have enough
        if (available >= 256) {
            uint8_t txBuf[256];
            for (int i = 0; i < 256; i++) {
                txBuf[i] = audioBuf[currentTail];
                currentTail = (currentTail + 1) % BUF_SIZE;
            }
            
            portENTER_CRITICAL(&timerMux);
            tail = currentTail;
            portEXIT_CRITICAL(&timerMux);

            webSocket.sendBIN(connectedClientId, txBuf, 256);
        }
    }
}
