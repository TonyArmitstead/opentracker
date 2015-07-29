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
unsigned long watchdogMillis = 0;        // will store last time modem watchdog was reset

unsigned long time_start, time_stop; //count execution time to trigger interval  
int interval_count = 0; //current interval count (increased on each data collection and reset after sending)
unsigned long lastSMSSendTime = 0;
char data_current[DATA_LIMIT];   //data collected in one go, max 2500 chars 
int data_index = 0;        //current data index (where last data record stopped)
char time_char[20];             //time attached to every data line
char modem_command[256];  // Modem AT command buffer
char modem_data[PACKET_SIZE]; // Modem TCP data buffer
char modem_reply[200];    //data received from modem, max 200 chars   
long logindex = STORAGE_DATA_START;
byte save_config = 0;      //flag to save config to flash
byte power_reboot = 0; //flag to reboot everything (used after new settings have been saved)

char lat_current[32];
char lon_current[32];

unsigned long last_time_gps, last_date_gps;

int engineRunning = -1;
unsigned long engineRunningTime = 0;
unsigned long engine_start;

TinyGPS gps;
DueFlashStorage dueFlashStorage;

int gsm_send_failures = 0;

//settings structure  
struct settings {
    char apn[40];
    char user[20];
    char pwd[20];
    long interval;     //how often to collect data (milli sec, 600000 - 10 mins)
    int interval_send; //how many times to collect data before sending (times), sending interval interval*interval_send
    byte powersave;
    char key[12]; //key for connection, will be sent with every data transmission
    char sim_pin[5];        //PIN for SIM card
    char sms_key[MAX_SMS_KEY_LEN]; //password for SMS commands
    char imei[20];          //IMEI number
    unsigned long send_flags1; // Bit set of what data to send to server
    unsigned long sms_send_interval; // How often (in ms) to send SMS message containing location data
    char sms_send_number[MAX_PHONE_NUMBER_LEN+1];
};

settings config;

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
#ifdef STORAGE
    storage_get_index();
#endif     
    //setup ignition detection
    pinMode(PIN_S_DETECT, INPUT);
    //set to connect once started
    interval_count = config.interval_send;
    if (strlen(config.sms_send_number) != 0) {
        sms_send_msg("System Booted", config.sms_send_number);
    }
    debug_println(F("setup() completed"));
}

unsigned long time_diff(unsigned long end_time, unsigned long start_time) {
    if (end_time >= start_time) {
        return end_time - start_time;
    }
    return end_time + (ULONG_MAX-start_time) + 1;
}

void loop() {
    int IGNT_STAT;
    //start counting time
    time_start = millis();
    //debug       
    if (data_index >= DATA_LIMIT) {
        data_index = 0;
    }
    status_led();
    // Check if ignition is turned on
    IGNT_STAT = digitalRead(PIN_S_DETECT);
    debug_print(F("Ignition status: "));
    debug_println(IGNT_STAT);
    if (IGNT_STAT == 0) {
        if (engineRunning != 0) {
            // engine started
            engine_start = millis();
            engineRunning = 0;
        }
    } else {
        if (engineRunning != 1) {
            // engine stopped
            if (engineRunning == 0) {
                engineRunningTime += (millis() - engine_start);
            }
            engineRunning = 1;
        }
    }
    // Process any SMS configuration requests
    sms_check();
    if (ALWAYS_ON || IGNT_STAT == 0) {
        if (IGNT_STAT == 0) {
            debug_println(F("Ignition is ON!"));
            // Insert here only code that should be processed when Ignition is ON
        }
        //collecting GPS data
        if (SEND_RAW) {
            collect_all_data_raw(IGNT_STAT);
        } else {
            collect_all_data(IGNT_STAT);
        }
        debug_print(F("Current: "));
        debug_println(data_current);
        int i = gsm_send_data();
        if (i != 1) {
            debug_println(F("Can not send data"));
        } else {
            debug_println(F("Data sent successfully."));
        }
        //reset current data and counter
        data_index = 0;
    } else {
        debug_println(F("Ignition is OFF!"));
        // Insert here only code that should be processed when Ignition is OFF 
    }
    // Check if sending SMS location updates
    if ((strlen(config.sms_send_number) != 0) &&
        (config.sms_send_interval != 0)) {
        if (time_diff(time_start, lastSMSSendTime) > config.sms_send_interval) {
            if ((strlen(lat_current) == 0) ||
                (strlen(lon_current) == 0)) {
                debug_println(F("Was time to send SMS location but no location data available"));
            } else {
                debug_println(F("Sending SMS location data"));
                char msg[255];
                gps_form_location_url(msg, DIM(msg));
                sms_send_msg(msg, config.sms_send_number);
                lastSMSSendTime = time_start;
            }
        }
    }
    if (save_config == 1) {
        //config should be saved
        settings_save();
        save_config = 0;
    }
    if (power_reboot == 1) {
        //reboot unit
        reboot();
        power_reboot = 0;
    }
    if (!ENGINE_RUNNING_LOG_FAST_AS_POSSIBLE || IGNT_STAT != 0) {
        time_stop = millis();
        unsigned long loop_time = time_diff(time_stop, time_start);
        if (loop_time < config.interval) {
            unsigned long sleep_time = config.interval - loop_time;
            debug_print(F("Sleeping for: "));
            debug_print(sleep_time);
            debug_println(F("ms"));
            delay(sleep_time);
        }
    }
}
