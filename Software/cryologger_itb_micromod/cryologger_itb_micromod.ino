/*
    Title:    Cryologger Ice Tracking Beacon (ITB) v3.0 Prototype
    Date:     December 24, 2020
    Author:   Adam Garbo

    Components:
    - SparkFun Artemis Processor
    - SparkFun MicroMod Data Logging Carrier Board
    - SparkFun GPS Breakout - SAM-M8Q (Qwiic)
    - SparkFun Qwiic Iridium 9603N
    - Maxtena M1621HCT-P-SMA Iridium antenna
    - SparkFun Buck-Boost Converter

    Comments:
*/

// ----------------------------------------------------------------------------
// Libraries
// ----------------------------------------------------------------------------
#include <IridiumSBD.h>                     // http://librarymanager/All#IridiumSBDI2C
#include <RTC.h>
#include <SparkFunBME280.h>                 // http://librarymanager/All#SparkFun_BME280_Arduino_Library
#include <SparkFun_Ublox_Arduino_Library.h> // http://librarymanager/All#SparkFun_Ublox_GPS
#include <SdFat.h>                          // http://librarymanager/All#SdFat
#include <SPI.h>
#include <WDT.h>
#include <Wire.h>

// ----------------------------------------------------------------------------
// Debugging macros
// ----------------------------------------------------------------------------
#define DEBUG       true

#if DEBUG
#define DEBUG_PRINT(x)            Serial.print(x)
#define DEBUG_PRINTLN(x)          Serial.println(x)
#define DEBUG_PRINT_HEX(x)        Serial.print(x, HEX)
#define DEBUG_PRINTLN_HEX(x)      Serial.println(x, HEX)
#define DEBUG_PRINT_DEC(x, y)     Serial.print(x, y)
#define DEBUG_PRINTLN_DEC(x, y)   Serial.println(x, y)
#define DEBUG_WRITE(x)            Serial.write(x)

#else
#define DEBUG_PRINT(x)
#define DEBUG_PRINTLN(x)
#define DEBUG_PRINT_HEX(x)
#define DEBUG_PRINTLN_HEX(x)
#define DEBUG_PRINT_DEC(x, y)
#define DEBUG_PRINTLN_DEC(x, y)
#define DEBUG_WRITE(x)
#endif

// ----------------------------------------------------------------------------
// Pin definitions
// ----------------------------------------------------------------------------
#define PIN_PWC_POWER           G1
#define PIN_QWIIC_POWER         G2
#define PIN_MICROSD_CHIP_SELECT 41

// ----------------------------------------------------------------------------
// Object instantiations
// ----------------------------------------------------------------------------
APM3_RTC      rtc;
APM3_WDT      wdt;
BME280        bme280;         // I2C Address: 0x77
IridiumSBD    modem(Wire);    // I2C Address: 0x63
SdFat         sd;             // File system object
SdFile        file;           // Log file
SFE_UBLOX_GPS gps;            // I2C Address: 0x42

// ----------------------------------------------------------------------------
// User defined global variable declarations
// ----------------------------------------------------------------------------
byte          alarmSeconds          = 0;
byte          alarmMinutes          = 5;
byte          alarmHours            = 0;
unsigned int  transmitInterval      = 1;    // Number of messages to transmit in each Iridium transmission (340 byte limit)
unsigned int  maxRetransmitCounter  = 10;    // Number of failed data transmissions to reattempt (340 byte limit)

// Global variable and constant declarations
volatile bool alarmFlag           = false;  // RTC alarm ISR flag
volatile bool watchdogFlag        = false;  // Watchdog Timer ISR flag
volatile int  watchdogCounter     = 0;      // Watchdog Timer interrupt counter
bool          ledState            = LOW;    // LED blink flag
bool          rtcSyncFlag         = false;  // RTC synchronization flag
bool          resetFlag           = 0;      // Force system reset flag

byte          gnssFixCounter      = 0;      // GNSS valid fix counter
byte          gnssFixCounterMax   = 5;      // GNSS max valid fix counter
uint8_t       transmitBuffer[340] = {};     // Iridium 9603 transmission buffer (SBD MO message max: 340 bytes)
char          fileName[30]        = "";     // Keep a record of this file name so that it can be re-opened upon wakeup from sleep
char          outputData[512];              // Factor of 512 for easier recording to SD in 512 chunks
unsigned int  sdPowerDelay        = 250;    // Delay (in milliseconds) before disabling power to microSD
unsigned int  qwiicPowerDelay     = 2000;   // Delay (in milliseconds) after enabling power to Qwiic connector
unsigned int  messageCounter      = 0;      // Iridium 9603 cumualtive transmission counter (zero indicates a reset)
unsigned int  retransmitCounter   = 0;      // Iridium 9603 failed transmission counter
unsigned int  transmitCounter     = 0;      // Iridium 9603 transmission interval counter
unsigned long previousMillis      = 0;      // Global millis() timer

// ----------------------------------------------------------------------------
// Data transmission unions/structures
// ----------------------------------------------------------------------------
// Union to store and transmit Iridium SBD Mobile Originated (MO) message
typedef union {
  struct {
    uint32_t  unixtime;           // UNIX Epoch time                (4 bytes)
    int16_t   temperature;        // Temperature (°C)               (2 bytes)
    uint16_t  humidity;           // Humidity (%)                   (2 bytes)
    uint16_t  pressure;           // Pressure (Pa)                  (2 bytes)
    int32_t   latitude;           // Latitude (DD)                  (4 bytes)
    int32_t   longitude;          // Longitude (DD)                 (4 bytes)
    uint8_t   satellites;         // # of satellites                (1 byte)
    uint16_t  pdop;               // PDOP                           (2 bytes)
    int16_t   rtcDrift;           // RTC offset from GNSS time      (2 bytes)
    uint16_t  voltage;            // Battery voltage (V)            (2 bytes)
    uint16_t  transmitDuration;   // Previous transmission duration (2 bytes)
    uint16_t  messageCounter;     // Message counter                (2 bytes)
  } __attribute__((packed));                                        // Total: (29 bytes)
  uint8_t bytes[29];
} SBD_MO_MESSAGE;

SBD_MO_MESSAGE moMessage;

// Union to receive Iridium SBD Mobile Terminated (MT) message
typedef union {
  struct {
    uint32_t  alarmInterval;      // (4 bytes)
    uint16_t  transmitInterval;   // (4 bytes)
    uint16_t  retransmitCounter;  // (4 bytes)
    uint8_t   resetFlag;          // (1 byte)
  } __attribute__((packed));      // Total: (13 bytes)
  uint8_t bytes[13]; // Size of message to be received (in bytes)
} SBD_MT_MESSAGE;

SBD_MT_MESSAGE mtMessage;

// Union to store device online/offline states
struct struct_online {
  bool bme280 = false;
  bool microSd = false;
  bool iridium = false;
  bool gnss = false;
} online;

// ----------------------------------------------------------------------------
// Setup
// ----------------------------------------------------------------------------
void setup() {

  // Pin assignments
  pinMode(PIN_QWIIC_POWER, OUTPUT);
  pinMode(PIN_PWC_POWER, OUTPUT);
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);

  qwiicPowerOn(); // Enable power to Qwiic connector
  peripheralPowerOn(); // Enable power to peripherials

  Wire.begin(); // Initialize I2C
  SPI.begin(); // Initialize SPI

  Serial.begin(115200);
  //while (!Serial); // Wait for user to open Serial Monitor
  delay(5000); // Delay to allow user to open Serial Monitor

  printLine();
  DEBUG_PRINTLN("Cryologger Iceberg Tracking Beacon");
  printLine();

  DEBUG_PRINT("Datetime: "); printDateTime();

  configureSd();      // Configure microSD
  configureGnss();    // Configure Sparkfun SAM-M8Q
  configureIridium(); // Configure SparkFun Qwiic Iridium 9603N
  configureRtc();     // Configure real-time clock (RTC)
  syncRtc();          // Synchronize RTC with GNSS
  configureWdt();     // Configure and start Watchdog Timer
  configureSensors(); // Configure attached sensors
  createLogFile();
  Serial.flush(); // Wait for transmission of any serial data to complete
}

// ----------------------------------------------------------------------------
// Loop
// ----------------------------------------------------------------------------
void loop() {

  // Check if alarm flag was set
  if (alarmFlag) {
    alarmFlag = false; // Clear alarm flag

    DEBUG_PRINT("Alarm trigger: "); printDateTime();
    
    setRtcAlarm();  // Set the next RTC alarm

    // Perform measurements
    readRtc();      // Read RTC
    readSensors();  // Read attached sensors
    readGnss();     // Read GNSS
    writeBuffer();  // Write the data to transmit buffer
    logData();      // Write data to SD
    transmitData(); // Transmit data

  }

  // Check for watchdog interrupt
  if (watchdogFlag) {
    petDog();
  }

  // Blink LED
  blinkLed(1, 25);

  // Enter deep sleep and await RTC alarm interrupt
  goToSleep();
}

// ----------------------------------------------------------------------------
// Interupt Service Routines (ISR)
// ----------------------------------------------------------------------------

// Interrupt handler for the RTC
extern "C" void am_rtc_isr(void) {

  // Clear the RTC alarm interrupt
  am_hal_rtc_int_clear(AM_HAL_RTC_INT_ALM);

  // Set alarm flag
  alarmFlag = true;
}

// Interrupt handler for the watchdog
extern "C" void am_watchdog_isr(void) {

  // Clear the watchdog interrupt
  wdt.clear();

  // Perform system reset after 15 watchdog interrupts (should not occur)
  if (watchdogCounter < 25 ) {
    wdt.restart(); // "Pet" the dog
  }
  watchdogFlag = true; // Set the watchdog flag
  watchdogCounter++; // Increment watchdog interrupt counter
}
