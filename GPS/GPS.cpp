/*************************************************************************
Mitch Bodmer
GPS Library Definitions
*************************************************************************/

#include "GPS.h"

/********************
| Private Functions |
********************/

/******Check GPS Fix*******
| Takes a char pointer to |
| the start of a GGA NMEA |
| sentence and returns    |
| an int representing     |
| it's value.             |
| 0: No Fix               |
| 1: GPS Fix              |
| 2: Differential GPS Fix |
**************************/
uint8_t GPS::cfix(char *buff)
{
    //Iterate to fix field
    for(int i=0; i<6; i++)
    {
        if(strchr(buff, ',')==NULL)
            return 0;

        buff = strchr(buff, ',')+1;
    }

    //Check if/return 1
    if (buff[0] == '1')
    {
        return 1;
    }

    //Check if/return 2
    else if (buff[0] == '2')
    {
        return 2;
    }

    // Anything else returns no fix
    else
    {
        return 0;
    }
}

/***Check NMEA Checksum****
| Takes a char pointer to |
| the start of a GGA NMEA |
| sentence and returns an |
| int representing the    |
| checksum match result.  |
| 0: Match                |
| 1: Mismatch             |
| 2: No Checksum          |
**************************/
uint8_t GPS::ccsum(char *buff)
{
    int chksum=0;
    int buffsz=strlen(buff);

    if(buffsz>5)
    {
        if(buff[buffsz-5]!= '*')
        {
            return 2;
        }

        //Get decimal checksum
        chksum = parseHex(buff[buffsz-4]) * 16;
        chksum += parseHex(buff[buffsz-3]);

        //Generate/check checksum
        for (int i=1; i < (buffsz-5); i++)
        {
            chksum ^= buff[i];
        }

        //Checksum mismatch
        if (chksum != 0)
        {
            return 1;
        }

        //Checksum match
        return 0;
    }

    // String too short to check
    return 2;
}

/****Convert HEX to DEC****
| Takes a char            |
| representing a          |
| hexidecimal digit and   |
| returns it's decimal    |
| equivalent.  Invalid    |
| characters return 0.    |
**************************/
uint8_t GPS::parseHex(char c)
{
    //Check for invalid char
    if (c < '0')
        return 0;

    //Check for 0-9
    if (c <= '9')
        return c - '0';

    //Check for invalid char
    if (c < 'A')
        return 0;

    //Check for A-F
    if (c <= 'F')
        return (c - 'A')+10;

    //Check for invalid char
    if (c > 'F')
        return 0;
}

/*******************
| Public Functions |
*******************/

/*******Constructor********
| Configures serial       |
| settings and turns      |
| power on.               |
**************************/
GPS::GPS(int rx, int tx, int serrt, int strrt, int power) : SoftwareSerialExt(rx, tx)
{
    powerpin=power;
    pinMode(powerpin, OUTPUT);
    digitalWrite(powerpin, LOW);
    begin(serrt);
    fix=0;
    sum=2;
    string=0;
    rate=strrt;
}

/*******Get Checksum*******
| Return checksum status. |
**************************/
uint8_t GPS::getcsum()
{
    return sum;
}

/**********Get Fix*********
| Return GPS fix status.  |
**************************/
uint8_t GPS::getfix()
{
    return fix;
}

/*******Got String?********
| Returns true if we got  |
| a string from serial.   |
**************************/
uint8_t GPS::gotstring()
{
    return string;
}

/*******Turn Off GPS*******
| Powers down GPS unit.   |
**************************/
void GPS::turnoff()
{
    digitalWrite(powerpin, HIGH);
}

/*******Turn On GPS********
| Powers up GPS unit.     |
**************************/
void GPS::turnon()
{
    digitalWrite(powerpin, LOW);
}

/****Read GPS Sentence*****
| Reads in a NMEA GGA     |
| sentence and stores it  |
| in the array given as   |
| an argument.            |
| No serial out, checksum |
| mismatches, and no fix  |
| retun empty strings and |
| and set flags.          |
**************************/
void GPS::getstring(char* extbuff)
{
    char c;
    int chkdly=(rate*100)+10;
    int bufferidx=0;
    int dlycnt=0;

    // Wait for a string to become avaiable
    for(dlycnt=0; (!available())&&(dlycnt<10); dlycnt++)
    {
        delay(chkdly);
    }

    string=1;

    // Read/Parse/Checksum/Output GGA sentence
    for(int i=0; i<500; i++)
    {
        if(available())
        {
            c = read();

            // Find the start of a sentence
            if (bufferidx == 0)
            {
                for(int k=0; (c!='$')&&(k<200); k++)
                {
                    c=read();
                }

                if (c!='$')
                {
                    for( ; dlycnt<9; dlycnt++)
                    {
                        delay(chkdly);
                    }
                    fix=0;
                    sum=0;
                    string=0;
                    *extbuff='\0';
                    return;
                }
            }

            //Store character
            buffer[bufferidx] = c;

            // When we have recieved an entire sentence
            if (c == '\n')
            {
                // Terminate String
                buffer[++bufferidx] = '\0';

                // Get Checksum
                sum=ccsum(buffer);

                // Check if checksum does not exist or checksum mismatch
                if ((sum == 2)||(sum==1))
                {
                    fix=0;
                    *extbuff='\0';
                    return;
                }

                // Get GPS fix
                fix=cfix(buffer);

                // If no fix
                if (!fix)
                {
                    *extbuff='\0';
                    return;
                }

                // Fill external buffer
                for(i=0; i<bufferidx; i++)
                    extbuff[i]=buffer[i];

                return;
            }

            // Increment buffer index
            bufferidx++;

            // Detect buffer overflow
            if (bufferidx == BUFFSIZE-1)
            {
                fix=0;
                sum=0;
                string=0;
                *extbuff='\0';
                return;
            }
        }

        // If we have only recived part of the string so far
        else
        {
            delay(2);
        }
    }

    // If we have no data in the buffer
    fix=0;
    sum=0;
    string=0;
    *extbuff='\0';
    return;
}
