// TODO sensor remap file
#include <WiFi.h>
#include <mysyslog.h>
#include <mywebserver.h>
#include <tempreporter.h>
#include <webupdater.h>
#include <mywifi.h>

#include <my_secrets.h>

// Data wire is plugged TO GPIO 4
#define ONE_WIRE_BUS 4

void setup(){
    // start serial port
    Serial.begin(115200);

    WIFI_init();
    WS_init("/temperatures");

    // wait for time to be known
    while (time(NULL) < 1000000000) {
        delay(1000);
    }

    // start tempreporter service
    TR_init(server, ONE_WIRE_BUS);

    // firmware updates
    UD_init(server);
}

void loop(){ 
    delay(1000);
}
