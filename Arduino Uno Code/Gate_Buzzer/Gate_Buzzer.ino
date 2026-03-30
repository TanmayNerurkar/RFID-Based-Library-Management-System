/*
 * RFID GATE SYSTEM (NodeMCU ESP-12E)
 * Detects book exit and buzzes if NOT issued.
 */

#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <SPI.h>
#include <MFRC522.h>

// RFID pins
#define RST_PIN D1
#define SS_PIN  D2
MFRC522 rfid(SS_PIN, RST_PIN);

// BUZZER pin
#define BUZZER_PIN D3

const char* WIFI_SSID = "";
const char* WIFI_PASS = "";
const char* SERVER_IP = "192.xxx.xxx.xxx"; 
const int SERVER_PORT = 3000;

const byte BOOK_BLOCK_ID = 5;

// ============= CONNECT TO WIFI =============
void connectWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  Serial.print("Connecting WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWiFi Connected!");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());
}

// ============= READ RFID DATA =============
String readBlock(byte block) {
  MFRC522::StatusCode status;
  byte buffer[18];
  byte size = sizeof(buffer);

  status = rfid.MIFARE_Read(block, buffer, &size);
  if (status != MFRC522::STATUS_OK) return "";

  String val = "";
  for (byte i = 0; i < 16; i++)
    if (buffer[i] > 32 && buffer[i] < 126)
      val += (char)buffer[i];

  val.trim();
  return val;
}

// ============= SEND GET REQUEST TO SERVER =============
bool checkBookIssued(String bookUID) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi NOT connected!");
    return false;
  }

  WiFiClient client;
  HTTPClient http;

  String url = "http://" + String(SERVER_IP) + ":" + String(SERVER_PORT) + "/check?book_uid=" + bookUID;

  http.begin(client, url);
  int code = http.GET();

  Serial.printf("Server Response Code: %d\n", code);

  if (code <= 0) {
    Serial.println("HTTP ERROR!");
    return false;
  }

  String response = http.getString();
  Serial.println("Server says: " + response);

  // FIX: Check "ok":true instead of "issued":true
  bool isIssued = (response.indexOf("\"ok\":true") != -1);

  http.end();
  return isIssued;
}

// ============= BUZZER ALERT =============
void buzzAlert() {
  Serial.println("BUZZER ALERT! Unauthorized Book!");

  for (int i = 0; i < 5; i++) {
    digitalWrite(BUZZER_PIN, HIGH);
    delay(200);
    digitalWrite(BUZZER_PIN, LOW);
    delay(100);
  }
}

// ============= WAIT FOR CARD REMOVAL =============
void waitForCardRemoval() {
  while (rfid.PICC_IsNewCardPresent() || rfid.PICC_ReadCardSerial())
    delay(50);
}

// ============= SETUP =============
void setup() {
  Serial.begin(115200);
  SPI.begin();
  rfid.PCD_Init();

  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  connectWiFi();

  Serial.println("Gate Security System Ready!");
}

// ============= MAIN LOOP =============
void loop() {

  if (!rfid.PICC_IsNewCardPresent() || !rfid.PICC_ReadCardSerial())
    return;

  Serial.println("Book detected at gate!");

  MFRC522::MIFARE_Key key;
  for (byte i = 0; i < 6; i++) key.keyByte[i] = 0xFF;

  MFRC522::StatusCode status =
      rfid.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, BOOK_BLOCK_ID, &key, &(rfid.uid));

  String bookUID = "";
  if (status == MFRC522::STATUS_OK)
    bookUID = readBlock(BOOK_BLOCK_ID);

  Serial.println("Book UID: " + bookUID);

  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();

  waitForCardRemoval();

  if (bookUID == "") {
    Serial.println("Invalid book card → Buzz!");
    buzzAlert();
    return;
  }

  bool allowed = checkBookIssued(bookUID);

  if (allowed) {
    Serial.println("Book is issued. Gate open.");
    digitalWrite(BUZZER_PIN, LOW);
  } else {
    Serial.println("Book NOT issued → BUZZER ON!");
    buzzAlert();
  }

  delay(500);
}
