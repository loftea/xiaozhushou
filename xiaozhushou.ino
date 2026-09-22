#include <Arduino.h>
#include <Wire.h>
#include <U8g2lib.h>
#include <RTClib.h>
#include "Reminder.h"

constexpr uint8_t kBuzzerPin = 13;
constexpr uint8_t kSensorPin = 4;
constexpr uint32_t kDisplayIntervalMs = 1000;
constexpr uint8_t kProgressWidth = 80;

RTC_DS3231 rtc;
// Hardware I2C uses the board's SDA/SCL. The second argument is RESET, not SCL.
// One-page buffering uses 128 bytes instead of the 1024-byte full framebuffer.
U8G2_SSD1306_128X64_NONAME_1_HW_I2C display(U8G2_R0, U8X8_PIN_NONE);
water::Reminder reminder;
bool rtcReady = false;
DateTime currentTime;
int16_t temperature = 0;
uint32_t lastDisplayMs = 0;

void drawScreen() {
  display.firstPage();
  do {
    if (!rtcReady) {
      display.setCursor(0, 26);
      display.print(F("RTC unavailable"));
      display.setCursor(0, 45);
      display.print(F("Check I2C wiring"));
      continue;
    }
    display.setCursor(0, 13);
    display.print(currentTime.year());
    display.print('/');
    display.print(currentTime.month());
    display.print('/');
    display.print(currentTime.day());
    display.print(' ');
    display.print(temperature);
    display.print(F(" C"));

    display.setCursor(0, 26);
    display.print(F("Avg: "));
    display.print(static_cast<uint32_t>(reminder.averageMinutes()));
    display.print(F(" min"));
    display.setCursor(0, 39);
    display.print(F("Last: "));
    display.print(reminder.elapsedMinutes());
    display.print(F(" min"));

    display.setCursor(0, 52);
    if (!reminder.cupPresent()) display.print(F("Cup removed"));
    // At night, an overdue cup gets a visual reminder even after a daytime timeout.
    else if (reminder.due() && water::isQuietHour(currentTime.hour()))
      display.print(F("Drink (silent)"));
    else if (reminder.timedOut()) display.print(F("Lift cup to reset"));
    else if (water::isQuietHour(currentTime.hour())) display.print(F("Quiet hours"));
    else if (reminder.due()) display.print(F("Time for water!"));
    else display.print(F("Water matters!"));

    const uint8_t percent = reminder.remainingPercent();
    display.drawRFrame(0, 54, kProgressWidth, 10, 2);
    display.drawBox(1, 55, (kProgressWidth - 2) * percent / 100, 8);
    display.setCursor(kProgressWidth + 2, 64);
    display.print(percent);
    display.print('%');
  } while (display.nextPage());
}

void setup() {
  pinMode(kBuzzerPin, OUTPUT);
  digitalWrite(kBuzzerPin, LOW);
  pinMode(kSensorPin, INPUT);
  Wire.begin();
  display.begin();
  display.setFont(u8g2_font_7x14B_tf);
  rtcReady = rtc.begin();
  if (rtcReady) {
    if (rtc.lostPower()) rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    currentTime = rtc.now();
    temperature = static_cast<int16_t>(rtc.getTemperature());
  }
  lastDisplayMs = millis();
  reminder.begin(lastDisplayMs, digitalRead(kSensorPin) == HIGH);
  drawScreen();
}

void loop() {
  const uint32_t now = millis();
  const bool refresh = static_cast<uint32_t>(now - lastDisplayMs) >= kDisplayIntervalMs;
  if (refresh && rtcReady) {
    currentTime = rtc.now();
    temperature = static_cast<int16_t>(rtc.getTemperature());
  }
  reminder.update(now, digitalRead(kSensorPin) == HIGH,
                  !rtcReady || water::isQuietHour(currentTime.hour()));
  digitalWrite(kBuzzerPin, reminder.buzzerOn() ? HIGH : LOW);
  if (refresh) {
    lastDisplayMs = now;
    drawScreen();
  }
}
