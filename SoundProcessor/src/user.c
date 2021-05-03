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
#include "arm_math.h"

#define THREADFREQ 500   // frequency in Hz
#define MAGNUM 512   // number of magnitude values
#define PLOTMAX 80
#define PLOTMIN 0
#define SAMPLELENGTH 1024 // number of samples to collect before calculating RMS (may overflow if greater than 4104)
#define FILTERSIZE 512

// runs each thread 2 ms
//uint32_t Count0;   // number of times Task0 loops
uint32_t Count1;   // number of times Task1 loops
uint32_t Count2;   // number of times Task2 loops
uint32_t Count3;   // number of times Task2 loops

//---------------- Global variables shared between tasks ----------------
uint32_t Time;              // elasped time in ?100? ms units
uint32_t mag[MAGNUM];	// array to hold all calculated magnitude values
int32_t dBArray[MAGNUM];
float32_t magnitudeArr[MAGNUM];
int16_t SoundArray[SAMPLELENGTH];
float32_t SoundBufferIn[SAMPLELENGTH];
float32_t SoundBufferOut[SAMPLELENGTH];
uint16_t SoundData;         // raw data sampled from the microphone
int32_t dBAvg;
int32_t rawAvg;
int32_t freqDb;
uint32_t rawRMS;
uint32_t avgFreq;
uint32_t bin;
//int ReDrawAxes = 0;         // non-zero means redraw axes on next display task
int32_t NewData;  // true when new numbers to display on top of LCD
int32_t LCDmutex ; // exclusive access to LCD
//// testing rfft function
arm_rfft_fast_instance_f32 fft_inst; // rfft fast instance structure
float32_t maxVal;
uint32_t maxInd;


int32_t h[FILTERSIZE]; // coefficient (imaginary part) after discrete fourier transform

int timeTest;


//color constants
#define BGCOLOR     LCD_BLACK
#define AXISCOLOR   LCD_ORANGE
#define SOUNDCOLOR  LCD_CYAN
#define TOPTXTCOLOR LCD_WHITE
#define VALUECOLOR	LCD_RED

//// Utility Functions
void drawaxes(void){
	uint32_t max = PLOTMAX;
	uint32_t min = PLOTMIN;
	BSP_LCD_Drawaxes(AXISCOLOR, BGCOLOR, "Frequency", "Magnitude", SOUNDCOLOR, "", 0, max, min-20); 
	return;
}

float32_t imaginary_abs(float32_t real, float32_t imag) {
	return sqrtf(real*real+imag*imag);
}

// Newton's method
// s is an integer
// sqrt(s) is an integer
uint32_t sqrt32(uint32_t s){
uint32_t t;   // t*t will become s
int n;             // loop counter
  t = s/16+1;      // initial guess
  for(n = 16; n; --n){ // will finish
    t = ((t*t+s)/t)/2;
  }
  return t;
}

void call_FFT(void){
	static int32_t dBsum = 0;	
	// call function to process fft
	arm_rfft_fast_f32(&fft_inst, SoundBufferIn, SoundBufferOut, 0);
	int counter = 0;
	// array with real numbers for decibels
	for(int i = 2; i < SAMPLELENGTH; i+=2){
		// convert from time to frequency domain
		dBArray[counter] = (uint32_t)(20*log10f(imaginary_abs(SoundBufferOut[i], SoundBufferOut[i+1])));
		//magnitudeArr[counter] = imaginary_abs(SoundBufferOut[i], SoundBufferOut[i+1]);
		magnitudeArr[counter] = (20*log10f(imaginary_abs(SoundBufferOut[i], SoundBufferOut[i+1])));
		
		// save real numbers in array
		dBsum = dBsum + dBArray[counter];
		counter++;
	}
	// get index of max magnitude value
	arm_max_f32(magnitudeArr, 1024, &maxVal, &maxInd);
	int binFreq = (125000/SAMPLELENGTH);
	bin = (uint32_t)binFreq; // for display
	avgFreq = maxInd*binFreq;
	// adjust offset
	int offs = avgFreq/MAGNUM;
	avgFreq = (maxInd-offs)*binFreq;
	dBAvg = dBsum/MAGNUM - 20; // account for the negative values
	dBsum = 0;
	
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
// periodic even thread, called every 1ms
void Task0(void){ // periodic even thread
	static int32_t rawSum = 0;
	static int time = 0; // units of microphone sampling rate
	BSP_Microphone_Input(&SoundData);
	SoundArray[time] = SoundData;
	// store raw sound data in buffer
	float32_t voltage = (float32_t)SoundData; // input is voltage * 100
  SoundBufferIn[time] = voltage;
	rawSum = rawSum + (int32_t)SoundData;
	
	// increment time counter
	time = time + 1;
	// if SoundBuffer is full
	if (time == SAMPLELENGTH){
			// call function to process fft
			//call_FFT();
			rawAvg = rawSum/SAMPLELENGTH;
			rawSum = 0;
			for(int i = 0; i < SAMPLELENGTH; i++){
				rawSum = rawSum + (SoundArray[i] - rawAvg)*(SoundArray[i]-rawAvg);
			}
			rawRMS = sqrt32(rawSum/SAMPLELENGTH);
			rawSum = 0;
			time = 0; // start writing back into beginning of array (MACQ)
	}
}

// plot display categories
void Task1(void){
	BSP_LCD_DrawString(0, 0, "dB", TOPTXTCOLOR);
	BSP_LCD_DrawString(0, 1, "RMS", TOPTXTCOLOR);
	BSP_LCD_DrawString(10, 0, "Freq", TOPTXTCOLOR);
	BSP_LCD_DrawString(10, 1, "Bin", TOPTXTCOLOR);
}

// Plot array - magnitude over frequency
// x axis can be 0-99 units long
void Task2(void){
 // Count2 = 0;
	// draw magnitude
	drawaxes();
		int i = 0;
		int32_t val = 0;
		while (i < MAGNUM){
			//val = (int32_t) mag[i];
			val = dBArray[i];
			BSP_LCD_PlotPoint(val, SOUNDCOLOR);
			BSP_LCD_PlotIncrement();
			i++;
		}
}

// update numerical values on LCD
void Task3(void){
	BSP_LCD_SetCursor(3, 0); BSP_LCD_OutUDec4(dBAvg,VALUECOLOR);
	BSP_LCD_SetCursor(3, 1); BSP_LCD_OutUDec4(rawRMS,VALUECOLOR);
	BSP_LCD_SetCursor(15, 0); BSP_LCD_OutUDec4(avgFreq,VALUECOLOR);
	BSP_LCD_SetCursor(15, 1); BSP_LCD_OutUDec4(bin,VALUECOLOR);
}


int main(void){
  OS_Init();            // initialize, disable interrupts
	Task0_Init();    // microphone init
	//arm_rfft_fast_init_f32(&fft_inst,1024);
	rfft_fast_init_1024_f32(&fft_inst);
	BSP_RGB_Init(0, 0, 0);
	BSP_Buzzer_Init(0);
	BSP_LCD_Init();
  BSP_LCD_FillScreen(BSP_LCD_Color565(0, 0, 0));
	Time = 0;
	//for(int i = 0; i < 100; i++){
	while(1){
		// take sample
		for(int i = 0; i < 44100; i++){
			Task0();
		}
		call_FFT();
		Task1(); // write on top
		Task2(); // update plot
		Task3(); // update numerical values
		
	}
	//return 0;
}
