/**
 * Form server update message
 * @param pGPSData points to the GPS data record
 * @param pMsg where to store the server update message
 * @param msgSize the size of the storage available for pMsg
 * @param ignState ignition state, true for ON
 * @return true if message formed OK, false if message not formed OK due to
 *         either something wrong with the GPS data or the message is not
 *         big enough to old the configured data set
 */
bool formServerUpdateMessage(
    GPSDATA_T* pGPSData,
    char* pMsg,
    size_t msgSize,
	bool ignState,
	unsigned long engineRuntime
) {
    bool rStat = true;
    char* pos = pMsg;

    if (pGPSData->fixAge == TinyGPS::GPS_INVALID_AGE) {
        debug_println(F("formServerUpdateMessage() pGPSData has invalid age"));
    	rStat = false;
    } else {
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
		if ((config.server_send_flags >> SERVER_SEND_GPSDATE_POS)
									   & SERVER_SEND_GPSDATE_MASK) {
			pos = calc_snprintf_return_pointer(
				pos, msgSize - (pos-pMsg),
				snprintf(pos, msgSize - (pos-pMsg),
						 "%s%lu", pos == dataStart ? "" : ",", pGPSData->date)
			);
		}
		if ((config.server_send_flags >> SERVER_SEND_GPSTIME_POS)
									   & SERVER_SEND_GPSTIME_MASK) {
			pos = calc_snprintf_return_pointer(
				pos, msgSize - (pos-pMsg),
				snprintf(pos, msgSize - (pos-pMsg),
						 "%s%lu", pos == dataStart ? "" : ",", pGPSData->time)
			);
		}
		if ((config.server_send_flags >> SERVER_SEND_LATITUDE_POS)
									   & SERVER_SEND_LATITUDE_MASK) {
			pos = calc_snprintf_return_pointer(
				pos, msgSize - (pos-pMsg),
				snprintf(pos, msgSize - (pos-pMsg),
						 "%s%1.6f", pos == dataStart ? "" : ",", pGPSData->lat)
			);
		}
		if ((config.server_send_flags >> SERVER_SEND_LONGITUDE_POS)
									   & SERVER_SEND_LONGITUDE_MASK) {
			pos = calc_snprintf_return_pointer(
				pos, msgSize - (pos-pMsg),
				snprintf(pos, msgSize - (pos-pMsg),
						 "%s%1.6f", pos == dataStart ? "" : ",", pGPSData->lon)
			);
		}
		if ((config.server_send_flags >> SERVER_SEND_SPEED_POS)
									   & SERVER_SEND_SPEED_MASK) {
			pos = calc_snprintf_return_pointer(
				pos, msgSize - (pos-pMsg),
				snprintf(pos, msgSize - (pos-pMsg),
						 "%s%1.6f", pos == dataStart ? "" : ",", pGPSData->speed)
			);
		}
		if ((config.server_send_flags >> SERVER_SEND_ALTITUDE_POS)
									   & SERVER_SEND_ALTITUDE_MASK) {
			pos = calc_snprintf_return_pointer(
				pos, msgSize - (pos-pMsg),
				snprintf(pos, msgSize - (pos-pMsg),
						 "%s%1.6f", pos == dataStart ? "" : ",", pGPSData->alt)
			);
		}
		if ((config.server_send_flags >> SERVER_SEND_HEADING_POS)
									   & SERVER_SEND_HEADING_MASK) {
			pos = calc_snprintf_return_pointer(
				pos, msgSize - (pos-pMsg),
				snprintf(pos, msgSize - (pos-pMsg),
						 "%s%1.6f", pos == dataStart ? "" : ",", pGPSData->course)
			);
		}
		if ((config.server_send_flags >> SERVER_SEND_HDOP_POS)
									   & SERVER_SEND_HDOP_MASK) {
			pos = calc_snprintf_return_pointer(
				pos, msgSize - (pos-pMsg),
				snprintf(pos, msgSize - (pos-pMsg),
						 "%s%lu", pos == dataStart ? "" : ",", pGPSData->hdop)
			);
		}
		if ((config.server_send_flags >> SERVER_SEND_NSAT_POS)
									   & SERVER_SEND_NSAT_MASK) {
			pos = calc_snprintf_return_pointer(
				pos, msgSize - (pos-pMsg),
				snprintf(pos, msgSize - (pos-pMsg),
						 "%s%lu", pos == dataStart ? "" : ",", pGPSData->nsats)
			);
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
	                     "%%s", pos == dataStart ? "" : ",", ignState ? "ON" : "OFF")
	        );
	    }
	    if ((config.server_send_flags >> SERVER_SEND_RUNTIME_POS)
	                                   & SERVER_SEND_RUNTIME_MASK) {
	        pos = calc_snprintf_return_pointer(
	            pos, msgSize - (pos-pMsg),
	            snprintf(pos, msgSize - (pos-pMsg),
	                     "%%ul", pos == dataStart ? "" : ",", engineRuntime)
	        );
	    }
    }
    if (rStat && (pos-pMsg >= msgSize)) {
    	// Out of buffer space
        debug_println(F("formServerUpdateMessage() out of message space"));
    	rStat = false;
    }
    return rStat;
}
