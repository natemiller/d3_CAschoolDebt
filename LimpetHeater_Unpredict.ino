  
  #include <Wire.h>
  #include "TembooAccount.h"      //second tab containing password information
  #include <Temboo.h>             //library that allows YUN to work with GoogleDrive
  #include <Bridge.h>             //necessary library connecting Arduino chip to Linux chip
  #include <Process.h>            //library that can calculate and process data (from server)
  #include <FileIO.h>             //library for read/write to SD card on YUN (different from UNO)
  
  //#include <LiquidCrystal.h> //From https://github.com/adafruit/LiquidCrystal//make sure to use the LiquidCrystal library from
                            //adafruit if you are using the i2C LCD backpack
  #include <SPI.h>  // Required for RTClib to compile properly
  #include <RTClib.h> // From https://github.com/mizraith/RTClib
  #include <RTC_DS1307.h>  // From https://github.com/mizraith/RTClib   (note: not the Chronodot)
  //#include <PID_v1.h> //From https://github.com/br3ttb/Arduino-PID-Library
  #include <Timer.h> // From https://github.com/JChristensen/Timer
  #include <OneWire.h> // From  http://www.pjrc.com/teensy/td_libs_OneWire.html
  #include <DallasTemperature.h> //From  http://milesburton.com/Main_Page?title=Dallas_Temperature_Control_Library
  
  
  #define Heater_LED 5          //pin connected to LED showing heater is on
  #define lowTide_LED 4         //pin connected to LED showing valve is open
  #define Heater_Relay 9        //pin connected to heater relay
  #define lowTideRelay 10       //pin connected to valve relay
  #define temperature_sensor_bus 6  // pin the temeperature sensors is connected to
  
  #define TempProgSize 13        //defined size of the unpredictable temp array
  //---------------------------------------------------------
  // Real Time Clock setup
  RTC_DS1307 RTC;                 //allow reference to clock with "RTC" (note: not the Chronodot)
  //---------------------------------------------------------
  
  //---------------------------------------------------------
  //Specify 1-Wire connections
  //---------------------------------------------------------
  OneWire oneWire(temperature_sensor_bus);
  DallasTemperature sensors(&oneWire);
  DeviceAddress Temp01 = {0x28, 0x38, 0xC5, 0x2F, 0x05, 0x00, 0x00, 0x7D}; //address for specific sensor
  //DeviceAddress deviceAddress
  //---------------------------------------------------------
  
  //---------------------------------------------------------
  //Specify timer
  //---------------------------------------------------------
  Timer t;                  //make the variable "t" represent a timer
  
  //---------------------------------------------------------
  
  unsigned long datetime;    //variable to hold date and time data
  
  int startDay;              //variable to specify numerical day exp. started
  int today;                 //variable to hold today's numerical day
  int previous_month_index;  //variable to hold the index (starting at Jan = 0, Feb = 1) of the previous month
  int elapsedDays;           //variable to hold the total elapsed days in experiment
  
  boolean tideValveState;    //variable to (0,1) to hold state of the valve (1 = open, 0 = closed)
  //---------------------------------------------------------
  //PID Setup (proportional-integral-derivative) controller
  //---------------------------------------------------------
  //Define variables
   // double Setpoint, Input, Output;
  
  //  //Specify the links and initial tuning parameters
   // PID myPID(&Input, &Output, &Setpoint,300,10,0, DIRECT);
  //
  //  //Set window size
   // int WindowSize = 5000;
   // unsigned long windowStartTime;
  //---------------------------------------------------------
  
  
  //---------------------------------------------------------
  // Tide Time/Temperature Variables
  //---------------------------------------------------------
  int lowTideHR_1 = 9; //first low tide starts at this hour
  int lowTideMIN_1 = 00; // ...and this minute
  
  int lowTideHR_2 = 21; //second low tide starts at this hour
  int lowTideMIN_2 = 00; // ...and this minute
  
  int duration_HR = 6;  // low tide duration length (hrs)
  int duration_MIN = 00; //...and minutes. (ex. low tide is 6hrs and 25min long)
  
  int duration = (duration_HR * 60) + (duration_MIN % 60); //tide duration in minutes
  
  int Start_1 = (lowTideHR_1 * 60) + (lowTideMIN_1 % 60);  //start time of tide 1 in min. since midnight
  
  int endTime_1 = (Start_1 + duration) % (24 * 60);        //end time of tide 1 in min. since midnight
  
  int Start_2 = (lowTideHR_2 * 60) + (lowTideMIN_2 % 60);  //start time of tide 2 in min. since midnight
  
  int endTime_2 = (Start_2 + duration) % (24 * 60);        //end time of tide 2 in min. since midnight
  
  
  byte TempProg[] = {13, 32, 31, 21, 19, 21, 23, 23, 26, 17, 29, 28, 29, 27}; //max temps for unpredictable tide
  float temp = 13;       //initiate temp variable
  float Tset = 13;       //initial set temperature
  float Tmin = 13;       //base temperature during high tide
  float Tmax = TempProg[0];       //maximum ramp temperature
  float rate = 0;        //initiate rate variable
  float timeDelay = 60000; // time between increases in heat ramp temperature in ms.
  const long tembooDelay = 300000; //time between updates to Google Spreadsheet (in ms)
  unsigned long previousMillis = 0; 
  
  boolean newDay;          //variable for keeping track if tide 2 into the following day
  
  float PulseWM;
  float deltaT;
  //---------------------------------------------------------
  
  //---------------------------------------------------------
  // LCD Variables
  //---------------------------------------------------------
  //LiquidCrystal lcd(0);    //setup lcd
  //---------------------------------------------------------
  
  //*********************************************************
  //SETUP LOOP
  //*********************************************************
  void setup()
  {
    Wire.begin();                         //required for communication
    RTC.begin();                         //Start clock
    Bridge.begin();                      //Start Bridge/Linux connection
    FileSystem.begin();                  //Start SD card saving functions
  
    DateTime now = RTC.now();            //specify "now" as the current datetime
    startDay = now.day();                //specify that the numerical day when sketch starts in the start day
    //Serial.begin(115200);              //Start serial monitor
    //lcd.begin(16, 2);                    //Start LCD (dimensions 16 x 2)
    sensors.begin();                     //Initialize 1-Wire sensors
    sensors.setResolution(Temp01, 12);   //set temperature sensor resolution (12bit = 0.0625 deg.C)
  
    pinMode(lowTideRelay, OUTPUT);       //set LED and Relays to output
    pinMode(lowTide_LED, OUTPUT);
    pinMode(Heater_LED, OUTPUT);
    pinMode(Heater_Relay, OUTPUT);
  
    digitalWrite(lowTideRelay, LOW);     //have the tide valve start closed
    digitalWrite(lowTide_LED, LOW);      //have the tide LED start off
  
    //---------------------------------------------------------
    // PID initialization
    //---------------------------------------------------------
    //    Setpoint = Tset;               //specify setpoint
    //    windowStartTime = millis();
    //   myPID.SetOutputLimits(0,WindowSize);
      // Turn the PID on
    //    myPID.SetMode(AUTOMATIC);
    //---------------------------------------------------------
  
  
    //---------------------------------------------------------
    //Setup Timer loops
    //---------------------------------------------------------
   // t.every(200, PIDloop);                  //update PID every 200ms
    //t.every(3000, printTimeTemp);          //print to Serial Monitor every 3000ms
    t.every(timeDelay, tempRamp);           //increase Tset every "timeDelay"
    //t.every(1000, lcdPrint);                 //update the lcd (running the lcdPrint function (below) every 1000 ms
    t.every(200, heater);                 //run heater function every 200ms
    t.every(10000, sd_card);               //run the sd_card function every 10000 ms, 10 s.
    //---------------------------------------------------------
  
  } //end of setup
  
  
  //*********************************************************
  //MAIN LOOP
  //*********************************************************
  
  void loop()
  {
    unsigned long currentMillis = millis();                        //initiate a timer counting ms from start of sketch
   
          getTempFunc();                                        //get temperatures
          tideValve();                                          //specify if tide valve is open of closed
          newTmax();                                            //check  
            
      if (currentMillis - previousMillis > tembooDelay) {                      //every tembooDelay
        previousMillis = currentMillis;                                  //reset timer...
        runAppendRow(datetime, temp, Tset, deltaT, PulseWM, Tmax);     //and send data to GoogleSpreadheet
      }
      
      t.update();                                               //update timer                                                  
     
  }//end Main
  
  
  
  
  
  //---------------------------------------------------------
  //getTempFunc function. This function takes a digital reading
  //and returns a temperature value.
  //---------------------------------------------------------
  float getTempFunc()
  {
    sensors.requestTemperatures();                                      //request temperature from sensor
    temp = sensors.getTempC(Temp01);                                    //get temperature from sensor with Temp01 address
    return temp;                                                        //make temperature available
  }
  
  
  
  //---------------------------------------------------------
  //tempRamp function. Takes time as an input and specifies
  //whether it is time for a heat ramp, and if so, specifies
  //the ramping rate.
  //---------------------------------------------------------
  void tempRamp()
  {
  
    // Get current time, store in object 'now'
    DateTime now = RTC.now();
  
    //create a current time variable (in min from midnight)
    int time_min = now.hour() * 60 + now.minute();
  
    //specify rate of temp increase based on the temperature
    //range of the ramp, the duration of the low tide (in ms)
    //and how often this temperature increase will be initiated
    float lowTideDurationMS = duration * 60000;
    float rate = (Tmax - Tmin) / (lowTideDurationMS / timeDelay);
  
  
    //series of if else statements to apply heating during the low tide period
    if (time_min >= Start_1 && time_min < endTime_1 && Tset < Tmax)
    {
      Tset = Tset + rate;
  
    }
    else if (time_min < Start_1 || time_min >= endTime_1)
    {
      Tset = Tmin;
  
    }
  
  }
  //---------------------------------------------------------
  
  
  //---------------------------------------------------------
  //tideValve function. Takes a time input and specifies when
  //a valve opens to initiate low tide (water leaves tank)
  ////---------------------------------------------------------
  void tideValve()
  {
    // Get current time, store in object 'now'
    DateTime now = RTC.now();
  
  
    if (float(Start_2 + duration) / 1440 > 1)                    // simple test to see if the second low tide will
    {                                                            //extend across midnight and into a new day
      newDay = TRUE;
    } else {
      newDay = FALSE;
    };
  
    //create a current time variable (in min from midnight)
    int time_min = now.hour() * 60 + now.minute();
  
    if (time_min >= Start_1 && time_min < endTime_1 || time_min >= Start_2 && time_min < endTime_2 || newDay == TRUE && time_min > Start_2 || newDay == TRUE && time_min < endTime_2) //if its low tide
    {
      digitalWrite(lowTideRelay, HIGH);                           //turn on Relay1 to power valve (open valve)
      digitalWrite(lowTide_LED, HIGH);                            // turn on indicator LED
    }
    else  {                                                       //if its high tide
      digitalWrite(lowTideRelay, LOW);                            //reverse Relay1 (close valve)
      digitalWrite(lowTide_LED, LOW);                            //turn off indicator LED
    }
  
    tideValveState = digitalRead(lowTideRelay);                  //determine whether the valve is open (1) or closed (0)
  }
  //---------------------------------------------------------
  
  
  //---------------------------------------------------------
  //heater function. Specifies how much power to provide to the heater
  //to achieve a desired ramp rate (based on Tset)
  //---------------------------------------------------------
  
  //The function for PWM applied above 19C may need some explanation. The d19 - toMax values increases with increasing setTemp.
  //this values is then multiplied by 0.025 (to put the values in the proper range) and added to 1. The pow() function raises this
  //value to the power of 2 (squares it). This in turn is multiplied by 255. The result is that at higher set temperatures 255 is 
  //multiplied by progressively larger values (1.1, 1.4, 1.63), changing the scale. Thus when this value is multiplied by the temperature 
  //differential at a higher set temp a differential of 0.2 result in greater power than a differential of 0.2 at a lower set temp. Below
  //19C the simple differential * 255 seems to work pretty well.
  
  void heater() {
    
          deltaT = temp - Tset;              //calculate the differential between current (actual) temp and set temp
      //    float d19 = Tmax - 19;            //calculate the range between the maximum ramp temp and 19C
      //    float toMax = Tmax - Tset;        //calculate the difference between the Tmax temp and the current set temp
          
      //    if(Tset <= 19) {                  //if set temp is below 19C
      //    PulseWM = abs(deltaT) * 255;      //...set the PWM value (0 to 255) to the (temp differential * 255) (the abs() keeps PWM positive)
      //    if(PulseWM >255) {                //if the PWM value calculated is greater than 255
     //       PulseWM = 255;                  // ...set it to 255 (as this is the max allowed)
     //     }
     //      if(deltaT <= 0) {                        //if the temperature differential (temp - Tset) is zero or negative
      //       analogWrite(Heater_Relay, PulseWM);    //...apply the PulseWM value to the heater relay
      //     } else if (deltaT > 0) {                 //otherwise if the differential is positive
      //       analogWrite(Heater_Relay, 0);          //... turn the relay off (no power)
       //    }
       //   }else if (Tset > 19) {                                              //if instead, the set temp is above 19C
           PulseWM = abs(deltaT) * 255;
       //    PulseWM = abs(deltaT) * (pow((1+((d19-toMax)*0.025)),2) * 255);    //specify the PulseWM value using this function
           if(PulseWM >255) {                                                //Again, if PulseWM is above 255
            PulseWM = 255;                                                   //...set it to 255
          }
          if(deltaT <= 0) {                                                  //if the differntial is zero or less
            analogWrite(Heater_Relay, PulseWM);                              //...apply the PulseWM valu to the relay
          } else if(deltaT > 0) {                                            //otherwise if the differential is positive
            analogWrite(Heater_Relay, 0);                                    //...turn the relay off
          }
        //  }
  }
  
  //---------------------------------------------------------
  
  
  //---------------------------------------------------------
  //sd_card function. Specifies how and what to write to SD card
  //---------------------------------------------------------
  void sd_card() {
    
    FileSystem.begin();
    String dataString;                                                        //make an empty data string
    dataString += String(getDateTime());                                      //add to that string data from the getDateTime function (below)
    dataString += ",";                                                        //add a comma to separate entries
    dataString += String(temp);                                               //add the measured temperature as a string
    dataString += ",";                                                        // etc....
    dataString += String(Tset);
    dataString += ",";                                                        // etc....
    //dataString += String(tideValveState);                                  // uncomment if Unpredictable heating is run on the arduino 
    //dataString += ",";                                                      //connected to the tide valve
    dataString += String(deltaT);
    dataString += ",";
    dataString += String(PulseWM);
  
    File dataFile = FileSystem.open("/mnt/sd/Unprdct1.txt", FILE_APPEND);    //define a datafile, with a name ".txt" (8 characters max) and
                                                                            //save location, if file exist add to it, if not make it
    if (dataFile) {
      dataFile.println(dataString);                                        //print the string made above into data file
      dataFile.close();                                                    //close the file, until called again
    }
  
  }
  //---------------------------------------------------------
  
  
  //---------------------------------------------------------
  //getDateTime function. Specifies how to get dateTime data from server
  //---------------------------------------------------------
  
  String getDateTime() {
    String result;                                                         //create an empty string
    Process datetime;                                                      //specify a Process entity called datetime
  
    datetime.begin("date");   // with datetime get the current date
    datetime.addParameter("+%D-%T");                                     //format the date so we get the date (%D) and the current time (%T) 
  
    datetime.run();                                                      //run this function to actually get datetime
  
    while (datetime.available() > 0) {                                   //return the result of datetime
      char c = datetime.read();
      if (c != '\n')
        result += c;
    }
    return result;
  }
    
  //---------------------------------------------------------
  
   
  //---------------------------------------------------------
  //runAppendRow function. Specifies how and what data is sent to
  //a GoogleDocs spreadsheet
  //---------------------------------------------------------
  
  void runAppendRow(unsigned long datetime, float temp, float Tset, float deltaT, float PulseWM, float Tmax) {
    TembooChoreo AppendRowChoreo;
  
    // Invoke the Temboo client
    AppendRowChoreo.begin();
  
    // Set Temboo account credentials
    AppendRowChoreo.setAccountName(TEMBOO_ACCOUNT);
    AppendRowChoreo.setAppKeyName(TEMBOO_APP_KEY_NAME);
    AppendRowChoreo.setAppKey(TEMBOO_APP_KEY);
  
    // Identify the Choreo to run
    AppendRowChoreo.setChoreo("/Library/Google/Spreadsheets/AppendRow");
  
    // your Google username (usually your email address)
    AppendRowChoreo.addInput("Username", GOOGLE_USERNAME);
  
    // your Google account password
    AppendRowChoreo.addInput("Password", GOOGLE_PASSWORD);
  
    // the title of the spreadsheet you want to append to
    AppendRowChoreo.addInput("SpreadsheetTitle", SPREADSHEET_TITLE);
  
    String timeString = getDateTime();
  
    // Format data
    String data = "";
    data = data + timeString + "," + String(temp) + "," + String(Tset) + "," + String(deltaT) + "," + String(PulseWM) + "," + String(Tmax);
  
    // Set Choreo inputs
    AppendRowChoreo.addInput("RowData", data);
  
    // Run the Choreo
    unsigned int returnCode = AppendRowChoreo.run();
    // A return code of zero means everything worked
    if (returnCode == 0) {
  
    } else {
      // A non-zero return code means there was an error
      // Read and print the error message
      while (AppendRowChoreo.available()) {
        char c = AppendRowChoreo.read();
  
      }
      AppendRowChoreo.close();
    }
  
  }
  //---------------------------------------------------------
  
  
  //---------------------------------------------------------
  // newTmax function. For unpredictable heat ramps, specifies the correct
  // maximum temperature
  //---------------------------------------------------------
  
  
  void newTmax() {
    DateTime now = RTC.now();                                        //get time
  
  byte MonthDays[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31}; //specify days in each month
  
  
     today = now.day();                                           //define today's numerical day
     elapsedDays = today - startDay;                              //calculate the number of elapsed days since the start day
     previous_month_index = (now.month() - 1) - 1;                      //calculate the index for the previous month (keeping in mind
                                                                     //that the indexing starts at 0
    if (now.month() == 0) {                                          //if the current month is 0 (Jan), the previous month was 11 (Dec)
      previous_month_index = 11;
    }
  
    //if in the above substraction elapsed days is <0 the experiment has gone into a second month (start = 28, today = 2,
    //elapsedDays = 2-28 = -26. Additionally if the current year is divisible evenly by 4, its a leap year and Feb has 28
    //days. The number of elapsed days if you have gone into a new month is the negative value from substraction, plus the
    //number of days in the previous month. The code below applies this math, keeping track of leap years.
  
    if (elapsedDays < 0 ) {
      elapsedDays += MonthDays[previous_month_index];                //Start with days from previous month
      if((previous_month_index == 1) && (now.year() % 4) == 0)      // Test if previous month was Feb and if it is a leap year
      elapsedDays++;                                          //if Yes, add 1 more day
    } else
      elapsedDays = today - startDay;                        //otherwise proceed as normal
   
    if (elapsedDays > TempProgSize) {                         //if the elapsed days exceed the number of assigned temperature
      elapsedDays = 0;                                        //values, use a roll-over and go back to the beginning.
    }
  
    Tmax = TempProg[elapsedDays];                               //once you have determined what day of the experiment you are in
  }                                                               //select that value from the TempProg array
  //---------------------------------------------------------
  
  
  ////---------------------------------------------------------
  ////lcdPrint function. Organizes and specifies output to LCD
  ////---------------------------------------------------------
  //void lcdPrint()
  //{
  ////  DateTime now = RTC.now();
  ////
  ////  lcd.setCursor(0, 0);                                         //place the cursor at the beginning
  ////  lcd.print("Time: ");                                         //print the time, the if/else statements allow
  ////  lcd.print(now.hour());                                       //for leading zeros when min or sec is less than 10
  ////  lcd.print(':');
  ////  if (now.minute() < 10) {
  ////    lcd.print("0");
  ////    lcd.print(now.minute());
  ////  }
  ////  else if (now.minute() >= 10) {
  ////    lcd.print(now.minute());
  ////  }
  ////  lcd.print(':');
  ////  if (now.second() < 10) {
  ////    lcd.print("0");
  ////    lcd.print(now.second());
  ////  }
  ////  else if (now.second() >= 10) {
  ////    lcd.print(now.second());
  ////  }
  //
  //  
  //  lcd.setCursor(2,0);
  //  lcd.print(getDateTime());
  //  lcd.setCursor(0, 1);                                      //set cursor to second line
  //  lcd.print("T:");                                          //print the temperature and the set temperature
  //  lcd.print(temp);
  //  lcd.print(" Ts:");
  //  lcd.print(Tset);
  //
  //
  //}
  //
  ////---------------------------------------------------------
  //
  
  
  //---------------------------------------------------------
  //PID Loop
  //---------------------------------------------------------
  //   void PIDloop()
  //   {
  //     Setpoint=Tset;
  //     Input = (double) getTempFunc(); //get an input (temperature) from the getTempFunc
  //     myPID.Compute();  //compute PID
  //
  //
  //     if( (millis() - windowStartTime) > WindowSize)
  //     {
  //       windowStartTime += WindowSize; //time to shift the Relay Window
  //     }
  //     if( Output > (millis() - windowStartTime) )
  //     {
  //       digitalWrite(Heater_Relay, HIGH); // turn PID relay on
  //       digitalWrite(Heater_LED, HIGH); //turn on LED when the PID is on
  //     }
  //     else
  //     {
  //       digitalWrite(Heater_Relay, LOW); // turn PID relay off
  //       digitalWrite(Heater_LED, LOW); //turn off LED when the PID is off
  //     }
  //   }
  
  //---------------------------------------------------------
  
  
  //---------------------------------------------------------
  // printTimeTemp Function for printing the current date/time and temp data
  //(actual temp and set temp) to the serial port in a nicely formatted layout.
  //---------------------------------------------------------
  // void printTimeTemp()
  //  {
  //    DateTime now = RTC.now();
  //    Serial.print(now.year(), DEC);
  //    Serial.print('/');
  //    Serial.print(now.month(), DEC);
  //    Serial.print('/');
  //    Serial.print(now.day(), DEC);
  //    Serial.print(' ');
  //    Serial.print(now.hour(), DEC);
  //    Serial.print(':');
  //     if (now.minute() < 10) {
  //        Serial.print("0");
  //        Serial.print(now.minute());
  //      }
  //      else if (now.minute() >= 10) {
  //        Serial.print(now.minute());
  //      }
  //    Serial.print(':');
  //      if (now.second() < 10) {
  //        Serial.print("0");
  //        Serial.print(now.second());
  //      }
  //      else if (now.second() >= 10){
  //        Serial.print(now.second());
  //      }
  //        Serial.print(",");
  //        Serial.print("    ");
  //        Serial.print(char(temp));
  //        Serial.print(" deg.C");
  //        Serial.print(",");
  //        Serial.print("   Set Temp: ");
  //        Serial.print(char(Tset));
  //        Serial.print(", ValveState: ");
  //        Serial.print(char(lowTideRelay));
  //  }
  //---------------------------------------------------------
  
  
  
  //---------------------------------------------------------
  //heater function using timers. Specifies when (at what temperature differential)
  //to turn on the heater and for how long to get a consistent ramp 
  //---------------------------------------------------------
  
  //void heater() {
  //  float deltaT = Tset - temp;                                  //define variable calculating temperature differential
  //  float invertDeltaT = temp - Tset;                            //define an inverse differential
  //
  //  unsigned long heatTimer = millis();
  //  
  //while(heatTimer_ON == TRUE) {
  //  if (millis() - heatTimer <= 5000) {
  //    if (Tset < 19) {
  //      if (deltaT < 0.05 && invertDeltaT < 0.1) {                   //use invert differential to make sure heating continues
  //        if (millis() - heatTimer <= 500) {
  //          heatTimer = millis();
  //          digitalWrite(Heater_Relay, HIGH);                          //even when temp is above Tset
  //          digitalWrite(Heater_LED, HIGH);
  //        }
  //      } else if (deltaT >= 0.05 && deltaT < 0.09) {
  //        if (millis() - heatTimer <= 1500) {
  //          heatTimer = millis();
  //          digitalWrite(Heater_Relay, HIGH);
  //          digitalWrite(Heater_LED, HIGH);
  //        }
  //      } else if (deltaT >= 0.09 && deltaT < 0.18) {
  //        if (millis() - heatTimer <= 2000) {
  //          heatTimer = millis();
  //          digitalWrite(Heater_Relay, HIGH);
  //          digitalWrite(Heater_LED, HIGH);
  //        }
  //      }  else if (deltaT >= 0.24) {
  //        if (millis() - heatTimer <= 4000) {
  //          heatTimer = millis();
  //          digitalWrite(Heater_Relay, HIGH);
  //          digitalWrite(Heater_LED, HIGH);
  //        }
  //      } else {
  //        digitalWrite(Heater_Relay, LOW);
  //        digitalWrite(Heater_LED, LOW);
  //      }
  //    } else if (Tset >= 19 && Tset < 25) {                        //change parameters at temps above 19 and below 25C
  //      if (deltaT < 0.05 && invertDeltaT < 0.1 ) {
  //        if (millis() - heatTimer < 2000) {
  //          heatTimer = millis();
  //          digitalWrite(Heater_Relay, HIGH);                          //even when temp is above Tset
  //          digitalWrite(Heater_LED, HIGH);
  //        }
  //      } else if (deltaT >= 0.05 && deltaT < 0.18) {
  //        if (millis() - heatTimer < 3000) {
  //          heatTimer = millis();
  //          digitalWrite(Heater_Relay, HIGH);                          //even when temp is above Tset
  //          digitalWrite(Heater_LED, HIGH);
  //        }
  //      } else if (deltaT >= 0.18) {
  //        if (millis() - heatTimer < 4000) {
  //          heatTimer = millis();
  //          digitalWrite(Heater_Relay, HIGH);                          //even when temp is above Tset
  //          digitalWrite(Heater_LED, HIGH);
  //        }
  //      } else {
  //        digitalWrite(Heater_Relay, LOW);
  //        digitalWrite(Heater_LED, LOW);
  //      }
  //    } else if (Tset >= 25) {                                   //change parameters again above 25C (make heating more aggressive)
  //      if (deltaT < 0.05 && invertDeltaT < 0.1) {
  //        if (millis() - heatTimer < 2000) {
  //          heatTimer = millis();
  //          digitalWrite(Heater_Relay, HIGH);                          //even when temp is above Tset
  //          digitalWrite(Heater_LED, HIGH);
  //        }
  //      } else if (deltaT >= 0.05) {
  //        if (millis() - heatTimer < 4000) {
  //          heatTimer = millis();
  //          digitalWrite(Heater_Relay, HIGH);                          //even when temp is above Tset
  //          digitalWrite(Heater_LED, HIGH);
  //          } else {
  //        digitalWrite(Heater_Relay, LOW);
  //        digitalWrite(Heater_LED, LOW);
  //          }
  //        }
  //      }
  //  } else if(heatTimer > 5000) {
  //      
  //      heatTimer = millis();
  //    }
  //}
  //  }
  //
  ////---------------------------------------------------------
  
  
  ////---------------------------------------------------------
  ////heater function using delays. Specifies when (at what temperature differential)
  ////to turn on the heater and for how long to get a consistent ramp
  ////---------------------------------------------------------
  //
  //void heater() {
  //  float deltaT = Tset - temp;                                  //define variable calculating temperature differential
  //  float invertDeltaT = temp - Tset;                            //define an inverse differential
  //
  //  if (Tset < 19) {
  //    if (deltaT < 0.05 && invertDeltaT < 0.1) {                   //use invert differential to make sure heating continues
  //      digitalWrite(Heater_Relay, HIGH);                          //even when temp is above Tset
  //      digitalWrite(Heater_LED, HIGH);
  //      delay(500);
  //      digitalWrite(Heater_Relay, LOW);
  //      digitalWrite(Heater_LED, LOW);
  //    } else if (deltaT >= 0.05 && deltaT < 0.09) {
  //      digitalWrite(Heater_Relay, HIGH);
  //      digitalWrite(Heater_LED, HIGH);
  //      delay(1500);
  //      digitalWrite(Heater_Relay, LOW);
  //      digitalWrite(Heater_LED, LOW);
  //    } else if (deltaT >= 0.09 && deltaT < 0.18) {
  //      digitalWrite(Heater_Relay, HIGH);
  //      digitalWrite(Heater_LED, HIGH);
  //      delay(2000);
  //      digitalWrite(Heater_Relay, LOW);
  //      digitalWrite(Heater_LED, LOW);
  //    }  else if (deltaT >= 0.24) {
  //      digitalWrite(Heater_Relay, HIGH);
  //      digitalWrite(Heater_LED, HIGH);
  //      delay(4000);
  //      digitalWrite(Heater_Relay, LOW);
  //      digitalWrite(Heater_LED, LOW);
  //    } else {
  //      digitalWrite(Heater_Relay, LOW);
  //      digitalWrite(Heater_LED, LOW);
  //    }
  //  } else if (Tset >= 19 && Tset < 25) {                        //change parameters at temps above 19 and below 25C
  //    if (deltaT < 0.05 && invertDeltaT < 0.1 ) {
  //      digitalWrite(Heater_Relay, HIGH);
  //      digitalWrite(Heater_LED, HIGH);
  //      delay(1000);
  //      digitalWrite(Heater_Relay, LOW);
  //      digitalWrite(Heater_LED, LOW);
  //    } else if (deltaT >= 0.05 && deltaT < 0.18) {
  //      digitalWrite(Heater_Relay, HIGH);
  //      digitalWrite(Heater_LED, HIGH);
  //      delay(2000);
  //      digitalWrite(Heater_Relay, LOW);
  //      digitalWrite(Heater_LED, LOW);
  //    } else if (deltaT >= 0.18) {
  //      digitalWrite(Heater_Relay, HIGH);
  //      digitalWrite(Heater_LED, HIGH);
  //      delay(4000);
  //      digitalWrite(Heater_Relay, LOW);
  //      digitalWrite(Heater_LED, LOW);
  //    }
  //  } else if (Tset >= 25) {                                   //change parameters again above 25C (make heating more aggressive)
  //    if (deltaT < 0.05 && invertDeltaT < 0.1) {
  //      digitalWrite(Heater_Relay, HIGH);
  //      digitalWrite(Heater_LED, HIGH);
  //      delay(2000);
  //      digitalWrite(Heater_Relay, LOW);
  //      digitalWrite(Heater_LED, LOW);
  //    } else if (deltaT >= 0.05) {
  //      digitalWrite(Heater_Relay, HIGH);
  //      digitalWrite(Heater_LED, HIGH);
  //      delay(4000);
  //      digitalWrite(Heater_Relay, LOW);
  //      digitalWrite(Heater_LED, LOW);
  //    }
  //  }
  //}
  //
  ////---------------------------------------------------------
  
  //---------------------------------------------------------
  //getDateTime function. Specifies how to get dateTime data from server
  //---------------------------------------------------------
  
  //Alternative method for getting the date time. Ends up not being necessary
  //in this context and so is not used
    
  //  DateTime now = RTC.now();
  //  
  //  String datetime;   //create an empty string
  //
  //  datetime += String(now.year());
  //  datetime += String('/');
  //  datetime += String(now.month());
  //  datetime += String('/');
  //  datetime += String(now.day());
  //  datetime += String(' ');
  //  datetime += String(now.hour());
  //  datetime += String(':');
  //  if(now.minute() < 10) {
  //  datetime += String('0');
  //  datetime += String(now.minute());
  //  }
  //  else if(now.minute() >= 10) {
  //    datetime += String(now.minute());
  //  }
  //  datetime += String(':');
  //  if(now.second() < 10) {
  //    datetime += String('0');
  //    datetime += String(now.second());
  //  }
  //  else if(now.second() >= 10) {
  //    datetime += String(now.second());
  //  }
  //  
  //  return datetime;
  
  //---------------------------------------------------------
  

