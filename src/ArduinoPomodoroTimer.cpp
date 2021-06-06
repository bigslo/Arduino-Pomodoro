#include "functions.h"

void setup()
{
  pinMode(DISPLAY_POWER_PIN, OUTPUT);
  pinMode(2, INPUT_PULLUP);
  pinMode(8, OUTPUT);

  attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), buttonInt, FALLING);
  timer.every(1000, update_sec);
  turnDisplayOn();
}

void loop()
{
  //----------------------STUDY STATE----------------------//
  if (STATE == STUDY)
  {
    if (!timerExpired && !timerPaused) //If timer is not expired or paused
    {
      if (isButtonPressed())
      {
        timerPaused = true; //If button is pressed, pause the timer
        return;
      }
      timer.tick(); //Counting down
    }
    else if (timerPaused) //If timer is paused
    {
      if (isButtonPressed()) //If button is pressed
      {                      //and
        if (!displayOn)      //if display is off
        {                    //then
          turnDisplayOn();   //turn the display back on
          return;
        }
        else                   //If display is still/already turned on
        {                      //then
          timerPaused = false; //resume the timer
          return;
        }
      }
      rest(); //Go rest
    }
    else //If timer is expired
    {
      timerInit = false; //Timer needs to be initialized
      alert(SHORT_BEEP); //sound the alarm
      pomoCounter++;     //increment the number of pomodoros
      if (pomoCounter != 0 && pomoCounter % 4 == 0)
        STATE = TOLONGBREAK; //Go to Long break state after every four study cycles
      else                   //else
        STATE = TOBREAK;     //go to regular break state
    }
  }

  //----------------------BREAK STATE----------------------//
  if (STATE == BREAK)
  {
    if (!timerExpired) //If timer is not expired
    {
      timer.tick(); //Counting down
    }
    else
    {
      timerInit = false; //Timer needs to be initialized
      alert(LONG_BEEP);  //sound the alarm
      STATE = TOSTUDY;   //go to staudy state
    }
  }

  //-------------------LONG BREAK STATE-------------------//
  if (STATE == LONGBREAK)
  {
    if (!timerExpired) //If timer is not expired
    {
      timer.tick(); //Counting down
    }
    else
    {
      timerInit = false; //Timer needs to be initialized
      alert(LONG_BEEP);  //sound the alarm
      STATE = TOSTUDY;   //go to staudy state
    }
  }

  //-------------------TOSTUDY STATE-------------------//
  if (STATE == TOSTUDY)
  {
    if (proceedToNextState()) //Check for button press
      STATE = STUDY;          //to proceed to the next state
  }

  //-------------------TOBREAK STATE-------------------//
  if (STATE == TOBREAK)
  {
    if (proceedToNextState()) //Check for button press
      STATE = BREAK;          //to proceed to the next state
  }

  //----------------TOLONGBREAK STATE----------------//
  if (STATE == TOLONGBREAK)
  {
    if (proceedToNextState()) //Check for button press
      STATE = LONGBREAK;      //to proceed to the next state
  }
}
