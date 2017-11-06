#include <Arduino.h>
#include <Adafruit_MMA8451.h>
#include <Adafruit_Sensor.h>
#include <WiFi.h>
#include <PubSubClient.h>

#include <main.h>

// Accelerometer MMA8451
Adafruit_MMA8451 mma = Adafruit_MMA8451();

WiFiClient espClient;
PubSubClient client(espClient);

// Global Values
long lastMsg = 0;
char msg[50];
int value = 0;

void setup_wifi()
{
    delay(10);
    // We start by connecting to a WiFi network
    Serial.println();
    Serial.print("Connecting to ");
    Serial.println(ssid);

    WiFi.begin(ssid, password);

    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);
        Serial.print(".");
    }

    randomSeed(micros());

    Serial.println("");
    Serial.println("WiFi connected");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
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

void reconnect()
{
    // Loop until we're reconnected
    while (!client.connected())
    {
        Serial.print("Attempting MQTT connection...");

        // Attempt to connect
        if (client.connect("ESP32", mqtt_user, mqtt_pass))
        {
            Serial.println("connected");
            // Once connected, publish an announcement...
            client.publish(mqtt_out_topic, "acc data connected");
            // ... and resubscribe
            client.subscribe(mqtt_in_topic);
        }
        else
        {
            Serial.print("failed, rc=");
            Serial.print(client.state());
            Serial.println(" try again in 5 seconds");
            // Wait 5 seconds before retrying
            delay(5000);
        }
    }
}

void setup(void)
{
    Serial.begin(9600);

    setup_wifi();
    setup_accl();

    client.setServer(mqtt_server, mqtt_port);
}

void loop()
{
    // Check the MQTT Connection
    if (!client.connected())
    {
        reconnect();
    }
    client.loop();

    // Read the 'raw' data in 14-bit counts
    mma.read();

    /* Get a new sensor event */
    sensors_event_t event;
    mma.getEvent(&event);

    long now = millis();
    if (now - lastMsg > 2000)
    {
        lastMsg = now;
        ++value;
        snprintf(
            msg, 
            75, 
            "%f,%f,%f", 
            event.acceleration.x,
            event.acceleration.y,
            event.acceleration.z
        );
        Serial.print("Publish message: ");
        Serial.println(msg);
        client.publish(mqtt_out_topic, msg);
    }
}