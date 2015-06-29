void reboot() {
    debug_println(F("reboot() started"));

    //reboot only works with normal power, without programming cable connected
    //turn off modem, GPS            
    gsm_on_off();
    delay(1000);

    gsm_get_reply();

    //turn off GPS
    digitalWrite(PIN_STANDBY_GPS, HIGH);

    //wait 4 sec
    delay(4000);

    //power cycle everything
    debug_println(F("Power cycling hardware."));

    digitalWrite(PIN_C_REBOOT, HIGH);
    delay(10000);  //this should never be executed

    setup();   //in case reboot failed, run setup() and continue;

}
