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
byte saveConfig = 0;      //flag to save config to flash
byte powerReboot = 0; //flag to reboot everything (used after new settings have been saved)
bool ignState = false;
bool engineRunning = false;
unsigned long engineRunningTime = 0;
unsigned long engineStartTime;
TinyGPS gps;
DueFlashStorage dueFlashStorage;
int gsm_send_failures = 0;
SETTINGS_T config;
GPSDATA_T lastGoodGPSData;
GPSDATA_T lastReportedGPSData;
GPSDATA_T gpsData;
char serverData[DATA_LIMIT];
unsigned long lastServerUpdateTime;
unsigned long serverUpdatePeriod;

//define serial ports 
#define gps_port Serial1
#define debug_port SerialUSB
#define gsm_port Serial2

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
    //send AT
    gsm_send_at();
    gsm_send_at();
    //supply PIN code is needed 
    gsm_set_pin();
    //get GSM IMEI
    gsm_get_imei();
    //misc GSM startup commands (disable echo)
    gsm_startup_cmd();
    //set GSM APN
    gsm_set_apn();
    //get current log index
#if STORAGE
    storage_get_index();
#endif     
    //setup ignition detection
    pinMode(PIN_S_DETECT, INPUT);
    lastServerUpdateTime = millis();
    serverUpdatePeriod = config.fast_server_interval;
    if (strlen(config.sms_send_number) != 0) {
        sms_send_msg("System Booted", config.sms_send_number);
    }
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
    return end_time + (ULONG_MAX-start_time) + 1;
}

void loop() {
    //start counting time
    unsigned long time_start = millis();
    status_led();
    // Check if ignition is turned on
    ignState = (digitalRead(PIN_S_DETECT) == 0);
    if (ignState) {
        // Insert here only code that should be processed when Ignition is ON
        debug_println(F("Ignition is ON!"));
        if (!engineRunning) {
            // engine started, so record time started
            engineStartTime = millis();
            engineRunning = true;
        } else {
            // Update engine running time
            engineRunningTime = millis() - engineStartTime;
        }
    } else {
        debug_println(F("Ignition is OFF!"));
        // Insert here only code that should be processed when Ignition is OFF 
        if (engineRunning) {
            engineRunning = false;
        }
    }
    // Process any SMS configuration requests
    sms_check();
    // Collect GPS data
    if (read_gps_data(&gpsData, 2000)) {
    	lastGoodGPSData = gpsData;
    	if (lastReportedGPSData.fixAge != TinyGPS::GPS_INVALID_AGE) {
    		// Inspect distance travelled
    		float distance = gps.distance_between(
    			gpsData.lat, gpsData.lon,
				lastReportedGPSData.lat, lastReportedGPSData.lon);
    		// If we have travelled more then 100m use the fast update period
    		if (distance > 100) {
    		    serverUpdatePeriod = config.fast_server_interval;
    		} else {
    		    serverUpdatePeriod = config.slow_server_interval;
    		}
    	}
    	// Is it time to update the server?
    	if (timeDiff(time_start, lastServerUpdateTime) > 1000*serverUpdatePeriod) {
			if (form_server_update_message(
				&gpsData, serverData, sizeof(serverData),
                ignState, engineRunningTime)) {
				if (gsm_send_data(serverData)) {
					lastServerUpdateTime = time_start;
					lastReportedGPSData = gpsData;
				}
			}
        }
    }
    // Check if sending SMS location updates
    if ((strlen(config.sms_send_number) != 0) &&
        (config.sms_send_interval != 0)) {
        if (timeDiff(time_start, lastSMSSendTime) > 1000*config.sms_send_interval) {
        	if (lastGoodGPSData.fixAge != TinyGPS::GPS_INVALID_AGE) {
                debug_println(F("Was time to send SMS location but no location data available"));
            } else {
                debug_println(F("Sending SMS location data"));
                char msg[MAX_SMS_MSG_LEN+1];
                sms_form_sms_update_str(msg, DIM(msg), &lastGoodGPSData, ignState);
                sms_send_msg(msg, config.sms_send_number);
                lastSMSSendTime = time_start;
            }
        }
    }
    if (saveConfig == 1) {
        //config should be saved
        settings_save();
        saveConfig = 0;
    }
    if (powerReboot == 1) {
        //reboot unit
        reboot();
        powerReboot = 0;
    }
}
