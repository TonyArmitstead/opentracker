/**
 * Sets the IO pins (directions) for the modem
 */
void gsmSetupPIO() {
    //setup modem pins
    debug_println(F("gsm_setup() started"));
    pinMode(PIN_C_PWR_GSM, OUTPUT);
    digitalWrite(PIN_C_PWR_GSM, LOW);
    pinMode(PIN_C_KILL_GSM, OUTPUT);
    digitalWrite(PIN_C_KILL_GSM, LOW);
    pinMode(PIN_STATUS_GSM, INPUT);
    pinMode(PIN_RING_GSM, INPUT);
    pinMode(PIN_WAKE_GSM, INPUT);
    debug_println(F("gsm_setup() finished"));
}

/**
 * Powers on the gsm modem
 * @returns true if gsm modem powered up ok, false if not
 */
bool gsmPowerOn() {
    bool isPoweredOn = false;
    debug_println(F("gsmPowerOn() started"));
    if (digitalRead(PIN_STATUS_GSM) == LOW) { // i.e. is OFF
        // turn on the modem
        digitalWrite(PIN_C_PWR_GSM, HIGH);
        delay(1000);
        // Wait for chip to power up
        unsigned long tStart = millis();
        while (timeDiff(millis(), tStart) < 2000) {
            if (digitalRead(PIN_STATUS_GSM) == HIGH) {
                debug_println(F("gsmPowerOn() gsm modem powered on"));
                isPoweredOn = true;
                break;
            }
        }
        digitalWrite(PIN_C_PWR_GSM, LOW);
        if (isPoweredOn) {
            // Give it 5s to boot up
            delay(5000);
        }
    } else {
        debug_println(F("gsmPowerOn() ERROR: gsm modem already on"));
        isPoweredOn = true;
    }
    debug_println(F("gsmPowerOn() finished"));
    return isPoweredOn;
}

/**
 * Powers off the gsm modem
 */
void gsmPowerOff() {
    debug_println(F("gsmPowerOff() started"));
    if (digitalRead(PIN_STATUS_GSM) == HIGH) { // i.e. is ON
        // turn off the modem
        digitalWrite(PIN_C_PWR_GSM, HIGH);
        delay(800); // 700..1000ms pulse to turn off
        digitalWrite(PIN_C_PWR_GSM, LOW);
        // Wait for chip to power down
        unsigned long tStart = millis();
        // Should power down within 12s
        while (timeDiff(millis(), tStart) < 12000) {
            if (digitalRead(PIN_STATUS_GSM) == LOW) {
                debug_println(F("gsmPowerOff() gsm modem powered down"));
                break;
            }
        }
    } else {
        debug_println(F("gsmPowerOff() ERROR: gsm modem already off"));
    }
    debug_println(F("gsmPowerOff() finished"));
}

void gsmIndicatePowerOn() {
    //blink modem restart
    for (int i = 0; i < 5; i++) {
        if (ledState == LOW)
            ledState = HIGH;
        else
            ledState = LOW;
        digitalWrite(PIN_POWER_LED, ledState);   // set the LED on
        delay(200);
    }
}

/**
 * Tries to synchronise the UART comms with the modem. We send AT commands
 * until the modem responds with OK.
 * @params timeout how long to wait for comms sync to be acheived
 * @returns true if comms sync'd OK, false if not
 */
bool gsmSyncComms(
    unsigned timeout
) {
    unsigned long tStart = millis();
    while (timeDiff(millis(), tStart) < timeout) {
        snprintf(modem_command, sizeof(modem_command), "AT");
        gsm_send_command();
        if (gsm_wait_for_reply(true)) {
            debug_println(F("gsmSyncComms() succeeded"));
            debug_println(modem_reply);
            return true;
        }
    }
    debug_println(F("gsmSyncComms() failed"));
    return false;
}

/**
 * Determines the modem connection status i.e. whether the modem is
 * currently operational
 * @returns one of the GSMSTATUS_E values
 */
GSMSTATUS_T gsmGetNetworkStatus() {
    GSMSTATUS_T status = NOT_READY;
    snprintf(modem_command, sizeof(modem_command), "AT+QNSTATUS");
    gsm_send_command();
    gsm_wait_for_reply(true);
    char *pos = strstr(modem_reply, "+QNSTATUS:");
    if (pos != NULL) {
        pos += 10;
        unsigned int s = 255;
        if (sscanf(pos, "%u", &s) == 1) {
            switch (s) {
            case 0: status = CONNECTED; break;
            case 1: status = NO_CELL; break;
            case 2: status = LIMITED; break;
            default: status = NOT_READY; break;
            }
        }
    }
    return status;
}

/**
 * Converts a gsm network status to a string form
 * @param networkStatus the network status value (from GSMSTATUS_T)
 * @returns a string which indicates the network status
 */
const char* gsmGetNetworkStatusString(
    GSMSTATUS_T networkStatus
) {
    const char* status = NULL;
    switch (networkStatus) {
    case CONNECTED: status = "CONNECTED"; break;
    case NO_CELL: status = "NO_CELL"; break;
    case LIMITED: status = "LIMITED"; break;
    default: status = "NOT_READY"; break;
    }
    return status;
}

void gsm_send_command() {
    // Empty out any residual received data
    while (gsm_port.available()) {
        (void)gsm_port.read();
    }
    modem_reply[0] = '\0';
    if (modemLogging) {
        debug_print(F("gsm_send_command(): "));
        debug_println(modem_command);
    }
    gsm_port.print(modem_command);
    gsm_port.print("\r");
}

void gsm_send_tcp_data() {
    if (modemLogging) {
        debug_print(F("gsm_send_tcp_data(): "));
        debug_println(modem_data);
    }
    gsm_port.print(modem_data);
}

void gsm_set_pin() {
    debug_println(F("gsm_set_pin() started"));
    //checking if PIN is set 
    snprintf(modem_command, sizeof(modem_command), "AT+CPIN?");
    gsm_send_command();
    gsm_wait_for_reply(true);
    char *tmp = strstr(modem_reply, "SIM PIN");
    if (tmp != NULL) {
        debug_println(F("gsm_set_pin(): PIN is required"));
        //checking if pin is valid one
        if (config.sim_pin[0] == -1) {
            debug_println(F("gsm_set_pin(): PIN is not supplied."));
        } else {
            if (strlen(config.sim_pin) == 4) {
                debug_println(
                    F("gsm_set_pin(): PIN supplied, sending to modem."));
                gsm_port.print("AT+CPIN=");
                gsm_port.print(config.sim_pin);
                gsm_port.print("\r");
                gsm_wait_for_reply(true);
                tmp = strstr(modem_reply, "OK");
                if (tmp != NULL) {
                    debug_println(F("gsm_set_pin(): PIN is accepted"));
                } else {
                    debug_println(F("gsm_set_pin(): PIN is not accepted"));
                }
            } else {
                debug_println(
                    F("gsm_set_pin(): PIN supplied, but has invalid length."
                      " Not sending to modem."));
            }
        }
    } else {
        debug_println(F("gsm_set_pin(): PIN is not required"));
    }
    debug_println(F("gsm_set_pin() completed"));
}

/**
 * Reads a time string from the modem
 * @param pStr points to where to write the string
 * @param strSize the storage size for pStr
 * @return true if we read a valid time, false if not
 * 
 * gsm_send_command(): AT+QLTS
 * Modem Reply: 'AT+QLTS\r\r\n+QLTS: "15/08/09,15:01:49+04,1"\r\n\r\nOK\r\n'
 */
bool gsmGetTime(
    char* pStr,
    size_t strSize,
    unsigned timeout
) {
    bool validTime = false;
    unsigned long tStart = millis();
    while (!validTime && (timeDiff(millis(), tStart) < timeout)) {
        *pStr = '\0';
        snprintf(modem_command, sizeof(modem_command), "AT+QLTS");
        gsm_send_command();
        gsm_wait_for_reply(true);
        char *tStart = strstr(modem_reply, "+QLTS: \"");
        if (tStart != NULL) {
            unsigned year, month, day, hour, mi, sec;
            char tz1, tz2, tz3;
            if (sscanf(tStart+8, "%u/%u/%u,%u:%u:%u%c%c%c",
                       &year, &month, &day, &hour, &mi, &sec,
                       &tz1, &tz2, &tz3) == 9) {
                unsigned tz = (10*(tz2-'0') + (tz3-'0'))/4;
                snprintf(pStr, strSize, "%02u/%02u/%02u,%02u:%02u:%02u%c%02u",
                    year, month, day, hour, mi, sec, tz1, tz
                );
                validTime = true;
            }
        }
    }
    return validTime;
}

void gsm_startup_cmd() {
    debug_println(F("gsm_startup_cmd() started"));
    //disable echo for TCP data
    snprintf(modem_command, sizeof(modem_command), "AT+QISDE=0");
    gsm_send_command();
    gsm_wait_for_reply(true);
    //set receiving TCP data by command
    snprintf(modem_command, sizeof(modem_command), "AT+QINDI=1");
    gsm_send_command();
    gsm_wait_for_reply(true);
    //set SMS as text format
    snprintf(modem_command, sizeof(modem_command), "AT+CMGF=1");
    gsm_send_command();
    gsm_wait_for_reply(true);
    // Request network time sync
    snprintf(modem_command, sizeof(modem_command), "AT+QNITZ=1");
    gsm_send_command();
    gsm_wait_for_reply(true);
    // Request local time saved to RTC time
    snprintf(modem_command, sizeof(modem_command), "AT+CTZU=3");
    gsm_send_command();
    gsm_wait_for_reply(true);
    // Report network time
    snprintf(modem_command, sizeof(modem_command), "AT+QLTS");
    gsm_send_command();
    gsm_wait_for_reply(true);
    debug_println(F("gsm_startup_cmd() completed"));
}

/**
 * Checks that a string of digits makes up a valid IMEI number
 * @param pIMEI points to the string of digits
 * @param lenIMEI number of characters in pIMEI
 * @returns true if digitd check out, false if not
 * @note see http://imei-number.com/check-digit-calculation
 */
bool checkValidIMEI(
    const char* pIMEI,
    size_t lenIMEI
) {
    bool isValid = false;
    if (lenIMEI > 1) {
        const char* pDigit = pIMEI + lenIMEI - 1;
        unsigned sum = (*pDigit--) - '0';
        for (bool doDouble = true; pDigit >= pIMEI; doDouble = !doDouble) {
            unsigned d = (*pDigit--) - '0';
            sum += doDouble ? ((2*d) / 10) + ((2*d) % 10) : d;
        }
        isValid = ((sum % 10) == 0);
    }
    return isValid;
}

/**
 * Reads the IMEI number (as a string) from the modem
 * @param pStr points to where to write the string
 * @param strSize the storage size for pStr
 * @return true if we read a valid IMEI number, false if not
 * @note even when we return false, we still return the (bad) IMEI number
 *       in pStr
 */
bool gsmGetIMEI(
    char* pStr,
    size_t strSize,
    unsigned long timeout
) {
    bool validIMEI = false;
    // For unknown reasons we can sometimes read a duff IMEI number from the
    // modem, so we try a number of times to get a good IMEI number
    unsigned long tStart = millis();
    while (!validIMEI && (timeDiff(millis(), tStart) < timeout)) {
        // Get modem's IMEI number
        snprintf(modem_command, sizeof(modem_command), "AT+GSN");
        gsm_send_command();
        gsm_wait_for_reply(true);
        // Should get a reply like: 'AT+GSN\r\r\n8x3x7x0x6x0x6x5\r\n\r\nOK\r\n'
        char* pSrc = strstr(modem_reply, "AT+GSN\r\r\n");
        if (pSrc != NULL) {
            // Step to start of IMEI number
            pSrc += strlen("AT+GSN\r\r\n");
            // copy IMEI string
            bool allDone = false;
            for (char* pDst = pStr;
                 !allDone && ((pDst - pStr) < strSize-1);
                 ++pSrc) {
                if (isdigit(*pSrc)) {
                    // Copy over valid digit
                    *pDst++ = *pSrc;
                } else if (*pSrc == '\r') {
                    // End of IMEI number located
                    *pDst = '\0';
                    size_t len = pDst-pStr; // NOTE no +1 since pDst -> '\0'
                    if (len == IMEI_LEN) {
                        validIMEI = checkValidIMEI(pStr, len);
                        if (!validIMEI) {
                            debug_print(F("gsmGetIMEI(): Failed check : "));
                            debug_println(pStr);
                        }
                    }
                    allDone = true;
                } else {
                    // Found non-digit so abandon this try
                    allDone = true;
                }
            }
        }
    }
    return validIMEI;
}

/**
 * Configures the GSM modem
 */
void gsmConfigure() {
    // supply PIN code is needed
    gsm_set_pin();
    // get GSM IMEI
    if (!gsmGetIMEI(config.imei, DIM(config.imei), SECS(5))) {
        debug_print(F("gsmConfigure() read bad IMEI: "));
        debug_println(config.imei);
        // Replace bad IMEI with a string which when sent to the server
        // should not match a real IMEI
        strncopy(config.imei, BAD_IMEI, DIM(config.imei));
    } else {
        debug_print(F("gsmConfigure() read IMEI: "));
        debug_println(config.imei);
    }
    // misc GSM startup commands (disable echo)
    gsm_startup_cmd();
    // set GSM APN
    gsm_set_apn();
}

/**
 * Waits for the modem to have a connection to the GSM network
 * @param timeout the ms timeout we are prepared to wait for the connection
 *        to be made
 * @return true if a connection was made, false if not
 */
bool gsmWaitForConnection(
    unsigned timeout
) {
    unsigned tStart = millis();
    GSMSTATUS_T status = NOT_READY;
    while (timeDiff(millis(), tStart) < timeout) {
        status = gsmGetNetworkStatus();
        if (status == CONNECTED) {
            break;
        }
    }
    return (status == CONNECTED);
}

int gsm_disconnect(int waitForReply) {
    int ret = 0;
    debug_println(F("gsm_disconnect() started"));
    //disconnect GSM 
    snprintf(modem_command, sizeof(modem_command), "AT+QIDEACT");
    gsm_send_command();
    if (waitForReply) {
        gsm_wait_for_reply(0);
        //check if result contains DEACT OK
        char *tmp = strstr(modem_reply, "DEACT OK");
        if (tmp != NULL) {
            debug_println(F("gsm_disconnect(): DEACT OK found"));
            ret = 1;
        } else {
            debug_println(F("gsm_disconnect(): DEACT OK not found."));
        }
    }
    debug_println(F("gsm_disconnect() completed"));
    return ret;
}

int gsm_set_apn() {
    debug_println(F("gsm_set_apn() started"));
    //set all APN data, dns, etc
    snprintf(modem_command, sizeof(modem_command),
        "AT+QIREGAPP=\"%s\",\"%s\",\"%s\"", config.apn, config.user,
        config.pwd);
    gsm_send_command();
    gsm_wait_for_reply(true);
    snprintf(modem_command, sizeof(modem_command), "AT+QIDNSCFG=\"8.8.8.8\"");
    gsm_send_command();
    gsm_wait_for_reply(true);
    snprintf(modem_command, sizeof(modem_command), "AT+QIDNSIP=1");
    gsm_send_command();
    gsm_wait_for_reply(true);
    debug_println(F("gsm_set_apn() completed"));
    return 1;
}

int gsm_connect() {
    int ret = 0;
    debug_println(F("gsm_connect() started"));
    //try to connect multiple times
    for (int i = 0; i < CONNECT_RETRY; i++) {
        debug_print(F("Connecting to remote server..."));
        debug_println(i);
        //open socket connection to remote host
        //opening connection
        snprintf(modem_command, sizeof(modem_command),
            "AT+QIOPEN=\"%s\",\"%s\",\"%s\"", PROTO, HOSTNAME, HTTP_PORT);
        gsm_send_command();
        gsm_wait_for_reply(0);
        char *tmp = strstr(modem_reply, "CONNECT OK");
        if (tmp != NULL) {
            debug_print(F("Connected to remote server: "));
            debug_println(HOSTNAME);
            ret = 1;
            break;
        } else {
            debug_print(F("Can not connect to remote server: "));
            debug_println(HOSTNAME);
        }
    }
    debug_println(F("gsm_connect() completed"));
    return ret;
}

int gsm_validate_tcp() {
    char *str;
    int nonacked = 0;
    int ret = 0;
    char *tmp;
    char *tmpval;

    debug_println(F("gsm_validate_tcp() started."));
    //todo check in the loop if everything delivered
    for (int k = 0; k < 10; k++) {
        snprintf(modem_command, sizeof(modem_command), "AT+QISACK");
        gsm_send_command();
        gsm_wait_for_reply(true);
        //todo check if everything is delivered
        tmp = strstr(modem_reply, "+QISACK: ");
        tmp += strlen("+QISACK: ");
        tmpval = strtok(tmp, "\r");
        //checking how many bytes NON-acked
        str = strtok_r(tmpval, ", ", &tmpval);
        str = strtok_r(NULL, ", ", &tmpval);
        str = strtok_r(NULL, ", ", &tmpval);
        //non-acked value
        nonacked = atoi(str);
        if (nonacked <= PACKET_SIZE_DELIVERY) {
            //all data has been delivered to the server , if not wait and check again            
            debug_println(F("gsm_validate_tcp() data delivered."));
            ret = 1;
            break;
        } else {
            debug_println(F("gsm_validate_tcp() data not yet delivered."));
        }
    }
    debug_println(F("gsm_validate_tcp() completed."));
    return ret;
}

void gsm_send_http_current(const char* pServerMsg) {
    //send HTTP request, after connection if fully opened
    //this will send Current data
    debug_print(F("gsm_send_http(): sending data: "));
    debug_println(pServerMsg);
    //sending header                     
    snprintf(modem_data, sizeof(modem_data), "%s%d%s", HTTP_HEADER1,
        13 + strlen(config.imei) + strlen(config.key) + strlen(pServerMsg),
        HTTP_HEADER2);
    //sending header packet to remote host
    snprintf(modem_command, sizeof(modem_command), "AT+QISEND=%d",
        strlen(modem_data));
    gsm_send_command();
    gsm_wait_for_reply(true);
    gsm_send_tcp_data();
    gsm_validate_tcp();
    //sending imei and key first
    snprintf(modem_data, sizeof(modem_data), "imei=%s&key=%s&d=%s", config.imei,
        config.key, pServerMsg);
    snprintf(modem_command, sizeof(modem_command), "AT+QISEND=%d",
        strlen(modem_data));
    gsm_send_command();
    gsm_wait_for_reply(true);
    gsm_send_tcp_data();
    gsm_validate_tcp();
#if 0  
    tmp_len = strlen(pServerMsg);
    int chunk_len;
    int chunk_pos = 0;
    int chunk_check = 0;
    if (tmp_len > PACKET_SIZE) {
        chunk_len = PACKET_SIZE;
    } else {
        chunk_len = tmp_len;
    }
    int k = 0;
    for (int i = 0; i < tmp_len; i++) {
        if ((i == 0) || (chunk_pos >= PACKET_SIZE)) {
            if (chunk_pos >= PACKET_SIZE) {
                gsm_wait_for_reply(true);
                //validate previous transmission  
                gsm_validate_tcp();
                //next chunk, get chunk length, check if not the last one                            
                chunk_check = tmp_len - i;
                if (chunk_check > PACKET_SIZE) {
                    chunk_len = PACKET_SIZE;
                } else {
                    //last packet
                    chunk_len = chunk_check;
                }
                chunk_pos = 0;
            }
            //sending chunk
            snprintf(modem_command, sizeof(modem_command),
                "AT+QISEND=%d",
                chunk_len);
            gsm_send_command();
            gsm_wait_for_reply(true);
        }
        //sending data 
        gsm_port.print(pServerMsg[i]);
        chunk_pos++;
        k++;
    }
#endif
    debug_println(F("gsm_send_http(): data sent."));
}

void gsm_send_raw_current(const char* pServerMsg) {
    //send raw TCP request, after connection if fully opened
    //this will send Current data
    debug_print(F("gsm_send_raw(): sending data: "));
    debug_println(pServerMsg);

    int tmp_len = strlen(pServerMsg);
    int chunk_len;
    int chunk_pos = 0;
    int chunk_check = 0;

    if (tmp_len > PACKET_SIZE) {
        chunk_len = PACKET_SIZE;
    } else {
        chunk_len = tmp_len;
    }
    int k = 0;
    for (int i = 0; i < tmp_len; i++) {
        if ((i == 0) || (chunk_pos >= PACKET_SIZE)) {
            if (chunk_pos >= PACKET_SIZE) {
                gsm_wait_for_reply(true);
                //validate previous transmission
                gsm_validate_tcp();
                //next chunk, get chunk length, check if not the last one
                chunk_check = tmp_len - i;
                if (chunk_check > PACKET_SIZE) {
                    chunk_len = PACKET_SIZE;
                } else {
                    //last packet
                    chunk_len = chunk_check;
                }
                chunk_pos = 0;
            }
            //sending chunk
            snprintf(modem_command, sizeof(modem_command), "AT+QISEND=%d",
                chunk_len);
            gsm_send_command();
            gsm_wait_for_reply(true);
        }
        //sending data
        gsm_port.print(pServerMsg[i]);
        chunk_pos++;
        k++;
    }
    debug_println(F("gsm_send_raw(): data sent."));
}

int gsm_send_data(const char* pServerMsg) {
    int ret_tmp = 0;

    //disconnect GSM
    ret_tmp = gsm_disconnect(1);
    if (ret_tmp == 1) {
        debug_println(F("GPRS deactivated."));
    } else {
        debug_println(F("Error deactivating GPRS."));
    }
    // Disable TCP data echo
    snprintf(modem_command, sizeof(modem_command), "AT+QISDE=0");
    gsm_send_command();
    gsm_wait_for_reply(true);
    // Only allow a single TCP session
    snprintf(modem_command, sizeof(modem_command), "AT+QIMUX=0");
    gsm_send_command();
    gsm_wait_for_reply(true);
    //opening connection
    ret_tmp = gsm_connect();
    if (ret_tmp == 1) {
        //connection opened, just send data 
        if (SEND_RAW) {
            gsm_send_raw_current(pServerMsg);
        } else {
            gsm_send_http_current(pServerMsg);  //send all current data
        }
        if (!SEND_RAW) {
            //get reply and parse
            ret_tmp = parse_receive_reply(10000);
        }
        gsm_disconnect(1);
    } else {
        debug_println(F("Error, can not send data, no connection."));
        gsm_disconnect(1);
    }
    return ret_tmp;
}

void gsm_get_reply() {
    //get reply from the modem
    size_t index = strlen(modem_reply);
    while (gsm_port.available()) {
        char inChar = gsm_port.read(); // Read a character
        if (index == sizeof(modem_reply) - 1) {
            break;
        }
        modem_reply[index++] = inChar; // Store it
        if (inChar == '\n') {
            break;
        }
    }
    modem_reply[index] = '\0'; // Null terminate the string
}

bool gsm_wait_for_reply(bool allowOK) {
    unsigned long timeout = millis();
    bool gotReply = false;
    modem_reply[0] = '\0';
    gsm_get_reply();
    while (!(gotReply = gsm_is_final_result(allowOK))) {
        if ((millis() - timeout) >= (GSM_MODEM_COMMAND_TIMEOUT * 1000)) {
            debug_println(F("Warning: timed out waiting for modem reply"));
            break;
        }
        gsm_get_reply();
    }
    show_modem_reply();
    return gotReply;
}

void gsm_wait_at() {
    unsigned long timeout = millis();

    modem_reply[0] = '\0';
    while (!strncmp(modem_reply, "AT+", 3) == 0) {
        if ((millis() - timeout) >= (GSM_MODEM_COMMAND_TIMEOUT * 1000)) {
            debug_println(F("Warning: timed out waiting for last modem reply"));
            break;
        }
        gsm_get_reply();
    }
}

bool gsm_modem_reply_ends_with(const char* pText) {
    bool rCode = false;
    size_t reply_len = strlen(modem_reply);
    if (reply_len >= strlen(pText)) {
        if (strcmp(modem_reply + reply_len - 6, pText) == 0) {
            rCode = true;
        }
    }
    return rCode;
}

bool gsm_modem_reply_matches(size_t offset, const char* pMatch) {
    bool rCode = false;
    size_t reply_len = strlen(modem_reply);
    size_t match_len = strlen(pMatch);
    if (reply_len >= offset + match_len) {
        if (strncmp(modem_reply + offset, pMatch, match_len) == 0) {
            rCode = true;
        }
    }
    return rCode;
}

void show_modem_reply() {
    if (modemLogging) {
        debug_print(F("Modem Reply: '"));
        size_t reply_len = strlen(modem_reply);
        for (size_t idx = 0; idx < reply_len; ++idx) {
            switch (modem_reply[idx]) {
            case '\r':
                debug_print("\\r");
                break;
            case '\n':
                debug_print("\\n");
                break;
            default:
                if (isprint(modem_reply[idx])) {
                    debug_print(modem_reply[idx]);
                } else {
                    debug_print("\\?");
                }
                break;
            }
        }
        debug_println("'");
    }
}

/*!
 * Locates the start of the last line in the modem reply buffer.
 * e.g.
 *    modem_reply = ""                      returns 0
 *    modem_reply = "\n"                    returns 0
 *    modem_reply = "\r\n"                  returns 0
 *    modem_reply = "hello\r\n"             returns 0
 *    modem_reply = "hello\r\nthere"        returns 7
 *    modem_reply = "hello\r\nthere\r\n")   returns 7
 * @return the offset into modem_reply[] of the start of the last line
 */
size_t locate_last_line() {
    /* "xxxxxx/r/nxxxxxx/r/n" */
    /*            ^           */
    size_t pos = 0;
    size_t len = strlen(modem_reply);
    if (len > 1) {
        pos = len - 1;
        if (modem_reply[pos] == '\n') {
            pos -= 1;
        }
        while (pos > 0) {
            if (modem_reply[pos] == '\n') {
                ++pos;
                break;
            }
            pos -= 1;
        }
    }
    return pos;
}

bool gsm_is_final_result(bool allowOK) {
    if (allowOK && gsm_modem_reply_ends_with("\r\nOK\r\n")) {
        return true;
    }
    size_t last_line_index = locate_last_line();
    switch (modem_reply[last_line_index]) {
    case '+':
        if (gsm_modem_reply_matches(last_line_index + 1, "CME ERROR:")) {
            return true;
        }
        if (gsm_modem_reply_matches(last_line_index + 1, "CMS ERROR:")) {
            return true;
        }
        if (gsm_modem_reply_matches(last_line_index + 1, "CPIN:")) {
            return true;
        }
        return false;
    case '>':
        return gsm_modem_reply_matches(last_line_index + 1, " ");
    case 'B':
        return gsm_modem_reply_matches(last_line_index + 1, "USY\r\n");
    case 'C':
        if (gsm_modem_reply_matches(last_line_index + 1, "ONNECT OK\r\n")) {
            return true;
        }
        if (gsm_modem_reply_matches(last_line_index + 1, "ONNECT FAIL\r\n")) {
            return true;
        }
        if (gsm_modem_reply_matches(last_line_index + 1, "all Ready\r\n")) {
            return true;
        }
        return false;
    case 'D':
        return gsm_modem_reply_matches(last_line_index + 1, "EACT OK\r\n");
    case 'E':
        return gsm_modem_reply_matches(last_line_index + 1, "RROR\r\n");
    case 'N':
        if (gsm_modem_reply_matches(last_line_index + 1, "O ANSWER\r\n")) {
            return true;
        }
        if (gsm_modem_reply_matches(last_line_index + 1, "O CARRIER\r\n")) {
            return true;
        }
        if (gsm_modem_reply_matches(last_line_index + 1, "O DIALTONE\r\n")) {
            return true;
        }
        return false;
    case 'O':
        if (allowOK && gsm_modem_reply_matches(last_line_index + 1, "K\r\n")) {
            return true;
        }
        return false;
    case 'S':
        if (gsm_modem_reply_matches(last_line_index + 1, "END ")) {
            return true;
        }
        /* no break */
    default:
        return false;
    }
}

void gsm_debug() {
    gsm_port.print("AT+QLOCKF=?");
    gsm_port.print("\r");
    delay(2000);
    gsm_get_reply();
    gsm_port.print("AT+QBAND?");
    gsm_port.print("\r");
    delay(2000);
    gsm_get_reply();
    gsm_port.print("AT+CGMR");
    gsm_port.print("\r");
    delay(2000);
    gsm_get_reply();
    gsm_port.print("AT+CGMM");
    gsm_port.print("\r");
    delay(2000);
    gsm_get_reply();
    gsm_port.print("AT+CGSN");
    gsm_port.print("\r");
    delay(2000);
    gsm_get_reply();
    gsm_port.print("AT+CREG?");
    gsm_port.print("\r");
    delay(2000);
    gsm_get_reply();
    gsm_port.print("AT+CSQ");
    gsm_port.print("\r");
    delay(2000);
    gsm_get_reply();
    gsm_port.print("AT+QENG?");
    gsm_port.print("\r");
    delay(2000);
    gsm_get_reply();
    gsm_port.print("AT+COPS?");
    gsm_port.print("\r");
    delay(2000);
    gsm_get_reply();
    gsm_port.print("AT+COPS=?");
    gsm_port.print("\r");
    delay(6000);
    gsm_get_reply();
}

