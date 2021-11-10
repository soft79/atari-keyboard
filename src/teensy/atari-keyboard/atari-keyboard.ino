#include <stdarg.h>

#define DEBUG_SERIAL //If defined, dump info to serial for debugging in Arduino IDE

//Ribbon pins
#define RIBBON_01 33
#define RIBBON_02 34
#define RIBBON_03 35
#define RIBBON_04 36
#define RIBBON_05 37
#define RIBBON_06 38
#define RIBBON_07 39
#define RIBBON_08 19
#define RIBBON_09 32
#define RIBBON_10 31
#define RIBBON_11 30
#define RIBBON_12 29
#define RIBBON_13 28
#define RIBBON_14 27
#define RIBBON_15 26
#define RIBBON_16 25
#define RIBBON_17 24
//#define RIBBON_18 gnd
//#define RIBBON_19 n.c.
#define RIBBON_20 20
#define RIBBON_21 21
#define RIBBON_22 22
#define RIBBON_23 23
//#define RIBBON_24 V+

#define PIN_START RIBBON_20
#define PIN_SELECT RIBBON_21
#define PIN_OPTION RIBBON_22
#define PIN_RESET RIBBON_23
#define PIN_RSC RIBBON_09

//Allows us to disable the USB keyboard functionality
#define PIN_ENABLE_KB 18

//RAW POKEY ADDRESSES
//#define AKEY_SHFT 0x40
//#define AKEY_CTRL 0x80
//byte pinsOut[]  = { RIBBON_06, RIBBON_01, RIBBON_04, RIBBON_07, RIBBON_02, RIBBON_08, RIBBON_03, RIBBON_05 };
//byte pinsIn[] = { RIBBON_16, RIBBON_15, RIBBON_12, RIBBON_17, RIBBON_10, RIBBON_14, RIBBON_11, RIBBON_13, PIN_RSC };

byte pinsOut[]  = { RIBBON_01, RIBBON_02, RIBBON_03, RIBBON_04, RIBBON_05, RIBBON_06, RIBBON_07, RIBBON_08 };
byte pinsIn[] = { PIN_RSC, RIBBON_10, RIBBON_11, RIBBON_12, RIBBON_13, RIBBON_14, RIBBON_15, RIBBON_16, RIBBON_17 };
byte pinsFunc[] = { RIBBON_20, RIBBON_21, RIBBON_22, RIBBON_23 };

bool enableKeyboard = false;

int keyStates[sizeof(pinsOut)][sizeof(pinsIn)]; //Int, we store the sent keyCode, to allow overriding keyCodes with shift/ctrl 
bool funcStates[4]; //Function keys: Start,select,option,reset

int funcKeyCodes[] = { 
  KEY_F4, //Start
  KEY_F3, //Select
  KEY_F2, //Option
  KEY_F5  //Reset
};

//See also http://www.kbdedit.com/manual/low_level_vk_list.html
int keyCodes[sizeof(pinsOut)][sizeof(pinsIn)] =
{
  { KEY_PAUSE,         KEY_7,       0x00,          KEY_8, KEY_9,     KEY_0,          KEY_MINUS/*<*/,       KEY_EQUAL/*>*/ ,            KEY_BACKSPACE }, //Minus and Shift because there are no designated keys for < and > )
  { 0x00,              KEY_6,       0x00,          KEY_5, KEY_4,     KEY_3,          KEY_2,                KEY_1,                      KEY_ESC },
  { 0x00,              KEY_U,       0x00,          KEY_I, KEY_O,     KEY_P,          KEY_UP,               KEY_DOWN,                   KEY_RETURN },
  { 0x00,              KEY_Y,       0x00,          KEY_T, KEY_R,     KEY_E,          KEY_W,                KEY_Q,                      KEY_TAB },
  { MODIFIERKEY_CTRL,  0x00/*F1*/,  KEY_J,         KEY_K, KEY_L,     KEY_SEMICOLON,  KEY_LEFT,             KEY_RIGHT,                  0x00/*F2*/ },
  { 0x00,              0x00,        KEY_H,         KEY_G, KEY_F,     KEY_D,          KEY_S,                KEY_A,                      KEY_CAPS_LOCK},
  { 0x00,              KEY_N,       KEY_SPACE,     KEY_M, KEY_COMMA, KEY_PERIOD,     KEY_SLASH/*QstnM*/,   KEY_TILDE /*Invers*/,        0x00 }, //atari800 uses KEY_TILDE for inverse
  { MODIFIERKEY_SHIFT, 0x00/*F3*/,  KEY_F6/*HLP*/, KEY_B, KEY_V,     KEY_C,          KEY_X,                KEY_Z,                      0x00/*F4*/ },
};

#define CTRL_IS_PRESSED ((bool)keyStates[4][0])
#define SHIFT_IS_PRESSED ((bool)keyStates[7][0])

void setup() {
  Serial.begin(9600);
  pinMode(PIN_ENABLE_KB, INPUT_PULLUP);
  pinMode(PIN_START, INPUT_PULLUP);
      
  for (byte di=0; di< sizeof(pinsIn); di++) {
    pinMode(pinsIn[di], INPUT_PULLUP);
  }
  
  for (byte dq=0; dq< sizeof(pinsOut); dq++) {
    pinMode(pinsOut[dq], OUTPUT);
    digitalWrite(pinsOut[dq], HIGH);
  }
  
  for (byte dq = 0; dq < sizeof(pinsOut); dq++) {
    for (byte di = 0; di < sizeof(pinsIn); di++) {
      keyStates[dq][di] = 0;
    }
  }

  for (byte di=0; di< sizeof(pinsFunc); di++) {
    pinMode(pinsFunc[di], INPUT_PULLUP);
    funcStates[di] = false;
  }
}

void loop() {
  updateKeyboardEnabledDisabled();
  updateKeyboard();
  delay(10);
}

/**
 * Toggles whether keypresses are actually sent
 */
void updateKeyboardEnabledDisabled() {
  bool useKb = digitalRead(PIN_ENABLE_KB) == LOW;
  if (useKb != enableKeyboard) {
    Serial.print(useKb ? "KB ON" : "KB OFF");
    enableKeyboard = useKb;
  }  
}

/**
 * updateKeyboard()
 */
void updateKeyboard()
{
  //QWERTY keys
  for(byte dq = 0; dq < sizeof(pinsOut); dq++)
  {
    digitalWrite(pinsOut[dq], LOW);
    
    for(byte di = 0; di < sizeof(pinsIn); di++)
    {
      bool state = digitalRead(pinsIn[di]) == LOW;

      if(state == (bool)keyStates[dq][di]) continue; //State not changed

      int keyCode = getKeyCode(dq, di);

      keyStates[dq][di] = state ? keyCode : 0;

      notifyChange(keyCode, state);
    }
    digitalWrite(pinsOut[dq], HIGH);
  }

  //Function keys
  for (byte di=0; di< sizeof(pinsFunc); di++) {
    bool state = digitalRead(pinsFunc[di]) == LOW;
    
    if (state == funcStates[di]) continue;  //State not changed
    
    funcStates[di] = state;

    int keyCode = getFuncKeyCode(di);
    notifyChange(keyCode, state);
  }
}

/**
 * Get function (USB-)keycode 
 * di: 0=Start,1=Select,2=Option,3=Reset
 */
int getFuncKeyCode(byte di) {
  int keyCode = 0;
  if (di < sizeof(pinsFunc)) keyCode = funcKeyCodes[di];

  #ifdef DEBUG_SERIAL
    char txt[60]; 
    sprintf(txt, "FUNC%d Code:%d\r\n", di, keyCode);
    Serial.print(txt);
  #endif
  
  return keyCode;
}

/**
 * Get (USB-)keycode of the pressed key
 */
int getKeyCode(byte dq, byte di) {
  int keyCode = 0;
  if (dq < sizeof(pinsOut) && di < sizeof(pinsIn)) keyCode = keyCodes[dq][di];

  //Override keyCodes with certain modifiers
  //Release the same key, even if CTRL/SHIFT state was changed
  if ((bool)keyStates[dq][di]) {
    keyCode=keyStates[dq][di];
  } else if (SHIFT_IS_PRESSED || CTRL_IS_PRESSED) {
    if (dq == 0 && di == 6) keyCode = KEY_HOME;  //Atari CLEAR
    if (dq == 0 && di == 7) keyCode = KEY_INSERT; //Atari INSERT
  }

  #ifdef DEBUG_SERIAL
    char txt[60]; 
    sprintf(txt, "Q%d/I%d Code:%d SHIFT:%d CTRL:%d\r\n", dq, di, keyCode, SHIFT_IS_PRESSED, CTRL_IS_PRESSED);
    Serial.print(txt);
  #endif

  return keyCode;
}

/**
 * Notify key press/release, but only if keyboard is enabled
 */
void notifyChange(int keyCode, bool state) {
  if (!enableKeyboard || keyCode == 0) return;
  
  if (state){
    Keyboard.press(keyCode);
  }
  else
  {
    Keyboard.release(keyCode);
  }
}
