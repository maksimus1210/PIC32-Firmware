#include "WSPR_config.h"

String callsign="A         ";
String locator="AA00aa";


int tx_percentage = 20;

char symbols[162]; //Used to store encoded WSPR symbols
char symbols2[162]; //Used to store more WSPR symbols in case of extended mode

Si5351 osc;
TinyGPSPlus gps;
DogLcd lcd(20, 21, 24, 22);
bool calibration_flag, pi_rx_flag, state_initialised=0, editing_flag=0, warning = 0, gps_enabled=1; //flags used to indicate to the main loop that an interrupt driven event has completed
uint32_t calibration_value; //Contains the number of pulses from 2.5MHz ouput of Si5351 in 80 seconds (should be 200e6) 
int substate=0;

enum menu_state{START, UNCONFIGURED, UNLOCKED, HOME, PANIC, CALLSIGN, LOCATOR, POWER, POWER_WARNING, POWER_QUESTION, TX_PERCENTAGE, IP, BAND, OTHER_BAND_WARNING, OTHER_BAND_QUESTION, DATE_FORMAT};
enum power_t{dbm0, dbm3, dbm7, dbm10, dbm13, dbm17, dbm20, dbm23, dbm27, dbm30, dbm33, dbm37, dbm40, dbm43, dbm47, dbm50, dbm53, dbm57, dbm60};
enum band_t{BAND_160, BAND_80, BAND_60, BAND_40, BAND_30, BAND_20, BAND_17, BAND_15, BAND_12, BAND_10, BAND_OTHER};
const String band_strings[] = {"160m", "80m", "60m", "40m", "30m", "20m", "17m", "15m", "12m", "10m", "Other"}; 
const int power_values[] = {0, 3, 7, 10, 13, 17, 20, 23, 27, 30, 33, 37, 40, 43, 47, 50, 53, 57, 60};
const String watt_strings[] = {"1mW", "2mW", "5mW", "10mW", "20mW", "50mW", "100mW", "200mW", "500mW", "1W", "2W", "5W", "10W", "20W", "50W", "100W", "200W", "500W", "1kW"};
enum date_t {BRITISH, AMERICAN, GLOBAL};
const String date_strings[] = {"DD/MM/YYYY", "MM/DD/YYYY", "YYYY/MM/DD"};

const char letters[] = {'A','B','C','D','E','F','G','H','I','J','K','L','M','N','O','P','Q','R','S','T','U','V','W','X','Y','Z','0','1','2','3','4','5','6','7','8','9','/',' ','A'}; //the extra A means the index can be incremented from '/', next time it searches for 'A' it will returns 0 not 37 as it loops from the starts
const String blank_line = "                ";
menu_state state = START;
power_t power = dbm23;
band_t band = BAND_20;
date_t date_format = BRITISH;
char frequency_char[9] = {'0','0','1','3','6','0','0','0',0};
uint32_t frequency = 136e3;


void setup()
{
  /*T2CON = 0x00;
  T3CON = 0x00;
  TMR2 = 0x00;
  PR2 = 0xFFFFFFFF;
  T2CKR = PIN_2;
  T2CON = (TIMER_ENABLED | NO_PRESCALER | MODE_32_BIT_TIMER | EXTERNAL_SOURCE);*/
  //mine.begin(XTAL_10pF, 25000000,GPS_ENABLED);
  //mine.set_freq(2,1,2500000.0);

 
  lcd.begin(DOG_LCD_M163);
  lcd.noCursor();
  GPS.begin(9600);
  RPI.begin(115200);
  PC.begin(9600); //Used for debugging and stuff
  
  pinMode(GPS_PPS, INPUT);
  pinMode(MENU_BTN, INPUT);
  pinMode(EDIT_BTN, INPUT);
  pinMode(LED, OUTPUT);
  pinMode(RPI_UART_PROD, OUTPUT);
  digitalWrite(LED, LOW);
  digitalWrite(RPI_UART_PROD, LOW);

  callsign.reserve(10);
  locator.reserve(6);

  attachInterrupt(GPS_PPS_INTERRUPT, check_frequency, RISING);
}

void lcd_write(int row, int col, String data)
{
  lcd.setCursor(row, col);
  lcd.print(data);
}

int letters_find(char x) //returns the index of char x in the array "letters"
{
  for (int i=0; i<39; i++)
  {
    if(letters[i]==x) return i;
  }
  return -1;

  
}
void state_clean() //clears anything that needs to be cleaned up before changing state
{
    lcd.clear();
    lcd.noBlink();
    lcd.noCursor();
    warning = 0;
    state_initialised=0;
    editing_flag=0;
    substate=0;
}

bool callsign_check()
{
  static bool zero_found;
  for (int i =0; i<10; i++)
  {
    if(callsign[i]==' ')//Once we find the first empty character
    {
      for (int j = i; j<10; j++)
      {
        if(callsign[j] != ' ') return 1; //valid data has been found after the blank
      }
      return 0; //All good
    }
  }
  return 0;
}

void check_frequency()
{
	static int counter = 0;
	counter++;
	if(counter==80)
	{
		calibration_value = TMR2;
		TMR2 = 0;
		calibration_flag = 1; //Notify main loop
		counter=0;
	}
}

bool menu_pressed()
{
	return !digitalRead(MENU_BTN);
}

bool edit_pressed()
{
	return !digitalRead(EDIT_BTN);
}

void request_from_PI(String command, String &return_string, int max_length)
{
	while(RPI.available()) RPI.read();
	return_string="";
	RPI.print(command+";\n"); //Send the request to the PI
	digitalWrite(RPI_UART_PROD, HIGH);
	while(!RPI.available()) {static int counter = 0; counter++; if (counter==TIMEOUT) panic(lcd, "no response",18);}
	if(RPI.read() != command[0]) panic(lcd, "Unexpected character received", 19);
	while(!RPI.available()) {static int counter = 0; counter++; if (counter==TIMEOUT) panic(lcd, "1st",18);}
	if(RPI.read() != command[1]) panic(lcd, "Unexpected character received", 19);
	while(!RPI.available()) {static int counter = 0; counter++; if (counter==TIMEOUT) panic(lcd, "2nd",18);}
	char x;
	for(int i = 0; i < max_length; i++)
	{
		x = RPI.read();
		if(x != ';') return_string += x;
		else break;
		while(!RPI.available()) {static int counter = 0; counter++; if (counter==TIMEOUT) panic(lcd, "3rd",18);}
	}
	/*while(!RPI.available()) {static int counter = 0; counter++; if (counter==TIMEOUT) panic(lcd, "Pi not responding",18);}
	x = RPI.read();
	while(x != '\n')
	{
		while(!RPI.available()) {static int counter = 0; counter++; if (counter==TIMEOUT) panic(lcd, "Pi not responding",18);}
		x = RPI.read();
	}
	digitalWrite(RPI_UART_PROD, LOW);
	*/
}

void loop()
{
  switch (state) //going to attempt to implement as a state machine, sure M0IKY will have some complaints to make 
  {
    case START:
    {
      if(0); //TODO add EEPROM read function to see if valid configuration data has been found  
      else
      {
        state_clean();
        state = UNCONFIGURED;
		goto end;
      }
      break;
    } 
    case UNCONFIGURED:
    {
      if(!state_initialised)
      {
        lcd_write(0,1, "Not configured");
        lcd_write(1,1, "Use webpage or");
        lcd_write(2,2, "Press \"Menu\"");
        while(menu_pressed()) delay(50); //Wait for any previous button press to clear (written here so the display updates before debouncing)
        state_initialised=1;
      }
      
      if(menu_pressed())
      {
        state_clean();
        state = IP;
		goto end;
      }
      break;
    }
    case CALLSIGN:
    {
      if(!state_initialised) //This is the first time in this state so draw on the LCD and wait for debounce
      {
        lcd_write(0,4, "Callsign");
        lcd_write(1,0, callsign);
        while(menu_pressed()) delay(50);
        state_initialised=1;
      }

      if(menu_pressed())
      {
        switch(editing_flag)
        {
          case 0: //Not editing callsign so move to next menu screen
          {
            if(!warning)
            {
              state_clean();
              state = LOCATOR;
			  goto end;
              
            }
            else
            {
              for (int i =0; i<3; i++)
              {
                lcd_write(2,0,blank_line);
                delay(400);
                lcd_write(2,0,"Data after blank");
                delay(400);
              }
            }
			break;
          }
          case 1: //Editing callsign so move cursor to next character
          { 
            if(++substate == 10)
            {
              substate = 0;
              editing_flag = 0; // Have swept over whole callsign so assume editing is done
              lcd.noCursor();
              lcd.noBlink();
              break;
            }
            lcd.setCursor(1,substate); //Callsign starts at column 3
            break;
          }
        }; 
        while(menu_pressed()) delay(50);  
      }

      if(edit_pressed())
      {
        switch(editing_flag)
        {
          case 0: //Start editing
          {
            lcd.setCursor(1,0);
            lcd.cursor();
            lcd.blink();
            editing_flag=1;
            break;
          }
          case 1: //Change current character (indexed by substate)
          {
            int x = letters_find(callsign[substate]); //x is the index of the current character in array "letters"
            if(x!=-1) callsign[substate] = letters[x+1]; //Overflow has been dealt with by adding the first character in letters to the end, when searched for next time, it will return the first instance of it at index 0
            else panic(lcd, "Invalid character found in callsign", 9);
            lcd_write(1, 0, callsign);
            if(callsign_check())
            {
              lcd_write(2,0,"Data after blank");
              warning = 1;
            }
            else
            {
              warning = 0;
              lcd_write(2,0,blank_line);
              
            }
            lcd.setCursor(1,substate);
            
            break;
          } 
        };
        while(edit_pressed())delay(50);
      }
      break; 
    }//end of CALLSIGN
    case LOCATOR:
    {
     if(!state_initialised) //This is the first time in this state so draw on the LCD and wait for debounce
      {
        lcd_write(0,4, "Locator");
        if(!gps_enabled) lcd_write(1,0, locator);
        else lcd_write(1,0, "Set by GPS");
        if(editing_flag) lcd_write(2,0,blank_line);
		else lcd_write(2,1, "GPS: Hold Edit");
        while(menu_pressed()) delay(50);
        while(edit_pressed())delay(50);
        state_initialised=1;
		break;
      }

      if(menu_pressed())
      {
        switch(editing_flag)
        {
          case 0: //Not editing locator so move to next menu screen
          {
            state_clean();
            state= POWER;
			goto end;
            break;
          }
          case 1: //Editing locator so move cursor to next character
          { 
            if(++substate == 6)
            {
              editing_flag = 0; // Have swept over whole locator so assume editing is done
              substate = 0;
              lcd.noCursor();
              lcd.noBlink();
              break;
            }
            lcd.setCursor(1,substate); 
            break;
          }
        }; 
        while(menu_pressed()) delay(50);  
      }

      if(edit_pressed())
      { 
        int counter =0;
        while(edit_pressed())
        {    
          if(++counter>(3*SECOND))
          {
            gps_enabled = !gps_enabled;
            state_clean();
            counter=0;
            editing_flag=0;
			if(editing_flag) lcd_write(2,0,blank_line);
			else lcd_write(2,1, "GPS: Hold Edit");
            goto end;
          }
        }
        
        if(!gps_enabled)
        {
          switch(editing_flag)
          {
            case 0: //Start editing
            {
              lcd.setCursor(1,0);
              lcd.cursor();
              lcd.blink();
              editing_flag=1;
              break;
            }
            case 1: //Change current character (indexed by substate)
            {
              switch (substate)
              {
                case 0: ;//Allow fall thourgh as same behaviour if editing the first two characters
                case 1: if(++locator[substate] == 'S') locator[substate] = 'A'; break;
                case 2: ;//Fallthrough
                case 3: if(++locator[substate] == ':') locator[substate] = '0'; break; //':' is next in ASCII after '9'
                case 4: ;//Fallthrough
                case 5: if(++locator[substate] =='y') locator[substate] = 'a'; break;
              };
              lcd_write(1, 0, locator);
              lcd.setCursor(1,substate);
              break;
            } 
          };
        }
		while(edit_pressed())delay(50);
      }
      break; 
    }//end of LOCATOR
    case POWER:
    {
      if(!state_initialised) //This is the first time in this state so draw on the LCD and wait for debounce
      {
        lcd_write(0,5, "Power");
        lcd_write(1,0, watt_strings[power]);
        lcd_write(1, 13-((String)power).length(), ((String)power+"dBm"));
        while(menu_pressed()) delay(50);
        state_initialised=1;
      }
      if(menu_pressed())
      {
		  state_clean();
          state = TX_PERCENTAGE;
		  goto end;       
      }
	  
      if(edit_pressed())
      {
		state_clean();
        state = POWER_WARNING;
		goto end;     
      }
      break;
    }//end of POWER
	case POWER_WARNING:
	{
	  if(!state_initialised) //This is the first time in this state so draw on the LCD and wait for debounce
      {
        lcd_write(0,0,"Changes reported");
        lcd_write(1,1, "PWR not actual");
        lcd_write(2,0, "PWR Press \"Menu\"");
        while(menu_pressed()) delay(50);
		while(edit_pressed()) delay(50);
        state_initialised=1;
      }
    
      if (menu_pressed())
      {
        state_clean();
        state = POWER_QUESTION;
		goto end;
      }
	}//end of POWER_WARNING
    case POWER_QUESTION:
    {
      if(!state_initialised) //This is the first time in this state so draw on the LCD and wait for debounce
      {
        lcd_write(0,5, "Power");
        lcd_write(1,0, watt_strings[power]);
        lcd_write(1, 13-((String)power).length(), ((String)power+"dBm"));
        while(menu_pressed()) delay(50);
        state_initialised=1;
      }
    
      if (menu_pressed())
      {
        state_clean();
        state= TX_PERCENTAGE;
		goto end;
      }
      if (edit_pressed())
      {
        if(power != dbm60) power = (power_t)((int)power+1);
        else power=dbm0;
        lcd_write(1,0,blank_line);
        lcd_write(1,0, watt_strings[power]);
        lcd_write(1, 13-((String)power).length(), ((String)power+"dBm"));
        while(edit_pressed())delay(50);
      }
      break;
    }//end of POWER_QUESTION
    case TX_PERCENTAGE:
	{
		
	  if(!state_initialised) //This is the first time in this state so draw on the LCD and wait for debounce
	  {
		lcd_write(0,1, "TX Percentage");
		lcd_write(1,0, ((String)tx_percentage+"%"));
		while(menu_pressed()) delay(50);
		while(edit_pressed())delay(50);
		state_initialised=1;
	  }
	  if(menu_pressed())
	  {
		state_clean();
		state = BAND;  
		goto end;	
	  }
	  if(edit_pressed())
	  {
		tx_percentage +=10;
		if(tx_percentage>100) tx_percentage=0;
		lcd_write(1,0,blank_line);
		lcd_write(1,0, ((String)tx_percentage+"%"));
		while(edit_pressed())delay(50);
	  }
	break;
	}//end of TX_PERCENTAGE	
	case IP:
	{
		if(!state_initialised) //This is the first time in this state so draw on the LCD and wait for debounce
		  {
			String data_received;
			data_received.reserve(16);
			lcd_write(0,0, "IP and Hostname");
			request_from_PI("IP", data_received,16);
			lcd_write(1,0, data_received);
			request_from_PI("HO", data_received,16);
			lcd_write(2,0,data_received);
			while(menu_pressed()) delay(50);
			while(edit_pressed())delay(50);
			state_initialised=1;
		  }
		  if(menu_pressed())
		  {
			state_clean();
			state = CALLSIGN;
		  }
		  goto end;
	}//end of IP
	case BAND:
    {
      if(!state_initialised) //This is the first time in this state so draw on the LCD and wait for debounce
      {
        lcd_write(0,6, "Band");
        lcd_write(1,0, band_strings[band]);
        while(menu_pressed()) delay(50);
        state_initialised=1;
      }
      if(menu_pressed())
      {
        state_clean();
        if(band == BAND_OTHER)
        {
          state = OTHER_BAND_WARNING;
        }
        else
        {
          state = DATE_FORMAT;
        }
		goto end;     
      }
      if(edit_pressed())
      {
        if(band != BAND_OTHER) band = (band_t)((int)band+1);
        else band=BAND_160;
        lcd_write(1,0,blank_line);
        lcd_write(1,0, band_strings[band]);
        while(edit_pressed())delay(50);
      }
      break;
    }//end of POWER	
	case OTHER_BAND_WARNING:
    {
      if(!state_initialised)
      {
        lcd_write(0,4, "WARNING!");
        lcd_write(1,0, "NO OUTPUT FILTER");
        lcd_write(2,2, "Press \"Menu\"");
        while(menu_pressed()) delay(50); //Wait for any previous button press to clear (written here so the display updates before debouncing)
        state_initialised=1;
      }
      
      if(menu_pressed())
      {
        state_clean();
        state = OTHER_BAND_QUESTION;
		goto end;
      }
      break;
    }	
	case OTHER_BAND_QUESTION:
	{
	  if(!state_initialised) //This is the first time in this state so draw on the LCD and wait for debounce
      {
        lcd_write(0,1, "Enter VFO Freq");
        lcd_write(1,0, frequency_char);
        while(menu_pressed()) delay(50);
        state_initialised=1;
      }

      if(menu_pressed())
      {
        switch(editing_flag)
        {
          case 0: //Not editing frequency so move to next menu screen
          {
           
			state_clean();
			frequency = 0;
			for (int i = 0; i<8; i++)
			{
				frequency *= 10;
				frequency += frequency_char[i];
			}
			state = DATE_FORMAT;
            goto end;
          }
          case 1: //Editing frequency so move cursor to next character
          { 
            if(++substate == 8)
            {
              substate = 0;
              editing_flag = 0; // Have swept over whole callsign so assume editing is done
              lcd.noCursor();
              lcd.noBlink();
              break;
            }
            lcd.setCursor(1,substate); //Callsign starts at column 3
            break;
          }
        }; 
        while(menu_pressed()) delay(50);  
      }

      if(edit_pressed())
      {
        switch(editing_flag)
        {
          case 0: //Start editing
          {
            lcd.setCursor(1,0);
            lcd.cursor();
            lcd.blink();
            editing_flag=1;
            break;
          }
          case 1: //Change current character (indexed by substate)
          {
			if(substate == 0)
			{
				if(++frequency_char[0] == '3') frequency_char[substate] = '0'; //Limiy maximum frequency to 29.999999MHz
			}
			else
			{
				if(++frequency_char[substate] == ':') frequency_char[substate] = '0';	
			}
			lcd_write(1,0, frequency_char);
			lcd.setCursor(1,substate);
            break;
          } 
        };
        while(edit_pressed())delay(50);
      }
      break; 
    }//end of OTHER_BAND_QUESTION
	case DATE_FORMAT:
    {
      if(!state_initialised) //This is the first time in this state so draw on the LCD and wait for debounce
      {
        lcd_write(0,2, "Date Format");
        lcd_write(1,0, date_strings[date_format]);
        while(menu_pressed()) delay(50);
        state_initialised=1;
      }
      if(menu_pressed())
      {
		  state_clean();
          if(gps_enabled) state = UNLOCKED;
		  else state = HOME;
		  goto end;       
      }
	  
      if(edit_pressed())
      {
		if(date_format != GLOBAL) date_format = (date_t)((int)date_format+1);
        else date_format = BRITISH;
        lcd_write(1,0, date_strings[date_format]);
        while(edit_pressed())delay(50);   
      }
      break;
    }//end of DATE_FORMAT	
	case UNLOCKED:
    {
      if(!state_initialised)
      {
        lcd_write(0,0, "Waiting for GPS");
        lcd_write(1,2, "Lock. Press");
		lcd_write(2,1, "\"Menu\" to skip");
        while(menu_pressed()) delay(50); //Wait for any previous button press to clear (written here so the display updates before debouncing)
        state_initialised=1;
      }
      
      if(digitalRead(GPS_PPS))
      {
        state_clean();
        state = HOME;
		goto end;
      }
	  
	  if(menu_pressed())
	  {
		  state_clean();
		  state = LOCATOR;
		  goto end;
	  }
      break;
    }	
	case HOME:
	{
	  static String new_locator;
	  if(!state_initialised) //This is the first time in this state so draw on the LCD and wait for debounce
      {
        lcd_write(0,0, callsign);
        lcd_write(0,10, locator);
        while(menu_pressed()) delay(50);
		while(edit_pressed()) delay(50);
		new_locator.reserve(6);
		uint8_t gps_symbol[7] = {14,27,17,27,31,14,4};
		lcd.createChar(0, gps_symbol);
        state_initialised=1;
      }
	  
	  if(gps_enabled)
	  {
		  static uint32_t gps_watchdog = 0;
		  if(++gps_watchdog == 10*SECOND)
		  {
			  state_clean();
			  state = UNLOCKED;
			  goto end;
		  }
		  static bool gps_flag=0;
		  if (digitalRead(GPS_PPS))
		  {
			  if(!gps_flag) 
			  {
				  lcd.setCursor(2,15);
				  lcd.print('\0');
				  gps_flag = 1;
			  }
			  gps_watchdog = 0;
		  }
		  else 
		  {
			  if(gps_flag)
			  {
				  lcd_write(2,15," ");
				  gps_flag = 0;
			  }
		  }
		  while(GPS.available())
		  {
			  gps.encode(GPS.read());
		  }
		  maidenhead(gps, new_locator);
		  if(locator != new_locator)
		  {
			  locator = new_locator;
			  lcd_write(0,10, locator);
		  }
		  static int old_time = 0;
		  if(old_time != gps.time.value());
		  {
			  lcd.setCursor(2,9);
			  lcd.print(gps.time.hour());
			  lcd.print(':');
			  lcd.print(gps.time.minute());
		  }
		  static int old_date = 0;
		  if(old_date != gps.date.value());
		  {
			  lcd.setCursor(2,0);
			  switch (date_format)
			  {
				  case BRITISH:
				  {
					  lcd.print(gps.date.day());
					  lcd.print('/');
					  lcd.print(gps.date.month());
					  lcd.print('/');
					  lcd.print(gps.date.year());
					  break;
				  }
				  case AMERICAN:
				  {
					  lcd.print(gps.date.month());
					  lcd.print('/');
					  lcd.print(gps.date.day());
					  lcd.print('/');
					  lcd.print(gps.date.year());
					  break;
				  }
				  case GLOBAL:
				  {
					  lcd.print(gps.date.year());
					  lcd.print('/');
					  lcd.print(gps.date.month());
					  lcd.print('/');
					  lcd.print(gps.date.day());
					  break;
				  }
			  };	  
		  }//end of updating date  
	  }//end of GPS stuff
	  
	}//end of HOME
  } //end of state machine
end:;
} //end of loop





