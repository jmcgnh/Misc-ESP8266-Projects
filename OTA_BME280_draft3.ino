/////////////// OTA_BME280_draft3 ////////////////////
//
//  This source file combines code from:
//      https://lastminuteengineers.com/bme280-esp8266-weather-station/
// and the example Basic_OTA from ESP8266core, perhaps from
//      https://github.com/esp8266-examples/ota-basic
//
// 2019-06-06 tested, draft 1 working
// 2019-06-25 tested, draft 2 working after addition of client.setInsecure();
// 2019-06-25 draft 3 will add IFTTT Webhooks
//
//////////////////////////////////////////////////////
#include <Adafruit_BME280.h>
#include <Adafruit_Sensor.h>
#include <ArduinoOTA.h>
#include <ESP8266WebServer.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <Wire.h>

// Identification
const char vfname[] = __FILE__;
const char vtimestamp[] = __DATE__ " " __TIME__;
const char versionstring[] = "20190626.1510.1";

#define EAGLEROCK 1
#define PHANT01 1
#define IFTTT 1

#include "secrets.h" // keep secret information elsewhere

#ifndef STASSID
#define STASSID "your-ssid"
#define STAPSK "your-password"
#endif

#ifdef PHANT01
/////////////// Phant ////////////////////////
const char phantHost[] = MYPHANTHOST;
const char phantWebPage[] = MYPHANTWEBPAGE;
const char phantPubKey[] = MYPHANTPUBKEY;
const char phantPrivKey[] = MYPHANTPRIVKEY;
const char phantCertFingerprint[] = PHANTSHA1FINGERPRINT;
unsigned long phantNextReport = 0;
unsigned long phantInterval = 15 * 60 * 1000; // 15 minutes
#endif                                        // PHANT01

#ifdef IFTTT
/////////////// IFTTT ////////////////////////
const char iftttHost[] = "maker.ifttt.com";
const char iftttWebPage[] = "/trigger/";
const char iftttEventname[] = MYIFTTTEVENTNAME;
const char iftttMakerKey[] = MYIFTTTMAKERKEY;
const char iftttCertFingerprint[] = IFTTTSHA1FINGERPRINT;
unsigned long iftttNextReport = 0;
unsigned long iftttInterval = 21 * 60 * 1000; // 21 minutes
#endif                                        // IFTTT

const char *ssid = STASSID;
const char *password = STAPSK;

// variabls for blinking an LED with Millis
const int led = BUILTIN_LED; // ESP8266 Pin to which onboard LED is connected
unsigned long previousMillis = 0; // will store last time LED was updated
const long interval = 20;         // interval at which to blink (milliseconds)
unsigned long ledcount;           // led counter for non-binary blink states
unsigned long ledmod;             // remainder of ledcount
int ledState = LOW;               // ledState used to set the LED

ESP8266WebServer server(80);

#define BME_ADDR 0x76
#define SEALEVELPRESSURE_HPA (1013.25)

Adafruit_BME280 bme;

float temperatureC, temperatureF, humidity, pressure, altitude;

void setup()
{
    pinMode(led, OUTPUT);
    Serial.begin(115200);
    Serial.println("Booting");
    WiFi.persistent(
        false); // This is an "always on" setup, so we want to avoid any
    WiFi.mode(WIFI_OFF); // previous wifi attachments.
    WiFi.mode(WIFI_STA);
    WiFi.enableInsecureWEP(true);
    WiFi.begin(ssid, password);
    while (WiFi.waitForConnectResult() != WL_CONNECTED)
    {
        Serial.println("Connection Failed! Rebooting...");
        ESP.restart();
    }

    // Identification
    Serial.println();
    Serial.print("file: ");
    Serial.println(vfname);
    Serial.print("timestamp (local time): ");
    Serial.println(vtimestamp);
    Serial.println();

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

    Serial.println("Ready");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());

    server.on("/", handle_OnConnect);
    server.onNotFound(handle_NotFound);

    server.begin();
    Serial.println("HTTP server started");

    bme.begin(BME_ADDR);
}

void loop()
{
    ArduinoOTA.handle();
    server.handleClient();

    unsigned long currentMillis = millis();
    // loop to blink without delay
    if (currentMillis - previousMillis >= interval)
    {
        // save the last time you changed the LED state
        previousMillis = currentMillis;
        ledcount++;
        ledmod = ledcount % 128;
        if (ledmod % 64 == 0 || ledmod == 74 || ledmod == 84)
        {
            ledState = 0; // Oddly, this turns LED _ON_
        }
        else if (ledmod % 64 == 1 || ledmod == 75 || ledmod == 85)
        {
            ledState = 1; // turns LED _OFF_
        }
        // Serial.print( ledState);
        // Serial.println(" blink");
        // set the LED with the ledState of the variable:
        digitalWrite(led, ledState);
    }

#ifdef PHANT01
    // handle phant reporting
    if (currentMillis >= phantNextReport)
    {
        handlePhantReport();
        phantNextReport = currentMillis + phantInterval;
    }
#endif // PHANT01

#ifdef IFTTT
    // handle ifttt reporting
    if (currentMillis >= iftttNextReport)
    {
        handleIftttReport();
        iftttNextReport = currentMillis + iftttInterval;
    }
#endif // IFTTT
}

void handle_OnConnect()
{
    temperatureC = bme.readTemperature();
    temperatureF = 32.0 + (9.0 * temperatureC / 5.0);
    humidity = bme.readHumidity();
    pressure = bme.readPressure() / 100.0F;
    altitude = bme.readAltitude(SEALEVELPRESSURE_HPA);
    server.send(
        200, "text/html",
        SendHTML(temperatureC, temperatureF, humidity, pressure, altitude));
}

void handle_NotFound()
{
    server.send(404, "text/plain", "Not found");
}

String SendHTML(float temperatureC, float temperaturef, float humidity,
                float pressure, float altitude)
{
    String ptr =
        "<!DOCTYPE html>"
        "<html>"
        "<head>"
        "<title>Linnea's Weather Station</br>(runs on ESP8266 with a "
        "BME280)</title>"
        "<meta name='viewport' content='width=device-width, "
        "initial-scale=1.0'>"
        "<link "
        "href='https://fonts.googleapis.com/"
        "css?family=Open+Sans:300,400,600' rel='stylesheet'>"
        "<style>"
        "html { font-family: 'Open Sans', sans-serif; display: block; "
        "margin: 0px auto; text-align: center;color: #444444;}"
        "body{margin: 0px;} "
        "h1 {margin: 50px auto 30px;} "
        ".side-by-side{display: table-cell;vertical-align: middle;position: "
        "relative;}"
        ".text{font-weight: 600;font-size: 19px;width: 200px;}"
        ".reading{font-weight: 300;font-size: 50px;padding-right: 25px;}"
        ".temperature .reading{color: #F29C1F;}"
        ".humidity .reading{color: #3B97D3;}"
        ".pressure .reading{color: #26B99A;}"
        ".altitude .reading{color: #955BA5;}"
        ".superscript{font-size: 17px;font-weight: 600;position: "
        "absolute;top: 10px;}"
        ".data{padding: 10px;}"
        ".container{display: table;margin: 0 auto;}"
        ".icon{width:65px}"
        "</style>"
        "<script>\n"
        "setInterval(loadDoc,5000);\n"
        "function loadDoc() {\n"
        "var xhttp = new XMLHttpRequest();\n"
        "xhttp.onreadystatechange = function() {\n"
        "if (this.readyState == 4 && this.status == 200) {\n"
        "document.body.innerHTML =this.responseText}\n"
        "};\n"
        "xhttp.open(\"GET\", \"/\", true);\n"
        "xhttp.send();\n"
        "}\n"
        "</script>\n"
        "</head>"
        "<body>"
        "<h1>ESP8266 Weather Station</h1>"
        "<div class='container'>"
        "<div class='data temperature'>"
        "<div class='side-by-side icon'>"
        "<svg enable-background='new 0 0 19.438 54.003'height=54.003px "
        "id=Layer_1 version=1.1 viewBox='0 0 19.438 54.003'width=19.438px "
        "x=0px xml:space=preserve xmlns=http://www.w3.org/2000/svg "
        "xmlns:xlink=http://www.w3.org/1999/xlink y=0px><g><path "
        "d='M11.976,8.82v-2h4.084V6.063C16.06,2.715,13.345,0,9.996,0H9."
        "313C5.965,0,3.252,2.715,3.252,6.063v30.982"
        "C1.261,38.825,0,41.403,0,44.286c0,5.367,4.351,9.718,9.719,9.718c5."
        "368,0,9.719-4.351,9.719-9.718"
        "c0-2.943-1.312-5.574-3.378-7.355V18.436h-3.914v-2h3.914v-2.808h-4."
        "084v-2h4.084V8.82H11.976z M15.302,44.833"
        "c0,3.083-2.5,5.583-5.583,5.583s-5.583-2.5-5.583-5.583c0-2.279,1."
        "368-4.236,3.326-5.104V24.257C7.462,23.01,8.472,22,9.719,22"
        "s2.257,1.01,2.257,2.257V39.73C13.934,40.597,15.302,42.554,15.302,"
        "44.833z'fill=#F29C21 /></g></svg>"
        "</div>"
        "<div class='side-by-side text'>Temperature</div>"
        "<div class='side-by-side reading'>";

    ptr += (int)temperatureF;

    ptr += "<span class='superscript'>&deg;F</span></div>"
           "<div class='side-by-side reading'> (";

    ptr += (int)temperatureC;

    ptr += "<span class='superscript'>&deg;C</span>&nbsp;&nbsp;)</div>"
           "</div>";
    "<div class='data humidity'>"
    "<div class='side-by-side icon'>"
    "<svg enable-background='new 0 0 29.235 40.64'height=40.64px "
    "id=Layer_1 version=1.1 viewBox='0 0 29.235 40.64'width=29.235px "
    "x=0px xml:space=preserve xmlns=http://www.w3.org/2000/svg "
    "xmlns:xlink=http://www.w3.org/1999/xlink y=0px><path "
    "d='M14.618,0C14.618,0,0,17.95,0,26.022C0,34.096,6.544,40.64,14.618,"
    "40.64s14.617-6.544,14.617-14.617";
    "C29.235,17.95,14.618,0,14.618,0z "
    "M13.667,37.135c-5.604,0-10.162-4.56-10.162-10.162c0-0.787,0.638-1."
    "426,1.426-1.426";
    "c0.787,0,1.425,0.639,1.425,1.426c0,4.031,3.28,7.312,7.311,7.312c0."
    "787,0,1.425,0.638,1.425,1.425";
    "C15.093,36.497,14.455,37.135,13.667,37.135z'fill=#3C97D3 /></svg>"
    "</div>"
    "<div class='side-by-side text'>Humidity</div>"
    "<div class='side-by-side reading'>";

    ptr += (int)humidity;

    ptr +=
        "<span class='superscript'>%</span></div>"
        "</div>"
        "<div class='data pressure'>"
        "<div class='side-by-side icon'>"
        "<svg enable-background='new 0 0 40.542 40.541'height=40.541px "
        "id=Layer_1 version=1.1 viewBox='0 0 40.542 40.541'width=40.542px "
        "x=0px xml:space=preserve xmlns=http://www.w3.org/2000/svg "
        "xmlns:xlink=http://www.w3.org/1999/xlink y=0px><g><path "
        "d='M34.313,20.271c0-0.552,0.447-1,1-1h5.178c-0.236-4.841-2.163-9."
        "228-5.214-12.593l-3.425,3.424"
        "c-0.195,0.195-0.451,0.293-0.707,0.293s-0.512-0.098-0.707-0.293c-0."
        "391-0.391-0.391-1.023,0-1.414l3.425-3.424"
        "c-3.375-3.059-7.776-4.987-12.634-5.215c0.015,0.067,0.041,0.13,0."
        "041,0.202v4.687c0,0.552-0.447,1-1,1s-1-0.448-1-1V0.25"
        "c0-0.071,0.026-0.134,0.041-0.202C14.39,0.279,9.936,2.256,6.544,5."
        "385l3.576,3.577c0.391,0.391,0.391,1.024,0,1.414"
        "c-0.195,0.195-0.451,0.293-0.707,0.293s-0.512-0.098-0.707-0.293L5."
        "142,6.812c-2.98,3.348-4.858,7.682-5.092,12.459h4.804"
        "c0.552,0,1,0.448,1,1s-0.448,1-1,1H0.05c0.525,10.728,9.362,19.271,"
        "20.22,19.271c10.857,0,19.696-8.543,20.22-19.271h-5.178"
        "C34.76,21.271,34.313,20.823,34.313,20.271z "
        "M23.084,22.037c-0.559,1.561-2.274,2.372-3.833,1.814"
        "c-1.561-0.557-2.373-2.272-1.815-3.833c0.372-1.041,1.263-1.737,2."
        "277-1.928L25.2,7.202L22.497,19.05"
        "C23.196,19.843,23.464,20.973,23.084,22.037z'fill=#26B999 /></g></svg>"
        "</div>"
        "<div class='side-by-side text'>Pressure</div>"
        "<div class='side-by-side reading'>";

    ptr += (int)pressure;

    ptr +=
        "<span class='superscript'>hPa</span></div>"
        "</div>"
        "<div class='data altitude'>"
        "<div class='side-by-side icon'>"
        "<svg enable-background='new 0 0 58.422 40.639'height=40.639px "
        "id=Layer_1 version=1.1 viewBox='0 0 58.422 40.639'width=58.422px "
        "x=0px xml:space=preserve xmlns=http://www.w3.org/2000/svg "
        "xmlns:xlink=http://www.w3.org/1999/xlink y=0px><g><path "
        "d='M58.203,37.754l0.007-0.004L42.09,9.935l-0.001,0.001c-0.356-0."
        "543-0.969-0.902-1.667-0.902"
        "c-0.655,0-1.231,0.32-1.595,0.808l-0.011-0.007l-0.039,0.067c-0.021,"
        "0.03-0.035,0.063-0.054,0.094L22.78,37.692l0.008,0.004"
        "c-0.149,0.28-0.242,0.594-0.242,0.934c0,1.102,0.894,1.995,1.994,1."
        "995v0.015h31.888c1.101,0,1.994-0.893,1.994-1.994"
        "C58.422,38.323,58.339,38.024,58.203,37.754z'fill=#955BA5 /><path "
        "d='M19.704,38.674l-0.013-0.004l13.544-23.522L25.13,1.156l-0.002,0."
        "001C24.671,0.459,23.885,0,22.985,0"
        "c-0.84,0-1.582,0.41-2.051,1.038l-0.016-0.01L20.87,1.114c-0.025,0."
        "039-0.046,0.082-0.068,0.124L0.299,36.851l0.013,0.004"
        "C0.117,37.215,0,37.62,0,38.059c0,1.412,1.147,2.565,2.565,2.565v0."
        "015h16.989c-0.091-0.256-0.149-0.526-0.149-0.813"
        "C19.405,39.407,19.518,39.019,19.704,38.674z'fill=#955BA5 /></g></svg>"
        "</div>"
        "<div class='side-by-side text'>Altitude</div>"
        "<div class='side-by-side reading'>";

    ptr += (int)altitude;

    ptr += "<span class='superscript'>m</span></div>"
           "</div>"
           "</div>"
           "<h2>(runs on ESP8266 with a BME280)</h2>"
           "</body>"
           "</html>";
    return ptr;
}

#ifdef PHANT01
void handlePhantReport()
{
    // Send data to logging site
    Serial.print("sending data to ");
    Serial.println(phantHost);

    float tempC = bme.readTemperature();
    float tempf = 32.0 + (9.0 * tempC / 5.0);
    float humidity = bme.readHumidity();
    float barom = bme.readPressure() / 100.0F;

    // Using HTTPS protocol
    WiFiClientSecure client;
    client.setInsecure(); // See BearSSL documentation
    if (!client.connect(phantHost, 443))
    {
        Serial.println("Connection Fail");
        // return;
    }
    if (client.verify(phantCertFingerprint, phantHost))
    {
        Serial.println(
            "certificate matches"); // With BearSSL set to Insecure mode, it
                                    // will always report that it matches
    }
    else
    {
        Serial.println("certificate doesn't match");
        // return;
    }

    String ReqData = "barom=";
    ReqData += barom;
    ReqData += "&humidity=";
    ReqData += humidity;
    ReqData += "&tempf=";
    ReqData += tempf;
    ReqData += "\r\n";
    Serial.println("ReqData= " + ReqData);

    String WebReq = "POST ";
    WebReq += phantWebPage;
    WebReq += " HTTP/1.1\r\n";
    WebReq += "Host: ";
    WebReq += phantHost;
    WebReq += "\r\n";
    WebReq += "Phant-Private-Key: ";
    WebReq += phantPrivKey;
    WebReq += "\r\n";
    WebReq += "Connection: "
              "close"
              "\r\n";
    WebReq += "Content-Length: ";
    WebReq += ReqData.length();
    WebReq += "\r\n";
    WebReq += "Content-Type: application/x-www-form-urlencoded\r\n";
    WebReq += "\r\n";  // end of headers
    WebReq += ReqData; // POST data

    Serial.println("WebReq= " + WebReq);
    client.print(WebReq);

    Serial.println("-----Response-----");
    while (client.connected())
    {
        if (client.available())
        {
            String line = client.readStringUntil('\n');
            Serial.println(line);
        }
    }
    Serial.println("----------");
}
#endif // PHANT01

#ifdef IFTTT
void handleIftttReport()
{
    // Send data to notification site
    Serial.print("sending data to ");
    Serial.println(iftttHost);

    float tempC = bme.readTemperature();
    float tempf = 32.0 + (9.0 * tempC / 5.0);
    float humidity = bme.readHumidity();
    float barom = bme.readPressure() / 100.0F;

    // Using HTTPS protocol
    WiFiClientSecure client;
    client.setInsecure(); // See BearSSL documentation
    if (!client.connect(iftttHost, 443))
    {
        Serial.println("Connection Fail");
        // return;
    }
    if (client.verify(iftttCertFingerprint, iftttHost))
    {
        Serial.println(
            "certificate matches"); // With BearSSL set to Insecure mode, it
                                    // will always report that it matches
    }
    else
    {
        Serial.println("certificate doesn't match");
        // return;
    }

    // IFTTT will accept JSON here
    String ReqData = "( ";
    ReqData += "\"value1\"    : \"";
    ReqData += barom;
    ReqData += "\", ";
    ReqData += "\"value2\"    : \"";
    ReqData += humidity;
    ReqData += "\", ";
    ReqData += "\"value3\"    : \"";
    ReqData += tempf;
    ReqData += "\" ";
    ReqData += "}\r\n";
    Serial.println("ReqData= " + ReqData);

    String WebReq = "POST ";
    WebReq += iftttWebPage;
    WebReq += iftttEventname;
    WebReq += "/with/key/";
    WebReq += iftttMakerKey;
    WebReq += " HTTP/1.1\r\n";
    WebReq += "Host: ";
    WebReq += iftttHost;
    WebReq += "\r\n";
    WebReq += "Connection: "
              "keep-alive"
              "\r\n";
    WebReq += "Content-Length: ";
    WebReq += ReqData.length();
    WebReq += "\r\n";
    /// WebReq += "Content-Type: application/json" "\r\n";
    WebReq += "\r\n";  // end of headers
    WebReq += ReqData; // POST data

    Serial.println("WebReq= " + WebReq);
    client.print(WebReq);

    Serial.println("-----Response-----");
    unsigned long contentLength = 0;
    bool EOHseen = 0;
    while (client.connected())
    {
        String line;
        if (client.available() and !EOHseen)
        {
            line = client.readStringUntil('\n');
            Serial.print(line.length());
            Serial.print(" [");
            Serial.print(line);
            Serial.println("]");
            if (line.startsWith("Content-Length:"))
            {
                // find first digit, or EOS
                unsigned int l = line.length();
                unsigned int p = 0;
                while (p <= l && !isDigit(line.charAt(p)))
                {
                    ++p;
                }
                if (p <= l)
                {
                    contentLength = line.substring(p).toInt();
                }
            }
            if (line.startsWith("\r") or line.startsWith("\n") or
                line.length() <= 1)
            {
                EOHseen = 1;
            }
        }
        if (client.available() >= contentLength and EOHseen)
        {
            char payload[contentLength + 1];
            client.readBytes(payload, contentLength);
            Serial.print(contentLength);
            Serial.print(" [");
            Serial.print(payload);
            Serial.println("] ");
            break;
        }
    }
    Serial.println("----------");
}
#endif // IFTTT
