// AcuiSee -eP  PRIMATE ENRICHMENT DEVICE
// COMMENTS
// **************************************************************
// date        who  ver	what
// ----------------------------------------------------------------------------------------------------
// 07/13/2016   rjw 1.0 original coding
// 09/17/2016   rjw 1.1 limited pumpon time
// 09/17/2016   rjw 1.2 add a return "a" and "b"
// 09/19/2016   rjw 1.3 fixed not setting pumpOn flag to false when pump turned off
// 11/03/2016   rjw 1.4 integrate L293DD board
// 12/01/2016   rjw 1.5 added prime pb in manual mode, turn light on in manual mode
// 12/05/2016   rjw 1.6 cleaned up manual mode extensively, formatted, and made timers variable
// ----------------------------------------------------------------------------------------------------


#define Version    1.60
const int          pumpRevTime =     2000;  // time to reverse pump to pull fluid away from sipper tube in milliseconds
const int          pumpOnLimit =     pumpRevTime + 500; // longer than reverse time
const unsigned int ManualModeLimit = 30000;  // time where no chars received b4 going into manual in milliseconds


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

#include <SoftwareSerial.h>
#define bluetoothRx    9  // RX-I
#define bluetoothTx    8  // TX-O 
#define run_lt         13
#define feed_lt        2
#define pb_lt          4
#define pb             6
#define enablePin      5 // Uno and similar boards, pins 5 and 6 have a frequency of approximately 980 Hz
#define in1Pin         3
#define in2Pin         11
#define solenoid       10



// Bluetooth instantiation
SoftwareSerial bluetooth(bluetoothTx, bluetoothRx);

// Variable definition and initiation

int           PumpSpeed         = 0;
boolean       reverse           = false;
const int     BlinkTime         = 1000; 
long          previousMillis    = 0;            // will store last time run LED was updated
long          interval          = BlinkTime;    // interval at which to blink (milliseconds)
int           divisor           = 1;            // easy way to change the blink rate of LED
unsigned long currentMillis     = millis();

int           badCharCounter    = 0 ;         // bluetooth bad char counter
boolean       badCharFlag       = false ;
unsigned long badCharTimer      = millis();
long          badCharTimerLimit = 250  ;      // 1/4 sec
char          inChar            = -1;         // Where to store the character read

boolean       pumpOn       = false ;
unsigned long pumpOnMillis = millis();

boolean       ManualMode  = true;             // start up in manual mode
unsigned long ManualTimer = millis();         // preload manual mode timer check


/* **************************************************************************** */

void setup()
{
  // IO
  pinMode(run_lt,   OUTPUT);
  pinMode(feed_lt,  OUTPUT);
  pinMode(enablePin, OUTPUT);
  pinMode(in1Pin,   OUTPUT);
  pinMode(in2Pin,   OUTPUT);
  pinMode(solenoid, OUTPUT);
  pinMode(pb_lt,    OUTPUT);
  pinMode(pb,       INPUT_PULLUP);

  // Serial Start
  Serial.begin(115200);     // Start serial port at specified baud rate
  Serial.print(F("\n\n  AcuiSee-eP Version: "));  Serial.println(Version);
  Serial.println(F("-------------------------------"));
  Serial.println(F("Serial started . . . "));
  delay(500);

  //  BlueTooth Start
  Serial.println(F("Bluetooth start . . . "));
  bluetooth.begin(115200);         // The Bluetooth module defaults to 115200bps
  bluetooth.print("$");            // Print three times individually to enter command mode
  bluetooth.print("$");
  bluetooth.print("$");
  delay(100);                      // Short delay, wait for the BT module to send back CMD
  bluetooth.println("U,9600,N");   // Change the baudrate to 9600, no parity - more reliable
  delay(100);
  bluetooth.begin(9600);           // Start bluetooth serial at 9600
  Serial.println(F("Bluetooth started . . . "));
  Serial.println(F("Flushing Bluetooth buffer . . . "));
  delay(500);
  while (bluetooth.available() > 0) inChar = bluetooth.read();  // Flush the bluetooth buffer
  Serial.println(F("Startup complete . . . "));

  // Misc startup
  ManualModeOn();                  // start system in manual mode to allow pump priming
  digitalWrite(run_lt,  LOW);
  digitalWrite(feed_lt, LOW);

} // setup

//  STAT() ///////////////////////////////////////////////
void STAT() {
  if ((pumpOn == LOW) && (digitalRead(feed_lt) == LOW)) {
    Serial.write("a"); bluetooth.print("a");
  }
  if ((pumpOn) && (digitalRead(feed_lt) == HIGH)) {
    Serial.write("b"); bluetooth.print("b");
  }

  if (pumpOn == LOW) {
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
} // STAT()
//  STAT() ///////////////////////////////////////////////

// processCommand()   ////////////////////////////////////////////
char processCommand(char c1) {
  switch (c1) {
    case 'A':
      Feed_LtOff();
      PumpOffReverse();
      Serial.write("a\n");
      bluetooth.print("a\n");
      return 0;
      break;

    case 'B':
      PumpOnFWD();
      Feed_LtOn();
      Serial.write("b\n");
      bluetooth.print("b\n");
      return 0;
      break;

    case 'C':
      PumpOnFWD();
      return 0;
      break;

    case 'D':
      PumpOffReverse();
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
// processCommand()   ////////////////////////////////////////////


// readSerialPort()   ////////////////////////////////////////////
char readSerialPort(void) {
  // Don't read unless you know there is data
  while (Serial.available() > 0) {
    if (ManualMode) ManualModeOff();    // whenever we get a character reset manual mode
    ManualTimer = millis();             // need to reset timer with every char received
    inChar = Serial.read();
    if ((inChar >= 65) && (inChar <= 90)) {     // ascii A through Z
      processCommand(inChar);
    } // of if inchar
  } // of While
} // of function readSerialPort
// readSerialPort()   ////////////////////////////////////////////

// readBlueTooth()  Bluetooth  ////////////////////////////////////////////
char readBlueTooth(void) {

  while (bluetooth.available() > 0) {
    if (ManualMode) ManualModeOff();  // whenever we get a bt character reset manual mode
    ManualTimer = millis();             // need to reset timer with every char received
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
      Serial.println(inChar, HEX);            // prints value as string in hexadecimal (base 16):
      processCommand(inChar);                 // process the command
      badCharCounter = 0;
      badCharFlag = false;
    } else {

      // if we make it here, character failed the filter test, enable retries
      while (bluetooth.available() > 0) inChar = bluetooth.read();  // Flush the buffer
      delay(5);                                                     // Delay a slight amount
      badCharCounter++;                                            // track bad character info
      badCharFlag = true;                                         // set the flag indicating bad character received
      badCharTimer = millis() ;                                  // reset the retry timer
      Serial.write("r\n");                                      // send a retry character
      bluetooth.print("r\n");                                  // send a retry character
      Serial.print("badCharCounter: ");
      Serial.println(badCharCounter);
    } // of if inchar
  } // of While
} // of function readBlueTooth
// readBlueTooth()  Bluetooth  ////////////////////////////////////////////

// Feed_LtOn  ///////////////////////////////////////////////////////////
void Feed_LtOn(void) {
  digitalWrite(feed_lt, HIGH);
  Serial.write("e\n"); bluetooth.print("e\n");
} //Feed_LtOn
// Feed_LtOn  ///////////////////////////////////////////////////////////

// FeedLightOff ///////////////////////////////////////////////////////////
void Feed_LtOff(void) {
  digitalWrite(feed_lt, LOW);
  Serial.write("f\n"); bluetooth.print("f\n");
} //FeedLtOff
// FeedLightOff ///////////////////////////////////////////////////////////

// setMotor Pump Speed and direction //////////////////////////////////////////
void setMotor(int PumpSpeed, boolean reverse)
{
  digitalWrite(solenoid, HIGH);
  digitalWrite(in1Pin, ! reverse);
  digitalWrite(in2Pin, reverse);
  analogWrite(enablePin, PumpSpeed); // Uno and similar boards, pins 5 and 6 have a frequency of approximately 980 Hz
}
// setMotor spped and direction //////////////////////////////////////////

// PumpOnFWD ///////////////////////////////////////////////////////////
void PumpOnFWD(void) {
  reverse = false; 
  PumpSpeed = 225;  // was 200
  setMotor(PumpSpeed, reverse);
  pumpOnMillis = millis();
  pumpOn = true;
  Serial.write("c\n"); bluetooth.print("c\n");
} //PumpOnFWD

// PumpOnREV ///////////////////////////////////////////////////////////
void PumpOnREV(void) {
  reverse = true; 
  PumpSpeed = 225;  // was 200
  setMotor(PumpSpeed, reverse);
  pumpOnMillis = millis();
  pumpOn = true;
  Serial.write("c\n"); bluetooth.print("c\n");
} //PumpOnFWD


// PumpOffReverse ///////////////////////////////////////////////////////////
// Stops and reverses pump to pull liquid from feed line and sipper tube
void PumpOffReverse(void) {
  // first stop, delay then run the pump in reverse to relieve sipper pressure
  analogWrite(enablePin, 0);
  delay(250);
  reverse = true;
  PumpSpeed   = 225;  // was 200
  setMotor(PumpSpeed, reverse);
  delay(pumpRevTime);

  // second turn off the pump totally
  analogWrite(enablePin, 0);
  digitalWrite(in1Pin, 0);
  digitalWrite(in2Pin, 0);
  digitalWrite(solenoid, 0);
  pumpOn = false;
  Serial.write("d\n"); bluetooth.print("d\n");
} //PumpOffReverse
// PumpOffReverse ///////////////////////////////////////////////////////////

// PumpStop ///////////////////////////////////////////////////////////
// Stops the pump, no reversal
void PumpStop(void) {
  // second turn off the pump totally
  analogWrite(enablePin, 0);
  digitalWrite(in1Pin, 0);
  digitalWrite(in2Pin, 0);
  digitalWrite(solenoid, 0);
  pumpOn = false;
  Serial.write("d\n"); bluetooth.print("d\n");
} //PumpStop
// PumpStop ///////////////////////////////////////////////////////////



// ManualModeOff /////////////////////////////////////////////////
void ManualModeOff(void) {
  ManualTimer = millis();
  ManualMode = false;
  digitalWrite(pb_lt, LOW);
  Serial.println("Manual Mode Off");
  delay(20); // need short delay to millis if statement can solve properly
}
// ManualModeOff /////////////////////////////////////////////////

// ManualModeOn  /////////////////////////////////////////////////
void ManualModeOn(void) {
  //ManualTimer = millis();
  ManualMode = true;
  digitalWrite(pb_lt, HIGH);
  Serial.println("Manual Mode On");
  delay(10); // need short delay to millis if statement can solve properly
}
// ManualModeOn  /////////////////////////////////////////////////


// loop()   ------- LOOP ------- LOOP ------- LOOP ------- LOOP ------- LOOP ------- LOOP ------- LOOP ------- LOOP
void loop()
{
  // Read and process any commands that have arrived
  if (Serial.available() > 0) { readSerialPort(); } // serial port
  if (bluetooth.available() > 0) { readBlueTooth(); } // bluetooth port

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

  // Check for manual mode
  if ((millis() - ManualTimer)  > ManualModeLimit) {
    if (!(ManualMode)) {
      Serial.println("Going to Manual Mode");
      ManualModeOn();
    } // necessary to make a one-shot otherwise println will execute each loop
  }

  // if we are in manual mode check for PB actuation
  if (ManualMode) {
    if ((digitalRead(pb) == LOW) and !(pumpOn))  {
      PumpOnFWD();
    }
    if ((digitalRead(pb) == HIGH) and (pumpOn)) {
      PumpOffReverse();
    }
  }

  // If pump is turned on for more than pumpOnLimit, turn it off
  if ((pumpOn) and !(ManualMode)) {
    if (millis() - pumpOnMillis > pumpOnLimit) {
      PumpStop(); // TURN THE PUMP OFF
      Serial.write("d - due to timeout\n"); bluetooth.print("d\n");
    }
  }

  // Blink Blink Blink Blink Blink Blink Blink Blink Blink Blink
  currentMillis = millis();
  if (currentMillis - previousMillis > interval / divisor) {
    // save the last time you blinked the LED
    previousMillis = currentMillis;
    digitalWrite(run_lt,  !digitalRead(run_lt)); // change state of run light
    
    if (!(ManualMode)) { digitalWrite(pb_lt, !digitalRead(pb_lt)); }
  }

} // loop()
// loop()   ------- LOOP ------- LOOP ------- LOOP ------- LOOP ------- LOOP ------- LOOP ------- LOOP ------- LOOP
