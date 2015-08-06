//parse remote commands from server
int parse_receive_reply() {
    //receive reply from modem and parse it 
    int ret = 0;
    byte index = 0;
    byte header = 0;
    char inChar = -1; // Where to store the character read
    char *tmp;
    char *tmpcmd;
    char cmd[100];  //remote commands stored here

    debug_println(F("parse_receive_reply() started"));
    //clean modem buffer
    gsm_get_reply();
    for (int i = 0; i < 30; i++) {
        snprintf(modem_command, sizeof(modem_command),
            "AT+QIRD=0,1,0,100");
        gsm_send_command();
        gsm_wait_for_reply(1);
        //check if no more data
        tmp = strstr(modem_reply, "ERROR");
        if (tmp != NULL) {
            debug_println(F("No more data available."));
            break;
        }
        if (header != 1) {
            tmp = strstr(modem_reply, "close\r\n\r\n");
            if (tmp != NULL) {
                debug_println(F("Found header packet"));
                header = 1;
                //all data from this packet and all next packets can be commands
                tmp = strstr(modem_reply, "\r\n\r\n");
                tmp += strlen("\r\n\r\n");
                tmpcmd = strtok(tmp, "OK");
                for (index = 0; index < strlen(tmpcmd) - 2; index++) {
                    cmd[index] = tmpcmd[index];
                }
                cmd[index] = '\0';
            }
        } else {
            //not header packet, get data from +QIRD ... \r\n till OK
            tmp = strstr(modem_reply, "\r\n");
            tmp += strlen("\r\n");
            tmpcmd = strtok(tmp, "OK");
            tmp = strstr(tmp, "\r\n");
            tmp += strlen("\r\n");
            tmpcmd = strtok(tmp, "\r\nOK");
            for (int i = 0; i < strlen(tmpcmd); i++) {
                cmd[index] = tmpcmd[i];
                index++;
            }
            cmd[index] = '\0';
        }
    }
    if (SEND_RAW) {
        debug_println(
            F(
                "RAW data mode enabled, not checking whether the packet was received or not."));
        ret = 1;
    } else {
        tmp = strstr((cmd), "eof");
        if (tmp != NULL) {
            //all data was received by server
            debug_println(F("Data was fully received by the server."));
            ret = 1;
        } else {
            debug_println(F("Data was not received by the server."));
        }
    }
    parse_cmd(cmd);
    debug_println(F("parse_receive_reply() completed"));
    return ret;
}

void parse_cmd(char *cmd) {
    //parse commands info received from the server
    debug_println(F("parse_cmd() started"));
    debug_print(F("Received commands: "));
    debug_println(cmd);
    debug_println(F("parse_cmd() completed"));
}

