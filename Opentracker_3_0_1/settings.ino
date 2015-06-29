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
    config.interval = INTERVAL;
    config.interval_send = INTERVAL_SEND;
    config.powersave = POWERSAVE;
    strlcpy(config.key, KEY, sizeof(config.key));
    strlcpy(config.sms_key, SMS_KEY, sizeof(config.sms_key));
    strlcpy(config.apn, DEFAULT_APN, sizeof(config.apn));
    strlcpy(config.user, DEFAULT_USER, sizeof(config.user));
    strlcpy(config.pwd, DEFAULT_PASS, sizeof(config.pwd));

    /* MEMORY saving does not work - using static values from config
     // dueFlashStorage needs comparatility fixing - TODO
     
     byte first_run = dueFlashStorage.read(STORAGE_FIRST_RUN_PAGE);  
     debug_println(F("settings_load(): First run flag:"));
     debug_println(first_run);     
     
     if(first_run != '1')
     {
     //first run was not set, filling config space with 0
     // storage_config_fill();  // DOES NOT WORK - dueFlashStorage needs comparatility fixing
     }
     else
     {
     debug_println(F("settings_load(): no a first run."));
     }
     
     
     byte* b = dueFlashStorage.readAddress(STORAGE_CONFIG_PAGE); // byte array which is read from flash at adress        
     memcpy(&config, b, sizeof(settings)); // copy byte array to temporary struct
     
     
     //setting defaults in case nothing loaded      
     debug_println(F("settings_load(): config.interval:"));
     debug_println(config.interval);    
     
     if((config.interval == -1) || (config.interval == NULL))
     {
     debug_println(F("settings_load(): interval not found, setting default"));
     config.interval = INTERVAL;
     
     debug_println(F("settings_load(): set config.interval:"));
     debug_println(config.interval);    
     }

     
     //interval send
     debug_println(F("settings_load(): config.interval_send:"));
     debug_println(config.interval_send);      

     if((config.interval_send == -1) || (config.interval_send == NULL))
     {
     debug_println(F("settings_load(): interval_send not found, setting default"));
     config.interval_send = INTERVAL_SEND;
     
     debug_println(F("settings_load(): set config.interval_send:"));
     debug_println(config.interval_send);              
     }
     
     //powersave
     debug_println(F("settings_load(): config.powersave:"));
     debug_println(config.powersave);      

     if((config.powersave != 1) && (config.powersave != 0))
     {
     debug_println(F("settings_load(): powersave not found, setting default"));
     config.powersave = POWERSAVE;
     
     debug_println(F("settings_load(): set config.powersave:"));
     debug_println(config.powersave);              
     }
     
     tmp = config.key[0];
     if(tmp == 255)
     {
     debug_println(F("settings_load(): key not found, setting default"));                    
     strlcpy(config.key, KEY, sizeof(config.key));
     }
     
     tmp = config.sms_key[0]; 
     if(tmp == 255)
     {
     debug_println("settings_load(): SMS key not found, setting default");                 
     strlcpy(config.sms_key, SMS_KEY, sizeof(config.sms_key));
     }         
     
     tmp = config.apn[0];       
     if(tmp == 255)
     {
     debug_println("settings_load(): APN not set, setting default");                 
     strlcpy(config.apn, DEFAULT_APN, sizeof(config.apn));
     }  
     
     tmp = config.user[0];  
     if(tmp == 255)
     {
     debug_println("settings_load(): APN user not set, setting default");                 
     strlcpy(config.user, DEFAULT_USER, sizeof(config.user));
     }  
     
     tmp = config.pwd[0];  
     if(tmp == 255)
     {
     debug_println("settings_load(): APN password not set, setting default");                 
     strlcpy(config.pwd, DEFAULT_PASS, sizeof(config.pwd));
     } 
     
     */
    debug_println(F("settings_load() finished"));
}

void settings_save() {
    debug_println(F("settings_save() started"));
    //save all settings to flash
    byte b2[sizeof(settings)]; // create byte array to store the struct
    memcpy(b2, &config, sizeof(settings)); // copy the struct to the byte array
    dueFlashStorage.write(STORAGE_CONFIG_PAGE, b2, sizeof(settings)); // write byte array to flash
    debug_println(F("settings_save() finished"));
}

