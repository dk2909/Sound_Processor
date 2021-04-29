// os.c
// Runs on LM4F120/TM4C123/MSP432
// A priority/blocking real-time operating system 
// Lab 4 starter file.
// Daniel Valvano
// March 25, 2016
// Hint: Copy solutions from Lab 3 into Lab 4
#include <stdint.h>
#include <stdlib.h>
#include <math.h>
#include "os.h"
#include "CortexM.h"
#include "BSP.h"
#include "../inc/tm4c123gh6pm.h"
#include "arm_math.h"

// function definitions in osasm.s
void StartOS(void);

#define NUMTHREADS  8        // maximum number of threads
#define NUMPERIODIC 2        // maximum number of periodic threads
#define STACKSIZE   100      // number of 32-bit words in stack per thread
struct tcb{
  int32_t *sp;       // pointer to stack (valid for threads not running
  struct tcb *next;  // linked-list pointer
//*FILL THIS IN****
	uint32_t Sleep; // nonzero if this thread is sleeping
	int32_t *blocked;// nonzero if blocked on this semaphore
	uint32_t Priority; // 0 is highest, 254 lowest
};
typedef struct tcb tcbType;
tcbType tcbs[NUMTHREADS];
tcbType *RunPt;
int32_t Stacks[NUMTHREADS][STACKSIZE];
void static runperiodicevents(void);

// ******** OS_Init ************
// Initialize operating system, disable interrupts
// Initialize OS controlled I/O: periodic interrupt, bus clock as fast as possible
// Initialize OS global variables
// Inputs:  none
// Outputs: none
void OS_Init(void){
  DisableInterrupts();
  BSP_Clock_InitFastest();// set processor clock to fastest speed
// perform any initializations needed, 
// set up periodic timer to run runperiodicevents to implement sleeping
	BSP_PeriodicTask_Init(&runperiodicevents,1000,0);  
}

void SetInitialStack(int i){
  // ****IMPLEMENT THIS**** 
  // **Same as Lab 2 and Lab 3****
 	tcbs[i].sp = &Stacks[i][STACKSIZE-16]; // thread stack pointer
	Stacks[i][STACKSIZE-1] = 0x01000000; // Thumb bit
	Stacks[i][STACKSIZE-3] = 0x14141414; // R14
	Stacks[i][STACKSIZE-4] = 0x12121212; // R12
	Stacks[i][STACKSIZE-5] = 0x03030303; // R3
	Stacks[i][STACKSIZE-6] = 0x02020202; // R2
	Stacks[i][STACKSIZE-7] = 0x01010101; // R1
	Stacks[i][STACKSIZE-8] = 0x00000000; // R0
	Stacks[i][STACKSIZE-9] = 0x11111111; // R11
	Stacks[i][STACKSIZE-10] = 0x10101010; // R10
	Stacks[i][STACKSIZE-11] = 0x09090909; // R9
	Stacks[i][STACKSIZE-12] = 0x08080808; // R8
	Stacks[i][STACKSIZE-13] = 0x07070707; // R7
	Stacks[i][STACKSIZE-14] = 0x06060606; // R6
	Stacks[i][STACKSIZE-15] = 0x05050505; // R5
	Stacks[i][STACKSIZE-16] = 0x04040404; // R4
}

//******** OS_AddThreads ***************
// Add eight main threads to the scheduler
// Inputs: function pointers to eight void/void main threads
//         priorites for each main thread (0 highest)
// Outputs: 1 if successful, 0 if this thread can not be added
// This function will only be called once, after OS_Init and before OS_Launch
int OS_AddThreads(void(*thread0)(void), uint32_t p0,
                  void(*thread1)(void), uint32_t p1,
                  void(*thread2)(void), uint32_t p2,
                  void(*thread3)(void), uint32_t p3,
                  void(*thread4)(void), uint32_t p4,
                  void(*thread5)(void), uint32_t p5,
                  void(*thread6)(void), uint32_t p6,
                  void(*thread7)(void), uint32_t p7){
// **similar to Lab 3. initialize priority field****
	int32_t status;
	status = StartCritical();
	tcbs[0].next = &tcbs[1];
	tcbs[1].next = &tcbs[2];
	tcbs[2].next = &tcbs[3];
	tcbs[3].next = &tcbs[4];
	tcbs[4].next = &tcbs[5];
	tcbs[5].next = &tcbs[6];
	tcbs[6].next = &tcbs[7];
	tcbs[7].next = &tcbs[0];		
										
	SetInitialStack(0); 
	Stacks[0][STACKSIZE-2] = (int32_t)(thread0); // PC
	tcbs[0].Priority = p0;
	tcbs[0].Sleep = 0;										
	SetInitialStack(1); 
	Stacks[1][STACKSIZE-2] = (int32_t)(thread1); // PC
	tcbs[1].Priority = p1;
	tcbs[1].Sleep = 0;									
	SetInitialStack(2); 
	Stacks[2][STACKSIZE-2] = (int32_t)(thread2); // PC
	tcbs[2].Priority = p2;
	tcbs[2].Sleep = 0;
	SetInitialStack(3); 
	Stacks[3][STACKSIZE-2] = (int32_t)(thread3); // PC
	tcbs[3].Priority = p3;
	tcbs[3].Sleep = 0;
	SetInitialStack(4); 
	Stacks[4][STACKSIZE-2] = (int32_t)(thread4); // PC
	tcbs[4].Priority = p4;
	tcbs[4].Sleep = 0;
	SetInitialStack(5); 
	Stacks[5][STACKSIZE-2] = (int32_t)(thread5); // PC
	tcbs[5].Priority = p5;
	tcbs[5].Sleep = 0;
	SetInitialStack(6); 	
	Stacks[6][STACKSIZE-2] = (int32_t)(thread6); // PC
	tcbs[6].Priority = p6;
	tcbs[6].Sleep = 0;
	SetInitialStack(7); 
	Stacks[7][STACKSIZE-2] = (int32_t)(thread7); // PC
	tcbs[7].Priority = p7;	
	tcbs[7].Sleep = 0;	
	
	/*	// set priority
	tcbs[0].Priority = p0;
	tcbs[1].Priority = p1;
	tcbs[2].Priority = p2;
	tcbs[3].Priority = p3;
	tcbs[4].Priority = p4;
	tcbs[5].Priority = p5;
	tcbs[6].Priority = p6;
	tcbs[7].Priority = p7;
	
	for (int i = 0; i < NUMTHREADS; i++){
		tcbs[i].Sleep = 0;
		tcbs[i].blocked = 0;
	}*/
																			
	RunPt = &tcbs[0]; // start at 0
	EndCritical(status);	 
  return 1;               // successful
}


void static runperiodicevents(void){
// ****IMPLEMENT THIS****
// **DECREMENT SLEEP COUNTERS
// In Lab 4, handle periodic events in RealTimeEvents
	/*tcbType *rpt;
	tcbType *rpth;
	rpt = RunPt;
	rpth = rpt; // header
	while (rpt->next != rpth){
		if ((rpt->Sleep) > 0){
			rpt->Sleep = (rpt->Sleep) - 1;
		}
		rpt = rpt->next;
	}*/
	for (int i = 0; i < NUMTHREADS; i++){
		if (tcbs[i].Sleep > 0){
			(tcbs[i].Sleep)--;
		}
	}
}
//******** OS_Launch ***************
// Start the scheduler, enable interrupts
// Inputs: number of clock cycles for each time slice
// Outputs: none (does not return)
// Errors: theTimeSlice must be less than 16,777,216
void OS_Launch(uint32_t theTimeSlice){
  STCTRL = 0;                  // disable SysTick during setup
  STCURRENT = 0;               // any write to current clears it
  SYSPRI3 =(SYSPRI3&0x00FFFFFF)|0xE0000000; // priority 7
  STRELOAD = theTimeSlice - 1; // reload value
  STCTRL = 0x00000007;         // enable, core clock and interrupt arm
  StartOS();                   // start on the first task
}
// runs every ms
void Scheduler(void){      // every time slice
// ****IMPLEMENT THIS****
// look at all threads in TCB list choose
// highest priority thread not blocked and not sleeping 
// If there are multiple highest priority (not blocked, not sleeping) run these round robin
	uint32_t max = 255; // max
	tcbType *pt;
	tcbType *bestPt;
	pt = RunPt;
	do {
		pt = pt->next; // skips at least one
		if ( (pt->Priority < max) && ((pt->blocked) == 0) && ((pt->Sleep) == 0)){
			max = pt->Priority;
			bestPt = pt;
		}
	} while (RunPt != pt);
	RunPt = bestPt;
}

//******** OS_Suspend ***************
// Called by main thread to cooperatively suspend operation
// Inputs: none
// Outputs: none
// Will be run again depending on sleep/block status
void OS_Suspend(void){
  STCURRENT = 0;        // any write to current clears it
  INTCTRL = 0x04000000; // trigger SysTick
// next thread gets a full time slice
}

// ******** OS_Sleep ************
// place this thread into a dormant state
// input:  number of msec to sleep
// output: none
// OS_Sleep(0) implements cooperative multitasking
void OS_Sleep(uint32_t sleepTime){
// ****IMPLEMENT THIS****
// set sleep parameter in TCB, same as Lab 3
// suspend, stops running
	RunPt->Sleep = sleepTime;
	OS_Suspend();
}

// ******** OS_InitSemaphore ************
// Initialize counting semaphore
// Inputs:  pointer to a semaphore
//          initial value of semaphore
// Outputs: none
void OS_InitSemaphore(int32_t *semaPt, int32_t value){
// ****IMPLEMENT THIS****
// Same as Lab 3
 	*semaPt = value;
}

// ******** OS_Wait ************
// Decrement semaphore and block if less than zero
// Lab2 spinlock (does not suspend while spinning)
// Lab3 block if less than zero
// Inputs:  pointer to a counting semaphore
// Outputs: none
void OS_Wait(int32_t *semaPt){
// ****IMPLEMENT THIS****
// Same as Lab 3
  DisableInterrupts();
	(*semaPt) = (*semaPt) - 1;
	if ((*semaPt) < 0){
		RunPt->blocked = semaPt; 
		EnableInterrupts();
		OS_Suspend();
	}
	EnableInterrupts();
}

// ******** OS_Signal ************
// Increment semaphore
// Lab2 spinlock
// Lab3 wakeup blocked thread if appropriate
// Inputs:  pointer to a counting semaphore
// Outputs: none
void OS_Signal(int32_t *semaPt){
// ****IMPLEMENT THIS****
// Same as Lab 3
 	tcbType *pt;
	DisableInterrupts();
	(*semaPt) = (*semaPt) + 1;
	if ((*semaPt) <= 0){
		pt = RunPt->next; // search for blocked thread
		while (pt->blocked != semaPt){
			pt = pt->next;
		}
		pt->blocked = 0; // wake up blocked thread
	}
	EnableInterrupts(); 
}

#define FIFOSIZE 10    // can be any size
uint32_t PutI;      // index of where to put next
uint32_t GetI;      // index of where to get next
uint32_t Fifo[FIFOSIZE];
int32_t CurrentSize;// 0 means FIFO empty, FSIZE means full
uint32_t LostData;  // number of lost pieces of data

// ******** OS_FIFO_Init ************
// Initialize FIFO.  The "put" and "get" indices initially
// are equal, which means that the FIFO is empty.  Also
// initialize semaphores to track properties of the FIFO
// such as size and busy status for Put and Get operations,
// which is important if there are multiple data producers
// or multiple data consumers.
// Inputs:  none
// Outputs: none
void OS_FIFO_Init(void){
// ****IMPLEMENT THIS****
// Same as Lab 3
	PutI = 0;
	GetI = 0;
	OS_InitSemaphore(&CurrentSize, 0);
	LostData = 0;  
}

// ******** OS_FIFO_Put ************
// Put an entry in the FIFO.  Consider using a unique
// semaphore to wait on busy status if more than one thread
// is putting data into the FIFO and there is a chance that
// this function may interrupt itself.
// Inputs:  data to be stored
// Outputs: 0 if successful, -1 if the FIFO is full
int OS_FIFO_Put(uint32_t data){
// ****IMPLEMENT THIS****
// Same as Lab 3
	if (CurrentSize == FIFOSIZE){ // queue is full
		LostData++;
		return -1;
	} else {
		Fifo[PutI] = data; // Put
		PutI = (PutI+1)%FIFOSIZE;
		OS_Signal(&CurrentSize);
		return 0; // success
	}	
}

// ******** OS_FIFO_Get ************
// Get an entry from the FIFO.  Consider using a unique
// semaphore to wait on busy status if more than one thread
// is getting data from the FIFO and there is a chance that
// this function may interrupt itself.
// Inputs:  none
// Outputs: data retrieved
uint32_t OS_FIFO_Get(void){uint32_t data;
// ****IMPLEMENT THIS****
// Same as Lab 3
	OS_Wait(&CurrentSize); // block if empty
	data = Fifo[GetI];
	GetI = (GetI+1)%FIFOSIZE; // place to get next
 return data;
}
// *****periodic events****************
int32_t *PeriodicSemaphore0;
uint32_t Period0; // time between signals
int32_t *PeriodicSemaphore1;
uint32_t Period1; // time between signals
void RealTimeEvents(void){int flag=0;
  static int32_t realCount = -10; // let all the threads execute once
  // Note to students: we had to let the system run for a time so all user threads ran at least one
  // before signalling the periodic tasks
  realCount++;
  if(realCount >= 0){
		if((realCount%Period0)==0){
      OS_Signal(PeriodicSemaphore0);
      flag = 1;
		}
    if((realCount%Period1)==0){
      OS_Signal(PeriodicSemaphore1);
      flag=1;
		}
    if(flag){
      OS_Suspend();
    }
  }
}
// ******** OS_PeriodTrigger0_Init ************
// Initialize periodic timer interrupt to signal 
// Inputs:  semaphore to signal
//          period in ms
// priority level at 0 (highest
// Outputs: none
void OS_PeriodTrigger0_Init(int32_t *semaPt, uint32_t period){
	PeriodicSemaphore0 = semaPt;
	Period0 = period;
	BSP_PeriodicTask_InitC(&RealTimeEvents,1000,0);
}
// ******** OS_PeriodTrigger1_Init ************
// Initialize periodic timer interrupt to signal 
// Inputs:  semaphore to signal
//          period in ms
// priority level at 0 (highest
// Outputs: none
void OS_PeriodTrigger1_Init(int32_t *semaPt, uint32_t period){
	PeriodicSemaphore1 = semaPt;
	Period1 = period;
	BSP_PeriodicTask_InitC(&RealTimeEvents,1000,0);
}

//****edge-triggered event************
int32_t *edgeSemaphore;
// ******** OS_EdgeTrigger_Init ************
// Initialize button2, PD7, to signal on a falling edge interrupt
// Inputs:  semaphore to signal
//          priority
// Outputs: none
void OS_EdgeTrigger_Init(int32_t *semaPt, uint8_t priority){
	edgeSemaphore = semaPt;
//***IMPLEMENT THIS***
	/*
// 1) activate clock for Port D
	SYSCTL_RCGCGPIO_R |= 0x00000008;
// allow time for clock to stabilize
	while((SYSCTL_PRGPIO_R&0x08) == 0){}
// 2) no need to unlock PD7
		GPIO_PORTD_LOCK_R = 0x4C4F434B; 
		GPIO_PORTD_CR_R |= 0xFF;
// 3) disable analog on PD7
		GPIO_PORTD_AMSEL_R &= ~0x80; // 1000 0000
// 4) configure PD7 as GPIO
		GPIO_PORTD_PCTL_R = (GPIO_PORTD_PCTL_R&0x0FFFFFFF)+0x00000000; 	
// 5) make PD7 input
		GPIO_PORTD_DIR_R &= ~0x80;
// 6) disable alt funct on PD7
		GPIO_PORTD_AFSEL_R &= ~0x80;
// disable pull-up on PD7
		GPIO_PORTD_PUR_R &= ~0x80; 
// 7) enable digital I/O on PD7 
	GPIO_PORTD_DEN_R |= 0x80;		*/
	BSP_Button2_Init();
// (d) PD7 is edge-sensitive 
	GPIO_PORTD_IS_R &= ~0x80;
//     PD7 is not both edges 
	GPIO_PORTD_IBE_R &= ~0x80;
//     PD7 is falling edge event 
	GPIO_PORTD_IEV_R &= ~0x80; 
// (e) clear PD7 flag
	GPIO_PORTD_ICR_R = 0x80;
// (f) arm interrupt on PD7
	GPIO_PORTD_IM_R |= 0x80;
// priority on Port D edge trigger is NVIC_PRI0_R	31 – 29
	uint32_t newPrio = priority << 29; // priority in bits 31-29
	NVIC_PRI0_R = (NVIC_PRI0_R&0x1FFFFFFF)| newPrio; 
// enable is bit 3 in NVIC_EN0_R
	NVIC_EN0_R = 0x00000008;
 }

// ******** OS_EdgeTrigger_Restart ************
// restart button1 to signal on a falling edge interrupt
// rearm interrupt
// Inputs:  none
// Outputs: none
void OS_EdgeTrigger_Restart(void){
//***IMPLEMENT THIS***
// rearm interrupt 3 in NVIC
	GPIO_PORTD_IM_R |= 0x80;
// clear flag7
	GPIO_PORTD_ICR_R = 0x80;
}
void GPIOPortD_Handler(void){
//***IMPLEMENT THIS***
	// step 1 acknowledge by clearing flag
		GPIO_PORTD_ICR_R = 0x80;
		// step 2 signal semaphore (no need to run scheduler)
		OS_Signal(edgeSemaphore); 
		// step 3 disarm interrupt to prevent bouncing to create multiple signals
		GPIO_PORTD_IM_R &= ~0x80;
}

#define PF2   (*((volatile uint32_t *)0x40025010))
#define Debug_Set()   (PF2 = 0x04)
#define Debug_Clear() (PF2 = 0x00)


typedef struct{
  int16_t real,imag;
}Complex_t;
// data for FFT
Complex_t x[1024],y[1024];
int32_t mag[512];
uint32_t before,elapsed,elapsed64,elapsed256;

void cr4_fft_1024_stm32(Complex_t *pssOUT, Complex_t *pssIN, unsigned short Nbin);
void cr4_fft_256_stm32(Complex_t *pssOUT, Complex_t *pssIN, unsigned short Nbin);
void cr4_fft_64_stm32(Complex_t *pssOUT, Complex_t *pssIN, unsigned short Nbin);

void test_fft(int16_t sinewave[], int t){
	int32_t k, real, imag;
	for(t=0; t<1024; t=t+1){   // t means 1/fs
    x[t].imag = 0;           // imaginary part is zero
    x[t].real = sinewave[t]; // fill real part with data
  }
  before = NVIC_ST_CURRENT_R;
  cr4_fft_1024_stm32(y, x, 1024);   // complex FFT of 1024 values
	
  elapsed = (before - NVIC_ST_CURRENT_R - 6)&0x00FFFFFF; 
  // the number 6 depends on the instructions before and after test
  // if you remove the call to FFT, elapsed measures 0

  while(1){
    for(t=0; t<1024; t=t+1){   // simulated ADC samples
      x[t].imag = 0;           // imaginary part is zero
      x[t].real = sinewave[t]; // fill real part with data
    }
    Debug_Set();                    // PF2=1
    cr4_fft_1024_stm32(y, x, 1024); // complex FFT of last 1024 ADC values
    Debug_Clear();                  // PF2=0
    for(k=0; k<512; k=k+1){         // k means fs/1024
      real = y[k].real;             // bottom 16 bits
      imag = y[k].imag;             // top 16 bits
      mag[k] = sqrt(real*real+imag*imag);
    }
  }

}

