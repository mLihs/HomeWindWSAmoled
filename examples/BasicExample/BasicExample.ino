/*
 * BasicExample.ino
 * 
 * Basic example demonstrating minimal setup and usage of HomeWindWSAmoled library.
 * This example shows how to:
 * - Initialize the library
 * - Set HR widget state
 * - Update HR values
 * - Set fan dots and the fan control field
 * - Register the plus/minus and mode callbacks
 * 
 * Hardware Requirements:
 * - ESP32 microcontroller
 * - SH8601 AMOLED display (280×456 pixels)
 * - FT3168 touch controller
 * 
 * Dependencies:
 * - LVGL library (must be installed)
 * 
 * Wiring:
 * See library documentation for pin connections
 */

#include <HomeWindWSAmoled.h>

void setup() {
  // Initialize serial communication for debugging
  Serial.begin(115200);
  delay(1000);
  Serial.println("HomeWindWSAmoled Basic Example");
  
  // Initialize the library (display, LVGL, touch, UI screens, powersave)
  homewind_init();

  // The LVGL task runs from here on: call the library only with the lock held
  lcd_lvgl_lock(-1);

  // Leave the "Warming Up" boot screen; powersave stays off until unlocked
  homewind_show_main_screen();
  powersave_unlock();
  
  // Optional: Set display rotation
  // homewind_set_rotation(LV_DISP_ROT_0);    // Normal (default)
  // homewind_set_rotation(LV_DISP_ROT_180);   // Rotate 180 degrees
  // homewind_set_rotation(LV_DISP_ROT_90);   // Rotate 90 degrees
  // homewind_set_rotation(LV_DISP_ROT_270);  // Rotate 270 degrees
  
  // Set initial HR state to inactive (sensor configured but not connected)
  // API: homewind_set_hr(state, name, value)
  homewind_set_hr(HR_STATE_INACTIVE, "TickrFit", 0);
  homewind_set_csc(CSC_STATE_INACTIVE, "Ridesense", 0);
  
  // Fan dots: homewind_set_fan(index, state, is_on)
  homewind_set_fan(0, FAN_STATE_ACTIVE, false);           // white
  homewind_set_fan(1, FAN_STATE_ACTIVE, true);            // green
  homewind_set_fan_state(2, FAN_STATE_NOT_CONFIGURATED);  // grey
  homewind_set_fan(3, FAN_STATE_ERROR, false);            // orange
  homewind_set_fan_control(FAN_MODE_PURE, true, 40);

  // Plus/minus and the mode long press run in the LVGL task: keep them short
  homewind_set_fan_control_callbacks(
    [](int8_t steps) { Serial.println(steps < 0 ? "Plus" : "Minus"); },
    []() { Serial.println("Mode long press"); });
  lcd_lvgl_unlock();
  
  Serial.println("Setup complete!");
  Serial.println("- HR card shows Disconnected");
  Serial.println("- Tap plus/minus or long-press the mode field");
}

void loop() {
  // Simulate HR sensor connection after 3 seconds
  static unsigned long lastUpdate = 0;
  static bool sensorConnected = false;
  
  if (millis() - lastUpdate > 3000) {
    lastUpdate = millis();
    lcd_lvgl_lock(-1);
    
    if (!sensorConnected) {
      // Connect sensor and set active state with name
      homewind_set_hr(HR_STATE_ACTIVE, "TickrFit", 72);
      sensorConnected = true;
      Serial.println("HR sensor connected: TickrFit, value: 72 BPM");
    } else {
      // Update HR value (simulate heart rate changes)
      static uint16_t hrValue = 72;
      hrValue += random(-5, 6);  // Random variation ±5 BPM
      if (hrValue < 60) hrValue = 60;   // Minimum
      if (hrValue > 180) hrValue = 180; // Maximum
      
      // Use homewind_set_hr_value() when only updating the value
      homewind_set_hr_value(hrValue);
      Serial.print("HR updated: ");
      Serial.print(hrValue);
      Serial.println(" BPM");
    }
    lcd_lvgl_unlock();
  }
  
  // Small delay to prevent excessive updates
  delay(100);
}

