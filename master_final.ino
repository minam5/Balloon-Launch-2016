//#includes
#include <SPI.h>
#include <SD.h>
#include <math.h>
#include <Servo.h>

//Power and V-ref
const int V_REF = 5;
const int POWER = 5;

//Pin Assignments 
const int CHIP_SELECT = 53; //done
const int PRESSURE_SENSOR = A11; //done
const int AARON_CO2_SENSOR = A15; //done    
const int AARON_CUTDOWN_PIN = 27;  //done
const int RYAN_GRAYSCALE_SENSOR = A13; //done
const int BACTERIA_SERVO_PINS[] = {44, 45, 42, 43}; //done
const int HUMIDITY_PIN = 34; //done
const int TEMPERATURE_SENSOR1 = A6; //external temp in shade
const int TEMPERATURE_SENSOR5 = A7; //internal temp
const int T_SERIAL_PIN = 29;   //done

//Mina Pressure+Altitude Variables 
const int SAFE_ALTITUDE_ARRAY_SIZE = 30;
float altitude = 0;
float safe_altitude = 0; 
float last_few_altitudes[SAFE_ALTITUDE_ARRAY_SIZE];
int altitude_array_position = 0;

double BASE_PRESSURE = 101.89414; // in KPascal
double BASE_TEMPERATURE = 287.039; //in Kelvin

//Aaron Cutdown Variables 
const unsigned long AARON_MIN_TIME = 6300000; // Wait at least 1hr 45min
const unsigned long AARON_MAX_TIME = 10800000; // MAX = 3 hours
const double AARON_MAX_TEMP = -25; // Celsius
const float AARON_SPACE_LAUNCH_HEIGHT = 24.384; // 24.384km = 80k ft

//Mina Temperature Variables
const int TEMPERATURE_SENSOR_1_R0 = 100;
const int TEMPERATURE_SENSOR_5_R0 = 100;

double safe_temperature = 0;

const int SAFE_TEMPERATURE_ARRAY_SIZE = 30;
float last_few_temperatures[SAFE_TEMPERATURE_ARRAY_SIZE];
int temperature_array_position = 0;

//Mina and Nolan Humidity Variables
unsigned long turnStart;
int turn;
unsigned int hum;
unsigned int temp;
int bitsRead;
double * tempHum = (double *) malloc(sizeof(double));
double * tempTemp = (double *) malloc(sizeof(double));

double humTemp;

//Nik and Ben Bacteria Variables 
const unsigned long BACTERIA_OPEN_DURATION = 900000ul;
const float BACTERIA_COLLECTION_ALTITUDES[] = {2.45384, 7.3152, 12.1920, 17.0688};
const int BACTERIA_SERVO_CLOSED_POSITIONS[] = {17, 1, 9, 12};
const int BACTERIA_SERVO_OPEN_POSITIONS[] = {170, 170, 170, 170};
Servo bacteriaServos[4];
long bacteriaOpenStartTimes[] = {-1, -1, -1, -1};


//Aaron CO2 Variables
const float AARON_CO2_SLOPE = 2500;

//Nolan GPS Variables 

//Radio and SD Variables
const int MAX_DATA_LOGS = 15;
String RADIO_HEADERS[] = { "t", "P", "A", "SA", "T1", "T2", "Ti", "ST", "CD","B", "HT", "H", "C", "L", "G"};
const String EMPTY_DATA = "-";
String writeBuffer[MAX_DATA_LOGS];
String STARTER = "qNB4";
String ENDER = "j5st";
const String GPS_ESCAPE_SEQUENCE = "GPGGA";
const unsigned long ERROR_CORRECTION_FNV_PRIME = 16777619ul;
const unsigned long ERROR_CORRECTION_OFFSET_BASIS = 2166136261ul;

void setup() {
  //Bacteria Setup 
  for(int i = 0; i < 4; i++) {
    bacteriaServos[i].attach(BACTERIA_SERVO_PINS[i]);
    bacteriaServos[i].write(BACTERIA_SERVO_CLOSED_POSITIONS[i]);
  }
  
  //Chip Setup
  SD.begin(CHIP_SELECT);
  
  //Nolan Radio Setup
  Serial3.begin(4800); //radio
  digitalWrite(T_SERIAL_PIN, HIGH); //used to turn radio on/off

  //Nolan GPS Setup 
  Serial1.begin(9600); //GPS

  //Mina Pressure+Altitude Setup
  initializeLastFewAltitudesArray();
  pinMode(PRESSURE_SENSOR, INPUT);
  
  //Aaron Cutdown Setup 
  pinMode(AARON_CUTDOWN_PIN, OUTPUT);

  //Mina Temperature Setup 
  initializeLastFewTemperaturesArray();
  pinMode(TEMPERATURE_SENSOR1, INPUT);
  pinMode(TEMPERATURE_SENSOR5, INPUT);

  //Aaron CO2 Setup

  //Mina and Nolan Humidity Setup

  //Nik and Ben Bacteria Setup

  //Chip Setup End
  writeToSDCard(concatenate(RADIO_HEADERS, MAX_DATA_LOGS));
  resetWriteBuffer();
}

void loop() {
  writeToSDCardAndRadio(String(millis()), 0); 

  altitudeLoopStuff();

  temperatureLoopStuff();

  aaronCutDownLoop();

  bacteriaLoop();
  
  minaNolanHumidityLoopStuff();

  aaronCO2Loop();

  lightLoop();
  
  writeToSDCardAndRadio(getGPSData(), 14);
  
  //Flush 
  flushWriteBuffer();
}





//RADIO AND SD CARD METHODS
void flushWriteBuffer() {
  writeToSDCard(concatenate(writeBuffer, MAX_DATA_LOGS));
  for(int i = 0; i < MAX_DATA_LOGS; i++) {
    if(writeBuffer[i] != EMPTY_DATA) {
      writeToRadio(RADIO_HEADERS[i] + ":" + writeBuffer[i]);
    }
  }
  resetWriteBuffer();
}

void resetWriteBuffer() {
  for(int i = 0; i < MAX_DATA_LOGS; i++) {
    writeBuffer[i] = EMPTY_DATA;
  }
}

void writeToSDCardAndRadio(String dataString, int pos) {
  writeBuffer[pos] = dataString;
}


void writeToRadio(String s) {
  digitalWrite(T_SERIAL_PIN, HIGH);
  delay(10);
  
  //int sLen = s.length();

//  // compute the hash of s
//  unsigned long hash = ERROR_CORRECTION_OFFSET_BASIS;
//  for(int i = 0; i < sLen; i++) {
//    hash ^= ((unsigned long) s.charAt(i));
//    hash *= ERROR_CORRECTION_FNV_PRIME;
//  }
//
//  // prepend the hash onto the string
//  for(int i = 4; i > 0; i--) {
//    s = String((char) (hash & 11111111ul)) + s;
//    hash >>= 8;
//  }

  // add the starter and ender
  s = STARTER + s + ENDER; // s is fully encoded

  
  Serial3.print(s);
  delay(100);
  digitalWrite(T_SERIAL_PIN, LOW);
  delay(600);
}

void writeToSDCard(String dataString) {
  File dataFile = SD.open("datalog.txt", FILE_WRITE);

  // if the file is available, write to it:
  if (dataFile) {
    dataFile.println(dataString);
    dataFile.close();
    // print to the serial port too:
  }
}

String concatenate(String input[], int len) {
  String ans = "";
  for(int i = 0; i < len - 1; i++) {
    ans = ans + input[i] + ",";
  }
  return ans + input[len - 1];
}

//GPS METHOD
/**
 * gets latitude, longitude and altitude of GPS. Values are combined
 * and stored in a single string of length 67. 
 */
String getGPSData() {
  String GPSstr = "";
  int count = 0;
  boolean lineFound = false;
  if (Serial1.available()) {
    lineFound = true;
    char checkDollar = (char) Serial1.read();
    if (checkDollar == '$') {
      for (int i = 0 ; i < GPS_ESCAPE_SEQUENCE.length() ; i = i + 1) {
        while (!Serial1.available());
        char checkTemp = (char) Serial1.read();
        if (GPS_ESCAPE_SEQUENCE.charAt(count) == checkTemp) {
          count = count + 1;
        }
        else {
          lineFound = false;
          break;
        }
      }
    }
  }
  String GPSValCont = "";
  if (lineFound) {
    char nextVal;
    for (int j = 0 ; j < 67 ; j = j + 1) {
      delay(10);
      if (Serial1.available()) {
        nextVal = char(Serial1.read());
        if(nextVal == ',') {
          nextVal = ';';
        } else if(nextVal == '\n' || nextVal == '\r') {
          nextVal = '\\';
        }
        GPSValCont = GPSValCont + String(nextVal);
      }
    }
  }
  return GPSValCont;
}


//BACTERIA METHOD
void bacteriaLoop(){
  // checks each servo individually
  for(int i = 0; i < 4; i++) {
    if(bacteriaOpenStartTimes[i] == -1) {
      // plate has not been opened at all.
      if(safe_altitude >= BACTERIA_COLLECTION_ALTITUDES[i]) {
        // plate should now open

        // save current time
        bacteriaOpenStartTimes[i] = millis();
        // open the servo
        bacteriaServos[i].write(BACTERIA_SERVO_OPEN_POSITIONS[i]);
        // log that the plate was opened
        writeToSDCardAndRadio(String(i) + "O", 9);
      }
    } else if(bacteriaOpenStartTimes[i] > 0) {
      // plate already open
      if(bacteriaOpenStartTimes[i] + BACTERIA_OPEN_DURATION
                  <= millis()) {
        // notes that plate should now be closed forever
        bacteriaOpenStartTimes[i] = -2;
        // close the servo
        bacteriaServos[i].write(BACTERIA_SERVO_CLOSED_POSITIONS[i]);
        // log that the plate was closed
        writeToSDCardAndRadio(String(i) + "C", 9);
      }
    }
  }
}

//LIGHT METHOD
void lightLoop(){
  writeToSDCardAndRadio(String(analogRead(RYAN_GRAYSCALE_SENSOR)), 13); 
}

//CO2 METHOD
void aaronCO2Loop() {
  float count;
  float voltage;
  float sensor_reading;
  
  count = analogRead(AARON_CO2_SENSOR);
  voltage = count / 1023 * 5.0;// convert from count to raw voltage
  sensor_reading = voltage * AARON_CO2_SLOPE;
  writeToSDCardAndRadio(String(sensor_reading), 12);
}

//HUMIDITY METHODS
void minaNolanHumidityLoopStuff(){
  readHumidity(tempHum, tempTemp);
  double actualHumidity = ((*tempHum)*2.268)-4.559;
  humTemp = *tempTemp;
  writeToSDCardAndRadio(String(*tempTemp), 10);
  writeToSDCardAndRadio(String(actualHumidity), 11);
}

void readHumidity(double * humAns, double * tempAns) {
  unsigned long maxTime = micros() + 1000000ul;
  jumpToTurn(0);
  while(true) {
    unsigned long current = micros();
    if(current - maxTime < 10000ul) { // should usually overflow, giving huge number, unless is over max
      jumpToTurn(7);
      *humAns = -1;
      *tempAns = -1;
    } else if(readHumidityHelper(current)) { // done reading, convert
      *humAns = ((double) (hum << 1)) / 20; // removes first bit, which appears to be noisy
      *tempAns = ((double)(temp << 1)) / 20; // removes sign bit
      if(temp >> 15 == 1) {
        *tempAns *= -1; // add sign bit correctly
      }
      break;
    }
  }
  
}

boolean readHumidityHelper(unsigned long microTime) { // must be called faster than every 10µs
  unsigned long relTime = microTime - turnStart;
  if(turn == 0 && relTime > 8000) { // host pulls low for 1-10ms (we'll do 8+)
    jumpToTurn(1);
  } else if(turn == 1 && relTime > 20) { // host pulls high for 20-40µs (we'll do 20
    jumpToTurn(2);
  } else if(turn == 2 && digitalRead(HUMIDITY_PIN) == HIGH) { // go until sensor says HIGH
    jumpToTurn(3);
  } else if(turn == 3 && digitalRead(HUMIDITY_PIN) == LOW) { // go until sensor says LOW
    jumpToTurn(4);
  } else if(turn == 4 && digitalRead(HUMIDITY_PIN) == HIGH) { // go until sensor says high
    jumpToTurn(5);
  } else if(turn == 5 && digitalRead(HUMIDITY_PIN) == LOW) { // significant bit until sensor says low
    if(bitsRead < 15) {
      hum <<= 1;
      if(relTime > 49) { // 0's are 26-28, 1's are ~70
        hum++; // 1
      }
    }
    if(bitsRead < 32) { // don't care about last 8 bits
      temp <<= 1;
      if(relTime > 49) { // 0's are 26-28, 1's are ~70
        temp++;
      }
    }
    bitsRead++;
    if(bitsRead < 40) {
      jumpToTurn(4);
    } else {
      jumpToTurn(6);
    }
  } else if(turn == 6 && digitalRead(HUMIDITY_PIN) == HIGH) { // go until sensor says high
    jumpToTurn(7);
  } else if(turn == 7) { // let other programs run
    return true; // exit
  }
  return false; // don't exit
}

void jumpToTurn(int now) {
  turn = now;
  turnStart = micros();
  if(turn == 0) {
    pinMode(HUMIDITY_PIN, OUTPUT);
    digitalWrite(HUMIDITY_PIN, LOW);
  } else if(turn == 1) {
    digitalWrite(HUMIDITY_PIN, HIGH);
  } else if(turn == 2) {
    pinMode(HUMIDITY_PIN, INPUT);
    // should read to check sensor, but won't
  } else if(turn == 3) {
    bitsRead = 0;
  } else if(turn == 7) {
    pinMode(HUMIDITY_PIN, OUTPUT);
    digitalWrite(HUMIDITY_PIN, HIGH);
  }
}

//CUTDOWN METHOD
void aaronCutDownLoop() {
  unsigned long current_time = millis();
  if (safe_altitude < AARON_SPACE_LAUNCH_HEIGHT || current_time < AARON_MIN_TIME)
  {
    // DO NOT CUT ANYTHING
    digitalWrite(AARON_CUTDOWN_PIN, LOW);
    writeToSDCardAndRadio("0", 8);
  }
  else if (current_time > AARON_MAX_TIME)
  {
    // CUT Down if balloon reaches MAX_TIME
    digitalWrite(AARON_CUTDOWN_PIN, HIGH);
    writeToSDCardAndRadio("1", 8);
  }
  else
  {
    if (safe_temperature < AARON_MAX_TEMP)
    {
      digitalWrite(AARON_CUTDOWN_PIN, HIGH);
      writeToSDCardAndRadio("1", 8);

    }
  }
}

//TEMPERATURE METHODS
void temperatureLoopStuff() {
  calculateSensor1Temperature();   
  calculateSensor5Temperature();   
  fillTemperaturesArray(temperature_array_position, humTemp);  
  calculateSafeTemperature();  
  temperature_array_position = (temperature_array_position + 1) % SAFE_TEMPERATURE_ARRAY_SIZE;
}

void initializeLastFewTemperaturesArray(){
  for (int i = 0; i < SAFE_TEMPERATURE_ARRAY_SIZE; i ++)
  {
    last_few_temperatures[i] = 40;
  }
}

void fillTemperaturesArray(int position, int currentTemperature){
  if (position < SAFE_TEMPERATURE_ARRAY_SIZE)
  {
      last_few_temperatures[position] = currentTemperature;
  }
}

void calculateSafeTemperature(){
  long biggest = -10000000;
  long secondBiggest = -10000000;

  for (int i = 0; i < SAFE_TEMPERATURE_ARRAY_SIZE; i ++){
      if (last_few_temperatures[i] == biggest){
        secondBiggest = biggest; 
      }
      else if (last_few_temperatures[i] > biggest){
        secondBiggest = biggest; 
        biggest = last_few_temperatures[i];
      }
      else if(last_few_temperatures[i] > secondBiggest){
        secondBiggest = last_few_temperatures[i];
      }
    }
   safe_temperature = secondBiggest;
   writeToSDCardAndRadio(String(safe_temperature), 7); 
}

void calculateSensor1Temperature(){
  double arduino_reading_1 = analogRead(TEMPERATURE_SENSOR1);
  double arduino_voltage_1 = (arduino_reading_1 * V_REF)/1023;
  double temperature_measurement_1 = (-17.977 * arduino_voltage_1) + 28.902;
  writeToSDCardAndRadio(String(temperature_measurement_1), 4);
}



void calculateSensor5Temperature(){
    double arduino_reading_5 = analogRead(TEMPERATURE_SENSOR5);
    double arduino_voltage_5 = (arduino_reading_5 * V_REF) / 1023;
    double temperature_measurement_5 = (-15.49 * arduino_voltage_5) + 26.737; 
    writeToSDCardAndRadio(String(temperature_measurement_5), 6);
}

//PRESSURE AND ALTITUDE METHODS
void altitudeLoopStuff() {
  calculateAltitude();    //call the method to assign actual altitude
  fillAltitudeArray(altitude_array_position, altitude);   //drop it into the array
  calculateSafeAltitude();   //call the method to assign safe altitude 
  altitude_array_position = (altitude_array_position + 1) % SAFE_ALTITUDE_ARRAY_SIZE; //update array position
}

void initializeLastFewAltitudesArray(){
  for (int i = 0; i < SAFE_ALTITUDE_ARRAY_SIZE; i ++)
  {
    last_few_altitudes[i] = -1;
  }
}

void calculateAltitude(){
  float press_x_value = analogRead(PRESSURE_SENSOR); 
  float pressure_value = ( (2.4941 * press_x_value) - 255.32 ) * 100; 
  float pressure_in_kilo = pressure_value / 1000; 

  if (pressure_in_kilo > 23){
     altitude = (   (BASE_TEMPERATURE * (pow(pressure_in_kilo/BASE_PRESSURE, 0.190255132132) - 1 ))/ -6.5);
  }
  else{
     altitude = (      (1.73 - log(pressure_in_kilo/22.65))/0.157 );
  }
 
  writeToSDCardAndRadio(String(pressure_in_kilo), 1);
  writeToSDCardAndRadio(String(altitude), 2);
}

void calculateSafeAltitude(){
  float smallest = 10000000;
  float secondSmallest = 10000000;

    for (int i = 0; i < SAFE_ALTITUDE_ARRAY_SIZE; i ++){
      if(last_few_altitudes[i] > 0) {
        if (last_few_altitudes[i] == smallest){
          secondSmallest = smallest; 
        }
        else if (last_few_altitudes[i]< smallest){
          secondSmallest = smallest; 
          smallest = last_few_altitudes[i];
        }
        else if(last_few_altitudes[i] < secondSmallest){
          secondSmallest = last_few_altitudes[i];
        }
      }
    }
   safe_altitude = ((secondSmallest == 10000000 ) ? -1 : secondSmallest);
   writeToSDCardAndRadio(String(safe_altitude), 3);

}

void fillAltitudeArray(int position, int currentAltitude){
  if (position < SAFE_ALTITUDE_ARRAY_SIZE)
  {
      last_few_altitudes[position] = currentAltitude;
  }
}

