/*
 * FullFeatures.ino
 * 
 * Complete example demonstrating all features of HomeWindWSAmoled library:
 * - HR widget with all states
 * - CSC widget with RPM display
 * - Fan widget (4 fans) with different states
 * - Power save functionality
 * - QR code settings
 * - Display rotation (manual and automatic)
 * 
 * Hardware Requirements:
 * - ESP32 microcontroller
 * - SH8601 AMOLED display (280×456 pixels)
 * - FT3168 touch controller
 * 
 * Dependencies:
 * - LVGL library (must be installed)
 * 
 * Optional: For automatic display rotation based on IMU:
 * 
 * The QMI8658 IMU driver is fully integrated into the HomeWindWSAmoled library.
 * Automatic rotation is enabled automatically if QMI8658 hardware is detected.
 * No additional code needed - just call homewind_init()!
 * 
 * Manual rotation example (works without IMU):
 *   homewind_set_rotation(LV_DISP_ROT_180);  // Rotate 180 degrees
 */

#include <HomeWindWSAmoled.h>

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("HomeWindWSAmoled Full Features Example");
  
  // Initialize the library (display, LVGL, touch, UI, powersave)
  homewind_init();
  
  // Set QR code URL for settings modal
  homewind_set_qr_code_url("https://homewind.example.com/setup");
  
  // Initialize HR widget - start with inactive state (configured but not connected)
  // API: homewind_set_hr(state, name, value)
  homewind_set_hr(HR_STATE_INACTIVE, "TickrFit", 0);
  Serial.println("HR: Inactive state (not connected)");
  
  // Initialize CSC widget - not configured
  // API: homewind_set_csc(state, name, cadence)
  homewind_set_csc(CSC_NOT_CONFIGURATED, NULL, 0);
  Serial.println("CSC: Not configured");
  
  // Initialize Fan states with toggle support
  // New v1.4 API: homewind_set_fan(index, state, is_on)
  // Fans can be toggled on/off by touching them (only ACTIVE and ERROR fans are toggleable)
  homewind_set_fan(0, FAN_STATE_ACTIVE, false);    // Fan 1: Active, OFF
  homewind_set_fan(1, FAN_STATE_ACTIVE, true);     // Fan 2: Active, ON
  homewind_set_fan(2, FAN_STATE_ERROR, false);     // Fan 3: Error, OFF
  homewind_set_fan_state(3, FAN_STATE_NOT_CONFIGURATED);   // Fan 4: Not configured (not toggleable)
  Serial.println("Fans: 2 active (1 on, 1 off), 1 error (off), 1 inactive");
  
  // Register fan toggle callback to handle user interactions
  homewind_set_fan_toggle_callback([](uint8_t fan_index, bool is_on) {
    Serial.print("Fan ");
    Serial.print(fan_index + 1);
    Serial.print(" toggled to: ");
    Serial.println(is_on ? "ON" : "OFF");
    // Here you can control actual fan hardware, update state, etc.
  });
  
  // Optional: Set initial display rotation
  // homewind_set_rotation(LV_DISP_ROT_0);    // Normal orientation
  // homewind_set_rotation(LV_DISP_ROT_180);  // Rotate 180 degrees
  // homewind_set_rotation(LV_DISP_ROT_90);   // Rotate 90 degrees
  // homewind_set_rotation(LV_DISP_ROT_270);  // Rotate 270 degrees
  
  Serial.println("\nSetup complete!");
  Serial.println("Features:");
  Serial.println("- Touch the settings button (top right) to open QR code modal");
  Serial.println("- Touch ACTIVE or ERROR fans to toggle them on/off (smooth 300ms animation)");
  Serial.println("- Power save will activate after 10s (dimmed) and 12s (soft powersave)");
  Serial.println("- Touch screen to wake from power save");
  Serial.println("- Display rotation: Use homewind_set_rotation() or enable automatic IMU rotation");
}

void loop() {
  static unsigned long lastHRUpdate = 0;
  static unsigned long lastCSCUpdate = 0;
  static unsigned long lastFanUpdate = 0;
  static unsigned long lastRotationDemo = 0;
  
  // Update HR value every 2 seconds
  if (millis() - lastHRUpdate > 2000) {
    lastHRUpdate = millis();
    
    // Simulate HR sensor connection after 5 seconds
    if (millis() > 5000) {
      static uint16_t hrValue = 75;
      hrValue += random(-3, 4);
      if (hrValue < 60) hrValue = 60;
      if (hrValue > 180) hrValue = 180;
      
      homewind_set_hr(HR_STATE_ACTIVE, "TickrFit", hrValue);
    }
  }
  
  // Update CSC value every 3 seconds
  if (millis() - lastCSCUpdate > 3000) {
    lastCSCUpdate = millis();
    
    // Simulate CSC sensor connection after 7 seconds
    if (millis() > 7000) {
      static uint16_t cadence = 85;
      cadence += random(-5, 6);
      if (cadence < 0) cadence = 0;
      if (cadence > 120) cadence = 120;
      
      homewind_set_csc(CSC_STATE_ACTIVE, "Wahoo Cadence", cadence);
    }
  }
  
  // Change fan states periodically (every 10 seconds)
  if (millis() - lastFanUpdate > 10000) {
    lastFanUpdate = millis();
    
    // Cycle through different fan states for demonstration
    static uint8_t fanCycle = 0;
    fanCycle = (fanCycle + 1) % 4;
    
    switch (fanCycle) {
      case 0:
        // All active, mixed on/off states
        homewind_set_fan(0, FAN_STATE_ACTIVE, true);   // ON
        homewind_set_fan(1, FAN_STATE_ACTIVE, false);  // OFF
        homewind_set_fan(2, FAN_STATE_ACTIVE, true);   // ON
        homewind_set_fan(3, FAN_STATE_ACTIVE, false);  // OFF
        Serial.println("All fans: ACTIVE (mixed on/off)");
        break;
      case 1:
        // Mixed states with different toggle states
        homewind_set_fan(0, FAN_STATE_ACTIVE, true);
        homewind_set_fan(1, FAN_STATE_ERROR, false);
        homewind_set_fan_state(2, FAN_STATE_NOT_CONFIGURATED);  // Not configured (not toggleable)
        homewind_set_fan(3, FAN_STATE_ERROR, true);
        Serial.println("Fans: Mixed states (with toggle)");
        break;
      case 2:
        // All error, all off
        for (uint8_t i = 0; i < 4; i++) {
          homewind_set_fan(i, FAN_STATE_ERROR, false);
        }
        Serial.println("All fans: ERROR (all off)");
        break;
      case 3:
        // All inactive
        for (uint8_t i = 0; i < 4; i++) {
          homewind_set_fan_state(i, FAN_STATE_NOT_CONFIGURATED);
        }
        Serial.println("All fans: INACTIVE");
        break;
    }
  }
  
  // Optional: Demonstrate manual rotation (uncomment to enable)
  // Rotates display every 15 seconds for demonstration
  /*
  if (millis() - lastRotationDemo > 15000) {
    lastRotationDemo = millis();
    static uint8_t rotationCycle = 0;
    rotationCycle = (rotationCycle + 1) % 4;
    
    switch (rotationCycle) {
      case 0:
        homewind_set_rotation(LV_DISP_ROT_0);
        Serial.println("Rotation: 0° (Normal)");
        break;
      case 1:
        homewind_set_rotation(LV_DISP_ROT_90);
        Serial.println("Rotation: 90°");
        break;
      case 2:
        homewind_set_rotation(LV_DISP_ROT_180);
        Serial.println("Rotation: 180°");
        break;
      case 3:
        homewind_set_rotation(LV_DISP_ROT_270);
        Serial.println("Rotation: 270°");
        break;
    }
  }
  */
  
  delay(100);
}

