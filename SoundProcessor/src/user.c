//*****************************************************************************
// user.c
// Runs on LM4F120/TM4C123/MSP432
// An example user program that initializes the simple operating system
//   Schedule three independent threads using preemptive round robin
//   Each thread rapidly toggles a pin on profile pins and increments its counter
//   THREADFREQ is the rate of the scheduler in Hz

// Daniel Valvano
// February 8, 2016

/* This example accompanies the book
   "Embedded Systems: Real Time Interfacing to ARM Cortex M Microcontrollers",
   ISBN: 978-1463590154, Jonathan Valvano, copyright (c) 2016

   "Embedded Systems: Real-Time Operating Systems for ARM Cortex-M Microcontrollers",
   ISBN: 978-1466468863, , Jonathan Valvano, copyright (c) 2016
   Programs 4.4 through 4.12, section 4.2

 Copyright 2016 by Jonathan W. Valvano, valvano@mail.utexas.edu
    You may use, edit, run or distribute this file
    as long as the above copyright notice remains
 THIS SOFTWARE IS PROVIDED "AS IS".  NO WARRANTIES, WHETHER EXPRESS, IMPLIED
 OR STATUTORY, INCLUDING, BUT NOT LIMITED TO, IMPLIED WARRANTIES OF
 MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE APPLY TO THIS SOFTWARE.
 VALVANO SHALL NOT, IN ANY CIRCUMSTANCES, BE LIABLE FOR SPECIAL, INCIDENTAL,
 OR CONSEQUENTIAL DAMAGES, FOR ANY REASON WHATSOEVER.
 For more information about my classes, my research, and my books, see
 http://users.ece.utexas.edu/~valvano/
 */

#include <stdlib.h>
#include <stdint.h>
#include "os.h"
#include "../inc/BSP.h"
#include "../inc/profile.h"
#include <time.h>

#define THREADFREQ 500   // frequency in Hz
#define MAGNUM 512   // number of magnitude values
#define PLOTMAX 500
#define PLOTMIN 0
#define SOUNDRMSLENGTH 1024 // number of samples to collect before calculating RMS (may overflow if greater than 4104)

// runs each thread 2 ms
uint32_t Count0;   // number of times Task0 loops
uint32_t Count1;   // number of times Task1 loops
uint32_t Count2;   // number of times Task2 loops
uint32_t Count3;   // number of times Task2 loops

//---------------- Global variables shared between tasks ----------------
uint32_t Time;              // elasped time in ?100? ms units
uint32_t mag[MAGNUM];	// array to hold all calculated magnitude values
int16_t SoundArray[SOUNDRMSLENGTH];
uint16_t SoundData;         // raw data sampled from the microphone
int ReDrawAxes = 0;         // non-zero means redraw axes on next display task
int32_t NewData;  // true when new numbers to display on top of LCD
int32_t LCDmutex ; // exclusive access to LCD


//color constants
#define BGCOLOR     LCD_BLACK
#define AXISCOLOR   LCD_ORANGE
#define SOUNDCOLOR  LCD_CYAN
#define TOPTXTCOLOR LCD_WHITE

//// Utility Functions
void drawaxes(void){
	uint32_t max = PLOTMAX;
	uint32_t min = PLOTMIN;
	OS_Wait(&LCDmutex);
	BSP_LCD_Drawaxes(AXISCOLOR, BGCOLOR, "Time", "Magnitude", SOUNDCOLOR, "", 0, max, min);
	OS_Signal(&LCDmutex);
	//ReDrawAxes = 0;
}

void Get_Mag(void){
	// initialize random numbers based on time
	uint32_t magVal = 1;
	for(int i = 0; i < MAGNUM; i++){
		mag[i] = magVal;
		magVal +=25;
		if(magVal >= 500){
			magVal = 2;
		}
	}
	return;
}

// *********Task0_Init*********
// initializes microphone
// Task0 measures sound intensity
// Inputs:  none
// Outputs: none
void Task0_Init(void){
  BSP_Microphone_Init();
}

// Capture and store raw sound data
// periodic even thread
void Task0(void){ // periodic even thread
  Count0 = 0;
	//////////
	static int32_t soundSum = 0;
	static int time = 0; // units of microphone sampling rate
	BSP_Microphone_Input(&SoundData);
	soundSum = soundSum + (int32_t)SoundData;
  SoundArray[time] = SoundData;
	time = time + 1;
	if (time == SOUNDRMSLENGTH){
		time = 0;
	}
	/*
	while(1){
		Count0++;
		Profile_Toggle0();    // toggle bit
	}*/
}
void Task1(void){
  Count1 = 0;
		///////////
	OS_Wait(&LCDmutex);
	BSP_LCD_DrawString(6, 0, "Violine", TOPTXTCOLOR);
  OS_Signal(&LCDmutex);
  while(1){
    Count1++;
    Profile_Toggle1();    // toggle bit
  }
}
void Task2(void){
  Count2 = 0;
	////
	// draw axes
	drawaxes();
	//ReDrawAxes = 0;
	
  while(1){
    Count2++;
    Profile_Toggle2();    // toggle bit
  }
}

void Task3(void){
  Count3 = 0;
	/////////
	// draw magnitude
	Get_Mag();
	drawaxes();
	/*if(ReDrawAxes){
      drawaxes();
      ReDrawAxes = 0;
   }*/
	OS_Wait(&LCDmutex);
	int i = 0;
	int32_t val = 0;
	while (i < MAGNUM){
		val = (int32_t) mag[i];
		BSP_LCD_PlotPoint(val, SOUNDCOLOR);
		BSP_LCD_PlotIncrement();
		i++;
	}
	OS_Signal(&LCDmutex);
	
  while(1){
    Count3++;
    Profile_Toggle3();    // toggle bit
	}
}


int main(void){
  OS_Init();            // initialize, disable interrupts
  Profile_Init();       // enable digital I/O on profile pins
	Task0_Init();    // microphone init
	BSP_Button1_Init();
  BSP_Button2_Init();
	BSP_RGB_Init(0, 0, 0);
	BSP_Buzzer_Init(0);
	BSP_LCD_Init();
  BSP_LCD_FillScreen(BSP_LCD_Color565(0, 0, 0));
	Time = 0;
	//OS_InitSemaphore(&NewData, 0);  // 0 means no data
  OS_InitSemaphore(&LCDmutex, 1); // 1 means free
	//OS_MailBox_Init();              // initialize mailbox used to send data
	OS_AddPeriodicEventThreads(&Task0, 1);
  OS_AddThreads(&Task1, &Task2, &Task3);
  OS_Launch(BSP_Clock_GetFreq()/THREADFREQ); // doesn't return, interrupts enabled in here
  return 0;             // this never executes
}
