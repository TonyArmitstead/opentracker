/**
 * Sets the IO pins (directions) for the modem
 */
void gsmSetupPIO() {
    //setup modem pins
    pinMode(PIN_C_PWR_GSM, OUTPUT);
    digitalWrite(PIN_C_PWR_GSM, LOW);
    pinMode(PIN_C_KILL_GSM, OUTPUT);
    digitalWrite(PIN_C_KILL_GSM, LOW);
    pinMode(PIN_STATUS_GSM, INPUT);
    pinMode(PIN_RING_GSM, INPUT);
    pinMode(PIN_WAKE_GSM, INPUT);
}

/**
 * Powers on the gsm modem
 * @returns true if gsm modem powered up ok, false if not
 */
bool gsmPowerOn() {
    bool isPoweredOn = false;
    if (digitalRead(PIN_STATUS_GSM) == LOW) { // i.e. is OFF
        // turn on the modem
        digitalWrite(PIN_C_PWR_GSM, HIGH);
        delay(1000);
        // Wait for chip to power up
        unsigned long tStart = millis();
        while (timeDiff(millis(), tStart) < 2000) {
            if (digitalRead(PIN_STATUS_GSM) == HIGH) {
                debug_println(F("gsmPowerOn: GSM modem powered on"));
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
        debug_println(F("gsmPowerOn: GSM modem already on"));
        isPoweredOn = true;
    }
    return isPoweredOn;
}

/**
 * Powers off the gsm modem
 */
void gsmPowerOff() {
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
                debug_println(F("gsmPowerOff: GSM modem powered down"));
                break;
            }
        }
    } else {
        debug_println(F("gsmPowerOff: GSM modem already off"));
    }
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
 * Sends a command string to the modem
 * @param pCommand the command to send (no trailing \n required)
 * @return true if the command was accepted by the modem. Any response is
 *         held in modem_reply[]
 */
bool gsmSendModemCommand(
    const char* pCommand
) {
    snprintf(modem_command, sizeof(modem_command), pCommand);
    gsmWriteCommand();
    return gsmWaitForReply(true);
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
        if (gsmSendModemCommand("AT")) {
            debug_println(F("gsmSyncComms: succeeded"));
            return true;
        }
    }
    debug_println(F("gsmSyncComms: failed"));
    return false;
}

/**
 * Determines the modem connection status i.e. whether the modem is
 * currently operational
 * @returns one of the GSMSTATUS_E values
 */
GSMSTATUS_T gsmGetNetworkStatus() {
    GSMSTATUS_T status = NOT_READY;
    gsmSendModemCommand("AT+QNSTATUS");
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

/**
 * Writes modem_command[] to the modem UART. The ASCIZ string in modem_command
 * does not need the '\r' as the last character.
 */
void gsmWriteCommand() {
    // Empty out any residual received data
    while (gsm_port.available()) {
        (void)gsm_port.read();
    }
    modem_reply[0] = '\0';
    if (modemLogging) {
        debug_print(F("gsmWriteCommand: "));
        debug_println(modem_command);
    }
    gsm_port.print(modem_command);
    gsm_port.print("\r");
}

/**
 * Unlocks the modem by setting any configured modem PIN
 * @return true if the pin is not required or is set OK
 */
bool gsmSetPin() {
    bool rStat = true;
    //checking if PIN is set 
    gsmSendModemCommand("AT+CPIN?");
    char *tmp = strstr(modem_reply, "SIM PIN");
    if (tmp != NULL) {
        debug_println(F("gsm_set_pin: PIN is required"));
        //checking if pin is valid one
        if (config.sim_pin[0] == -1) {
            debug_println(F("gsm_set_pin: PIN is not supplied."));
        } else {
            if (strlen(config.sim_pin) == 4) {
                debug_println(
                    F("gsm_set_pin: PIN supplied, sending to modem."));
                gsm_port.print("AT+CPIN=");
                gsm_port.print(config.sim_pin);
                gsm_port.print("\r");
                gsmWaitForReply(true);
                tmp = strstr(modem_reply, "OK");
                if (tmp != NULL) {
                    debug_println(F("gsm_set_pin: PIN is accepted"));
                } else {
                    debug_println(F("gsm_set_pin: PIN is not accepted"));
                }
            } else {
                debug_println(
                    F("gsm_set_pin(): PIN supplied, but has invalid length."
                      " Not sending to modem."));
                rStat = false;
            }
        }
    } else {
        debug_println(F("gsm_set_pin: PIN is not required"));
    }
    return rStat;
}

/**
 * Reads a time string from the modem
 * @param pStr points to where to write the string
 * @param strSize the storage size for pStr
 * @return true if we read a valid time, false if not
 * 
 * gsmWriteCommand(): AT+QLTS
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
        gsmSendModemCommand("AT+QLTS");
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

/**
 * Checks that a string of digits makes up a valid IMEI number
 * @param pIMEI points to the string of digits
 * @param lenIMEI number of characters in pIMEI
 * @returns true if digitd check out, false if not
 * @note see http://imei-number.com/check-digit-calculation
 */
bool gsmCheckValidIMEI(
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
        gsmSendModemCommand("AT+GSN");
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
                        validIMEI = gsmCheckValidIMEI(pStr, len);
                        if (!validIMEI) {
                            debug_print(F("gsmGetIMEI: Failed check : "));
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
 * Sets the model APN value and configures the DSN server to Google's server
 * @return true if all configured OK
 */
bool gsmSetAPN() {
    bool rStat = true;
    // set all APN data, dns, etc
    snprintf(modem_command, sizeof(modem_command),
        "AT+QIREGAPP=\"%s\",\"%s\",\"%s\"", config.apn, config.user,
        config.pwd);
    gsmWriteCommand();
    rStat = rStat && gsmWaitForReply(true);
    rStat = rStat && gsmSendModemCommand("AT+QIDNSCFG=\"8.8.8.8\"");
    rStat = rStat && gsmSendModemCommand("AT+QIDNSIP=1");
    return rStat;
}

/**
 * Configures the GSM modem
 * @return true if all configured OK
 */
bool gsmConfigure() {
    bool rStat = true;
    // supply PIN code is needed
    rStat = rStat && gsmSetPin();
    // get GSM IMEI
    if (!gsmGetIMEI(config.imei, DIM(config.imei), SECS(5))) {
        debug_print(F("gsmConfigure: read bad IMEI: "));
        debug_println(config.imei);
        // Replace bad IMEI with a string which when sent to the server
        // should not match a real IMEI
        strncopy(config.imei, BAD_IMEI, DIM(config.imei));
        rStat = false;
    } else {
        debug_print(F("gsmConfigure: read IMEI: "));
        debug_println(config.imei);
    }
    //disable echo for TCP data
    rStat = rStat && gsmSendModemCommand("AT+QISDE=0");
    //set receiving TCP data by command
    rStat = rStat && gsmSendModemCommand("AT+QINDI=1");
    //set SMS as text format
    rStat = rStat && gsmSendModemCommand("AT+CMGF=1");
    // Request network time sync
    rStat = rStat && gsmSendModemCommand("AT+QNITZ=1");
    // Request local time saved to RTC time
    rStat = rStat && gsmSendModemCommand("AT+CTZU=3");
    // Report network time
    rStat = rStat && gsmSendModemCommand("AT+QLTS");
    // set GSM APN
    rStat = rStat && gsmSetAPN();
    return rStat;
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

/**
 * Disconnects the modem from any currently connected TCP host
 * @param waitForReply true if you want us to correctly wait for a response
 *        from the modem (you normally want this set to true)
 * @return true if disconnected OK, false if not
 */
bool gsmDisconnect(
    bool waitForReply
) {
    bool rStat = true;
    //disconnect GSM 
    snprintf(modem_command, sizeof(modem_command), "AT+QIDEACT");
    gsmWriteCommand();
    if (waitForReply) {
        gsmWaitForReply(false);
        //check if result contains DEACT OK
        char *tmp = strstr(modem_reply, "DEACT OK");
        if (tmp == NULL) {
            debug_println(F("gsmDisconnect: DEACT OK not found."));
            rStat = false;
        }
    }
    return rStat;
}

/**
 * Connects the modem to a TCP host
 * @return true if connected OK, false if not
 */
bool gsmConnect() {
    bool rStat = false;
    //try to connect multiple times
    for (int i = 0; i < CONNECT_RETRY; i++) {
        debug_print(F("Connecting to remote server..."));
        debug_println(i);
        //open socket connection to remote host
        //opening connection
        snprintf(modem_command, sizeof(modem_command),
            "AT+QIOPEN=\"%s\",\"%s\",\"%s\"", PROTO, HOSTNAME, HTTP_PORT);
        gsmWriteCommand();
        gsmWaitForReply(false);
        char *tmp = strstr(modem_reply, "CONNECT OK");
        if (tmp != NULL) {
            debug_print(F("Connected to remote server: "));
            debug_println(HOSTNAME);
            rStat = true;
            break;
        } else {
            debug_print(F("Failed to connect to remote server: "));
            debug_println(HOSTNAME);
        }
    }
    return rStat;
}

void gsm_send_tcp_data() {
    if (modemLogging) {
        debug_print(F("gsm_send_tcp_data: "));
        debug_println(modem_data);
    }
    gsm_port.print(modem_data);
}

int gsm_validate_tcp() {
    char *str;
    int nonacked = 0;
    int ret = 0;
    char *tmp;
    char *tmpval;

    //todo check in the loop if everything delivered
    for (int k = 0; k < 10; k++) {
        gsmSendModemCommand("AT+QISACK");
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
            ret = 1;
            break;
        } else {
            debug_println(F("gsm_validate_tcp: data not yet delivered."));
        }
    }
    return ret;
}

/**
 * Sends one or more messages to the server
 * @param pServerMessages points to an array of pointers to messages. Each
 *        message is a '\0' terminated string
 * @return true if all messages set ok, false if not
 */
bool gsmSendServerMessages(
    const char** pServerMessages,
    size_t count
) {
    bool allSentOK = true;
    if (!gsmDisconnect(true)) {
        debug_println(F("gsmSendServerMessages: Error deactivating GPRS"));
    }
    // Disable TCP data echo
    gsmSendModemCommand("AT+QISDE=0");
    // Only allow a single TCP session
    gsmSendModemCommand("AT+QIMUX=0");
    // opening connection
    if (gsmConnect()) {
        // connection opened, send all messages
        while (count--) {
            if (!gsmSendServerMessage(*pServerMessages)) {
                allSentOK = false;
            }
            ++pServerMessages;
        }
        gsmDisconnect(true);
    } else {
        debug_println(F("Error, cannot send data, no connection"));
        gsmDisconnect(true);
        allSentOK = false;
    }
    return allSentOK;
}

/**
 * Sends a single message to the server. Note that this call assumes we have
 * already connected to the server
 * @param pServerMsg points to an ASCIZ string containing the message to send
 * @return true if message sent ok
 */
bool gsmSendServerMessage(
    const char* pServerMsg
) {
    debug_print(F("gsmSendServerMessage: sending data: "));
    debug_println(pServerMsg);
    // sending HTTP header
    snprintf(modem_data, sizeof(modem_data), "%s%d%s", HTTP_HEADER1,
        13 + strlen(config.imei) + strlen(config.key) + strlen(pServerMsg),
        HTTP_HEADER2);
    snprintf(modem_command, sizeof(modem_command), "AT+QISEND=%d",
        strlen(modem_data));
    gsmWriteCommand();
    gsmWaitForReply(true);
    gsm_send_tcp_data();
    gsm_validate_tcp();
    // sending imei and key first
    snprintf(modem_data, sizeof(modem_data), "imei=%s&key=%s&d=%s", config.imei,
        config.key, pServerMsg);
    snprintf(modem_command, sizeof(modem_command), "AT+QISEND=%d",
        strlen(modem_data));
    gsmWriteCommand();
    gsmWaitForReply(true);
    gsm_send_tcp_data();
    gsm_validate_tcp();
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

/**
 * Waits for the modem to respond to a command
 * @param allowOK when true, indicates that a modem response of "OK" is enough
 *        to indicate the end of the modem response. Some commands send an
 *        "OK" response and then send some data, so for these you dont want
 *        to pass this is as true.
 * @return true if the modem response arrived within the timeout period
 */
bool gsmWaitForReply(
    bool allowOK
) {
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

