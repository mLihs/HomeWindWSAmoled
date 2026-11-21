/*
 * FullFeatures.ino
 * 
 * Complete example demonstrating all features of HomeWindWSAmoled library:
 * - HR card with all states
 * - Power and cadence rows, status badges
 * - Fan control (4 fan dots, mode, speed, plus/minus)
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

  // The LVGL task runs from here on: call the library only with the lock held
  lcd_lvgl_lock(-1);

  // Leave the "Warming Up" boot screen; powersave stays off until unlocked
  homewind_show_main_screen();
  powersave_unlock();
  
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
  
  // Power row and badges
  homewind_set_power_row(ROW_DISCONNECTED, "Assioma Duo", 0);
  homewind_set_badges(BADGE_GREY, BADGE_HIDDEN);

  // Fan dots: homewind_set_fan(index, state, is_on)
  homewind_set_fan(0, FAN_STATE_ACTIVE, false);    // Fan 1: white
  homewind_set_fan(1, FAN_STATE_ACTIVE, true);     // Fan 2: green
  homewind_set_fan(2, FAN_STATE_ERROR, false);     // Fan 3: orange
  homewind_set_fan_state(3, FAN_STATE_NOT_CONFIGURATED);   // Fan 4: grey
  homewind_set_fan_control(FAN_MODE_SMART, true, 55);
  Serial.println("Fans: 2 active (1 on, 1 off), 1 error, 1 not configured");
  
  // Plus/minus and the mode long press run in the LVGL task: keep them short
  homewind_set_fan_control_callbacks(
    [](int8_t steps) { Serial.println(steps < 0 ? "Plus" : "Minus"); },
    []() { Serial.println("Mode long press"); });
  
  // Optional: Set initial display rotation
  // homewind_set_rotation(LV_DISP_ROT_0);    // Normal orientation
  // homewind_set_rotation(LV_DISP_ROT_180);  // Rotate 180 degrees
  // homewind_set_rotation(LV_DISP_ROT_90);   // Rotate 90 degrees
  // homewind_set_rotation(LV_DISP_ROT_270);  // Rotate 270 degrees
  lcd_lvgl_unlock();
  
  Serial.println("\nSetup complete!");
  Serial.println("Features:");
  Serial.println("- Tap the pink status bar or HR card to open the QR code modal");
  Serial.println("- Tap plus/minus, long-press the mode field");
  Serial.println("- Power save: dimmed after 8 s, screensaver 20 s later, panel off after 10 min");
  Serial.println("- Touch screen to wake from power save (that touch only wakes)");
  Serial.println("- Display rotation: Use homewind_set_rotation() or enable automatic IMU rotation");
}

void loop() {
  static unsigned long lastHRUpdate = 0;
  static unsigned long lastCSCUpdate = 0;
  static unsigned long lastFanUpdate = 0;
  static unsigned long lastRotationDemo = 0;
  lcd_lvgl_lock(-1);
  
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
        homewind_set_fan_state(2, FAN_STATE_NOT_CONFIGURATED);
        homewind_set_fan(3, FAN_STATE_ERROR, true);
        Serial.println("Fans: Mixed states");
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
  lcd_lvgl_unlock();
  
  delay(100);
}

