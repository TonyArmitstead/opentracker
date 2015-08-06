void gps_setup() {
    debug_println(F("gps_setup() started"));
    pinMode(PIN_STANDBY_GPS, OUTPUT);
    digitalWrite(PIN_STANDBY_GPS, LOW);
    pinMode(PIN_RESET_GPS, OUTPUT);
    digitalWrite(PIN_RESET_GPS, LOW);
    debug_println(F("gps_setup() started"));
}

void gps_on_off() {
    //turn on GPS
    debug_println(F("gps_on_off() started"));
    digitalWrite(PIN_STANDBY_GPS, LOW);
    debug_println(F("gps_on_off() finished"));
}

/**
 * Collect current gps position data
 * @param pGPSData points to the record to receive the GPS data
 * @param timeout how long to wait in ms to receive current data
 */
bool read_gps_data(
    GPSDATA_T* pGPSData,
    unsigned long timeout
) {
    bool rStat = false;
    unsigned long tStart = millis();
    // Clear out any buffered received GPS data
    while (gps_port.available() && time_diff(millis(), tStart) < timeout) {
        (void)gps_port.read();
    }
    while ((rStat == false) && time_diff(millis(), tStart) < timeout) {
        if (gps_port.available()) {
            // As new GPS data arrives, feed it to the gps object until it tells
            // us it has received a new gps data sentence
            int c = gps_port.read();
            if (gps.encode(c)) {
                gps.f_get_position(
                    &pGPSData->lat, &pGPSData->lon, &pGPSData->fixAge);
                if ((pGPSData->fixAge != TinyGPS::GPS_INVALID_AGE) &&
                    (pGPSData->fixAge < 1000)) {
                    // We have a fix which is < 1s old so consider it
                    // as current
                    pGPSData->alt = gps.f_altitude();
                    pGPSData->course = gps.f_course();
                    pGPSData->speed = gps.f_speed_kmph();
                    pGPSData->hdop = gps.hdop();
                    pGPSData->nsats = gps.satellites();
                    gps.get_datetime(&pGPSData->date, &pGPSData->time);
                    blink_got_gps();
                    rStat = true;
                }
            }
        }
    }
    return rStat;
}

char* calc_snprintf_return_pointer(
    char* pStr,
    size_t strSize,
    int snprintf_len
) {
    if (snprintf_len > 0) {
        if (snprintf_len > strSize) {
            return pStr + strSize;
        } else {
            return pStr + snprintf_len;
        }
    } else {
        return pStr;
    }
}

char* gps_form_map_location_url(
    char* pURL,
    size_t urlSize,
    GPSDATA_T* pGPSData
) {
    return calc_snprintf_return_pointer(
        pURL, urlSize,
        snprintf(pURL, urlSize,
                 "comgooglemaps://?q=%.6f,%.6f",
                 pGPSData->lat, pGPSData->lon)
    );
}

char* gps_form_web_location_url(
    char* pURL,
    size_t urlSize,
    GPSDATA_T* pGPSData
) {
    return calc_snprintf_return_pointer(
        pURL, urlSize,
        snprintf(pURL, urlSize,
                 "https://maps.google.co.uk/maps/place/%.6f,%.6f",
                 pGPSData->lat, pGPSData->lon)
    );
}

char* gps_form_val_location_str(
    char* pStr,
    size_t strSize,
    GPSDATA_T* pGPSData
) {
    return calc_snprintf_return_pointer(
        pStr, strSize,
        snprintf(pStr, strSize,
                 "lat=%.6f, lon=%.6f",
                 pGPSData->lat, pGPSData->lon)
    );
}

