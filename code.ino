#include <ESP8266WiFi.h>
#include <ThingSpeak.h>
#include <DHT.h>

const int soilMoistureAnalogPin = A0;
const int trigPin = 5;
const int echoPin = 4;
const int dhtPin = 12;
const int vibrationPin = 13;
const int flowSensorPin = 14;
const int lampPin = 2;

#define DHTTYPE DHT11
DHT dht(dhtPin, DHTTYPE);

const long floodThreshold = 10;
const long lampThreshold = 15;

const char* ssid = "abcd";
const char* password = "12345678";
const char* apiKey = "41S8TG184QJQ413V";
unsigned long channelNumber = 2665580;

WiFiClient client;

volatile int pulseCount = 0;
float flowRate = 0;
unsigned long lastFlowTime = 0;
const float calibrationFactor = 4.5;

void IRAM_ATTR pulseCounter() {
  pulseCount++;
}

void setup() {
    Serial.begin(9600);
    pinMode(trigPin, OUTPUT);
    pinMode(echoPin, INPUT);
    pinMode(vibrationPin, INPUT_PULLUP);
    pinMode(flowSensorPin, INPUT_PULLUP);
    pinMode(lampPin, OUTPUT);

    dht.begin();

    Serial.print("Connecting to ");
    Serial.println(ssid);
    WiFi.begin(ssid, password);

    while (WiFi.status() != WL_CONNECTED) {
        delay(1000);
        Serial.print(".");
    }
    Serial.println("\nWiFi connected.");

    ThingSpeak.begin(client);
    attachInterrupt(digitalPinToInterrupt(flowSensorPin), pulseCounter, FALLING);
}

void loop() {
    long duration, distance;

    digitalWrite(trigPin, LOW);
    delayMicroseconds(2);
    digitalWrite(trigPin, HIGH);
    delayMicroseconds(10);
    digitalWrite(trigPin, LOW);

    duration = pulseIn(echoPin, HIGH);
    distance = duration * 0.034 / 2;
    Serial.print("Distance: ");
    Serial.print(distance);
    Serial.println(" cm");

    if (distance < floodThreshold) {
        Serial.println("Alert: There is a high probability of flooding!");
    } else {
        Serial.println("No Flood Detected.");
    }

    if (distance < lampThreshold) {
        digitalWrite(lampPin, HIGH);
        Serial.println("Lamp ON: Distance less than 15 cm");
    } else {
        digitalWrite(lampPin, LOW);
        Serial.println("Lamp OFF: Distance greater than or equal to 15 cm");
    }

    float humidity = dht.readHumidity();
    float temperature = dht.readTemperature();

    if (isnan(humidity) || isnan(temperature)) {
        Serial.println("Failed to read from DHT sensor!");
    } else {
        Serial.print("Temperature: ");
        Serial.print(temperature);
        Serial.println("°C");

        Serial.print("Humidity: ");
        Serial.print(humidity);
        Serial.println("%");
    }

    int vibrationState = digitalRead(vibrationPin);
    bool isVibrating = (vibrationState == LOW);

    if (isVibrating) {
        Serial.println("Alert: High Vibration Detected!");
    } else {
        Serial.println("Normal: No problem detected.");
    }

    int soilMoistureAnalogValue = analogRead(soilMoistureAnalogPin);
    Serial.print("Soil Moisture (Analog): ");
    Serial.println(soilMoistureAnalogValue);

    int soilMoisturePercent = map(soilMoistureAnalogValue, 1023, 0, 0, 100);
    Serial.print("Soil Moisture (%): ");
    Serial.print(soilMoisturePercent);
    Serial.println("%");

    if (soilMoisturePercent >= 71) {
        Serial.println("Alert: Wet condition detected. Potential risk of flood.");
    } else if (soilMoisturePercent >= 21) {
        Serial.println("Moist Condition.");
    } else {
        Serial.println("Dry Condition.");
    }

    unsigned long currentTime = millis();
    if (currentTime - lastFlowTime >= 1000) {
        detachInterrupt(digitalPinToInterrupt(flowSensorPin));
        flowRate = (pulseCount / calibrationFactor);
        pulseCount = 0;

        Serial.print("Water Flow Rate: ");
        Serial.print(flowRate);
        Serial.println(" L/min");

        lastFlowTime = currentTime;
        attachInterrupt(digitalPinToInterrupt(flowSensorPin), pulseCounter, FALLING);
    }

    ThingSpeak.setField(1, distance);
    ThingSpeak.setField(2, temperature);
    ThingSpeak.setField(3, humidity);
    ThingSpeak.setField(4, soilMoisturePercent);
    ThingSpeak.setField(5, isVibrating ? 1 : 0);
    ThingSpeak.setField(6, flowRate);

    int result = ThingSpeak.writeFields(channelNumber, apiKey);
    if (result == 200) {
        Serial.println("Data sent to ThingSpeak successfully.");
    } else {
        Serial.print("Failed to send data to ThingSpeak. Error code: ");
        Serial.println(result);
    }

    delay(10000);
}
