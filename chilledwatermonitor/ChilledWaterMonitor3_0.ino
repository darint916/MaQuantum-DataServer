// ================ ====================

// // ==== I2C 128x64 oled ====
#include <Wire.h>
#include "SSD1306Ascii.h"
#include "SSD1306AsciiWire.h"
// 0X3C+SA0 - 0x3C or 0x3D
#define I2C_ADDRESS_OLED 0x3C
// Define proper RST_PIN if required.
#define RST_PIN -1

// ==== ADC ======
#include <Adafruit_ADS1X15.h>
#define I2C_ADDRESS_ADC 0x48
Adafruit_ADS1115 ads1115;

// ==== DHT =====
#include <DHT.h>

// ==== InfluxDB ====
#if defined(ESP32)
#include <WiFiMulti.h>
WiFiMulti wifiMulti;
#define DEVICE "ESP32"
#elif defined(ESP8266)
#include <ESP8266WiFiMulti.h>
ESP8266WiFiMulti wifiMulti;
#define DEVICE "ESP8266"
#define WIFI_AUTH_OPEN ENC_TYPE_NONE
#endif

#include <InfluxDbClient.h>
#include <InfluxDbCloud.h>

// WiFi AP SSID
#define WIFI_SSID "MaLab"
// WiFi password
#define WIFI_PASSWORD "MaLabG41"

// InfluxDB v2 server url, e.g. https://eu-central-1-1.aws.cloud2.influxdata.com (Use: InfluxDB UI -> Load Data -> Client Libraries)
#define INFLUXDB_URL "https://us-central1-1.gcp.cloud2.influxdata.com"
// InfluxDB v2 server or cloud API token (Use: InfluxDB UI -> Data -> API Tokens -> Generate API Token)
#define INFLUXDB_TOKEN "Gnpd4NJVTlLCHhTo80ZjX6gMHhGn4-BlzifQbwfgah-ZB35AaTriLezTmvHP1D2FU8_POUx9-knpwwHEY-Xc0Q=="
// InfluxDB v2 organization id (Use: InfluxDB UI -> User -> About -> Common Ids )
#define INFLUXDB_ORG "ma.quantumlab@gmail.com"
//#define INFLUXDB_ORG "1d17bc6926f18098"
// InfluxDB v2 bucket name (Use: InfluxDB UI ->  Data -> Buckets)
#define INFLUXDB_BUCKET "ChillerMonitorBucket"


#define TZ_INFO "EST5EDT"
#define NTP_SERVER1  "pool.ntp.org"
#define NTP_SERVER2  "time.nis.gov"
#define WRITE_PRECISION WritePrecision::S
#define MAX_BATCH_SIZE 10
#define WRITE_BUFFER_SIZE 30

// InfluxDB client instance with preconfigured InfluxCloud certificate
InfluxDBClient client(INFLUXDB_URL, INFLUXDB_ORG, INFLUXDB_BUCKET, INFLUXDB_TOKEN, InfluxDbCloud2CACert);
// InfluxDB client instance without preconfigured InfluxCloud certificate for insecure connection 
//InfluxDBClient client(INFLUXDB_URL, INFLUXDB_ORG, INFLUXDB_BUCKET, INFLUXDB_TOKEN);

//// Data point
Point chillerStatus("chiller_status");

// Number for loops to sync time using NTP
int iterations = 0;

// oled monitor
SSD1306AsciiWire oled;

// variables for the monitors
bool isWifiConnected = false;
bool isDataSent = false;

int16_t adc0, adc1, adc2, adc3;
float ADC_bit2volt = 4.096/pow(2.0,15);

//float a0_value = 0.0;
float temp_in = 0.0;
float pressure_in = 0.0;
float pressure_out = 0.0;
float pressure_diff = 0.0;

// read A0 on ESP8266
const int analogInPin = A0;

//DHT for Room -- Constants and Variables
#define DHTPIN1 14     // what pin we're connected to
#define DHTTYPE1 DHT22   // DHT 22  (AM2302)
DHT dhtRoom(DHTPIN1, DHTTYPE1); //// Initialize DHT sensor for normal 16mhz Arduino
//Variables
int chk;
float room_hum = 0.0;  //Stores humidity value
float room_temp = 0.0; //Stores temperature value

// DHT for lab -- constants and variables
#define DHTPIN2 12 // what pin we're connected to
#define DHTTYPE2 DHT22 // DHT 22  (AM2302)
DHT dhtLab(DHTPIN2, DHTTYPE2); //// Initialize DHT sensor for normal 16mhz Arduino
//Variables
int chk1;
float lab_hum = 0.0;  //Stores humidity value
float lab_temp = 0.0; //Stores temperature value

// ================== setup ====================
void setup() {
   
  //DHT
  Serial.begin(9600); 
  Serial.println("DHTxx test!");
  dhtRoom.begin();
  dhtLab.begin();
  
  // open serial connection for debugging
  Serial.begin(115200);

  // Accurate time is necessary for certificate validation and writing in batches
  // Syncing progress and the time will be printed to Serial.
  timeSync(TZ_INFO, NTP_SERVER1, NTP_SERVER2);
  
  // open I2C connection
  Wire.begin();
  Wire.setClock(400000L);

  Serial.println("Connecting to OLED display via I2C");
  #if RST_PIN >= 0
  oled.begin(&Adafruit128x64, I2C_ADDRESS_OLED, RST_PIN);
  #else // RST_PIN >= 0
  oled.begin(&Adafruit128x64, I2C_ADDRESS_OLED);
  #endif // RST_PIN >= 0
  oled.setFont(Adafruit5x7);

  Serial.println("Connecting to Adafruit_ADS1115 ADC via I2C");
  ads1115.begin(I2C_ADDRESS_ADC);  // Initialize ads1115 at address
  ads1115.setGain(GAIN_ONE);     // 1x gain   +/- 4.096V
  Serial.println(" - Getting single-ended readings from AIN0..3");
  Serial.println(" - ADC Range: 0 to +4.096V, GAIN_ONE (1 bit = 0.125 mV)");

  // Setup wifi
  WiFi.mode(WIFI_STA);
  wifiMulti.addAP(WIFI_SSID, WIFI_PASSWORD);

  Serial.print("Connecting to wifi");
  while (wifiMulti.run() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  isWifiConnected = true;
  Serial.println();
  Serial.print(" - wifi_SSID: ");
  Serial.println(WiFi.SSID());

  // Add tags
  chillerStatus.addTag("device", DEVICE);
  chillerStatus.addTag("wifi_SSID", WiFi.SSID());

  // Check server connection
  if (client.validateConnection()) {
    Serial.print("Connected to InfluxDB: ");
    Serial.println(client.getServerUrl());
  } else {
    Serial.print("InfluxDB connection failed: ");
    Serial.println(client.getLastErrorMessage());
  }

  // Enable messages batching and retry buffer
  client.setWriteOptions(WriteOptions().writePrecision(WRITE_PRECISION).batchSize(MAX_BATCH_SIZE).bufferSize(WRITE_BUFFER_SIZE));
}


// ================== functions ====================

void readSensorValues(){
  
//  a0_value = analogRead(analogInPin)/1023.0*3.3;
//  temp_in = a0_value;
//  pressure_in = a0_value;
//  pressure_out = a0_value;
//  pressure_diff = a0_value;

  Serial.println("Read ads1115:");
  adc0 = ads1115.readADC_SingleEnded(0);
  adc1 = ads1115.readADC_SingleEnded(1);
  adc2 = ads1115.readADC_SingleEnded(2);
  adc3 = ads1115.readADC_SingleEnded(3);
  Serial.print(" - AIN0: "); Serial.print(adc0); Serial.print(", V="); Serial.println(adc0*ADC_bit2volt);
  Serial.print(" - AIN1: "); Serial.print(adc1); Serial.print(", V="); Serial.println(adc1*ADC_bit2volt);
  Serial.print(" - AIN2: "); Serial.print(adc2); Serial.print(", V="); Serial.println(adc2*ADC_bit2volt);
  Serial.print(" - AIN3: "); Serial.print(adc3); Serial.print(", V="); Serial.println(adc3*ADC_bit2volt);

  // convert to sensor reading: 4-20mA is mapped to 0.88V-4V

  float slope, offset;
  float cal_x1, cal_y1, cal_x2, cal_y2;
  
  // temp sensor: 0-140F
  // cal point 1: 0.8V = 0F = -17.78 C
  // cal point 2: 4.0V = 140F = 60 C
  // increase calibration points x by factor 0f 0.1 in order to account for change in resistor value from Alex's design

  cal_x1 = 0.8*1.1; cal_y1 = -17.78; cal_x2 = 4.0*1.1; cal_y2 = 60.0;
  slope = (cal_y2-cal_y1)/(cal_x2-cal_x1);
  offset = cal_y1 - cal_x1*slope;

  temp_in = adc0*ADC_bit2volt * slope + offset;

  // pressure sensor: 0-50psi
  // cal point 1: 0.8V = 0 psi
  // cal point 2: 4.0V = 50 psi
 // increase calibration points x by factor of 0.1 in order to account for change in resistor value from Alex's design
 
  cal_x1 = 0.8*1.1; cal_y1 = 0.0; cal_x2 = 4.0*1.1; cal_y2 = 50.0;
  slope = (cal_y2-cal_y1)/(cal_x2-cal_x1);
  offset = cal_y1 - cal_x1*slope;
  
  pressure_in = adc1*ADC_bit2volt * slope + offset;
  pressure_out = adc2*ADC_bit2volt * slope + offset;
  
  pressure_diff = pressure_in - pressure_out;
  
}

void readDHT(){
 // Wait a few seconds between measurements.
  delay(2000);
  // read values from monitor
  // room DHT
  room_hum = dhtRoom.readHumidity();
  room_temp = dhtRoom.readTemperature();
  // print values
  Serial.print("Humidity: ");
  Serial.print(room_hum);
  Serial.print(" %, Temp: ");
  Serial.print(room_temp);
  Serial.println(" Celsius");
  // Check if any reads failed and exit early (to try again).
  if (isnan(room_hum) || isnan(room_temp)) {
    Serial.println("Failed to read from Room DHT sensor!");
  }
// lab DHT
  lab_hum = dhtLab.readHumidity();
  lab_temp = dhtLab.readTemperature();
  // print values
  Serial.print("Humidity: ");
  Serial.print(lab_hum);
  Serial.print(" %, Temp: ");
  Serial.print(lab_temp);
  Serial.println(" Celsius");
  // Check if any reads failed and exit early (to try again).
  if (isnan(lab_hum) || isnan(lab_temp)) {
    Serial.println("Failed to read from Lab DHT sensor!");
  }
}

void reportSensorValues(){
  
  // Report values
  chillerStatus.setTime(time(nullptr));
  
  chillerStatus.addField("temp_in", temp_in);
  chillerStatus.addField("pressure_in", pressure_in);
  chillerStatus.addField("pressure_out", pressure_out);
  chillerStatus.addField("pressure_diff", pressure_diff);
  chillerStatus.addField("room_temp", room_temp);
  chillerStatus.addField("room_hum", room_hum);
  chillerStatus.addField("lab_temp", lab_temp);
  chillerStatus.addField("lab_hum", lab_hum);

  chillerStatus.addField("wifi_rssi", WiFi.RSSI()); //wifi signal strength

  // Print what are we exactly writing
  Serial.print("Writing: ");
  Serial.println(client.pointToLineProtocol(chillerStatus));

  // Write point into buffer - high priority measure
  client.writePoint(chillerStatus);

  // Clear fields for next usage. Tags remain the same.
  chillerStatus.clearFields();
}

void writeToInfluxDB(){
  
  Serial.println("Flushing data into InfluxDB");
  if (!client.flushBuffer()) {
    Serial.print("InfluxDB flush failed: ");
    Serial.println(client.getLastErrorMessage());
    Serial.print("Full buffer: ");
    Serial.println(client.isBufferFull() ? "Yes" : "No");
    isDataSent = false; //fail
  }
  else{
    Serial.println("InfluxDB flush succcessful.");
    isDataSent = true; //successful
  }
}

void reconnectWifi(){

  // If no Wifi signal, try to reconnect it
  if (wifiMulti.run() != WL_CONNECTED) {
    Serial.println("Wifi connection lost");
    isWifiConnected = false;
  }
}

void displayData(){

  // note: each line fit 22 char at 1x
  oled.clear();

  // two yellow lines
  oled.set1X();
  //  oled.println("MaLab Chiller Monitor");
  oled.print("Wifi:");
  if (isWifiConnected) {
    oled.print("OK - ");
    oled.println(WiFi.SSID());
  } else{
    oled.println("FAIL");
  }
  if (isDataSent) {
    oled.println("InfluxDB: OK  ");
  } else{
    oled.println("InfluxDB: FAIL");
  }

  // blue lines
  oled.println("Temp In    Diff Press");
  
  oled.set2X();
  char strBuf[10];
  sprintf(strBuf, "%04.1f %04.1f", temp_in, pressure_diff);
  oled.println(strBuf);
  
  oled.set1X();
  oled.println("Press In   Press Out");
  
  oled.set2X();
  sprintf(strBuf, "%04.1f %04.1f", pressure_in, pressure_out);
  oled.println(strBuf);
  
  oled.set1X();

//  time_t now = time(nullptr);
//  oled.println(ctime(&now));
  
}


// ================== main loop ====================

void loop() {
  
  // Sync time for batching once per N iterations
  if (iterations++ >= 3000) {
    timeSync(TZ_INFO, NTP_SERVER1, NTP_SERVER2);
    iterations = 0;
  }

  // read sensor values from ADC
  readSensorValues();

  // read DHT values
  readDHT();

  // save into local InfluxDB Point
 reportSensorValues();

  // if no Wifi signal, try to reconnect it
  reconnectWifi();
  
  // force write to InfluxDB as single transaction
  writeToInfluxDB();

  // show on oled display
  displayData();
  
  // Wait 1s
  Serial.println("Wait 10s");
  delay(10000);
  
}
