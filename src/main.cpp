#include <Arduino.h>
#include <Adafruit_MMA8451.h>
#include <Adafruit_Sensor.h>
#include <AWS_IOT.h>
#include <WiFi.h>

// Local Settings
#include <main.h>

// AWS_IOT Lib
AWS_IOT aws_iot;

// Accelerometer MMA8451
Adafruit_MMA8451 mma = Adafruit_MMA8451();

// Global Values
int status = WL_IDLE_STATUS;
int tick = 0, msgCount = 0, msgReceived = 0;
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
        Serial.print("Attempting to connect to SSID: ");
        Serial.println(ssid);

        // Connect to WPA/WPA2 network.
        status = WiFi.begin(ssid, password);

        // wait 5 seconds for connection:
        delay(5000);
    }

    Serial.println("Connected to wifi");
}

void setup_aws_iot()
{
    if (aws_iot.connect(aws_mqtt_server, aws_mqtt_client_id) == 0)
    {
        Serial.println("Connected to AWS");
        delay(1000);

        if (0 == aws_iot.subscribe(aws_mqtt_thing_topic, awsSubCallBackHandler))
        {
            Serial.println("Subscribe Successfull");
        }
        else
        {
            Serial.println("Subscribe Failed, Check the Thing Name and Certificates");
            while (1)
                ;
        }
    }
    else
    {
        Serial.println("AWS connection failed, Check the HOST Address");
        while (1)
            ;
    }
}

void setup_accl()
{
    if (!mma.begin())
    {
        Serial.println("MMA8451 Couldnt start");
        while (1)
            ;
    }
    Serial.println("MMA8451 found!");

    mma.setRange(MMA8451_RANGE_2_G);

    Serial.print("Range = ");
    Serial.print(2 << mma.getRange());
    Serial.println("G");
}

void setup()
{
    Serial.begin(9600);
    delay(10);

    setup_wifi();
    setup_aws_iot();
    setup_accl();

    delay(2000);
}

void loop()
{
    // Read the 'raw' data in 14-bit counts
    mma.read();

    // Get a new sensor event
    sensors_event_t event;
    mma.getEvent(&event);

    if (msgReceived == 1)
    {
        msgReceived = 0;
        Serial.print("Received Message:");
        Serial.println(rcvdPayload);
    }
    if (tick >= 2) // publish to topic every 2 seconds
    {
        tick = 0;

        // Construct payload item
        snprintf(
            payload,
            75,
            "%f,%f,%f", 
            event.acceleration.x,
            event.acceleration.y,
            event.acceleration.z
        );

        if (aws_iot.publish(aws_mqtt_thing_topic, payload) == 0)
        {
            Serial.print("Publish Message:");
            Serial.println(payload);
        }
        else
        {
            Serial.println("Publish failed");
        }
    }
    vTaskDelay(1000 / portTICK_RATE_MS);
    tick++;
}