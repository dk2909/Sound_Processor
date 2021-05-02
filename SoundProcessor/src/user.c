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
#define PLOTMAX 100
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
	//OS_Wait(&LCDmutex);
	BSP_LCD_Drawaxes(AXISCOLOR, BGCOLOR, "Frequency", "Magnitude", SOUNDCOLOR, "", 0, max, min);
	//OS_Signal(&LCDmutex);
	//ReDrawAxes = 0;
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
		//h[counter] = SoundBufferOut[i+1];
	//	uint32_t real = (uint32_t)SoundBufferOut[i];
		//uint32_t imag = (uint32_t)abs(h[counter]);
	//	uint32_t magn = sqrt32(real*real+imag*imag);
		magnitudeArr[counter] = imaginary_abs(SoundBufferOut[i], SoundBufferOut[i+1]);
		
		// save real numbers in array
		dBsum = dBsum + dBArray[counter];
		counter++;
	}
	arm_max_f32(magnitudeArr, 1024, &maxVal, &maxInd);
	
	avgFreq = (uint32_t)(maxInd*122);
	dBAvg =  dBsum/MAGNUM;
	dBsum = 0;
	
	/*
	// filter with complex conjugates
	counter = 0;
	
	for(int i = 1; i < SAMPLELENGTH; i+=2){
		h[counter] = (int32_t)SoundBufferOut[i];
	}
	
	*/
	return;
}

/*
int16_t Data[FILTERSIZE*2]; // two copies
int16_t *Pt; // pointer to current
void Filter_Init(void){
	Pt = &Data[0];
}
// calculate one filter output
// called at sampling rate
// Input: new ADC data
// Output: filter output, DAC data
int16_t Filter_Calc(int16_t newdata){
	int i; 
	int32_t sum; 
	int16_t *pt;
	int16_t *apt;
	if(Pt == &Data[0]){
		Pt = &Data[FILTERSIZE-1]; // wrap
	} else{
		Pt--; // make room for data
	}
	*Pt = newdata; // two copies
	Pt = Pt+FILTERSIZE; 
	*Pt = newdata; // two copies
	pt = Pt; // copy of data pointer
	apt =  (int16_t *) h; // pointer to coefficients
	sum = 0;
	for(i=FILTERSIZE; i; i--){
		sum += (*pt)*(*apt);
		apt++;
		pt++;
	}
	return sum/16384;
}
*/

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
  //Count0 = 0;
	//////////
	static int32_t rawSum = 0;
	static int time = 0; // units of microphone sampling rate
	uint32_t firSum;
	BSP_Microphone_Input(&SoundData);
	SoundArray[time] = SoundData;
	// store raw sound data in buffer
	float32_t voltage = (float32_t)SoundData;
	//voltage = voltage/100;
  SoundBufferIn[time] = voltage;
	SoundArray[time] = SoundData;
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
			/*
			// apply FIR filter
			for(int i = 0; i < SAMPLELENGTH; i++){
				firSum = firSum + Filter_Calc(SoundArray[i]);
			}
			avgFreq = (uint32_t)(firSum/FILTERSIZE);
			*/
	}
	//return;
}
void Task1(void){
  //Count1 = 0;
		///////////
	//OS_Wait(&LCDmutex);
	BSP_LCD_DrawString(0, 0, "dB", TOPTXTCOLOR);
	BSP_LCD_DrawString(0, 1, "RMS", TOPTXTCOLOR);
	BSP_LCD_DrawString(10, 0, "Freq", TOPTXTCOLOR);
  //OS_Signal(&LCDmutex);
	/*
  while(1){
    Count1++;
    Profile_Toggle1();    // toggle bit
  }*/
	//return;
}

// Plot from array
// x axis can be 0-99 units long
void Task2(void){
 // Count2 = 0;
	// draw magnitude
	//Get_Mag();
	drawaxes();
	//while(1){
		// waiting for data to plot
		//OS_Wait(&NewData);
		//OS_Wait(&LCDmutex);
		int i = 0;
		int32_t val = 0;
		while (i < MAGNUM){
			//val = (int32_t) mag[i];
			val = dBArray[i];
			BSP_LCD_PlotPoint(val, SOUNDCOLOR);
			BSP_LCD_PlotIncrement();
			i++;
		}
		//OS_Signal(&LCDmutex);
		//i = 0;
		//BSP_LCD_SetCursor(7,  0); BSP_LCD_OutUDec4(SoundAvg,       TOPTXTCOLOR);
	//	Count2++;
   // Profile_Toggle2();    // toggle bit
	//}
		//return;
}

// update information on LCD
void Task3(void){
	BSP_LCD_SetCursor(5, 0); BSP_LCD_OutUDec4(dBAvg,VALUECOLOR);
	BSP_LCD_SetCursor(5, 1); BSP_LCD_OutUDec4(rawRMS,VALUECOLOR);
	BSP_LCD_SetCursor(16, 0); BSP_LCD_OutUDec4(avgFreq,VALUECOLOR);
}


int main(void){
  OS_Init();            // initialize, disable interrupts
  //Profile_Init();       // enable digital I/O on profile pins
	Task0_Init();    // microphone init
	//arm_rfft_fast_init_f32(&fft_inst,1024);
	rfft_fast_init_1024_f32(&fft_inst);
	//BSP_Button1_Init();
 // BSP_Button2_Init();
	BSP_RGB_Init(0, 0, 0);
	BSP_Buzzer_Init(0);
	BSP_LCD_Init();
  BSP_LCD_FillScreen(BSP_LCD_Color565(0, 0, 0));
	Time = 0;
	//for(int i = 0; i < 100; i++){
	while(1){
		// take sample
		for(int i = 0; i < 62500; i++){
			Task0();
			//BSP_Delay1ms(1);
		}
		call_FFT();
		Task1(); // write on top
		Task2(); // update plot
		Task3(); // update numerical values
		
	}
	
	//OS_InitSemaphore(&NewData, 0);  // 0 means no data
  //OS_InitSemaphore(&LCDmutex, 1); // 1 means free
	//OS_MailBox_Init();              // initialize mailbox used to send data
	//OS_AddPeriodicEventThreads(&Task0, 1);
  //OS_AddThreads(&Task1, &Task2);
 // OS_Launch(BSP_Clock_GetFreq()/THREADFREQ); // doesn't return, interrupts enabled in here
  //return 0;             // this never executes
}
