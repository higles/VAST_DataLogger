/*************************************************************************
Mitch Bodmer, Joe Higley, Brandan Nelson, Kendall Gregory, Chase Guyer
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
char errors[NUM_ERRORS]; //long array
int errorList[NUM_ERROR_CODES]; //short array
long unsigned int errorStart = 0; //fill value

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

//Buffer must fit NMEA sentence
char buffer[100];

double PressureConversion(int volts) {
	return (((volts / 1024)*5)/0.3)*6.89475729;
}

double TemperatureConversion(int volts) {
	return ((volts * (5.0 / 1024)) - 1.375) / 0.0225;
}

void setup() {
#ifdef USBOUT
    Serial.begin(9600);
    Serial.println("\r\nGPS Log");
#endif
    
    //Initialize blink array to 0's
    for (int i = 0; i < NUM_ERRORS; ++i) {
      errors[i] = 0;
    }
    
    //Initialize error array to 0's
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
    /** Initialize Card    **/
    if (!SD.begin(chipselectPin)) {
      AddError(NO_SD);
     
#ifdef USBOUT
        Serial.println("\r\nCard init. failed!");
#endif
    }

    // Store filename in buffer
    strcpy(buffer, "GPSLOG00.txt");

    /** Increment file name postfix if files arleady exist **/
    for (i = 0; i < 100; i++) {
        buffer[6] = '0' + i/10;
        buffer[7] = '0' + i%10;
        
        if (! SD.exists(buffer)) {            
            break;
        }
    }

    // Create file with incremented filename
    logfile = SD.open(buffer, O_CREAT | O_WRITE | O_CREAT | O_EXCL);

    /** Check if file was created **/
    if(!logfile) {
        AddError(NO_SD_LOG);
#ifdef USBOUT
        Serial.print("\r\nCouldnt create ");
        Serial.println(buffer);
#endif
    }

    //if file was created
#ifdef USBOUT
    Serial.print("\r\nWriting to ");
    Serial.println(buffer);
    Serial.println("\r\nReady!");
#endif
}

void loop() {
    RunError(); //start error blink at start of loop
    
#ifdef USBOUT
    PrintErrorArray(); //only for debugging
#endif
    // Get our string
    gps.getstring(buffer);

    /** Check if we were able to read a string **/
    if(!gps.gotstring()) {
        AddError(NO_GPS_LOG);
#ifdef USBOUT
        Serial.println("\r\nNo string!");
#endif
        logfile.write("$NoStr,,,,,,,,,,,,,,*\r\n");
        logfile.flush();
    }

    /** Check for bad checksum or no fix **/
    else if(*buffer=='\0') {
#ifdef USBOUT
        if(gps.getcsum()) {
            if(gps.getcsum()==1) {
                Serial.println("\r\nBad checksum!");
            }
            
            if(gps.getcsum()==2) {
                Serial.println("\r\nNo checksum!");
            }
        }

        if(!gps.getfix()) {
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
#ifdef USBOUT
        Serial.print('\n');
        Serial.write((uint8_t *)buffer, strlen(buffer));
#endif

        /** Log data **/
        logfile.write((uint8_t *)buffer, strlen(buffer));
        logfile.flush();
    }
	
    if(!gps.getfix()) {
        AddError(NO_GPS);
    }

    ReadSensor('T', 0);
    ReadSensor('T', 1);
    ReadSensor('P', 2);
	logfile.write('\n');
    
    /** Continue error for SD problems **/
    if (!logfile) {
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
void ReadSensor(char info, uint8_t pin) {
    // Read voltage
    int reading=analogRead(pin);

#ifdef USBOUT
    /** Print data to serial **/
    Serial.write(',');
    Serial.print(result);
#endif

    /** Log data **/
	double result = 0;
	if (info == 'T') {
		result = TemperatureConversion(reading);
	}
	else if (info == 'P') {
		result = PressureConversion(reading);
	}
	else {
		result = reading;
	}
	logfile.write(',');
	logfile.write(info);
	logfile.print(pin);
    logfile.write(',');
	logfile.print(result);
	logfile.write(',');
	logfile.print(reading);
}

/****Add Error*************
| Adds error codes for    |
| light to errorList[]    |
| in a queue format       |
**************************/

void AddError(int error) {
  int i = 0;
  
  /** Check if error is already in queue **/
  while (i < NUM_ERROR_CODES && errorList[i] != 0) {
    if (errorList[i] == error)
      return;
    ++i;
  }
  
  //add the error to the queue
  errorList[i] = error;
  
  /** Convert and add error to blink codes in errors[] **/
  for (int power = 4; power >= 1; power/=2) {
    if (power & error) { //if binary digit is a 1
      AddErrorCode(LONG_CODE);
    }
    else { //if binary digit is a 0
      AddErrorCode(SHORT_CODE);
    }
    //short off between each on
    if (power != 1) AddErrorCode(BREAK_CODE);
  }
  AddErrorCode(ENDBREAK_CODE); //long off at end of blink sequence
}

/************************|
| Add blink codes 1-4 to |
| errors[] to determine  |
| if the light should be |
| on or off              |
|************************/
void AddErrorCode(char code) {
  int i = 0;
  while (errors[i] != 0 && i < NUM_ERRORS) {
    ++i;
  }
  errors[i] = code;
}

/***********************|
| Checks the life of a  |
| blink and switches to |
| next blink when it's  |
| life has ended        |
|***********************/
void RunError() {
  if (errors[0]) { // if error array has more than 0 elements
    switch(errors[0]) {
      case LONG_CODE:
      /** check if blink time is done **/
        if (millis() - errorStart >= LONG) {
          errorStart = millis();
          digitalWrite(led2Pin, LOW);
          AdvanceArray();
        }
        else {
          digitalWrite(led2Pin, HIGH);
        }
      break;
      
      case SHORT_CODE:
      /** check if blink time is done **/
        if (millis() - errorStart >= SHORT) {
          errorStart = millis();
          digitalWrite(led2Pin, LOW);
          AdvanceArray();
        }
        else {
          digitalWrite(led2Pin, HIGH);
        }
      break;
      
      case BREAK_CODE:
      /** check if blink time is done **/
        if (millis() - errorStart >= BREAK) {
          errorStart = millis();
          AdvanceArray();
          digitalWrite(led2Pin, HIGH);
        }
        else {
          digitalWrite(led2Pin, LOW);
        }
      break;
      
      case ENDBREAK_CODE:
      /** check if blink time is done **/
        if (millis() - errorStart >= ENDBREAK) {
          errorStart = millis();
          AdvanceArray();
          DeleteErrorCode();
          digitalWrite(led2Pin, HIGH);
        }
        else {
          digitalWrite(led2Pin, LOW);
        }
      break;
    }
  }
  else {
    errorStart = millis();
    digitalWrite(led2Pin, HIGH);
  }
}

/************************| 
| Advancing to the next  |
| blink code in errors[] | 
|************************/
void AdvanceArray() {
  int i = 0;
  while (i < NUM_ERRORS - 1 && errors[i] != 0) {
    errors[i] = errors[i+1];
    ++i;
  }
  errors[i] = 0;
}

/** Prints the blink code to serial for debugging **/
void PrintErrorArray() {
  for (int i = 0; i < NUM_ERRORS; ++i) {
    Serial.print((char)(errors[i] + '0'));
    Serial.print(" ");
  }
  Serial.print("\n");
}

/**********************|
| Deletes error off of |
| errorList[] after    |
| execution            |
|**********************/
void DeleteErrorCode() {
  int i = 0;
  while (i < NUM_ERROR_CODES - 1 && errorList[i] != 0) {
    errorList[i] = errorList[i+1];
    ++i;
  }
  errorList[i] = 0;
}
