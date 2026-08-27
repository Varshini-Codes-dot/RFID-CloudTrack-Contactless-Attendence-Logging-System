#include <SPI.h>
#include <MFRC522.h>
#include "FS.h"
#include "SD.h"
#include <Wire.h>
#include <RTClib.h>
#include <LiquidCrystal_I2C.h>

SPIClass hspi(HSPI);

#define SS_PIN 5
#define RST_PIN 4
#define SD_CS_PIN 2

MFRC522 mfrc522(SS_PIN, RST_PIN);
RTC_DS3231 rtc;
LiquidCrystal_I2C lcd(0x27, 16, 2);   // Change to 0x3F if needed

bool cardHandled = false;

const int NUM_CARDS = 4;
const int UID_SIZE = 4;

/* 🔹 ENTER YOUR CARD UIDs HERE */
byte uidList[NUM_CARDS][UID_SIZE] = {
  {0xDE, 0xAD, 0xBE, 0xEF},
  {0x11, 0x22, 0x33, 0x44},
  {0xAA, 0xBB, 0xCC, 0xDD},
  {0x12, 0x34, 0x56, 0x78}
};

const char* nameList[NUM_CARDS] = {
  "Student_1",
  "Student_2",
  "Student_3",
  "Student_4"
};

const char* rollList[NUM_CARDS] = {
  "21EC001",
  "21EC002",
  "21EC003",
  "21EC004"
};

#define SET_RTC_TIME false

int limitHour = 16;
int limitMinute = 45;

/* ---------------- UID MATCH FUNCTION ---------------- */

bool matchUID(byte *uid, int &index) {
  for (int i = 0; i < NUM_CARDS; i++) {
    bool match = true;
    for (int j = 0; j < UID_SIZE; j++) {
      if (uid[j] != uidList[i][j]) {
        match = false;
        break;
      }
    }
    if (match) {
      index = i;
      return true;
    }
  }
  return false;
}

/* ---------------- TIMESTAMP FUNCTION ---------------- */

String makeTimestamp(const DateTime &now) {
  char buffer[20];
  sprintf(buffer, "%04d-%02d-%02d %02d:%02d:%02d",
          now.year(), now.month(), now.day(),
          now.hour(), now.minute(), now.second());
  return String(buffer);
}

/* ---------------- SETUP ---------------- */

void setup() {

  Serial.begin(115200);
  delay(1000);

  /* RFID */
  SPI.begin();
  mfrc522.PCD_Init();
  Serial.println("RFID Ready");

  /* SD CARD */
  hspi.begin(14, 12, 13, SD_CS_PIN);
  if (!SD.begin(SD_CS_PIN, hspi)) {
    Serial.println("SD Fail");
  } else {
    Serial.println("SD OK");

    File f = SD.open("/attendance.txt", FILE_READ);
    if (!f) {
      File nf = SD.open("/attendance.txt", FILE_WRITE);
      if (nf) {
        nf.println("Timestamp, Name, Roll");
        nf.close();
      }
    } else {
      f.close();
    }
  }

  /* RTC */
  Wire.begin(21, 22);
  rtc.begin();

  if (SET_RTC_TIME) {
    rtc.adjust(DateTime(2025, 12, 9, 16, 15, 0));
  }

  /* LCD */
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Attendance Sys");
  lcd.setCursor(0, 1);
  lcd.print("Initializing...");
  delay(2000);
  lcd.clear();

  lcd.setCursor(0, 0);
  lcd.print("Tap Your Card");

  Serial.println("Tap Your Card");
}

/* ---------------- LOOP ---------------- */

void loop() {

  if (mfrc522.PICC_IsNewCardPresent() && mfrc522.PICC_ReadCardSerial()) {

    if (!cardHandled) {

      int index = -1;

      Serial.print("UID: ");
      for (byte i = 0; i < mfrc522.uid.size; i++) {
        Serial.print(mfrc522.uid.uidByte[i], HEX);
        Serial.print(" ");
      }
      Serial.println();

      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Card Detected");
      delay(1000);

      if (matchUID(mfrc522.uid.uidByte, index)) {

        DateTime now = rtc.now();
        String ts = makeTimestamp(now);

        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print(nameList[index]);
        lcd.setCursor(0, 1);
        lcd.print(rollList[index]);
        delay(2000);

        bool onTime = (now.hour() < limitHour) ||
                      (now.hour() == limitHour && now.minute() <= limitMinute);

        if (onTime) {

          File file = SD.open("/attendance.txt", FILE_APPEND);
          if (file) {
            file.print(ts);
            file.print(", ");
            file.print(nameList[index]);
            file.print(", ");
            file.println(rollList[index]);
            file.close();

            Serial.println("Attendance Saved");

            lcd.clear();
            lcd.setCursor(0, 0);
            lcd.print("Attendance");
            lcd.setCursor(0, 1);
            lcd.print("Saved");
            delay(2000);

          } else {
            Serial.println("SD Write Failed");
          }

        } else {

          Serial.println("Late Entry");

          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.print("Late Entry");
          lcd.setCursor(0, 1);
          lcd.print("Not Allowed");
          delay(2000);
        }

      } else {

        Serial.println("Unauthorized Card");

        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Unauthorized");
        lcd.setCursor(0, 1);
        lcd.print("Card");
        delay(2000);
      }

      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Tap Your Card");

      mfrc522.PICC_HaltA();
      mfrc522.PCD_StopCrypto1();
      cardHandled = true;
    }

  } else {
    cardHandled = false;
  }
}
