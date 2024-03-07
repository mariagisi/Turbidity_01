/*
    Code for deploying turbidity sensor in the field - 
    measure turbidity (ADS), temperature, water level US (need to add)
    send data by LoRa 
    average data for 60 seconds and end data every 10min (need to add)
*/

const char *appEui = "0000000000000000";
// const char *appKey = "6AF58E40ECC61AA4DEEC2767824D29C7"; //1-Chaudanne
// const char *appKey = "0F5FF5AC56FFB3407E7E7A16FE3BEA9C"; //2
// const char *appKey = "A0EC96B73771354328E4EA0D81A2B456"; //3-Mercier
// const char *appKey = "605364872D0E6FDB4B3B407CC6E059B3"; //4-Yzeron Francheville
const char *appKey = "6AF58E40ECC61AA4DEEC2767824D29C7"; //5-Ratier
// const char *appKey = "20826B274B35B01CFFDDA662E321FA5B"; //6
// const char *appKey = "3D630283230D09CE298F0D5AA9BA5CF9"; //7
// const char *appKey = "EB78A77A718946416E2ADB464E9B9E33"; //8-test




#include <Adafruit_ADS1X15.h>
#include <MKRWAN.h>             //Depending on the version of the MKR used, include <MKRWAN.h> or <MKRWAN_v2.h>
#include <ArduinoLowPower.h>
#include <Wire.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <RTClib.h>
#include <SPI.h>
#include <SD.h>
#include <RunningMedian.h>
#include <RunningAverage.h>
#include "ds3231.h"
#include "RTClib.h"
#include <CayenneLPP.h>
Adafruit_ADS1115 ads;  

LoRaModem modem;
RTC_DS3231 rtc;
DateTime now;
File dataFile;
CayenneLPP lpp(51);

unsigned long previousMillis = 0;   // Précédent temps de mesure
int timeStep = 1000;                // Pas de temps entre chaque mesure (ms)
int delayTime = 200;                // Temps de basculement de la diode (ms)
int pinSD = 4;                      // Pin utilisé pour la carte SD

bool SDdata = true; 

int sensorSupply = 3;               // Alimentation de la sonde modifiée
int sensorOutput = 0;              // Signal de sortie sonde modifiée

int pinBattery = A0;                // Tension de la batterie
int sensorIndex = 0;                // Numéro de la mesure en cours

uint8_t sleep_period = 10;  // the sleep interval in minutes between 2 consecutive alarms

const int tempPin = 1;        // temperature sensor yellow wire
const int cdeTransistor = 7;  // DS3231 SQW pin to set the alarm

unsigned int minutes;
unsigned int secondes;

int nb_mesures = 30;  /*---------------nombre de mesures par série---------------*/

// Tables des valeurs obtenues sur une minute
RunningMedian high_samples = RunningMedian(nb_mesures);
RunningMedian low_samples = RunningMedian(nb_mesures);
RunningMedian diff_samples = RunningMedian(nb_mesures);
RunningAverage diff_for_stDev = RunningAverage(nb_mesures);
RunningMedian temp_samples = RunningMedian(nb_mesures);

#define ONE_WIRE_BUS tempPin              // Pin utilisé pour la température
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
int16_t adc0H, adc0L;
float ValuesHigh2;
float ValuesLow2;
float Difference;
float tempC = 0;

void set_next_alarm(void) //From rtc_ds3231_alarm.ino
{
  struct ts t;
  unsigned char wakeup_min;

  DS3231_get(&t);

  // calculate the minute when the next alarm will be triggered
  wakeup_min = (t.min / sleep_period + 1) * sleep_period;
  if (wakeup_min > 59) 
  {
    wakeup_min -= 60;
  }
  // flags define what calendar component to be checked against the current time in order
  // to trigger the alarm
  // A2M2 (minutes) (0 to enable, 1 to disable)
  // A2M3 (hour)    (0 to enable, 1 to disable)
  // A2M4 (day)     (0 to enable, 1 to disable)
  // DY/DT          (dayofweek == 1/dayofmonth == 0)
  uint8_t flags[4] = { 0, 1, 1, 1 };

  DS3231_set_a2(wakeup_min, 0, 0, flags);                           // set Alarm2. only the minute is set since we ignore the hour and day component

  DS3231_set_creg(DS3231_CONTROL_INTCN | DS3231_CONTROL_A2IE);      // activate Alarm2
}

//*******************************************************SETUP*************************************************
void setup() {
  
  /* Définition des E/S */
  Serial.begin(9600);
  Wire.begin();
  rtc.begin();
  sensors.begin();
  SD.begin(pinSD);
 
  pinMode(cdeTransistor, OUTPUT);
  digitalWrite(cdeTransistor, HIGH);                // Lipo battery powers the system
  pinMode(LED_BUILTIN, OUTPUT);                               // built-in LED on MKR 1310
    digitalWrite(LED_BUILTIN, HIGH);
    delay(250);
    digitalWrite(LED_BUILTIN, LOW);
    delay(250);
    digitalWrite(LED_BUILTIN, HIGH);
    delay(250);
    digitalWrite(LED_BUILTIN, LOW);
  
  pinMode(tempPin, INPUT_PULLUP);                   
  
  pinMode(pinBattery, INPUT);
  pinMode(sensorOutput, INPUT);
  pinMode(sensorSupply, OUTPUT);
  
  DS3231_init(DS3231_CONTROL_INTCN);
  DS3231_clear_a1f();                               // clear alarm1
  DS3231_clear_a2f();                               // clear alarm2

  /* ADJUST RTC TIME */
  now = rtc.now();
  // rtc.adjust(DateTime(F(__DATE__), F(__TIME__))); Serial.println("RTC Adjusted"); 
  Serial.print(now.year(), DEC); Serial.print('/'); Serial.print(now.month(), DEC); Serial.print('/'); Serial.print(now.day(), DEC); Serial.print(' '); Serial.print(now.hour(), DEC); Serial.print(':'); Serial.print(now.minute(), DEC); Serial.print(':'); Serial.println(now.second(), DEC);
  analogReadResolution(12);  //4096 numerical steps to emulate analogic resolution
  delay(1000);

  /* Initialisation du fichier d'enregistrement */
  Serial.print("Initializing SD card...");
  delay(2000);
  if (!SD.begin(pinSD))
  {
    Serial.println(" Card failed, or not present");
    for(int i=0; i<12; i++) {                                     // Bloque le programme
      digitalWrite(LED_BUILTIN, HIGH); delay(200);
      digitalWrite(LED_BUILTIN, LOW); delay(300);

      SDdata = false;
    }
  }
  else {
    Serial.println(" OK, card detected");
      digitalWrite(LED_BUILTIN, HIGH);
      delay(3000);
      digitalWrite(LED_BUILTIN, LOW);
  }
  
  if (SD.exists("Turb.txt")) {
      digitalWrite(LED_BUILTIN, HIGH);
      delay(50);
      digitalWrite(LED_BUILTIN, LOW);
      delay(50);
      digitalWrite(LED_BUILTIN, HIGH);
      delay(50);
  }
  else {
    File dataFile = SD.open("Turb.txt", FILE_WRITE);
    dataFile.println("dd/mm/yyyy hh:mm:ss ; LED ON (mV) ; LED OFF (mV) ; Turb Med (mV) ; Turb Moy (mV) ; Turb Min (mV) ; Turb Max (mV) ; Q25 (mV) ; Q75 (mV) ; Variance(µV²) ; Écart-Type (mV) ; Temperature(°C) ; Battery(V)");
    dataFile.close();

    Serial.println();
    Serial.println("SD file created with : "); 
    Serial.println("dd/mm/yyyy hh:mm:ss ; LED ON (mV) ; LED OFF (mV) ; Turb Med (mV) ; Turb Moy (mV) ; Turb Min (mV) ; Turb Max (mV) ; Q25 (mV) ; Q75 (mV) ; Variance(µV²) ; Écart-Type (mV) ; Temperature(°C) ; Battery(V)");
    Serial.println();
  }
  delay(3000);

  Serial.println("Connecting to TTN... ");
  
  if (!modem.begin(EU868)) 
  {
    Serial.println("Failed to start module");
    while (1) {}
  };
  Serial.print("Your module version is: ");
  Serial.println(modem.version());
  Serial.print("Your device EUI is: ");
  Serial.println(modem.deviceEUI());
  
  int connected = modem.joinOTAA(appEui, appKey);
  if (!connected) 
  {
    Serial.println("Something went wrong; are you indoor? Move near a window and retry"); 
    while (1) {break;}
  };
  
  ads.begin();
  
  /*Fin du setup*/
  digitalWrite(LED_BUILTIN, HIGH);
  delay(500);
  digitalWrite(LED_BUILTIN, LOW);
}

//*******************************************************LOOP**************************************************
void loop() {
  if (sensorIndex < nb_mesures && millis() - previousMillis >= timeStep) {
    previousMillis = millis();

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

    /* Calcul de la différence et enregistrement dans la table */
    Difference = ValuesHigh2 - ValuesLow2;

    /*Conversion en 10^(-4)Volt avant d'ajouter dans la table des samples pour avoir une meilleure précision à la fin*/
    diff_samples.add(Difference*10000);
    diff_for_stDev.add(Difference*10000);
    high_samples.add(ValuesHigh2*10000);
    low_samples.add(ValuesLow2*10000);

    /* Obtention de la température*/
    sensors.requestTemperatures();
    tempC = sensors.getTempCByIndex(0);
    temp_samples.add(tempC);

    sensorIndex += 1;
    Serial.println();
    Serial.print("Index : "); Serial.println(sensorIndex);
    Serial.print("Température : "); Serial.print(tempC); Serial.println(" °C");
    Serial.print("LED ON : "); Serial.print(ValuesHigh2); Serial.println(" V");  
    Serial.print("LED OFF : "); Serial.print(ValuesLow2); Serial.println(" V"); 
    Serial.print("Différence : "); Serial.print(Difference); Serial.println(" V");
  }

  if (sensorIndex >= nb_mesures) {
    Serial.println("Fin de l'acquisition de la turbidité");

    /*Conversion de l'horodatage pour l'enregistrer*/
    long dateHeure = now.unixtime();
    byte Adate = dateHeure / (256*256*256);
    byte Bdate = (dateHeure - Adate * (256*256*256)) / (256*256);
    byte Cdate = (dateHeure - Adate * (256*256*256) - Bdate*(256*256)) / (256);
    byte Ddate = (dateHeure - Adate * (256*256*256) - Bdate*(256*256)) % (256);
    float dateA = Adate; Serial.print(dateA); Serial.print(" ");
    float dateB = Bdate; Serial.print(dateB); Serial.print(" ");
    float dateC = Cdate; Serial.print(dateC); Serial.print(" ");
    float dateD = Ddate; Serial.println(dateD);

    /* Obtention de la valeur médiane, moyenne, minimale et maximale sur une minute */
    Serial.println("Statistique turbidité : Médiane ; Moyenne ; Min ; Max ; Q25 ; Q75"); // ajouter l'écart-type puis coeff variation : sigma/moyenne
    float diffMedian = diff_samples.getMedian()/10; // export to ODE
    float diffAverage = diff_samples.getAverage()/10;
    float diffMin = diff_samples.getLowest()/10;
    float diffMax = diff_samples.getHighest()/10;
    float quantile25 = diff_samples.getQuantile(0.25)/10;
    float quantile75 = diff_samples.getQuantile(0.75)/10;

    /*Conversion en mV*/
    float highMedian = high_samples.getMedian()/10;
    float lowMedian = low_samples.getMedian()/10;

    /* Calculate the variance */
    float sum = 0;  // Initialize a variable to store the sum of squared differences
    for (int indice = 0; indice < nb_mesures; indice++) {
      float x = diff_samples.getElement(indice)/10;
      sum += x*x;
    }
      float AverageSquaredX = sum / nb_mesures;
      float variance = AverageSquaredX - diffAverage*diffAverage ;
      // float var = variance*10;
      float ecartType = sqrt(variance);
      float std_dev = diff_for_stDev.getStandardDeviation()/10; //test
    Serial.print(variance); Serial.println(" µV²");
    Serial.print(ecartType); Serial.println(" mV");
    Serial.print(std_dev*std_dev); Serial.println(" µV²");
    Serial.print(std_dev); Serial.println(" mV");

    /* Temperature médiane */
    float temperature = temp_samples.getMedian();
    Serial.print(temperature); Serial.println(" °C");

    /* Obtention de la tension de la batterie */
    uint16_t bat = analogRead(pinBattery);
    Serial.print(bat); Serial.println(" (raw battery tension value)");
    
    float batteryValue = bat * 2 * (3.3/4095.0);
    Serial.print(batteryValue); Serial.println(" V");

    digitalWrite(LED_BUILTIN, HIGH);
    delay (50);
    digitalWrite(LED_BUILTIN, LOW);
    delay (50);
    digitalWrite(LED_BUILTIN, HIGH);
    delay (50);  
    digitalWrite(LED_BUILTIN, LOW);
    delay (50);
    digitalWrite(LED_BUILTIN, HIGH);
    delay (50); 
    digitalWrite(LED_BUILTIN, LOW);
    delay(750);

    Serial.println("dd/mm/yyyy hh:mm:ss | ON (mV) ; OFF (mV) | Turb Med (mV) ; Moy (mV) ; Min (mV) ; Max (mV) ; Q25 (mV) ; Q75 (mV) | Variance(µV²) ; Écart-Type (mV) | Temperature(°C)");
  
    Serial.print(now.day(), DEC); 
    Serial.print('/');
    Serial.print(now.month(), DEC);
    Serial.print('/');
    Serial.print(now.year(), DEC);
    Serial.print(" ");
    Serial.print(now.hour(), DEC);
    Serial.print(':');
    Serial.print(now.minute(), DEC);
    Serial.print(':');                                                                                  
    Serial.print(now.second(), DEC);   
    Serial.print("  |  ");
    Serial.print(highMedian); Serial.print("  ;  ");
    Serial.print(lowMedian); Serial.print("  |  ");
    Serial.print(diffMedian); Serial.print("  ;  ");
    Serial.print(diffAverage); Serial.print("  ;  ");
    Serial.print(diffMin); Serial.print("  ;  ");
    Serial.print(diffMax); Serial.print("  ;  ");
    Serial.print(quantile25); Serial.print("  ;  ");
    Serial.print(quantile75); Serial.print("  |  ");
    Serial.print(std_dev*std_dev, 3); Serial.print("  ;  ");
    Serial.print(std_dev, 3); Serial.print("  |  ");
    Serial.println(temperature, 2);
  
    /* Enregistrement des résultats sur la carte SD */
    if (SDdata){
      dataFile = SD.open("Turb.txt", FILE_WRITE);
      dataFile.print(now.day(), DEC);                          // Ecriture dans le fichier de la carte SD de la date et de l'heure, à partir du RTC                                                                       
      dataFile.print('/');
      dataFile.print(now.month(), DEC);
      dataFile.print('/');
      dataFile.print(now.year(), DEC);
      dataFile.print(" ");
      dataFile.print(now.hour(), DEC);
      dataFile.print(':');
      dataFile.print(now.minute(), DEC);
      dataFile.print(':');                                                                                  
      dataFile.print(now.second(), DEC);                      // Les secondes ne sont pas représentatives car la mesure se fait sur 30 sec + temps de SETUP plus délais de transmission LoRa
      dataFile.print(" ; ");
      dataFile.print(highMedian); dataFile.print(" ; ");
      dataFile.print(lowMedian); dataFile.print(" ; ");
      dataFile.print(diffMedian); dataFile.print(" ; ");
      dataFile.print(diffAverage); dataFile.print(" ; ");
      dataFile.print(diffMin); dataFile.print(" ; ");
      dataFile.print(diffMax); dataFile.print(" ; ");
      dataFile.print(quantile25); dataFile.print(" ; ");
      dataFile.print(quantile75); dataFile.print(" ; ");
      dataFile.print(std_dev*std_dev, 3); dataFile.print(" ; ");
      dataFile.print(std_dev, 3); dataFile.print(" ; ");
      dataFile.print(temperature, 3);dataFile.print(" ; ");
      dataFile.println(batteryValue, 3);

      dataFile.close();
    }

/*Envoi des données en LoRa/code Cayenne vers TTN*/
  int err;
  lpp.reset();
  // lpp.addTemperature(0, dateHeure);
  lpp.addTemperature(1, temperature*100);          //à diviser par 100 sur googlesheet
  
  lpp.addTemperature(2, highMedian);
  lpp.addTemperature(3, lowMedian);

  lpp.addTemperature(4, diffMedian); 
  // lpp.addTemperature(5, diffAverage);
  // lpp.addTemperature(6, diffMin);
  // lpp.addTemperature(7, diffMax);
  // lpp.addTemperature(8, quantile25);
  // lpp.addTemperature(9, quantile75);

  // lpp.addTemperature(10, std_dev*std_dev*1000);          //à diviser par 1000 sur googlesheet
  lpp.addTemperature(11, std_dev*100);          //à diviser par 100 sur googlesheet
  
  lpp.addTemperature(12, bat);          //faire *2*(3.3/4095.0) sur googlesheet
  // lpp.addTemperature(13, dateA);
  // lpp.addTemperature(14, dateB);
  // lpp.addTemperature(15, dateC);
  // lpp.addTemperature(16, dateD);

  modem.beginPacket();
  modem.write(lpp.getBuffer(), lpp.getSize());
  err = modem.endPacket(true);
  if (err > 0)
  {
    Serial.println("Message sent correctly!");
      digitalWrite(LED_BUILTIN, HIGH);
      delay (200);
      digitalWrite(LED_BUILTIN, LOW);
  }
  else 
  {
    Serial.println("Error sending message :(");
    Serial.println("(you may send a limited amount of messages per minute, depending on the signal strength");
    Serial.println("it may vary from 1 message every couple of seconds to 1 message every minute)");
      digitalWrite(LED_BUILTIN, HIGH);
      delay (500);  
      digitalWrite(LED_BUILTIN, LOW);
      delay (500);
      digitalWrite(LED_BUILTIN, HIGH);
      delay (500); 
      digitalWrite(LED_BUILTIN, LOW);
  }
 // delay(1000);
  if (!modem.available()) 
  {
    Serial.println("No downlink message received at this time.");
  }
  else {
    String rcv;
    rcv.reserve(64);
    while (modem.available()) {
      rcv += (char)modem.read();
    }
    Serial.print("Received: " + rcv + " - ");
    for (unsigned int i = 0; i < rcv.length(); i++) {
      Serial.print(rcv[i] >> 4, HEX);
      Serial.print(rcv[i] & 0xF, HEX);
      Serial.print(" ");
    }
    Serial.println();
  }

    set_next_alarm();
    delay(1000);
    Serial.println("Fin");
      digitalWrite(LED_BUILTIN, HIGH);
      delay(5000);
      digitalWrite(LED_BUILTIN, LOW);
    digitalWrite(cdeTransistor, LOW);

  while (1) {
    digitalWrite(LED_BUILTIN, LOW);
    delay(2000);
    digitalWrite(cdeTransistor, LOW);  // POUR MISE EN SOMMEIL DE LA CARTE MKR
    delay(2000);
    digitalWrite(cdeTransistor, HIGH);
    delay(1000);
    DS3231_init(DS3231_CONTROL_INTCN);
    DS3231_clear_a1f();
    DS3231_clear_a2f();
    delay(200);
    set_next_alarm();
    delay(800);
    digitalWrite(cdeTransistor, LOW);
  }

  }
}
