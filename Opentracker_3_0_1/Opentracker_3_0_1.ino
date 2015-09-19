#include <limits.h>
#include <stdint.h>
#include <TinyGPS.h>  
#include <avr/dtostrf.h>
#include <DueFlashStorage.h>
#include "tracker.h"
#include "storage.h"
#include "secrets.h"

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
bool saveConfig = false;      //flag to save config to flash
bool powerReboot = false; //flag to reboot everything (used after new settings have been saved)
bool gsmRestart = false;
bool ignState = false;
bool engineRunning = false;
unsigned long engineRunningTime = 0;
unsigned long engineStartTime;
TinyGPS gps;
DueFlashStorage dueFlashStorage;
unsigned long gsmFailedToUpdateTime = 0;
SETTINGS_T config;
GPSDATA_T lastGoodGPSData;
GPSDATA_T lastReportedGPSData;
GPSDATA_T gpsData;
char serverMsg[DATA_LIMIT];
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

/**
 * Safe string copy to copy a string with limited length and guaranteed
 * '\0' termination
 * @param pDst destination buffer to write the string into
 * @param pSrc the string source
 * @param sz the size of the destination buffer
 */
void strncopy(
    char* pDst,
    const char* pSrc,
    size_t sz
) {
    strncpy(pDst, pSrc, sz);
    pDst[sz-1] = '\0';
}

/**
 * Sends a boot up message to the configured SMS phone (if any)
 */
void sendBootMessage() {
    if (strlen(config.sms_send_number) != 0) {
        char timeStr[22];
        if (!gsmGetTime(timeStr, DIM(timeStr), SECS(5))) {
            debug_println(F("sendBootMessage(): Could not read modem time"));
            strncopy(timeStr, BAD_TIME, DIM(timeStr));
        }
        char imeiStr[IMEI_LEN+1];
        if (!gsmGetIMEI(imeiStr, DIM(imeiStr), SECS(5))) {
            debug_println(F("sendBootMessage(): Could not read modem IMEI"));
            strncopy(imeiStr, BAD_IMEI, DIM(imeiStr));
        }
        char bootMsg[80];
        snprintf(bootMsg, DIM(bootMsg), "%s: System Booted IMEI=%s",
            timeStr, imeiStr);
        sms_send_msg(bootMsg, config.sms_send_number);
    }
}

/*
 * Sends an SMS message indicating that the GSM network connection has been
 * restarted
 * @param networkStatus the network status prior to us restarting the GSN
 *        device
 */
void sendGSMRestartMessage(
    GSMSTATUS_T networkStatus
) {
    if (strlen(config.sms_send_number) != 0) {
        char timeStr[22];
        if (!gsmGetTime(timeStr, DIM(timeStr), SECS(5))) {
            strncopy(timeStr, BAD_TIME, DIM(timeStr));
        }
        char imeiStr[IMEI_LEN+1];
        if (!gsmGetIMEI(imeiStr, DIM(imeiStr), SECS(5))) {
            strncopy(imeiStr, BAD_IMEI, DIM(imeiStr));
        }
        char bootMsg[80];
        snprintf(bootMsg, DIM(bootMsg), "%s: GSM Restart IMEI=%s nStat=%s",
            timeStr, imeiStr, gsmGetNetworkStatusString(networkStatus));
        sms_send_msg(bootMsg, config.sms_send_number);
    }
}

/**
 * Gets the gsm modem powered up and ready for operation
 */
void powerUpGSMModem() {
    gsmIndicatePowerOn();
    if (!gsmPowerOn()) {
        debug_println(F("powerUpGSMModem() Did not manage to power on gsm modem"));
        // Yikes! What else can we do?
        reboot();
    } else {
        // Sync the command stream to the modem
        gsmSyncComms(SECS(5));
        // Setup the modem
        gsmConfigure();
        // Wait up to 60s for a gsm connection
        if (!gsmWaitForConnection(SECS(60))) {
            debug_println(F("powerUpGSMModem() did not get a gsm connection"));
        } else {
            // Wait up to 60s for time sync
            char timeStr[22];
            if (!gsmGetTime(timeStr, DIM(timeStr), SECS(60))) {
                debug_println(F("powerUpGSMModem() did not get time sync"));
            }
        }
    }
}

void setup() {
    powerReboot = false;
    //setting serial ports
    gsm_port.begin(115200);
    debug_port.begin(9600);
    gps_port.begin(9600);
    debug_println(F("setup(): Initialising system"));
    //setup led pin
    pinMode(PIN_POWER_LED, OUTPUT);
    digitalWrite(PIN_POWER_LED, LOW);
    pinMode(PIN_C_REBOOT, OUTPUT);
    digitalWrite(PIN_C_REBOOT, LOW);  //this is required
    //blink software start
    blink_start();
    settings_load();
    //GPS setup 
    gps_setup();
    gps_on_off();
    //GSM setup
    gsmSetupPIO();
    // turn on GSM
    powerUpGSMModem();
    // Initialise server data flash storage
    storageServerDataInit();
    //setup ignition detection
    pinMode(PIN_S_DETECT, INPUT);
    lastServerUpdateTime = millis();
    serverUpdatePeriod = config.fast_server_interval;
    lastGoodGPSData.fixAge = TinyGPS::GPS_INVALID_AGE;
    gpsData.fixAge = TinyGPS::GPS_INVALID_AGE;
    lastReportedGPSData.fixAge = TinyGPS::GPS_INVALID_AGE;
    sendBootMessage();
    debug_println(F("setup(): System initialisation complete"));
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

void ignitionCheck() {
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
}

void gpsCheck() {
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
    }
}

bool sendMessageToServer(
    SERVER_DATA_T* pServerData
) {
    bool sentStatus = false;
    if (formServerUpdateMessage(
            pServerData, serverMsg, sizeof(serverMsg))) {
        sentStatus = gsm_send_data(serverMsg);
    }
    return sentStatus;
}


GSMSTATUS_T sendStoredMessagesToServer(
    GSMSTATUS_T networkStatus
) {
    unsigned storedMessagesDelivered = 0;
    SERVER_DATA_T serverData;
    unsigned long timeNow = millis();
    // Spend up to 2 mins sending any old GPS data we stored whilst
    // there was no GSM connection
    while ((networkStatus == CONNECTED) &&
            storageReadOldestServerData(&serverData) &&
            (timeDiff(millis(), timeNow) < 120*ONE_SEC)) {
        if (sendMessageToServer(&serverData)) {
            if (storageForgetOldestServerData()) {
                storedMessagesDelivered += 1;
            }
        }
        networkStatus = gsmGetNetworkStatus();
    }
    if (storedMessagesDelivered > 0) {
        debug_print(F("Sent "));
        debug_print(storedMessagesDelivered);
        debug_println(F(" stored messages to server"));
    }
    return networkStatus;
}

bool updateServerWithCurrentData(
    SERVER_DATA_T* pServerData
) {
    bool serverUpdatedStatus = false;
    if (!sendMessageToServer(pServerData)) {
        debug_println(F("Failed to send server update message"));
        unsigned long timeNow = millis();
        if (gsmFailedToUpdateTime == 0) {
            gsmFailedToUpdateTime = timeNow;
        } else if (timeDiff(timeNow, gsmFailedToUpdateTime) > MINS(10)) {
            debug_println(F("Failed to update server, scheduling gsm restart"));
            gsmFailedToUpdateTime = 0;
            gsmRestart = true;
        }
    } else {
        gsmFailedToUpdateTime = 0;
        serverUpdatedStatus = true;
    }
    return serverUpdatedStatus;
}

void serverUpdateCheck() {
    GSMSTATUS_T networkStatus = gsmGetNetworkStatus();
    // If we have network connection and have stored messages then send
    // them (or at least some of them) now
    if ((networkStatus == CONNECTED) &&
        (storageGetStoredServerDataCount() > 0)) {
        networkStatus = sendStoredMessagesToServer(networkStatus);
    }
    // Is it time to update the server with current data?
    unsigned long timeNow = millis();
    unsigned long timeSinceLastServerUpdate =
        timeDiff(timeNow, lastServerUpdateTime);
    if (timeSinceLastServerUpdate < SECS(serverUpdatePeriod)) {
        // No, not time to update server
        debug_print(F("Seconds to next server update = "));
        debug_print((SECS(serverUpdatePeriod) - timeSinceLastServerUpdate)/ONE_SEC);
        debug_print(F(", Network Status = "));
        debug_println(gsmGetNetworkStatusString(networkStatus));
    } else {
        // Yes, we are due a server update
        debug_println(F("It is time to update server"));
        bool shouldReportData = true;
        if (lastGoodGPSData.fixAge == TinyGPS::GPS_INVALID_AGE) {
            debug_println(F("But dont have current GPS data"));
            shouldReportData = false;
        } else if (memcmp(&lastReportedGPSData,
                          &lastGoodGPSData, sizeof(GPSDATA_T)) == 0) {
            debug_println(F("But GPS data has not changed since last update"));
            shouldReportData = false;
        }
        if (shouldReportData) {
            SERVER_DATA_T serverData;
            serverData.gpsData = lastGoodGPSData;
            serverData.ignState = ignState;
            serverData.engineRuntime = engineRunningTime;
            bool serverUpdatedOK = false;
            if (networkStatus == CONNECTED) {
                serverUpdatedOK = updateServerWithCurrentData(&serverData);
            }
            if (serverUpdatedOK) {
                debug_println(F("Server updated OK"));
            } else {
                debug_println(F("Server update failed so storing to flash"));
                if (storageWriteServerData(&serverData)) {
                    serverUpdatedOK = true;
                } else {
                    debug_println(F("Store to flash failed"));
                }
            }
            if (serverUpdatedOK) {
                // Note we record the data as reported even if we only stored it
                // to flash, it will eventually get reported and this assignment
                // prevents us repeatedly writing to the flash (or server)
                lastReportedGPSData = lastGoodGPSData;
                lastServerUpdateTime = timeNow;
            }
        }
    }
}

void smsNotificationCheck() {
    unsigned long timeNow = millis();
    if (timeDiff(timeNow, lastSMSSendTime)
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
            lastSMSSendTime = timeNow;
        }
    }
}

void loop() {
    GSMSTATUS_T networkStatus = gsmGetNetworkStatus();
    status_led();
    // Ignition status update
    ignitionCheck();
    // Process any SMS configuration requests
    smsRequestCheck();
    // Collect GPS data
    gpsCheck();
    // Server update
    serverUpdateCheck();
    // SMS notification update
    if ((strlen(config.sms_send_number) != 0) &&
        (config.sms_send_interval != 0)) {
        smsNotificationCheck();
    }
    if (saveConfig) {
        debug_println(F("Saving config to flash"));
        settings_save();
        saveConfig = false;
    }
    if (gsmRestart) {
        debug_println(F("Restarting gsm modem"));
        gsmPowerOff();
        powerUpGSMModem();
        gsmRestart = false;
        sendGSMRestartMessage(networkStatus);
    }
    if (powerReboot) {
        debug_println(F("Rebooting"));
        reboot();
        powerReboot = false;
    }
}
