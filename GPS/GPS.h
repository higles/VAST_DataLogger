/*************************************************************************
Mitch Bodmer
GPS Library Header
*************************************************************************/

#ifndef GPS_h
#define GPS_h
#include "Arduino.h"
#include "../SoftwareSerialExt/SoftwareSerialExt.h"
#define BUFFSIZE 100

class GPS : private SoftwareSerialExt
{
public:

    // Functions
    GPS(int tx, int rx, int serrt, int strrt, int power);
    uint8_t getcsum();
    uint8_t getfix();
    uint8_t gotstring();
    void turnoff();
    void turnon();
    void getstring(char* extbuff);

private:

    // Variables
    char buffer[BUFFSIZE];
    uint8_t i;
    uint8_t powerpin;
    uint8_t fix;
    uint8_t sum;
    uint8_t string;
    uint8_t rate;

    // Functions
    uint8_t cfix(char *buff);
    uint8_t ccsum(char *buff);
    uint8_t parseHex(char c);
};

#endif
