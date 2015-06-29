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
    { "locate", sms_locate_handler }
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
        power_reboot = 1;
        sms_send_msg("Update interval saved", pPhoneNumber);
    }
}

void sms_locate_handler(const char* pPhoneNumber, const char* pValue) {
    debug_println(F("sms_locate_handler(): Locate command received"));
    char msg[255];
    if (LOCATE_COMMAND_FORMAT_IOS) {
        snprintf(msg, 255, "comgooglemaps://?q=%s,%s", lat_current,
            lon_current);
    } else {
        snprintf(msg, 255, "https://maps.google.co.uk/maps/place/%s,%s",
            lat_current, lon_current);
    }
    sms_send_msg(msg, pPhoneNumber);
}

//check SMS 
void sms_check() {
    char index;
    byte cmd;
    int reply_index = 0;
    char *tmp, *tmpcmd;

    debug_println(F("sms_check(): Started"));
    snprintf(modem_command, sizeof(modem_command),
        "AT+CMGL=\"REC UNREAD\"");
    gsm_send_command();
    gsm_wait_at();

    for (int i = 0; i < 30; i++) {
        if (gsm_port.available()) {
            while (gsm_port.available()) {
                index = gsm_port.read();
                debug_port.print(index);
                if (index == '#') {
                    //next data is probably command till \r 
                    //all data before "," is sms password, the rest is command
                    cmd = 1;
                    //get phone number 
                    modem_reply[reply_index] = '\0';
                    //phone info will look like this: +CMGL: 10,"REC READ","+436601601234","","5 12:13:17+04"
                    //phone will start from ","+  and end with ",
                    tmp = strstr(modem_reply, "+CMGL:");
                    if (tmp != NULL) {
                        tmp = strstr(modem_reply, "\",\"+");
                        tmp += strlen("\",\"+");
                        tmpcmd = strtok(tmp, "\",\"");
                    }
                    reply_index = 0;
                } else if (index == '\r') {
                    if (cmd == 1) {
                        modem_reply[reply_index] = '\0';
                        sms_cmd(modem_reply, tmpcmd);
                        reply_index = 0;
                        cmd = 0;
                    }
                } else {
                    if (cmd == 1) {
                        modem_reply[reply_index] = index;
                        reply_index++;
                    } else {
                        if (reply_index < 200) {
                            modem_reply[reply_index] = index;
                            reply_index++;
                        } else {
                            reply_index = 0;
                        }
                    }
                }
            }
        }
        delay(150);
    }
    //remove all READ and SENT sms   
    snprintf(modem_command, sizeof(modem_command),
        "AT+QMGDA=\"DEL READ\"");
    gsm_send_command();
    gsm_wait_for_reply(1);
    snprintf(modem_command, sizeof(modem_command),
        "AT+QMGDA=\"DEL SENT\"");
    gsm_send_command();
    gsm_wait_for_reply(1);
    debug_println(F("sms_check(): Stopped"));
}

void sms_cmd(char *cmd, char *phone) {
    char *tmp;
    int i = 0;

    //command separated by "," format: password,command=value
    while ((tmp = strtok_r(cmd, ",", &cmd)) != NULL) {
        if (i == 0) {
            //checking password
            if (strcmp(tmp, config.sms_key) == 0) {
                sms_cmd_run(cmd, phone);
                break;
            } else {
                debug_print(F("sms_cmd(): SMS password failed: "));
                debug_println(tmp);
            }
        }
        i++;
    }
}

void sms_cmd_run(char *pRequest, char *pPhoneNumber) {
    while (isspace(*pRequest))
        ++pRequest;
    size_t cmdLen = strcspn(pRequest, "=");
    char command[SMS_MAX_CMD_LEN + 1];
    strncpy(command, pRequest, MIN(cmdLen, sizeof(command) - 1));
    command[sizeof(command) - 1] = '\0';
    const char* pValue = pRequest + cmdLen + 1;
    if (pRequest[cmdLen] == '\0') {
        pValue = NULL;
    } else {
        while (isspace(*pValue))
            ++pValue;
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
        "AT+CMGS=\"+%s\"", phone);
    gsm_send_command();
    gsm_wait_for_reply(0);
    char *tmp = strstr(modem_reply, ">");
    if (tmp != NULL) {
        gsm_port.print(cmd);
        //sending ctrl+z
        gsm_port.print("\x1A");
        gsm_wait_for_reply(1);
    }
}

