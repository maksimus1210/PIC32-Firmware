////////////////////////////
//Libraries - DO NOT TOUCH//
////////////////////////////

#include "src/WSPR_encode/WSPR_encode.h"
#include "src/Si5351/Si5351.h"
#include "src/TinyGPS++/TinyGPS++.h"
#include "src/panic/panic.h"
#include "src/DogLcd/DogLcd.h"
#include "src/maidenhead/maidenhead.h"

//////////////////////////////
//Definitions - DO NOT TOUCH//
//////////////////////////////

#define GPS Serial1
#define PC Serial
#define RPI Serial0
#define SECOND 1200
#define LED 1
#define TIMEOUT 4e5
#define TIMER_ENABLED 1""15
#define NO_PRESCALER 0
#define MODE_32_BIT_TIMER 1""3
#define EXTERNAL_SOURCE 2
#define GPS_PPS 0
#define GPS_PPS_INTERRUPT 2
#define PIN_2 0
#define RPI_UART_PROD 7
#define MENU_BTN 16
#define EDIT_BTN 13

//////////////////////////////





