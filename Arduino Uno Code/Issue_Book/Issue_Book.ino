/*
 * RFID Library System (NodeMCU ESP-12E)
 * Supports both Issue and Return with RFID Cards
 */

#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <SPI.h>
#include <MFRC522.h>

#define RST_PIN D1
#define SS_PIN  D2
MFRC522 rfid(SS_PIN, RST_PIN);

const char* WIFI_SSID = "";
const char* WIFI_PASS = "";
const char* SERVER_IP = "192.xxx.x.xxx"; 
const int SERVER_PORT = 3000;

const byte STUDENT_BLOCK_NAME = 8;
const byte STUDENT_BLOCK_ROLL = 9;
const byte STUDENT_BLOCK_DEPT = 10;

const byte BOOK_BLOCK_TITLE  = 4;
const byte BOOK_BLOCK_ID     = 5;
const byte BOOK_BLOCK_AUTHOR = 6;

String mode = ""; // "issue" or "return"

// ================= Helpers =================
String readBlock(byte block) {
  MFRC522::StatusCode status;
  byte buffer[18];
  byte size = sizeof(buffer);
  status = rfid.MIFARE_Read(block, buffer, &size);
  if (status != MFRC522::STATUS_OK) return "";
  String val = "";
  for (byte i = 0; i < 16; i++) if (buffer[i] != 32 && buffer[i] != 0) val += (char)buffer[i];
  val.trim();
  return val;
}

void waitForCardRemoval() {
  Serial.println("📤 Please remove card...");
  while (rfid.PICC_IsNewCardPresent() || rfid.PICC_ReadCardSerial()) delay(100);
  Serial.println("✅ Card removed.");
}

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
  int code = http.POST(json);
  Serial.printf("➡️ POST %s -> %d\n", path.c_str(), code);
  if (code > 0) Serial.println("Server: " + http.getString());
  else Serial.printf("❌ HTTP Error: %s\n", http.errorToString(code).c_str());
  http.end();
}

// ================= Setup =================
void setup() {
  Serial.begin(115200);
  SPI.begin();
  rfid.PCD_Init();

  Serial.println("\n📚 RFID Library System Ready!");
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("Connecting Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
  Serial.println("\n✅ Wi-Fi connected!");
  Serial.print("IP: "); Serial.println(WiFi.localIP());
}

// ================= Main Loop =================
void loop() {
  // Wait for user to choose mode
  while (mode == "") {
    Serial.println("\nChoose Mode: Type 'I' for ISSUE or 'R' for RETURN, then press Enter.");
    while (!Serial.available()) delay(100);
    char c = Serial.read();
    if (c == 'I' || c == 'i') { mode = "issue"; Serial.println("📘 Mode: ISSUE selected"); }
    else if (c == 'R' || c == 'r') { mode = "return"; Serial.println("📗 Mode: RETURN selected"); }
    else Serial.println("❌ Invalid input. Try again.");
    delay(500);
  }

  MFRC522::MIFARE_Key key;
  for (byte i = 0; i < 6; i++) key.keyByte[i] = 0xFF;
  MFRC522::StatusCode status;

  if (mode == "issue") {
    // --- Scan Student Card ---
    Serial.println("\nScan Student Card...");
    while (!rfid.PICC_IsNewCardPresent() || !rfid.PICC_ReadCardSerial()) delay(50);
    Serial.println("👤 Student Card Detected!");

    status = rfid.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, STUDENT_BLOCK_NAME, &key, &(rfid.uid));
    String studentName, rollNo, dept;
    if (status == MFRC522::STATUS_OK) {
      studentName = readBlock(STUDENT_BLOCK_NAME);
      rollNo      = readBlock(STUDENT_BLOCK_ROLL);
      dept        = readBlock(STUDENT_BLOCK_DEPT);
    }
    rfid.PICC_HaltA(); rfid.PCD_StopCrypto1(); waitForCardRemoval();

    // --- Scan Book Card ---
    Serial.println("\nNow scan Book Card...");
    while (!rfid.PICC_IsNewCardPresent() || !rfid.PICC_ReadCardSerial()) delay(50);
    Serial.println("📘 Book Card Detected!");

    status = rfid.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, BOOK_BLOCK_TITLE, &key, &(rfid.uid));
    String bookTitle, bookID, author;
    if (status == MFRC522::STATUS_OK) {
      bookTitle = readBlock(BOOK_BLOCK_TITLE);
      bookID    = readBlock(BOOK_BLOCK_ID);
      author    = readBlock(BOOK_BLOCK_AUTHOR);
    }

    rfid.PICC_HaltA(); rfid.PCD_StopCrypto1(); waitForCardRemoval();

    // --- Send to Server ---
    String studentEmail = "tanmaynerurkar12@gmail.com";
    String json = "{\"student_uid\":\"" + rollNo + "\","
                  "\"student_name\":\"" + studentName + "\","
                  "\"student_email\":\"" + studentEmail + "\","
                  "\"book_uid\":\"" + bookID + "\","
                  "\"book_title\":\"" + bookTitle + "\"}";
    sendPOST("/issue", json);
    mode = ""; // Reset after one transaction
  }

  else if (mode == "return") {
    // --- Scan Book Card Only ---
    Serial.println("\nScan Book Card to RETURN...");
    while (!rfid.PICC_IsNewCardPresent() || !rfid.PICC_ReadCardSerial()) delay(50);
    Serial.println("📗 Book Card Detected!");

    status = rfid.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, BOOK_BLOCK_ID, &key, &(rfid.uid));
    String bookID = readBlock(BOOK_BLOCK_ID);
    rfid.PICC_HaltA(); rfid.PCD_StopCrypto1(); waitForCardRemoval();

    String json = "{\"book_uid\":\"" + bookID + "\"}";
    sendPOST("/return", json);
    mode = ""; // Reset after one transaction
  }

  delay(1500);
}
