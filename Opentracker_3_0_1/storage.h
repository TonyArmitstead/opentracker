/**
 * Definition of how the settings are stored into flash. We wrap the settings
 * to enable us to detect the validity of the saved settings and to detect
 * any change to the settings.
 */
typedef struct STORED_SETTINGS_S {
    uint32_t marker;        //!< Marks the start of a valid settings in flash
    size_t settingsSize;    //!< The size of the settings when last stored
    SETTINGS_T settings;    //!< The actual settings values
    uint32_t crc32;         //!< CRC of settings record
} STORED_SETTINGS_T;
/**
 * Definition of how the GPS data is stored into flash.
 * Note that we need the length of each stored record to divisible by 4
 */
typedef union STORED_GPSDATA_S {
    uint8_t blockData[(sizeof(uint32_t) + sizeof(GPSDATA_T) + 3) / 4];
    struct {
        uint32_t marker;   //!< One of the STORED_GPSDATA_xxx values
        GPSDATA_T gpsData; //!< The actual gps data
    };
} STORED_GPSDATA_T;
/**
 * The index data we maintain for the currently stored GPS data
 */
typedef struct STORED_GPSDATA_INDEX_S {
    bool storeValid;            //!< if false, we dont use the flash store
    size_t count;               //!< How many stored entries
    STORED_GPSDATA_T* pOldest;  //!< Oldest entry in the store
} STORED_GPSDATA_INDEX_T;
