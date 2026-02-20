/*
 * FullFeaturesHeapDebug.ino
 *
 * Same as FullFeatures example, plus heap debug log every 30 seconds:
 *   [Heap] TICK 01:29:43: free=95984, largest=49140, frag=48.8%, drift=2912, minFree=73748
 *
 * All values refer to INTERNAL RAM only (MALLOC_CAP_INTERNAL), so frag is meaningful
 * even when LVGL uses PSRAM (otherwise largest would be PSRAM and frag would be wrong).
 *
 * - TICK   = uptime HH:MM:SS
 * - free   = free internal heap (bytes)
 * - largest= largest contiguous free block in internal heap (bytes)
 * - frag   = internal heap fragmentation % = (1 - largest/free)*100
 * - drift  = change in free internal heap since last log (bytes)
 * - minFree= minimum free internal heap seen since start (bytes)
 *
 * If PSRAM is present, a second line reports PSRAM: free_psram=..., largest_psram=...
 *
 * Hardware Requirements:
 * - ESP32 microcontroller
 * - SH8601 AMOLED display (280×456 pixels)
 * - FT3168 touch controller
 *
 * Dependencies:
 * - HomeWindWSAmoled, LVGL
 */

#include <HomeWindWSAmoled.h>
#include <esp_heap_caps.h>
#include <stdio.h>

/* Use printf() so output appears on same channel as library (qmi8658_log = printf). */

static const unsigned long HEAP_LOG_INTERVAL_MS = 30000;  // 30 seconds

/* Shared with setup() so first loop() log after 30s has correct drift/minFree */
static uint32_t s_lastFree = 0;
static uint32_t s_minFree = UINT32_MAX;
static unsigned long s_lastLogMs = 0;

/* Print one [Heap] line (internal RAM only). Uses printf() so it appears on same UART as library. */
static void printHeapLine(unsigned long nowMs, uint32_t freeBytes, size_t largestBytes,
                          int32_t drift, uint32_t minFree) {
  float fragPct = (freeBytes > 0) ? (1.0f - (float)largestBytes / (float)freeBytes) * 100.0f : 0.0f;
  unsigned long totalSec = nowMs / 1000;
  unsigned int h = (unsigned int)(totalSec / 3600);
  unsigned int m = (unsigned int)((totalSec % 3600) / 60);
  unsigned int s = (unsigned int)(totalSec % 60);

  printf("[Heap] TICK %02u:%02u:%02u: free=%lu, largest=%u, frag=%.1f%%, drift=%ld, minFree=%lu\n",
         (unsigned)h, (unsigned)m, (unsigned)s,
         (unsigned long)freeBytes, (unsigned)largestBytes, (double)fragPct,
         (long)drift, (unsigned long)minFree);
  fflush(stdout);
}

/* Print PSRAM stats when available (one line). */
static void printPsramLine(unsigned long nowMs) {
  size_t freePsram = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
  size_t largestPsram = heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM);
  if (freePsram == 0 && largestPsram == 0) return;  /* No PSRAM */
  unsigned long totalSec = nowMs / 1000;
  unsigned int h = (unsigned int)(totalSec / 3600);
  unsigned int m = (unsigned int)((totalSec % 3600) / 60);
  unsigned int s = (unsigned int)(totalSec % 60);
  printf("[Heap] TICK %02u:%02u:%02u: free_psram=%u, largest_psram=%u\n",
         (unsigned)h, (unsigned)m, (unsigned)s,
         (unsigned)freePsram, (unsigned)largestPsram);
  fflush(stdout);
}

static void logHeapDebug() {
  static bool firstRun = true;

  unsigned long now = millis();
  if (now - s_lastLogMs < HEAP_LOG_INTERVAL_MS) return;
  s_lastLogMs = now;

  /* Internal RAM only so frag is correct when LVGL uses PSRAM (MALLOC_CAP_8BIT would mix heaps). */
  uint32_t freeBytes = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
  size_t largestBytes = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL);

  if (freeBytes < s_minFree) s_minFree = freeBytes;

  int32_t drift = firstRun ? 0 : (int32_t)freeBytes - (int32_t)s_lastFree;
  if (firstRun) firstRun = false;
  s_lastFree = freeBytes;

  printHeapLine(now, freeBytes, largestBytes, drift, s_minFree);
  printPsramLine(now);
}

void setup() {
  Serial.begin(115200);
  delay(500);

  /* Init library first (its printf/qmi8658_log output appears here). */
  homewind_init();
  homewind_set_qr_code_url("https://homewind.example.com/setup");

  homewind_set_hr(HR_STATE_INACTIVE, "TickrFit", 0);
  homewind_set_csc(CSC_NOT_CONFIGURATED, NULL, 0);
  homewind_set_fan(0, FAN_STATE_ACTIVE, false);
  homewind_set_fan(1, FAN_STATE_ACTIVE, true);
  homewind_set_fan(2, FAN_STATE_ERROR, false);
  homewind_set_fan_state(3, FAN_STATE_NOT_CONFIGURATED);

  homewind_set_fan_toggle_callback([](uint8_t fan_index, bool is_on) {
    printf("Fan %u toggled to: %s\n", fan_index + 1, is_on ? "ON" : "OFF");
    fflush(stdout);
  });

  /* Use printf() so output appears on same UART as library (qmi8658_log). */
  printf("\n--- FullFeaturesHeapDebug ---\n");
  printf("Heap log every 30s: [Heap] TICK HH:MM:SS: free=..., largest=..., frag=..., drift=..., minFree=...\n");
  fflush(stdout);

  /* First heap line right after setup (then every 30s in loop). Internal RAM only. */
  {
    unsigned long now = millis();
    uint32_t freeBytes = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    size_t largestBytes = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL);
    printHeapLine(now, freeBytes, largestBytes, 0, freeBytes);
    printPsramLine(now);
    s_lastFree = freeBytes;
    s_minFree = freeBytes;
    s_lastLogMs = now;
  }
  printf("Setup complete.\n");
  fflush(stdout);
}

void loop() {
  logHeapDebug();

  static unsigned long lastHRUpdate = 0;
  static unsigned long lastCSCUpdate = 0;
  static unsigned long lastFanUpdate = 0;

  if (millis() - lastHRUpdate > 2000) {
    lastHRUpdate = millis();
    if (millis() > 5000) {
      static uint16_t hrValue = 75;
      hrValue += random(-3, 4);
      if (hrValue < 60) hrValue = 60;
      if (hrValue > 180) hrValue = 180;
      homewind_set_hr(HR_STATE_ACTIVE, "TickrFit", hrValue);
    }
  }

  if (millis() - lastCSCUpdate > 3000) {
    lastCSCUpdate = millis();
    if (millis() > 7000) {
      static uint16_t cadence = 85;
      cadence += random(-5, 6);
      if (cadence < 0) cadence = 0;
      if (cadence > 120) cadence = 120;
      homewind_set_csc(CSC_STATE_ACTIVE, "Wahoo Cadence", cadence);
    }
  }

  if (millis() - lastFanUpdate > 10000) {
    lastFanUpdate = millis();
    static uint8_t fanCycle = 0;
    fanCycle = (fanCycle + 1) % 4;

    switch (fanCycle) {
      case 0:
        homewind_set_fan(0, FAN_STATE_ACTIVE, true);
        homewind_set_fan(1, FAN_STATE_ACTIVE, false);
        homewind_set_fan(2, FAN_STATE_ACTIVE, true);
        homewind_set_fan(3, FAN_STATE_ACTIVE, false);
        break;
      case 1:
        homewind_set_fan(0, FAN_STATE_ACTIVE, true);
        homewind_set_fan(1, FAN_STATE_ERROR, false);
        homewind_set_fan_state(2, FAN_STATE_NOT_CONFIGURATED);
        homewind_set_fan(3, FAN_STATE_ERROR, true);
        break;
      case 2:
        for (uint8_t i = 0; i < 4; i++) homewind_set_fan(i, FAN_STATE_ERROR, false);
        break;
      case 3:
        for (uint8_t i = 0; i < 4; i++) homewind_set_fan_state(i, FAN_STATE_NOT_CONFIGURATED);
        break;
    }
  }

  delay(100);
}
