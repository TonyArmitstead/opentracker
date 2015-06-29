void storage_save_current() {
    debug_println(F("storage_save_current() started"));
    //saving data_current to flash memory    
    for (int i = 0; i < strlen(data_current); i++) {
        //check for flash limit
        if (logindex >= STORAGE_DATA_END) {
            debug_println(
                F(
                    "storage_save_current(): flash memory is full, starting to overwrite"));
            logindex = STORAGE_DATA_START;
        }
        dueFlashStorage.write(logindex, data_current[i]);
        logindex++;
    }
    //adding index marker, it will be overwritten with next flash write
    dueFlashStorage.write(logindex, STORAGE_INDEX_CHAR);  //35 = #
    debug_println(F("storage_save_current() ended"));
}

void storage_get_index() {
    byte c;

    debug_println(F("store_get_index() started"));
    //scan flash for current log position (new log writes will continue from there)
    for (long i = STORAGE_DATA_START; i <= STORAGE_DATA_END; i++) {
        c = dueFlashStorage.read(i);
        if (c == STORAGE_INDEX_CHAR) {
            //found log index 
            debug_println(F("store_get_index(): Found log position:"));
            debug_println(i);
            logindex = i;
            break;  //we only need first found index
        }
    }
    debug_println(F("store_get_index() ended"));
}

void storage_send_logs() {
    long index_tmp;
    long sent_position = STORAGE_DATA_START; //by default sent from the beggining
    byte sent = 0;
    byte c;
    int delivered = 0;
    int ret_tmp = 0;

    debug_println(F("storage_send_logs() started"));
    //check if some logs were saved 
    if (logindex > STORAGE_DATA_START) {
        //send all storred logs to server
        //looking for the start of the log
        //sent position must be the last before current logindex
        for (long i = STORAGE_DATA_START; i <= logindex; i++) {
            c = dueFlashStorage.read(i);
            if (c == STORAGE_INDEX_SENT_CHAR) {
                //found log index 
                debug_println(F("storage_send_logs(): Found log sent position:"));
                debug_println(i);
                sent_position = i + 1;     //do not send separator itself       
            }
        }
        debug_println(F("storage_send_logs(): Sending data from/to:"));
        debug_println(sent_position);
        debug_println(logindex - 1);
        //send 2 ATs              
        gsm_send_at();
        //disconnect GSM
        ret_tmp = gsm_disconnect(1);
        ret_tmp = gsm_connect();
        if (ret_tmp == 1) {
            index_tmp = logindex - sent_position - 1; //how many bytes of actuall data will be sent            
            unsigned long http_len = strlen(config.imei) + strlen(config.key)
                + index_tmp;
            http_len = http_len + 13;         //imei= &key= &d=                 
            http_len = http_len + strlen(HTTP_HEADER1) + strlen(HTTP_HEADER2); //adding header length
            char tmp_http_len[20];
            ltoa(http_len, tmp_http_len, 10);
            unsigned long tmp_len = strlen(HTTP_HEADER1) + strlen(tmp_http_len)
                + strlen(HTTP_HEADER2);
            debug_println(F("DEBUG"));
            debug_println(index_tmp);
            debug_println(strlen(HTTP_HEADER1));
            debug_println(strlen(tmp_http_len));
            debug_println(strlen(HTTP_HEADER2));
            debug_println(F("storage_send_logs(): Sending bytes:"));
            debug_println(index_tmp);
            debug_println(http_len);
            debug_println(tmp_len);
            //sending header packet to remote host                 
            gsm_port.print("AT+QISEND=");
            gsm_port.print(tmp_len);
            gsm_port.print("\r");
            delay(500);
            gsm_get_reply();
            //sending header                     
            gsm_port.print(HTTP_HEADER1);
            gsm_port.print(http_len);
            gsm_port.print(HTTP_HEADER2);
            gsm_get_reply();
            //validate header delivery
            delivered = gsm_validate_tcp();
            //do not send other data before current is delivered
            if (delivered == 1) {
                //sending imei and key first
                gsm_port.print("AT+QISEND=");
                gsm_port.print(13 + strlen(config.imei) + strlen(config.key));
                gsm_port.print("\r");
                delay(500);
                gsm_get_reply();
                gsm_port.print("imei=");
                gsm_port.print(config.imei);
                gsm_port.print("&key=");
                gsm_port.print(config.key);
                gsm_port.print("&d=");
                delay(500);
                gsm_get_reply();
                //sending the actual log by packets (PACKET_SIZE)
                tmp_len = logindex - sent_position - 1;
                unsigned long chunk_len;
                unsigned long chunk_pos = 0;
                unsigned long chunk_check = 0;
                if (tmp_len > PACKET_SIZE) {
                    chunk_len = PACKET_SIZE;
                } else {
                    chunk_len = tmp_len;
                }
                int k = 0;
                for (int i = 0; i < tmp_len; i++) {
                    if ((i == 0) || (chunk_pos >= PACKET_SIZE)) {
                        debug_println(chunk_pos);
                        if (chunk_pos >= PACKET_SIZE) {
                            gsm_get_reply();
                            //validate previous transmission  
                            delivered = gsm_validate_tcp();
                            if (delivered == 1) {
                                //next chunk, get chunk length, check if not the last one                            
                                chunk_check = tmp_len - i;
                                if (chunk_check > PACKET_SIZE) {
                                    chunk_len = PACKET_SIZE;
                                } else {
                                    //last packet
                                    chunk_len = chunk_check;
                                }
                                chunk_pos = 0;
                            } else {
                                //data can not be delivered, abort the transmission and try again
                                debug_println(
                                    F("sd_send_logs() Can not deliver HTTP data"));
                                break;
                            }
                        }
                        //sending chunk
                        gsm_port.print("AT+QISEND=");
                        gsm_port.print(chunk_len);
                        gsm_port.print("\r");
                        delay(500);
                    }  //if((i == 0) || (chunk_pos >= PACKET_SIZE))
                    //sending data                             
                    c = dueFlashStorage.read(sent_position + i);
                    chunk_pos++;
                    k++;
                }
                //parse server reply, in case #eof received mark logs as sent
                delay(2000);
                sent = parse_receive_reply();

            } else {
                debug_println(F("sd_send_logs() Can not deliver HTTP header"));
            }
            gsm_get_reply();
        }
        //disconnecting 
        ret_tmp = gsm_disconnect(1);
        if (sent == 1) {
            //logs sent, marking sent position (must be before current logindex)
            index_tmp = logindex - 1;
            dueFlashStorage.write(index_tmp, STORAGE_INDEX_SENT_CHAR);
            debug_println(F("storage_send_logs(): Logs were sent successfully"));
        } else {
            debug_println(F("storage_send_logs(): Error sending logs"));
        }
    } else {
        debug_println(F("storage_send_logs(): No logs present"));
    }
    debug_println(F("storage_send_logs() ended"));
}

//debug function 
void storage_dump() {
    byte c;

    debug_println(F("storage_dump() started"));
    //lilsting contents of flash
    for (long i = STORAGE_DATA_START; i <= STORAGE_DATA_END; i++) {
        c = dueFlashStorage.read(i);
        debug_port.print(c);
        if (c == STORAGE_INDEX_CHAR) {
            break;
        }
    }
    debug_println(F(""));
    debug_println(F("storage_dump() ended"));
}

