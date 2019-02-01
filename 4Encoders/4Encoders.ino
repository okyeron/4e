/* 4 Encoders
 *  
 *  hackable encoder controller
 *  first version desiged to emulate the monome arc 
 *  outputs serial monome protocol for arc encoders/buttons/led-rings  https://monome.org/docs/serial.txt
 *  use usb_names.c file to change emulated monome serial number
 *  
 *  or use for MIDI stuff like CC's 
 *  
 */

#include <Arduino.h>
#include <U8g2lib.h>
#include <Encoder.h>
#include <Bounce.h>
// #include <MIDI.h>
#include "MonomeSerial.h"


#ifdef U8X8_HAVE_HW_SPI
	#include <SPI.h>
#endif
#ifdef U8X8_HAVE_HW_I2C
	#include <i2c_t3.h>
#endif

// 1x8 or 2x4 expander
bool VERTICAL = false; // 2x4 configuration

#define MONOMEDEVICECOUNT 1
#define MONOMEARCENCOUDERCOUNT 8

MonomeSerial monomeDevices;
elapsedMillis monomeRefresh;
int arcValues[MONOMEARCENCOUDERCOUNT];


#define PIN_1 15  // 0
#define PIN_2 12  // 3
#define PIN_3 9  // 20
#define PIN_4 6  // 33
#define PIN_5 3
#define PIN_6 0
#define PIN_7 20
#define PIN_8 33

// ENCODER ASSIGNMENTS

// swap A/B to reverse encoder direction
#define ENC1A 16  // 5
#define ENC1B 17  // 4
#define ENC2A 30  // 2
#define ENC2B 14  // 1
#define ENC3A 10  // 22
#define ENC3B 11  // 21
#define ENC4A 7   // 31
#define ENC4B 8   // 32

#if VERTICAL // 1x8 layout
  #define ENC5A 5  // 5
  #define ENC5B 4  // 4
  #define ENC6A 2  // 2
  #define ENC6B 1  // 1
  #define ENC7A 22  // 22
  #define ENC7B 21  // 21
  #define ENC8A 31   // 31
  #define ENC8B 32   // 32
#else  // 2x4
  #define ENC5A 31  
  #define ENC5B 32 
  #define ENC6A 22 
  #define ENC6B 21 
  #define ENC7A 2 
  #define ENC7B 1 
  #define ENC8A 5  
  #define ENC8B 4   
#endif


// i2c ADDR for oled diplays
byte adr1 = (0x3D *2); //display is 0x3D
byte adr2 = (0x3C *2); //display 2 is 0x3C

// U8g2 Contructor
U8G2_SH1106_128X64_NONAME_1_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE);

// additional setup for 2 displays
#if VERTICAL // 1x8 layout
  U8G2_SH1106_128X64_NONAME_1_HW_I2C u8g2_2(U8G2_R2, /* reset=*/ U8X8_PIN_NONE); // U8G2_R2 is 180 degree rotation
#else // 2x4 layout
  U8G2_SH1106_128X64_NONAME_1_HW_I2C u8g2_2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE); // U8G2_R0 is normal rotation
#endif
//U8G2 *u8g2_array[] =  {&u8g2, &u8g2_2};


// MIDI CHANNEL
const byte midiChannel = 1;       // The MIDI Channel to send the commands over
// MIDI_CREATE_DEFAULT_INSTANCE();

// HOW MANY ENCODERS?
const byte numberEncoders = 8;   // The number of encoders
const byte numberButtons = numberEncoders;   // The number of buttons

// ENCODER PIN SETUP - use teensy digital pin numbers
// Avoid using pins with LEDs attached
// (pin1, pin2,);
Encoder enc01(ENC1A, ENC1B);
Encoder enc02(ENC2A, ENC2B);
Encoder enc03(ENC3A, ENC3B);
Encoder enc04(ENC4A, ENC4B);

Encoder enc08(ENC5A, ENC5B);
Encoder enc07(ENC6A, ENC6B);
Encoder enc06(ENC7A, ENC7B);
Encoder enc05(ENC8A, ENC8B);

Encoder *encoders[numberEncoders] {
  &enc01,  &enc02,  &enc03,  &enc04, &enc05,  &enc06,  &enc07,  &enc08
 };

 // BUTTONS (pin#, debounce time in milliseconds)
Bounce button0 = Bounce(PIN_1, 10);
Bounce button1 = Bounce(PIN_2, 10);
Bounce button2 = Bounce(PIN_3, 10);
Bounce button3 = Bounce(PIN_4, 10);
Bounce button4 = Bounce(PIN_8, 10);
Bounce button5 = Bounce(PIN_7, 10);
Bounce button6 = Bounce(PIN_6, 10);
Bounce button7 = Bounce(PIN_5, 10);

Bounce *buttons[numberButtons] {
  &button0,  &button1,  &button2,  &button3, &button4,  &button5,  &button6,  &button7
 };

//MIDI ONLY
// SET CC NUMBERS FOR EACH ENCODER
int encoderCCs[]{ 16, 17, 18, 19, 20, 21, 22, 23 };

// ENCODER SETUP

long knobs[numberEncoders] {};
long buttonval[numberButtons] {};
long encposition[]={-999, -999, -999, -999,-999, -999, -999, -999};

int _multiplier = 5;
int _stepSize = 2;
unsigned long _pauseLength = 1000;
int _lastENCread[numberEncoders] {0,0,0,0,0,0,0,0};
int _ENCcounter[numberEncoders] {0,0,0,0,0,0,0,0};
unsigned long _lastENCreadTime[numberEncoders] = {millis(),millis(),millis(),millis(),millis(),millis(),millis(),millis()};

// MATH FOR GUAGES
float gs_rad; //stores angle from where to start in radinats
float ge_rad; //stores angle where to stop in radinats

// example values for testing, use the values you wish to pass as argument while calling the function
byte cx=16; //x center
byte cy=25; //y center
byte radius=14; //radius
byte percent=80; //needle percent

void Drawgauge(int x, byte y, byte r, byte p, int v, int minVal, int maxVal) {
  int n=(r/100.00)*p; // calculate needle percent lenght
                float gs_rad=-3.141;
                float ge_rad=3.141;
                float i=((v-minVal)*(ge_rad-gs_rad)/(maxVal-minVal)+gs_rad);
                int xp = x+(sin(i) * n);
                int yp = y-(cos(i) * n);
                  u8g2.drawCircle(x,y,r, U8G2_DRAW_ALL );
                  u8g2.drawLine(x,y,xp,yp); 
                  u8g2.setFont(u8g2_font_5x8_tn);
                  u8g2.setFontPosCenter();
                  u8g2.setCursor(x-3,y+25);
                  u8g2.print(v);
}
void Drawbutton(int dx,byte dy,byte brad, int v){
  u8g2.drawCircle(dx,dy,brad,U8G2_DRAW_ALL );
}
void DrawBox1(uint8_t x, uint8_t y, uint8_t w, uint8_t h){
  u8g2.drawBox(x, y, w, h);
}
void DrawBox2(uint8_t x, uint8_t y, uint8_t w, uint8_t h){
  u8g2_2.drawBox(x, y, w, h);
}

// AM I USING THIS?
int encoderVelocity(int enc_id, int enc_val, int multiplier, int stepSize, int pauseLength) {
    int returnVal = 0;
    int changevalue = 1;
    
    if (enc_val > 0){
      returnVal = 1;
    } else if  (enc_val < 0){
      returnVal = -1;
    }
    if(returnVal != 0) {
      if(returnVal == _lastENCread[enc_id]) {
        _ENCcounter[enc_id]++;
        if((millis() - _lastENCreadTime[enc_id]) < _pauseLength) {
          changevalue = max((_ENCcounter[enc_id]*_multiplier)/_stepSize,1);
        }
        _lastENCreadTime[enc_id] = millis();  
      } else {
        _ENCcounter[enc_id]=0;
      }
      _lastENCread[enc_id] = returnVal;
     }
     return returnVal*changevalue;
}


// SETUP
void setup() {
	  // setup buttons
	  pinMode(PIN_1, INPUT_PULLUP); //INPUT_PULLUP
	  pinMode(PIN_2, INPUT_PULLUP);
	  pinMode(PIN_3, INPUT_PULLUP);
	  pinMode(PIN_4, INPUT_PULLUP);
  
	  pinMode(PIN_5, INPUT_PULLUP); //INPUT_PULLUP
	  pinMode(PIN_6, INPUT_PULLUP);
	  pinMode(PIN_7, INPUT_PULLUP);
	  pinMode(PIN_8, INPUT_PULLUP);

	  Serial.begin(115200);
//  Serial4.begin(115200); // send to serial 4 pins for debug
  
    // USB MIDI
    usbMIDI.setHandleNoteOn(myNoteOn);
    usbMIDI.setHandleNoteOff(myNoteOff);
    usbMIDI.setHandleControlChange(myControlChange);

  	//u8g2
  	u8g2.setI2CAddress(adr1);
  	u8g2_2.setI2CAddress(adr2);
  	u8g2.begin();
  	u8g2_2.begin();
    
  	Serial.println("--4Encoders--");
  
    monomeDevices.getDeviceInfo();

} 
//END SETUP


// LOOP
void loop() {
  	// flip display
    //u8g2.setDisplayRotation(U8G2_R2);
    //u8g2.setFlipMode(0);

    // Read USB MIDI
    while (usbMIDI.read()) {
        // controllers must call .read() to keep the queue clear even if they
        // are not responding to MIDI
    }


/*
  // FOR # BUTTONS LOOP
  for (byte z = 0; z < numberButtons; z++) {
    buttons[z]->update();
    if (buttons[z]->risingEdge()) { // release
      //  do release things
          // write to libmonome
          writeInt(0x51); // send encoder key up
          writeInt(z);
      buttonval[z] = 0;
      //Serial.print(z);
      //Serial.println(" released" );
    }
    if (buttons[z]->fallingEdge()) {  // press
      // do press things
         // write to libmonome
          writeInt(0x52); // send encoder key down
          writeInt(z);
      buttonval[z] = 1;
      //Serial.print(z);
      //Serial.println(" pressed" );
    }
  } // END FOR # BUTTONS LOOP
*/

  // START FOR # ENCODERS LOOP
  
  for (byte i = 0; i < numberEncoders; i++) {
    int encvalue = encoders[i]->read();    // read encoder value

      knobs[i] = encvalue;
        //did an encoder move?
        if (encvalue != 0) {
          // write to monome
 
			    monomeDevices.sendArcDelta(i, constrain(encvalue, -127, 127));
          //Serial.println(constrain(encvalue, -127, 127));
          //addArcEvent(i, constrain(encvalue, -127, 127));
         
          // then reset encoder to 0
          encoders[i]->write(0); 
        }

  } // END FOR # ENCODERS LOOP
  
  monomeDevices.poll();
  
   // draw stuff to i2c display
    
    u8g2.firstPage(); do{
     for (int j=0; j<4; j++){  // 1-4
        for (int q=0; q<64; q++){
        	//int k = led_array[j][q];
        	int index = q + (j << 6);
        	int k = monomeDevices.leds[index];
          if (k > 0){

            u8g2.setDrawColor(1);
            DrawBox1((q*2), (j*15)+1, 2, 15); // draw white box
            u8g2.setDrawColor(2);
            DrawBox1((q*2), (j*15)+1, 2, 16-k); //
            
            //led_array[j][q] = 0;          
          }
        }
        if (buttonval[j]){
         //Drawbutton((j*30)+30,5,5,j);
         }
     }
      
      //DrawBox(knobs[0], 2, 2, 12);
      //DrawBox(knobs[1], 16, 2, 12);
      //DrawBox(knobs[2], 30, 2, 12);
      //DrawBox(knobs[3], 44, 2, 12);
      //Drawgauge(cx,cy,radius,percent,constrain(knobs[0], 0, 127),0,127);
      //Drawgauge(cx*3,cy,radius,percent,constrain(knobs[1], 0, 127),0,127);
      //Drawgauge(cx*5,cy,radius,percent,constrain(knobs[2], 0, 127),0,127);
      //Drawgauge(cx*7,cy,radius,percent,constrain(knobs[3], 0, 127),0,127);
      
      } while ( u8g2.nextPage() );

    u8g2_2.firstPage(); do{
     for (int j=4; j<8; j++){  // 4-8
        for (int q=0; q<64; q++){
	        int index = q + (j << 6);
	        int k = monomeDevices.leds[index];
          if (k > 0){
            u8g2_2.setDrawColor(1);
            DrawBox2((q*2), ((j-4)*15)+1, 2, 15); // draw white box
            u8g2_2.setDrawColor(2);
            DrawBox2((q*2), ((j-4)*15)+1, 2, 16-k); //      
          }
        }
        if (buttonval[j]){
        }
       }
    } while ( u8g2_2.nextPage() );
      


    if (monomeRefresh > 50) {
        for (int i = 0; i < MONOMEDEVICECOUNT; i++) monomeDevices.refresh();
        monomeRefresh = 0;
    }

   
} //END LOOP


// MIDI NOTE/CC HANDLERS

void myNoteOn(uint8_t channel, uint8_t note, uint8_t velocity) {
    // When using MIDIx4 or MIDIx16, usbMIDI.getCable() can be used
    // to read which of the virtual MIDI cables received this message.
    usbMIDI.sendNoteOn(note, velocity, channel);

    Serial.print("Note On, ch=");
    Serial.print(channel, DEC);
    Serial.print(", note=");
    Serial.print(note, DEC);
    Serial.print(", velocity=");
    Serial.println(velocity, DEC);
}

void myNoteOff(uint8_t channel, uint8_t note, uint8_t velocity) {
    usbMIDI.sendNoteOff(note, 0, channel);

    Serial.print("Note Off, ch=");
    Serial.print(channel, DEC);
    Serial.print(", note=");
    Serial.print(note, DEC);
    Serial.print(", velocity=");
    Serial.println(velocity, DEC);

}

void myControlChange(byte channel, byte control, byte value) {
    usbMIDI.sendControlChange(control, value, channel);

    Serial.print("Control Change, ch=");
    Serial.print(channel, DEC);
    Serial.print(", control=");
    Serial.print(control, DEC);
    Serial.print(", value=");
    Serial.println(value, DEC);
}
