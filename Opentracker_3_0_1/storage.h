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
typedef union STORED_SERVER_DATA_S {
    uint8_t blockData[(sizeof(uint32_t) + sizeof(GPSDATA_T) + 3) / 4];
    struct {
        uint32_t marker;    //!< One of the STORED_GPSDATA_xxx values
        SERVER_DATA_T serverData; //!< The actual server data
    };
} STORED_SERVER_DATA_T;
/**
 * The index data we maintain for the currently stored server data
 */
typedef struct STORED_SERVER_DATA_INDEX_S {
    bool storeValid;            //!< if false, we dont use the flash store
    size_t count;               //!< How many stored entries
    STORED_SERVER_DATA_T* pOldest;  //!< Oldest entry in the store
} STORED_SERVER_DATA_INDEX_T;

/**
 * Base class for managing server data which is persisted until such time
 * as we can send it to the server. To specialise this class for a specific
 * storage medium, subclass it and implement the abstract methods.
 */
class ServerDataStore {
public:
    ServerDataStore(size_t size) {
        this->storeSize = size;
        this->indexData.count = 0;
        this->indexData.storeValid = true;
        this->indexData.pOldest = NULL;
    }
    void init();
    bool writeServerData(const SERVER_DATA_T* pServerData);
    size_t getStoredServerDataCount();
    bool readOldestServerData(SERVER_DATA_T* pServerData);
    bool readOldestServerDataBlock(
        SERVER_DATA_T* pServerData,
        size_t dimServerData,
        size_t* pUsed);
    bool forgetOldestServerData(size_t count);
protected:
    size_t size() { return this->storeSize; }
    size_t maxRecordCount() { return size()/sizeof(STORED_SERVER_DATA_T); }
    virtual byte read(uint32_t address)=0;
    virtual byte* readAddress(uint32_t address)=0;
    virtual boolean write(uint32_t address, byte value)=0;
    virtual boolean write(uint32_t address, byte *data, uint32_t dataLength)=0;
    STORED_SERVER_DATA_T* getFirst();
    STORED_SERVER_DATA_T* getLast();
    STORED_SERVER_DATA_T* getNext(STORED_SERVER_DATA_T* pServerData);
    STORED_SERVER_DATA_T* getPrev(STORED_SERVER_DATA_T* pServerData);
    void scan();
    bool wipe();
    bool writeServerDataToStore(
        const STORED_SERVER_DATA_T* pServerDataLocation,
        unsigned long marker,
        const SERVER_DATA_T* pServerData);
private:
    /**
     * STORED_SERVER_DATA_T.marker values
     */
    static const uint32_t STORED_SERVER_DATA_START = 0x11111111;
    static const uint32_t STORED_SERVER_DATA_VALID = 0xAA557700;
    static const uint32_t STORED_SERVER_DATA_EMPTY = 0xFFFFFFFF;
    /**
     * The size of the storage in bytes
     */
    size_t storeSize;
    /**
     * Tracks what is currently stored
     */
    STORED_SERVER_DATA_INDEX_T indexData;
};

/**
 * Class which persists server data in RAM
 */
class RAMServerDataStore : public ServerDataStore {
public:
    RAMServerDataStore(size_t size) : ServerDataStore(size) {
        this->pRAMStore = (uint8_t*)malloc(size);
    }
protected:
    virtual byte read(uint32_t address) {
        return (address >= size()) ? 0 : this->pRAMStore[address];
    }
    virtual byte* readAddress(uint32_t address) {
        return (address >= size()) ? NULL : this->pRAMStore + address;
    }
    virtual boolean write(uint32_t address, byte value) {
        if (address < size()) {
            this->pRAMStore[address] = value;
            return true;
        }
        return false;
    }
    virtual boolean write(uint32_t address, byte *data, uint32_t dataLength) {
        if (address + dataLength <= size()) {
            memcpy(this->pRAMStore+address, data, dataLength);
            return true;
        }
        return false;
    }
private:
    uint8_t* pRAMStore;
};

/**
 * Class which persists server data in Flash using the DueFlashStarage
 * library
 */
class FlashServerDataStore : public ServerDataStore {
public:
    FlashServerDataStore(DueFlashStorage& dueFlashStorage, uint32_t start, size_t size)
        : ServerDataStore(size), flashStorage(dueFlashStorage) {
        this->start = start;
    }
protected:
    virtual byte read(uint32_t address) {
        return this->flashStorage.read(this->start + address);
    }
    virtual byte* readAddress(uint32_t address) {
        return this->flashStorage.readAddress(this->start + address);
    }
    virtual boolean write(uint32_t address, byte value) {
        return this->flashStorage.write(this->start + address, value);
    }
    virtual boolean write(uint32_t address, byte *data, uint32_t dataLength) {
        return this->flashStorage.write(this->start + address, data, dataLength);
    }
private:
    DueFlashStorage& flashStorage;
    uint32_t start;
};

