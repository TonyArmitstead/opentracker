#include <limits.h>
//tracker config
#include "tracker.h" 

//External libraries
#include <TinyGPS.h>  
#include <avr/dtostrf.h>

#include <DueFlashStorage.h>

#ifdef DEBUG
#define debug_print(x)  debug_port.print(x)
#define debug_println(x)  debug_port.println(x)
#else
#define debug_print(x)
#define debug_println(x)
#endif

#define MAX(x, y) (((x) > (y)) ? (x) : (y))
#define MIN(x, y) (((x) < (y)) ? (x) : (y))
#define DIM(x) (sizeof(x)/sizeof(x[0]))

// Variables will change:
int ledState = LOW;             // ledState used to set the LED
unsigned long previousMillis = 0;        // will store last time LED was updated
unsigned long lastSMSSendTime = 0;
#if STORAGE
long logindex = STORAGE_DATA_START;
#endif
bool saveConfig = false;      //flag to save config to flash
bool powerReboot = false; //flag to reboot everything (used after new settings have been saved)
bool gsmRestart = false;
bool ignState = false;
bool engineRunning = false;
unsigned long engineRunningTime = 0;
unsigned long engineStartTime;
TinyGPS gps;
DueFlashStorage dueFlashStorage;
unsigned gsmFailureCount = 0;
SETTINGS_T config;
GPSDATA_T lastGoodGPSData;
GPSDATA_T lastReportedGPSData;
GPSDATA_T gpsData;
char serverData[DATA_LIMIT];
unsigned long lastServerUpdateTime;
unsigned long serverUpdatePeriod;
char modem_command[256];  // Modem AT command buffer
char modem_data[PACKET_SIZE]; // Modem TCP data buffer
char modem_reply[1024];    //data received from modem
/**
 * Controls the logging of commands/replies from the modem
 */
bool modemLogging = false;

//define serial ports 
#define gps_port Serial1
#define debug_port SerialUSB
#define gsm_port Serial2

void gsmConfigure() {
    // supply PIN code is needed
    gsm_set_pin();
    // get GSM IMEI
    gsm_get_imei();
    // misc GSM startup commands (disable echo)
    gsm_startup_cmd();
    // set GSM APN
    gsm_set_apn();
}

void sendBootMessage() {
    if (strlen(config.sms_send_number) != 0) {
        char timeStr[22];
        gsmGetTime(timeStr, DIM(timeStr));
        gsm_get_imei();
        char bootMsg[80];
        snprintf(bootMsg, DIM(bootMsg), "%s: System Booted IMEI=%s",
            timeStr, config.imei);
        sms_send_msg(bootMsg, config.sms_send_number);
    }
}

void setup() {
    //setting serial ports
    gsm_port.begin(115200);
    debug_port.begin(9600);
    gps_port.begin(9600);
    //setup led pin
    pinMode(PIN_POWER_LED, OUTPUT);
    digitalWrite(PIN_POWER_LED, LOW);
    pinMode(PIN_C_REBOOT, OUTPUT);
    digitalWrite(PIN_C_REBOOT, LOW);  //this is required
    debug_println(F("setup() started"));
    //blink software start
    blink_start();
    settings_load();
    //GPS setup 
    gps_setup();
    gps_on_off();
    //GSM setup
    gsm_setup();
    //turn on GSM
    gsm_restart();
    // Sync the command stream to the modem
    gsmSync();
    gsmConfigure();
    //get current log index
#if STORAGE
    storage_get_index();
#endif     
    //setup ignition detection
    pinMode(PIN_S_DETECT, INPUT);
    lastServerUpdateTime = millis();
    serverUpdatePeriod = config.fast_server_interval;
    sendBootMessage();
    lastGoodGPSData.fixAge = TinyGPS::GPS_INVALID_AGE;
    gpsData.fixAge = TinyGPS::GPS_INVALID_AGE;
    lastReportedGPSData.fixAge = TinyGPS::GPS_INVALID_AGE;
    debug_println(F("setup() completed"));
}

/**
 * Calculates the time difference in ms between two values returned from the
 * millis() call. This handles the case when the 32 bit timer value has
 * wrapped between the two values e.g.
 *                     ULONG_MAX-+0+-1
 *                               | |
 *                               v v
 * ...----------------------------0-----------------------...
 *                   ^                       ^
 *                   |                       |
 *        start_time-+***********************+-end_time
 */

unsigned long timeDiff(unsigned long end_time, unsigned long start_time) {
    if (end_time >= start_time) {
        return end_time - start_time;
    }
    return end_time + (ULONG_MAX - start_time) + 1;
}

void loop() {
    //start counting time
    unsigned long timeStart = millis();
    status_led();
    // Check if ignition is turned on
    ignState = (digitalRead(PIN_S_DETECT) == 0);
    if (ignState) {
        // Insert here only code that should be processed when Ignition is ON
        if (!engineRunning) {
            debug_println(F("Engine started"));
            // engine started, so record time started
            engineStartTime = millis();
            engineRunning = true;
        } else {
            // Update engine running time
            engineRunningTime = timeDiff(millis(), engineStartTime);
        }
    } else {
        // Insert here only code that should be processed when Ignition is OFF 
        if (engineRunning) {
            debug_println(F("Engine stopped"));
            engineRunning = false;
        }
    }
    // Process any SMS configuration requests
    sms_check();
    // Collect GPS data
    if (!readGPSData(&gpsData, 2000)) {
        debug_println(F("Failed to read GPS data"));
    } else {
        lastGoodGPSData = gpsData;
        if (lastReportedGPSData.fixAge != TinyGPS::GPS_INVALID_AGE) {
            // Inspect distance travelled
            float distance = gps.distance_between(
                gpsData.lat, gpsData.lon,
                lastReportedGPSData.lat, lastReportedGPSData.lon);
            // If we have travelled more then 100m use the fast update period
            if (distance > 100) {
                if (serverUpdatePeriod != config.fast_server_interval) {
                    debug_println(F("Switching to use fast update period"));
                    serverUpdatePeriod = config.fast_server_interval;
                }
            } else {
                if (serverUpdatePeriod != config.slow_server_interval) {
                    debug_println(F("Switching to use slow update period"));
                    serverUpdatePeriod = config.slow_server_interval;
                }
            }
        }
        // Is it time to update the server?
        unsigned long timeSinceLastServerUpdate = 
            timeDiff(timeStart, lastServerUpdateTime);
        if (timeSinceLastServerUpdate
            < 1000 * serverUpdatePeriod) {
            debug_print(F("Seconds to next server update = "));
            debug_println((1000 * serverUpdatePeriod - timeSinceLastServerUpdate)/1000);
        } else {
            debug_println(F("Is time to update server"));
            if (!formServerUpdateMessage(
                &gpsData, serverData, sizeof(serverData),
                ignState, engineRunningTime)) {
                debug_println(F("Failed to form server update message"));
            } else {
                if (!gsm_send_data(serverData)) {
                    debug_println(F("Failed to send server update message"));
                    if (++gsmFailureCount > 10) {
                        debug_println(F("Auto request of gsm restart"));
                        gsmFailureCount = 0;
                        gsmRestart = true;
                    }
                } else {
                    debug_println(F("Server updated OK"));
                    gsmFailureCount = 0;
                    lastServerUpdateTime = timeStart;
                    lastReportedGPSData = gpsData;
                }
            }
        }
    }
    // Check if sending SMS location updates
    if ((strlen(config.sms_send_number) != 0) &&
        (config.sms_send_interval != 0)) {
        if (timeDiff(timeStart, lastSMSSendTime)
            > 1000 * config.sms_send_interval) {
            if (lastGoodGPSData.fixAge != TinyGPS::GPS_INVALID_AGE) {
                debug_println(F("Was time to send SMS location but "
                        "no location data available"));
            } else {
                debug_println(F("Sending SMS location data"));
                char msg[MAX_SMS_MSG_LEN + 1];
                sms_form_sms_update_str(msg, DIM(msg), &lastGoodGPSData,
                    ignState);
                sms_send_msg(msg, config.sms_send_number);
                lastSMSSendTime = timeStart;
            }
        }
    }
    if (saveConfig) {
        debug_println(F("Saving config to flash"));
        settings_save();
        saveConfig = false;
    }
    if (gsmRestart) {
        debug_println(F("Restarting gsm modem"));
        gsm_restart();
        gsmSync();
        gsmConfigure();
        gsmRestart = false;
        sendBootMessage();
    }
    if (powerReboot) {
        debug_println(F("Rebooting"));
        reboot();
        powerReboot = false;
    }
}
