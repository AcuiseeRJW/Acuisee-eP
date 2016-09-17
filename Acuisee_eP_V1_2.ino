// AcuiSee -eP  PRIMATE ENRICHMENT DEVICE
// COMMENTS
// **************************************************************
// date        who  ver	what
// ----------------------------------------------------------------------------------------------------
// 07/13/2016   rjw 1.0 original coding
// 09/17/2016   rjw 1.1 limited pumpon time
// 09/17/2016   rjw 1.2 add a return "a" and "b"

// ----------------------------------------------------------------------------------------------------
// FROM TABLET         FROM ARDUINO    
// Command Description Response  Description     Note
// --------------------------------------------------------------------
// A       OFF All     a         All off         Pump, LED
// B       ON All      b         All on          Pump, LED
// C       ON Pump     c         Pump on 
// D       OFF Pump    d         Pump off  
// E       ON LED      e         LED on  
// F       OFF LED     f         LED off 
// R                   r         RESEND last command 
// S       Status Req 
// Y       PING        y         PING            Arduino to tablet     
// Z       NOOP        z         NOOP            Tablet to Arduino                 
// ----------------------------------------------------------------------------------------------------

//PIN	 I/O/A	DESCRIPTION
// -----------------------------------------------------
// D6	 OP     PUMP MOTOR / SOLENOID VALVE Combination
// D13 OP     Board Run Light (blinky)
// D14 OP     Feed light


#include <SoftwareSerial.h>

#define Version        1.10
#define bluetoothRx    2  // RX-I
#define bluetoothTx    3  // TX-O 
#define pump           6
#define run_lt         13
#define feed_lt        14
#define pumpOnLmit     5000

// Bluetooth
SoftwareSerial bluetooth(bluetoothTx, bluetoothRx);


long          previousMillis    = 0;        // will store last time run LED was updated
long          interval          = 1000;     // interval at which to blink (milliseconds)
int           divisor           = 1;        // used to control blink rate of LED
unsigned long currentMillis     = millis();
int           badCharCounter    = 0 ;       // bluetooth bad char counter
boolean       badCharFlag       = false ;
unsigned long badCharTimer      = millis();
long          badCharTimerLimit = 200  ;      // .1 sec 
char          inChar            = -1; // Where to store the character read

boolean       pumpOn       = false ;
unsigned long pumpOnMillis = millis();

/* **************************************************************************** */

void setup()
{
  // IO
  pinMode(pump,     OUTPUT);
  pinMode(run_lt,   OUTPUT);
  pinMode(feed_lt,  OUTPUT);
  digitalWrite(pump,    LOW);
  digitalWrite(run_lt,  LOW);
  digitalWrite(feed_lt, LOW);
  
  //  BlueTooth
  bluetooth.begin(115200);  // The Bluetooth Mate defaults to 115200bps
  bluetooth.print("$");     // Print three times individually to enter command mode
  bluetooth.print("$");
  bluetooth.print("$"); 
  delay(100);               // Short delay, wait for the BT module to send back CMD 
  bluetooth.println("U,9600,N");  // Temporarily Change the baudrate to 9600, no parity
  delay(100);
  bluetooth.begin(9600);  // Start bluetooth serial at 9600

  // Serial
  Serial.begin(9600);     // Start serial port at 9600 
  Serial.print(F("\n\n  AcuiSee-eP Version: "));  Serial.println(Version);
  Serial.println(F("-------------------------------"));
} // setup

//  STAT() ///////////////////////////////////////////////
void STAT() {
  if ((digitalRead(pump) == LOW) && (digitalRead(feed_lt) == LOW)) {
    Serial.write("a"); bluetooth.print("a");
  }
  if ((digitalRead(pump) == HIGH) && (digitalRead(feed_lt) == HIGH)) {
    Serial.write("b"); bluetooth.print("b");
  }
    
  if (digitalRead(pump) == LOW) {
    Serial.write("d"); bluetooth.print("d");
  } else {
    Serial.write("c"); 
    bluetooth.print("c"); 
  }
  
  if (digitalRead(feed_lt) == LOW) {
    Serial.write("f"); bluetooth.print("f");
  } else {
    Serial.write("e"); 
    bluetooth.print("e");
  }
  Serial.write("\n"); bluetooth.print("\n");
 } // STAT


// processCommand()   ////////////////////////////////////////////

char processCommand(char c1) {
   switch (c1) {
        case 'A':
          PumpOff();
          Feed_LtOff();
          Serial.write("a\n");    
          bluetooth.print("a\n");  
          return 0;
          break;
         case 'B':
          PumpOn();
          Feed_LtOn();
          Serial.write("b\n");    
          bluetooth.print("b\n");  
          return 0;
          break; 
         case 'C':
          PumpOn();
          return 0;
          break;
        case 'D':
          PumpOff();
          return 0;
          break;  
        case 'E':
          Feed_LtOn();
          return 0;
          break; 
        case 'F':
          Feed_LtOff();
          return 0;
          break;  
        case 'S':
          STAT();
          return 0;
          break;
        case'Z':
          Serial.write("z\n");  bluetooth.print("z\n"); // NO-OP
          delay(50);
          return 0;
          break;
        default: 
          return 1;
      } // of switch
   } // end of processCommand()

// readSerialPort()   ////////////////////////////////////////////
char readSerialPort(void) {

    while (Serial.available() > 0) { // Don't read unless you know there is data
      inChar = Serial.read(); // Read a character
      if ((inChar >= 65) && (inChar <= 90)) {     // ascii A through Z
         processCommand(inChar);
      } // of if inchar
     } // of While
    } // of function readSerialPort

// readBlueTooth()  Bluetooth  ////////////////////////////////////////////
char readBlueTooth(void) {

    while (bluetooth.available() > 0) { // Don't read unless you know there is data
      Serial.print("bluetooth.available() = ");  //debug
       Serial.println(bluetooth.available());    //debug
    
    inChar = bluetooth.read();  // Read a character     
     Serial.print("bt raw >>");
      Serial.print(inChar); 
       Serial.print(", hex: ");
        Serial.println(inChar, HEX);   // prints value as string in hexadecimal (base 16):
        
    if ((inChar >= 65) && (inChar <= 90)) {     // ascii A through Z
      // if we make it here, it is a good character that passed the filter tests          
      Serial.print("bt filtered >>");
       Serial.print(inChar); 
        Serial.print(", hex: ");
         Serial.println(inChar, HEX);               // prints value as string in hexadecimal (base 16): 
          processCommand(inChar);                 // process the command
           badCharCounter = 0;
            badCharFlag = false;
       } else { 

      // if we make it here, character failed the filter test, enable retries  
       while (bluetooth.available() > 0) inChar = bluetooth.read();  // Flush the buffer
       
       delay(5);                                                     // Delay a slight amount
        badCharCounter++; 
         badCharFlag = true;
          badCharTimer = millis() ;
        Serial.write("r\n");                                         // send a retry character 
         bluetooth.print("r\n");                                      // send a retry character
        Serial.print("badCharCounter: ");
        Serial.println(badCharCounter);
         } // of if inchar
    } // of While
} // of function readBlueTooth

// Feed_LtOn  ///////////////////////////////////////////////////////////
 void Feed_LtOn(void){
   digitalWrite(feed_lt, HIGH);
     Serial.write("e\n"); bluetooth.print("e\n");
} //Feed_LtOn

// FeedLightOff ///////////////////////////////////////////////////////////
 void Feed_LtOff(void){
    digitalWrite(feed_lt, LOW);
     Serial.write("f\n"); bluetooth.print("f\n");
} //FeedLtOff

// PumpOn ///////////////////////////////////////////////////////////
 void PumpOn(void){
    digitalWrite(pump, HIGH);
    pumpOnMillis = millis();
    pumpOn = true;
     Serial.write("c\n"); bluetooth.print("c\n");
} //PumpOn

// PumpOff ///////////////////////////////////////////////////////////
 void PumpOff(void){
    digitalWrite(pump, LOW);
     Serial.write("d\n"); bluetooth.print("d\n");
} //PumpOff

// loop()   //////////////////////////////////////////////
void loop()
{
  // Read and process any commands that have arrived via USB
   if (Serial.available()    > 0) readSerialPort(); // serial port
   if (bluetooth.available() > 0) readBlueTooth();  // bluetooth port
  // execute a BT character retry if necessary
   if (((millis() - badCharTimer) > badCharTimerLimit) && (badCharFlag == true)) {
     // send out the next retry request
       bluetooth.print("r\n");                                      // send a retry character
       badCharCounter++; 
        badCharTimer = millis() ;
        Serial.write("r\n");    
         Serial.print("badCharCounter: ");
          Serial.println(badCharCounter);
    }


// If pump is turned in for more than 5 seconds, turn it off
  if (pumpOn){
  if (millis() - pumpOnMillis > pumpOnLmit) {
   digitalWrite(pump, LOW);
   pumpOn = false;
   Serial.write("d\n"); bluetooth.print("d\n");
  }  }
   
// Blink Blink Blink Blink Blink Blink Blink Blink Blink Blink
  currentMillis = millis();
  if (currentMillis - previousMillis > interval/divisor) {
    // save the last time you blinked the LED
    previousMillis = currentMillis;
    digitalWrite(run_lt,  !digitalRead(run_lt)); // change state of run light
  }
} // loop
