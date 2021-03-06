//*****************************************************************************
// os.c
// Runs on TM4C123 with BOOSTXL-EDUMKII booster pack
// A program to process captured sound samples and print magnitude, root mean square,
// and sound frequency on a LCD Display.

// May, 3, 2021
// Daniel King, Lujayna Taha, Thaddeus Phipps

// Project was built on RTOS_4C123 template written and copyrighted by Daniel Valvano, February 2016
// Copyright 2016 by Jonathan W. Valvano, valvano@mail.utexas.edu
// This program does currently make use of Valvano's BSP.C and other assembly configurations.
// RTOS functions and mechanism from template currently not used in this program.

#include <stdint.h>
#include "os.h"
#include "CortexM.h"
#include "BSP.h"
#include "arm_math.h"
#include "arm_common_tables.h"


//******** FFT ********\\
// initialize transform function with only one fiddle factor table

arm_status rfft_fast_init_1024_f32(arm_rfft_fast_instance_f32 * S){
  arm_cfft_instance_f32 * Sint;
	Sint = &(S->Sint);
	Sint->fftLen = 512u;
	S->fftLenRFFT = Sint->fftLen;
	Sint->bitRevLength = ARMBITREVINDEXTABLE_512_TABLE_LENGTH;
	Sint->pBitRevTable = (uint16_t*)armBitRevIndexTable512;
	Sint->pTwiddle = (float32_t*)twiddleCoef_512;
	S->pTwiddleRFFT = (float32_t*)twiddleCoef_rfft_1024;
	
  return ARM_MATH_SUCCESS;
}


//******** OS FUNCTIONS ********\\
// currently not used in code

// function definitions in osasm.s
void StartOS(void);

#define NUMTHREADS  3		      // maximum number of threads
#define STACKSIZE   100      // number of 32-bit words in stack
struct tcb{
  int32_t *sp;       // pointer to stack (valid for threads not running
  struct tcb *next;  // linked-list pointer
};
typedef struct tcb tcbType;
tcbType tcbs[NUMTHREADS];
tcbType *RunPt;
int32_t Stacks[NUMTHREADS][STACKSIZE];

// declare global variables for mailbox
uint32_t Mail; // shared data
int32_t Send; // that's used as a semaphore!
int32_t Ack; // that's used as a semaphore!
uint32_t Lost;
// declare global variables for periodic tasks
void(*EventThread)(void);
uint32_t wait;
uint32_t Counter;


// ******** OS_Init ************
// initialize operating system, disable interrupts until OS_Launch
// initialize OS controlled I/O: systick
// input:  none
// output: none
void OS_Init(void){
  DisableInterrupts();
  BSP_Clock_InitFastest();// set processor clock to fastest speed
	wait = 1;
}

void SetInitialStack(int i){
  tcbs[i].sp = &Stacks[i][STACKSIZE-16]; // thread stack pointer
  Stacks[i][STACKSIZE-1] = 0x01000000;   // thumb bit
  Stacks[i][STACKSIZE-3] = 0x14141414;   // R14
  Stacks[i][STACKSIZE-4] = 0x12121212;   // R12
  Stacks[i][STACKSIZE-5] = 0x03030303;   // R3
  Stacks[i][STACKSIZE-6] = 0x02020202;   // R2
  Stacks[i][STACKSIZE-7] = 0x01010101;   // R1
  Stacks[i][STACKSIZE-8] = 0x00000000;   // R0
  Stacks[i][STACKSIZE-9] = 0x11111111;   // R11
  Stacks[i][STACKSIZE-10] = 0x10101010;  // R10
  Stacks[i][STACKSIZE-11] = 0x09090909;  // R9
  Stacks[i][STACKSIZE-12] = 0x08080808;  // R8
  Stacks[i][STACKSIZE-13] = 0x07070707;  // R7
  Stacks[i][STACKSIZE-14] = 0x06060606;  // R6
  Stacks[i][STACKSIZE-15] = 0x05050505;  // R5
  Stacks[i][STACKSIZE-16] = 0x04040404;  // R4
}

//******** OS_AddThread ***************
// add three foregound threads to the scheduler
// Inputs: three pointers to a void/void foreground tasks
// Outputs: 1 if successful, 0 if this thread can not be added
int OS_AddThreads(void(*task0)(void),
                 void(*task1)(void)){ 
	int32_t status;
  status = StartCritical();
  tcbs[0].next = &tcbs[1]; // 0 points to 1
  tcbs[1].next = &tcbs[0]; // 1 points to 2				 
									 
  SetInitialStack(0); 
	Stacks[0][STACKSIZE-2] = (int32_t)(task0); // PC
									 
  SetInitialStack(1); 
	Stacks[1][STACKSIZE-2] = (int32_t)(task1); // PC
									 	 
									 
  RunPt = &tcbs[0];       // thread 0 will run first
  EndCritical(status);
  return 1;               // successful
}
								 
int OS_AddPeriodicEventThreads(void(*thread)(void), uint32_t period){ // from previous lab assignments
	EventThread = *thread;
	wait = period;

  return 1;
}

//******** SCHEDULER ********\\
// Round Robin Scheduler, runs every ms
void Scheduler(void){ // Program 3.12 from book
  // run any periodic event threads if needed
  // implement round robin scheduler, update RunPt
	if ((++Counter) == wait){	
			EventThread();
			Counter = 0; // reset counter
	}
	// Round Robin scheduler
	RunPt = RunPt->next;
	
	return;
}

//******** OS_Launch ***************
// start the scheduler, enable interrupts
// Inputs: number of clock cycles for each time slice
//         (maximum of 24 bits)
// Outputs: none (does not return)
void OS_Launch(uint32_t theTimeSlice){
  STCTRL = 0;                  // disable SysTick during setup
  STCURRENT = 0;               // any write to current clears it
  SYSPRI3 =(SYSPRI3&0x00FFFFFF)|0xE0000000; // priority 7
  STRELOAD = theTimeSlice - 1; // reload value
  STCTRL = 0x00000007;         // enable, core clock and interrupt arm
  StartOS();                   // start on the first task
}

//******** SEMAPHORES ********\\

void OS_InitSemaphore(int32_t *semaPt, int32_t value){
	*semaPt = value;
}

void OS_Wait(int32_t *semaPt){
	DisableInterrupts();
	while ((*semaPt) == 0){
		EnableInterrupts();
		DisableInterrupts();
	}
	// decrease counter and get out of here, value is either 0 or greater
	*semaPt = (*semaPt) - 1; 
	EnableInterrupts();
	
	return;
}

void OS_Signal(int32_t *semaPt){
	DisableInterrupts();
	(*semaPt)++; 	// dereference and increment
	EnableInterrupts();
	
	return;
}

//******** MAIL BOX ********\\

void OS_MailBox_Init(void){
	Mail = 0;
	Send = 0;
	Lost = 0;
}

void OS_MailBox_Send(uint32_t data){ // code from book
	Mail = data;
	if(Send){
		Lost++;
	}else{
		OS_Signal(&Send);
	}
}

uint32_t OS_MailBox_Recv(void){ 
	uint32_t data;  // code from book
	
	OS_Wait(&Send);
	data = Mail;
	
  return data;
}





