#include <Arduino.h>
// https://github.com/JChristensen/DS3232RTC
#include <DS3232RTC.h>

#include "i2c_scanner.h"

#if ENABLE_OTA
#include <ArduinoOTA.h>
#include <ESPmDNS.h>
#include <WiFi.h>
#include <WiFiUdp.h>

unsigned long lastPrint = 0;
#endif

DS3232RTC RTC;

typedef union {
  uint8_t b[20];
  struct {
    /* 00 */ uint8_t onesSeconds : 4, tensSeconds : 4; 
    /* 01 */ uint8_t onesMinutes : 4, tensMinutes : 4;
    /* 02 */ uint8_t onesHours : 4, tensHours : 1, pm : 1, twelveHR : 2;
    /* 03 */ uint8_t day : 8;  // [1..7]
    /* 04 */ uint8_t onesDate : 4, tensDate : 4;
    /* 05 */ uint8_t onesMonth : 4, tensMonth : 3, century : 1;
    /* 06 */ uint8_t onesYear : 4, tensYear : 4;
    /* 07 */ uint8_t a1OnesSeconds : 4, a1TensSeconds : 3, a1m1 : 1;
    /* 08 */ uint8_t a1OnesMinutes : 4, a1TensMinutes : 3, a1m2 : 1;
    /* 09 */ uint8_t a1OnesHour : 4, a1TensHour : 1, a1PM : 1, a1TwelveHR : 1, a1m3 : 1;
    /* 0a */ uint8_t a1OnesDate : 4, a1TensDate : 2, a1DayDate : 1, a1m4 : 1;
    /* 0b */ uint8_t a2OnesMinutes : 4, a2TensMinutes : 3, a2m2 : 1;
    /* 0c */ uint8_t a2OnesHour : 4, a2TensHour : 1, a2PM : 1, a2TwelveHR : 1, a2m3 : 1;
    /* 0d */ uint8_t a2OnesDate : 4, a2TensDate : 2, a2DayDate : 1, a2m4 : 1;
    /* 0e */ uint8_t a1ie : 1, a2ie : 1, intcn : 1, rs1 : 1, rs2 : 1, conv : 1, bbsqw : 1, notEOSC : 1;
    /* 0f */ uint8_t a1f : 1, a2f : 1, bsy : 1, en32khz : 1, crate0 : 1, crate1 : 1, bb32khz : 1, osf : 1;
    /* 10 */ int8_t agingOffset : 8; 
    /* 11 */ int8_t msbTemp : 8; 
    /* 12 */ uint8_t :6, lsbTemp : 2; 
    /* 13 */ uint8_t unused : 8;
    // 14-FF SRAM
  };
} RTCregisters;

#define field(VAR, FIELD) (VAR.FIELD?" "#FIELD:"")

bool timeIsSet = false;
uint8_t secs = 0;

void printRTC() {
  RTCregisters regs;
  static_assert(sizeof(regs) <= 32, "regs too big for single I2C read");
  auto err = RTC.readRTC(0x00, regs.b, uint8_t(sizeof(regs)));
  Serial.printf(
      "%02x%02x%02x"       // 00-02 time
      " %02x%02x%02x%02x"  // 03-06 date
      " %02x%02x%02x%02x"  // 07-0a a1
      " %02x%02x%02x"      // 0b-0d a2
      " %02x %02x %02x"    // 0e-10 status/control aging
      " %02x%02x"          // 11-12 temp
      " %02x\n",           // 13 unused
      regs.b[0x00], regs.b[0x01], regs.b[0x02], regs.b[0x03], regs.b[0x04],
      regs.b[0x05], regs.b[0x06], regs.b[0x07], regs.b[0x08], regs.b[0x09],
      regs.b[0x0a], regs.b[0x0b], regs.b[0x0c], regs.b[0x0d], regs.b[0x0e],
      regs.b[0x0f], regs.b[0x10], regs.b[0x11], regs.b[0x12], regs.b[0x13]);
  Serial.printf("err: %d", err);
  Serial.print(", 0E:");
  Serial.print(field(regs,notEOSC));
  Serial.print(field(regs,bbsqw));
  Serial.print(field(regs,conv));
  Serial.print(field(regs,rs2));
  Serial.print(field(regs,rs1));
  Serial.print(field(regs,intcn));
  Serial.print(field(regs,a2ie));
  Serial.print(field(regs,a1ie));
  Serial.printf(", 0F:");
  Serial.print(field(regs,osf));
  Serial.print(field(regs,bb32khz));
  Serial.print(field(regs,crate1));
  Serial.print(field(regs,crate0));
  Serial.print(field(regs,en32khz));
  Serial.print(field(regs,bsy));
  Serial.print(field(regs,a2f));
  Serial.print(field(regs,a1f));
  Serial.printf(", temp: %d.%02dC\n", regs.msbTemp,
      regs.msbTemp < 0 ? (4 - regs.lsbTemp) % 4 * 25 : regs.lsbTemp * 25);
  Serial.printf("date: %d%d/%d%d/%d%d%d %d %d%d:%d%d:%d%d\n", regs.tensDate,
                regs.onesDate, regs.tensMonth, regs.onesMonth, regs.century,
                regs.tensYear, regs.onesYear, regs.day, regs.tensHours,
                regs.onesHours, regs.tensMinutes, regs.onesMinutes,
                regs.tensSeconds, regs.onesSeconds);
  Serial.printf("a1: %x %s %d%d %d%d:%d%d:%d%d\n",
                regs.a1m4 << 3 | regs.a1m3 << 2 | regs.a1m2 << 1 | regs.a1m1,
                regs.a1DayDate ? "day" : "date", regs.a1TensDate,
                regs.a1OnesDate, regs.a1TensHour, regs.a1OnesHour,
                regs.a1TensMinutes, regs.a1OnesMinutes, regs.a1TensSeconds,
                regs.a1OnesSeconds);
  Serial.printf("a2: %x %s %d%d %d%d:%d%d\n", 
                regs.a2m4<<2|regs.a2m3<<1|regs.a2m2, 
                regs.a2DayDate?"day":"date",
                regs.a2TensDate, regs.a2OnesDate, regs.a2TensHour,
                regs.a2OnesHour, regs.a2TensMinutes, regs.a2OnesMinutes );
  if (regs.notEOSC) {
    Serial.println("* oscillator disabled [!EOSC = 1]");
  }
  if (regs.osf) {
    Serial.println("* oscillator stopped [OSC = 1]");
  }
  if (!regs.intcn) {
    Serial.println("* square wave enabled not timer interrupt [INTCN = 0]");
  }
  if (regs.b[0x00] > 0x59 || regs.onesSeconds > 9) {
    Serial.println("* seconds out of range");
  }
  if (regs.b[0x01] > 0x59 || regs.onesMinutes > 9) {
    Serial.println("* minutes out of range");
  }
  if (regs.b[0x02] > 0x70 || regs.onesMinutes > 9 ||
      (regs.twelveHR == 0 // 24 hour clock
        && regs.pm && (regs.tensHours || regs.onesHours > 3)) ||
      (regs.twelveHR == 1 // 12 hour clock
        && regs.tensHours && regs.onesHours > 2)) {
    Serial.printf("* hours out of range 0x%02x\n", regs.b[0x02]);
  }
  if (regs.b[0x03] > 0x07) {
    Serial.println("* day out of range");
  }
  if (regs.b[0x04] > 0x30 || regs.onesDate > 9) {
    Serial.println("* date out of range");
  }
  if (regs.b[0x11] == 0 && regs.b[0x12] == 0) {
    Serial.println("* temp = 0c");
  }
  if (timeIsSet && secs == regs.b[0x00]) {
    Serial.println("* secs not changing");
  }
  secs = regs.b[0x00];
  timeIsSet = true;
}

void setup() {
  Serial.begin(115200);
  Serial.println("DS3232 I2C diagnostic");
  i2c_scan();
  if (!devices[0x18]) {
    Serial.println("no BMA423 found");
  }
  if (!devices[0x68]) {
    Serial.println("no DS3231 RTC found");
  }
  RTC.begin();
  printRTC();
  RTC.writeRTC(0x0E, 0X04);
  RTC.writeRTC(0x0F, 0X00);

#if ENABLE_OTA
  WiFi.mode(WIFI_STA);
  WiFi.begin("your_ssid", "your_wifi_password");
  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
    Serial.println("Connection Failed! Rebooting...");
    delay(5000);
    ESP.restart();
  }
  ArduinoOTA
      .onStart([]() {
        String type;
        if (ArduinoOTA.getCommand() == U_FLASH) {
          type = "sketch";
        } else {
          // U_SPIFFS
          type = "filesystem";
        }

        // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS
        // using SPIFFS.end()
        Serial.println("Start updating " + type);
      })
      .onEnd([]() { Serial.println("\nEnd"); })
      .onProgress([](unsigned int progress, unsigned int total) {
        Serial.printf("Progress: %u%%\r", (progress * 100 / total));
      })
      .onError([](ota_error_t error) {
        Serial.printf("Error[%u]: ", error);
        if (error == OTA_AUTH_ERROR) {
          Serial.println("Auth Failed");
        } else if (error == OTA_BEGIN_ERROR) {
          Serial.println("Begin Failed");
        } else if (error == OTA_CONNECT_ERROR) {
          Serial.println("Connect Failed");
        } else if (error == OTA_RECEIVE_ERROR) {
          Serial.println("Receive Failed");
        } else if (error == OTA_END_ERROR) {
          Serial.println("End Failed");
        }
      });

  ArduinoOTA.begin();
#endif
}

void loop() {
#if ENABLE_OTA
  if (millis() - lastPrint > 10000) {
    printRTC();
    lastPrint = millis();
  }
  ArduinoOTA.handle();
  delay(1);
#else
  printRTC();
  delay(10000);
#endif
}