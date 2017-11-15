/********************************************/
/*                 Imports                  */
/********************************************/
#include <Arduino.h>

// Accelerometer
#include <Adafruit_MMA8451.h>
#include <Adafruit_Sensor.h>

// AWS IoT Support
#include <AWS_IOT.h>

// Wifi and NTP Support
#include <WiFi.h>
#include <NTPClient.h>

// JSON Support
#include <ArduinoJson.h>

// Local Settings
#include <main.h>

/********************************************/
/*                 Globals                  */
/********************************************/
// Accelerometer MMA8451
Adafruit_MMA8451 mma = Adafruit_MMA8451();

// Accelerometer threshold
float accl_mag_thresh = 12.0f;

// AWS_IOT Lib
AWS_IOT aws_iot;

// NTP Client
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "au.pool.ntp.org", 3600, 60000);

// Misc Values
int status = WL_IDLE_STATUS;
int msgCount = 0, msgReceived = 0;
bool realtime = false;
char payload[512];
char rcvdPayload[512];

void awsSubCallBackHandler(char *topicName, int payloadLen, char *payLoad)
{
    strncpy(rcvdPayload, payLoad, payloadLen);
    rcvdPayload[payloadLen] = 0;
    msgReceived = 1;
}

void setup_wifi()
{
    while (status != WL_CONNECTED)
    {
        // Connect to WPA/WPA2 network.
        status = WiFi.begin(ssid, password);

        // wait 2 seconds for connection:
        delay(2000);
    }
}

void setup_ntp()
{
    timeClient.begin();
    Serial.println("ntp [Connected]");
}

void setup_aws_iot()
{
    if (aws_iot.connect(aws_mqtt_server, aws_mqtt_client_id) == 0)
    {
        // Wait 1 second for connection
        delay(1000);

        Serial.println("aws [Connected]");

        if (0 == aws_iot.subscribe(aws_mqtt_thing_topic_sub, awsSubCallBackHandler))
        {
            Serial.println("aws-sub [Connected]");
        }
        else
        {
            Serial.println("aws-sub [Failed]");
            while (1)
                ;
        }
    }
    else
    {
        Serial.println("aws [Failed]");
        while (1)
            ;
    }
}

void setup_accl()
{
    if (!mma.begin())
    {
        Serial.println("accl [Failed]");
        while (1)
            ;
    }
    Serial.println("accl [Connected]");
    mma.setRange(MMA8451_RANGE_2_G);
}

void setup()
{
    Serial.begin(9600);
    delay(10);

    setup_wifi();
    setup_ntp();
    setup_aws_iot();
    setup_accl();

    delay(2000);
}

void loop()
{
    // 10hz delay
    delay(100);

    // NTP Update
    timeClient.update();

    // Read the 'raw' data in 14-bit counts
    // Get a new sensor event
    mma.read();
    sensors_event_t event;
    mma.getEvent(&event);

    // Magnitude of values
    float accl_mag = sqrt(
        pow(event.acceleration.x, 2) +
        pow(event.acceleration.y, 2) +
        pow(event.acceleration.z, 2));

    if (msgReceived == 1)
    {
        msgReceived = 0;
        Serial.print("Received Message:");
        Serial.println(rcvdPayload);
        realtime = !realtime;
    }
    if (accl_mag >= accl_mag_thresh || realtime)
    {
        // JSON buffer
        StaticJsonBuffer<200> jsonBuffer;
        JsonObject &root = jsonBuffer.createObject();

        root["thing_id"] = thing_id;
        root["timestamp"] = timeClient.getEpochTime();
        root["accl_x"] = event.acceleration.x;
        root["accl_y"] = event.acceleration.y;
        root["accl_z"] = event.acceleration.z;
        root["accl_mag"] = accl_mag;

        String json_output;
        root.printTo(json_output);

        // Construct payload item
        json_output.toCharArray(payload, 160);

        if (aws_iot.publish(aws_mqtt_thing_topic_pub, payload) == 0)
        {
            Serial.print("aws-pub [Success]: ");
            Serial.println(payload);
        }
        else
        {
            Serial.println("aws-pub [Failed]");
        }
    }
}