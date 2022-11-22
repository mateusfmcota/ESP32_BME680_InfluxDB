// https://microcontrollerslab.com/bme680-esp32-arduino-oled-display/
// https://arduinojson.org/v6/api/jsonobject/createnestedobject/
// https://stackoverflow.com/questions/62267292/fastapi-pydantic-accept-arbitrary-post-request-body
// https://www.techcoil.com/blog/how-to-post-json-data-to-a-http-server-endpoint-from-your-esp32-development-board-with-arduinojson/
// https://github.com/524c/esp32-grafana
#include <InfluxDbClient.h>
#include <WiFiMulti.h>

#include <Wire.h>
#include <Adafruit_Sensor.h>
#include "Adafruit_BME680.h"
#include "./secrets.h"

Adafruit_BME680 bme; // I2C
WiFiMulti wifiMulti;

InfluxDBClient client(INFLUXDB_URL, INFLUXDB_ORG, INFLUXDB_BUCKET, INFLUXDB_TOKEN);

void setup() {
  Serial.begin(9600);
  
  //Setup BME680
  if (!bme.begin()) {
    Serial.println("Could not find a valid BME680 sensor, check wiring!");
    while (1);
  }

  //Setup WIFI
  wifiMulti.addAP(AP_SSID, AP_PWD);

  Serial.print("Connecting to wifi");
  while (wifiMulti.run() != WL_CONNECTED) {
    Serial.print(".");
    delay(100);
  }
  Serial.println("wifi connected");

  //Setting up the esp32 RTC to the defined timezone
  timeSync(TZ_INFO, "pool.ntp.org", "time.nis.gov");
  
  // Set up oversampling and filter initialization
  bme.setTemperatureOversampling(BME680_OS_8X);
  bme.setHumidityOversampling(BME680_OS_2X);
  bme.setPressureOversampling(BME680_OS_4X);
  bme.setIIRFilterSize(BME680_FILTER_SIZE_3);
  bme.setGasHeater(320, 150); // 320*C for 150 ms

   // Check server connection
  if (client.validateConnection()) {
    Serial.print("Connected to InfluxDB: ");
    Serial.println(client.getServerUrl());
  } else {
    Serial.print("InfluxDB connection failed: ");
    Serial.println(client.getLastErrorMessage());
  }

}

struct BmeData{
  float temperature;
  float pressure;
  float humidity;
  uint32_t gas;
  float altitude;
};

// A function which reads the data from the bme680 and stores into a struct
struct BmeData getBme680Data(){
    if (! bme.performReading()) {
      Serial.println("Failed to perform reading :(");
      return nullptr;
    }
    BmeData sensorData;
    sensorData.temperature = bme.temperature;
    sensorData.pressure = (bme.pressure / 100.0);
    sensorData.humidity = bme.humidity;
    sensorData.gas = (bme.gas_resistance / 1000.0);
    sensorData.altitude = bme.readAltitude(SEALEVELPRESSURE_HPA);

    return sensorData;
}

void postDataToServer(BmeData bmeData ){
  Point sensorData("sensorData");
  sensorData.addField("temperature", bmeData.temperature,2);
  sensorData.addField("humidity",    bmeData.humidity,2);
  sensorData.addField("gas",         bmeData.gas);
  sensorData.addField("pressure",    bmeData.pressure);
  sensorData.addField("altitude",    bmeData.altitude,2);
  
  if (wifiMulti.run() != WL_CONNECTED) {
    Serial.println("Wifi connection lost");
  }

  // Write point
  if (!client.writePoint(sensorData)) {
    Serial.print("InfluxDB write failed: ");
    Serial.println(client.getLastErrorMessage());
  }

  //30s delay between the readings
  delay(30000);

}

void loop() {
  BmeData bmeData = getBme680Data();
  if(bmeData != nullptr){
    postDataToServer(bmeData);
  }
}
