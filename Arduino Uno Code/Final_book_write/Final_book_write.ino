#include <SPI.h>
#include <MFRC522.h>

#define RST_PIN D1   // GPIO5
#define SS_PIN  D2   // GPIO4
MFRC522 mfrc522(SS_PIN, RST_PIN);
MFRC522::MIFARE_Key key;

// Safe Sector 1 (Blocks 4,5,6)
const byte BLOCK_TITLE  = 4;
const byte BLOCK_BOOKID = 5;
const byte BLOCK_AUTHOR = 6;

void fill16(const String &src, byte *out) {
  for (byte i = 0; i < 16; i++) out[i] = (i < src.length()) ? src[i] : ' ';
}

void setup() {
  Serial.begin(115200);
  SPI.begin();
  mfrc522.PCD_Init();
  for (byte i = 0; i < 6; i++) key.keyByte[i] = 0xFF;
  Serial.println("📚 Place the BOOK RFID tag near the reader...");
}

void loop() {
  if (!mfrc522.PICC_IsNewCardPresent() || !mfrc522.PICC_ReadCardSerial()) return;

  Serial.println("\n✅ Book Tag detected!");
  Serial.print("UID: ");
  for (byte i = 0; i < mfrc522.uid.size; i++) {
    Serial.print(mfrc522.uid.uidByte[i] < 0x10 ? " 0" : " ");
    Serial.print(mfrc522.uid.uidByte[i], HEX);
  }
  Serial.println();

  Serial.println("Enter Book Title:");
  String title = readSerialInput();
  Serial.println("Enter Book ID / ISBN:");
  String bookID = readSerialInput();
  Serial.println("Enter Author / Category:");
  String author = readSerialInput();

  // Authenticate ONCE for sector 1 (block 4)
  MFRC522::StatusCode status;
  status = mfrc522.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A,
                                    BLOCK_TITLE, &key, &(mfrc522.uid));
  if (status != MFRC522::STATUS_OK) {
    Serial.println("❌ Auth failed! Try another card or re-scan.");
    return;
  }

  writeBlock(BLOCK_TITLE, title,  "Title");
  writeBlock(BLOCK_BOOKID, bookID, "Book ID");
  writeBlock(BLOCK_AUTHOR, author, "Author");

  Serial.println("✅ Book data written successfully!");
  Serial.println("------------------------------");

  mfrc522.PICC_HaltA();
  mfrc522.PCD_StopCrypto1();

  delay(2000);
}

String readSerialInput() {
  String s = "";
  while (s == "") {
    if (Serial.available()) {
      s = Serial.readStringUntil('\n');
      s.trim();
    }
  }
  return s;
}

void writeBlock(byte block, String data, const char* label) {
  byte buffer[16];
  fill16(data, buffer);
  MFRC522::StatusCode status;

  status = mfrc522.MIFARE_Write(block, buffer, 16);
  Serial.printf(status == MFRC522::STATUS_OK ?
                "✅ %s written\n" : "❌ Failed to write %s\n", label, label);
}
