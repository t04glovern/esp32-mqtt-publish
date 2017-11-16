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

// FFT and Filtering
#include <Filters.h>
#include <arduinoFFT.h>

// Local Settings
#include <main.h>

// FFT and Filtering definitions
#define Nbins 32
#define FILTER_FREQUENCY 10   // filter 10Hz and higher
#define SAMPLES_PER_SECOND 64 //sample at 30Hz - Needs to be minimum 2x higher than desired filterFrequency

/********************************************/
/*                 Globals                  */
/********************************************/
// Accelerometer MMA8451
Adafruit_MMA8451 mma = Adafruit_MMA8451();

// FFT
arduinoFFT FFT = arduinoFFT();

// Accelerometer threshold
float energy_thresh = 15.0f;

// AWS_IOT Lib
AWS_IOT aws_iot;

// NTP Client
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "au.pool.ntp.org", 3600, 60000);

// Misc Values (Filtering)
double vReal[Nbins];
double vImag[Nbins];
float delayTime;
float accl_mag;
int lastTime = 0;

FilterOnePole filterX(LOWPASS, FILTER_FREQUENCY);
FilterOnePole filterY(LOWPASS, FILTER_FREQUENCY);
FilterOnePole filterZ(LOWPASS, FILTER_FREQUENCY);

// Misc Values (WiFi & MQTT)
int status = WL_IDLE_STATUS;
int msgCount = 0, msgReceived = 0;
bool realtime = false;
char payload[512];
char rcvdPayload[512];

double totalEnergy(float array[])
{
   int i;
   double integrate = 0;
   for (i=2; i<Filter_Frequency*2; i+=2)
   {
       //taking basic numerical value for integration, multiplying bin frequency width by its height.
	 integrate +=  array[i] * 1;
   }
   return(integrate);
}

float filteredMagnitude(float ax, float ay, float az)
{
    filterX.input(ax);
    filterY.input(ay);
    filterZ.input(az);
    return (sqrt(pow(filterX.output(), 2) + pow(filterY.output(), 2) + pow(filterZ.output(), 2)));
}

float normalMagnitude(float ax, float ay, float az)
{
    return (sqrt(pow(ax, 2) + pow(ay, 2) + pow(az, 2)));
}

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
    delay(1000);

    setup_wifi();
    setup_ntp();
    setup_aws_iot();
    setup_accl();

    delayTime = 1 / SAMPLES_PER_SECOND * 1000; //in millis

    delay(2000);
}

void loop()
{
    // NTP Update
    timeClient.update();

    sensors_event_t event;

    //run it at 30 samples/s
    for (int ii = 0; ii <= Nbins; ii++)
    {
        // delay
        if (millis() < lastTime + delayTime)
        {
            delay(lastTime + delayTime - millis());
        }

        // Read the 'raw' data in 14-bit counts
        // Get a new sensor event
        mma.read();
        mma.getEvent(&event);

        // Magnitude of values
        vReal[ii] = filteredMagnitude(event.acceleration.x, event.acceleration.y, event.acceleration.z);
        vImag[ii] = 0;
        lastTime = millis();
    }

    FFT.Windowing(vReal, Nbins, FFT_WIN_TYP_HAMMING, FFT_FORWARD);
    FFT.Compute(vReal, vImag, Nbins, FFT_FORWARD);
    FFT.ComplexToMagnitude(vReal, vImag, Nbins);
    accl_mag = normalMagnitude(event.acceleration.x, event.acceleration.y, event.acceleration.z);

    if (msgReceived == 1)
    {
        msgReceived = 0;
        Serial.print("Received Message:");
        Serial.println(rcvdPayload);
        realtime = !realtime;
    }
    //since we've calculated the frequency, check the ranges we care about (1hz-8hz)
    //sum them and check if they're higher than our threshold.
    if (totalEnergy(vReal) >= energy_thresh || realtime)
    {
        // JSON buffer
        StaticJsonBuffer<220> jsonBuffer;
        JsonObject &root = jsonBuffer.createObject();

        root["thing_id"] = thing_id;
        root["timestamp"] = timeClient.getEpochTime();
        root["accl_x"] = event.acceleration.x;
        root["accl_y"] = event.acceleration.y;
        root["accl_z"] = event.acceleration.z;
        root["accl_mag"] = accl_mag;

        JsonArray& accl_fft_data = root.createNestedArray("accl_fft");
        for(int ii = 2; ii <= Nbins - 1;ii += 2)
        {
            accl_fft_data.add(vReal[ii]);
        }

        String json_output;
        root.printTo(json_output);

        // Construct payload item
        json_output.toCharArray(payload, 220);

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
