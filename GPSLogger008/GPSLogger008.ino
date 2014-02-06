/*************************************************************************
Mitch Bodmer, Joe Higley, Brandan Nelson, Kendel Gregory, Chase Guyer
GPS Datalogging Sketch
Version 0.08
*************************************************************************/

#include <SD.h>
#include <SoftwareSerialExt.h>
#include <GPS.h>

/** Set the GPSbaudrate to the baud rate of the GPS module **/
#define GPSBaudRate 4800

/** Set the StringRate to the rate in seconds that the GPS module sends strings **/
#define StringRate 1

/** Define blink times **/
const int LONG = 2800;
const int SHORT = 800;
const int BREAK = 800;
const int ENDBREAK = 2800;

/** Define blink code for error list **/
const char LONG_CODE = 4;
const char SHORT_CODE = 3;
const char BREAK_CODE = 2;
const char ENDBREAK_CODE = 1;

/** Define ErrorCodes **/
const int NO_SD = 0b010;
const int NO_SD_LOG = 0b011;
const int NO_INIT = 0b000;
const int NO_GPS = 0b101;
const int NO_GPS_LOG = 0b100;
const int GPS_STOP = 0b110;
const int CHECKSUM_MISMATCH = 0b111;

/** Define array params **/
const int NUM_ERROR_CODES = 8;
const int NUM_ERRORS = 48;

/** Define arrays **/
char errors[NUM_ERRORS]; // Long array
int errorList[NUM_ERROR_CODES]; // Short array
long unsigned int errorStart = 0; // Fill value

/** Turn on or off USB output **/
#define USBOUT 1

/** Set the pins used **/
#define powerPin 4
#define led2Pin 5
#define chipselectPin 10
#define gpstxPin 2
#define gpsrxPin 3
#define offbuttonPin 7

/** Forward Declarations **/
void readsensor(char info, uint8_t pin);
void AddError(int error);
void RunError();
void AdvanceArray();
void PrintErrorArray();
void DeleteErrorCode();

/** Global Variables **/
uint8_t i;
File logfile;
GPS gps(gpstxPin, gpsrxPin, GPSBaudRate, StringRate, powerPin);

// Buffer must fit NMEA sentence
char buffer[100];

void setup() {
#ifdef USBOUT
    Serial.begin(9600);
    Serial.println("\r\nGPS Log");
#endif

    for (int i = 0; i < NUM_ERRORS; ++i) {
      errors[i] = 0;
    }
    for (int i = 0; i < NUM_ERROR_CODES; ++i) {
      errorList[i] = 0;
    }

    /** Set pin modes **/
    pinMode(led2Pin, OUTPUT);
    pinMode(offbuttonPin, INPUT);

    /** Chip select pin must be output for SD library to function **/
    pinMode(chipselectPin, OUTPUT);

    /** Turn on LED 1 to show system on then turn off **/
    digitalWrite(led2Pin,HIGH);
    
    
    /** Once we have a fix **/

    /** Initialize Card **/
    if (!SD.begin(chipselectPin)) {
        AddError(NO_SD);
     
#ifdef USBOUT
        Serial.println("\r\nCard init. failed!");
#endif
    }

    /** Store filename in buffer **/
    strcpy(buffer, "GPSLOG00.txt");

    /** Increment file name postfix if files arleady exist **/
    for (i = 0; i < 100; i++) {
        buffer[6] = '0' + i/10;
        buffer[7] = '0' + i%10;
        if (! SD.exists(buffer)) { 
            break;
        }
    }

    /** Create file with incremented filename **/
    logfile = SD.open(buffer, O_CREAT | O_WRITE | O_CREAT | O_EXCL);

    /** Check if file was created **/
    if(!logfile) {
        AddError(NO_SD_LOG);
#if USBOUT==1
        Serial.print("\r\nCouldnt create ");
        Serial.println(buffer);
#endif
    }

#if USBOUT==1
    Serial.print("\r\nWriting to ");
    Serial.println(buffer);
    Serial.println("\r\nReady!");
#endif
}

void loop()
{
    RunError();
#if USBOUT==1
    PrintErrorArray();
#endif
    // Get our string
    gps.getstring(buffer);

    // Check if we were able to read a string
    if(!gps.gotstring())
    {
      AddError(NO_GPS_LOG);
#if USBOUT==1
        Serial.println("\r\nNo string!");
#endif
        logfile.write("$NoStr,,,,,,,,,,,,,,*\r\n");
        logfile.flush();
    }

    /** Check for bad checksum or no fix **/
    else if(*buffer=='\0')
    {
#if USBOUT==1
        if(gps.getcsum())
        {
            if(gps.getcsum()==1)
            {

                Serial.println("\r\nBad checksum!");

            }
            if(gps.getcsum()==2)
            {

                Serial.println("\r\nNo checksum!");

            }
        }

        if(!gps.getfix())
        {
          AddError(NO_GPS);
            Serial.println("\r\nNo fix!");

        }
#endif
        logfile.write("$BadStr,,,,,,,,,,,,,,*\r\n");
        logfile.flush();
    }

    /** Log good data with fix and checksum **/
    else
    {
#if USBOUT==1
        Serial.print('\n');
        Serial.write((uint8_t *)buffer, strlen(buffer));
#endif

        // Log data
        logfile.write((uint8_t *)buffer, strlen(buffer));
        logfile.flush();
    }

    /** Check for gps fix **/
    if(!gps.getfix())
    {
      // If no fix add to error arrays
      AddError(NO_GPS);
    }

    readsensor('T', 0);
    readsensor('T', 1);
    readsensor('P', 2);
    
    /** Continue error for SD problems **/
    if (!logfile)
    {
      AddError(NO_SD_LOG);
    }   
}

/****Read Analog Sensor****
| Takes an info type and  |
| a pin number and logs   |
| a reading in the        |
| correct format to the   |
| SD card.                |
**************************/
void readsensor(char info, uint8_t pin)
{
    // Read voltage
    int reading=analogRead(pin);

#if USBOUT==1
    // Print data to serial
    Serial.write('~');
    Serial.print(pin);
    Serial.print(info);
    Serial.write(':');
    Serial.println(reading);
#endif

    // Log data
    logfile.write('~');
    logfile.print(pin);
    logfile.print(info);
    logfile.write(':');
    logfile.println(reading);
    logfile.flush();
}

/****Read Analog Sensor****
| Takes a Binary int      |
| And converts the int    |
| to a blinking error     |
|                         |
|                         |
**************************/


/****Add Error*************
| Adds codes for light    |
| to errors[]             |
**************************/

void AddError(int error)
{
  int i = 0;
  while (i < NUM_ERROR_CODES && errorList[i] != 0) 
  {
    if (errorList[i] == error)
      return;
    ++i;
  }
  errorList[i] = error;
  for (int power = 4; power >= 1; power/=2)
  {
    if (power & error)
    {
      AddErrorCode(LONG_CODE);
    }
    else
    {
      AddErrorCode(SHORT_CODE);
    }
    if (power != 1) AddErrorCode(BREAK_CODE);
  }
  AddErrorCode(ENDBREAK_CODE);
}

void AddErrorCode(char code)
{
  int i = 0;
  while (errors[i] != 0 && i < NUM_ERRORS)
  {
    ++i;
  }
  errors[i] = code;
}

void RunError()
{
  if (errors[0])
  {
    switch(errors[0])
    {
      case LONG_CODE:
        if (millis() - errorStart >= LONG)
        {
          errorStart = millis();
          digitalWrite(led2Pin, LOW);
          AdvanceArray();
        }
        else
        {
          digitalWrite(led2Pin, HIGH);
        }
      break;
      
      case SHORT_CODE:
        if (millis() - errorStart >= SHORT)
        {
          errorStart = millis();
          digitalWrite(led2Pin, LOW);
          AdvanceArray();
        }
        else
        {
          digitalWrite(led2Pin, HIGH);
        }
      break;
      
      case BREAK_CODE:
        if (millis() - errorStart >= BREAK)
        {
          errorStart = millis();
          AdvanceArray();
          digitalWrite(led2Pin, HIGH);
        }
        else
        {
          digitalWrite(led2Pin, LOW);
        }
      break;
      
      case ENDBREAK_CODE:
        if (millis() - errorStart >= ENDBREAK)
        {
          errorStart = millis();
          AdvanceArray();
          DeleteErrorCode();
          digitalWrite(led2Pin, HIGH);
        }
        else
        {
          digitalWrite(led2Pin, LOW);
        }
      break;
    }
  }
  else
  {
    errorStart = millis();
    digitalWrite(led2Pin, HIGH);
  }
}

void AdvanceArray()
{
  int i = 0;
  while (i < NUM_ERRORS - 1 && errors[i] != 0)
  {
    errors[i] = errors[i+1];
    ++i;
  }
  errors[i] = 0;
}

void PrintErrorArray()
{
  for (int i = 0; i < NUM_ERRORS; ++i)
  {
    Serial.print((char)(errors[i] + '0'));
    Serial.print(" ");
  }
  Serial.print("\n");
}

void DeleteErrorCode()
{
  int i = 0;
  while (i < NUM_ERROR_CODES - 1 && errorList[i] != 0)
  {
    errorList[i] = errorList[i+1];
    ++i;
  }
  errorList[i] = 0;
}

// poop
