#include <TM1637Display.h>
#include <SPI.h>
#include <SD.h>
#include <SoftwareSerial.h> 
#include <TinyGPS.h>
#include <TimeLib.h>

#define LOG_PERIOD 36000  //Logging period in milliseconds, recommended value 15000-60000.
#define MAX_PERIOD 60000  //Maximum logging period without modifying this sketch

unsigned long counts;     //variable for GM Tube events
float cpm;        //variable for CPM
unsigned long multiplier;  //variable for calculation CPM in this sketch
unsigned long previousMillis;  //variable for time measurement
float MSVh;
float MR;
uint8_t segto; 
bool colon;

const int offset = 3; // TZ = UTC + 3
const int CLK = 9; //Set the CLK pin connection to the display
const int DIO = 8; //Set the DIO pin connection to the display
const int chipSelect = 10;

float lat = 0,lon = 0; // create variable for latitude and longitude object  
SoftwareSerial gpsSerial(4,5);//rx,tx 
TinyGPS gps; // create gps object  

TM1637Display display(CLK, DIO);  //set up the 4-Digit Display

void tube_impulse() {       //subprocedure for capturing events from Geiger Kit
  Serial.print("+");
  counts++;
}

time_t getTimeFunction()
{
 unsigned long age;
 int Year;
 byte Month, Day, Hour, Minute, Second;
 gps.crack_datetime(&Year, &Month, &Day, &Hour, &Minute, &Second, NULL, &age);
 if (age < 1000) {
   // set the Time to the latest GPS reading if less then 0.2 seconds old
   setTime(Hour, Minute, Second, Day, Month, Year);
   adjustTime(offset * SECS_PER_HOUR);

   String timestring = String(day()) + "." + String(month()) + "." + String(year()) + " " + String(hour()) + ":" + String(minute()) + ":" + String(second());
   Serial.print("Time adjusted: ");
   Serial.println(timestring);
   return now();
 }
 else return 0;
}

void setup() {
  counts = 0;
  cpm = 0;
  MR = 0;
  colon = false;
  multiplier = MAX_PERIOD / LOG_PERIOD;      //calculating multiplier, depend on your log period
  
  Serial.begin(9600);
  while (!Serial) {
    ; // wait for serial port to connect. Needed for native USB port only
  }
  Serial.println();
  
  display.setBrightness(0x09);  //set the diplay to maximum brightness
  
  uint8_t data[] = {0b01000000, 0b01000000, 0b01000000, 0b01000000}; // "----"
  display.setSegments(data);
  delay(500);
  
  Serial.print("Initializing GPS...");
  data[0] = 0;
  data[1] = 0b00111101; 
  data[2] = 0b01110011; 
  data[3] = 0b01101101; // " GPS"
  
  gpsSerial.begin(9600); // connect gps sensor
  while (lat == 0 || lon == 0) {
    display.setSegments(data);
    if (colon) {
      segto = 0x80 | data[1]; 
      display.setSegments(&segto, 1, 1);
      colon = !colon;
    } else {
      colon = !colon;
    }
    while(gpsSerial.available()){ // check for gps data 
      if(gps.encode(gpsSerial.read()))// encode gps data 
      {  
        gps.f_get_position(&lat,&lon); // get latitude and longitude
      }
    }
    delay(500);
  }
  
  String latitude = String(lat,6); 
  String longitude = String(lon,6); 
  Serial.print("ready: ");
  Serial.println(latitude+";"+longitude); 

  Serial.print("Set time:");
  data[0] = 0b01000000;
  data[1] = 0b01000000; 
  data[2] = 0b01000000; 
  data[3] = 0b01000000; // "----"
  // setSyncProvider(getTimeFunction);
  // setSyncInterval(120); // Get time every 2 minutes
  while(timeStatus() == timeNotSet) { 
    display.setSegments(data);
    if (colon) {
      segto = 0x80 | data[1]; 
      display.setSegments(&segto, 1, 1);
      colon = !colon;
    } else {
      colon = !colon;
    }
    while(gpsSerial.available()){ // check for gps data 
      if(gps.encode(gpsSerial.read()))// encode gps data 
      {  
        getTimeFunction();  
      }
    }
    delay(500);
  }    

  Serial.println("Initializing SD card...");

  // see if the card is present and can be initialized:
  if (!SD.begin(chipSelect)) {
    Serial.println("Card failed, or not present");
    data[0] = 0;
    data[1] = 0b01101101; 
    data[2] = 0b01011110; 
    data[3] = 0; // " Sd "
    display.setSegments(data);
    // don't do anything more:
    while (SD.begin(chipSelect)) delay(500);
  }
  Serial.println("card initialized.");

  pinMode(2, INPUT);   
  attachInterrupt(digitalPinToInterrupt(2), tube_impulse, FALLING); //define external interrupts 
  pinMode(3, OUTPUT); 
  digitalWrite(3, HIGH);
  previousMillis = millis();
}

void loop() {
  unsigned long currentMillis = millis();
  Serial.print(".");
  
  display.showNumberDec(ceil(MR), true);
  if (colon) {
    segto = 0x80 | display.encodeDigit(((int)ceil(MR) / 100)%10); 
    display.setSegments(&segto, 1, 1);
    colon = !colon;
  } else {
    colon = !colon;
  }
  
  delay(500);
  if(currentMillis - previousMillis > LOG_PERIOD){
    previousMillis = currentMillis;
    cpm = counts * multiplier;
    MSVh = cpm/151;
    MR = MSVh * 100;

    Serial.print("cpm = ");
    Serial.println(cpm);
    Serial.print(MSVh);
    Serial.println(" mSv/h");
    Serial.print(MR);
    Serial.println(" mRh");

    display.showNumberDec(ceil(MR), true);

    String dataString = String(MSVh);

    while(gpsSerial.available()){ // check for gps data 
    if(gps.encode(gpsSerial.read()))// encode gps data 
      {  
        gps.f_get_position(&lat,&lon); // get latitude and longitude
      }
    }
    
    if (lat == TinyGPS::GPS_INVALID_F_ANGLE || lon == TinyGPS::GPS_INVALID_F_ANGLE) {
      counts=0;
      return;
    }
    
    String latitude = String(lat,6); 
    String longitude = String(lon,6);

    char timestring[50];

    sprintf(timestring, "%02d.%02d.%02d,%02d:%02d:%02d", day(), month(), year(), hour(), minute(), second());

    dataString = String(timestring) + "," + latitude + "," + longitude + "," + dataString;

    // open the file. note that only one file can be open at a time,
    // so you have to close this one before opening another.
    File dataFile = SD.open("datalog.txt", FILE_WRITE);
  
    // if the file is available, write to it:
    if (dataFile) {
      dataFile.println(dataString);
      dataFile.close();
      // print to the serial port too:
      Serial.println(dataString);
    }
    // if the file isn't open, pop up an error:
    else {
      Serial.println("error opening datalog.txt");
    }

    counts = 0;
  }
}

