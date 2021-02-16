/*
    Title:    Cryologger Ice Tracking Beacon (ITB) v3.0 Prototype
    Date:     January 13, 2020
    Author:   Adam Garbo

    Components:
    - SparkFun Artemis Processor
    - SparkFun MicroMod Data Logging Carrier Board
    - SparkFun GPS Breakout - SAM-M8Q (Qwiic)
    - SparkFun Qwiic Iridium 9603N
    - Maxtena M1621HCT-P-SMA Iridium antenna
    - SparkFun Buck-Boost Converter

    Comments:
    Serial.flush() will crash the program when DEBUG is enabled
*/

// ----------------------------------------------------------------------------
// Libraries
// ----------------------------------------------------------------------------

#include <ICM_20948.h>                            // https://github.com/sparkfun/SparkFun_ICM-20948_ArduinoLibrary
#include <IridiumSBD.h>                           // https://github.com/sparkfun/SparkFun_IridiumSBD_I2C_Arduino_Library
#include <RTC.h>
#include <SparkFunBME280.h>                       // https://github.com/sparkfun/SparkFun_BME280_Arduino_Library
#include <SparkFun_ADS1015_Arduino_Library.h>     // https://github.com/sparkfun/SparkFun_ADS1015_Arduino_Library
#include <SparkFun_u-blox_GNSS_Arduino_Library.h> // https://github.com/sparkfun/SparkFun_Ublox_Arduino_Library
#include <SdFat.h>                                // https://github.com/greiman/SdFat
#include <SPI.h>
#include <TimeLib.h>                              // https://github.com/PaulStoffregen/Time
#include <WDT.h>
#include <Wire.h>                                 // https://www.arduino.cc/en/Reference/Wire

// -----------------------------------------------------------------------------
// Debugging macros
// -----------------------------------------------------------------------------
#define DEBUG           true   // Output debug messages to Serial Monitor
#define DEBUG_GNSS      true   // Output GNSS information to Serial Monitor
#define DEBUG_IRIDIUM   false   // Output Iridium debug messages to Serial Monitor

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
#define PIN_VBAT          A0
#define PIN_PWC_POWER     33 // G1
#define PIN_QWIIC_POWER   34 // G2
#define PIN_SD_CS         41 // CS

// ----------------------------------------------------------------------------
// Object instantiations
// ----------------------------------------------------------------------------
ADS1015           adc;            // I2C address: 0x48
APM3_RTC          rtc;
APM3_WDT          wdt;
BME280            bme280;         // I2C address: 0x77
IridiumSBD        modem(Wire);    // I2C address: 0x63
SdFs              sd;             // File system object
FsFile            file;           // Log file
SFE_UBLOX_GNSS    gnss;           // I2C address: 0x42

// ----------------------------------------------------------------------------
// User defined global variable declarations
// ----------------------------------------------------------------------------
unsigned long alarmInterval         = 3600;  // Sleep duration in seconds
byte          alarmSeconds          = 0;
byte          alarmMinutes          = 5;
byte          alarmHours            = 0;
unsigned int  transmitInterval      = 1;    // Number of messages to transmit in each Iridium transmission (340 byte limit)
unsigned int  retransmitCounterMax  = 1;    // Number of failed data transmissions to reattempt (340 byte limit)
unsigned int  gnssTimeout           = 300;   // Timeout for GNSS signal acquisition (s)
int           iridiumTimeout        = 300;   // Timeout for Iridium transmission (s)

// ----------------------------------------------------------------------------
// Global variable declarations
// ----------------------------------------------------------------------------
const float   R1                  = 99800.0; // Voltage divider resistor 1
const float   R2                  = 9914.0; // Voltage divider resistor 2
volatile bool alarmFlag           = false;  // Flag for alarm interrupt service routine
volatile bool watchdogFlag        = false;  // Flag for Watchdog Timer interrupt service routine
volatile int  watchdogCounter     = 0;      // Watchdog Timer interrupt counter
bool          firstTimeFlag       = true;   // Flag to determine if the program is running for the first time
bool          resetFlag           = 0;      // Flag to force system reset using Watchdog Timer
bool          gnssFixFlag         = false;  // Flag to indicate if GNSS valid fix has been acquired
bool          rtcSyncFlag         = false;  // Flag to indicate if the RTC was syned with the GNSS
byte          gnssFixCounter      = 0;      // Counter for valid GNSS fixes
byte          gnssFixCounterMax   = 30;     // Counter limit for threshold of valid GNSS fixes
uint8_t       transmitBuffer[340] = {};     // Iridium 9603 transmission buffer (SBD MO message max: 340 bytes)
char          fileName[30]        = "";     // Keep a record of this file name so that it can be re-opened upon wakeup from sleep
unsigned int  sdPowerDelay        = 250;    // Delay before disabling power to microSD (milliseconds)
unsigned int  qwiicPowerDelay     = 2000;   // Delay after enabling power to Qwiic connector (milliseconds)
unsigned int  messageCounter      = 0;      // Iridium 9603 cumualtive transmission counter (zero indicates a reset)
byte          retransmitCounter   = 0;      // Iridium 9603 failed transmission counter
byte          transmitCounter     = 0;      // Iridium 9603 transmission interval counter
unsigned long previousMillis      = 0;      // Global millis() timer
float         voltage             = 0.0;    // Battery voltage
unsigned long unixtime            = 0;
time_t        alarmTime           = 0;

// ----------------------------------------------------------------------------
// Data transmission unions/structures
// ----------------------------------------------------------------------------
// Union to store and transmit Iridium SBD Mobile Originated (MO) message
typedef union
{
  struct
  {
    uint32_t  unixtime;           // UNIX Epoch time                (4 bytes)
    int16_t   temperature;        // Temperature (°C)               (2 bytes)
    uint16_t  humidity;           // Humidity (%)                   (2 bytes)
    uint16_t  pressure;           // Pressure (Pa)                  (2 bytes)
    int32_t   latitude;           // Latitude (DD)                  (4 bytes)
    int32_t   longitude;          // Longitude (DD)                 (4 bytes)
    uint8_t   satellites;         // # of satellites                (1 byte)
    uint16_t  pdop;               // PDOP                           (2 bytes)
    int16_t   rtcDrift;           // RTC offset from GNSS time      (4 bytes)
    uint16_t  voltage;            // Battery voltage (V)            (2 bytes)
    uint16_t  transmitDuration;   // Previous transmission duration (2 bytes)
    uint16_t  messageCounter;     // Message counter                (2 bytes)
  } __attribute__((packed));                              // Total: (31 bytes)
  uint8_t bytes[29];
} SBD_MO_MESSAGE;

SBD_MO_MESSAGE moMessage;

// Union to receive Iridium SBD Mobile Terminated (MT) message
typedef union
{
  struct
  {
    unsigned long alarmInterval;      // (4 bytes)
    byte          transmitInterval;   // (1 byte)
    byte          retransmitCounter;  // (1 byte)
    byte          resetFlag;          // (1 byte)
  };
  uint8_t bytes[7]; // Size of message to be received (in bytes)
} SBD_MT_MESSAGE;

SBD_MT_MESSAGE mtMessage;

// Union to store device online/offline states
struct struct_online
{
  bool adc      = false;
  bool bme280   = false;
  bool microSd  = false;
  bool iridium  = false;
  bool gnss     = false;
} online;

// Union to store loop timers
struct struct_timer
{
  unsigned long adc;
  unsigned long voltage;
  unsigned long rtc;
  unsigned long microSd;
  unsigned long sensor;
  unsigned long gnss;
  unsigned long iridium;
} timer;

// ----------------------------------------------------------------------------
// Setup
// ----------------------------------------------------------------------------
void setup()
{
  // Pin assignments
  pinMode(PIN_QWIIC_POWER, OUTPUT);
  pinMode(PIN_PWC_POWER, OUTPUT);
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);

  // Set analog resolution to 14-bits
  analogReadResolution(14);

  qwiicPowerOn();       // Enable power to Qwiic connector
  peripheralPowerOn();  // Enable power to peripherials

  Wire.begin(); // Initialize I2C
  Wire.setClock(100000); // Set I2C clock speed to 400 kHz

  SPI.begin(); // Initialize SPI

#if DEBUG
  Serial.begin(115200);
  //while (!Serial); // Wait for user to open Serial Monitor
  blinkLed(2, 1000); // Non-blocking delay to allow user to open Serial Monitor
#endif

  DEBUG_PRINTLN();
  printLine();
  DEBUG_PRINTLN("Cryologger Iceberg Tracking Beacon v3.0");
  printLine();
  printDateTime();

  // Configure devices
  configureGnss();    // Configure GNSS receiver
  readGnss();         // Acquire fix and synchronize RTC with GNSS
  configureIridium(); // Configure SparkFun Qwiic Iridium 9603N
  configureWdt();     // Configure and start Watchdog Timer (WDT)
  configureSd();      // Configure microSD
  configureSensors(); // Configure attached sensors
  createLogFile();    // Create initial log file
  configureRtc();     // Configure initial real-time clock (RTC) alarm

  DEBUG_PRINT("Datetime: "); printDateTime();
  DEBUG_PRINT("Initial alarm: "); printAlarm();

  // Blink LED to indicate completion of setup
  blinkLed(10, 100);
}

// ----------------------------------------------------------------------------
// Loop
// ----------------------------------------------------------------------------
void loop()
{
  // Check if alarm flag is set or if program is running for the first time
  if (alarmFlag || firstTimeFlag)
  {
    // Clear alarm flag
    alarmFlag = false;

    DEBUG_PRINT("Alarm trigger: "); printDateTime();

    // If program is running for the first time
    if (firstTimeFlag)
    {
      readRtc(); // Read RTC
    }

    // Perform measurements
    readSensors();  // Read attached sensors
    readGnss();     // Read GNSS
    logData();      // Write data to SD
    writeBuffer();  // Write the data to transmit buffer
    transmitData(); // Transmit data
    printTimers();  // Print function execution timers
    setRtcAlarm();  // Set the next RTC alarm
  }

  // Check for watchdog interrupt
  if (watchdogFlag)
  {
    petDog();
  }

  // Blink LED
  blinkLed(1, 100);

  // Enter deep sleep and await RTC alarm interrupt
  goToSleep();
}

// ----------------------------------------------------------------------------
// Interupt Service Routines (ISR)
// ----------------------------------------------------------------------------

// Interrupt handler for the RTC
extern "C" void am_rtc_isr(void)
{
  // Clear the RTC alarm interrupt
  //rtc.clearInterrupt();
  am_hal_rtc_int_clear(AM_HAL_RTC_INT_ALM);

  // Set alarm flag
  alarmFlag = true;
}

// Interrupt handler for the watchdog timer
extern "C" void am_watchdog_isr(void)
{
  // Clear the watchdog interrupt
  wdt.clear();

  // Perform system reset after 10 watchdog interrupts (should not occur)
  if (watchdogCounter < 10 )
  {
    wdt.restart(); // "Pet" the dog
  }
  watchdogFlag = true; // Set the watchdog flag
  watchdogCounter++; // Increment watchdog interrupt counter
}
