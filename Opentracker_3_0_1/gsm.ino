//gsm functions
char modem_command[256];  // Modem AT command buffer
char modem_data[PACKET_SIZE]; // Modem TCP data buffer
char modem_reply[200];    //data received from modem, max 200 chars

void gsm_setup() {
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

void gsm_on_off() {
    //turn on the modem
    debug_println(F("gsm_on_off() started"));
    digitalWrite(PIN_C_PWR_GSM, HIGH);
    delay(4000);
    digitalWrite(PIN_C_PWR_GSM, LOW);
    debug_println(F("gsm_on_off() finished"));
}

void gsm_restart() {
    debug_println(F("gsm_restart() started"));
    //blink modem restart
    for (int i = 0; i < 5; i++) {
        if (ledState == LOW)
            ledState = HIGH;
        else
            ledState = LOW;
        digitalWrite(PIN_POWER_LED, ledState);   // set the LED on
        delay(200);
    }
    //restart modem
    //check if modem is ON (PWRMON is HIGH)
    int pwrmon = digitalRead(PIN_STATUS_GSM);
    if (pwrmon == HIGH) {
        //modem already on, turn modem off 
        gsm_on_off();
        delay(5000);    //wait for modem to shutdown         
    }
    //turn on modem   
    gsm_on_off();
    debug_println(F("gsm_restart() completed"));
}

void gsm_send_command() {
    modem_reply[0] = '\0';
    debug_print(F("gsm_send_command(): "));
    debug_println(modem_command);
    gsm_port.print(modem_command);
    gsm_port.print("\r");
}

void gsm_send_tcp_data() {
    debug_print(F("gsm_send_tcp_data(): "));
    debug_println(modem_data);
    gsm_port.print(modem_data);
}

void gsm_set_pin() {
    debug_println(F("gsm_set_pin() started"));
    //checking if PIN is set 
    snprintf(modem_command, sizeof(modem_command), "AT+CPIN?");
    gsm_send_command();
    gsm_wait_for_reply(1);
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
                gsm_wait_for_reply(1);
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
 */
void gsm_get_time(char* pStr, size_t strSize) {
    int i;
    //clean any serial data       
    gsm_get_reply();
    //get time from modem
    snprintf(modem_command, sizeof(modem_command), "AT+CCLK?");
    gsm_send_command();
    gsm_wait_for_reply(1);
    char *tmp = strstr(modem_reply, "+CCLK: \"");
    tmp += strlen("+CCLK: \"");
    char *tmpval = strtok(tmp, "\"");
    //copy data to main time var
    for (i = 0; i < strlen(tmpval); i++) {
        pStr[i] = tmpval[i];
        if (i > 17) {  //time can not exceed 20 chars
            break;
        }
    }
    //null terminate time
    pStr[i + 1] = '\0';
}

void gsm_startup_cmd() {
    debug_println(F("gsm_startup_cmd() started"));
    //disable echo for TCP data
    snprintf(modem_command, sizeof(modem_command), "AT+QISDE=0");
    gsm_send_command();
    gsm_wait_for_reply(1);
    //set receiving TCP data by command
    snprintf(modem_command, sizeof(modem_command), "AT+QINDI=1");
    gsm_send_command();
    gsm_wait_for_reply(1);
    //set SMS as text format
    snprintf(modem_command, sizeof(modem_command), "AT+CMGF=1");
    gsm_send_command();
    gsm_wait_for_reply(1);
    debug_println(F("gsm_startup_cmd() completed"));
}

void gsm_get_imei() {
    int i;
    char preimei[20];             //IMEI number
    debug_println(F("gsm_get_imei() started"));
    //get modem's imei 
    snprintf(modem_command, sizeof(modem_command), "AT+GSN");
    gsm_send_command();
    gsm_wait_for_reply(1);
    //reply data stored to modem_reply[200]
    char *tmp = strstr(modem_reply, "AT+GSN\r\r\n");
    tmp += strlen("AT+GSN\r\r\n");
    char *tmpval = strtok(tmp, "\r");
    //copy data to main IMEI var
    for (i = 0; i < strlen(tmpval); i++) {
        preimei[i] = tmpval[i];
        if (i > 17) { //imei can not exceed 20 chars
            break;
        }
    }
    //null terminate imei
    preimei[i + 1] = '\0';
    debug_println(F("gsm_get_imei() result:"));
    debug_println(preimei);
    memcpy(config.imei, preimei, 20);
    debug_println(F("gsm_get_imei() completed"));
}

void gsm_send_at() {
    debug_println(F("gsm_send_at() started"));
    gsm_port.print("AT");
    gsm_port.print("\r");
    gsm_port.print("AT");
    gsm_port.print("\r");
    debug_println(F("gsm_send_at() completed"));
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
    gsm_wait_for_reply(1);
    snprintf(modem_command, sizeof(modem_command), "AT+QIDNSCFG=\"8.8.8.8\"");
    gsm_send_command();
    gsm_wait_for_reply(1);
    snprintf(modem_command, sizeof(modem_command), "AT+QIDNSIP=1");
    gsm_send_command();
    gsm_wait_for_reply(1);
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
        gsm_wait_for_reply(1);
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
    gsm_wait_for_reply(1);
    gsm_send_tcp_data();
    gsm_validate_tcp();
    //sending imei and key first
    snprintf(modem_data, sizeof(modem_data), "imei=%s&key=%s&d=%s", config.imei,
        config.key, pServerMsg);
    snprintf(modem_command, sizeof(modem_command), "AT+QISEND=%d",
        strlen(modem_data));
    gsm_send_command();
    gsm_wait_for_reply(1);
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
                gsm_wait_for_reply(1);
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
            gsm_wait_for_reply(1);
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
                gsm_wait_for_reply(1);
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
            gsm_wait_for_reply(1);
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

    //send 2 ATs
    gsm_send_at();
    gsm_wait_for_reply(1);
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
    gsm_wait_for_reply(1);
    // Only allow a single TCP session
    snprintf(modem_command, sizeof(modem_command), "AT+QIMUX=0");
    gsm_send_command();
    gsm_wait_for_reply(1);
    //opening connection
    ret_tmp = gsm_connect();
    if (ret_tmp == 1) {
        //connection opened, just send data 
        if (SEND_RAW) {
            gsm_send_raw_current(pServerMsg);
        } else {
            gsm_send_http_current(pServerMsg);  //send all current data
        }
        gsm_wait_for_reply(1);
        if (!SEND_RAW) {
            //get reply and parse
            ret_tmp = parse_receive_reply();
        }
        gsm_disconnect(1);
        gsm_send_failures = 0;
    } else {
        debug_println(F("Error, can not send data, no connection."));
        gsm_disconnect(1);
        gsm_send_failures++;
        if (GSM_SEND_FAILURES_REBOOT > 0
            && gsm_send_failures >= GSM_SEND_FAILURES_REBOOT) {
            powerReboot = 1;
        }
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

void gsm_wait_for_reply(int allowOK) {
    unsigned long timeout = millis();

    modem_reply[0] = '\0';
    gsm_get_reply();
    while (!gsm_is_final_result(allowOK)) {
        if ((millis() - timeout) >= (GSM_MODEM_COMMAND_TIMEOUT * 1000)) {
            debug_println(F("Warning: timed out waiting for modem reply"));
            break;
        }
        delay(50);
        gsm_get_reply();
    }
    show_modem_reply();
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
        delay(50);
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

