void storage_config_fill() {
    //fill settings storage space with zeros on very first run
    debug_println(F("storage_config_fill() started"));
    for (int i = STORAGE_CONFIG_PAGE; i < STORAGE_DATA_START; i++) {
        debug_println(F("storage_config_fill(): filling address with 0"));
        debug_println(i);
        dueFlashStorage.write(i, 0);
    }
    //set first run flag
    dueFlashStorage.write(STORAGE_FIRST_RUN_PAGE, 1);
    debug_println(F("storage_config_fill() finished"));
}

void settings_load() {
    //load all settings from EEPROM
    int tmp;
    debug_println(F("settings_load() started"));
    config.slow_server_interval = SLOW_SERVER_INTERVAL;
    config.fast_server_interval = FAST_SERVER_INTERVAL;
    strlcpy(config.key, KEY, sizeof(config.key));
    strlcpy(config.sms_key, SMS_KEY, sizeof(config.sms_key));
    strlcpy(config.apn, DEFAULT_APN, sizeof(config.apn));
    strlcpy(config.user, DEFAULT_USER, sizeof(config.user));
    strlcpy(config.pwd, DEFAULT_PASS, sizeof(config.pwd));
    config.sms_send_interval = SMS_SEND_INTERVAL;
    strlcpy(config.sms_send_number, SMS_SEND_NUMBER, sizeof(config.sms_send_number));
    config.sms_send_flags = SMS_SEND_DEFAULT;
    config.server_send_flags = SERVER_SEND_DEFAULT;
    debug_println(F("settings_load() finished"));
}

void settings_save() {
    debug_println(F("settings_save() started"));
    //save all settings to flash
    byte b2[sizeof(SETTINGS_T)]; // create byte array to store the struct
    memcpy(b2, &config, sizeof(SETTINGS_T)); // copy the struct to the byte array
    dueFlashStorage.write(STORAGE_CONFIG_PAGE, b2, sizeof(SETTINGS_T)); // write byte array to flash
    debug_println(F("settings_save() finished"));
}

