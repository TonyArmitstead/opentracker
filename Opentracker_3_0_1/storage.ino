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
 * Size of total storage available for everything
 */
#define STORAGE_SIZE (256*1024)
/**
 *  Offset into flash where we store the configuration settings
 */
#define STORAGE_SETTINGS_OFFSET 0
/**
 * Offset into flash where we store the un-reported location data
 */
#define STORAGE_SERVER_DATA_OFFSET     1024
/**
 * Total length of GPS data area
 */
#define STORAGE_SERVER_DATA_SIZE       (64*1024)
/**
 * STORED_SETTINGS_T.marker value
 */
#define SETTINGS_VALID 0xAA557700
/**
 * STORED_SERVER_DATA_T.marker values
 */
#define STORED_SERVER_DATA_START 0x11111111
#define STORED_SERVER_DATA_VALID 0xAA557700
#define STORED_SERVER_DATA_EMPTY 0xFFFFFFFF
/**
 * The max number of GPS records we can fit into the storage area
 */
#define STORED_SERVER_DATA_START \
    (STORAGE_SERVER_DATA_SIZE/sizeof(STORED_SERVER_DATA_INDEX_T))
/**
 * The index data we use to track usage of the GPS storage area
 */
STORED_SERVER_DATA_INDEX_T serverDataIndex;

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
    STORED_SETTINGS_T savedSettings;
    savedSettings.marker = SETTINGS_VALID;
    savedSettings.settingsSize = sizeof(SETTINGS_T);
    memcpy(&savedSettings.settings, pSettings, sizeof(SETTINGS_T));
    savedSettings.crc32 = calcCRC32(&savedSettings.settings,
                                    sizeof(SETTINGS_T));
    if (!dueFlashStorage.write(
            STORAGE_SETTINGS_OFFSET,
            (byte*)&savedSettings,
            (uint32_t)sizeof(savedSettings))) {
        debug_println(F("storageSaveSettings: failed to save settings to flash"));
        rStat = false;
    } else {
        rStat = true;
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
 * Gets a pointer to the start of the GPS flash storage area
 * @return a pointer to the start of the GPS flash storage area
 */
STORED_SERVER_DATA_T* storageGetServerFirst() {
    return
        (STORED_SERVER_DATA_T*)dueFlashStorage.readAddress(STORAGE_SERVER_DATA_OFFSET);
}

/**
 * Gets a pointer to the last record in the GPS flash storage area
 * @return a pointer to the last record of the GPS flash storage area
 */
STORED_SERVER_DATA_T* storageGetServerLast() {
    return storageGetServerFirst() + STORED_SERVER_DATA_START - 1;
}

/**
 * Given a pointer to a record in the flash storage area, we return a pointer
 * to the one after, taking in to account any buffer wrap
 * @param pServerData a pointer to a record in the flash storage area
 * @return a pointer to the record which follows pServerData, taking in to account
 *         any buffer wrap
 */
STORED_SERVER_DATA_T* storageGetServerNext(
    STORED_SERVER_DATA_T* pServerData
) {
    if (pServerData >= storageGetServerLast())
        return storageGetServerFirst();
    return pServerData + 1;
}

/**
 * Given a pointer to a record in the flash storage area, we return a pointer
 * to the one before, taking in to account any buffer wrap
 * @param pServerData a pointer to a record in the flash storage area
 * @return a pointer to the record which precedes pServerData, taking in to account
 *         any buffer wrap
 */
STORED_SERVER_DATA_T* storageGetServerPrev(
    STORED_SERVER_DATA_T* pServerData
) {
    if (pServerData <= storageGetServerFirst())
        return storageGetServerLast();
    return pServerData - 1;
}

/**
 * Checks the flash store is valid and assigns the flash index data
 * @param pIndexData assigned the flash index data
 */
void storageServerScan(
    STORED_SERVER_DATA_INDEX_T* pIndexData
) {
    STORED_SERVER_DATA_T* pServerFirst = storageGetServerFirst();
    STORED_SERVER_DATA_T* pServerData = pServerFirst;
    pIndexData->count = 0;
    pIndexData->storeValid = true;
    pIndexData->pOldest = NULL;
    bool scanComplete = false;
    // Locate the _single_ oldest record and checks for presence
    // of duff marker values (indicating corrupt store)
    while (!scanComplete) {
        switch (pServerData->marker) {
        case STORED_SERVER_DATA_START:
            // Have located the oldest record
            if (pIndexData->pOldest == NULL) {
                // Record 1st oldest marker record
                pIndexData->pOldest = pServerData;
                pIndexData->count += 1;
            } else {
                // There can be only one
                pIndexData->storeValid = false;
                scanComplete = true;
            }
            break;
        case STORED_SERVER_DATA_VALID:
            pIndexData->count += 1;
            break;
        case STORED_SERVER_DATA_EMPTY:
            break;
        default:
            // Invalid marker = duff flash (or maybe we have changed the record
            // format)
            pIndexData->storeValid = false;
            scanComplete = true;
            break;
        }
        pServerData = storageGetServerNext(pServerData);
        if (pServerData == pServerFirst) {
            scanComplete = true;
        }
    }
    if (pIndexData->storeValid) {
        if (pIndexData->pOldest == NULL) {
            // Didnt find a start (oldest) record, so should not have seen
            // and valid records
            if (pIndexData->count > 0) {
                pIndexData->storeValid = false;
            } else {
                pIndexData->pOldest = storageGetServerFirst();
            }
        } else {
            // Did find a start (oldest) record, so check we got a consecutive
            // sequence of valid records following it. This checks we see
            // something like EEEESVVVEEEE and not EEEESVVVEVEE.
            pServerFirst = pIndexData->pOldest;
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
                pServerData = storageGetServerNext(pServerData);
                if (pServerData == pServerFirst) {
                    scanComplete = true;
                }
            }
            if (validCount != pIndexData->count) {
                pIndexData->storeValid = false;
            }
        }
    }
}

/**
 * Erases the GPS data store. This effectively marks the store full of
 * STORED_SERVER_DATA_EMPTY markers.
 * @return true if the store wiped OK, false if not
 */
bool storageWipeServerData() {
    bool wipeOK = true;
    byte wipeData[256];
    memset(wipeData, 0xFF, sizeof(wipeData));

    size_t wipeLeft = STORAGE_SERVER_DATA_SIZE;
    size_t wipeOffset = 0;
    while (wipeOK && (wipeLeft > 0)) {
        size_t wipeSize = MIN(sizeof(wipeData), wipeLeft);
        if (!dueFlashStorage.write(
                STORAGE_SERVER_DATA_OFFSET + wipeOffset,
                wipeData, wipeSize)) {
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
 * @param pServerDataLocation the address within the flash area to write
 * @param pServerData the GPS record to write
 * @return true if written OK, false if not
 */
bool storageWriteServerDataToFlash(
    const STORED_SERVER_DATA_T* pServerDataLocation,
    unsigned long marker,
    const SERVER_DATA_T* pServerData
) {
    STORED_SERVER_DATA_T storedServerData;
    storedServerData.marker = marker;
    storedServerData.ServerData = *pServerData;
    size_t byteOffset = sizeof(STORED_SERVER_DATA_T) * (pServerDataLocation -
                                                    storageGetServerFirst());
    return dueFlashStorage.write(
        STORAGE_SERVER_DATA_OFFSET + byteOffset,
        (byte*)pServerData, sizeof(STORED_SERVER_DATA_T));
}

/**
 * Call after boot to initialise our storage index data and to check the
 * flash content is valid. If flash is not valid we erase it all and
 * prepare it for use.
 */
void storageServerDataInit() {
    storageServerScan(&serverDataIndex);
    if (!serverDataIndex.storeValid) {
        debug_println(F("storageServerDataInit: wiping flash"));
        serverDataIndex.count = 0;
        serverDataIndex.pOldest = storageGetServerFirst();
        serverDataIndex.storeValid = storageWipeServerData();
    } else {
        debug_print(F("storageServerDataInit: flash is OK and holding "));
        debug_print(serverDataIndex.count);
        debug_println(F(" records"));
    }
}

/**
 * Writes a GPS data block to flash storage.
 * @param pServerData the GPS data to write
 * @return true if stored OK, false if not
 */
bool storageWriteServerData(
    const SERVER_DATA_T* pServerData
) {
    bool writtenOK = false;
    if (serverDataIndex.storeValid) {
        if (serverDataIndex.count == 0) {
            // Store is empty so use first slot
            STORED_SERVER_DATA_T* pStoredServerData = storageGetServerFirst();
            writtenOK = storageWriteServerDataToFlash(
                            pStoredServerData, STORED_SERVER_DATA_START,
                            pServerData, ignState, engineRuntime);
            serverDataIndex.count = 1;
            serverDataIndex.pOldest = pStoredServerData;
        } else if (serverDataIndex.count == STORED_SERVER_DATA_START) {
            // Store is full so overwrite oldest slot
            STORED_SERVER_DATA_T* pStoredServerData = serverDataIndex.pOldest;
            writtenOK = storageWriteServerDataToFlash(
                            pStoredServerData, STORED_SERVER_DATA_VALID,
                            pServerData, ignState, engineRuntime);
            // Mark the following slot as the oldest
            pStoredServerData = storageGetServerNext(pStoredServerData);
            writtenOK = writtenOK && storageWriteServerDataToFlash(
                           pStoredServerData, STORED_SERVER_DATA_START,
                           &pStoredServerData->ServerData,
                           pStoredServerData->ignState,
                           pStoredServerData->engineRuntime);
            serverDataIndex.pOldest = pStoredServerData;
        } else {
            // Locate next free slot as oldest+count
            STORED_SERVER_DATA_T* pStoredServerData = serverDataIndex.pOldest;
            for (size_t idx=0; idx < serverDataIndex.count; ++idx) {
                pStoredServerData = storageGetServerNext(pStoredServerData);
            }
            writtenOK = storageWriteServerDataToFlash(
                            pStoredServerData, STORED_SERVER_DATA_VALID,
                            pServerData, ignState, engineRuntime);
            serverDataIndex.count += 1;
        }
        if (!writtenOK) {
            debug_println(F("storageSaveServerData: failed to update flash"));
            serverDataIndex.storeValid = false;
            debug_println(F("storageSaveServerData: marked flash as invalid"));
        }
    }
    return writtenOK;
}

size_t storageGetStoredServerDataCount() {
    if (serverDataIndex.storeValid) {
        return serverDataIndex.count;
    }
    return 0;
}

/**
 * Returns the oldest GPS data record from flash
 * @param pServerData where to write the GPS data
 * @return true if pServerData assigned ok, false if there is no data to return
 */
bool storageReadOldestServerData(
    SERVER_DATA_T* pServerData
) {
    if (!serverDataIndex.storeValid)
        return false;
    if (serverDataIndex.count == 0)
        return false;
    *pServerData = serverDataIndex.pOldest->ServerData;
    return true;
}

/**
 * Removes the oldest GPS data record from flash
 * @return true if removed OK, false if no GPS data in flash or we failed
 *         to update the flash
 */
bool storageForgetOldestServerData() {
    if (!serverDataIndex.storeValid)
        return false;
    if (serverDataIndex.count == 0)
        return false;
    // Mark the oldest slot as empty
    STORED_SERVER_DATA_T* pStoredData = serverDataIndex.pOldest;
    bool writtenOK = storageWriteServerDataToFlash(
                    pStoredData, STORED_SERVER_DATA_EMPTY,
                    &pStoredData->serverData);
    serverDataIndex.count -= 1;
    if (serverDataIndex.count > 0) {
        // Mark the following slot as the oldest
        pStoredData = storageGetServerNext(pStoredData);
        writtenOK = writtenOK && storageWriteServerDataToFlash(
                       pStoredData, STORED_SERVER_DATA_START,
                       &pStoredData->serverData);
        serverDataIndex.pOldest = pStoredServerData;
    }
    return writtenOK;
}

/*
 * If the GPS storage block had space for 4 entries, these diagrams show all
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

