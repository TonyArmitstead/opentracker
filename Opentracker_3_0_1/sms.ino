/**
 * Each SMS command handler is identified by a string and a function to
 * handle the command value
 */
typedef struct SMS_CMD_HANDLER_S {
    const char* pCommandName;
    void (*SMSCmdHandler)(const char* pPhoneNumber, const char* pValue);
} SMS_CMD_HANDLER_T;
/**
 * The set of supported SMS commands
 */
SMS_CMD_HANDLER_T smsCommands[] = {
    { "apn", sms_apn_handler },
    { "gprspass", sms_gprspass_handler },
    { "gprsuser", sms_gprsuser_handler },
    { "smskey", sms_smskey_handler },
    { "pin", sms_pin_handler },
    { "sint", sms_sint_handler },
    { "fint", sms_fint_handler },
    { "locate", sms_locate_handler },
    { "smsnumber", sms_smsnumber_handler },
    { "smsfreq", sms_smsfreq_handler },
    { "smssend", sms_smssend_handler },
    { "srvsend", sms_srvsend_handler },
    { "gsmrestart", sms_gsmrestart_handler },
    { "reboot", sms_reboot_handler }
};
/**
 * Max size of an SMS command string
 */
const size_t SMS_MAX_CMD_LEN = 144;
/**
 * The value strings for ON/OFF configuration fields
 */
const char* ON_OFF_VALUES[] = {"off", "on"};
/**
 * Declare a structure for defining a bit field within an integer value
 */
typedef struct SMS_FIELD_SPEC_S {
    const char* pFieldName;     // The name of the field
    unsigned int fieldStartPos; // The position of the field LSB
    unsigned int fieldMask;     // A mask for the field assuming it has
                                // been right justified (starts at bit 0)
    const char** fieldValues;   // points to an array of string values, one
                                // for each allowed binary value.
                                // The matching index value is taken to be the
                                // binary field value. Unused index values
                                // should be filled with an empty string or
                                // NULL(0)
} SMS_FIELD_SPEC_T;
/**
 * Helper macros for defining generic fields and OFF/ON fields
 */
#define SMS_FIELD(field, class, name, values) \
  {field, class##_SEND_##name##_POS, class##_SEND_##name##_MASK, \
        values}
#define SMS_ONOFF_FIELD(field, class, name) \
  SMS_FIELD(field, class, name, ON_OFF_VALUES)
/**
 * The value strings for the location format field
 */
const char* LOCFMT_VALUES[] = {"web", "map", "val", NULL};
/**
 * Declare the known SMS configuration field names and bit positions, along
 * with the set of allowed values (mapped to strings)
 */
SMS_FIELD_SPEC_T sms_msg_config_fields[] = {
    SMS_ONOFF_FIELD("loc", SMS, LOCATION),
    SMS_FIELD("locfmt", SMS, LOCATION_FORMAT, LOCFMT_VALUES),
    SMS_ONOFF_FIELD("nsat", SMS, NSAT),
    SMS_ONOFF_FIELD("alt", SMS, ALT),
    SMS_ONOFF_FIELD("speed", SMS, SPEED),
    SMS_ONOFF_FIELD("ign", SMS, IGN)
};

/**
 * Declare the known Server configuration field names and bit positions, along
 * with the set of allowed values (mapped to strings)
 */
SMS_FIELD_SPEC_T sms_server_config_fields[] = {
    SMS_ONOFF_FIELD("date", SERVER, GPSDATE),
    SMS_ONOFF_FIELD("time", SERVER, GPSTIME),
    SMS_ONOFF_FIELD("lat", SERVER, LATITUDE),
    SMS_ONOFF_FIELD("lon", SERVER, LONGITUDE),
    SMS_ONOFF_FIELD("speed", SERVER, SPEED),
    SMS_ONOFF_FIELD("alt", SERVER, ALTITUDE),
    SMS_ONOFF_FIELD("head", SERVER, HEADING),
    SMS_ONOFF_FIELD("hdop", SERVER, HDOP),
    SMS_ONOFF_FIELD("nsat", SERVER, NSAT),
    SMS_ONOFF_FIELD("bat", SERVER, BATT),
    SMS_ONOFF_FIELD("ign", SERVER, IGN),
    SMS_ONOFF_FIELD("run", SERVER, RUNTIME)
};

/**
 * Forms a string containing the configured set of SMS data return values
 * @param pStr points to where to write the string
 * @param strSize the storage size for pStr
 * @param pGPSData points to the GPS data to send
 * @param ignState ignition state, true for ON
 */
void sms_form_sms_update_str(
    char* pStr,
    size_t strSize,
    GPSDATA_T* pGPSData,
    bool ignState
) {
    char* pos = pStr;
    char* lineStart = pos;
    if (((config.sms_send_flags >> SMS_SEND_LOCATION_POS)
                                 & SMS_SEND_LOCATION_MASK)
                                == SMS_SEND_LOCATION_ON) {
        switch (((config.sms_send_flags >> SMS_SEND_LOCATION_FORMAT_POS)
                                         & SMS_SEND_LOCATION_FORMAT_MASK)) {
        case SMS_SEND_LOCATION_FORMAT_MAP:
            pos = gps_form_map_location_url(pos, strSize - (pos - pStr),
                pGPSData);
            break;
        case SMS_SEND_LOCATION_FORMAT_WEB:
            pos = gps_form_web_location_url(pos, strSize - (pos - pStr),
                pGPSData);
            break;
        case SMS_SEND_LOCATION_FORMAT_VAL:
            pos = gps_form_val_location_str(pos, strSize - (pos - pStr),
                pGPSData);
            break;
        }
        pos = calc_snprintf_return_pointer(
                pos, strSize - (pos - pStr),
                snprintf(pos, strSize - (pos - pStr), "\n")
              );
    }
    lineStart = pos;
    if (((config.sms_send_flags >> SMS_SEND_NSAT_POS)
                                 & SMS_SEND_NSAT_MASK)
                                == SMS_SEND_NSAT_ON) {
        pos = calc_snprintf_return_pointer(
                pos, strSize - (pos - pStr),
                snprintf(pos, strSize - (pos - pStr),
                    "%snsat=%lu", pos == lineStart ? "" : ",", pGPSData->nsats)
              );
    }
    if (((config.sms_send_flags >> SMS_SEND_ALT_POS)
                                 & SMS_SEND_ALT_MASK)
                                == SMS_SEND_ALT_ON) {
        pos = calc_snprintf_return_pointer(
                pos, strSize - (pos - pStr),
                snprintf(pos, strSize - (pos - pStr),
                "%salt=%.1f", pos == lineStart ? "" : ",", pGPSData->alt)
              );
    }
    if (((config.sms_send_flags >> SMS_SEND_SPEED_POS)
                                 & SMS_SEND_SPEED_MASK)
                                == SMS_SEND_SPEED_ON) {
        pos = calc_snprintf_return_pointer(
                pos, strSize - (pos - pStr),
                snprintf(pos, strSize - (pos - pStr),
                "%sspeed=%.1f", pos == lineStart ? "" : ",", pGPSData->speed)
              );
    }
    if (((config.sms_send_flags >> SMS_SEND_IGN_POS)
                                 & SMS_SEND_IGN_MASK)
                                == SMS_SEND_IGN_ON) {
        pos = calc_snprintf_return_pointer(
                pos, strSize - (pos - pStr),
                snprintf(pos, strSize - (pos - pStr),
                    "%sign=%s", pos == lineStart ? "" : ",",
                    ignState ? "ON" : "OFF")
              );
    }
}

/**
 * Handles the SMS set APN command
 * @param pPhoneNumber points to the text phone number we send any response to
 * @param pValue points to the APN (access point name) string value
 */
void sms_apn_handler(
    const char* pPhoneNumber,
    const char* pValue
) {
    if (strlen(pValue) > sizeof(config.apn) - 1) {
        sms_send_msg("Error: APN is too long", pPhoneNumber);
    } else {
        strcpy(config.apn, pValue);
        saveConfig = true;
        powerReboot = true;
        sms_send_msg("APN saved", pPhoneNumber);
    }
}

/**
 * Handles the SMS set gprspass command
 * @param pPhoneNumber points to the text phone number we send any response to
 * @param pValue points to the GPRS password string value
 */
void sms_gprspass_handler(
    const char* pPhoneNumber,
    const char* pValue
) {
    if (strlen(pValue) > sizeof(config.pwd) - 1) {
        sms_send_msg("Error: gprs password is too long", pPhoneNumber);
    } else {
        strcpy(config.pwd, pValue);
        saveConfig = true;
        powerReboot = true;
        sms_send_msg("gprs password saved", pPhoneNumber);
    }
}

/**
 * Handles the SMS set gprsuser command
 * @param pPhoneNumber points to the text phone number we send any response to
 * @param pValue points to the GPRS user name string value
 */
void sms_gprsuser_handler(
    const char* pPhoneNumber,
    const char* pValue
) {
    if (strlen(pValue) > sizeof(config.user) - 1) {
        sms_send_msg("Error: gprs username is too long", pPhoneNumber);
    } else {
        strcpy(config.user, pValue);
        saveConfig = true;
        powerReboot = true;
        sms_send_msg("gprs username saved", pPhoneNumber);
    }
}

/**
 * Handles the SMS set smskey command
 * @param pPhoneNumber points to the text phone number we send any response to
 * @param pValue points to the new SMS key string value
 */
void sms_smskey_handler(
    const char* pPhoneNumber,
    const char* pValue
) {
    if (strlen(pValue) > sizeof(config.sms_key) - 1) {
        sms_send_msg("Error: sms password is too long", pPhoneNumber);
    } else {
        strcpy(config.sms_key, pValue);
        saveConfig = true;
        sms_send_msg("sms password saved", pPhoneNumber);
    }
}

/**
 * Handles the SMS set pin command
 * @param pPhoneNumber points to the text phone number we send any response to
 * @param pValue points to the SIM card pin string value
 */
void sms_pin_handler(
    const char* pPhoneNumber,
    const char* pValue
) {
    if (strlen(pValue) > sizeof(config.sim_pin) - 1) {
        sms_send_msg("Error: sim pin is too long", pPhoneNumber);
    } else {
        strcpy(config.sim_pin, pValue);
        saveConfig = true;
        powerReboot = true;
        sms_send_msg("sim pin saved", pPhoneNumber);
    }
}

/**
 * Handles the SMS set sint command
 * @param pPhoneNumber points to the text phone number we send any response to
 * @param pValue points to the new time string value (in secs) for the slow gps
 *        data update interval
 */
void sms_sint_handler(
    const char* pPhoneNumber,
    const char* pValue
) {
    unsigned long updateSecs = strtoul(pValue, NULL, 0);
    if ((updateSecs == 0) || (updateSecs > config.fast_server_interval)) {
        sms_send_msg("Error: bad slow update interval", pPhoneNumber);
    } else {
        config.slow_server_interval = updateSecs;
        saveConfig = true;
        sms_send_msg("Slow update interval saved", pPhoneNumber);
    }
}

/**
 * Handles the SMS set fint command
 * @param pPhoneNumber points to the text phone number we send any response to
 * @param pValue points to the new time string value (in secs) for the fast gps
 *        data update interval
 */
void sms_fint_handler(
    const char* pPhoneNumber,
    const char* pValue
) {
    unsigned long updateSecs = strtoul(pValue, NULL, 0);
    if ((updateSecs == 0) || (updateSecs < config.slow_server_interval)) {
        sms_send_msg("Error: bad fast update interval", pPhoneNumber);
    } else {
        config.fast_server_interval = updateSecs;
        saveConfig = true;
        sms_send_msg("Fast update interval saved", pPhoneNumber);
    }
}

/**
 * Handles the SMS locate command
 * @param pPhoneNumber points to the text phone number we send any response to
 * @param pValue not used (should be NULL)
 */
void sms_locate_handler(
    const char* pPhoneNumber,
    const char* pValue
) {
    if (lastGoodGPSData.fixAge == TinyGPS::GPS_INVALID_AGE) {
        sms_send_msg("Current location not known yet", pPhoneNumber);
    } else {
        char msg[MAX_SMS_MSG_LEN + 1];
        sms_form_sms_update_str(msg, DIM(msg), &lastGoodGPSData, ignState);
        sms_send_msg(msg, pPhoneNumber);
    }
}

/**
 * Handles the SMS set number command
 * @param pPhoneNumber points to the text phone number we send any response to
 * @param pValue points to the phone number to which we send any SMS update
 *        messages
 */
void sms_smsnumber_handler(
    const char* pPhoneNumber,
    const char* pValue
) {
    if (strlen(pValue) > sizeof(config.sms_send_number) - 1) {
        sms_send_msg("Error: sms number is too long", pPhoneNumber);
    } else {
        strcpy(config.sms_send_number, pValue);
        saveConfig = true;
        sms_send_msg("sms number saved", pPhoneNumber);
    }
}

/**
 * Handles the SMS set SMS freq command
 * @param pPhoneNumber points to the text phone number we send any response to
 * @param pValue points to a string time value (in secs) which is the new
 *        SMS send update interval
 */
void sms_smsfreq_handler(
    const char* pPhoneNumber,
    const char* pValue
) {
    long updateSecs = atol(pValue);
    if (updateSecs <= 0) {
        sms_send_msg("Error: bad frequency", pPhoneNumber);
    } else {
        config.sms_send_interval = updateSecs;
        saveConfig = true;
        sms_send_msg("sms freq saved", pPhoneNumber);
    }
}

/**
 * Requests a gsm restart. Use to force a gsm disconnect and reconnect cycle
 * @param pPhoneNumber points to the text phone number we send any response to
 * @param pValue not used (should be NULL)
 */
void sms_gsmrestart_handler(
    const char* pPhoneNumber,
    const char* pValue
) {
    gsmRestart = true;
    sms_send_msg("gsm restart request received", pPhoneNumber);
}

/**
 * Requests a system reboot
 * @param pPhoneNumber points to the text phone number we send any response to
 * @param pValue not used (should be NULL)
 */
void sms_reboot_handler(
    const char* pPhoneNumber,
    const char* pValue
) {
    powerReboot = true;
    sms_send_msg("Reboot request received", pPhoneNumber);
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
 *         character past the last one stored input pDest due to size
 *         restrictions.
 */
const char* sms_extract_field(
    const char* pSource,
    char* pDest,
    size_t sizeDest,
    const char* pTerm
) {
    size_t idx = 0;
    while ((*pSource != '\0') &&
        (strchr(pTerm, *pSource) == NULL) &&
        (idx < sizeDest - 1))
        pDest[idx++] = *pSource++;
    pDest[idx] = '\0';
    return pSource;
}

/**
 * Forms a string containing all of the known configuration field values
 * @param pMsg where to store the string
 * @param msgSize the size of the storage available for pMsg
 * @param fieldValues the current field value set
 * @param pFieldSpecs the set of known fields
 * @param fieldCount the number of field specs pointed to by pFieldSpecs
 */
void sms_form_field_config_message(
    char* pMsg,
    size_t msgSize,
    unsigned long fieldValues,
    const struct SMS_FIELD_SPEC_S* pFieldSpecs,
    size_t fieldCount
) {
    char* pos = pMsg;
    size_t msgLeft = msgSize;
    // For each known field ...
    for (size_t idx = 0; (idx < fieldCount) && (msgLeft > 0); ++idx) {
        // Get the binary field value
        unsigned int fieldValue =
            (fieldValues >> pFieldSpecs[idx].fieldStartPos)
                & pFieldSpecs[idx].fieldMask;
        // Map the value to a string
        const char* pFieldValue = pFieldSpecs[idx].fieldValues[fieldValue];
        const char* pFieldName = pFieldSpecs[idx].pFieldName;
        // Output the field string as name:value[,]
        pos = calc_snprintf_return_pointer(
            pos, msgLeft,
            snprintf(
                pos, msgLeft,
                "%s:%s%s",
                pFieldName,
                pFieldValue == NULL ? "?" : pFieldValue,
                idx == fieldCount - 1 ? "" : ","));
        // Calc space available for the next snprintf call
        msgLeft = msgSize - (pos - pMsg);
    }
}

/**
 * Sets a configuration field value within an unsigned long value
 * @param pFieldName points to the field name
 * @param pFieldValue points to the requested field value as a string
 * @param pFieldSpecs the set of known fields
 * @param fieldCount the number of field specs pointed to by pFieldSpecs
 * @param defaultValue the default value for the (complete) field set
 * @param pCurValues points to the current field set values (that we will
 *        modify)
 * @return true if field name/value processed OK
 *         false if field name is not known or field value is not a
 *         recognised value
 */
bool sms_process_config_field(
    const char* pFieldName,
    const char* pFieldValue,
    const struct SMS_FIELD_SPEC_S* pFieldSpecs,
    size_t fieldCount,
    unsigned long defaultValues,
    unsigned long* pCurValues
) {
    bool fieldProcessedStat = false;
    debug_print(F("sms_process_config_field setting field '"));
    debug_print(pFieldName);
    debug_print(F("' to '"));
    debug_print(pFieldValue);
    debug_println(F("'"));
    // Is this a request to set the default value set?
    if (stricmp(pFieldName, "default") == 0) {
        // To set defaults the value _must_ be sent as "on"
        if (stricmp(pFieldValue, "on") == 0) {
            *pCurValues = defaultValues;
            fieldProcessedStat = true;
        }
    } else {
        // For each known field ...
        for (size_t idx = 0;
            (idx < fieldCount) && !fieldProcessedStat;
            ++idx) {
            if (stricmp(pFieldSpecs[idx].pFieldName, pFieldName) == 0) {
                unsigned int fieldMask = pFieldSpecs[idx].fieldMask;
                unsigned int fieldStartPos = pFieldSpecs[idx].fieldStartPos;
                const char** pValueStrings = pFieldSpecs[idx].fieldValues;
                size_t fieldCount = fieldMask + 1;
                // Known field name :-), so convert field value string to
                // its binary value (which is the index into the matching
                // field values array).
                for (size_t value = 0;
                    value < fieldCount;
                    ++value, ++pValueStrings) {
                    if ((*pValueStrings != NULL) &&
                        (stricmp(*pValueStrings, pFieldValue) == 0)) {
                        // Field value string located
                        // Clear the bit(s) in the flag value
                        *pCurValues &= !(fieldMask << fieldStartPos);
                        value &= fieldMask;
                        *pCurValues |= (value << fieldStartPos);
                        fieldProcessedStat = true;
                    }
                }
            }
        }
    }
    return fieldProcessedStat;
}

/**
 * Configures what information is included in a SMS update message and send
 * a return message showing the current set of configuration values
 * @param pPhoneNumber points to the text phone number we send any response to
 * @param pValue points to the configuration information, defined as follows:
 *        SMS message configuration changes the state of the
 *        config.sms_send_flags. Each flag (or configuration field) can be
 *        set individually by sending a ',' separated list of field names and
 *        values. The name and value are separated by ':'.
 *        If pValue is NULL, we respond with the current configuration values.
 *        Supported field names:values (all case insensitive) are:
 *          loc:<on,off>
 *          locfmt:<web,map,val>
 *          nsat:<on,off>
 *          alt:<on,off>
 *          speed:<on,off>
 *          ign:<on,off>
 *        There is also the special field name 'default' which restores the
 *        default configuration values
 *          default:on
 *        Examples of configuration strings:
 *          loc:on,locfmt:val,speed:off
 *          default:on
 */
void sms_smssend_handler(
    const char* pPhoneNumber,
    const char* pValue
) {
    const char* pos = pValue;
    while ((pos != NULL) && (*pos != '\0')) {
        char fieldName[20];
        char fieldValue[20];
        pos = sms_extract_field(pos, fieldName, DIM(fieldName) - 1, ":");
        if (*pos != ':') {
            sms_send_msg("SMS field name too long or missing ':'",
                pPhoneNumber);
            pos = NULL; // Abandon processing the message
        } else {
            pos += 1; // Skip over the ':'
            pos = sms_extract_field(pos, fieldValue, DIM(fieldValue) - 1, ",");
            if ((*pos == ',') || (*pos == '\0')) {
                if (*pos == ',') {
                    pos += 1;
                }
                if (!sms_process_config_field(
                    fieldName, fieldValue,
                    sms_msg_config_fields, DIM(sms_msg_config_fields),
                    SMS_SEND_DEFAULT, &config.sms_send_flags
                    )) {
                    sms_send_msg("Bad SMS field name or field value",
                        pPhoneNumber);
                    pos = NULL; // Abandon processing the message
                }
            } else if (*pos != '\0') {
                sms_send_msg("SMS field value too long", pPhoneNumber);
                pos = NULL; // Abandon processing the message
            }
        }
    }
    char msg[MAX_SMS_MSG_LEN + 1];
    sms_form_field_config_message(
        msg, DIM(msg), config.sms_send_flags,
        sms_msg_config_fields, DIM(sms_msg_config_fields));
    sms_send_msg(msg, pPhoneNumber);
}

/**
 * Configures what information is included in a server update message and send
 * a return message showing the current set of server update configuration
 * values
 * @param pPhoneNumber points to the text phone number we send any response to
 * @param pValue points to the configuration information, defined as follows:
 *        Server message configuration changes the state of the
 *        config.server_send_flags. Each flag (or configuration field) can be
 *        set individually by sending a ',' separated list of field names and
 *        values. The name and value are separated by ':'.
 *        If pValue is NULL, we respond with the current configuration values.
 *        Supported field names:values (all case insensitive) are:
 *          date:<on,off>
 *          time:<on,off>
 *          lat:<on,off>
 *          lon:<on,off>
 *          speed:<on,off>
 *          alt:<on,off>
 *          head:<on,off>
 *          hdop:<on,off>
 *          nsat:<on,off>
 *          batn:<on,off>
 *          ign:<on,off>
 *          run:<on,off>
 *        There is also the special field name 'default' which restores the
 *        default configuration values
 *          default:on
 *        Examples of configuration strings:
 *          lat:on,lon:on,speed:off
 *          default:on
 */
void sms_srvsend_handler(
    const char* pPhoneNumber,
    const char* pValue
) {
    const char* pos = pValue;
    while ((pos != NULL) && (*pos != '\0')) {
        char fieldName[20];
        char fieldValue[20];
        pos = sms_extract_field(pos, fieldName, DIM(fieldName) - 1, ":");
        if (*pos != ':') {
            sms_send_msg("serverfield name too long or missing ':'",
                pPhoneNumber);
            pos = NULL; // Abandon processing the message
        } else {
            pos += 1;
            pos = sms_extract_field(pos, fieldName, DIM(fieldName) - 1, ",");
            if ((*pos == ',') || (*pos == '\0')) {
                pos += 1;
                if (!sms_process_config_field(
                    fieldName, fieldValue,
                    sms_server_config_fields, DIM(sms_server_config_fields),
                    SERVER_SEND_DEFAULT, &config.server_send_flags
                    )) {
                    sms_send_msg("Bad server field name or field value",
                        pPhoneNumber);
                    pos = NULL; // Abandon processing the message
                }
            } else if (*pos != '\0') {
                sms_send_msg("server field value too long", pPhoneNumber);
                pos = NULL; // Abandon processing the message
            }
        }
    }
    char msg[MAX_SMS_MSG_LEN + 1];
    sms_form_field_config_message(
        msg, DIM(msg), config.server_send_flags,
        sms_server_config_fields, DIM(sms_server_config_fields));
    sms_send_msg(msg, pPhoneNumber);
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
    // Issue command to modem to read all unread messages
    snprintf(modem_command, sizeof(modem_command),
        "AT+CMGL=\"REC UNREAD\"");
    gsm_send_command();
    gsmWaitForReply(true, SECS(1));
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
    if (msgCount > 0) {
        debug_print(F("Processed "));
        debug_print(msgCount);
        debug_println(F(" SMS messages"));
        // remove all READ and SENT sms
        snprintf(modem_command, sizeof(modem_command),
            "AT+QMGDA=\"DEL READ\"");
        gsm_send_command();
        gsmWaitForReply(true, SECS(2));
        snprintf(modem_command, sizeof(modem_command),
            "AT+QMGDA=\"DEL SENT\"");
        gsm_send_command();
        gsmWaitForReply(true, SECS(2));
    }
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
const char* sms_process(
    const char* msgStart
) {
    char phoneNumber[MAX_PHONE_NUMBER_LEN + 1];
    char message[SMS_MAX_CMD_LEN + 1];
    const char* pos = strchr(msgStart, ','); // after <index>,
    if (pos != NULL) {
        pos = strchr(pos + 1, ','); // after <stat>,
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
void sms_cmd(
    char *pSMSMessage,
    char *pPhone
) {
    debug_print(F("Received SMS message: "));
    debug_println(pSMSMessage);
    const char* pCmd = pSMSMessage;
    while (isspace(*pCmd))
        ++pCmd;
    if (*pCmd != '#') {
        debug_print(F("sms_cmd(): SMS message did not start with #: "));
        debug_println(pSMSMessage);
    } else {
        ++pCmd; // Skip over '#'
        char smsKey[MAX_SMS_KEY_LEN + 1];
        // Extract SMS password/key
        pCmd = sms_extract_field(
            pCmd, smsKey, DIM(smsKey), ",");
        if (*pCmd != ',') {
            debug_print(
                F("sms_cmd(): SMS message missing SMS key delimiter: "));
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

/**
 * Processes a single SMS received command
 * @param pRequest points to the command string which should be in
 *           <command>[=<value>]
 *        format
 * @param pPhoneNumber points to the text phone number we send any response to
 */
void sms_cmd_run(
    const char *pRequest,
    const char *pPhoneNumber
) {
    char command[SMS_MAX_CMD_LEN + 1];
    char value[100];
    while (isspace(*pRequest))
        ++pRequest;
    // Extract the command
    pRequest = sms_extract_field(
        pRequest, command, DIM(command), "=");
    const char* pValue = NULL;
    if (*pRequest == '=') {
        pRequest += 1; // Skip over the '='
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
        debug_print(F("sms_cmd_run(): Received unknown command: "));
        debug_println(command);
    }
}

/**
 * Sends a plain English SMS message to a phone number
 * @param pMsg points to the plain English text to send
 * @param pPhoneNumber points to the phone number
 */
void sms_send_msg(
    const char *pMsg,
    const char *pPhoneNumber
) {
    //send SMS message to number
    debug_print(F("Sending SMS to:"));
    debug_print(pPhoneNumber);
    debug_print(F(" : "));
    debug_println(pMsg);

    snprintf(modem_command, sizeof(modem_command),
        "AT+CMGS=\"%s\"", pPhoneNumber);
    gsm_send_command();
    gsmWaitForReply(false, GSM_MODEM_COMMAND_TIMEOUT);
    char *tmp = strstr(modem_reply, ">");
    if (tmp != NULL) {
        gsm_port.print(pMsg);
        //sending ctrl+z
        gsm_port.print("\x1A");
        gsmWaitForReply(true, GSM_MODEM_COMMAND_TIMEOUT);
    }
}

