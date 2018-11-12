/* Encoder Library - TwoKnobs Example
 * http://www.pjrc.com/teensy/td_libs_Encoder.html
 *
 * This example code is in the public domain.
 */

#include <Arduino.h>
#include <U8g2lib.h>
#include <Encoder.h>
#include <Bounce.h>
// #include <MIDI.h>

#ifdef U8X8_HAVE_HW_SPI
#include <SPI.h>
#endif
#ifdef U8X8_HAVE_HW_I2C
#include <i2c_t3.h>
#endif

bool isMIDI = false;
bool isMonome = true;

// U8g2 Contructor
U8G2_SH1106_128X64_NONAME_1_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE);

// MIDI CHANNEL
const byte midiChannel = 1;       // The MIDI Channel to send the commands over

// MIDI_CREATE_DEFAULT_INSTANCE();

String deviceID  = "arc";

// HOW MANY ENCODERS?
const byte numberEncoders = 4;   // The number of encoders
const byte numberButtons = numberEncoders;   // The number of buttons

// ENCODER PIN SETUP - use teensy digital pin numbers
// Avoid using pins with LEDs attached
// (pin1, pin2,);
Encoder enc01(2, 3);
Encoder enc02(5, 6);
Encoder enc03(8, 9);
Encoder enc04(11, 12);

Encoder *encoders[numberEncoders] {
  &enc01,  &enc02,  &enc03,  &enc04
 };

 // BUTTONS
Bounce button0 = Bounce(1, 10);
Bounce button1 = Bounce(4, 10);
Bounce button2 = Bounce(7, 10);
Bounce button3 = Bounce(10, 10);

Bounce *buttons[numberButtons] {
  &button0,  &button1,  &button2,  &button3
 };

// SET CC NUMBERS FOR EACH ENCODER
int encoderCCs[] {16,17,18,19};

static uint8_t led_array[numberEncoders][64];

long knobs[numberEncoders] {};
long buttonval[numberButtons] {};
long encposition[]={-999, -999, -999, -999};

int _multiplier = 5;
int _stepSize = 2;
unsigned long _pauseLength = 1000;
int _lastENCread[numberEncoders] {0,0,0,0};
int _ENCcounter[numberEncoders] {0,0,0,0};
unsigned long _lastENCreadTime[numberEncoders] = {millis(),millis(),millis(),millis()};

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
void DrawBox(uint8_t x, uint8_t y, uint8_t w, uint8_t h){
  u8g2.drawBox(x, y, w, h);
}

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

void writeInt(uint8_t value) {
   Serial.write(value);           // standard is to write out the 8 bit value on serial
}
uint8_t readInt() {
  uint8_t val = Serial.read();
  //Serial2.write(val); // send to serial 2 pins for debug
  return val; 
}

void processSerial() {
  uint8_t identifierSent;                     // command byte sent from controller to matrix
  uint8_t deviceAddress;                      // device address sent from controller
  uint8_t dummy;                              // for reading in data not used by the matrix
  uint8_t intensity = 15;                     // default full led intensity
  uint8_t readX, readY, readN, readA;                       // x and y values read from driver
  uint8_t i, x, y, z;


  identifierSent = Serial.read();             // get command identifier: first byte of packet is identifier in the form: [(a << 4) + b]
                                              // a = section (ie. system, key-grid, digital, encoder, led grid, tilt)
                                              // b = command (ie. query, enable, led, key, frame)
  
  //Serial2.write(identifierSent);    // send to serial 2 pins for debug                  
  
  // monome serial protocol documentation: https://monome.org/docs/serial.txt
  
  switch (identifierSent) {
    case 0x00:                                // system/query - bytes: 1 - [0x00]
      writeInt((uint8_t)0x00);                // action: response, 0x00
      writeInt((uint8_t)0x01);                // id?
      writeInt((uint8_t)0x01);                // id response?
      break;

    case 0x01:                                // system / query ID - bytes: 1 - [0x01]
      writeInt((uint8_t)0x01);                // action: response, 0x01
       
      for (i = 0; i < 32; i++) {              // has to be 32
        if (i < deviceID.length()) {
          Serial.print(deviceID[i]);
        } 
        else {
          Serial.print('\0');
        }
      }
      
      break;

    case 0x02:                                // system / write ID - bytes: 33 - [0x02, d0..31]
      for (i = 0; i < 32; i++) {
        deviceID[i] = Serial.read();          // write ID (text string)
      }                                       // response: confirms written id, 0x01
      break;

    case 0x03:
      writeInt((uint8_t)0x02);                // system / request grid offset - bytes: 1 - [0x03]
      // writeInt(0);                         // 
      //writeInt((uint8_t)0x00);                // 
      //writeInt((uint8_t)0x00);                // action: response, 0x02
      break;

    case 0x04:                                // system / set grid offset - bytes: 4 - [0x04, n, x, y]
      dummy = readInt();                      // grid number
      readX = readInt();                      // x offset - an offset of 8 is valid only for 16 x 8 monome
      readY = readInt();                      // y offset - an offset is invalid for y as it's only 8
      //if(NUM_KEYS > 64 && readX == 8) offsetX = 8; 
      break;

    case 0x05:
      writeInt((uint8_t)0x03);                // system / request grid size
      writeInt(0);
      writeInt(0);
      break;

    case 0x06:
      readX = readInt();                      // system / set grid size - ignored
      readY = readInt();
      break;

    case 0x07:
      break;                                  // I2C get addr (scan) - ignored

    case 0x08:
      deviceAddress = readInt();              // I2C set addr - ignored
      dummy = readInt();
      break;

    case 0x0F:
      writeInt((uint8_t)0x0F);                // query firmware version
      //Serial.print(serialNum);
      
      break;

    // 0x5x are encoder
    case 0x50:    // /prefix/enc/delta n d
      //bytes: 3
      //structure: [0x50, n, d]
      //n = encoder number
      //  0-255
      //d = delta
      //  (-128)-127 (two's comp 8 bit)
      //description: encoder position change
      break;
      
    case 0x51:    // /prefix/enc/key n d (d=0 key up)
      //bytes: 2
      //structure: [0x51, n]
      //n = encoder number
      //  0-255
      //description: encoder switch up
      break;
      
    case 0x52:    // /prefix/enc/key n d (d=1 key down)
      //bytes: 2
      //structure: [0x52, n]
      //n = encoder number
      //  0-255
      //description: encoder switch down
      break;

    // 0x6x are analog in, 0x70 are analog out
    // 0x80 are tilt
    
    // 0x90 variable 64 LED ring 
    case 0x90:
      //pattern:  /prefix/ring/set n x a
      //desc:   set led x of ring n to value a
      //args:   n = ring number
      //      x = led number
      //      a = value (0-15)
      //serial:   [0x90, n, x, a]
      readN = readInt();
      readX = readInt();
      readA = readInt();
      led_array[readN][readX] = readA;
      break;
     
    case 0x91:
      //pattern:  /prefix/ring/all n a
      //desc:   set all leds of ring n to a
      //args:   n = ring number
      //      a = value
      //serial:   [0x91, n, a]
      readN = readInt();
      readA = readInt();
      for (int q=0; q<64; q++){
        led_array[readN][q]=readA;
      }
      break;
      
    case 0x92:
      //pattern:  /prefix/ring/map n d[32]
      //desc:   set leds of ring n to array d
      //args:   n = ring number
      //      d[32] = 64 states, 4 bit values, in 32 consecutive bytes
      //      d[0] (0:3) value 0
      //      d[0] (4:7) value 1
      //      d[1] (0:3) value 2
      //      ....
      //      d[31] (0:3) value 62
      //      d[31] (4:7) value 63
      //serial:   [0x92, n d[32]]
      readN = readInt();
      for (y = 0; y < 64; y++) {
          if (y % 2 == 0) {                    
            intensity = readInt();
            if ( (intensity >> 4 & 0x0F) > 0) {  // even bytes, use upper nybble
              led_array[readN][y] = (intensity >> 4 & 0x0F);
            }
            else {
              //led_array[readN][y]=0;
            }
          } else {                              
            if ((intensity & 0x0F) > 0 ) {      // odd bytes, use lower nybble
              led_array[readN][y] = (intensity & 0x0F);
            }
            else {
              //led_array[readN][y]=0;
            }
          }
      }
      break;

    case 0x93:
      //pattern:  /prefix/ring/range n x1 x2 a
      //desc:   set leds inclusive from x1 and x2 of ring n to a
      //args:   n = ring number
      //      x1 = starting position
      //      x2 = ending position
      //      a = value
      //serial:   [0x93, n, x1, x2, a]
      readN = readInt();
      readX = readInt();  // x1
      readY = readInt();  // x2
      readA = readInt();
      memset(led_array[readN],0,sizeof(led_array[readN]));
      
      if (readX < readY){
        for (y = readX; y < readY; y++) {
          led_array[readN][y] = readA;
        }
      }else{
        // wrapping?
        for (y = readX; y < 64; y++) {
          led_array[readN][y] = readA;
        }
        for (x = 0; x < readY; x++) {
          led_array[readN][x] = readA;
        }
      }
      
      //note:   set range x1-x2 (inclusive) to a. wrapping supported, ie. set range 60,4 would set values 60,61,62,63,0,1,2,3,4. 
      // always positive direction sweep. ie. 4,10 = 4,5,6,7,8,9,10 whereas 10,4 = 10,11,12,13...63,0,1,2,3,4 
     break;
 
    default:
      break;
    }
}
// SETUP
void setup() {
  // setup buttons
  pinMode(1, INPUT_PULLUP);
  pinMode(4, INPUT_PULLUP);
  pinMode(7, INPUT_PULLUP);
  pinMode(10, INPUT_PULLUP);
 
  Serial.begin(115200);
//  MIDI.begin(midiChannel);
  u8g2.begin();
  //u8g2.setAutoPageClear(0);
  
  //Serial.println("Encoders:");

} //END SETUP

// LOOP
void loop() {
  // flip display
    //u8g2.setDisplayRotation(U8G2_R2);
    //u8g2.setFlipMode(0);

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

  // START FOR # ENCODERS LOOP
  for (byte i = 0; i < numberEncoders; i++) {
    int encvalue = encoders[i]->read();    // read encoder value

    // if MIDI 
    
    if (isMIDI) {
      //Serial.println("is midi");
      // reset enc to 0 or 127 if out of range
      if (encvalue < 0) {
          encoders[i]->write(0);
          encvalue = 0;
      }
      if (encvalue > 127) {
          encoders[i]->write(127);
          encvalue = 127;
      }
      
      //did an encoder move?
      if (encvalue != encposition[i]) {
        Serial.print("Encoder changed: ");
        Serial.print(encoderCCs[i]);
        Serial.print("=");
        Serial.print(encvalue);
        Serial.println();
        encposition[i] = knobs[i];
  
        // send MIDI
        // usbMIDI.sendControlChange(encoderCCs[i], constrain(encvalue, 0, 127), midiChannel);
      }
    }
    
    // if MONOME 
    
    if (isMonome) {
         knobs[i] = encvalue;
        
        //did an encoder move?
        if (encvalue != 0) {
          
          int thisknob = encoderVelocity(i, encvalue, _multiplier, _stepSize, _pauseLength);
          
          //knobs[i] = encvalue;
          //Serial.print("Enc ");
          //Serial.print(i);
          //Serial.print(" = ");
          //Serial.print(knobs[i]);
          //Serial.println();
          
          // do stuff
          
          // write to libmonome
          writeInt(0x50); // send encoder delta
          writeInt(i);
          writeInt(constrain(encvalue, -127, 127));
         
          // then reset encoder to 0
          encoders[i]->write(0); 

        }
    }
  } // END FOR # ENCODERS LOOP
   
   // draw stuff to i2c display
    
    u8g2.firstPage(); do{
     for (int j=0; j<numberEncoders; j++){ 
        for (int q=0; q<64; q++){
          if (led_array[j][q] > 0){
            u8g2.setDrawColor(1);
            DrawBox((q*2), (j*15)+1, 2, 15); // draw white box
            u8g2.setDrawColor(2);
            DrawBox((q*2), (j*15)+1, 2, 16-led_array[j][q]); //
            
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
      

   if (Serial.available() > 0) {
      do { processSerial();  } 
      while (Serial.available() > 16);
    }
   
   //delay(1); // do we need to delay?
   
} //END LOOP
