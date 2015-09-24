/**
 * Form server update message
 * @param pServer points to data set to send to the server
 * @param pMsg where to store the server update message
 * @param msgSize the size of the storage available for pMsg
 * @return true if the message is not big enough to old the configured data set
 */
bool formServerUpdateMessage(
    SERVER_DATA_T* pServerData,
    char* pMsg,
    size_t msgSize
) {
    bool rStat = true;
    char* pos = pMsg;
    char timeStr[22];

    if (!gsmGetTime(timeStr, DIM(timeStr), SECS(5))) {
        strncopy(timeStr, BAD_TIME, DIM(timeStr));
    }
    pos = calc_snprintf_return_pointer(
        pos, msgSize - (pos-pMsg),
        snprintf(pos, msgSize - (pos-pMsg),
                 "%s[", timeStr)
    );
    char* dataStart = pos;
    if (pServerData->gpsData.fixAge == TinyGPS::GPS_INVALID_AGE) {
        debug_println(F("formServerUpdateMessage() GPS data has invalid age"));
    } else {
        if ((config.server_send_flags >> SERVER_SEND_GPSDATE_POS)
                                       & SERVER_SEND_GPSDATE_MASK) {
            pos = calc_snprintf_return_pointer(
                pos, msgSize - (pos-pMsg),
                snprintf(pos, msgSize - (pos-pMsg),
                         "%s%lu", pos == dataStart ? "" : ",",
                         pServerData->gpsData.date)
            );
        }
        if ((config.server_send_flags >> SERVER_SEND_GPSTIME_POS)
                                       & SERVER_SEND_GPSTIME_MASK) {
            pos = calc_snprintf_return_pointer(
                pos, msgSize - (pos-pMsg),
                snprintf(pos, msgSize - (pos-pMsg),
                         "%s%lu", pos == dataStart ? "" : ",",
                         pServerData->gpsData.time)
            );
        }
        if ((config.server_send_flags >> SERVER_SEND_LATITUDE_POS)
                                       & SERVER_SEND_LATITUDE_MASK) {
            pos = calc_snprintf_return_pointer(
                pos, msgSize - (pos-pMsg),
                snprintf(pos, msgSize - (pos-pMsg),
                         "%s%1.6f", pos == dataStart ? "" : ",",
                         pServerData->gpsData.lat)
            );
        }
        if ((config.server_send_flags >> SERVER_SEND_LONGITUDE_POS)
                                       & SERVER_SEND_LONGITUDE_MASK) {
            pos = calc_snprintf_return_pointer(
                pos, msgSize - (pos-pMsg),
                snprintf(pos, msgSize - (pos-pMsg),
                         "%s%1.6f", pos == dataStart ? "" : ",",
                         pServerData->gpsData.lon)
            );
        }
        if ((config.server_send_flags >> SERVER_SEND_SPEED_POS)
                                       & SERVER_SEND_SPEED_MASK) {
            pos = calc_snprintf_return_pointer(
                pos, msgSize - (pos-pMsg),
                snprintf(pos, msgSize - (pos-pMsg),
                         "%s%1.6f", pos == dataStart ? "" : ",",
                         pServerData->gpsData.speed)
            );
        }
        if ((config.server_send_flags >> SERVER_SEND_ALTITUDE_POS)
                                       & SERVER_SEND_ALTITUDE_MASK) {
            pos = calc_snprintf_return_pointer(
                pos, msgSize - (pos-pMsg),
                snprintf(pos, msgSize - (pos-pMsg),
                         "%s%1.6f", pos == dataStart ? "" : ",",
                         pServerData->gpsData.alt)
            );
        }
        if ((config.server_send_flags >> SERVER_SEND_HEADING_POS)
                                       & SERVER_SEND_HEADING_MASK) {
            pos = calc_snprintf_return_pointer(
                pos, msgSize - (pos-pMsg),
                snprintf(pos, msgSize - (pos-pMsg),
                         "%s%1.6f", pos == dataStart ? "" : ",",
                         pServerData->gpsData.course)
            );
        }
        if ((config.server_send_flags >> SERVER_SEND_HDOP_POS)
                                       & SERVER_SEND_HDOP_MASK) {
            pos = calc_snprintf_return_pointer(
                pos, msgSize - (pos-pMsg),
                snprintf(pos, msgSize - (pos-pMsg),
                         "%s%lu", pos == dataStart ? "" : ",",
                         pServerData->gpsData.hdop)
            );
        }
        if ((config.server_send_flags >> SERVER_SEND_NSAT_POS)
                                       & SERVER_SEND_NSAT_MASK) {
            pos = calc_snprintf_return_pointer(
                pos, msgSize - (pos-pMsg),
                snprintf(pos, msgSize - (pos-pMsg),
                         "%s%lu", pos == dataStart ? "" : ",",
                         pServerData->gpsData.nsats)
            );
        }
    }
    pos = calc_snprintf_return_pointer(
        pos, msgSize - (pos-pMsg),
        snprintf(pos, msgSize - (pos-pMsg), "]")
    );
    dataStart = pos;
    if ((config.server_send_flags >> SERVER_SEND_BATT_POS)
                                   & SERVER_SEND_BATT_MASK) {
        // append battery level to data packet
        float sensorValue = analogRead(AIN_S_INLEVEL);
        float outputValue = sensorValue
            * (242.0f / 22.0f * ANALOG_VREF / 1024.0f);
        pos = calc_snprintf_return_pointer(
            pos, msgSize - (pos-pMsg),
            snprintf(pos, msgSize - (pos-pMsg),
                     "%s%.2f", pos == dataStart ? "" : ",", outputValue)
        );
    }
    if ((config.server_send_flags >> SERVER_SEND_IGN_POS)
                                   & SERVER_SEND_IGN_MASK) {
        pos = calc_snprintf_return_pointer(
            pos, msgSize - (pos-pMsg),
            snprintf(pos, msgSize - (pos-pMsg),
                     "%%s", pos == dataStart ? "" : ",",
                     pServerData->ignState ? "ON" : "OFF")
        );
    }
    if ((config.server_send_flags >> SERVER_SEND_RUNTIME_POS)
                                   & SERVER_SEND_RUNTIME_MASK) {
        pos = calc_snprintf_return_pointer(
            pos, msgSize - (pos-pMsg),
            snprintf(pos, msgSize - (pos-pMsg),
                     "%%ul", pos == dataStart ? "" : ",",
                         pServerData->engineRuntime)
        );
    }
    if (rStat && (pos-pMsg >= msgSize)) {
    	// Out of buffer space
        debug_println(F("formServerUpdateMessage() out of message space"));
    	rStat = false;
    }
    return rStat;
}
