// ConsoleApplication2.cpp : Defines the entry point for the console application.
//

#include "SystemControl.h"
#include <assert.h>

//Initialize Global Counter for time base of System Control
volatile long globalCounter = 0;

//Initialize global warning variables
Bool bpHigh = FALSE;
Bool tempHigh = FALSE;
Bool pulseLow = FALSE;
Bool batteryLow = FALSE;

int bpOutOfRange = 0;
int tempOutOfRange = 0;
int pulseOutOfRange = 0;

int alarmAck = 0;
int alarmAckStartTime = 0;

// Initialize function1 and function2 along with counters for schedule function testing
unsigned int counter1 = 0;
void function1(void* input) { counter1++; }

unsigned int counter2 = 0;
void function2(void* input) { counter2++; }

int input = 0;
TCB function1Task = { &function1,  &input };
TCB function2Task = { &function2,  &input };
TCB* tasks_head;
TCB* tasks_tail;
Bool addTaskArray[TASKCOUNT];
Bool removeTaskArray[TASKCOUNT];

TCB tasks[] = { function1Task, function1Task, function1Task, function2Task, function2Task };

/************************************************************************
function name:            test_compute
function inputs:          none
function outputs:         none
function description:     Tests the compute code functionality
author:                   unknown
*************************************************************************/
void test_compute() {
  //Initialize global measurement variables
  unsigned int temperatureRawBuf[8] = { 75, 0, 0, 0, 0, 0, 0, 75 };
  unsigned int systolicPressRawBuf[8] = { 80, 0, 0, 0, 0, 0, 0, 80 };
  unsigned int diastolicPressRawBuf[8] = { 80, 0, 0, 0, 0, 0, 0, 80 };
  unsigned int pulseRateRawBuf[8] = { 50, 0, 0, 0, 0, 0, 0, 50 };

  //Initialize global display variables
  unsigned char* tempCorrectedBuf[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
  unsigned char* systolicPressCorrectedBuf[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
  unsigned char* diastolicPressCorrectedBuf[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
  unsigned char* pulseRateCorrectedBuf[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };

  volatile long nextMeasureIndex = 1;
  volatile long lastComputeIndex = 6;

  ComputeTaskInput computeTaskInput = { &temperatureRawBuf, &systolicPressRawBuf, &diastolicPressRawBuf, &pulseRateRawBuf, &tempCorrectedBuf, &systolicPressCorrectedBuf, &diastolicPressCorrectedBuf, &pulseRateCorrectedBuf };

  // Test initial run of compute() modifies all the values between last compute and next measure as expected
  globalCounter = 5;
  globalCounter = 5;
  compute(&computeTaskInput);
  assert(temperatureRawBuf[0] == 169);
  assert(systolicPressCorrectedBuf[0] == 126);
  assert(diastolicPressCorrectedBuf[0] == 158);
  assert(pulseRateCorrectedBuf[0] == 30);

  assert(temperatureRawBuf[7] == 169);
  assert(systolicPressCorrectedBuf[7] == 126);
  assert(diastolicPressCorrectedBuf[7] == 158);
  assert(pulseRateCorrectedBuf[7] == 30);
}

/************************************************************************
function name:            test_warning
function inputs:          none
function outputs:         none
function description:     Tests the warning code functionality
author:                   unknown
*************************************************************************/
void test_warning() {
  //Initialize global measurement variables
  unsigned int temperatureRaw = 75;
  unsigned int systolicPressRaw = 80;
  unsigned int diastolicPressRaw = 80;
  unsigned int pulseRateRaw = 50;
  unsigned int respirationRateRaw = 75;
  unsigned int ekgFreq = 300;

  // So we can pass in single int instead of full buffer
  unsigned int lastComputeIndex = 0;

  //Initialize global status variable
  unsigned short batteryState = 200;

  WarningTaskInput warningTaskInput = { &temperatureRaw, &systolicPressRaw, &diastolicPressRaw, &pulseRateRaw, &respirationRateRaw, &ekgFreqBuf, &batteryState };

  // Test warning() does modify existing values after 1 second:
  // Lab 5 addition of ekg values
  globalCounter = 1;
  warning(&warningTaskInput);
  assert(*(warningTaskInput.temperatureRawPtr) == 75);
  assert(*(warningTaskInput.systolicPressRawPtr) == 80);
  assert(*(warningTaskInput.diastolicPressRawPtr) == 80);
  assert(*(warningTaskInput.pulseRateRawPtr) == 50);
  assert(*(warningTaskInput.batteryStatePtr) == 200);
  assert(bpHigh == FALSE);
  assert(tempHigh == TRUE);
  assert(pulseLow == TRUE);
  assert(batteryLow == FALSE);
  assert(ekgHigh == FALSE);
  assert(pulseOutOfRange == 0);
  assert(tempOutOfRange == 0);
  assert(bpOutOfRange == 0);

  // Test warning() modifies the blood pressure out of range alarm
  globalCounter = 2;
  systolicPressRaw = 180;
  diastolicPressRaw = 80;
  warning(&warningTaskInput);
  assert(bpOutOfRange == 1);

  // Test warning() does not set blood pressure out of range if there is a valid acknowledgement
  globalCounter = 3;
  systolicPressRaw = 180;
  diastolicPressRaw = 80;
  alarmAck = 1;
  warning(&warningTaskInput);
  assert(bpOutOfRange == 0);

  // Test warning() does sets blood pressure out of range if acknowledgement is out of date
  globalCounter = 30;
  warning(&warningTaskInput);
  assert(bpOutOfRange == 1);
}

voi

/************************************************************************
function name:            test_status
function inputs:          none
function outputs:         none
function description:     Tests the measure code functionality
author:                   unknown
*************************************************************************/
void test_status() {
  //Initialize global status variable
  unsigned short batteryState = 200;

  StatusTaskInput statusTaskInput = { &batteryState };

  // Test status() does not modify existing values if 5 seconds has not passed:
  globalCounter = 1;
  status(&statusTaskInput);
  assert(*(statusTaskInput.batteryStatePtr) == 200);

  // Test status() does not modify existing values if 5 seconds has not passed:
  globalCounter = 5;
  status(&statusTaskInput);
  assert(*(statusTaskInput.batteryStatePtr) == 199);
}

/************************************************************************
function name:            test_schedule
function inputs:          none
function outputs:         none
function description:     Tests the schedule code functionality along with insert and remove functions
author:                   unknown
*************************************************************************/
void test_schedule() {
  // Tests schedule along with add task queue works
  int oldGlobalCounter = globalCounter;
  addTaskArray = { 1, 0, 0, 0, 1 };
  schedule();
  assert(globalCounter - oldGlobalCounter == 1);		// Asserts that the schedule function increments global counter once per execution
  assert(counter1 == 1);								// Asserts that function1 is called 1 times
  assert(counter2 == 1);								// Asserts that function2 is called 1 times

                                        // Tests schedule along with remove task queue works
  oldGlobalCounter = globalCounter;
  removeTaskArray = { 1, 0, 0, 0, 1 };
  schedule();
  assert(globalCounter - oldGlobalCounter == 1);		// Asserts that the schedule function increments global counter once per execution
  assert(counter1 == 1);								// Asserts that function1 has not been called since
  assert(counter2 == 1);								// Asserts that function2 has not been called since
}

void main() {
  test_schedule();
  test_measure();
  test_compute();
  test_warning();
  test_status();
}