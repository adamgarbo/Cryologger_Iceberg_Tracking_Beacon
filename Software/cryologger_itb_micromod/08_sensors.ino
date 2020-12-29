// Configure attached sensors
void configureSensors() {

  // SparkFun BME280 Configuration
  if (bme280.beginI2C()) {
    bme280.setMode(MODE_SLEEP); // Enter sleep mode
    online.bme280 = true;
  }
  else {
    DEBUG_PRINTLN("Warning: SparkFun BME280 not detected at default I2C address! Please check wiring.");
    online.bme280 = false;
  }
}

// Read attached sensors
void readSensors() {

  if (online.bme280) {
    unsigned long loopStartTime = millis(); // Start the loop timer

    // Wake-up, take readings and re-enter sleep mode
    bme280.setMode(MODE_FORCED);
    blinkLed(1, 5); // Non-blocking delay to allow sensor to complete the reading
    float temperature = bme280.readTempC();
    float humidity = bme280.readFloatHumidity();
    float pressure = bme280.readFloatPressure();

    // Write data to union
    moMessage.temperature = temperature * 100;
    moMessage.humidity = humidity * 100;
    moMessage.pressure = pressure / 10;

    unsigned long loopEndTime = millis() - loopStartTime; // Stop loop timer
    //DEBUG_PRINT("readSenors() function execution: "); DEBUG_PRINT(loopEndTime); DEBUG_PRINTLN(" ms");
  }
  else {
    DEBUG_PRINTLN("Warning: BME280 offline!");
    return;
  }
}