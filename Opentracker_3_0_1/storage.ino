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
#define STORAGE_GPSDATA_OFFSET     1024
/**
 * Total length of GPS data area
 */
#define STORAGE_GPSDATA_SIZE       (64*1024)
/**
 * STORED_SETTINGS_T.marker value
 */
#define SETTINGS_VALID 0xAA557700
/**
 * STORED_GPSDATA_T.marker values
 */
#define STORED_GPSDATA_START 0x11111111
#define STORED_GPSDATA_VALID 0xAA557700
#define STORED_GPSDATA_EMPTY 0xFFFFFFFF
/**
 * The max number of GPS records we can fit into the storage area
 */
#define DIM_STORED_GPSDATA (STORAGE_GPSDATA_SIZE/sizeof(STORED_GPSDATA_INDEX_T))
/**
 * The index data we use to track usage of the GPS storage area
 */
STORED_GPSDATA_INDEX_T gpsDataIndex;

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
STORED_GPSDATA_T* storageGetGPSFirst() {
    return
        (STORED_GPSDATA_T*)dueFlashStorage.readAddress(STORAGE_GPSDATA_OFFSET);
}

/**
 * Gets a pointer to the last record in the GPS flash storage area
 * @return a pointer to the last record of the GPS flash storage area
 */
STORED_GPSDATA_T* storageGetGPSLast() {
    return storageGetGPSFirst() + DIM_STORED_GPSDATA - 1;
}

/**
 * Given a pointer to a record in the flash storage area, we return a pointer
 * to the one after, taking in to account any buffer wrap
 * @param pGPSData a pointer to a record in the flash storage area
 * @return a pointer to the record which follows pGPSData, taking in to account
 *         any buffer wrap
 */
STORED_GPSDATA_T* storageGetGPSNext(
    STORED_GPSDATA_T* pGPSData
) {
    if (pGPSData >= storageGetGPSLast())
        return storageGetGPSFirst();
    return pGPSData + 1;
}

/**
 * Given a pointer to a record in the flash storage area, we return a pointer
 * to the one before, taking in to account any buffer wrap
 * @param pGPSData a pointer to a record in the flash storage area
 * @return a pointer to the record which precedes pGPSData, taking in to account
 *         any buffer wrap
 */
STORED_GPSDATA_T* storageGetGPSPrev(
    STORED_GPSDATA_T* pGPSData
) {
    if (pGPSData <= storageGetGPSFirst())
        return storageGetGPSLast();
    return pGPSData - 1;
}

/**
 * Checks the flash store is valid and assigns the flash index data
 * @param pIndexData assigned the flash index data
 */
void storageGPSScan(
    STORED_GPSDATA_INDEX_T* pIndexData
) {
    STORED_GPSDATA_T* pGPSFirst = storageGetGPSFirst();
    STORED_GPSDATA_T* pGPSData = pGPSFirst;
    pIndexData->count = 0;
    pIndexData->storeValid = true;
    pIndexData->pOldest = NULL;
    bool scanComplete = false;
    // Locate the _single_ oldest record and checks for presence
    // of duff marker values (indicating corrupt store)
    while (!scanComplete) {
        switch (pGPSData->marker) {
        case STORED_GPSDATA_START:
            // Have located the oldest record
            if (pIndexData->pOldest == NULL) {
                // Record 1st oldest marker record
                pIndexData->pOldest = pGPSData;
                pIndexData->count += 1;
            } else {
                // There can be only one
                pIndexData->storeValid = false;
                scanComplete = true;
            }
            break;
        case STORED_GPSDATA_VALID:
            pIndexData->count += 1;
            break;
        case STORED_GPSDATA_EMPTY:
            break;
        default:
            // Invalid marker = duff flash (or maybe we have changed the record
            // format)
            pIndexData->storeValid = false;
            scanComplete = true;
            break;
        }
        pGPSData = storageGetGPSNext(pGPSData);
        if (pGPSData == pGPSFirst) {
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
                pIndexData->pOldest = storageGetGPSFirst();
            }
        } else {
            // Did find a start (oldest) record, so check we got a consecutive
            // sequence of valid records following it. This checks we see
            // something like EEEESVVVEEEE and not EEEESVVVEVEE.
            pGPSFirst = pIndexData->pOldest;
            pGPSData = pGPSFirst;
            size_t validCount = 0;
            scanComplete = false;
            while (!scanComplete) {
                switch (pGPSData->marker) {
                case STORED_GPSDATA_START:
                case STORED_GPSDATA_VALID:
                    validCount += 1;
                    break;
                case STORED_GPSDATA_EMPTY:
                default:
                    scanComplete = true;
                    break;
                }
                pGPSData = storageGetGPSNext(pGPSData);
                if (pGPSData == pGPSFirst) {
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
 * STORED_GPSDATA_EMPTY markers.
 * @return true if the store wiped OK, false if not
 */
bool storageWipeGPSData() {
    bool wipeOK = true;
    byte wipeData[256];
    memset(wipeData, 0xFF, sizeof(wipeData));

    size_t wipeLeft = STORAGE_GPSDATA_SIZE;
    size_t wipeOffset = 0;
    while (wipeOK && (wipeLeft > 0)) {
        size_t wipeSize = MIN(sizeof(wipeData), wipeLeft);
        if (!dueFlashStorage.write(
                STORAGE_GPSDATA_OFFSET + wipeOffset,
                wipeData, wipeSize)) {
            debug_println(F("storageWipeGPSData: failed to wipe flash"));
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
 * @param pGPSDataLocation the address within the flash area to write
 * @param pGPSData the GPS record to write
 * @return true if written OK, false if not
 */
bool storageWriteGPSDataToFlash(
    const STORED_GPSDATA_T* pGPSDataLocation,
    unsigned long marker,
    const GPSDATA_T* pGPSData
) {
    STORED_GPSDATA_T storedGPSData;
    storedGPSData.marker = marker;
    storedGPSData.gpsData = *pGPSData;
    size_t byteOffset = sizeof(STORED_GPSDATA_T) * (pGPSDataLocation -
                                                    storageGetGPSFirst());
    return dueFlashStorage.write(
        STORAGE_GPSDATA_OFFSET + byteOffset,
        (byte*)pGPSData, sizeof(STORED_GPSDATA_T));
}

/**
 * Call after boot to initialise our storage index data and to check the
 * flash content is valid. If flash is not valid we erase it all and
 * prepare it for use.
 */
void storageGPSDataInit() {
    storageGPSScan(&gpsDataIndex);
    if (!gpsDataIndex.storeValid) {
        debug_println(F("storageGPSDataInit: wiping flash"));
        gpsDataIndex.count = 0;
        gpsDataIndex.pOldest = storageGetGPSFirst();
        gpsDataIndex.storeValid = storageWipeGPSData(); 
    } else {
        debug_print(F("storageGPSDataInit: flash is OK and holding "));
        debug_print(gpsDataIndex.count);
        debug_println(F(" records"));
    }
}

/**
 * Writes a GPS data block to flash storage.
 * @param pGPSData the GPS data to write
 * @return true if stored OK, false if not
 */
bool storageWriteGPSData(
    const GPSDATA_T* pGPSData
) {
    bool writtenOK = false;
    if (gpsDataIndex.storeValid) {
        if (gpsDataIndex.count == 0) {
            // Store is empty so use first slot
            STORED_GPSDATA_T* pStoredGPSData = storageGetGPSFirst();
            writtenOK = storageWriteGPSDataToFlash(
                            pStoredGPSData, STORED_GPSDATA_START, pGPSData);
            gpsDataIndex.count = 1;
            gpsDataIndex.pOldest = pStoredGPSData;
        } else if (gpsDataIndex.count == DIM_STORED_GPSDATA) {
            // Store is full so overwrite oldest slot
            STORED_GPSDATA_T* pStoredGPSData = gpsDataIndex.pOldest;
            writtenOK = storageWriteGPSDataToFlash(
                            pStoredGPSData, STORED_GPSDATA_VALID, pGPSData);
            // Mark the following slot as the oldest
            pStoredGPSData = storageGetGPSNext(pStoredGPSData);
            writtenOK = writtenOK && storageWriteGPSDataToFlash(
                           pStoredGPSData, STORED_GPSDATA_START,
                           &pStoredGPSData->gpsData);
            gpsDataIndex.pOldest = pStoredGPSData;
        } else {
            // Locate next free slot as oldest+count
            STORED_GPSDATA_T* pStoredGPSData = gpsDataIndex.pOldest;
            for (size_t idx=0; idx < gpsDataIndex.count; ++idx) {
                pStoredGPSData = storageGetGPSNext(pStoredGPSData);
            }
            writtenOK = storageWriteGPSDataToFlash(
                            pStoredGPSData, STORED_GPSDATA_VALID, pGPSData);
            gpsDataIndex.count += 1;
        }
        if (!writtenOK) {
            debug_println(F("storageSaveGPSData: failed to update flash"));
            gpsDataIndex.storeValid = false;
            debug_println(F("storageSaveGPSData: marked flash as invalid"));
        }
    }
    return writtenOK;
}

/**
 * Returns the oldest GPS data record from flash
 * @param pGPSData where to write the GPS data
 * @return true if pGPSData assigned ok, false if there is no data to return
 */
bool storageReadOldestGPSData(
    GPSDATA_T* pGPSData
) {
    if (!gpsDataIndex.storeValid)
        return false;
    if (gpsDataIndex.count == 0)
        return false;
    *pGPSData = gpsDataIndex.pOldest->gpsData;
    return true;
}

/**
 * Removes the oldest GPS data record from flash
 * @return true if removed OK, false if no GPS data in flash or we failed
 *         to update the flash
 */
bool storageForgetOldestGPSData() {
    if (!gpsDataIndex.storeValid)
        return false;
    if (gpsDataIndex.count == 0)
        return false;
    // Mark the oldest slot as empty
    STORED_GPSDATA_T* pStoredGPSData = gpsDataIndex.pOldest;
    bool writtenOK = storageWriteGPSDataToFlash(
                    pStoredGPSData, STORED_GPSDATA_EMPTY,
                    &pStoredGPSData->gpsData);
    gpsDataIndex.count -= 1;
    if (gpsDataIndex.count > 0) {
        // Mark the following slot as the oldest
        pStoredGPSData = storageGetGPSNext(pStoredGPSData);
        writtenOK = writtenOK && storageWriteGPSDataToFlash(
                       pStoredGPSData, STORED_GPSDATA_START,
                       &pStoredGPSData->gpsData);
        gpsDataIndex.pOldest = pStoredGPSData;
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
 * | E | E | E | E | gpsDataIndex.count=0, gpsDataIndex.oldestIdx=x
 * +---+---+---+---+
 *
 * +---+---+---+---+
 * | S | E | E | E | gpsDataIndex.count=1, gpsDataIndex.oldestIdx=0
 * +---+---+---+---+
 *
 * +---+---+---+---+
 * | E | S | E | E | gpsDataIndex.count=1, gpsDataIndex.oldestIdx=1
 * +---+---+---+---+
 *
 * +---+---+---+---+
 * | E | E | S | E | gpsDataIndex.count=1, gpsDataIndex.oldestIdx=2
 * +---+---+---+---+
 *
 * +---+---+---+---+
 * | E | E | E | S | gpsDataIndex.count=1, gpsDataIndex.oldestIdx=3
 * +---+---+---+---+
 *
 * +---+---+---+---+
 * | S | V | E | E | gpsDataIndex.count=2, gpsDataIndex.oldestIdx=0
 * +---+---+---+---+
 *
 * +---+---+---+---+
 * | E | S | V | E | gpsDataIndex.count=2, gpsDataIndex.oldestIdx=1
 * +---+---+---+---+
 *
 * +---+---+---+---+
 * | E | E | S | V | gpsDataIndex.count=2, gpsDataIndex.oldestIdx=2
 * +---+---+---+---+
 *
 * +---+---+---+---+
 * | V | E | E | S | gpsDataIndex.count=2, gpsDataIndex.oldestIdx=3
 * +---+---+---+---+
 *
 * +---+---+---+---+
 * | S | V | V | E | gpsDataIndex.count=3, gpsDataIndex.oldestIdx=0
 * +---+---+---+---+
 *
 * +---+---+---+---+
 * | E | S | V | V | gpsDataIndex.count=3, gpsDataIndex.oldestIdx=1
 * +---+---+---+---+
 *
 * +---+---+---+---+
 * | V | E | S | V | gpsDataIndex.count=3, gpsDataIndex.oldestIdx=2
 * +---+---+---+---+
 *
 * +---+---+---+---+
 * | V | V | E | S | gpsDataIndex.count=3, gpsDataIndex.oldestIdx=3
 * +---+---+---+---+
 *
 * +---+---+---+---+
 * | S | V | V | V | gpsDataIndex.count=4, gpsDataIndex.oldestIdx=0
 * +---+---+---+---+
 *
 * +---+---+---+---+
 * | V | S | V | V | gpsDataIndex.count=4, gpsDataIndex.oldestIdx=1
 * +---+---+---+---+
 *
 * +---+---+---+---+
 * | V | V | S | V | gpsDataIndex.count=4, gpsDataIndex.oldestIdx=2
 * +---+---+---+---+
 *
 * +---+---+---+---+
 * | V | V | V | S | gpsDataIndex.count=4, gpsDataIndex.oldestIdx=3
 * +---+---+---+---+
 */

