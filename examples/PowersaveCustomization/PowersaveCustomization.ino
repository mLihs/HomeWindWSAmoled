/*
 * PowersaveCustomization.ino
 * 
 * Advanced example demonstrating power save customization features:
 * - Custom breathing animation curves
 * - Manual power state control
 * - Brightness management
 * - Touch interaction handling
 * 
 * Hardware Requirements:
 * - ESP32 microcontroller
 * - SH8601 AMOLED display (280×456 pixels)
 * - FT3168 touch controller
 * 
 * Dependencies:
 * - LVGL library (must be installed)
 */

#include <HomeWindWSAmoled.h>

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("HomeWindWSAmoled Power Save Customization Example");
  
  // Initialize the library
  homewind_init();
  
  // Customize breathing animation curves
  // Set inhale to cubic ease-in (smooth acceleration)
  set_breathing_inhale_curve(BREATHING_EASE_CUBIC_IN);
  Serial.println("Breathing inhale: CUBIC_IN");
  
  // Set exhale to sine ease-out (natural deceleration)
  set_breathing_exhale_curve(BREATHING_EASE_SINE_OUT);
  Serial.println("Breathing exhale: SINE_OUT");
  
  // Alternative: Try different combinations
  // set_breathing_inhale_curve(BREATHING_EASE_QUADRATIC_IN);
  // set_breathing_exhale_curve(BREATHING_EASE_QUADRATIC_OUT);
  
  // Set initial states (new v1.4 API)
  homewind_set_hr(HR_STATE_ACTIVE, "TickrFit", 75);
  homewind_set_csc(CSC_STATE_ACTIVE, "Wahoo", 90);
  homewind_set_fan_state(0, FAN_STATE_ACTIVE);
  homewind_set_fan_state(1, FAN_STATE_ACTIVE);
  
  Serial.println("\nPower Save Behavior:");
  Serial.println("- After 10s inactivity: DIMMED state");
  Serial.println("- After 12s inactivity: SOFT_POWERSAVE (breathing animation)");
  Serial.println("- Touch screen to return to ACTIVE");
  Serial.println("\nTry different breathing curves by uncommenting lines above!");
}

void loop() {
  static unsigned long lastUpdate = 0;
  static unsigned long lastStateChange = 0;
  static bool manualControl = false;
  
  // Update HR value
  if (millis() - lastUpdate > 2000) {
    lastUpdate = millis();
    static uint16_t hrValue = 75;
    hrValue += random(-2, 3);
    if (hrValue < 60) hrValue = 60;
    if (hrValue > 180) hrValue = 180;
    homewind_set_hr_value(hrValue);  // Only update value (state/name unchanged)
  }
  
  // Demonstrate manual power state control (optional)
  // Uncomment to manually control power states
  /*
  if (millis() - lastStateChange > 15000) {
    lastStateChange = millis();
    manualControl = !manualControl;
    
    if (manualControl) {
      // Manually enter soft powersave
      set_state_soft_powersave();
      Serial.println("Manual: Entered SOFT_POWERSAVE");
    } else {
      // Return to active
      set_state_active();
      Serial.println("Manual: Returned to ACTIVE");
    }
  }
  */
  
  // Note: Normal operation uses automatic power save based on inactivity
  // The on_user_activity() function is called automatically on touch
  
  delay(100);
}

/*
 * Available Breathing Easing Types:
 * 
 * BREATHING_EASE_LINEAR              - Linear interpolation
 * BREATHING_EASE_QUADRATIC_IN        - Slow start, fast end (inhale)
 * BREATHING_EASE_QUADRATIC_OUT       - Fast start, slow end (exhale)
 * BREATHING_EASE_QUADRATIC_IN_OUT    - Slow start and end
 * BREATHING_EASE_CUBIC_IN            - Very slow start
 * BREATHING_EASE_CUBIC_OUT           - Very slow end
 * BREATHING_EASE_CUBIC_IN_OUT        - Very slow start and end
 * BREATHING_EASE_SINE_IN             - Smooth acceleration
 * BREATHING_EASE_SINE_OUT            - Smooth deceleration
 * BREATHING_EASE_SINE_IN_OUT         - Smooth start and end
 * 
 * Experiment with different combinations to find the most natural breathing effect!
 */

