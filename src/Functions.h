#include <avr/sleep.h>
#include <avr/wdt.h>
#include <U8x8lib.h>
#include <arduino-timer.h>

//PHYSICAL PINS
#define DISPLAY_POWER_PIN 9
#define SCL_PIN SCL
#define SDA_PIN SDA
#define BUZZER_PIN 8
#define BUTTON_PIN 2

//TIME CONSTANTS
#define IDLE_DURATION 60
#define STUDY_MINUTE 25
#define BREAK_MINUTE 5
#define LONG_BREAK_MINUTE 15
#define ALL_SECOND 0

//DEVICES INITIALIZATIONS
U8X8_SSD1306_128X32_UNIVISION_SW_I2C u8x8(SCL_PIN, SDA_PIN, U8X8_PIN_NONE);
auto timer = timer_create_default();

enum wdt_time
{
  SLEEP_15MS,
  SLEEP_30MS,
  SLEEP_60MS,
  SLEEP_120MS,
  SLEEP_250MS,
  SLEEP_500MS,
  SLEEP_1S,
  SLEEP_2S,
  SLEEP_4S,
  SLEEP_8S,
  SLEEP_FOREVER
};

enum alert_duration
{
  SHORT_BEEP = 1,
  LONG_BEEP = 3
};

//DIFFERENT STATES OF THE MACHINE
enum states
{
  STUDY,
  BREAK,
  LONGBREAK,
  TOBREAK,
  TOLONGBREAK,
  TOSTUDY,
};

//FLAGS FOR DISPLAY INITIALIZATION
bool timerInit = false;
bool displayOn = false;

//FLAGS FOR TIMER
bool timerExpired = false;
bool timerPaused = false;

//FLAGS FOR DISPLAYING
bool isMinChanged = false;
bool isSecChanged = false;
bool isStateChanged = false;

//FLAG FOR USER INPUT
volatile bool buttonPressed = false;

//VARIABLES FOR STATE, TIMER, IDLE DISPLAY, POMODORO
states STATE = TOSTUDY;
int minute, second;
int displayIdleCount = 0;
int pomoCounter = 0;

//FUNCTION DECLARATIONS
bool update_sec(void *);
ISR(WDT_vect);
void powerDown(uint8_t time);
void deBounce();
void buttonInt();
void initTimer();
void turnDisplayOn();
void displayTime();
void turnDisplayOff();
void updateScreen();
bool isButtonPressed();
void rest();
bool proceedToNextState();
void alert(alert_duration);

//THIS FUNCTION RUNS EVERYTIME 'timer.tick()' IS CALLED
bool update_sec(void *)
{
  second--;

  if (second == 0 && minute == 0) //If both second and minutes are zero; raise the timeExpired flag
    timerExpired = true;

  if (second < 0) //If second is less than 0; decrement minute
  {
    second = 59;
    minute--;
    isMinChanged = true;
  }
  isSecChanged = true;
  updateScreen();
  return true;
}

//CODE STOLEN FROM https://github.com/cbm80amiga/HX1230_CountdownTimer
ISR(WDT_vect)
{
  wdt_disable();
}

//CODE STOLEN FROM https://github.com/cbm80amiga/HX1230_CountdownTimer
void powerDown(uint8_t time)
{
  ADCSRA &= ~(1 << ADEN); // turn off ADC
  if (time != SLEEP_FOREVER)
  { // use watchdog timer
    wdt_enable(time);
    WDTCSR |= (1 << WDIE);
  }
  set_sleep_mode(SLEEP_MODE_PWR_DOWN); // most power saving
  cli();
  sleep_enable();
  sleep_bod_disable();
  sei();
  sleep_cpu();
  // ... sleeping here
  sleep_disable();
  ADCSRA |= (1 << ADEN); // turn on ADC
}

//CODE STOLEN FROM https://gammon.com.au/forum/?id=11488&reply=1#reply1
void debounce()
{
  unsigned long now = millis();
  do
  {
    if (digitalRead(2) == LOW)
      now = millis();
  } while (digitalRead(2) == LOW || (millis() - now) <= 20);
}

//CODE STOLEN FROM https://github.com/cbm80amiga/HX1230_CountdownTimer
void buttonInt()
{
  buttonPressed = true;
  sleep_disable();
}

//FUNCTION INITIALIZE TIMER BASED ON STATE
void initTimer()
{
  timerExpired = false; //set time expired flag : false
  timerPaused = false;  //set timer paused flag : false
  if (STATE == TOSTUDY)
  {
    second = 5; //ALL_SECOND;
    minute = 0; //STUDY_MINUTE;
  }

  if (STATE == TOBREAK)
  {
    second = 2; //ALL_SECOND;
    minute = 0; //BREAK_MINUTE;
  }

  if (STATE == TOLONGBREAK)
  {
    second = 3; //ALL_SECOND;
    minute = 0; //LONG_BREAK_MINUTE;
  }

  isMinChanged = isSecChanged = isStateChanged = true; //Raise flags to update display
  updateScreen();                                      //Update the screen
  timerInit = true;                                    //Timer is initialized
}

//FUNCTION TO TURN THE DISPLAY ON
void turnDisplayOn()
{
  digitalWrite(DISPLAY_POWER_PIN, HIGH); //Giving power to the display
  u8x8.begin();
  u8x8.setPowerSave(0);
  u8x8.setFont(u8x8_font_profont29_2x3_r);
  isMinChanged = isSecChanged = isStateChanged = true;
  updateScreen(); //Update the screen
  displayOn = true;
}

//FUNCTION TO TURN DISPLAY OFF
void turnDisplayOff()
{
  digitalWrite(DISPLAY_POWER_PIN, LOW); //Cutting power of the display
  displayOn = false;                    //Display on flag to false
  powerDown(SLEEP_FOREVER);             //Sleep till user input
}

//FUNCTION TO UPDATE THE SCREEN
void updateScreen()
{
  //If minute change flag is raised, update minute on the screen
  if (isMinChanged)
  {
    String buff = minute < 10 ? "0" + String(minute) + ":" : String(minute) + ":"; //Prefix 0 if value is a single digit
    u8x8.setCursor(0, 0);
    u8x8.print(buff);
    isMinChanged = false;
  }

  //If second change flag is raised, update second on the screen
  if (isSecChanged)
  {
    String buff = second < 10 ? "0" + String(second) : String(second); //Prefix 0 if value is a single digit
    u8x8.setCursor(6, 0);
    u8x8.print(buff);
    isSecChanged = false;
  }

  //If state change flag is raised, update state and number of pomodoros on the screen
  if (isStateChanged)
  {
    String buff;
    if (STATE == STUDY || STATE == TOSTUDY)
      buff = "ST";
    else if (STATE == BREAK || STATE == TOBREAK)
      buff = "BR";
    else if (STATE == LONGBREAK || STATE == TOLONGBREAK)
      buff = "LB";

    u8x8.setFont(u8x8_font_8x13B_1x2_r);
    u8x8.setCursor(14, 0);
    u8x8.print(buff);
    buff = pomoCounter < 10 ? "0" + String(pomoCounter) : String(pomoCounter); //Prefix 0 if value is a single digit
    u8x8.setCursor(14, 2);
    u8x8.print(buff);
    u8x8.setFont(u8x8_font_profont29_2x3_r);
    isStateChanged = false;
  }
}

//FUNCTION TO CHECK IF THERE IS ANY USER INPUT (BUTTON PRESS)
bool isButtonPressed()
{
  if (buttonPressed) //If button press flag is raised
  {
    debounce(); //Debounce the button
    displayIdleCount = 0;
    buttonPressed = false;
    return true;
  }
  else
    return false;
}

//FUNCTION TO LET THE MACHINE GO TO REST BETWEEN STATE IF THERE IS NO USER INPUT
void rest()
{
  if (displayOn)
  {
    powerDown(SLEEP_1S); //Sleep for 1 second
    displayIdleCount++;
    if (displayIdleCount >= IDLE_DURATION)
    {                       //If being idle for IDLE_DURATION
      displayIdleCount = 0; //reset the idle counter and
      turnDisplayOff();     //turn display off
    }
  }
}

//FUNCTION TO CHECK IF IT CAN GO TO NEXT STATE (IF THERE IS USER INPUT)
bool proceedToNextState()
{
  if (!timerInit)
  {
    initTimer();
  }
  if (isButtonPressed())
  {                    //If there is user input (button is pressed)
    if (!displayOn)    //if display is off
      turnDisplayOn(); //turn on display
    else               //if display is on
      return true;     //permission go to the next state
  }
  rest();
  return false;
}

//FUNCTION TO SOUND THE BUZZER
void alert(alert_duration dur)
{
  for (int i = 0; i < dur * 2; i++)
  {
    digitalWrite(BUZZER_PIN, HIGH);
    delay(15 * dur);
    digitalWrite(BUZZER_PIN, LOW);
    delay(200 * dur);
    if (isButtonPressed())
    {
      buttonPressed = true;
      return;
    }
  }
  buttonPressed = false;
}