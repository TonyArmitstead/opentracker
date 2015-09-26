
void settings_load() {
    debug_println(F("settings_load() started"));
    //load all settings from EEPROM
    if (!storageLoadSettings(&config)) {
        debug_println(F("settings_load() using default configuration values"));
        config.slow_server_interval = SLOW_SERVER_INTERVAL;
        config.fast_server_interval = FAST_SERVER_INTERVAL;
        strlcpy(config.key, SERVER_KEY, sizeof(config.key));
        strlcpy(config.sms_key, SMS_KEY, sizeof(config.sms_key));
        strlcpy(config.apn, DEFAULT_APN, sizeof(config.apn));
        strlcpy(config.user, DEFAULT_USER, sizeof(config.user));
        strlcpy(config.pwd, DEFAULT_PASS, sizeof(config.pwd));
        config.sms_send_interval = SMS_SEND_INTERVAL;
        strlcpy(config.sms_send_number, SMS_SEND_NUMBER, sizeof(config.sms_send_number));
        config.sms_send_flags = SMS_SEND_DEFAULT;
        config.server_send_flags = SERVER_SEND_DEFAULT;
        config.reboot_interval = REBOOT_INTERVAL;
        storageSaveSettings(&config);
    }
    debug_println(F("settings_load() finished"));
}

void settings_save() {
    debug_println(F("settings_save() started"));
    storageSaveSettings(&config);
    debug_println(F("settings_save() finished"));
}

