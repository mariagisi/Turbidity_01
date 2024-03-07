#include <MKRWAN.h>
#include <Wire.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <RTClib.h>
#include <SPI.h>
#include <SD.h>
#include <RunningMedian.h>
#include "ds3231.h"
#include "RTClib.h"
#include <Adafruit_ADS1X15.h>
Adafruit_ADS1115 ads;


RTC_DS3231 rtc;
DateTime now;
File dataFile;

unsigned long previousMillis = 0;   // Précédent temps de mesure
int timeStep = 1000;                // Pas de temps entre chaque mesure (ms)
int delayTime = 200;                // Temps de basculement de la diode (ms)
int pinSD = 4;                      // Pin utilisé pour la carte SD
int sensorSupply = 3;               // Alimentation de la sonde modifiée
int sensorOutput = 0;              // Signal de sortie sonde modifiée

int pinBattery = A0;                // Tension de la batterie
int sensorIndex = 0;                // Numéro de la mesure en cours

#define ONE_WIRE_BUS 1             // Pin utilisé pour la température
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

int16_t adc0H, adc0L;
float ValuesHigh2;
float ValuesLow2;
float Difference;
 String userInput; // Define a String variable to store user input


void setup() {
  Serial.begin(9600);
      Wire.begin();
  pinMode(sensorOutput, INPUT);
  pinMode(pinBattery, INPUT);
  pinMode(sensorSupply, OUTPUT);
   pinMode(5, OUTPUT);
 digitalWrite(5, LOW);

  rtc.begin();
  sensors.begin();
  analogReadResolution(12);
  /* Initialisation du fichier d'enregistrement */

  if (!SD.begin(pinSD)){
     digitalWrite(6, HIGH);
     delay(200);
    }
    delay(200);

      SD.begin(pinSD);

   if (SD.exists("DATA.txt")) {
    File dataFile = SD.open("DATA.txt", FILE_WRITE);
    dataFile.close();
  }
  else {
    File dataFile = SD.open("DATA.txt", FILE_WRITE);
    dataFile.println("NTU;V;Temperature");
    dataFile.close();
  }
delay(1000);

    Serial.println("hi :)");


    ads.begin();
  // Prompt the user for input


      delay(5000);
  Serial.println("Please enter the turbidity you are measuring (in NTU):");
  
  // Wait for user input
  while (Serial.available() == 0) {
    // do nothing until input is received
  }

  // Read the user input from the Serial Monitor
  userInput = Serial.readStringUntil('\n');
  
  // Print the received input back to the Serial Monitor
  Serial.print("You entered: ");
  Serial.print(userInput);
  Serial.println("NTU");

  Serial.println("NTU;Turb(V); Temperature");
}

void loop() {

   //rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
   now = rtc.now();

    /*Mesure de la turbidité */
    digitalWrite(LED_BUILTIN, HIGH);
    digitalWrite(sensorSupply, HIGH);
    delay(delayTime);
    adc0H = ads.readADC_SingleEnded(sensorOutput);
    ValuesHigh2 = ads.computeVolts(adc0H);
    
    digitalWrite(LED_BUILTIN, LOW);
    digitalWrite(sensorSupply, LOW);
    delay(delayTime);
    adc0L = ads.readADC_SingleEnded(sensorOutput);
    ValuesLow2 = ads.computeVolts(adc0L);

        /* Calcul de la différence */
    Difference = ValuesHigh2 - ValuesLow2;

  
    long dateHeure = now.unixtime();
    byte Adate = dateHeure / (256*256*256);
    byte Bdate = (dateHeure - Adate * (256*256*256)) / (256*256);
    byte Cdate = (dateHeure - Adate * (256*256*256) - Bdate*(256*256)) / (256);
    byte Ddate = (dateHeure - Adate * (256*256*256) - Bdate*(256*256)) % (256);


    /* Obtention de la température */
    sensors.requestTemperatures();
    float tempC = sensors.getTempCByIndex(0);

sensorIndex += 1;
Serial.println();
Serial.print("Number of measurements: "); Serial.println(sensorIndex);
Serial.print(userInput);
Serial.print(" ; ");
Serial.print(Difference);
Serial.print(" ; ");
Serial.println(tempC);
    
    /* Enregistrement des résultats sur la carte SD */
    dataFile = SD.open("DATA.txt", FILE_WRITE);
    dataFile.print(userInput); dataFile.print(";");
    dataFile.print(sensorIndex); dataFile.print(";");
    dataFile.print(Difference, 3); dataFile.print(";");
    dataFile.println(tempC, 2);;
    dataFile.close();

   delay(600);

  }
  
