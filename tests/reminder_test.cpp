#include "../Reminder.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
using namespace water;

int main() {
  Reminder r;
  r.begin(0, true);
  r.update(59999, true, false);
  assert(!r.due() && r.elapsedMinutes() == 0);
  r.update(kIntervalMs - 1, true, false);
  assert(!r.due() && !r.buzzerOn());
  r.update(kIntervalMs, true, false);
  assert(r.due() && r.buzzerOn() && r.remainingPercent() == 0);
  r.update(kIntervalMs + 300, true, false);
  assert(!r.buzzerOn());
  r.update(kIntervalMs + 600, true, false);
  assert(r.buzzerOn());
  r.update(kIntervalMs + kMaxAlarmMs, true, false);
  assert(r.timedOut() && !r.buzzerOn());
  r.update(kIntervalMs + 2 * kMaxAlarmMs, true, false);
  assert(r.timedOut() && !r.buzzerOn());

  r.update(kIntervalMs + 2 * kMaxAlarmMs + 1, false, false);
  r.update(kIntervalMs + 2 * kMaxAlarmMs + 51, false, false);
  r.update(kIntervalMs + 2 * kMaxAlarmMs + 100, true, false);
  r.update(kIntervalMs + 2 * kMaxAlarmMs + 150, true, false);
  assert(!r.timedOut() && !r.due() && r.remainingPercent() == 100);

  // Pickup immediately silences the buzzer; a real return resets exactly once.
  r.begin(0, true);
  r.update(kIntervalMs, true, false);
  r.update(kIntervalMs + 1, false, false);
  assert(!r.buzzerOn() && r.cupPresent());
  r.update(kIntervalMs + 51, false, false);
  assert(!r.cupPresent());
  r.update(2 * kIntervalMs, true, false);
  r.update(2 * kIntervalMs + 50, true, false);
  assert(!r.due() && r.cupPresent() && r.elapsedMinutes() == 0);
  assert(fabs(r.averageMinutes() - 96.0005f) < 0.001f);
  const float average = r.averageMinutes();
  r.update(2 * kIntervalMs + 5000, true, false);
  assert(r.averageMinutes() == average);

  // Sensor bounce is not a drink; absence at boot is not a drink either.
  r.begin(0, true);
  r.update(10000, false, false);
  r.update(10030, true, false);
  r.update(10100, true, false);
  assert(r.cupPresent() && r.remainingPercent() < 100 && r.averageMinutes() == 60);
  r.begin(0, false);
  r.update(kIntervalMs, false, false);
  assert(!r.due());
  r.update(kIntervalMs + 1, true, false);
  r.update(kIntervalMs + 51, true, false);
  assert(r.elapsedMinutes() == 0 && r.averageMinutes() == 60);

  // Silent overnight expiry defers the alarm without latching a false fault.
  assert(isQuietHour(0) && isQuietHour(9) && isQuietHour(23));
  assert(!isQuietHour(10) && !isQuietHour(22));
  r.begin(0, true);
  r.update(kIntervalMs, true, true);
  r.update(kIntervalMs + 3600000UL, true, true);
  assert(r.due() && !r.timedOut() && !r.buzzerOn());
  r.update(kIntervalMs + 3600001UL, true, false);
  assert(r.buzzerOn());
  r.update(kIntervalMs + 3600010UL, true, true);
  assert(!r.buzzerOn() && !r.timedOut());

  // Both interval and beep/debounce timers cross the 32-bit millis boundary.
  const uint32_t start = UINT32_MAX - kIntervalMs + 1000;
  r.begin(start, true);
  r.update(start + kIntervalMs - 1, true, false);
  assert(!r.due());
  r.update(start + kIntervalMs, true, false);
  assert(r.due() && r.buzzerOn());
  r.begin(UINT32_MAX - kIntervalMs - 10, true);
  r.update(UINT32_MAX - 10, true, false);
  r.update(289, true, false);
  assert(!r.buzzerOn());
  r.update(589, true, false);
  assert(r.buzzerOn());
  r.update(17989, true, false);
  assert(r.timedOut());
  r.begin(UINT32_MAX - 100, true);
  r.update(UINT32_MAX - 20, false, false);
  r.update(30, false, false);
  assert(!r.cupPresent());

  // Saturate a very long untouched cycle; never wrap back to "not due".
  r.begin(0, true);
  r.update(UINT32_MAX - 1, true, true);
  r.update(1000, true, true);
  assert(r.due() && r.remainingPercent() == 0);
  puts("PASS: real timing, beep budget, cup cycles, debounce, quiet hours, rollover and saturation");
}
