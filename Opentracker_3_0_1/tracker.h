//OpenTracker config
#define DEBUG 1          //enable debug msg, sent to serial port
#define CR '\r'
#define LF '\n'
#define MAX_PHONE_NUMBER_LEN 16  // Max number of digits in a phone number

unsigned long led_interval = 1000; // interval at which to blink status led (milliseconds)
// millis() count for one second
#define ONE_SEC 1000
//default settings (can be overwritten and stored in EEPRom)
#define FAST_SERVER_INTERVAL 30      // how often, in s, to update the server
                                     // data when we are moving
#define SLOW_SERVER_INTERVAL (10*60) // how often, in s, to update the server
                                     // data when we are stopped
#define SMS_SEND_INTERVAL (24*60*60) // how often, in s, to send a location
                                     // SMS message
#define SMS_SEND_NUMBER ""           // The phone number to send the location
                                     // SMS message
#define KEY ""   //key for connection, will be sent with every data transmission
#define DATA_LIMIT 2500     //current data limit, data collected before sending to remote server can not exceed this
// SMS related definitions
#define MAX_SMS_MSG_LEN 144
#define MAX_SMS_KEY_LEN 12  // SMS key cant be longer than this
#define SMS_KEY "pass"      // default key for SMS command auth
// Macro to help with forming SMS_SEND bit data values
#define SMS_SEND(name, val) \
    ((SMS_SEND_##name##_##val & SMS_SEND_##name##_MASK) << SMS_SEND_##name##_POS)
// Definitions of bit positions and values for settings.SMS_send_flags
#define SMS_SEND_LOCATION_POS   0
#define SMS_SEND_LOCATION_MASK  1
#define     SMS_SEND_LOCATION_ON  1
#define     SMS_SEND_LOCATION_OFF 0
#define SMS_SEND_LOCATION_FORMAT_POS 1
#define SMS_SEND_LOCATION_FORMAT_MASK 0x03
#define     SMS_SEND_LOCATION_FORMAT_WEB 0
#define     SMS_SEND_LOCATION_FORMAT_MAP 1
#define     SMS_SEND_LOCATION_FORMAT_VAL 2
#define SMS_SEND_NSAT_POS 3
#define SMS_SEND_NSAT_MASK 1
#define     SMS_SEND_NSAT_ON  1
#define     SMS_SEND_NSAT_OFF 0
#define SMS_SEND_ALT_POS 4
#define SMS_SEND_ALT_MASK 1
#define     SMS_SEND_ALT_ON  1
#define     SMS_SEND_ALT_OFF 0
#define SMS_SEND_SPEED_POS 5
#define SMS_SEND_SPEED_MASK 1
#define     SMS_SEND_SPEED_ON  1
#define     SMS_SEND_SPEED_OFF 0
#define SMS_SEND_IGN_POS 6
#define SMS_SEND_IGN_MASK 1
#define     SMS_SEND_IGN_ON  1
#define     SMS_SEND_IGN_OFF 0
// Default value for settings.SMS_send_flags
#define SMS_SEND_DEFAULT \
    SMS_SEND(LOCATION, ON) | \
    SMS_SEND(LOCATION_FORMAT, MAP) | \
    SMS_SEND(NSAT, ON) | \
    SMS_SEND(ALT, ON) | \
    SMS_SEND(SPEED, ON) | \
    SMS_SEND(IGN, ON)

#define GSM_MODEM_COMMAND_TIMEOUT 20
#define GSM_SEND_FAILURES_REBOOT 0  // 0 == disabled, increase to set the number of GSM failures that will trigger a reboot of the opentracker device

#define ENGINE_RUNNING_LOG_FAST_AS_POSSIBLE 0 // when the engine is running, log position as fast as possible

#define SEND_RAW 0 // enable to use the new raw tcp send method to minimise data use
#define SEND_RAW_INCLUDE_KEY 1
#define SEND_RAW_INCLUDE_TIMESTAMP 0

// Macro to help with forming SERVER_SEND bit data values
#define SERVER_SEND(name, val) \
    ((SERVER_SEND_##name##_##val & SERVER_SEND_##name##_MASK) << SERVER_SEND_##name##_POS)
// Definitions of bit positions and values for settings.server_send_flags
#define SERVER_SEND_GPSDATE_POS   0
#define SERVER_SEND_GPSDATE_MASK  1
#define     SERVER_SEND_GPSDATE_ON  1
#define     SERVER_SEND_GPSDATE_OFF 0
#define SERVER_SEND_GPSTIME_POS   1
#define SERVER_SEND_GPSTIME_MASK  1
#define     SERVER_SEND_GPSTIME_ON  1
#define     SERVER_SEND_GPSTIME_OFF 0
#define SERVER_SEND_LATITUDE_POS   2
#define SERVER_SEND_LATITUDE_MASK  1
#define     SERVER_SEND_LATITUDE_ON  1
#define     SERVER_SEND_LATITUDE_OFF 0
#define SERVER_SEND_LONGITUDE_POS   3
#define SERVER_SEND_LONGITUDE_MASK  1
#define     SERVER_SEND_LONGITUDE_ON  1
#define     SERVER_SEND_LONGITUDE_OFF 0
#define SERVER_SEND_SPEED_POS   4
#define SERVER_SEND_SPEED_MASK  1
#define     SERVER_SEND_SPEED_ON  1
#define     SERVER_SEND_SPEED_OFF 0
#define SERVER_SEND_ALTITUDE_POS   5
#define SERVER_SEND_ALTITUDE_MASK  1
#define     SERVER_SEND_ALTITUDE_ON  1
#define     SERVER_SEND_ALTITUDE_OFF 0
#define SERVER_SEND_HEADING_POS   6
#define SERVER_SEND_HEADING_MASK  1
#define     SERVER_SEND_HEADING_ON  1
#define     SERVER_SEND_HEADING_OFF 0
#define SERVER_SEND_HDOP_POS   7
#define SERVER_SEND_HDOP_MASK  1
#define     SERVER_SEND_HDOP_ON  1
#define     SERVER_SEND_HDOP_OFF 0
#define SERVER_SEND_NSAT_POS   8
#define SERVER_SEND_NSAT_MASK  1
#define     SERVER_SEND_NSAT_ON  1
#define     SERVER_SEND_NSAT_OFF 0
#define SERVER_SEND_BATT_POS   9
#define SERVER_SEND_BATT_MASK  1
#define     SERVER_SEND_BATT_ON  1
#define     SERVER_SEND_BATT_OFF 0
#define SERVER_SEND_IGN_POS   10
#define SERVER_SEND_IGN_MASK  1
#define     SERVER_SEND_IGN_ON  1
#define     SERVER_SEND_IGN_OFF 0
#define SERVER_SEND_RUNTIME_POS   11
#define SERVER_SEND_RUNTIME_MASK  1
#define     SERVER_SEND_RUNTIME_ON  1
#define     SERVER_SEND_RUNTIME_OFF 0

// Default value for settings.SERVER_send_flags
#define SERVER_SEND_DEFAULT \
    SERVER_SEND(GPSDATE, ON) | \
    SERVER_SEND(GPSTIME, ON) | \
    SERVER_SEND(LATITUDE, ON) | \
    SERVER_SEND(LONGITUDE, ON) | \
    SERVER_SEND(SPEED, ON) | \
    SERVER_SEND(ALTITUDE, ON) | \
    SERVER_SEND(HEADING, ON) | \
    SERVER_SEND(HDOP, ON) | \
    SERVER_SEND(NSAT, ON) | \
    SERVER_SEND(BATT, OFF) | \
    SERVER_SEND(IGN, OFF) | \
    SERVER_SEND(RUNTIME, OFF)

#define HOSTNAME "updates.geolink.io"
#define PROTO "TCP"
#define HTTP_PORT "80"
#define URL "/index.php"

const char HTTP_HEADER1[] =
    "POST /index.php  HTTP/1.0\r\nHost: updates.geolink.io\r\nContent-type: application/x-www-form-urlencoded\r\nContent-length:"; //HTTP header line before length
const char HTTP_HEADER2[] =
    "\r\nUser-Agent:OpenTracker3.0\r\nConnection: close\r\n\r\n"; //HTTP header line after length

#define PACKET_SIZE 1400    //TCP data chunk size, modem accept max 1460 bytes per send
#define PACKET_SIZE_DELIVERY 3000    //in case modem has this number of bytes undelivered, wait till sending new data (3000 bytes default, max sending TCP buffer is 7300)

#define CONNECT_RETRY 5    //how many time to retry connecting to remote server

/**
 * Definition of data collected from each gps update
 */
typedef struct GPSDATA_S {
	unsigned long fixAge;  // age of this fix in ms or TinyGPS::GPS_INVALID_AGE
    float lat;              // latitude
    float lon;              // longitude
    float alt;              // altitude in meters (+/-)
    float course;           // course/direction in degrees
    float speed;            // speed in km/h
    unsigned long hdop;    // horizontal dilution of precision in 100ths
    unsigned long time;    // GPS time
    unsigned long date;    // GPS date
    unsigned short nsats;  // number of satellites
} GPSDATA_T;
/**
 * Definition of the data set we send to the server
 */
typedef struct SERVER_DATA_S {
    GPSDATA_T gpsData;  //!< The actual gps data
    bool ignState;     //!< State of the ignition switch
    unsigned long engineRuntime; //<! How long engine has been running
} SERVER_DATA_T;
/**
 * Definition of the configuration settings
 */
typedef struct SETTINGS_S {
    char apn[40];           // GPS APN string
    char user[20];          // GPS user name
    char pwd[20];           // GPS password
    unsigned short slow_server_interval; // slow server update interval in s
    unsigned short fast_server_interval; // fast server update interval in s
    char key[12]; //key for connection, will be sent with every data transmission
    char sim_pin[5];        //PIN for SIM card
    char sms_key[MAX_SMS_KEY_LEN+1]; // password for SMS commands
    char imei[20];          // IMEI number
    unsigned long server_send_flags; // Bit set of what data to send to server
    unsigned long sms_send_interval; // How often (in ms) to send SMS message containing location data
    char sms_send_number[MAX_PHONE_NUMBER_LEN+1];
    unsigned long sms_send_flags; // Bit set of what data to send in SMS message
} SETTINGS_T;

