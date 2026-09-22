#ifndef XIAOZHUSHOU_REMINDER_H
#define XIAOZHUSHOU_REMINDER_H

#include <stdint.h>

namespace water {
constexpr uint32_t kMinuteMs = 60000UL;
constexpr uint32_t kIntervalMs = 60UL * kMinuteMs;
constexpr uint32_t kDebounceMs = 50;
constexpr uint32_t kBeepHalfPeriodMs = 300;
constexpr uint32_t kMaxAlarmMs = 18000;  // Original 30 x (300 ms on + 300 ms off).
constexpr float kRecentWeight = 0.6f;
constexpr uint8_t kQuietStartHour = 23;
constexpr uint8_t kQuietEndHour = 10;
static_assert(kIntervalMs > 0 && kIntervalMs <= UINT32_MAX / 100UL,
              "Interval must fit the percentage calculation");

inline bool isQuietHour(uint8_t hour) {
  return hour < kQuietEndHour || hour >= kQuietStartHour;
}

// Pure control logic: no hardware calls, delays, dynamic allocation, or RTC dependence.
class Reminder {
 public:
  void begin(uint32_t now, bool cupPresent) {
    lastTick_ = changedAt_ = alarmStarted_ = now;
    elapsedMs_ = 0;
    rawPresent_ = present_ = cycleStarted_ = cupPresent;
    lifted_ = alarming_ = timedOut_ = buzzerOn_ = false;
    averageMinutes_ = static_cast<float>(kIntervalMs) / kMinuteMs;
  }

  void update(uint32_t now, bool rawPresent, bool quiet) {
    const uint32_t delta = now - lastTick_;  // Unsigned subtraction survives millis() rollover.
    lastTick_ = now;
    if (cycleStarted_) {
      // A cup left untouched for weeks stays overdue instead of wrapping to a fresh cycle.
      elapsedMs_ = delta > UINT32_MAX - elapsedMs_ ? UINT32_MAX : elapsedMs_ + delta;
    }
    if (rawPresent != rawPresent_) {
      rawPresent_ = rawPresent;
      changedAt_ = now;
    }
    if (present_ != rawPresent_ && static_cast<uint32_t>(now - changedAt_) >= kDebounceMs) {
      present_ = rawPresent_;
      if (!present_) {
        lifted_ = cycleStarted_;
        alarming_ = false;
      } else {
        if (lifted_) {
          averageMinutes_ = averageMinutes_ * (1.0f - kRecentWeight) +
                            (static_cast<float>(elapsedMs_) / kMinuteMs) * kRecentWeight;
        }
        // Initial placement starts timing but is not counted as a drink.
        cycleStarted_ = true;
        elapsedMs_ = 0;
        lifted_ = alarming_ = timedOut_ = false;
      }
    }

    buzzerOn_ = false;
    if (!present_ || !due() || timedOut_) return;
    if (quiet) {
      alarming_ = false;  // Quiet hours defer an alarm; they do not exhaust its sound budget.
      return;
    }
    if (!alarming_) {
      alarming_ = true;
      alarmStarted_ = now;
    }
    const uint32_t alarmElapsed = now - alarmStarted_;
    if (alarmElapsed >= kMaxAlarmMs) {
      alarming_ = false;
      timedOut_ = true;
      return;
    }
    // Mute on the first raw absence sample; only statistics wait for debounce.
    buzzerOn_ = rawPresent && (alarmElapsed % (2 * kBeepHalfPeriodMs) < kBeepHalfPeriodMs);
  }

  bool cupPresent() const { return present_; }
  bool due() const { return cycleStarted_ && elapsedMs_ >= kIntervalMs; }
  bool timedOut() const { return timedOut_; }
  bool buzzerOn() const { return buzzerOn_; }
  uint32_t elapsedMinutes() const { return elapsedMs_ / kMinuteMs; }
  uint8_t remainingPercent() const {
    if (!cycleStarted_) return 100;
    if (due()) return 0;
    return static_cast<uint8_t>((kIntervalMs - elapsedMs_) * 100UL / kIntervalMs);
  }
  float averageMinutes() const { return averageMinutes_; }

 private:
  uint32_t lastTick_ = 0, changedAt_ = 0, alarmStarted_ = 0, elapsedMs_ = 0;
  bool rawPresent_ = false, present_ = false, cycleStarted_ = false, lifted_ = false;
  bool alarming_ = false, timedOut_ = false, buzzerOn_ = false;
  float averageMinutes_ = 60.0f;
};
}  // namespace water
#endif
