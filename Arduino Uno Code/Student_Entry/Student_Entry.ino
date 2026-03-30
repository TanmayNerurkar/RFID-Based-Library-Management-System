/*
 * RFID Library Entry/Exit Logger (NodeMCU ESP-12E)
 * Reads student data and sends entry/exit info to Node.js server
 */

#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <SPI.h>
#include <MFRC522.h>

#define RST_PIN D1
#define SS_PIN  D2
MFRC522 rfid(SS_PIN, RST_PIN);

// ==== Wi-Fi & Server ==== Add wifi id pass and IP
const char* WIFI_SSID = "";
const char* WIFI_PASS = "";
const char* SERVER_IP = "192.xxx.xxx.xxx"; 
const int SERVER_PORT = 3000;

// Student data blocks
const byte BLOCK_NAME = 8;
const byte BLOCK_ROLL = 9;
const byte BLOCK_DEPT = 10;

MFRC522::MIFARE_Key key;

// Read 16-byte block
String readBlock(byte block) {
  byte buffer[18];
  byte size = sizeof(buffer);
  MFRC522::StatusCode status = rfid.MIFARE_Read(block, buffer, &size);

  if (status != MFRC522::STATUS_OK) {
    Serial.print("❌ Read failed for block ");
    Serial.println(block);
    return "";
  }

  String value = "";
  for (byte i = 0; i < 16; i++) {
    if (buffer[i] != 32 && buffer[i] != 0)
      value += (char)buffer[i];
  }
  value.trim();
  return value;
}

// Send data to server
void sendPOST(String path, String json) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("❌ Wi-Fi disconnected");
    return;
  }

  WiFiClient client;
  HTTPClient http;
  String url = "http://" + String(SERVER_IP) + ":" + String(SERVER_PORT) + path;

  http.begin(client, url);
  http.addHeader("Content-Type", "application/json");

  int httpCode = http.POST(json);
  Serial.printf("➡️ POST %s -> %d\n", path.c_str(), httpCode);

  if (httpCode > 0)
    Serial.println("Server: " + http.getString());
  else
    Serial.printf("❌ HTTP Error: %s\n", http.errorToString(httpCode).c_str());

  http.end();
}

void setup() {
  Serial.begin(115200);
  SPI.begin();
  rfid.PCD_Init();

  Serial.println("\n🏫 Student Entry/Exit System Ready");

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("Connecting Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
  Serial.println("\n✅ Wi-Fi connected!");
  Serial.print("IP: "); Serial.println(WiFi.localIP());

  for (byte i = 0; i < 6; i++) key.keyByte[i] = 0xFF;
}

void loop() {
  // Wait for card
  if (!rfid.PICC_IsNewCardPresent() || !rfid.PICC_ReadCardSerial())
    return;

  Serial.println("\n✅ Student Card Detected!");
  MFRC522::StatusCode status;

  status = rfid.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, BLOCK_NAME, &key, &(rfid.uid));

  String name, roll, dept;
  if (status == MFRC522::STATUS_OK) {
    name = readBlock(BLOCK_NAME);
    roll = readBlock(BLOCK_ROLL);
    dept = readBlock(BLOCK_DEPT);
  } else {
    Serial.println("❌ Authentication failed!");
  }

  Serial.println("👤 Name: " + name);
  Serial.println("🆔 Roll No: " + roll);
  Serial.println("🏫 Dept/Year: " + dept);

  // Send to server
  String json = "{\"roll\":\"" + roll + "\",\"name\":\"" + name + "\",\"dept\":\"" + dept + "\"}";
  sendPOST("/entry", json);

  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();

  delay(3000);
}
