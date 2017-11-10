//---------------------------------------------------------------------------------------------------
//Este es parte del codigo original que descrive el funcionamiento del display (ignorar toda esta parte)
//---------------------------------------------------------------------------------------------------


// This simplified demo scrolls the text of the Jaberwoky poem directly from flash memory
// Full article at  http://wp.josh.com/2016/05/20/huge-scrolling-arduino-led-sign/

// Change this to be at least as long as your pixel string (too long will work fine, just be a little slower)

#define PIXELS 96*4  // Number of pixels in the string. I am using 4 meters of 96LED/M

// These values depend on which pins your 8 strings are connected to and what board you are using 
// More info on how to find these at http://www.arduino.cc/en/Reference/PortManipulation

// PORTD controls Digital Pins 0-7 on the Uno

// You'll need to look up the port/bit combination for other boards. 

// Note that you could also include the DigitalWriteFast header file to not need to to this lookup.

#define PIXEL_PORT  PORTD  // Port of the pin the pixels are connected to
#define PIXEL_DDR   DDRD   // Port of the pin the pixels are connected to


static const uint8_t onBits=0b11111110;   // Bit pattern to write to port to turn on all pins connected to LED strips. 
                                          // If you do not want to use all 8 pins, you can mask off the ones you don't want
                                          // Note that these will still get 0 written to them when we send pixels
                                          // TODO: If we have time, we could even add a variable that will and/or into the bits before writing to the port to support any combination of bits/values                                  

// These are the timing constraints taken mostly from 
// imperically measuring the output from the Adafruit library strandtest program

// Note that some of these defined values are for refernce only - the actual timing is determinted by the hard code.

#define T1H  814    // Width of a 1 bit in ns - 13 cycles
#define T1L  438    // Width of a 1 bit in ns -  7 cycles

#define T0H  312    // Width of a 0 bit in ns -  5 cycles
#define T0L  936    // Width of a 0 bit in ns - 15 cycles 

// Phase #1 - Always 1  - 5 cycles
// Phase #2 - Data part - 8 cycles
// Phase #3 - Always 0  - 7 cycles


#define RES 50000   // Width of the low gap between bits to cause a frame to latch

// Here are some convience defines for using nanoseconds specs to generate actual CPU delays

#define NS_PER_SEC (1000000000L)          // Note that this has to be SIGNED since we want to be able to check for negative values of derivatives

#define CYCLES_PER_SEC (F_CPU)

#define NS_PER_CYCLE ( NS_PER_SEC / CYCLES_PER_SEC )

#define NS_TO_CYCLES(n) ( (n) / NS_PER_CYCLE )







//------------------------------------------------------------------------------------
//  PINES USADOS EN ARDUINO
//------------------------------------------------------------------------------------

// led
#define led 13
// botones
#define Boton_Modo A0
#define Boton_Start A1
#define Boton_Stop A2
#define Boton_Reset A3
#define Sensor_vuelta A4


//------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------








//---------------------------------------------------------------------------------------------------
//Este es parte del codigo de funcionamiento del display (ignorar toda esta parte)
//---------------------------------------------------------------------------------------------------

// Sends a full 8 bits down all the pins, represening a single color of 1 pixel
// We walk though the 8 bits in colorbyte one at a time. If the bit is 1 then we send the 8 bits of row out. Otherwise we send 0. 
// We send onBits at the first phase of the signal generation. We could just send 0xff, but that mught enable pull-ups on pins that we are not using. 

/// Unforntunately we have to drop to ASM for this so we can interleave the computaions durring the delays, otherwise things get too slow.

// OnBits is the mask of which bits are connected to strips. We pass it on so that we
// do not turn on unused pins becuase this would enable the pullup. Also, hopefully passing this
// will cause the compiler to allocate a Register for it and avoid a reload every pass.

static inline void sendBitx8(  const uint8_t row , const uint8_t colorbyte , const uint8_t onBits ) {  
              
    asm volatile (


      "L_%=: \n\r"  
      
      "out %[port], %[onBits] \n\t"                 // (1 cycles) - send either T0H or the first part of T1H. Onbits is a mask of which bits have strings attached.

      // Next determine if we are going to be sending 1s or 0s based on the current bit in the color....
      
      "mov r0, %[bitwalker] \n\t"                   // (1 cycles) 
      "and r0, %[colorbyte] \n\t"                   // (1 cycles)  - is the current bit in the color byte set?
      "breq OFF_%= \n\t"                            // (1 cycles) - bit in color is 0, then send full zero row (takes 2 cycles if branch taken, count the extra 1 on the target line)

      // If we get here, then we want to send a 1 for every row that has an ON dot...
      "nop \n\t  "                                  // (1 cycles) 
      "out %[port], %[row]   \n\t"                  // (1 cycles) - set the output bits to [row] This is phase for T0H-T1H.
                                                    // ==========
                                                    // (5 cycles) - T0H (Phase #1)


      "nop \n\t nop \n\t "                          // (2 cycles) 
      "nop \n\t nop \n\t "                          // (2 cycles) 
      "nop \n\t nop \n\t "                          // (2 cycles) 
      "nop \n\t "                                   // (1 cycles) 

      "out %[port], __zero_reg__ \n\t"              // (1 cycles) - set the output bits to 0x00 based on the bit in colorbyte. This is phase for T0H-T1H
                                                    // ==========
                                                    // (8 cycles) - Phase #2
                                                    
      "ror %[bitwalker] \n\t"                      // (1 cycles) - get ready for next pass. On last pass, the bit will end up in C flag
                  
      "brcs DONE_%= \n\t"                          // (1 cycles) Exit if carry bit is set as a result of us walking all 8 bits. We assume that the process around us will tak long enough to cover the phase 3 delay

      "nop \n\t \n\t "                             // (1 cycles) - When added to the 5 cycles in S:, we gte the 7 cycles of T1L
            
      "jmp L_%= \n\t"                              // (3 cycles) 
                                                   // (1 cycles) - The OUT on the next pass of the loop
                                                   // ==========
                                                   // (7 cycles) - T1L
                                                   
                                                          
      "OFF_%=: \n\r"                                // (1 cycles)    Note that we land here becuase of breq, which takes takes 2 cycles

      "out %[port], __zero_reg__ \n\t"              // (1 cycles) - set the output bits to 0x00 based on the bit in colorbyte. This is phase for T0H-T1H
                                                    // ==========
                                                    // (5 cycles) - T0H

      "ror %[bitwalker] \n\t"                      // (1 cycles) - get ready for next pass. On last pass, the bit will end up in C flag
                  
      "brcs DONE_%= \n\t"                          // (1 cycles) Exit if carry bit is set as a result of us walking all 8 bits. We assume that the process around us will tak long enough to cover the phase 3 delay

      "nop \n\t nop \n\t "                          // (2 cycles) 
      "nop \n\t nop \n\t "                          // (2 cycles) 
      "nop \n\t nop \n\t "                          // (2 cycles)             
      "nop \n\t nop \n\t "                          // (2 cycles)             
      "nop \n\t "                                   // (1 cycles)             
            
      "jmp L_%= \n\t"                               // (3 cycles) 
                                                    // (1 cycles) - The OUT on the next pass of the loop      
                                                    // ==========
                                                    //(15 cycles) - T0L 
      
            
      "DONE_%=: \n\t"

      // Don't need an explicit delay here since the overhead that follows will always be long enough
    
      ::
      [port]    "I" (_SFR_IO_ADDR(PIXEL_PORT)),
      [row]   "d" (row),
      [onBits]   "d" (onBits),
      [colorbyte]   "d" (colorbyte ),     // Phase 2 of the signal where the actual data bits show up.                
      [bitwalker] "r" (0x80)                      // Alocate a register to hold a bit that we will walk down though the color byte

    );
                                  
    // Note that the inter-bit gap can be as long as you want as long as it doesn't exceed the reset timeout (which is A long time)
    
} 




// Just wait long enough without sending any bots to cause the pixels to latch and display the last sent frame

void show() {
  delayMicroseconds( (RES / 1000UL) + 1);       // Round up since the delay must be _at_least_ this long (too short might not work, too long not a problem)
}


// Send 3 bytes of color data (R,G,B) for a signle pixel down all the connected stringsat the same time
// A 1 bit in "row" means send the color, a 0 bit means send black. 

static inline void sendRowRGB( uint8_t row ,  uint8_t r,  uint8_t g,  uint8_t b ) {

  sendBitx8( row , g , onBits);    // WS2812 takes colors in GRB order
  sendBitx8( row , r , onBits);    // WS2812 takes colors in GRB order
  sendBitx8( row , b , onBits);    // WS2812 takes colors in GRB order
  
}

// This nice 5x7 font from here...
// http://sunge.awardspace.com/glcd-sd/node4.html

// Font details:
// 1) Each char is fixed 5x7 pixels. 
// 2) Each byte is one column.
// 3) Columns are left to right order, leftmost byte is leftmost column of pixels.
// 4) Each column is 8 bits high.
// 5) Bit #7 is top line of char, Bit #1 is bottom.
// 6) Bit #0 is always 0, becuase this pin is used as serial input and setting to 1 would enable the pull-up.

// defines ascii characters 0x20-0x7F (32-127)
// PROGMEM after variable name as per https://www.arduino.cc/en/Reference/PROGMEM

#define FONT_WIDTH 5      
#define INTERCHAR_SPACE 1
#define ASCII_OFFSET 0x20    // ASSCI code of 1st char in font array

const uint8_t Font5x7[] PROGMEM = {
0x00,0x00,0x00,0x00,0x00,//  
0x00,0x00,0xfa,0x00,0x00,// !
0x00,0xe0,0x00,0xe0,0x00,// "
0x28,0xfe,0x28,0xfe,0x28,// #
0x24,0x54,0xfe,0x54,0x48,// $
0xc4,0xc8,0x10,0x26,0x46,// %
0x6c,0x92,0xaa,0x44,0x0a,// &
0x00,0xa0,0xc0,0x00,0x00,// '
0x00,0x38,0x44,0x82,0x00,// (
0x00,0x82,0x44,0x38,0x00,// )
0x10,0x54,0x38,0x54,0x10,// *
0x10,0x10,0x7c,0x10,0x10,// +
0x00,0x0a,0x0c,0x00,0x00,// ,
0x10,0x10,0x10,0x10,0x10,// -
0x00,0x06,0x06,0x00,0x00,// .
0x04,0x08,0x10,0x20,0x40,// /
0x7c,0x8a,0x92,0xa2,0x7c,// 0
0x00,0x42,0xfe,0x02,0x00,// 1
0x42,0x86,0x8a,0x92,0x62,// 2
0x84,0x82,0xa2,0xd2,0x8c,// 3
0x18,0x28,0x48,0xfe,0x08,// 4
0xe4,0xa2,0xa2,0xa2,0x9c,// 5
0x3c,0x52,0x92,0x92,0x0c,// 6
0x80,0x8e,0x90,0xa0,0xc0,// 7
0x6c,0x92,0x92,0x92,0x6c,// 8
0x60,0x92,0x92,0x94,0x78,// 9
0x00,0x6c,0x6c,0x00,0x00,// :
0x00,0x6a,0x6c,0x00,0x00,// ;
0x00,0x10,0x28,0x44,0x82,// <
0x28,0x28,0x28,0x28,0x28,// =
0x82,0x44,0x28,0x10,0x00,// >
0x40,0x80,0x8a,0x90,0x60,// ?
0x4c,0x92,0x9e,0x82,0x7c,// @
0x7e,0x88,0x88,0x88,0x7e,// A
0xfe,0x92,0x92,0x92,0x6c,// B
0x7c,0x82,0x82,0x82,0x44,// C
0xfe,0x82,0x82,0x44,0x38,// D
0xfe,0x92,0x92,0x92,0x82,// E
0xfe,0x90,0x90,0x80,0x80,// F
0x7c,0x82,0x82,0x8a,0x4c,// G
0xfe,0x10,0x10,0x10,0xfe,// H
0x00,0x82,0xfe,0x82,0x00,// I
0x04,0x02,0x82,0xfc,0x80,// J
0xfe,0x10,0x28,0x44,0x82,// K
0xfe,0x02,0x02,0x02,0x02,// L
0xfe,0x40,0x20,0x40,0xfe,// M
0xfe,0x20,0x10,0x08,0xfe,// N
0x7c,0x82,0x82,0x82,0x7c,// O
0xfe,0x90,0x90,0x90,0x60,// P
0x7c,0x82,0x8a,0x84,0x7a,// Q
0xfe,0x90,0x98,0x94,0x62,// R
0x62,0x92,0x92,0x92,0x8c,// S
0x80,0x80,0xfe,0x80,0x80,// T
0xfc,0x02,0x02,0x02,0xfc,// U
0xf8,0x04,0x02,0x04,0xf8,// V
0xfe,0x04,0x18,0x04,0xfe,// W
0xc6,0x28,0x10,0x28,0xc6,// X
0xc0,0x20,0x1e,0x20,0xc0,// Y
0x86,0x8a,0x92,0xa2,0xc2,// Z
0x00,0x00,0xfe,0x82,0x82,// [
0x40,0x20,0x10,0x08,0x04,// (backslash)
0x82,0x82,0xfe,0x00,0x00,// ]
0x20,0x40,0x80,0x40,0x20,// ^
0x02,0x02,0x02,0x02,0x02,// _
0x00,0x80,0x40,0x20,0x00,// `
0x04,0x2a,0x2a,0x2a,0x1e,// a
0xfe,0x12,0x22,0x22,0x1c,// b
0x1c,0x22,0x22,0x22,0x04,// c
0x1c,0x22,0x22,0x12,0xfe,// d
0x1c,0x2a,0x2a,0x2a,0x18,// e
0x10,0x7e,0x90,0x80,0x40,// f
0x10,0x28,0x2a,0x2a,0x3c,// g
0xfe,0x10,0x20,0x20,0x1e,// h
0x00,0x22,0xbe,0x02,0x00,// i
0x04,0x02,0x22,0xbc,0x00,// j
0x00,0xfe,0x08,0x14,0x22,// k
0x00,0x82,0xfe,0x02,0x00,// l
0x3e,0x20,0x18,0x20,0x1e,// m
0x3e,0x10,0x20,0x20,0x1e,// n
0x1c,0x22,0x22,0x22,0x1c,// o
0x3e,0x28,0x28,0x28,0x10,// p
0x10,0x28,0x28,0x18,0x3e,// q
0x3e,0x10,0x20,0x20,0x10,// r
0x12,0x2a,0x2a,0x2a,0x04,// s
0x20,0xfc,0x22,0x02,0x04,// t
0x3c,0x02,0x02,0x04,0x3e,// u
0x38,0x04,0x02,0x04,0x38,// v
0x3c,0x02,0x0c,0x02,0x3c,// w
0x22,0x14,0x08,0x14,0x22,// x
0x30,0x0a,0x0a,0x0a,0x3c,// y
0x22,0x26,0x2a,0x32,0x22,// z
0x00,0x10,0x6c,0x82,0x00,// {
0x00,0x00,0xfe,0x00,0x00,// |
0x00,0x82,0x6c,0x10,0x00,// }
0x10,0x10,0x54,0x38,0x10,// ~
0x10,0x38,0x54,0x10,0x10,// 
};



// https://learn.adafruit.com/led-tricks-gamma-correction/the-quick-fix

const uint8_t PROGMEM gamma[] = {
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  1,  1,  1,  1,
    1,  1,  1,  1,  1,  1,  1,  1,  1,  2,  2,  2,  2,  2,  2,  2,
    2,  3,  3,  3,  3,  3,  3,  3,  4,  4,  4,  4,  4,  5,  5,  5,
    5,  6,  6,  6,  6,  7,  7,  7,  7,  8,  8,  8,  9,  9,  9, 10,
   10, 10, 11, 11, 11, 12, 12, 13, 13, 13, 14, 14, 15, 15, 16, 16,
   17, 17, 18, 18, 19, 19, 20, 20, 21, 21, 22, 22, 23, 24, 24, 25,
   25, 26, 27, 27, 28, 29, 29, 30, 31, 32, 32, 33, 34, 35, 35, 36,
   37, 38, 39, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 50,
   51, 52, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 66, 67, 68,
   69, 70, 72, 73, 74, 75, 77, 78, 79, 81, 82, 83, 85, 86, 87, 89,
   90, 92, 93, 95, 96, 98, 99,101,102,104,105,107,109,110,112,114,
  115,117,119,120,122,124,126,127,129,131,133,135,137,138,140,142,
  144,146,148,150,152,154,156,158,160,162,164,167,169,171,173,175,
  177,180,182,184,186,189,191,193,196,198,200,203,205,208,210,213,
  215,218,220,223,225,228,231,233,236,239,241,244,247,249,252,255 };

// Map 0-255 visual brightness to 0-255 LED brightness 
#define GAMMA(x) (pgm_read_byte(&gamma[x]))

// Send the pixels to form the specified char, not including interchar space
// skip is the number of pixels to skip at the begining to enable sub-char smooth scrolling

// TODO: Subtract the offset from the char before starting the send sequence to save time if nessisary
// TODO: Also could pad the begining of the font table to aovid the offset subtraction at the cost of 20*8 bytes of progmem
// TODO: Could pad all chars out to 8 bytes wide to turn the the multiply by FONT_WIDTH into a shift 
//====================================================================================================================
//====================================================================================================================
static inline void sendChar( uint8_t c ,  uint8_t skip , uint8_t r,  uint8_t g,  uint8_t b ) {

  const uint8_t *charbase = Font5x7 + (( c -' ')* FONT_WIDTH ) ; 

  uint8_t col=FONT_WIDTH; 

  while (skip--) {
      charbase++;
      col--;    
  }
  
  while (col--) {
      sendRowRGB( pgm_read_byte_near( charbase++ ) , r , g , b );
  }    
  
  col=INTERCHAR_SPACE;
  
  while (col--) {

    sendRowRGB( 0 , r , g , b );    // Interchar space
    
  }
  
}


// Show the passed string. The last letter of the string will be in the rightmost pixels of the display.
// Skip is how many cols of the 1st char to skip for smooth scrolling

//====================================================================================================================
//====================================================================================================================
static inline void Send_String( const char *s , uint8_t skip ,  const uint8_t r,  const uint8_t g,  const uint8_t b ) {

  unsigned int l=PIXELS/(FONT_WIDTH+INTERCHAR_SPACE); 

  sendChar( *s , skip ,  r , g , b );   // First char is special case becuase it can be stepped for smooth scrolling
  
  while ( *(++s) && l--) {

    sendChar( *s , 0,  r , g , b );

  }

  show();
}

//--------------------------------------------------------------------------------------------------------------
void showTextoSimple( uint8_t which , const char *pointsStr , uint8_t r , uint8_t g, uint8_t b) {

  clear();
  delay(500);

  for( uint8_t p = 0 ; p<strlen( pointsStr) ; p++ ) {

      cli();
      //Send_StringAlt( " " );
      //sendIcon( enimies , which , 0 , ENIMIES_WIDTH , r , g , b );
      for(uint8_t i=0; i<=p ;i++ ){        
        sendChar( *(pointsStr+i) , 0 ,r>>2 , g>>2 , b>>2 );     // Dim text slightly
      }
      sei();
      delay(100);
    
  }

  delay(1500);

  
}
//------------------------------------------------------------------------------------------------------------------
static inline void clear() {

  cli();
  for( unsigned int i=0; i< PIXELS; i++ ) {

    sendRowRGB( 0 , 0 , 0 , 0 );
  }
  sei();
  show(); 
}

static String Modos [] {"LABERINTO","SIGUELINEAS","VELOCISTAS","COMBATE"};
  byte Modo = 0;









//---------------------------------------------------------------------------------------------------
//El codigo propio para el uso como crono comienza a partir de aqui
//---------------------------------------------------------------------------------------------------





//------------------------------------------------------------------------------------------------------------------
// CRONO ESTANDAR (Usado para laverinto, y siguelineas)
//------------------------------------------------------------------------------------------------------------------
void crono01(){

  unsigned long currentTime=0;
  unsigned long time_of_tune=0;
  unsigned long tiempoV=0; 
   byte minu=0;
   byte Seg=0;
   int dec=0;
     
  clear();

  cli();
    Send_String( "  00:00:00" , 5 ,  10,  5, 5 ) ;
    sei();
    show();
    
 
  while (digitalRead(Boton_Start) != LOW) {}//Espera a que el boton star este sin pulsar...
  digitalWrite(led, 0);
  
  while (digitalRead(Boton_Start) == LOW &&digitalRead(Sensor_vuelta)!=LOW) {}; //Ahora espera a que se pulse el boton star/stop o pulsador/sensor
  digitalWrite(led, 1);
 
  while (digitalRead(Boton_Start) == HIGH || digitalRead(Sensor_vuelta)==LOW) {};
  digitalWrite(led, 0);

  
  //Se inicia unicamente con nivel alto en la entrada Boton_Start, pero puede ser detenido por el mismo boton o por el sensor de buelta (por si se usase en la meta)
  while (digitalRead(Boton_Start) == LOW&&digitalRead(Sensor_vuelta)!=LOW) {//Espera a que el boton star este sin pulsar...
  

    dec++;
    if(dec == 100){ //When it has passed 10 tenths of a second it count one second
      dec = 0;
      Seg++;
    }
    if(Seg == 60){ //When it has passed 60 seconds it count one minute
      Seg = 0;
      minu++;
    }
    if(minu == 60){ //When it has passed 60 minutes it count one hour
      minu = 0;

    }



    
  
    //formatear los datos MM:SS:DD  
    char crono[0];    
    sprintf(crono,"  %.2d:%.2d:%.2d",minu,Seg,dec);

    
    //Enviar al display
    cli();
    Send_String( crono , 5 ,  20,  10, 10 ) ;
    sei();
    
   
    //clear();
    show();
    delay(8); //este delay ajusta el tiempo a la medida "real" se puede ajustar mas usando la linea de abajo
    //delayMicroseconds(500);        // pauses for 50 microseconds      
   }

  
   while (digitalRead(Boton_Start)==1) delay(5);//asegurarse de no salir con ningun boton pulsado
   while (digitalRead(Boton_Modo)==0) {if(digitalRead(Boton_Reset)==1)crono01();} //Resetear el crono o salir al menu
}

//------------------------------------------------------------------------------------------------------------------
// Crono Velocistas       identico al alterior pero cuenta tres vueltas
//------------------------------------------------------------------------------------------------------------------
void crono02(){

  unsigned long currentTime=0;
  unsigned long time_of_tune=0;
  unsigned long tiempoV=0; 
  int Iniciado=0;
   byte minu=0;
   byte Seg=0;
   int dec=0;
   int tiempo_ultimo_paso=0;
   int vueltas_restantes=2;
     
  clear();

  cli();
    Send_String( "  00:00:00" , 5 ,  10,  5, 5 ) ;
    sei();
    show();
  //Serial.println("esperando Boton start");  
  //tiempoF = millis(); //Calculo de tiempo de vuelta
  delay(1000);
  
  while (digitalRead(Boton_Start) != LOW) {}//Espera a que el boton star este sin pulsar...
  digitalWrite(led, 0);
  
  while (digitalRead(Boton_Start) == LOW &&digitalRead(Sensor_vuelta)!=LOW) {}; //Ahora espera a que se pulse el boton star/stop
  digitalWrite(led, 1);
 
  while (digitalRead(Boton_Start) == HIGH ) {};
  digitalWrite(led, 0);




  while (digitalRead(Boton_Stop) == LOW) {//Condicion para parar la cuenta...


    dec++;
    if(dec == 100){ //When it has passed 10 tenths of a second it count one second
      dec = 0;
      Seg++;
      tiempo_ultimo_paso++;
    }
    if(Seg == 60){ //When it has passed 60 seconds it count one minute
      Seg = 0;
      minu++;
    }
    if(minu == 60){ //When it has passed 60 minutes it count one hour
      minu = 0;

    }


    


    
  
  
    char crono[0];    
    sprintf(crono,"  %.2d:%.2d:%.2d",minu,Seg,dec);
    
    
    cli();
    Send_String( crono , 5 ,  20,  10, 10 ) ;
    sei();
    
   
    //clear();
    show();
    delay(8); //Ajuste de riempo
    //delayMicroseconds(500);        // pauses for 50 microseconds    
    
    
    //esta linea es la que lee el sensor, se asegura de que haya transcurrido el minimo de 3seg y decrementa el numero de vueltas. si es 0 sale
    if (digitalRead(Sensor_vuelta)==LOW&& tiempo_ultimo_paso>3)  {tiempo_ultimo_paso=0;if (vueltas_restantes--==0){goto Fin2;}}
   }

Fin2:  
   while (digitalRead(Boton_Start)==1) delay(5);//asegurarse de no salir con ningun boton pulsado
   while (digitalRead(Boton_Modo)==0) {if(digitalRead(Boton_Reset)==1)crono02();} //reset crono o menu principal
}
//------------------------------------------------------------------------------------------------------------------
// Crono Cuenta a tras
//------------------------------------------------------------------------------------------------------------------
void crono03(){

  unsigned long currentTime=0;
  unsigned long time_of_tune=0;
  unsigned long tiempoV=0; 
  int   Terminado=0;
   byte minu=2; //Tiempo 2min <-----
   byte Seg=1; 
   int dec=1;
     
  clear();

    cli();
    Send_String( "  02:00:00" , 5 ,  10,  5, 5 ) ;
    sei();
    show();
    
  while (digitalRead(Boton_Start) != LOW) {}//Espera a que el boton star este sin pulsar...
  digitalWrite(led, 0);
  
  while (digitalRead(Boton_Start) == LOW ) {}; //Ahora espera a que se pulse el boton star/stop
  digitalWrite(led, 1);
 
  while (digitalRead(Boton_Start) == HIGH ) {};
  digitalWrite(led, 0);



  
  while (digitalRead(Boton_Stop) == LOW&Terminado==0) {//Continuar hasta que la cuenta llegue a cero Salvo que se pulse el boton --OJO--> Boton_Stop


    dec--;
    if(dec == 0){ //When it has passed 10 tenths of a second it count one second
      dec = 99;
      Seg--;
      if (Seg == 0&&minu==0){dec=0;Terminado=1;goto Fin;}
    }
    if(Seg == 0){ //When it has passed 60 seconds it count one minute
      Seg = 59;
      minu--;
    }
    if(minu == 00){ //When it has passed 60 minutes it count one hour
      minu = 0;

    }




    
Fin:  
  
    char crono[0];    
    
    sprintf(crono,"  %.2d:%.2d:%.2d",minu,Seg,dec);
    
    
    cli();
    Send_String( crono , 5 ,  20,  10, 10 ) ;
    sei();
    
   
    //clear();
    show();
    delay(8);
    //delayMicroseconds(500);        // pauses for 50 microseconds      
   }

  
   while (digitalRead(Boton_Start)==1) delay(5);//asegurarse de no salir con ningun boton pulsado
   while (digitalRead(Boton_Modo)==0) {if(digitalRead(Boton_Reset)==1)crono03();} 
}





















//----------------------------------------------------------------------------------------------------
//                  S E T U P
//----------------------------------------------------------------------------------------------------
void setup() {
  PIXEL_DDR |= onBits;         // Set used pins to output mode

  pinMode(Boton_Modo, INPUT);
  pinMode(Boton_Start, INPUT);
  pinMode(Boton_Stop, INPUT);
  pinMode(Boton_Reset, INPUT);  
  pinMode(Sensor_vuelta, INPUT);

  showTextoSimple( 4 , "Bricolabs" , 0x10 , 0x10 ,0x40 );
  showTextoSimple( 6 , "Modo:" , 0x10 , 0x10 ,0x10 );
  showTextoSimple( 4 , Modos[0].c_str() , 0x30 , 0x5 ,0x5 );

   // initialize serial: Se puede usar para debug, pero anula la ultima linea del display y envia datos sin sentido cuando edta contando
   //Serial.begin(115200);
}



  
//----------------------------------------------------------------------------------------------------
//                  L O O P
//----------------------------------------------------------------------------------------------------
void loop() {

  
  
  if (digitalRead(Boton_Modo)==1)  {
    if (Modo++>2)Modo=0;
    showTextoSimple( 4 , Modos[Modo].c_str() , 0x30 , 0x0 ,0x0 );
    while (digitalRead(Boton_Modo)==1) delay(5);
  }
  delay (100);
 
  if (digitalRead(Boton_Start)==1)  {
    while (digitalRead(Boton_Start)==1) delay(5);
    if (Modo==0)crono01();//Crono normal
    if (Modo==1)crono01();//TextoScroll(jabberText);
    if (Modo==2)crono02();//Crono velocistas
    if (Modo==3)crono03();//cuenta a tras
    
  }
  
// Boton_Modo A0
// Boton_Start A1
// Boton_Stop A2
// Boton_Reset A3

//Modos [] {"LABERINTO","SigueLineas","VELOCISTAS","COMBATE"};

//char[] chArray = "some characters";
//  String str(chArray); 

// string.c_str(). string a contchar


}




