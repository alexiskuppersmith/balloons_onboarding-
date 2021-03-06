#include <Adafruit_BMP280.h>
#include <IridiumSBD.h>
#include <SD.h>
#include <String>
#include <SD_t3.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BNO055.h>
#include <SD_t3.h>
#include <Wire.h>
#include <utility/imumaths.h>
#include <TinyGPS++.h>
#include <SPI.h>
#include "Adafruit_MAX31855.h"
#include <Wire.h>
#include <SPI.h>


//Log
File flightLog;


#define BMP_SCK 13
#define BMP_MISO 12
#define BMP_MOSI 11 
#define BMP_CS 10
#define fadePin 21
#define fadePin2 22

//Adafruit_BMP280 bmp; // I2C commented out, using SPI.
Adafruit_BMP280 bmp(BMP_CS); // hardware SPI
//Adafruit_BMP280 bmp(BMP_CS, BMP_MOSI, BMP_MISO,  BMP_SCK);


// Example creating a thermocouple instance with software SPI on any three
// digital IO pins.
#define MAXDO   6
#define MAXCS   8
#define MAXCLK  9
#define IridiumSerial Serial1 //Changed to Serial Port 1
#define DIAGNOSTICS false // Change this to see diagnostics
void setupGPS();

// Declare the IridiumSBD object
IridiumSBD modem(IridiumSerial); //passing IridiumSerial as object
int heaterControlPin = 23;
// initialize the Thermocouple
Adafruit_MAX31855 thermocouple(MAXCLK, MAXCS, MAXDO);

// Example creating a thermocouple instance with hardware SPI
// on a given CS pin.
//#define MAXCS   10
//Adafruit_MAX31855 thermocouple(MAXCS);
uint8_t buffer[200] = 
{ 1, 1, 2, 3, 5, 8, 13, 21, 34, 55, 89 };


//orientation sensor
Adafruit_BNO055 bno = Adafruit_BNO055(55);


//=====================GPS=====================
// A sample NMEA stream.
const char *gpsStream =
  "$GPRMC,045103.000,A,3014.1984,N,09749.2872,W,0.67,161.46,030913,,,A*7C\r\n"
  "$GPGGA,045104.000,3014.1985,N,09749.2873,W,1,09,1.2,211.6,M,-22.5,M,,0000*62\r\n"
  "$GPRMC,045200.000,A,3014.3820,N,09748.9514,W,36.88,65.02,030913,,,A*77\r\n"
  "$GPGGA,045201.000,3014.3864,N,09748.9411,W,1,10,1.2,200.8,M,-22.5,M,,0000*6C\r\n"
  "$GPRMC,045251.000,A,3014.4275,N,09749.0626,W,0.51,217.94,030913,,,A*7D\r\n"
  "$GPGGA,045252.000,3014.4273,N,09749.0628,W,1,09,1.3,206.9,M,-22.5,M,,0000*6F\r\n";

// The TinyGPS++ object
TinyGPSPlus gps;

byte gps_set_success = 0; 

void setup(void) 
{
 
  pinMode(heaterControlPin, OUTPUT);
  pinMode(fadePin, OUTPUT);
  
  Serial.begin(115200); //for rockblock, can get more bits per second than 9600
  while (!Serial); //waiting for serial to turn on
  
  if (!bmp.begin()) {  
    Serial.println("Could not find a valid BMP280 sensor, check wiring!");
    //while (1);
  }
  
  //Initialize BNO Orientation sensor
  if(!bno.begin())
  {
    Serial.print("Ooops, no BNO055 detected ... Check your wiring or I2C ADDR!");
    //while(1);
  }
  delay(1000); 
  bno.setExtCrystalUse(true);

  //Initialize GPS
  setupGPS();
  
  //Initialize rockBLOCK
  int signalQuality = -1; //error thing
  // Start the serial port connected to the satellite modem
  IridiumSerial.begin(19200); //start the rockblock modem using a baud rate of 19200

  // Begin satellite modem operation
  Serial.println("Starting modem..."); //if it fails to start, we do some print stuff 
   if (modem.begin() != ISBD_SUCCESS)
  {
    Serial.println("Couldn't begin modem operations.");
    exit(0);
  } 
  Serial.println("After modem.begin()");
  modem.getSignalQuality(signalQuality);
  Serial.println("After getSignalQuality"); 

  Serial.print("On a scale of 0 to 5, signal quality is currently ");
  Serial.print(signalQuality);
  Serial.println(".");
  pinMode(22, OUTPUT);
}
static bool messageSent = false; 


void loop() 
{
  flightLog = SD.open("datalog.txt", FILE_WRITE);
   
  String logString;
  Serial.println(logString); 
  gps.encode(*gpsStream++);
  
  double altitude = getAltitude();
  logString += getTempAndPressure() + ", " +  externalTemperature() + ", " + "Altitude: " + altitude + ", " + "Orientation: " + getOrientationString() + ", " + "GPS " + getLatLong();

  //TODO: log the logString to SD card
  
  //cut egg
  if(altitude >= 4334){
    digitalWrite(22, HIGH); 
    
    }
  
  int err;
  
  // Read/Write the first time or if there are any remaining messages
  if (!messageSent || modem.getWaitingMessageCount() > 0)
  {
    size_t bufferSize = sizeof(buffer);

    // First time through send+receive; subsequent loops receive only
      err = modem.sendReceiveSBDText(logString.c_str(), buffer, bufferSize);
      
    if (err != ISBD_SUCCESS)
    {
      Serial.print("sendReceiveSBD* failed: error ");
      Serial.println(err);
    }
    else // success!
    {
      messageSent = true;
      Serial.print("Inbound buffer size is ");
      Serial.println(bufferSize);
      for (int i=0; i<bufferSize; ++i)
      {
        Serial.print(buffer[i]);
        if (isprint(buffer[i]))
        {
          Serial.print("(");
          Serial.write(buffer[i]);
          Serial.print(")");
        }
        Serial.print(" ");
      }
      Serial.println();
      Serial.print("Messages remaining to be retrieved: ");
      Serial.println(modem.getWaitingMessageCount());
    }
  } 
     if(flightLog) {
    flightLog.println(logString);
    flightLog.close();
  } 
  delay(5000);
}

//returns a string in the form "temp, pressure"
String getTempAndPressure(){
    manageHeaters(bmp.readTemperature());
  return "Temperature: " + String(bmp.readTemperature()) + " *C, " + "Pressure: "+String(bmp.readPressure()) + " Pa";
}

//returns altitude as a double
double getAltitude(){
  return bmp.readAltitude(1013.25);
}



//=============BNO 055======================

//Returns a string in the form "X, Y, Z"
String getOrientationString() {
  imu::Vector<3> euler = bno.getVector(Adafruit_BNO055::VECTOR_EULER);
  return String(euler.x()) + ", " + euler.y() + ", " + euler.z();
}

//=============GPS======================

//sets up the GPS, brah
void displayInfo()
{
  Serial.print(F("Location: ")); 
  if (gps.location.isValid())
  {
    Serial.print(gps.location.lat(), 6);
    Serial.print(F(","));
    Serial.print(gps.location.lng(), 6);
  }
  else
  {
    Serial.print(F("INVALID"));
  }

  Serial.print(F("  Date/Time: "));
  if (gps.date.isValid())
  {
    Serial.print(gps.date.month());
    Serial.print(F("/"));
    Serial.print(gps.date.day());
    Serial.print(F("/"));
    Serial.print(gps.date.year());
  }
  else
  {
    Serial.print(F("INVALID"));
  }

  Serial.print(F(" "));
  if (gps.time.isValid())
  {
    if (gps.time.hour() < 10) Serial.print(F("0"));
    Serial.print(gps.time.hour());
    Serial.print(F(":"));
    if (gps.time.minute() < 10) Serial.print(F("0"));
    Serial.print(gps.time.minute());
    Serial.print(F(":"));
    if (gps.time.second() < 10) Serial.print(F("0"));
    Serial.print(gps.time.second());
    Serial.print(F("."));
    if (gps.time.centisecond() < 10) Serial.print(F("0"));
    Serial.print(gps.time.centisecond());
  }
  else
  {
    Serial.print(F("INVALID"));
  }

  Serial.println();
}
void setupGPS()
{
  Serial.begin(9600);
  Serial3.begin(9600); 
  //This is how we set flight mode 
  Serial.println("Setting uBlox nav mode: ");
  uint8_t setNav[] = {
    0xB5, 0x62, 0x06, 0x24, 0x24, 0x00, 0xFF, 0xFF, 0x06, 0x03, 0x00, 0x00, 0x00, 0x00, 0x10, 0x27, 0x00, 0x00,
    0x05, 0x00, 0xFA, 0x00, 0xFA, 0x00, 0x64, 0x00, 0x2C, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x16, 0xDC };

    sendUBX(setNav, 44);

  Serial.println(TinyGPSPlus::libraryVersion());
  Serial.println();
  
  while (*gpsStream)
    if (gps.encode(*gpsStream++))
      displayInfo();

  Serial.println();
  Serial.println(F("Done."));
}

//Returns a string in the form lat, long
String getLatLong()
{
  String location;
 if (gps.location.isValid())
  {
    location += "Location: " + String(gps.location.lat()) + ", " + String(gps.location.lng());
    return location;
  }
  else
  {
    return "INVALID LOCATION";
  }
}

void sendUBX(uint8_t* MSG, uint8_t len) {
  for(int i = 0; i < len; i++) {
    Serial1.write(MSG[i]);
    Serial.print(MSG[i], HEX);
  }
  Serial1.println();
}



String externalTemperature() 
{
    thermocouple.readInternal(); 
   String externalTemperatureString = "External Temperature = " + String(thermocouple.readCelsius()) +"C";
   double c = thermocouple.readCelsius();
   if (isnan(c)) {
     Serial.println("Something wrong with thermocouple!");
   } else {
     Serial.print("C = "); 
     Serial.println(c);
   }
   //Serial.print("F = ");
   //Serial.println(thermocouple.readFarenheit());
  return externalTemperatureString; 
   delay(1000);
  
}

void manageHeaters(double currentTemp) {
  if (currentTemp < 0) {
    double p = min(1, max(0, currentTemp / -10));
    Serial.print("p=");
    Serial.print(p);
    Serial.println();
    analogWrite(heaterControlPin, 255. * p);
  }
}




#if DIAGNOSTICS
void ISBDConsoleCallback(IridiumSBD *device, char c)
{
  Serial.write(c);
}

void ISBDDiagsCallback(IridiumSBD *device, char c)
{
  Serial.write(c);
}
#endif
