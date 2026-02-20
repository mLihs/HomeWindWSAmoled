/*
 * BasicExample.ino
 * 
 * Basic example demonstrating minimal setup and usage of HomeWindWSAmoled library.
 * This example shows how to:
 * - Initialize the library
 * - Set HR widget state
 * - Update HR values
 * - Set fan states with toggle support
 * - Register fan toggle callback
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
  
  // Optional: Set display rotation
  // homewind_set_rotation(LV_DISP_ROT_0);    // Normal (default)
  // homewind_set_rotation(LV_DISP_ROT_180);   // Rotate 180 degrees
  // homewind_set_rotation(LV_DISP_ROT_90);   // Rotate 90 degrees
  // homewind_set_rotation(LV_DISP_ROT_270);  // Rotate 270 degrees
  
  // Set initial HR state to inactive (sensor configured but not connected)
  // API: homewind_set_hr(state, name, value)
  homewind_set_hr(HR_STATE_INACTIVE, "TickrFit", 0);
  homewind_set_csc(CSC_STATE_INACTIVE, "Ridesense", 0);
  
  // Set initial fan states
  // New v1.4 API: homewind_set_fan(index, state, is_on)
  // Fan 1: Active, OFF (can be toggled by touching)
  homewind_set_fan(0, FAN_STATE_ACTIVE, false);
  // Fan 2: Active, ON (can be toggled by touching)
  homewind_set_fan(1, FAN_STATE_ACTIVE, true);
  // Fan 3: Inactive (not toggleable)
  homewind_set_fan_state(2, FAN_STATE_NOT_CONFIGURATED);
  // Fan 4: Error, OFF (can be toggled by touching)
  homewind_set_fan(3, FAN_STATE_ERROR, false);
  
  // Register callback to handle fan toggle events
  // This is called when user touches an ACTIVE or ERROR fan to toggle it on/off
  homewind_set_fan_toggle_callback([](uint8_t fan_index, bool is_on) {
    Serial.print("Fan ");
    Serial.print(fan_index + 1);
    Serial.print(" toggled to: ");
    Serial.println(is_on ? "ON" : "OFF");
    
    // Here you can control actual fan hardware, update state, etc.
  });
  
  Serial.println("Setup complete!");
  Serial.println("- Display shows HR widget in error state");
  Serial.println("- Fan 1: Active OFF (touch to toggle)");
  Serial.println("- Fan 2: Active ON (touch to toggle)");
  Serial.println("- Fan 3: Inactive (not toggleable)");
  Serial.println("- Fan 4: Error OFF (touch to toggle)");
}

void loop() {
  // Simulate HR sensor connection after 3 seconds
  static unsigned long lastUpdate = 0;
  static bool sensorConnected = false;
  
  if (millis() - lastUpdate > 3000) {
    lastUpdate = millis();
    
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
  }
  
  // Small delay to prevent excessive updates
  delay(100);
}

