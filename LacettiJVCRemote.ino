//Ref:
//https://github.com/ronnied/holden-viva-jvc/blob/master/HoldenVivaJVC/HoldenVivaJVC.ino 
//https://pastebin.com/fXbScxV4 
//https://github.com/kprega/Clio2JVC/blob/master/libraries/JvcRadio/commands_dfs.h

// Protocol specifications:
//   Pulse width                    527.5 µs
//   Pulse interval for sending 0  1055.0 µs (HIGH for 1 pulse width, LOW for 1 pulse width)
//   Pulse interval for sending 1  2110.0 µs (HIGH for 1 pulse width, LOW for 3 pulse widths)
//   Note: since the delayMicroseconds() function accepts only unsigned integers, we're using a pulse width of 527 µs
// Data packets are constructed as follows:
//   HEADER     always LOW (1 pulse width), HIGH (16 pulse widths), LOW (8 pulse widths)
//   START BIT  always 1
//   ADDRESS    7-bits (0x00 to 0x7F), send LSB first, always 0x47 for JVC KD-R531, probably the same for all JVC car radio's
//   COMMAND    7-bits (0x00 to 0x7F), send LSB first, see next section for list of known commands for JVC KD-R531
//   STOP BITS  always 1, 1
//   Note: the ADDRESS, COMMAND and STOP BITS are repeated 3 times to ensure the radio properly receives them.
// Known commands for JVC KD-R531:
//   HEX   DEC  BIN(7b)  FUNCTION
//   0x04    4  0000100  Volume +
//   0x05    5  0000101  Volume -
//   0x08    8  0001000  Source cycle
//   0x0D   13  0001101  Equalizer preset cycle
//   0x0E   14  0001110  Mute toggle / Play/pause toggle
//   0x12   18  0010010  Tuner Search + / Track + (and Manual Tune + / Fast Forward with press & hold)
//   0x13   19  0010011  Tuner Search - / Track - (and Manual Tune - / Fast Rewind with press & hold)
//   0x14   20  0010100  Tuner Preset + / USB Folder +
//   0x15   21  0010101  Tuner Preset - / USB Folder -
//   0x37   55  0110111  UNKNOWN, appears to be a sort of reset as well as a display test
//   0x58   88  1011000  UNKNOWN, displays 'SN WRITING' where WRITING is blinking
//  #define VOL_UP         0x04  // Volume up
//  #define VOL_DOWN       0x05  // Volume down
//  #define SOURCE         0x08  // Toggle source
//  #define EQUALIZER      0x0D  // Equalizer
//  #define MUTE           0x0E  // Mute
//  #define TRACK_FORW     0x12  // Track forward
//  #define TRACK_BACK     0x13  // Track backward
//  #define FOLDER_FORW    0x14  // Track forward hold
//  #define FOLDER_BACK    0x15  // Track backward hold
//  #define VOICE_CONTROL  0x1A  // Voice control
//  #define BTCALL         0x1B  // Answer bluetooth call (Not tested yet)
//  #define POWER          0x80  // Power on/off (didn't work for KD-X342BT or KW-R510)
//  #define UNKNOWN1       0x37  // Unknown function 1 (LCD test)
//  #define UNKNOWN2       0x58  // Unknown function 2 (displays SN WRITING, then SN NG)


#include <avr/power.h>

// Configuration
#pragma GCC optimize ("-Ofast")                                                               // Max compiler optimisation level.
#define DEBUG_MODE     0                                                                      // Enable Serial. Uses lots of power
#define VOL_UP         0x04                                                                   // Volume up
#define VOL_DOWN       0x05                                                                   // Volume down
#define SOURCE         0x08                                                                   // Toggle source
#define MUTE           0x0E                                                                   // Mute
#define TRACK_FORW     0x12                                                                   // Track forward
#define OUTPUTPIN      10                                                                     // D10, Connect optocoupler input through a limiting resistor to this pin
#define INPUTPIN	     0                                                                      // A0, Connect steering signal to this pin
#define KEY_INTERVAL   5                                                                      // Throttle input (ms)
#define PULSEWIDTH     527                                                                    // Pulse width (µs)
#define ADDRESS        0x47                                                                   // Address that the radio responds to

unsigned long timer;

void setup() {
  pinMode(OUTPUTPIN, OUTPUT);                                                                 // Set the proper pin as output
  digitalWrite(OUTPUTPIN, LOW);                                                               // Output LOW to make sure optocoupler is off
  disable_peripherals();
  #if DEBUG_MODE
    Serial.begin(38400);
    while(!Serial);
    Serial.println("Ready to receive");
  #endif
  timer = millis();
}


void loop() {
  int Key = GetInput();                                                                       // If any buttons are being pressed the GetInput() function will return the appropriate command code

  if (Key) {  						                                                                    // If no buttons are being pressed the function will have returned 0 and no command will be sent
    if (millis() - timer >= KEY_INTERVAL){     
      SendCommand(Key);
      timer = millis();
    }
  }
}


int GetInput(void) {
  int s = analogRead(INPUTPIN);
  if (s > 1021)
    return VOL_DOWN;
  if(s > 985 && s < 995)
    return VOL_UP;
  if(s > 940 && s < 951)
    return TRACK_FORW;
  if(s > 875 && s < 885)
    return SOURCE;
  if (s > 755 && s < 765)
    return MUTE;
  return 0;
}


void SendValue(unsigned char value) {
  unsigned char tmp = 1;
  for (int i = 0; i < sizeof(value) * 8 - 1; i++) {
    if (value & tmp)  // Do a bitwise AND on the value and tmp
      SendOne();
    else
      SendZero();
    tmp = tmp << 1; // Bitshift left by 1
  }
}
 
void SendCommand(int value) {                                                                   // Send a command to the radio, including the header, start bit, address and stop bits
  #if DEBUG_MODE
    Serial.print("Car->Adapter: ");
    Serial.println(value);
  #endif
  Preamble();                                                                                   // Send signals to precede a command to the radio                                                                      
  SendValue(ADDRESS);                                                                           // Send the address
  SendValue(value);                                                                             // Send the command
  Postamble();                                                                                  // Send signals to follow a command to the radio
}
 
// Signals to transmit a '0' bit
void SendZero() {
  digitalWrite(OUTPUTPIN, HIGH);                                                                // Output HIGH for 1 pulse width
  delayMicroseconds(PULSEWIDTH);
  digitalWrite(OUTPUTPIN, LOW);                                                                 // Output LOW for 1 pulse width
  delayMicroseconds(PULSEWIDTH);
}
 
// Signals to transmit a '1' bit
void SendOne() {
  digitalWrite(OUTPUTPIN, HIGH);                                                                // Output HIGH for 1 pulse width
  delayMicroseconds(PULSEWIDTH);
  digitalWrite(OUTPUTPIN, LOW);                                                                 // Output LOW for 3 pulse widths
  delayMicroseconds(PULSEWIDTH * 3);
}
 
// Signals to precede a command to the radio
void Preamble() {                                                                               // HEADER: always LOW (1 pulse width), HIGH (16 pulse widths), LOW (8 pulse widths)
  digitalWrite(OUTPUTPIN, LOW);                                                                 // Make sure output is LOW for 1 pulse width, so the header starts with a rising edge
  delayMicroseconds(PULSEWIDTH * 1);
  digitalWrite(OUTPUTPIN, HIGH);                                                                // Start of header, output HIGH for 16 pulse widths
  delayMicroseconds(PULSEWIDTH * 16);
  digitalWrite(OUTPUTPIN, LOW);                                                                 // Second part of header, output LOW 8 pulse widths
  delayMicroseconds(PULSEWIDTH * 8);
  
  // START BIT: always 1
  SendOne();
}
 
// Signals to follow a command to the radio
void Postamble() {
  // STOP BITS: always 1
  SendOne();
  SendOne();
}

void disable_peripherals(){
  power_usart0_disable();                                                                       // Disable UART
  power_usart1_disable();                                                                       
  power_twi_disable();                                                                          // Disable I2C
  power_spi_disable();                                                                          // Disable SPI
  power_timer1_disable();                                                                       // Disable unused timers. 0 still on.
  power_timer2_disable();
  power_timer3_disable();
  #if !DEBUG_MODE
    delay(4000);
    power_usb_disable();                                                                        // After radio boots up, disable the USB module. 4s should allow sketch upload.
  #endif
}
