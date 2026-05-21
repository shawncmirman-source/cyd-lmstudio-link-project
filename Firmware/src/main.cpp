#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <SPI.h>
#include <TFT_eSPI.h>

// NETWORK CREDENTIALS & WORKSTATION IP BRIDGE CONFIGURATION
const char* ssid     = "YOUR SSID HERE";
const char* password = "YOUR WIFI PASSWORD HERE";
const char* endpoint = "http://192.168.x.XXX:5000/metrics"; 

TFT_eSPI tft = TFT_eSPI();

// BRANDED SYSTEM PALETTE (Midnight Luxury Custom Themes)
#define PACK_COLOR(r, g, b) (((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3))
uint16_t BASE_BG      = PACK_COLOR(22, 22, 22);     
uint16_t PANEL_BG     = PACK_COLOR(33, 33, 33);     
uint16_t TEXT_PEARL   = PACK_COLOR(220, 220, 220);  
uint16_t ACCENT_AMBER  = PACK_COLOR(245, 158, 11);   

// TELEMETRY RECONCILIATION REGISTERS
unsigned long lastPollTime = 0;
const long pollInterval    = 2500; 

String trackingStatus = "";
String trackingModel  = "";
int trackingTokens    = -1;

void drawDashboardFramework() {
    tft.fillScreen(BASE_BG);
    
    // UI Header Region Block
    tft.fillRect(0, 0, 320, 32, PANEL_BG);
    tft.drawFastHLine(0, 32, 320, ACCENT_AMBER);
    
    tft.setTextColor(TEXT_PEARL, PANEL_BG);
    tft.setTextDatum(MC_DATUM); 
    tft.drawString("DEVELOPER HUD // RUNTIME METRICS", 160, 16, 2);
    
    // Status Field Sub-Card Setup
    tft.fillRect(10, 44, 300, 56, PANEL_BG);
    tft.drawRect(10, 44, 300, 56, PACK_COLOR(55, 55, 55));
    tft.setTextColor(TEXT_PEARL, PANEL_BG);
    tft.setTextDatum(TL_DATUM);
    tft.drawString("AGENT ECOSYSTEM STATE:", 20, 50, 1);
    
    // Engine Model Field Sub-Card Setup
    tft.fillRect(10, 112, 300, 56, PANEL_BG);
    tft.drawRect(10, 112, 300, 56, PACK_COLOR(55, 55, 55));
    tft.drawString("CORE PARALLEL LLM:", 20, 118, 1);
    
    // Metrics Counting Display Area Setup
    tft.fillRect(10, 180, 300, 50, PANEL_BG);
    tft.drawRect(10, 180, 300, 50, PACK_COLOR(55, 55, 55));
    tft.drawString("SESSION VOLUME TOKENS:", 20, 186, 1);
}

void processIncomingMetrics() {
    if (WiFi.status() != WL_CONNECTED) return;
    
    HTTPClient http;
    http.begin(endpoint);
    int responseCode = http.GET();
    
    if (responseCode == 200) {
        String serverPayload = http.getString();
        JsonDocument jsonDoc;
        DeserializationError parseErr = deserializeJson(jsonDoc, serverPayload);
        
        if (!parseErr) {
            String freshStatus = jsonDoc["agent_status"].as<String>();
            String freshModel  = jsonDoc["active_model"].as<String>();
            int freshTokens    = jsonDoc["tokens_used"].as<int>();
            
            tft.setTextDatum(TL_DATUM);
            
            if (freshStatus != trackingStatus) {
                tft.fillRect(20, 68, 280, 24, PANEL_BG); 
                tft.setTextColor(ACCENT_AMBER, PANEL_BG);
                tft.drawString(freshStatus, 20, 68, 4);
                trackingStatus = freshStatus;
            }
            
            if (freshModel != trackingModel) {
                tft.fillRect(20, 136, 280, 24, PANEL_BG);
                tft.setTextColor(TEXT_PEARL, PANEL_BG);
                tft.drawString(freshModel, 20, 136, 4);
                trackingModel = freshModel;
            }
            
            if (freshTokens != trackingTokens) {
                tft.fillRect(160, 186, 140, 40, PANEL_BG);
                tft.setTextColor(ACCENT_AMBER, PANEL_BG);
                tft.drawNumber(freshTokens, 160, 186, 4);
                trackingTokens = freshTokens;
            }
        }
    }
    http.end();
}

void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("\n--- CYD Developer HUD Initialization ---");
    
    tft.init();
    
    // NOTE: If your screen layout still appears inverted or backward 
    // after connecting to WiFi, change this '1' to a '3'.
    tft.setRotation(1); 
    
    tft.fillScreen(BASE_BG);
    tft.setTextColor(ACCENT_AMBER, BASE_BG);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("CONNECTING TO WIFI...", 160, 120, 4);
    
    // Explicitly set Wi-Fi station mode
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);
    
    Serial.print("Connecting to SSID: ");
    Serial.println(ssid);

    int connectionAttempts = 0;
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
        connectionAttempts++;
        
        // If it takes more than 20 seconds, print a diagnostic error
        if (connectionAttempts > 40) {
            Serial.println("\n[ERROR] WiFi Connection Timeout! Check credentials/frequency.");
            tft.fillScreen(BASE_BG);
            tft.drawString("WIFI LINK FAILED", 160, 120, 4);
            while(1) { delay(1000); } // Halt execution
        }
    }
    
    Serial.println("\n[SUCCESS] Connected to WiFi Network!");
    Serial.print("Local IP Address: ");
    Serial.println(WiFi.localIP());
    
    drawDashboardFramework();
}

void loop() {
    unsigned long currentClock = millis();
    if (currentClock - lastPollTime >= pollInterval) {
        lastPollTime = currentClock;
        processIncomingMetrics();
    }
}
