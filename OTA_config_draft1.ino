/////////////// OTA_config_draft1 ////////////////////
//
// While derived from OTA_BME280 line, the goal with this thread is to cut
// everything out except what is needed to connect to WiFi and make a request
// to espsite for configuration info, followed by a request to download the
// "latest" version of the code that is appropriate for this device.
//
// 2019-10-27 copy from OTA_BME280_draft7_ws7 (or was it draft8_ws8?)
// 2019-11-06 add a simple web responder to trigger an immediate check for
// update
//   - the idea is to aid development of the perl scripts
// 2019-11-07 This is now working pretty well with an updated arduino.php
// script.

//////////////////////////////////////////////////////
#define DEBUG_ESP_WIFI 1
#define DEBUG_ESP_HTTP_UPDATE 1
#include <ArduinoOTA.h>
#include <ESP8266WebServer.h>
#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <ESP8266httpUpdate.h>

// Identification
const char vfname[] = __FILE__;
const char vtimestamp[] = __DATE__ " " __TIME__;
String versionstring = "20191107.1515.1";
String myHostname = "unknown";
String myMacAddress = "00:00:00:00:00:00";
IPAddress myIpAddress;

// This blink code should be replaced...
// variables for blinking an LED with Millis
const int led = LED_BUILTIN; // ESP8266 Pin to which onboard LED is connected
#define LED_OFF HIGH
#define LED_ON LOW
int ledState = LED_ON; // initial ledState
int ledSeqPos = 0;     // blink pattern position counter
// blink pattern: 1 - 7 - 1 - 7 - 1 ...
const int blinkSeq[] = {1000, 10, // 1 long
                        900,  1,  // 4 short
                        100,  1,  200, 1,
                        100,  1,  0}; // sentinal for end of sequence
unsigned long nextLedTransition = 0;

// OTA variables
const unsigned long OtaUpdateInterval = 15 * 60 * 1000; // 15 minutes
unsigned long nextOtaUpdate = 0;

ESP8266WiFiMulti wifiMulti;
ESP8266WebServer server(80);

void setup()
{
    pinMode(led, OUTPUT);
    Serial.begin(115200);
    Serial.setDebugOutput(true);
    Serial.println("Booting");
    WiFi.mode(WIFI_STA);
    WiFi.enableInsecureWEP(true); // needed since we still run WEP at home

#include "wifi-secrets.h" // secrets kept in separate file

    wifiMulti.run();

    // Identification
    Serial.println();
    Serial.print("file: ");
    Serial.println(vfname);
    Serial.print("timestamp (local time): ");
    Serial.println(vtimestamp);
    Serial.println(versionstring);
    Serial.println();

    //////// OTA Handlers ///////////////
    // The following blocks set up handlers for OTA requests initiated via the
    // arduino IDE or similar mechanism

    ArduinoOTA.onStart([]() {
        String type;
        if (ArduinoOTA.getCommand() == U_FLASH)
            {
                type = "sketch";
            }
        else
            { // U_SPIFFS
                type = "filesystem";
            }

        // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS
        // using SPIFFS.end()
        Serial.println("Start updating " + type);
    });

    ArduinoOTA.onEnd([]() { Serial.println("\nEnd"); });

    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
        Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
    });

    ArduinoOTA.onError([](ota_error_t error) {
        Serial.printf("Error[%u]: ", error);
        if (error == OTA_AUTH_ERROR)
            {
                Serial.println("Auth Failed");
            }
        else if (error == OTA_BEGIN_ERROR)
            {
                Serial.println("Begin Failed");
            }
        else if (error == OTA_CONNECT_ERROR)
            {
                Serial.println("Connect Failed");
            }
        else if (error == OTA_RECEIVE_ERROR)
            {
                Serial.println("Receive Failed");
            }
        else if (error == OTA_END_ERROR)
            {
                Serial.println("End Failed");
            }
    });
    ArduinoOTA.begin();

    Serial.print("MAC address: ");
    myMacAddress = WiFi.macAddress();
    Serial.println(myMacAddress);
    Serial.print("default hostname: ");
    Serial.println(WiFi.hostname());
    if (myIpAddress != WiFi.localIP())
        {
            Serial.print("IP address: ");
            myIpAddress = WiFi.localIP();
            Serial.println(myIpAddress);
        }
    Serial.println("");
    // start the web server
    server.on("/", handle_OnConnect);
    server.onNotFound(handle_NotFound);

    server.begin();
    Serial.println("HTTP server started");
}

void loop()
{
    Serial.setDebugOutput(true);
    wifiMulti.run();
    if (myIpAddress != WiFi.localIP())
        {
            Serial.print("IP address: ");
            myIpAddress = WiFi.localIP();
            Serial.println(myIpAddress);
        }

    ArduinoOTA.handle();
    server.handleClient();

    unsigned long currentMillis = millis();

    // loop to blink without invoking delay()
    if (currentMillis > nextLedTransition)
        {
            ledState ^= 1; // toggle ledState;
            // set the LED with the ledState of the variable:
            digitalWrite(led, ledState);
            // Serial.print("blink ");
            // Serial.println( ledState);

            nextLedTransition = currentMillis + blinkSeq[ledSeqPos++];
            if (blinkSeq[ledSeqPos] == 0)
                {
                    ledSeqPos = 0;
                }
        }

    // This section needs to be reworked...
    // At intervals, check website to see if a new version is availalble
    if ((currentMillis >= nextOtaUpdate) && (WiFi.status() == WL_CONNECTED))
        {
            Serial.println("");
            Serial.print("Look for OTA update from curent version ");
            Serial.println(versionstring);

            WiFiClientSecure client;
            client.setInsecure(); // See BearSSL documentation

            HTTPUpdateResult ret =
                ESPhttpUpdate.update(client, "espsite.jmcg.net", 443,
                                     "/update/arduino.php", versionstring);
            switch (ret)
                {
                case HTTP_UPDATE_FAILED:
                    Serial.println("[update] Update (php) failed.");
                    break;
                case HTTP_UPDATE_NO_UPDATES:
                    Serial.println(
                        "[update] Not needed, already latest version.");
                    break;
                case HTTP_UPDATE_OK:
                    Serial.println(
                        "[update] Update ok."); // should not be called
                    // as we reboot the ESP
                    break;
                }
            // try using the Perl request; only one of these should succeed
            ret = ESPhttpUpdate.update(client, "espsite.jmcg.net", 443,
                                       "/update/updrequest.pl", versionstring);
            switch (ret)
                {
                case HTTP_UPDATE_FAILED:
                    Serial.println("[update] Update (pl) failed.");
                    break;
                case HTTP_UPDATE_NO_UPDATES:
                    Serial.println(
                        "[update] Not needed, already latest version.");
                    break;
                case HTTP_UPDATE_OK:
                    Serial.println(
                        "[update] Update ok."); // should not be called
                    // as we reboot the ESP
                    break;
                }
            nextOtaUpdate = currentMillis + OtaUpdateInterval;
            Serial.println("");
        }
}

// webserver handlers
void handle_OnConnect()
{
    Serial.println("reset OTA update timer");
    nextOtaUpdate = 0;
    server.send(200, "text/plain", "OK");
}

void handle_NotFound()
{
    Serial.println("webserver: target not found");
    server.send(404, "text/plain", "Not found");
}
