typedef struct SMS_CMD_HANDLER {
    const char* pCommandName;
    void (*SMSCmdHandler)(const char* pPhoneNumber, const char* pValue);
};

SMS_CMD_HANDLER smsCommands[] = {
    { "apn", sms_apn_handler }, 
    { "gprspass", sms_gprspass_handler }, 
    { "gprsuser", sms_gprsuser_handler }, 
    { "smspass", sms_smspass_handler }, 
    { "pin", sms_pin_handler },
    { "int", sms_int_handler }, 
    { "locate", sms_locate_handler },
    { "smsnumber", sms_smsnumber_handler },
    { "smsint", sms_smsint_handler }
};

const size_t SMS_MAX_CMD_LEN = 80;

void sms_apn_handler(const char* pPhoneNumber, const char* pValue) {
    debug_print(F("sms_apn_handler(): APN: "));
    debug_println(pValue);
    if (strlen(pValue) > sizeof(config.apn) - 1) {
        sms_send_msg("Error: APN is too long", pPhoneNumber);
    } else {
        strcpy(config.apn, pValue);
        save_config = 1;
        power_reboot = 1;
        //send SMS reply  
        sms_send_msg("APN saved", pPhoneNumber);
    }
}

void sms_gprspass_handler(const char* pPhoneNumber, const char* pValue) {
    debug_print(F("sms_gprspass_handler(): gprs password: "));
    debug_println(pValue);
    if (strlen(pValue) > sizeof(config.pwd) - 1) {
        sms_send_msg("Error: gprs password is too long", pPhoneNumber);
    } else {
        strcpy(config.pwd, pValue);
        save_config = 1;
        power_reboot = 1;
        sms_send_msg("gprs password saved", pPhoneNumber);
    }
}

void sms_gprsuser_handler(const char* pPhoneNumber, const char* pValue) {
    debug_print(F("sms_gprsuser_handler(): gprs username: "));
    debug_println(pValue);
    if (strlen(pValue) > sizeof(config.user) - 1) {
        sms_send_msg("Error: gprs username is too long", pPhoneNumber);
    } else {
        strcpy(config.user, pValue);
        save_config = 1;
        power_reboot = 1;
        sms_send_msg("gprs username saved", pPhoneNumber);
    }
}

void sms_smspass_handler(const char* pPhoneNumber, const char* pValue) {
    debug_print(F("sms_smspass_handler(): sms password: "));
    debug_println(pValue);
    if (strlen(pValue) > sizeof(config.sms_key) - 1) {
        sms_send_msg("Error: sms password is too long", pPhoneNumber);
    } else {
        strcpy(config.sms_key, pValue);
        save_config = 1;
        sms_send_msg("sms password saved", pPhoneNumber);
    }
}

void sms_pin_handler(const char* pPhoneNumber, const char* pValue) {
    debug_print(F("sms_pin_handler(): sim pin: "));
    debug_println(pValue);
    if (strlen(pValue) > sizeof(config.sim_pin) - 1) {
        sms_send_msg("Error: sim pin is too long", pPhoneNumber);
    } else {
        strcpy(config.sim_pin, pValue);
        save_config = 1;
        power_reboot = 1;
        sms_send_msg("sim pin saved", pPhoneNumber);
    }
}

void sms_int_handler(const char* pPhoneNumber, const char* pValue) {
    debug_print(F("sms_int_handler(): update interval: "));
    debug_println(pValue);
    long updateSecs = atol(pValue);
    if (updateSecs <= 0) {
        sms_send_msg("Error: bad update interval", pPhoneNumber);
    } else {
        config.interval = updateSecs * 1000;
        save_config = 1;
        sms_send_msg("Update interval saved", pPhoneNumber);
    }
}

void sms_locate_handler(const char* pPhoneNumber, const char* pValue) {
    debug_println(F("sms_locate_handler(): Locate command received"));
    if ((strlen(lat_current) == 0) ||
        (strlen(lon_current) == 0)) {
        sms_send_msg("Current location not known yet", pPhoneNumber);
    } else {
        char msg[255];
        gps_form_location_url(msg, DIM(msg));
        sms_send_msg(msg, pPhoneNumber);
    }
}

void sms_smsnumber_handler(const char* pPhoneNumber, const char* pValue) {
    debug_print(F("sms_smsnumber_handler(): sms number: "));
    debug_println(pValue);
    if (strlen(pValue) > sizeof(config.sms_send_number) - 1) {
        sms_send_msg("Error: sms number is too long", pPhoneNumber);
    } else {
        strcpy(config.sms_send_number, pValue);
        save_config = 1;
        sms_send_msg("sms number saved", pPhoneNumber);
    }
}

void sms_smsint_handler(const char* pPhoneNumber, const char* pValue) {
    debug_print(F("sms_smsint_handler(): update sms interval: "));
    debug_println(pValue);
    long updateSecs = atol(pValue);
    if (updateSecs <= 0) {
        sms_send_msg("Error: bad update interval", pPhoneNumber);
    } else {
        config.sms_send_interval = updateSecs * 1000;
        save_config = 1;
        sms_send_msg("sms_smsint_handler interval saved", pPhoneNumber);
    }
}

/*
 * Extracts a field from an input string
 * @param pSource the string containing the input field
 * @param pDest where to save the extracted field characters
 * @param sizeDest the size of the buffer pointed to by pDest
 * @param pTerm a string containing characters which are considered as
 *        terminating characters for the field
 * @return a pointer to the input character that terminated the field. This
 *         could be a pointer to one of the terminator characters, a pointer
 *         to the end of the input string (i.e. the '\0') or the first
 *         character past the lat ome stored input pDest due to size
 *         restrictions.
 */
const char* sms_extract_field(
    const char* pSource,
    char* pDest,
    size_t sizeDest,
    const char* pTerm
) {
    size_t idx = 0;
    // Extract phone number
    while ((*pSource != '\0') &&
           (strchr(pTerm, *pSource) == NULL) &&
           (idx < sizeDest-1))
        pDest[idx++] = *pSource++;
    pDest[idx] = '\0';
    return pSource;
}

/*
 * Checks for having received a new text message and processes each new
 * message.
 * NOTE:
 *  We issue the modem AT+CMGL="REC UNREAD" command, which responds with:
 *    AT+CMGL="REC UNREAD"<CR>
 *    *{+CMGL: <index>,<stat>,<oa>,[<alpha>],[<scts>]<CR><LF><data><CR><LF>}
 *    <CR><LF>OK<CR><LF>
 *  Where:
 *    <stat>: Status = "REC UNREAD"
 *    <index>: Index number of the message
 *    <oa>: Originator address (telephone number)
 *    <alpha>: Originator name (if available in the phonebook)
 *    <scts>: Service Center Time Stamp
 *    <data>: The content of the text message
 *    <CR>: ASCII character 13
 *    <LF>: ASCII character 10
 *  e.g.
 *    AT+CMGL="REC UNREAD"\r\r\n+CMGL: 1,"REC UNREAD","+44xxxxxxxxxx","","2015/07/28 16:34:03+04"\r\n#xxxx,locate\r\n\r\nOK\r\n
 */
void sms_check() {
    debug_println(F("sms_check(): Started"));
    // Issue command to modem to read all unread messages
    snprintf(modem_command, sizeof(modem_command),
        "AT+CMGL=\"REC UNREAD\"");
    gsm_send_command();
    gsm_wait_for_reply(true);
    const char* msgStart = modem_reply;
    unsigned int msgCount = 0;
    while (msgStart != NULL) {
        // Locate start of next message response
        msgStart = strstr(msgStart, "+CMGL:");
        // If found ...
        if (msgStart != NULL) {
            // ... process the message and move past it
            msgStart = sms_process(msgStart);
            ++msgCount;
        }
    }
    debug_print(F("Processed "));
    debug_print(msgCount);
    debug_println(F(" SMS messages"));
    // remove all READ and SENT sms
    snprintf(modem_command, sizeof(modem_command),
        "AT+QMGDA=\"DEL READ\"");
    gsm_send_command();
    gsm_wait_for_reply(true);
    snprintf(modem_command, sizeof(modem_command),
        "AT+QMGDA=\"DEL SENT\"");
    gsm_send_command();
    gsm_wait_for_reply(true);
    debug_println(F("sms_check(): Stopped"));
}

/*
 * Consumes the modem response for one SMS message and returns a
 * pointer to just after the end of the consumed message.
 * @param msgStart points to the start of the message (->"+CMGL")
 *    +CMGL: <index>,<stat>,<oa>,[<alpha>],[<scts>]<CR><LF><data><CR><LF>
 *    e.g.
 *    +CMGL: 1,"REC UNREAD","+31628870634",,"11/01/09,10:26:26+04"
 *    This is text message 1
 * @return pointer to the character after the processed message
 */
const char* sms_process(const char* msgStart) {
    char phoneNumber[MAX_PHONE_NUMBER_LEN+1];
    char message[SMS_MAX_CMD_LEN+1];
    const char* pos = strchr(msgStart, ','); // after <index>,
    if (pos != NULL) {
        pos = strchr(pos+1, ','); // after <stat>,
        if (pos != NULL) {
            if (pos[1] == '"') {
                pos += 2; // start of phone number
                pos = sms_extract_field(
                    pos, phoneNumber, DIM(phoneNumber), "\"");
                pos = strchr(pos, LF);
                if (pos != NULL) {
                    pos += 1; // start of <data>
                    pos = sms_extract_field(
                        pos, message, DIM(message), "\r");
                    sms_cmd(message, phoneNumber);
                }
            }
        }
    }
    return pos;
}

/*
 * Process the content of a received SMS message. The message should have the
 * following format:
 *   #<sms_key>,<command>=<value>
 * @param pSMSMessage points to the received SMS message
 * @param pPhone points to the phone number the SMS message was received from
 */
void sms_cmd(char *pSMSMessage, char *pPhone) {
    const char* pCmd = pSMSMessage;
    while (isspace(*pCmd))
        ++pCmd;
    if (*pCmd != '#') {
        debug_print(F("sms_cmd(): SMS message did not start with #: "));
        debug_println(pSMSMessage);
    } else {
        ++pCmd; // Skip over '#'
        char smsKey[MAX_SMS_KEY_LEN+1];
        // Extract SMS password/key
        pCmd = sms_extract_field(
            pCmd, smsKey, DIM(smsKey), ",");
        if (*pCmd != ',') {
            debug_print(F("sms_cmd(): SMS message missing SMS key delimiter: "));
            debug_println(pSMSMessage);
        } else {
            if (strcmp(smsKey, config.sms_key) != 0) {
                debug_print(F("sms_cmd(): SMS message had bad SMS key: "));
                debug_println(pSMSMessage);
            } else {
                pCmd += 1; // Skip over ',' following SMS key
                sms_cmd_run(pCmd, pPhone);
            }
        }
    }
}

void sms_cmd_run(const char *pRequest, const char *pPhoneNumber) {
    char command[SMS_MAX_CMD_LEN + 1];
    char value[100];
    while (isspace(*pRequest))
        ++pRequest;
    // Extract the command
    pRequest = sms_extract_field(
        pRequest, command, DIM(command), "=");
    const char* pValue = NULL;
    if (*pRequest == '=') {
        // Extract the value
        pRequest = sms_extract_field(
            pRequest, value, DIM(value), "\r");
        pValue = value;
    }
    bool found = false;
    for (int idx = 0; !found && (idx < DIM(smsCommands)); ++idx) {
        if (stricmp(smsCommands[idx].pCommandName, command) == 0) {
            smsCommands[idx].SMSCmdHandler(pPhoneNumber, pValue);
            found = true;
        }
    }
    if (!found) {
        debug_println(F("sms_cmd_run(): Received unknown command: "));
        debug_println(command);
    }
}

void sms_send_msg(const char *cmd, const char *phone) {
    //send SMS message to number 
    debug_print(F("Sending SMS to:"));
    debug_print(phone);
    debug_print(F(" : "));
    debug_println(cmd);

    snprintf(modem_command, sizeof(modem_command),
        "AT+CMGS=\"%s\"", phone);
    gsm_send_command();
    gsm_wait_for_reply(false);
    char *tmp = strstr(modem_reply, ">");
    if (tmp != NULL) {
        gsm_port.print(cmd);
        //sending ctrl+z
        gsm_port.print("\x1A");
        gsm_wait_for_reply(1);
    }
}

