


#include "avr/io.h"                    // included for timer
#include "avr/interrupt.h"             // included for interrupt


//
///// Universal Variables /////
//
   boolean parkFlag;                   // used for parking
   boolean darkFlag;                   // used to keep the panel west 'till dark
   byte lightVals[25];                  // light value array
   int ct = 0;                         // variable used for counting timer interupts
   int ctl = 0;                        // variable used for counting array values



//
///// Assembly Functions & Variables /////
//
extern "C" {
   void initializePanel();
   void operatePanel();
   void reset();                       // assembly method to move panel to east switch
   byte readWest(byte);                // assembly method to return the west sensor value
   byte readEast(byte);                // assembly method to return the east sensor value
   byte west, east;                    // variables to store east and west sensor data
   byte westDigit;                     // variable used for storing the assembly west switch pressed
}



//
// If there is any serial input, read it into the
// given array; the array MUST be at least 32 chars long
// - returns true if a string is read in, false otherwise
// (note: 9600baud (bits per second) is slow so I need
//  to have delays so that I don't go too fast)
//
boolean readUserCommand(char cmdString[]){
   if (!Serial.available())
      return false;
   delayMicroseconds(5000); // allow serial to catch up
   int i=0;
   while (i < 31) {
      cmdString[i++] = Serial.read();
      delayMicroseconds(1000);
      if (!Serial.available())
         break; // quit when no more input
   }
   cmdString[i] = '\0'; // null-terminate the string
   while (Serial.available()) {
      Serial.read(); // flush any remain input (more than 31 chars)
      delayMicroseconds(1000);
   }
   return true;
}



//
///// Code that uses the functions /////
//
void setup() {
   Serial.begin(9600);
   initTimer();                        // initializes the timer
   parkFlag = false;                   
   initializePanel();                  // assembly method to initialize everything else
}

//
// In order to process user command AND operate the
// solar panel, the loop function needs to poll for
// user input and then invoke "operatePanel" to allow
// the panel operation code to do what it needs to 
// for ONE STEP. You should not do a continuous loop
// in your assembly code, but just cycle through
// checking everything you need to one time, and then
// returning back and allowing the loop function here
// continue.
//
void loop() {
   char cmd[32];

   //delayMicroseconds(100); // no need to go too fast (using a longer delay later on)

   cmd[0] = '\0'; // reset string to empty
   if (readUserCommand(cmd)) {
      // this if statement just shows that command strings
      // are being read; it serves no other useful purpose
      // and can be deleted or commented out
      Serial.print("User command is (");
      Serial.print(cmd);
      Serial.println(")");
   }
   // The conditions below recognize each individual
   // command string; all they do now is print, but you
   // will need to add code to do the proper actions
   if (!strcmp(cmd,"reset")) {
      resetPanel();
   } else if (!strcmp(cmd,"park")) {
      parkPanel();
   } else if (!strcmp(cmd,"query")) {
      queryPanel(lightVals);
   }
   
///// Code Added /////
   
   delay(200);                          // a decent speed for the panel to move
   
   west = readWest(5);                  //call assembly read sensor routine, A/D pin #5
   east = readEast(0);                  //call assembly read sensor routine, A/D pin #0
 
   /*
   Serial.print(" west sensor = ");     //debugging step to see sensor reading
   Serial.println(west,HEX);            //debugging step to see sensor reading
   Serial.print(" east sensor = ");     //debugging step to see sensor reading
   Serial.println(east,HEX);            //debugging step to see sensor reading
   //*/
   
   if (east < 15 && darkFlag == true && west < 15)   //if the light goes dark and west switch is pressed
     resetPanel();
  
   if (parkFlag == true)                // if the park flag is on, skips operate panel
     return;
  
   if (darkFlag == true)                // if the west switch is pressed skips operate panel
     return;

   operatePanel();                      // single step of moving the panel
      
   //Serial.println(westDigit);              // debugging step to see if assembly passed correct value
   if (westDigit == 1)                  // sets darkFlag true if assembly passes back a 1
      darkFlag = true;  
   
}// end of loop()


//************************************************************************************************
// Methods That were added
//***



//
///// Reset Panel /////
// 
// Method called when user commands "reset", some basic dialog was added
// to improve user interface. Assembly reset() is called to move the panel 
// to the east. parkFlag and darkFlag are reset allowing light tracking to 
// begin, westDigit is returned to 0 to prepare for next west switch event. 
// The light value array is emptied and array counter reset to make way for
// new values.
//
void resetPanel() {                                      
   Serial.println("   Resetting The Panel.");
   parkFlag = false;
   darkFlag = false;
   westDigit = 0;
   reset();
   lightVals[0] = NULL;
   ctl = 0;
   Serial.println("   Finished Reseting.");
   Serial.println("   Beginning Light Tracking.");
}// end resetPanel()



//
///// Park Panel /////
//
// Method called when user commands "park", some basic dialog was added to 
// improve user interface. Assembly reset() is called to move the panel
// to the east. parkFlag is set to true to prevent the panel from tracking 
// light and movingand to prevent arrayAdd from adding new light
// values to the array.
//
void parkPanel() {
   Serial.println("   Parking The Panel");
   parkFlag = true;
   reset();
   Serial.println("   Finished Parking.");
   Serial.println("   Awaiting Reset Command");
}//end parkPanel()



//
///// Query Panel /////
//
// Method called when user commands "query", some basic dialog was added to 
// improve user interface. A counter is initialized to print the array, if 
// the current value in the array is 0, it is assumed to be the end of the 
// stored values, so it will not continue to print 0's on the screen and 
// alerts the user. Once all values are printed ctl (the official array counter)
// is reset. As each value is printed to the screen it is immediately erased
// and set to zero before moving onto the next value. In that manner the array
// is emptied when the query command is called.
//
int queryPanel(byte lightVals[]) {
    int ctp=0;
    while(ctp<ctl){
      //if (lightVals[ctp] != 0){
         Serial.print("   Val at element ");
         Serial.print(ctp+1);
         Serial.print(" is ");
         Serial.println(lightVals[ctp]);
         //lightVals[ctp]=0;
         ctp++;
      //}else {

      //}// end if/else

    }//end while 
    Serial.print("   End of Values. Total = ");
    Serial.println(ctp);
    ctp = 25; 
}// end queryPanel()



//
///// Interrupt Service /////
//
// Timer1 interrupt, activated when Timer 1 reaches 1 second. Every second this
// interrupt increments the timer interrupt counter (ct). When the counter reaches
// 5 (5 Seconds) or higher arrayAdd() is called and the current west light value
// is added to lightVal[] (the array holding the light values) and ct is reset to 0.
//
ISR (TIMER1_COMPA_vect) {                        
   ct++;
   //Serial.println(ct);      // debugging step to see if timer counter is progessing correctly
   if (ct >= 5)                                   
      arrayAdd();
}// end Interrupt



// 
///// Timer1 Initialization /////
//
// Method used for initializing timer 1. This method is only called once, during
// the setup(). Timer 1 is initialed with a prescaler of 15624 which will cause an
// interrup every second.
//
void initTimer() {
   noInterrupts();
   DDRB |= 1<<PINB0;
   TCCR1A =0; TCCR1B=0; TCNT1=0;
   TCCR1B |= 1<<CS10 | 1<<CS12 | 1<<WGM12;
   TIMSK1 |= 1<<OCIE1A;
   OCR1A = 15624;
   interrupts();
}// end initTimer()



//
///// Array Add /////
//
// Method to add the currently stored light value in west to lightVals[] (the array
// holding the light values for query). Resets ct ( the timer counter) to 0, then 
// checks to see if parkFlag is set, if set it returns and does not store a new value
// if not set it checks the array index (ctl) to prevent the array from going out of 
// bounds. Because in queryPanel I use the condition that an element in the array is 
// zero it is to be taken as the last element in the array, arrayAdd checks whether 
// the west value is zero first, if not the value is added, if west is zero it adds 
// one then adds the new value to the array. Afterwards it increments the array index
// (ctl)
//
void arrayAdd() {
  
   ct=0;
   
   if (parkFlag == true) 
      return;
  
   if (ctl<25) {
      if (west != 0) 
         lightVals[ctl]=west;
      else 
         lightVals[ctl] = west+1;
   }else {
      ctl=24;
      for(int i=0; i<ctl; i++)
        lightVals[i] = lightVals[i+1];
        
      if (west != 0) 
         lightVals[ctl]=west;
      else 
         lightVals[ctl] = west+1;
   }
   ctl++;
   //Serial.println("Light Value Added.");       // debugging step to see when a light value is added
}// end arrayAdd()

