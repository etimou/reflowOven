// BOF preprocessor bug prevent - insert me on top of your arduino-code
// From: http://www.a-control.de/arduino-fehler/?lang=en
#if 1
__asm volatile ("nop");
#endif

#include <math.h>
#include <PID_v1.h>

//#define USE_MAX31855
#ifdef  USE_MAX31855
  #define thermocoupleSOPin A3
  #define thermocoupleCSPin A2
  #define thermocoupleCLKPin A1
  #include <Adafruit_MAX31855.h>
  Adafruit_MAX31855 thermocouple(thermocoupleCLKPin, thermocoupleCSPin, thermocoupleSOPin);
#endif

// GCODE commands
char inData[20]; // Allocate some space for the string
char inChar; // Where to store the character read
byte index = 0; // Index into array; where to store the character

// low frequency PWM
unsigned long lastFlip=0;
boolean statusPWM=0;

// reading temperature every ...ms
unsigned long lastReadTemp=0;
unsigned long periodReadTemp=1000;

// PID
#define PID_KP_PREHEAT 100
#define PID_KI_PREHEAT 0.025
#define PID_KD_PREHEAT 20
double setpoint;
double input;
double output;
double kp = PID_KP_PREHEAT;
double ki = PID_KI_PREHEAT;
double kd = PID_KD_PREHEAT;

PID reflowOvenPID(&input, &output, &setpoint, kp, ki, kd, DIRECT);


double Thermister(int RawADC) {
 double Temp;
 RawADC =1024-RawADC;
 Temp = log(((10240000/RawADC) - 10000));
 Temp = 1 / (0.001129148 + (0.000234125 + (0.0000000876741 * Temp * Temp ))* Temp );
 Temp = Temp - 273.15;            // Convert Kelvin to Celcius
 //Temp = (Temp * 9.0)/ 5.0 + 32.0; // Convert Celcius to Fahrenheit
 return Temp;
}

int getTempCommand(char* command)
{
  char inDataCopy[20];
  strcpy(inDataCopy,inData);
     
  char * pch;
                 
  pch = strtok (inDataCopy," ");
  while (pch != NULL)
  {
    if (pch[0] == 'S')
    {
      return(atoi(pch+1));
    }
    pch = strtok (NULL, " ");
  }
}


void setup() {
 Serial.begin(115200);
 pinMode(8, OUTPUT);
 
 
 Serial.println("Reflow Oven for Acolab");
 
 for (int i=0; i<8; i++)
 {
   Serial.println("ok");
   delay(100); 
 }
 
  reflowOvenPID.SetMode(MANUAL);
  reflowOvenPID.SetOutputLimits(0.0,100.0);
  reflowOvenPID.SetTunings(kp,ki,kd);
  reflowOvenPID.SetSampleTime(8000);  
}

void loop()
{
   while(Serial.available() > 0) // Don't read unless
                                // there you know there is data
   {
       if(index < 19) // One less than the size of the array
       {
           inChar = Serial.read(); // Read a character
           
           if ((inChar == '\n') | (inChar == '\r')) 
           {
             //end of command

             if (index>1)
             {
               Serial.print("ok "); //it is a command
              
               if (strstr(inData, "M105") != NULL)
               {
                  Serial.print("T:");
                  Serial.print(input);  
                  Serial.println(" /0.0 B:0.0 /0.0 @:0 B@:0");
               }
               else if (strstr(inData, "M115") != NULL)
               {
                  Serial.println("Reflow oven rev0.0");
               }
               else if (strstr(inData, "M104") != NULL)
               {
                  setpoint = getTempCommand(inData);
                  if (setpoint==0) {reflowOvenPID.SetMode(MANUAL);}
                  else {reflowOvenPID.SetMode(AUTOMATIC);}
               }
               else
               {
                  Serial.println("");
               }
             }
             
             index=0;
           }
           else
           {
              inData[index] = inChar; // Store it
              index++; // Increment where to write next
              inData[index] = '\0'; // Null terminate the string
           }        
       }
   }
   
   //Manage low frequency PWM
   unsigned long now = millis();
   if (statusPWM==0)
   {
     if (now - lastFlip >= (100-(int)output)*20)
     {
        statusPWM=1;
        digitalWrite(8,1);
        lastFlip=now;
     }
   }
   else
   {
     if (now - lastFlip >= (int)output*20)
     {
        statusPWM=0;
        digitalWrite(8,0);
        lastFlip=now;
     }
   }
   
   //Read Temperature
   if (now - lastReadTemp >= periodReadTemp)
   {
      #ifdef  USE_MAX31855
        input = thermocouple.readCelsius();
      #else
        input = Thermister(analogRead(0));
      #endif

      lastReadTemp = now;
      //Serial.println((int)output);
   }
   
   
   //PID Compute()
   reflowOvenPID.Compute();
}

