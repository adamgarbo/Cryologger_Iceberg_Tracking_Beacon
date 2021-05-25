// Read battery voltage from voltage divider
void readBattery()
{
  // Start loop timer
  unsigned long loopStartTime = millis();

  int reading = 0;
  byte samples = 10;

  for (byte i = 0; i < samples; ++i)
  {
    reading += analogRead(PIN_VBAT); // Read VIN across a 1/10 MΩ resistor divider
    myDelay(1);
  }

  // External battery
  //float voltage = (float)reading / samples * 3.3 * ((R2 + R1) / R2) / 4096.0; // Convert 1/10 VIN to VIN (12-bit resolution)

  // LiPo
  float voltage = (float)reading / samples * 3.3 * 2 / 4096.0; 


  // Write data to union
  moSbdMessage.voltage = voltage * 1000;

  /*
      // Write minimum battery voltage value to union
      if (moSbdMessage.voltage == 0)
      {
      moSbdMessage.voltage = voltage * 1000;
      } else if ((voltage * 1000) < moSbdMessage.voltage)
      {
      moSbdMessage.voltage = voltage * 1000;
      }
  */
  // Stop loop timer
  timer.battery = millis() - loopStartTime;
}

// Disable serial port
void disableSerial()
{
#if DEBUG
  SERIAL_PORT.end(); // Close serial port
  USBDevice.detach(); // Safely detach USB prior to sleeping
#endif
}

// Enable serial port
void enableSerial()
{
#if DEBUG
  USBDevice.attach(); // Re-attach USB
  SERIAL_PORT.begin(115200);
  //myDelay(3000); // Non-blocking delay to allow user to open Serial Monitor
#endif
}

// Enable power to IMU
void enableImuPower()
{
  digitalWrite(PIN_IMU_EN, HIGH);
  myDelay(500);
}

// Disable power to IMU
void disableImuPower()
{
  digitalWrite(PIN_IMU_EN, LOW); 
}

// Enable power to sensors
void enableSensorPower()
{
  digitalWrite(PIN_SENSOR_EN, HIGH);
  myDelay(500);
}

// Disable power to sensors
void disableSensorPower()
{
  digitalWrite(PIN_SENSOR_EN, LOW); 
}

// Enable power to GPS
void enableGpsPower()
{
  digitalWrite(PIN_GPS_EN, LOW);
}

// Disable power to GPS
void disableGpsPower()
{
  digitalWrite(PIN_GPS_EN, HIGH);
}

// Enable power to RockBLOCK 9603
void enableIridiumPower()
{
  digitalWrite(PIN_IRIDIUM_EN, HIGH);
}

// Disable power to RockBLOCK 9603
void disableIridiumPower()
{
  digitalWrite(PIN_IRIDIUM_EN, LOW);
}

// 
void prepareForSleep()
{
  // Disable serial
  disableSerial();

  // Clear online union
  memset(&online, 0, sizeof(online));

  // Clear timer union
  memset(&timer, 0, sizeof(timer));
}

// Enter deep sleep
void goToSleep()
{
  // Clear first-time flag after initial power-down
  if (firstTimeFlag)
  {
    firstTimeFlag = false;
  }

  // Enter deep sleep
  //LowPower.deepSleep();

  // Sleep until next alarm match
  rtc.standbyMode();

  /* Code sleeps here and awaits RTC or WDT interrupt */
}

// Wake from deep sleep
void wakeUp()
{
  // Enable serial port
  enableSerial();
}

// Non-blocking blink LED (https://forum.arduino.cc/index.php?topic=503368.0)
void blinkLed(byte ledFlashes, unsigned int ledDelay)
{
  byte i = 0;
  while (i < ledFlashes * 2)
  {
    unsigned long currentMillis = millis();
    if (currentMillis - previousMillis >= ledDelay)
    {
      digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
      previousMillis = currentMillis;
      i++;
    }
  }
  // Ensure LED is off at end of blink cycle
  digitalWrite(LED_BUILTIN, LOW);
}

// Non-blocking delay (milliseconds)
// https://arduino.stackexchange.com/questions/12587/how-can-i-handle-the-millis-rollover
void myDelay(unsigned long ms)
{
  unsigned long start = millis(); // Start: timestamp
  for (;;)
  {
    petDog();                            // Reset watchdog timer
    unsigned long now = millis();        // Now: timestamp
    unsigned long elapsed = now - start; // Elapsed: duration
    if (elapsed >= ms)                   // Comparing durations: OK
      return;
  }
}
