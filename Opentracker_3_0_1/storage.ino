/**
 * Flash layout is:
 *
 * +----------------------+ +0x00000000
 * |                      |  ^
 * |  Config settings     |  1K
 * |                      |  v
 * +----------------------+ +0x00000400 (+1K)
 * |                      |  ^
 * |  Location data       |  |
 * |  stored whilst no    |  |
 * |  GSM connection      | 64K
 * |  available           |  |
 * |                      |  |
 * |                      |  v
 * +----------------------+ +0x00014000 (+65K)
 * |                      |
 * |                      |
 * |       Unused         |
 * | (reserved for future |
 * |  use)                |
 * |                      |
 * +----------------------+ +0x00040000 (+256K)
 *
 */
/**
 *  Offset into flash where we store the configuration settings
 */
#define STORAGE_SETTINGS_OFFSET 0
/**
 * STORED_SETTINGS_T.marker value
 */
#define SETTINGS_VALID 0xAA557700

/**
 * Calculates the 32 bit CRC of a memory block
 * @param pData points to the data to calculate the CRC for
 * @param length the number of bytes in the data block
 * @return the 32 bit CRC of the data block
 */
uint32_t calcCRC32(
    const void* pData,
    size_t length
) {
    uint32_t byteVal, mask;
    unsigned char* pMsg = (unsigned char*)pData;
    uint32_t crc = 0xFFFFFFFF;
    while (length-- != 0) {
        byteVal = *pMsg++;
       crc = crc ^ byteVal;
       for (int j = 7; j >= 0; j--) {
          mask = -(crc & 1);
          crc = (crc >> 1) ^ (0xEDB88320 & mask);
       }
    }
    return ~crc;
}

/**
 * Saves the configuration settings to flash
 * @param pSettings the settings to save
 * @return true if saved OK, false if not
 */
bool storageSaveSettings(
    const SETTINGS_T* pSettings
) {
    bool rStat = false;
    // Form record to save to flash - including a CRC
    STORED_SETTINGS_T savedSettings;
    savedSettings.marker = SETTINGS_VALID;
    savedSettings.settingsSize = sizeof(SETTINGS_T);
    memcpy(&savedSettings.settings, pSettings, sizeof(SETTINGS_T));
    savedSettings.crc32 = calcCRC32(&savedSettings.settings,
                                    sizeof(SETTINGS_T));
    // Write it to flash
    if (!dueFlashStorage.write(
            STORAGE_SETTINGS_OFFSET,
            (byte*)&savedSettings,
            (uint32_t)sizeof(savedSettings))) {
        debug_println(F("storageSaveSettings: failed to save settings to flash"));
    } else {
        // Verify flash now matches saved record
        const SETTINGS_T* pSavedSettings =
            (const SETTINGS_T*)dueFlashStorage.readAddress(
                STORAGE_SETTINGS_OFFSET);
        if (memcmp(&savedSettings, pSavedSettings, sizeof(SETTINGS_T)) != 0) {
            debug_println(F("storageSaveSettings: failed to verify settings in flash"));
        } else {
            // All is good
            rStat = true;
        }
    }
    return rStat;
}

/**
 * Loads the configuration settings from flash
 * @param pSettings where to write the retrieved settings
 * @return true if read OK, false if not
 */
bool storageLoadSettings(
    SETTINGS_T* pSettings
) {
    bool rStat = false;
    STORED_SETTINGS_T savedSettings;
    memcpy(&savedSettings,
           dueFlashStorage.readAddress(STORAGE_SETTINGS_OFFSET),
           sizeof(savedSettings));
    if (savedSettings.marker != SETTINGS_VALID) {
        debug_println(F("storageLoadSettings: detected bad .marker"));
    } else if (savedSettings.settingsSize != sizeof(SETTINGS_T)) {
        debug_println(F("storageLoadSettings: detected bad .settingsSize"));
    } else if (savedSettings.crc32 != calcCRC32(&savedSettings.settings,
                                                 sizeof(SETTINGS_T))) {
        debug_println(F("storageLoadSettings: detected bad .crc32"));
    } else {
        memcpy(pSettings, &savedSettings.settings, sizeof(SETTINGS_T));
        rStat = true;
    }
    return rStat;
}

/**
 * Gets a pointer to the first block of stored server data
 * @return a pointer to the first block of stored server data
 */
STORED_SERVER_DATA_T* ServerDataStore::getFirst(
) {
    return (STORED_SERVER_DATA_T*)readAddress(0);
}

/**
 * Gets a pointer to the last block of stored server data
 * @return a pointer to the last block of stored server data
 */
STORED_SERVER_DATA_T* ServerDataStore::getLast(
) {
    return getFirst() + size()/sizeof(STORED_SERVER_DATA_T) - 1;
}

/**
 * Given a pointer to an existing stored server data block we return a pointer
 * to the following data block. This takes in to account any wrap at the end
 * of the storage area.
 * @param pServerData a pointer to a server data block in the storage area
 * @return a pointer to the record which follows pServerData, taking in to
 *         account any buffer wrap
 */
STORED_SERVER_DATA_T* ServerDataStore::getNext(
    STORED_SERVER_DATA_T* pServerData
) {
    if (pServerData == getLast()) {
        return getFirst();
    }
    return pServerData + 1;
}

/**
 * Given a pointer to an existing stored server data block we return a pointer
 * to the previous data block. This takes in to account any wrap at the start
 * of the storage area.
 * @param pServerData a pointer to a server data block in the storage area
 * @return a pointer to the record which follows pServerData, taking in to
 *         account any buffer wrap
 */
STORED_SERVER_DATA_T* ServerDataStore::getPrev(
    STORED_SERVER_DATA_T* pServerData
) {
    if (pServerData == getFirst()) {
        return getLast();
    }
    return pServerData - 1;
}

/**
 * Checks the flash store is valid and assigns the flash index data
 */
void ServerDataStore::scan() {
    STORED_SERVER_DATA_T* pServerFirst = getFirst();
    STORED_SERVER_DATA_T* pServerData = pServerFirst;
    this->indexData.count = 0;
    this->indexData.storeValid = true;
    this->indexData.pOldest = NULL;
    bool scanComplete = false;
    // Scan the complete store looking for a _single_ oldest record and
    // checking for presence of invalid marker values (indicating corrupt store)
    while (!scanComplete) {
        switch (pServerData->marker) {
        case STORED_SERVER_DATA_START:
            // Have located the oldest record
            if (this->indexData.pOldest == NULL) {
                debug_println(F("storageServerScan: located oldest record"));
                // Record 1st oldest marker record
                this->indexData.pOldest = pServerData;
                this->indexData.count += 1;
            } else {
                debug_println(F("storageServerScan: found multiple oldest records!"));
                // There can be only one
                this->indexData.storeValid = false;
                scanComplete = true;
            }
            break;
        case STORED_SERVER_DATA_VALID:
            this->indexData.count += 1;
            break;
        case STORED_SERVER_DATA_EMPTY:
            break;
        default:
            debug_print(F("storageServerScan: found corrupt store at index: "));
            debug_println(pServerData - pServerFirst);
            // Invalid marker = duff flash (or maybe we have changed the record
            // format)
            this->indexData.storeValid = false;
            scanComplete = true;
            break;
        }
        pServerData = getNext(pServerData);
        // Detect back at start i.e. all done
        if (pServerData == pServerFirst) {
            scanComplete = true;
        }
    }
    if (this->indexData.storeValid) {
        if (this->indexData.pOldest == NULL) {
            // Didnt find a start (oldest) record, so should not have seen
            // and valid records
            if (this->indexData.count > 0) {
                this->indexData.storeValid = false;
            } else {
                // Store is empty so we choose the oldest location as the start
                this->indexData.pOldest = getFirst();
            }
        } else {
            // Did find a start (oldest) record, so check we got a consecutive
            // sequence of valid records following it. This checks we see
            // something like EEEESVVVEEEE and not EEEESVVVEVEE.
            pServerFirst = this->indexData.pOldest;
            pServerData = pServerFirst;
            size_t validCount = 0;
            scanComplete = false;
            while (!scanComplete) {
                switch (pServerData->marker) {
                case STORED_SERVER_DATA_START:
                case STORED_SERVER_DATA_VALID:
                    validCount += 1;
                    break;
                case STORED_SERVER_DATA_EMPTY:
                default:
                    scanComplete = true;
                    break;
                }
                pServerData = getNext(pServerData);
                if (pServerData == pServerFirst) {
                    scanComplete = true;
                }
            }
            if (validCount != this->indexData.count) {
                this->indexData.storeValid = false;
            }
        }
    }
}

/**
 * Erases the data store. This effectively marks the store full of
 * STORED_SERVER_DATA_EMPTY markers.
 * @return true if the store wiped OK, false if not
 */
bool ServerDataStore::wipe() {
    bool wipeOK = true;
    byte wipeData[256];
    memset(wipeData, 0xFF, sizeof(wipeData));

    size_t wipeLeft = size();
    size_t wipeOffset = 0;
    while (wipeOK && (wipeLeft > 0)) {
        size_t wipeSize = MIN(sizeof(wipeData), wipeLeft);
        if (!write(wipeOffset, wipeData, wipeSize)) {
            debug_println(F("storageWipeServerData: failed to wipe flash"));
            wipeOK = false;
        } else {
            wipeLeft -= wipeSize;
            wipeOffset += wipeSize;
        }
    }
    return wipeOK;
}

/**
 * Writes a GPS record to flash
 * @param pServerDataLocation points to where in the store to write to
 * @param marker the marker to use for this record
 * @param pServerData the server data record to write
 * @return true if written OK, false if not
 */
bool ServerDataStore::writeServerDataToStore(
    const STORED_SERVER_DATA_T* pServerDataLocation,
    unsigned long marker,
    const SERVER_DATA_T* pServerData
) {
    STORED_SERVER_DATA_T storedServerData;
    storedServerData.marker = marker;
    storedServerData.serverData = *pServerData;
    size_t byteOffset = (pServerDataLocation - getFirst()) *
                        sizeof(STORED_SERVER_DATA_T);
    return write(byteOffset,
        (byte*)&storedServerData, sizeof(STORED_SERVER_DATA_T));
}

/**
 * Call after boot to initialise our storage index data and to check the
 * flash content is valid. If flash is not valid we erase it all and
 * prepare it for use.
 */
void ServerDataStore::init() {
    scan();
    if (!this->indexData.storeValid) {
        debug_println(F("storageServerDataInit: wiping flash"));
        this->indexData.count = 0;
        this->indexData.pOldest = getFirst();
        this->indexData.storeValid = wipe();
    } else {
        debug_print(F("storageServerDataInit: flash is OK and holding "));
        debug_print(this->indexData.count);
        debug_println(F(" records"));
    }
}

/**
 * Writes a server data block to storage.
 * @param pServerData the server data block to write
 * @return true if stored OK, false if not
 */
bool ServerDataStore::writeServerData(
    const SERVER_DATA_T* pServerData
) {
    bool writtenOK = false;
    if (this->indexData.storeValid) {
        if (this->indexData.count == 0) {
            // Store is empty so use first slot
            STORED_SERVER_DATA_T* pStoredServerData = getFirst();
            writtenOK = writeServerDataToStore(
                            pStoredServerData, STORED_SERVER_DATA_START,
                            pServerData);
            this->indexData.count = 1;
            this->indexData.pOldest = pStoredServerData;
        } else if (this->indexData.count == maxRecordCount()) {
            // Store is full so overwrite oldest slot
            STORED_SERVER_DATA_T* pStoredServerData = this->indexData.pOldest;
            writtenOK = writeServerDataToStore(
                            pStoredServerData, STORED_SERVER_DATA_VALID,
                            pServerData);
            // Mark the following slot as the oldest
            pStoredServerData = getNext(pStoredServerData);
            writtenOK = writtenOK && writeServerDataToStore(
                           pStoredServerData, STORED_SERVER_DATA_START,
                           &pStoredServerData->serverData);
            this->indexData.pOldest = pStoredServerData;
        } else {
            // Locate next free slot as oldest+count
            STORED_SERVER_DATA_T* pStoredServerData = this->indexData.pOldest;
            for (size_t idx=0; idx < this->indexData.count; ++idx) {
                pStoredServerData = getNext(pStoredServerData);
            }
            writtenOK = writeServerDataToStore(
                            pStoredServerData, STORED_SERVER_DATA_VALID,
                            pServerData);
            this->indexData.count += 1;
        }
        if (!writtenOK) {
            debug_println(F("storageSaveServerData: failed to update flash"));
            this->indexData.storeValid = false;
            debug_println(F("storageSaveServerData: marked flash as invalid"));
        }
    }
    return writtenOK;
}

size_t ServerDataStore::getStoredServerDataCount() {
    if (this->indexData.storeValid) {
        return this->indexData.count;
    }
    return 0;
}

/**
 * Returns the oldest GPS data record from flash
 * @param pServerData where to write the GPS data
 * @return true if pServerData assigned ok, false if there is no data to return
 */
bool ServerDataStore::readOldestServerData(
    SERVER_DATA_T* pServerData
) {
    if (!this->indexData.storeValid)
        return false;
    if (this->indexData.count == 0)
        return false;
    *pServerData = this->indexData.pOldest->serverData;
    return true;
}

/**
 * Returns the oldest GPS data record from flash
 * @param pServerData an array into which we write the server data
 * @param dimServerData the size of the pServerData array
 * @param pUsed assigned the number of array entries assigned
 * @return true if pServerData assigned ok, false if there is no data to return
 */
bool ServerDataStore::readOldestServerDataBlock(
    SERVER_DATA_T* pServerData,
    size_t dimServerData,
    size_t* pUsed
) {
    if (!this->indexData.storeValid)
        return false;
    if (this->indexData.count == 0)
        return false;
    size_t usedEntries = 0;
    STORED_SERVER_DATA_T* pStoredData = this->indexData.pOldest;
    while (usedEntries < dimServerData) {
        *pServerData++ = pStoredData->serverData;
        pStoredData = getNext(pStoredData);
        ++usedEntries;
    }
    if (pUsed) {
        *pUsed = usedEntries;
    }
    return true;
}

/**
 * Removes the oldest server data record(s) from flash
 * @param the number to server data entries to remove
 * @return true if removed OK, false if no GPS data in flash or we failed
 *         to update the flash
 */
bool ServerDataStore::forgetOldestServerData(
    size_t count
) {
    bool writtenOK = false;
    if (this->indexData.storeValid && (this->indexData.count != 0)) {
        STORED_SERVER_DATA_T* pStoredData = this->indexData.pOldest;
        while (count-- && this->indexData.count) {
            // Mark this slot as empty
            writtenOK = writeServerDataToStore(
                            pStoredData, STORED_SERVER_DATA_EMPTY,
                            &pStoredData->serverData);
            pStoredData = getNext(pStoredData);
            this->indexData.count -= 1;
        }
        if (this->indexData.count > 0) {
            // Mark the last slot as the 'new' oldest
            writtenOK = writtenOK && writeServerDataToStore(
                           pStoredData, STORED_SERVER_DATA_START,
                           &pStoredData->serverData);
        }
        this->indexData.pOldest = pStoredData;
    }
    return writtenOK;
}

/*
 * If the server storage block had space for 4 entries, these diagrams show all
 * possible valid states of the storage area.
 *   E = empty, S = start (oldest), V = Valid
 * When adding, scanning or removing entries make sure your algorithm copes
 * with all these states.
 *
 *   0   1   2   3
 * +---+---+---+---+
 * | E | E | E | E | serverDataIndex.count=0, serverDataIndex.oldestIdx=x
 * +---+---+---+---+
 *
 * +---+---+---+---+
 * | S | E | E | E | serverDataIndex.count=1, serverDataIndex.oldestIdx=0
 * +---+---+---+---+
 *
 * +---+---+---+---+
 * | E | S | E | E | serverDataIndex.count=1, serverDataIndex.oldestIdx=1
 * +---+---+---+---+
 *
 * +---+---+---+---+
 * | E | E | S | E | serverDataIndex.count=1, serverDataIndex.oldestIdx=2
 * +---+---+---+---+
 *
 * +---+---+---+---+
 * | E | E | E | S | serverDataIndex.count=1, serverDataIndex.oldestIdx=3
 * +---+---+---+---+
 *
 * +---+---+---+---+
 * | S | V | E | E | serverDataIndex.count=2, serverDataIndex.oldestIdx=0
 * +---+---+---+---+
 *
 * +---+---+---+---+
 * | E | S | V | E | serverDataIndex.count=2, serverDataIndex.oldestIdx=1
 * +---+---+---+---+
 *
 * +---+---+---+---+
 * | E | E | S | V | serverDataIndex.count=2, serverDataIndex.oldestIdx=2
 * +---+---+---+---+
 *
 * +---+---+---+---+
 * | V | E | E | S | serverDataIndex.count=2, serverDataIndex.oldestIdx=3
 * +---+---+---+---+
 *
 * +---+---+---+---+
 * | S | V | V | E | serverDataIndex.count=3, serverDataIndex.oldestIdx=0
 * +---+---+---+---+
 *
 * +---+---+---+---+
 * | E | S | V | V | serverDataIndex.count=3, serverDataIndex.oldestIdx=1
 * +---+---+---+---+
 *
 * +---+---+---+---+
 * | V | E | S | V | serverDataIndex.count=3, serverDataIndex.oldestIdx=2
 * +---+---+---+---+
 *
 * +---+---+---+---+
 * | V | V | E | S | serverDataIndex.count=3, serverDataIndex.oldestIdx=3
 * +---+---+---+---+
 *
 * +---+---+---+---+
 * | S | V | V | V | serverDataIndex.count=4, serverDataIndex.oldestIdx=0
 * +---+---+---+---+
 *
 * +---+---+---+---+
 * | V | S | V | V | serverDataIndex.count=4, serverDataIndex.oldestIdx=1
 * +---+---+---+---+
 *
 * +---+---+---+---+
 * | V | V | S | V | serverDataIndex.count=4, serverDataIndex.oldestIdx=2
 * +---+---+---+---+
 *
 * +---+---+---+---+
 * | V | V | V | S | serverDataIndex.count=4, serverDataIndex.oldestIdx=3
 * +---+---+---+---+
 */

