#include <stdio.h>
#include <stdlib.h>

#include <Elegoo_GFX.h>                                                     // Core graphics library
#include <Elegoo_TFTLCD.h>                                                  // Hardware-specific library
#include <TouchScreen.h>                                                    // Touchscreen library

#include "fft.h"
#include "System.h"
#include "SystemControl.h"

Elegoo_TFTLCD tft(LCD_CS, LCD_CD, LCD_WR, LCD_RD, LCD_RESET);
TouchScreen ts = TouchScreen(XP, YP, XM, YM, 300);
// If using the shield, all control and data lines are fixed, and
// a simpler declaration can optionally be used:
// Elegoo_TFTLCD tft;

// Declare measure and compute task variables
unsigned int temperatureRawBuf[MEASURE_BUF_LEN];                            // Declare raw temperature measurement buffer
unsigned int bloodPressRawBuf[MEASURE_BUF_LEN * 2];                         // Declare raw blood pressure measurement buffer
unsigned int pulseRateRawBuf[MEASURE_BUF_LEN];                              // Declare raw pulse rate measurement buffer
unsigned int respirationRateRawBuf[MEASURE_BUF_LEN];                        // Declare raw respiratory rate buffer
unsigned int tempCorrectedBuf[MEASURE_BUF_LEN];                             // Declare corrected temperature measurement char buffer
unsigned int bloodPressCorrectedBuf[MEASURE_BUF_LEN * 2];                   // Declare corrected blood pressure char buffer
unsigned int pulseRateCorrectedBuf[MEASURE_BUF_LEN];                        // Declare corrected pulse rate char buffer
unsigned int respirationRateCorrectedBuf[MEASURE_BUF_LEN];                  // Declare corrected respiratory rate buffer
unsigned char ekgRawBuf[EKG_SAMPLES];                                       // Declare raw EKG value buffer
unsigned int ekgFreqBuf[EKG_FFT_BUF_LEN];                                   // Declare raw EKG frequency buffer
unsigned short batteryState;                                                // Declare battery state

unsigned short measurementSelect;                                           // Declare the selected measurement options
                                                                            // temp = 0b10000, bp = 0b100000, pr = 0b1000000. Plus any compbinations of these three numbers

// Declare system metadata
String remoteHeader;                                                        // Declare remote display title variable
String productName;                                                         // Declare product name
String patientName;                                                         // Declare patient name
String doctorName;                                                          // Declare doctor name

// Declare warning variables
Bool bpHigh;                                                                // Declare high blood pressure bool
Bool tempHigh;                                                              // Declare high temperature bool
Bool pulseLow;                                                              // Declare low pulse rate
Bool batteryLow;                                                            // Declare low battery
Bool rrLow;                                                                 // Declare low respiratory rate
Bool rrHigh;                                                                // Declare high respiratory rate bool
Bool ekgLow;                                                                // Declare low EKG bool
Bool ekgHigh;                                                               // Delcare high EKG bool

// Declare alarm variables
unsigned char bpOutOfRange;                                                 // Declare out of range blood pressure
unsigned char tempOutOfRange;                                               // Declare out of range temperature    
unsigned char pulseOutOfRange;                                              // Declare out of range pulse rate    
unsigned char rrOutOfRange;                                                 // Declare out of range respiratory rate
unsigned char EKGOutOfRange;                                                // Declare out of range EKG variable

unsigned short alarmAck;                                                    // Declare alarm acknowledgement state

// Declare display variable
unsigned short displayState;                                                // Declare keypad and display state variable

// Logging data
unsigned long tempWarningTime = 0;                                          // Declare the amount of time temperature has been in warning
unsigned long prWarningTime = 0;                                            // Declare the amount of time pulse rate has been in warning
unsigned long rrWarningTime = 0;                                            // Declare the amount of time respiratory rate has been in warning
unsigned long bpWarningTime = 0;                                            // Declare the amount of time blood pressure has been in warning
unsigned long ekgWarningTime = 0;                                           // Declare the amount of time EKG measurement has been in warning

// Create input and task structs
MeasureTaskInput measureTaskInput = { temperatureRawBuf, bloodPressRawBuf, pulseRateRawBuf, respirationRateRawBuf, &measurementSelect };
TCB measureTask = { &measure, &measureTaskInput, NULL, NULL, 0 };

ComputeTaskInput computeTaskInput = { temperatureRawBuf, bloodPressRawBuf, pulseRateRawBuf, respirationRateRawBuf, tempCorrectedBuf, bloodPressCorrectedBuf, pulseRateCorrectedBuf, respirationRateCorrectedBuf, &measurementSelect };
TCB computeTask = { &compute, &computeTaskInput, NULL, NULL, 1 };

DisplayTaskInput displayTaskInput = { &displayState, tempCorrectedBuf, bloodPressCorrectedBuf, pulseRateCorrectedBuf, respirationRateCorrectedBuf, ekgFreqBuf, &batteryState };
TCB displayTask = { &display, &displayTaskInput, NULL, NULL, 2 };

WarningTaskInput warningTaskInput = { temperatureRawBuf, bloodPressRawBuf, pulseRateRawBuf, respirationRateRawBuf, ekgFreqBuf, &batteryState };
TCB warningTask = { &warning, &warningTaskInput, NULL, NULL, 3 };

StatusTaskInput statusTaskInput = { &batteryState };
TCB statusTask = { &status, &statusTaskInput, NULL, NULL, 4 };

KeypadTaskInput keypadTaskInput = { &displayState, &measurementSelect, &alarmAck };
TCB keypadTask = { &keypad, &keypadTaskInput, NULL, NULL, 5 };

RemoteDisplayTaskInput remoteDisplayTaskInput = { tempCorrectedBuf, bloodPressCorrectedBuf, pulseRateCorrectedBuf, respirationRateCorrectedBuf, ekgFreqBuf, &tempWarningTime, &bpWarningTime, &prWarningTime, &rrWarningTime, &ekgWarningTime };
TCB remoteDisplayTask = { &remoteDisplaySend, &remoteDisplayTaskInput, NULL, NULL, 8 };

CommunicationsTaskInput communicationsTaskInput = { &measurementSelect, tempCorrectedBuf, bloodPressCorrectedBuf, pulseRateCorrectedBuf };
TCB communicationsTask = { &communications, &communicationsTaskInput, NULL, NULL, 6 };

// Ensure that the ordering is the same as TaskID in System.h
TCB* taskArray[TASKCOUNT] = { &measureTask, &computeTask, &displayTask, &warningTask, &statusTask, &keypadTask, &communicationsTask, &remoteDisplayTask };

// Declare global task queue
TCB* tasks_head;
TCB* tasks_tail;
Bool addTaskArray[TASKCOUNT];
Bool removeTaskArray[TASKCOUNT];

// Declare and initializes the measurement buffer states
long nextMeasureIndex = 0;
long lastComputeIndex = 0;
long lastEKGProcessIndex = 0;

// Declare peripheral communications buffer and variables
int comBufReady = 0;
char comOutBuf[COM_BUF_LEN];
char comInBuf[COM_NUM_BUF][COM_BUF_LEN];
short comInBufNum = 0;
short comInBufIndex = 0;

// Declare remote communications buffer and variables
int remComBufReady = 0;
char remComOutBuf[REM_COM_BUF_LEN];
char remComInBuf[REM_COM_NUM_BUF][REM_COM_BUF_LEN];
short remComInBufNum = 0;
short remComInBufIndex = 0;
char doRemoteDisplay = 1;
char enableMeasurement = 1;

// Declare scheduler variables
volatile long globalCounter;

// Declare warning/alarm state variable
unsigned long alarmAckTime = 0;

/************************************************************************
  function name:            setup
  function inputs:          none
  function outputs:         none
  function description:     Initialize the Arduino Mega and TFT display
  author:                   unknown
*************************************************************************/

void setup() {
  Serial.begin(9600);

  tft.reset();

  uint16_t identifier = tft.readID();
  if (identifier == 0x9325) {}
  else if (identifier == 0x9328) {}
  else if (identifier == 0x4535) {}
  else if (identifier == 0x7575) {}
  else if (identifier == 0x9341) {}
  else if (identifier == 0x8357) {}
  else if (identifier == 0x0101) {
    identifier = 0x9341;
  }
  else if (identifier == 0x1111) {
    identifier = 0x9328;
  }
  else {
    identifier = 0x9328;
  }

  tft.begin(identifier);

  startup();
}


/**************************************************************************************
function name:            startup
function inputs:          none
function outputs:         none
function description:     Initialize system control vairables and global timebase
author:                   Yifan Shao
*************************************************************************************/

void startup() {
  Serial1.begin(9600);                                                                      // Initialize serial connection speed with Uno

  globalCounter = 0;

  // Set up hardware timer for system time base
  noInterrupts();
  TCNT1 = 0;                                                                                // Clear timer counter
  TCCR1A = 0;                                                                               // Clear timer1 flags A
  TCCR1B = 0;                                                                               // Clear timer1 flags B
  TCCR1B |= (1 << CS12);                                                                    // Set clock prescaler = 1/256
  TCCR1B |= (1 << WGM12);                                                                   // Set mode to compare
  OCR1A = 15625;                                                                            // Set timer to 1 Hz
  TIMSK1 |= (1 << OCIE1A);                                                                  // Turn on timer1 compare interrupt A
  interrupts();

  // Initialize measurement selection options
  measurementSelect = 0;

  // Initialize global task inputs variables
  for (int i = 0; i < MEASURE_BUF_LEN; i++) {
    temperatureRawBuf[i] = 75;
    bloodPressRawBuf[i] = 80;
    bloodPressRawBuf[MEASURE_BUF_LEN + i] = 80;
    tempCorrectedBuf[i] = 0;
    bloodPressCorrectedBuf[i] = 0;
    bloodPressCorrectedBuf[MEASURE_BUF_LEN + i] = 0;
    pulseRateCorrectedBuf[i] = 0;
  }

  // Initialize battery state variable
  batteryState = 200;

  productName = "Medical Monitoring System";
  patientName = "Iam Patient";
  doctorName = "Iam Doctor";
  remoteHeader = String(productName + "\r\n" + patientName + "\r\n" + doctorName + "\r\n\n");

  // Initialize warning and alarm variables
  bpHigh = FALSE;
  tempHigh = FALSE;
  pulseLow = FALSE;
  batteryLow = FALSE;
  bpOutOfRange = 0;
  tempOutOfRange = 0;
  pulseOutOfRange = 0;

  // Initialze task queue with static tasks
  tasks_head = NULL;
  tasks_tail = NULL;
  insert_task(&communicationsTask);
  insert_task(&statusTask);
  insert_task(&warningTask);
  insert_task(&keypadTask);
  insert_task(&remoteDisplayTask);

  // Inititalize add and remove task labels
  for (int i = 0; i < TASKCOUNT; i++) {
    removeTaskArray[i] = FALSE;
    addTaskArray[i] = FALSE;
  }

  // Initialize measurement and compute index variables
  nextMeasureIndex = 0;
  lastComputeIndex = 0;
  lastEKGProcessIndex = 0;

  // Initialize remote command variables
  doRemoteDisplay = 1;
  enableMeasurement = 1;

  // Initialize display state.
  displayState = 1; // 1 = main menu, 2 = measurement select, 3 = annunciation mode

  // Configure TFT display
  tft.setTextSize(2);                                                                     // Set the TFT text size to size "1"
  tft.fillScreen(BLACK);                                                                  // Color the TFT display with a black screen
  tft.setRotation(3);                                                                     // Set screen to horizontal
}


/************************************************************************
function name:            ISR
function inputs:          none
function outputs:         none
function description:     ISR interrupt for timer1
author:                   Eric Ho
*************************************************************************/

ISR(TIMER1_COMPA_vect) {
  globalCounter++;
}


/************************************************************************
  function name:            loop
  function inputs:          none
  function outputs:         none
  function description:     calls "schedule" task to start going through the task queue
  author:                   Yifan Shao
*************************************************************************/

void loop() {
  schedule();
}


/*************************************************************************************************************
  function name:            schedule
  function inputs:          none
  function outputs:         none
  function description:     Goes through the task queue and mainains the global clock on 1 second intervals
  author:                   Yifan Shao
**************************************************************************************************************/

void schedule() {
  // Add and remove the necessary tasks to and from task queue
  // All tasks created (static and dynamic) are stored in the staty array taskArray
  for (int i = 0; i < TASKCOUNT; i++) {                                                   // Loop through each of the task in tasks array
    if (addTaskArray[i]) {                                                                // If the add task flag is set for the ith task, we add it to the task queue
      if (i == 7) { Serial.println(); }
      insert_task(taskArray[i]);
      addTaskArray[i] = FALSE;                                                            // Reset the add task flag for the specified task
    }
  }

  // Loop through the task queue and run each task in queue
  TCB* current_task = tasks_head;
  while (current_task != NULL) {                                                          // Loop through the task queue and run each task
    (*(current_task->taskPtr))(current_task->taskInputPtr);                               // Call the task function with the input (pointer) provided
    current_task = current_task->nextTask;
  }

  for (int i = 0; i < TASKCOUNT; i++) {                                                   // Loop through each of the task in tasks array
    if (removeTaskArray[i]) {                                                             // If the remove task flag is set for the ith task, we delete it fromm the task queue
      delete_task(taskArray[i]);
      removeTaskArray[i] = FALSE;                                                         // Reset the remove task flag
    }
  }

  return;
}


/******************************************************************************************************
  function name:            measure
  function inputs:          pointer to struct containing "measure" data
  function outputs:         none
  function description:     Collects and updates raw values for temperature, pressure, and pulse rate
  author:                   Eric Ho
******************************************************************************************************/

void measure(void* dataPtr) {
  if (enableMeasurement) {
    MeasureTaskInput* data = (MeasureTaskInput*)dataPtr;                                  // Cast the input pointer into type request data
    char requestData[80];
    int len = snprintf(requestData, 80, "%d", *(data->measurementSelectPtr));
    sendMessage(MeasureTask, PMeasure, requestData, len);                                 // Sends the request to the arduino Uno
    removeTaskArray[0] = TRUE;
  }

  return;
}


/******************************************************************************************************
function name:            measureCallback
function inputs:          pointer to struct containing new "measure" data send from the Uno
function outputs:         none
function description:     Place newly measured values into the global measurement buffers for system control
author:                   Eric Ho
******************************************************************************************************/

void measureCallback(void* dataPtr) {
  if (enableMeasurement) {
    PMeasureResponseData* responseData = (PMeasureResponseData*)dataPtr;                  // Cast the input pointer int type response data
    int index = nextMeasureIndex % MEASURE_BUF_LEN;                                       // Compute which data buffer index to use
    temperatureRawBuf[index] = responseData->temperature;                                 // Place raw temeprature measurement into appropriate buffer
    bloodPressRawBuf[index] = responseData->systolic;                                     // Place systolic blood pressure measurement into appropriate buffer
    bloodPressRawBuf[index + MEASURE_BUF_LEN] = responseData->diastolic;                  // Place diastolic bood pressure measurement into appropriate buffer
    pulseRateRawBuf[index] = responseData->pulse;                                         // Place pulse rate measurement into appropriate buffer
    respirationRateRawBuf[index] = responseData->resp;                                    // Place respiratory rate measurement into appropriate buffer
    nextMeasureIndex++;                                                                   // Increment data buffer index
    getEKGValues(0);                                                                      // Fetch EKG raw measurements
    char buf[80];
    int len = snprintf(
      buf,
      80,
      "%d,%d,%d,%d,%d\n",
      responseData->temperature,
      responseData->systolic,
      responseData->diastolic,
      responseData->pulse,
      responseData->resp);                                                                // Generate a buffer of the measurements received
    addTaskArray[1] = TRUE;                                                               // Set flag to add 'compute' task to dynamic task queue
    alarmAckTime++;                                                                       // Increment alarm ack time as 1 more measurement has been made
  }

  return;
}


/******************************************************************************************************
function name:            getEKGValues
function inputs:          the current request index for EKG values
function outputs:         none
function description:     Request for measured EKG raw values
author:                   Eric Ho
******************************************************************************************************/

void getEKGValues(int index) {
  char message[2];
  int len = snprintf(message, 2, "%d", index);                                            // Create a request with current index of EKG measurement
  sendMessage(EKGTask, PEKG, message, len);                                               // Send request to peripheral subsystem

  return;
}


/******************************************************************************************************
function name:            storeEKGValues
function inputs:          EKG response data from peripheral subsystem
function outputs:         none
function description:     Place newly measured EKG values into the global measurement buffers for system control
author:                   Eric Ho
******************************************************************************************************/

void storeEKGValues(void* input) {
  PEKGResponseData* d = (PEKGResponseData*)input;                                         // Cast input into of type EKG response from peripheral subsystem
  memcpy(ekgRawBuf + d->index * EKG_BLOCK_SIZE, d->data, EKG_BLOCK_SIZE);                 // Copy EKG response into puffer
  if (d->index < EKG_SAMPLES / EKG_BLOCK_SIZE) {                                          // If EKG raw buff is not yet filled (256 values), request for the next bathc
    getEKGValues(d->index + 1);
  }
  else {                                                                                  // If all EKG values has been saved, request for EKG processing
    ekgprocessing();
  }

  return;
}

/********************************************************************************************************************
  function name:            compute
  function inputs:          pointer to struct containing "compute" data
  function outputs:         none
  function description:     Corrects the raw temperature, pressure, and pulse rate values
  author:                   Yifan Shao
********************************************************************************************************************/

void compute(void* input) {
  ComputeTaskInput* typedInput = (ComputeTaskInput*)input;                              // Cast the void ptr input into of type ComputeTaskInput
  for (int i = lastComputeIndex; i < nextMeasureIndex; i++) {                           // For each of the index that has been updated by measure but hasnt been updated by compute
    int index = i % MEASURE_BUF_LEN;                                                    // Compute the current index to update
    int adjusted_temp = typedInput->temperatureRawBufPtr[index] * 20.0 / 1024.0 + 25;   // Valid temperature from 25 ~ 45
    typedInput->tempCorrectedBufPtr[index] =
      (int)(5 + 0.75 * adjusted_temp);                                                  // Corect temperature value at index
    typedInput->bloodPressCorrectedBufPtr[index] =
      (9 + 2 * typedInput->bloodPressRawBufPtr[index]);                                 // Correct systolic pressure value at index
    typedInput->bloodPressCorrectedBufPtr[index + MEASURE_BUF_LEN] =
      (int)(6 + 1.5 * typedInput->bloodPressRawBufPtr[index + MEASURE_BUF_LEN]);        // Correct diastolic pressure value at index
    typedInput->prCorrectedBufPtr[index] =
      (8 + 3 * typedInput->pulseRateRawBufPtr[index]);                                  // Correct pulse rate value at index
    typedInput->respirationRateCorrectedBufPtr[index] =
      (7 + 3 * typedInput->respirationRateRawBufPtr[index]);                            // Correct respiratory rate value at index
  }

  lastComputeIndex = nextMeasureIndex - 1;                                              // Update last compute index with the values updated
  removeTaskArray[1] = TRUE;                                                            // Set the remove flag for compute, the compute task will be removed from the task queue until measure schedules it again
  return;
}


/******************************************************************************************************************
  function name:            warning
  function inputs:          pointer to struct containing raw temperature, pulse rate, blood pressure, and battery state data
  function outputs:         none
  function description:     Sets warning flags for display and prompting for alarms when applicable
  author:                   Yifan Shao
******************************************************************************************************************/
void warning(void* input) {
  static long lastExecutionTime = 0;                                                                          // Initialize the last execution time
  if (globalCounter - lastExecutionTime >= TASKDELAY_1s) {                                                    // Only execute the function if 1 second has passed since the last execution
    lastExecutionTime = globalCounter;                                                                        // Should the rest of function run, update the last execution time to the current time

    int currentValueIndex = nextMeasureIndex - 1;                                                             // Compute the index for the last updated compute value
    WarningTaskInput* warningData = (WarningTaskInput*)input;                                                 // Cast the input into type warning data pointer

    bpHigh = (Bool)(((int*)warningData->bloodPressRawBufPtr)[currentValueIndex] < 114 ||
      ((int*)warningData->bloodPressRawBufPtr)[currentValueIndex] > 136.5 ||
      ((int*)warningData->bloodPressRawBufPtr)[currentValueIndex + MEASURE_BUF_LEN] < 70 ||
      ((int*)warningData->bloodPressRawBufPtr)[currentValueIndex + MEASURE_BUF_LEN] > 80);                    // Set the bpHigh warning flag if systolic or dystollic pressure is out of range
    bpOutOfRange = ((int*)warningData->bloodPressRawBufPtr)[currentValueIndex] > 156 ||
      ((int*)warningData->bloodPressRawBufPtr)[currentValueIndex] < 96;                                       // Set the bpOutOfRange alarm flag if the systolic is out of range by 20%
    if (bpHigh) { bpWarningTime++; }                                                                          // Increment blood pressure warning time if in warning state

    tempHigh = (Bool)(((int*)warningData->temperatureRawBufPtr)[currentValueIndex] > (37.8 * 1.05) ||
      ((int*)warningData->temperatureRawBufPtr)[currentValueIndex] < (36.1 * 0.95));                          // Set the tempHigh warning flag if temperature is out of range
    tempOutOfRange = ((int*)warningData->temperatureRawBufPtr)[currentValueIndex] > (37.8 * 1.15) ||
      ((int*)warningData->temperatureRawBufPtr)[currentValueIndex] < (36.1 * 0.85);                           // Set the tempOutOfRange alarm flag if the temperature is out of range by 15%
    if (tempHigh) { tempWarningTime++; }                                                                      // Increment temperature warning time if in warning state

    pulseLow = (Bool)(((int*)warningData->pulseRateRawBufPtr)[currentValueIndex] < (60 * 0.95) ||
      ((int*)warningData->pulseRateRawBufPtr)[currentValueIndex] > (100 * 1.05));                             // Set the pulseLow warning flag if pulse rate is out of range
    pulseOutOfRange = ((int*)warningData->pulseRateRawBufPtr)[currentValueIndex] > 115 ||
      ((int*)warningData->pulseRateRawBufPtr)[currentValueIndex] < (60 * 0.85);                               // Set the pulseOutOfRange alarm flag if the pulse rate is out of range by 15% 
    if (pulseLow) { prWarningTime++; }                                                                        // Increment pulse rate warning time if in warning state

    rrLow = (Bool)(((int*)warningData->respirationRateRawBufPtr)[currentValueIndex] < (12 * 0.95) ||
      ((int*)warningData->respirationRateRawBufPtr)[currentValueIndex] > (25 * 1.05));                        // Set the rrLow warning flag if the respiratory rate is out of range
    rrOutOfRange = ((int*)warningData->respirationRateRawBufPtr)[currentValueIndex] < (12 * 0.85) ||
      ((int*)warningData->respirationRateRawBufPtr)[currentValueIndex] > (25 * 1.15);                         // Set the rrOutOfRange alarm flag if the respiratory rate is out of range by 15%
    if (rrLow) { rrWarningTime++; }                                                                           // Increment respiratory rate warning time if in warning state

    ekgHigh = (Bool)(((int*)warningData->ekgFreqBufPtr)[lastEKGProcessIndex] < (35 * 0.95) ||
      ((int*)warningData->ekgFreqBufPtr)[lastEKGProcessIndex] > (3750 * 1.05));                               // Set the ekgHigh flag if the EKG frequency is out of range
    if (ekgHigh) { ekgWarningTime++; }                                                                        // Increment EKG warning time if in warning state

    if (!pulseOutOfRange && !tempOutOfRange && !bpOutOfRange && !rrOutOfRange) {                              // If neither of the alarms are active, reset the alarm acknowledgement status
      alarmAck = 0;
      alarmAckTime = 0;
    }

    if (alarmAck && alarmAckTime < 5) {                                                                       // If not 5 measurement iterations has past since alarm was last acked, we reset all the alarm vairables
      pulseOutOfRange = 0;
      tempOutOfRange = 0;
      bpOutOfRange = 0;
      rrOutOfRange = 0;
    }

    char requestData[80];
    requestData[0] = pulseOutOfRange || tempOutOfRange || bpOutOfRange || rrOutOfRange;
    sendMessage(WarningTask, PWarning, requestData, 1);                                                       // Send a request to peripheral subsystem should any alarm is active.

    batteryLow = (Bool)(*(warningData->batteryStatePtr) < 20);                                                // Set the batteryLow alarm/warning if battery state is out of range

    return;
  }
}


/************************************************************************************************************************************************************
  function name:            display
  function inputs:          pointer to struct containing corrected temperature, pulse rate, pressure, and battery state values
  function outputs:         none
  function description:     Displays the main menu, measurement selection menu, or annunication of measurement values depending on the displayState variable
  author:                   Irina Golub
************************************************************************************************************************************************************/

void display(void* taskDataPtr) {
  static long lastExecutionTime = 0;                                                                        // Initialize the last execution time

  static long tempLastBlinkTime = 0;                                                                        // Initialize the last blink time for temperature
  static int tempeartureOn = 1;                                                                             // Initialize whether to display temperature value

  static long prLastBlinkTime = 0;                                                                          // Initialize the last blink time for pulse rate
  static int puseRateOn = 1;                                                                                // Initialize whether to display pulse rate value

  static long bpLastBlinkTime = 0;                                                                          // Initialize the last blink time for blood pressure
  static int bloodPressureOn = 1;                                                                           // Initialize whether to display blood pressure value

  if (!doRemoteDisplay) {                                                                                   // If remote display is disabled by user, wipe screen black
    tft.fillRect(0, 0, tft.width(), tft.height(), BLACK);
    return;
  }

  DisplayTaskInput* displayData = (DisplayTaskInput*)taskDataPtr;                                           // Cast the input into of type DisplayTaskInput
  tft.setCursor(0, 0);                                                                                      // Reset the display cursor
  int index = lastComputeIndex % MEASURE_BUF_LEN;

  if (3 == (*(displayData->displayStatePtr))) {                                                             // Condition the TFT into normal display state
    tft.setTextSize(1);
    tft.setTextColor(GREEN, BLACK);
    if (measurementSelect & MEASURE_TEMP) {                                                                 // Prints temperature measurement section if measurement select has the temperature flag set
      tft.print((String)("Temperature:\n              ")
        + (String)(int)displayData->tempCorrectedBufPtr[index]
        + (String)(" C\n"));                                                                                // Prints the corrected value for temperature (in C)
    }

    if (measurementSelect & MEASURE_PRES) {                                                                 // Prints blood pressure measurement section if measurement select has the blood pressure flag set
      tft.print((String)("Systolic pressure:\n              ")
        + (String)(int)displayData->bloodPressCorrectedBufPtr[index]
        + String(" mm Hg\n"));                                                                              // Prints the corrected value for systolic pressure (in mmHG)
      tft.print((String)("Diastolic pressure:\n              ")
        + (String)(int)displayData->bloodPressCorrectedBufPtr[index + MEASURE_BUF_LEN]
        + String(" mm Hg\n"));                                                                              // Prints the corrected value for diastolic pressure (in mmHG)
    }

    if (measurementSelect & MEASURE_PULS) {                                                                 // Prints blood pressure measurement section if measurement select has the blood pressure flag set
      tft.print((String)("Pulse Rate:\n              ")
        + (String)(int)displayData->prCorrectedBufPtr[index]
        + (String)(" BPM\n"));                                                                              // Prints the corrected value for pulse rate (in BPM)
    }

    if (measurementSelect & MEASURE_RESP) {                                                                 // Prints blood pressure measurement section if measurement select has the blood pressure flag set
      tft.print((String)("Respiration Rate:\n              ")
        + (String)(int)displayData->respirationRateRawBufPtr[index]
        + (String)("\n"));                                                                                  // Prints the corrected value for pulse rate (in BPM)
    }

    if (measurementSelect & MEASURE_EKGC) {                                                                 // Prints EKG measurement section if measurement select has the EKG flag set
      tft.print((String)("EKG Capture:\n              ")
        + (String)(int)displayData->ekgFreqBufPtr[lastEKGProcessIndex]
        + (String)("\n"));                                                                                  // Prints the corrected value for EKG frequency (in Hz)
    }
  }
  else if (4 == (*(displayData->displayStatePtr))) {                                                        // Else condition the TFT into annunciate statue
    tft.setTextSize(2);

    if (measurementSelect & MEASURE_TEMP) {                                                                 // Prints temperature measurement section if measurement select has the temperature flag set
      tft.setTextColor((int)tempOutOfRange ? RED : tempHigh ? YELLOW : GREEN, BLACK);                       // Sets the text color (red for alarm, yellow for warning, and green for valid)
      if (!tempOutOfRange && tempHigh && globalCounter - tempLastBlinkTime >= TEMPERATURE_RATE)             // If temperature is out of range, initialize blinking for the temperature values
      {
        tempeartureOn = !tempeartureOn;
        tempLastBlinkTime = globalCounter;
      }

      if (!tempeartureOn) { tft.setTextColor(BLACK, BLACK); }                                               // Set text color to black to simulate temperature blinking
      tft.print((String)("Temperature:\n              ")
        + (String)(int)displayData->tempCorrectedBufPtr[index]
        + (String)(" C\n"));                                                                                // Prints the corrected value for temperature (in C)
    }

    if (measurementSelect & MEASURE_PRES) {                                                                 // Prints blood pressure measurement section if measurement select has the blood pressure flag set
      tft.setTextColor((int)bpOutOfRange ? RED : tempHigh ? YELLOW : GREEN, BLACK);                         // Sets the text color (red for alarm, yellow for warning, and green for valid)
      if (!bpOutOfRange && bpHigh && globalCounter - bpLastBlinkTime >= BLOODPRESSURE_RATE)                 // If blood pressure is out of range, initialize blinking for the blood pressure values
      {
        bloodPressureOn = !bloodPressureOn;
        bpLastBlinkTime = globalCounter;
      }

      if (!bloodPressureOn) { tft.setTextColor(BLACK, BLACK); }                                             // Set text color to black to simulate blood pressure blinking
      tft.print((String)("Systolic pressure:\n              ")
        + (String)(int)displayData->bloodPressCorrectedBufPtr[index]
        + String(" mm Hg\n"));                                                                              // Prints the corrected value for systolic pressure (in mmHG)
      tft.print((String)("Diastolic pressure:\n              ")
        + (String)(int)displayData->bloodPressCorrectedBufPtr[index + MEASURE_BUF_LEN]
        + String(" mm Hg\n"));                                                                              // Prints the corrected value for diastolic pressure (in mmHG)
    }

    if (measurementSelect & MEASURE_PULS) {                                                                 // Prints blood pressure measurement section if measurement select has the blood pressure flag set
      tft.setTextColor((int)pulseOutOfRange ? RED : pulseLow ? YELLOW : GREEN, BLACK);                      // Sets the text color (red for alarm, yellow for warning, and green for valid)
      if (!pulseOutOfRange && pulseLow && globalCounter - prLastBlinkTime >= PULSERATE_RATE)                // If pulse rate is out of range, initialize blinking for the pulse rate values
      {
        puseRateOn = !puseRateOn;
        prLastBlinkTime = globalCounter;
      }

      if (!puseRateOn) { tft.setTextColor(BLACK, BLACK); }                                                  // Set text color to black to simulate blood pressure blinking
      tft.print((String)("Pulse Rate:\n              ")
        + (String)(int)displayData->prCorrectedBufPtr[index]
        + (String)(" BPM\n"));                                                                              // Prints the corrected value for pulse rate (in BPM)
    }

    if (measurementSelect & MEASURE_RESP) {                                                                 // Prints blood pressure measurement section if measurement select has the blood pressure flag set
      tft.setTextColor((int)rrOutOfRange ? RED : (rrLow || rrHigh) ? YELLOW : GREEN);                       // Sets the text color (red for alarm, yellow for warning, and green for valid)
      tft.print((String)("Respiration Rate:\n              ")
        + (String)(int)displayData->respirationRateRawBufPtr[index]
        + (String)(" RR\n"));                                                                               // Prints the corrected value for pulse rate (in BPM)
    }

    if (measurementSelect & MEASURE_EKGC) {                                                                 // Prints blood pressure measurement section if measurement select has the blood pressure flag set
      tft.setTextColor((ekgLow || ekgHigh) ? YELLOW : GREEN);                                               // Sets the text color (red for alarm, yellow for warning, and green for valid)
      tft.print((String)("EKG Frequency:\n              ")
        + (String)(int)displayData->ekgFreqBufPtr[lastEKGProcessIndex]
        + (String)(" Hz\n"));                                                                               // Prints the corrected value for pulse rate (in BPM)
    }

    tft.setTextColor(batteryLow ? RED : GREEN, BLACK);                                                      // Sets the text color (red for alarm, yellow for warning, and green for valid)
    tft.print((String)("Battery: ")
      + (String)(*displayData->batteryStatePtr));                                                           // Prints the remaining battery state
  }

  removeTaskArray[2] = TRUE;                                                                                // Remove display task after running
  return;
}


/*****************************************************************************************************************************
  function name:            status
  function inputs:          pointer to struct containing battery state value
  function outputs:         none
  function description:     Updates the battery state
  author:                   Yifan Shao
*****************************************************************************************************************************/

void status(void* input) {
  static long lastExecutionTime = 0;                                                                    // Initialize the last execution time
  if (globalCounter - lastExecutionTime >= TASKDELAY_5s) {                                              // Only execute the function if 5 seconds has passed since the last execution
    lastExecutionTime = globalCounter;                                                                  // Should the rest of function run, update the last execution time to the current time

    StatusTaskInput* statusInput = (StatusTaskInput*)input;
    if (*(statusInput->batteryStatePtr)) {
      (*((StatusTaskInput*)input)->batteryStatePtr)--;                                                  // Decrement the remaining battery by 1
    }
  }

  return;
}


/*****************************************************************************************************************************
function name:            initializeCommandCallback
function inputs:          pointer to initialzie command data
function outputs:         none
function description:     Handler for initialize command (I)
author:                   Yifan Shao
*****************************************************************************************************************************/

void initializeCommandCallback(void* data) {
  InitializeCommandData* d = (InitializeCommandData*)data;                                            // Cast input into of type initialize command data pointer
  if (d->doctorName[0] != '\0' && d->patientName[0] != '\0') {                                        // Parse and store the doctor and patient name should it be available
    doctorName = d->doctorName;
    patientName = d->patientName;
  }
  else {
    char message[80];                                                                                 // Fail with an error message requesting user provide doctor and patient names
    int len = snprintf(message, 80, "Initialize - Doctor or patient name missing.");
    sendRemoteResponse(ErrorResponse, message, len);
  }

  return;
}


/*****************************************************************************************************************************
function name:            startCommandCallback
function inputs:          input pointer
function outputs:         none
function description:     Handler for start measurement command (S)
author:                   Yifan Shao
*****************************************************************************************************************************/

void startCommandCallback(void* data) {
  if (!enableMeasurement) {                                                                           // Set the enable measurement flag to resume interrupts
    enableMeasurement = 1;
  }
  else {                                                                                              // Else fail with error message if measurement is already enabled
    char message[80];
    int len = snprintf(message, 80, "Start - Measurement already started.");
    sendRemoteResponse(ErrorResponse, message, len);
  }

  return;
}


/*****************************************************************************************************************************
function name:            startCommandCallback
function inputs:          input pointer
function outputs:         none
function description:     Handler for stop measurement command (P)
author:                   Yifan Shao
*****************************************************************************************************************************/

void stopCommandCallback(void* data) {
  if (enableMeasurement) {                                                                            // Set the enable measurement flag to resume interrupts
    enableMeasurement = 0;
  }
  else {                                                                                              // Else fail with error message if measurement is already disabled
    char message[80];
    int len = snprintf(message, 80, "Stop - Measurement already stopped.");
    sendRemoteResponse(ErrorResponse, message, len);
  }

  return;
}


/*****************************************************************************************************************************
function name:            displayCommandCallback
function inputs:          input pointer
function outputs:         none
function description:     Handler for enable/disable TFT command (D)
author:                   Yifan Shao
*****************************************************************************************************************************/

void displayCommandCallback(void* data) {
  doRemoteDisplay = !doRemoteDisplay;                                                               // Update/reverse the enable display flag

  return;
}


/*****************************************************************************************************************************
function name:            measureCommandCallback
function inputs:          input pointer
function outputs:         none
function description:     Handler for measure command callback (M)
author:                   Yifan Shao
*****************************************************************************************************************************/

void measureCommandCallback(void* data) {
  int index = lastComputeIndex % MEASURE_BUF_LEN;
  char message[80];
  int len = snprintf(
    message,
    80,
    "Temp:%d,Syst:%d,Dias:%d,Puls:%d,Resp:%d,EKG:%d\n",
    temperatureRawBuf[index],
    bloodPressRawBuf[index],
    bloodPressRawBuf[index + MEASURE_BUF_LEN],
    pulseRateRawBuf[index],
    respirationRateRawBuf[index],
    ekgFreqBuf[lastEKGProcessIndex]);
  sendRemoteResponse(MeasureCommand, message, len);                                                   // Respond to remote caller with most recent measurement values all properties

  return;
}


/*****************************************************************************************************************************
function name:            warningCommandCallback
function inputs:          input pointer
function outputs:         none
function description:     Handler for warning command callback (W)
author:                   Yifan Shao
*****************************************************************************************************************************/

void warningCommandCallback(void* data) {
  int index = lastComputeIndex % MEASURE_BUF_LEN;
  String message = "";
  int addComma = 0;
  if (bpHigh) {
    message += String(addComma ? "," : "") + String("Syst:") + String(bloodPressRawBuf[index]) + String(",Dias:") + String(bloodPressRawBuf[index + MEASURE_BUF_LEN]);
    addComma = 1;
  }

  if (tempHigh) {
    message += String(addComma ? "," : "") + String("Temp:") + String(temperatureRawBuf[index]);
    addComma = 1;
  }

  if (pulseLow) {
    message += String(addComma ? "," : "") + String("Puls:") + String(pulseRateRawBuf[index]);
    addComma = 1;
  }

  if (rrLow || rrHigh) {
    message += String(addComma ? "," : "") + String("Resp:") + String(respirationRateRawBuf[index]);
    addComma = 1;
  }

  if (ekgLow || ekgHigh) {
    message += String(addComma ? "," : "") + String("EKG:") + String(ekgFreqBuf[lastEKGProcessIndex]);
    addComma = 1;
  }

  sendRemoteResponse(WarningCommand, message.c_str(), message.length());                          // Respond to remote caller with most recent measurement values for only out of range properties
  return;
}


/*****************************************************************************************************************************
  function name:            communications
  function inputs:          Struct containing all data necessary for communicating to the Peripheral subsystem
  function outputs:         none
  function description:     Processes any message that is ready and dispatches the appropriate function
  author:                   Eric Ho
*****************************************************************************************************************************/

void communications(void* input) {
  if (comBufReady) {                                                                              // If a Serial 1 message is ready (connection to peripheral)
    CommunicationsTaskInput* taskData = (CommunicationsTaskInput*)input;

    for (int i = 0; i < COM_NUM_BUF; i++)
      if (comBufReady & (1 << i)) {                                                               // Parse the message if it is ready
        char* token;
        token = strtok(comInBuf[i], MESSAGE_TOKENIZER);                                           // Parse START_OF_MESSAGE
        token = strtok(NULL, MESSAGE_TOKENIZER);                                                  // Parse Requesting Task
        TaskID task = static_cast<TaskID>(atoi(token));
        token = strtok(NULL, MESSAGE_TOKENIZER);                                                  // Parse Requested Function
        token = strtok(NULL, MESSAGE_TOKENIZER);                                                  // Parse Input Data
        void* responseData = parseData(token, task);
        token = strtok(NULL, MESSAGE_TOKENIZER);                                                  // Parse END_OF_MESSAGE
        token = strtok(NULL, MESSAGE_TOKENIZER);                                                  // Assert the end of message

        if (task == MeasureTask) {                                                                // Call measureCallback to handle measure responses
          measureCallback(responseData);
        }
        else if (task == EKGTask) {                                                               // Call EKG callback to handle raw EKG responses
          storeEKGValues(responseData);
        }
        // Potential future add-ons

        if (responseData) {                                                                       // Free any allocated data
          free(responseData);
        }
      }
  }

  comBufReady = 0;                                                                              // Reset message ready flags
}

if (remComBufReady) {                                                                           // If a Serial message is ready (connection to PuTTY/user computer)
  for (int i = 0; i < REM_COM_NUM_BUF; i++) {
    if (remComBufReady & (1 << i)) {                                                            // Parse the message if it is ready
      char* token = strtok(remComInBuf[i], MESSAGE_TOKENIZER);                                  // Parse the command, skipping all new line characters client might've introduced
      while (*token == '\r' || *token == '\n' || *token == ' ') {
        Serial.print("\r\nReceived command: ");
        Serial.print((int)*token);
        Serial.print("\r\n");
        token++;
      }

      CommandID command = static_cast<CommandID>(*token);                                         // Extract command Id from token
      token = strtok(NULL, MESSAGE_TOKENIZER);
      void* commandData = parseCommandData(token, command);

      if (command == InitializeCommand) {                                                         // Call initialize command callback if is initialize command
        initializeCommandCallback(commandData);
      }
      else if (command == StartCommand) {                                                         // Call start command callback if is initialize command
        startCommandCallback(commandData);
      }
      else if (command == StopCommand) {                                                          // Call stop command callback if is initialize command
        stopCommandCallback(commandData);
      }
      else if (command == DisplayCommand) {                                                       // Call display command callback if is initialize command
        displayCommandCallback(commandData);
      }
      else if (command == MeasureCommand) {                                                       // Call measure command callback if is initialize command
        measureCommandCallback(commandData);
      }
      else if (command == WarningCommand) {                                                       // Call warning command callback if is initialize command
        warningCommandCallback(commandData);
      }
      else {                                                                                      // If unknown message received, respond to client with error message
        char message[80];
        int len = snprintf(message, 80, "%s", "Unknown command received.");
        sendRemoteResponse(ErrorResponse, message, len);
      }

      if (commandData) {                                                                          // Free any allocated data
        free(commandData);
      }
    }
  }

  remComBufReady = 0;                                                                             // Reset message ready flags
}

return;
}


/*****************************************************************************************************************************
function name:            serialEvent
function inputs:          none
function outputs:         Writes data to remote communications buffers
function description:     Event handler for when a message is received from remote client
author:                   Eric Ho
*****************************************************************************************************************************/

void serialEvent() {
  while (Serial.available()) {                                                                      // While there is still data to read
    char in = (char)Serial.read();                                                                  // Read input date
    if (in == MESSAGE_TERMINATOR) {                                                                 // Process message if it is the end
      remComInBuf[remComInBufNum][remComInBufIndex] = '\0';                                         // Null terminate 
      setFlag(remComBufReady, 1 << remComInBufNum);                                                 // Set flag for message to be processed
      remComInBufNum = (remComInBufNum >= REM_COM_NUM_BUF - 1 ? 0 : remComInBufNum + 1);            // Switch to the next buffer
      remComInBufIndex = 0;                                                                         // Reset buffer index
    }
    else {                                                                                          // Otherwise continue writting to buffer
      remComInBuf[remComInBufNum][remComInBufIndex++] = in;
    }
  }

  return;
}


/*****************************************************************************************************************************
  function name:            serialEvent1
  function inputs:          none
  function outputs:         Writes data to communications buffers
  function description:     Writes incoming data to local buffers, setting a flag when the message is ready
  author:                   Eric Ho
*****************************************************************************************************************************/

void serialEvent1() {
  while (Serial1.available()) {                                                                     // While there is still data to read
    char in = (char)Serial1.read();                                                                 // Read input data
    if (in == MESSAGE_TERMINATOR) {                                                                 // Process the message if it is the end
      comInBuf[comInBufNum][comInBufIndex] = '\0';                                                  // Null terminate string
      setFlag(comBufReady, 1 << comInBufNum);                                                       // Set flag that message is ready
      comInBufNum = (comInBufNum >= COM_NUM_BUF - 1 ? 0 : comInBufNum + 1);                         // Switch to the next buffer
      comInBufIndex = 0;                                                                            // Reset buffer index
    }
    else {                                                                                          // Otherwise continue writting to buffer
      comInBuf[comInBufNum][comInBufIndex++] = in;
    }
  }

  return;
}


/*****************************************************************************************************************************
  function name:            parseData
  function inputs:          The data to be parsed and the task the data is for
  function outputs:         Pointer to a struct containing the parsed data
  function description:     Parses the data for the given task, presuming that the data is properly formed
  author:                   Eric Ho
*****************************************************************************************************************************/

void* parseData(char* data, TaskID task) {
  void* parsed = NULL;
  if (task == MeasureTask) {                                                                          // Set measure properties should it be the received message type
    PMeasureResponseData* d = (PMeasureResponseData*)malloc(sizeof(PMeasureResponseData));            // Allocate data struct
    char* token;
    token = strtok(data, ",");
    d->temperature = atoi(token);                                                                     // Fill in temperature
    token = strtok(NULL, ",");
    d->systolic = atoi(token);                                                                        // Fill in systolic pressure
    token = strtok(NULL, ",");
    d->diastolic = atoi(token);                                                                       // Fill in diastolic pressure
    token = strtok(NULL, ",");
    d->pulse = atoi(token);                                                                           // Fill in pulse rate
    token = strtok(NULL, ",");
    d->resp = atoi(token);                                                                            // Fill in respiratory rate
    token = strtok(NULL, ",");
    d->ekgfreq = atoi(token);                                                                         // Fill in ekg frequency
    parsed = (void*)d;                                                                                // Set return to the struct
  }
  else if (task == EKGTask) {                                                                         // Parse EKG properties
    PEKGResponseData* d = (PEKGResponseData*)malloc(sizeof(PEKGResponseData));
    d->index = atoi(data);
    memcpy(d->data, data + 2, EKG_BLOCK_SIZE);                                                        // Fill in raw EKG bytes
    parsed = (void*)d;
  }
  // Add other tasks here

  return parsed;                                                                                      // Return any parsed data
}


/*****************************************************************************************************************************
function name:            parseCommandData
function inputs:          The data to be parsed and the task the data is for
function outputs:         Pointer to a struct containing the parsed data
function description:     Parses the data for the given task, presuming that the data is properly formed
author:                   Eric Ho
*****************************************************************************************************************************/

void* parseCommandData(char* data, CommandID command) {
  void* parsed = NULL;
  if (command == InitializeCommand) {
    InitializeCommandData* d = (InitializeCommandData*)malloc(sizeof(InitializeCommandData));
    char* token;
    token = strtok(data, ",");
    if (token) {
      strcpy(d->doctorName, token);
    }
    else {
      d->doctorName[0] = '\0';
    }
    token = strtok(NULL, ",");
    if (token) {
      strcpy(d->patientName, token);
    }
    else {
      d->patientName[0] = '\0';
    }
    parsed = (void*)d;
  }

  // Add other commands here

  return parsed;
}


/*****************************************************************************************************************************
  function name:            sendMessage
  function inputs:          The requesting task, requested function, the data for the function, and the length of the data
  function outputs:         nothing
  function description:     Sends a formatted message to the peripheral device, containing the data given
  author:                   Eric Ho
*****************************************************************************************************************************/

void sendMessage(int task, int request, char* data, int data_len) {
  int len = snprintf(comOutBuf, COM_BUF_LEN, "%s%s%d%s%d%s",
    START_OF_MESSAGE, MESSAGE_TOKENIZER, task, MESSAGE_TOKENIZER,
    request, MESSAGE_TOKENIZER);                                                                            // Format the starting part of the message
  Serial1.write(comOutBuf, len);                                                                            // Write the starting part of the message
  Serial1.write(data, data_len);                                                                            // Write the data part of the message
  len = snprintf(comOutBuf, COM_BUF_LEN, "%s%s%c", MESSAGE_TOKENIZER, END_OF_MESSAGE, MESSAGE_TERMINATOR);  // Format the ending part of the message
  Serial1.write(comOutBuf, len);                                                                            // Write the ending part of the message

  return;
}


/*****************************************************************************************************************************
function name:            sendRemoteResponse
function inputs:          The response command, message, and length of message
function outputs:         nothing
function description:     Sends a formatted message to the peripheral device, containing the data given
author:                   Eric Ho
*****************************************************************************************************************************/

void sendRemoteResponse(CommandID command, char* message, int message_len) {
  int len = snprintf(remComOutBuf, REM_COM_BUF_LEN, "\n\r%c: ", command);
  Serial.write(remComOutBuf, len);
  Serial.write(message, message_len);
  Serial.write("\n\rK>");

  return;
}

/*****************************************************************************************************************************
  function name:            remoteDisplaySend
  function inputs:
  function outputs:
  function description:
  author:                   Irina Golub
*****************************************************************************************************************************/

void remoteDisplaySend(void* input) {
  static long lastExecutionTime = 0;                                                                    // Initialize the last execution time
  if (lastExecutionTime == 0 || globalCounter - lastExecutionTime >= TASKDELAY_5s) {                    // Only execute the function if 5 seconds has passed since the last execution
    lastExecutionTime = globalCounter;                                                                  // Should the rest of function run, update the last execution time to the current time

    RemoteDisplayTaskInput* data = (RemoteDisplayTaskInput*)input;
    int index = lastComputeIndex % MEASURE_BUF_LEN;

    String remoteTempLine = String("Temperature:\t" + String(data->tempCorrectedBufPtr[index]) + " C\t(" + String(*data->tempWarningAmtPtr) + " warnings)\r\n");
    String remoteSysPresLine = String("Systolic Pressure:\t" + String(data->bloodPressCorrectedBufPtr[index]) + " mm Hg\t(" + String(*data->bpWarningAmtPtr) + " warnings)\r\n");
    String remoteDiasPresLine = String("Diastolic Pressure:\t" + String(data->bloodPressCorrectedBufPtr[index + MEASURE_BUF_LEN]) + " mm Hg\t(" + String(*data->bpWarningAmtPtr) + " warnings)\r\n");
    String remotePRLine = String("Pulse Rate:\t" + String(data->prCorrectedBufPtr[index]) + " BPM\t(" + String(*data->prWarningAmtPtr) + " warnings)\r\n");
    String remoteRRLine = String("Respiration Rate:\t" + String(data->respirationRateCorrectedBufPtr[index]) + " Breaths Per Minute\t(" + String(*data->rrWarningAmtPtr) + " warnings)\r\n");
    String remoteEKGLine = String("EKG:\t" + String(data->ekgFreqBufPtr[lastEKGProcessIndex]) + " Hz\t(" + String(*data->ekgWarningAmtPtr) + " warnings)\r\n");

    Serial.write(27);                                                                                     // ESC command
    Serial.print("[2J");                                                                                  // Clear screen command - testing to see if this works
    Serial.write(27);
    Serial.print("[H");                                                                                   // Cursor to home command - not sure if needed

    Serial.print(remoteHeader);
    Serial.print(remoteTempLine);
    Serial.print(remoteSysPresLine);
    Serial.print(remoteDiasPresLine);
    Serial.print(remotePRLine);
    Serial.print(remoteRRLine);
    Serial.print(remoteEKGLine);
    Serial.write("\nK>");
  }
}

/*****************************************************************************************************************************
  function name:            keypad
  function inputs:          struct containing necessary keypad variables
  function outputs:         buttons on the TFT display that the user can press
  function description:     Shows appropriate buttons for menu mode, measurement selection mode, and annunciation mode
                            Reads user button presses and changes the display state based on user input
  author:                   Irina Golub
*****************************************************************************************************************************/

void keypad(void* input) {
  KeypadTaskInput* data = (KeypadTaskInput*)input;                                                          // Cast the input into of type KeypadTaskInput
  static int cachedDisplayState = 0;

  if (!doRemoteDisplay) {
    cachedDisplayState = 0;
    tft.fillRect(0, 0, tft.width(), tft.height(), BLACK);
    return;
  }
  // Initialize the last execution time
  int drawButtons = cachedDisplayState != (*(data->displayStatePtr));                                       // Redraw buttons if display state has changed
  if (drawButtons) { tft.fillRect(0, 0, tft.width(), tft.height(), BLACK); }
  cachedDisplayState = *(data->displayStatePtr);

  switch (cachedDisplayState) {                                                                             // Switch the cache display into main menu, menu selection, and announciating modes 
  case 1:
    mainMenu(drawButtons, data->displayStatePtr);                                                           // Create keypad for main menu
    break;
  case 2:

    selectionMenu(data->measurementSelectPtr, drawButtons, data->displayStatePtr);                          // Create keypad for menu selection
    break;
  case 3:
    displayMode(drawButtons, data->displayStatePtr);
    addTaskArray[2] = TRUE;   // Create keypad for display
    break;
  case 4:																		//Create keypad for warning/alarm display
    annunciation(drawButtons, data->displayStatePtr);
    addTaskArray[2] = TRUE;
    break;
  }

  return;
}


/*****************************************************************************************************************************
  function name:            mainMenu
  function inputs:          'drawButtons' integer that tells this function whether to draw the buttons or not
  function outputs:         Two pressable buttons on the lower half of the TFT display: one for 'menu' and one for 'annunciate'
  function description:     Creates the two buttons required for the 'main menu'. Changes display state based on user input
  author:                   Irina Golub

  --This function is based on the 'phonecal-1.ino' example given in the course website--
*****************************************************************************************************************************/

void mainMenu(int drawButtons, unsigned short* displayStatePtr) {
  Elegoo_GFX_Button buttons[3];                                                                       // Declare 2 buttons to use in main menu mode

  buttons[0].initButton(&tft, BUTTON_X_1, BUTTON_Y_1, BUTTON_W_1, BUTTON_H, ILI9341_WHITE,
    ILI9341_PURPLE, ILI9341_WHITE, (char*)"Menu", BUTTON_TEXTSIZE);                                   // Initialize Menu Selection Button

  buttons[1].initButton(&tft, BUTTON_X_1, BUTTON_Y_1 + BUTTON_H + BUTTON_SPACING_Y, BUTTON_W_1,
    BUTTON_H, ILI9341_WHITE, ILI9341_PURPLE, ILI9341_WHITE, (char*)"Display", BUTTON_TEXTSIZE);       // Initialize Announicate Button

  buttons[2].initButton(&tft, BUTTON_X_1, BUTTON_Y_1 + BUTTON_H * 2 + BUTTON_SPACING_Y * 2, BUTTON_W_1,
    BUTTON_H, ILI9341_WHITE, ILI9341_PURPLE, ILI9341_WHITE, (char*)"Annunciate", BUTTON_TEXTSIZE);    // Initialize Announicate Button

  if (drawButtons) { // If the draw button flag is set, draw the menu and annunciate button
    tft.fillRect(0, 0, tft.width(), tft.height(), BLACK);
    delay(50);
    buttons[0].drawButton();
    buttons[1].drawButton();
    buttons[2].drawButton();
  }

  digitalWrite(13, HIGH);                                                                             // Set pin 13 to high
  TSPoint p = ts.getPoint();                                                                          // Get the currently pressured point
  digitalWrite(13, LOW);                                                                              // Reset pin 13 to low

  pinMode(XM, OUTPUT);                                                                                // Set pin XM to output mode
  pinMode(YP, OUTPUT);                                                                                // Set pin YP to output mode

  if (p.z > MINPRESSURE && p.z < MAXPRESSURE) {                                                       // If the pressured point is within valid pressure range, normalize the x and y value of the point
    p.x = map(p.x, TS_MINX, TS_MAXX, tft.width(), 0);
    p.y = map(p.y, TS_MINY, TS_MAXY, tft.height(), 0);
  }

  for (uint8_t i = 0; i < 3; i++) {                                                                   // Loop through the buttons and check if each button is pressed
    if (buttons[i].contains(p.x, p.y)) {                                                              // Button is pressed if the x and y value of the point is inside the button
      buttons[i].press(true);
    }
    else {                                                                                            // Else the button is considered unpressed
      buttons[i].press(false);
    }
  }

  for (uint8_t i = 0; i < 3; i++) {                                                                   // Loop through the buttons and set the display state based on the button pressed
    if (buttons[i].isPressed()) {                                                                     // If the current button is pressed, set button properties
      buttons[i].drawButton(true);                                                                    // Draw the inverse of the button to feedback to the user
      if (i == 0) {                                                                                   // If the 'Menu' button is pressed, set display state to menu selection
        *displayStatePtr = 2;
        delay(100);
      }
      else if (i == 1) {                                                                              // If the 'Announciate' button is pressed, set display state to announciation
        *displayStatePtr = 3;
        delay(100);
      }
      else if (i == 2) {                                                                              // If the 'Announciate' button is pressed, set display state to announciation
        *displayStatePtr = 4;
        delay(100);
      }
    }
    else if (buttons[i].justReleased()) {                                                             // If the button has just been released, redraw the normal button
      buttons[i].drawButton();
    }
  }

  return;
}


/*****************************************************************************************************************************
  function name:            selectionMenu
  function inputs:          a pointer to the 'measurementSelect' variable that stores the user selection of what measurements to take
                            'redrawButton' integer that lets this module know whether to draw the buttons or not
  function outputs:         Three buttons representing the three measurements the device can take. One 'enter' button and one 'exit' button
  function description:     Displays three measurement buttons, one enter button, and an exit button. Reads button presses and changes
                            measurementSelect and display state as necessary
  author:                   Irina Golub

   --This function is based on the 'phonecal-1.ino' example given in the course website--
*****************************************************************************************************************************/

void selectionMenu(unsigned short* selectionPtr, int redrawButton, unsigned short* displayStatePtr) {
  static short buttonsPressed[5] = { 0, 0, 0, 0, 0 };                                                 // Initialize static variable to keep the state of the menu buttons pressed or depressed
  static short buttonsPressed[5] = { 0, 0, 0, 0, 0 };                                                 // Initialize static variable to keep the state of the menu buttons pressed or depressed
  Elegoo_GFX_Button buttons[7];                                                                       // Declare 5 buttons (3 more menu + Enter and Exit) to be used in menu selection mode
  char* buttonlabels[5] = { (char*)"Temp", (char*)"BP", (char*)"Pulse", (char*)"Resprtn", (char*)"EKG" };

  for (int8_t num = 0; num < 5; num++) {                                                              // Loop through and initialize the menu buttons
    buttons[num].initButton(&tft, BUTTON_X_1, BUTTON_Y_2 + (num * (BUTTON_H_2 + BUTTON_SPACING_Y_2)),
      BUTTON_W_2a, BUTTON_H_2, ILI9341_WHITE, ILI9341_BLACK, ILI9341_WHITE,
      buttonlabels[num], BUTTON_TEXTSIZE);                                                            // Initialzie the menu button

    if (redrawButton) {
      buttons[num].drawButton();                                                                      // Redraw the button per necessary
      buttonsPressed[num] = 0;                                                                        // Since button is redrawn, reset the button pressed
    }
  }

  buttons[5].initButton(&tft, BUTTON_X_2b, BUTTON_Y_2 + 5 * BUTTON_SPACING_Y_2 + 5 * BUTTON_H_2 + 10,
    BUTTON_W_2b, BUTTON_H_2, ILI9341_WHITE, ILI9341_GREEN, ILI9341_WHITE,
    (char*)"Enter", BUTTON_TEXTSIZE);                                                                 // Initialize the 'Enter' button 

  buttons[6].initButton(&tft, BUTTON_X_2b + BUTTON_W_2b + BUTTON_SPACING_X,
    BUTTON_Y_2 + 5 * BUTTON_SPACING_Y_2 + 5 * BUTTON_H_2 + 10, BUTTON_W_2b, BUTTON_H_2, ILI9341_WHITE,
    ILI9341_GREEN, ILI9341_WHITE, (char*)"Exit", BUTTON_TEXTSIZE);                                    // Initialize the 'Exit' button

  if (redrawButton) {                                                                                 // Draw Enter and Exit button as necessary
    buttons[5].drawButton();
    buttons[6].drawButton();
  }

  digitalWrite(13, HIGH);                                                                             // Set pin 13 to high
  TSPoint p = ts.getPoint();                                                                          // Get the currently pressured point
  digitalWrite(13, LOW);                                                                              // Reset pin 13 to low

  pinMode(XM, OUTPUT);                                                                                // Set pin XM to output mode
  pinMode(YP, OUTPUT);                                                                                // Set pin YP to output mode

  if (p.z > MINPRESSURE && p.z < MAXPRESSURE) {                                                       // If the pressured point is within valid pressure range, normalize the x and y value of the point
    p.x = map(p.x, TS_MINX, TS_MAXX, tft.width(), 0);
    p.y = map(p.y, TS_MINY, TS_MAXY, tft.height(), 0);
  }

  for (uint8_t i = 0; i < 7; i++) {                                                                   // Loop through the buttons and check if each button is pressed
    if (buttons[i].contains(p.x, p.y)) {                                                              // Button is pressed if the x and y value of the point is inside the button
      buttons[i].press(true);
    }
    else {                                                                                            // Else the button is considered unpressed
      buttons[i].press(false);
    }
  }

  for (uint8_t i = 0; i < 5; i++) {                                                                   // For each of the menu buttons, update and redraw the button as per necessary
    if (buttons[i].isPressed()) {                                                                     // If the button is pressed, re-color the button and set the button pressed flag
      buttonsPressed[i] = !buttonsPressed[i];
      buttons[i].drawButton(buttonsPressed[i]);
    }
  }

  if (buttons[5].isPressed()) {                                                                       // If 'Enter' is pressed, update the flags as per necessary
    setFlag(*selectionPtr, 0);                                                                        // Reset selection pointer as per necessary
    if (buttonsPressed[0]) { setFlag(*selectionPtr, MEASURE_TEMP); buttonsPressed[0] = 0; }           // Set the temperature flag if 1 is pressed
    if (buttonsPressed[1]) { setFlag(*selectionPtr, MEASURE_PRES); buttonsPressed[1] = 0; }           // Set the blood pressure flag if 2 is pressed
    if (buttonsPressed[2]) { setFlag(*selectionPtr, MEASURE_PULS); buttonsPressed[2] = 0; }           // Set the pule rate flag if 3 is pressed
    if (buttonsPressed[3]) { setFlag(*selectionPtr, MEASURE_RESP); buttonsPressed[3] = 0; }           // Set the pule rate flag if 3 is pressed
    if (buttonsPressed[4]) { setFlag(*selectionPtr, MEASURE_EKGC); buttonsPressed[4] = 0; }           // Set the pule rate flag if 3 is pressed
    if (*selectionPtr != 0) { addTaskArray[0] = TRUE; }
    *displayStatePtr = 1;                                                                             // Set display state back to Main Menu mode
  }

  if (buttons[6].isPressed()) {                                                                       // If 'Exit' button is pressed, set display state back to Main Menu mode
    *displayStatePtr = 1;
  }

  return;
}


/*****************************************************************************************************************************
  function name:            displayMode
  function inputs:          'redrawButton' integer to control whether the buttons are re-drawn on the displya or not
  function outputs:         Two pressable buttons on the display: one for exiting, and one for acknowledging an alarm
  function description:     Displays an 'exit' button and an 'ack' button. Scans for user button press and changes alarmAck vriable
                            and display state as necessary
  author:                   Irina Golub

   --This function is based on the 'phonecal-1.ino' example given in the course website--
*****************************************************************************************************************************/

void displayMode(int redrawButton, unsigned short* displayStatePtr) {
  Elegoo_GFX_Button buttons[2];                                                                       // Declare 2 buttons to use in main menu mode

  buttons[0].initButton(&tft, BUTTON_X_1, BUTTON_Y_3, BUTTON_W_1, BUTTON_H, ILI9341_WHITE,
    ILI9341_ORANGE, ILI9341_WHITE, (char*)"Annunciate", BUTTON_TEXTSIZE);                             // Initialize the Acknowledgement button

  buttons[1].initButton(&tft, BUTTON_X_1, BUTTON_Y_3 + BUTTON_H + BUTTON_SPACING_Y, BUTTON_W_1,
    BUTTON_H, ILI9341_WHITE, ILI9341_ORANGE, ILI9341_WHITE, (char*)"Exit", BUTTON_TEXTSIZE);          // Initialize the Exit button

  if (redrawButton) {                                                                                 // Redraw button as per necessary
    buttons[0].drawButton();
    buttons[1].drawButton();
  }

  digitalWrite(13, HIGH);                                                                             // Set pin 13 to high
  TSPoint p = ts.getPoint();                                                                          // Get the currently pressured point
  digitalWrite(13, LOW);                                                                              // Reset pin 13 to low

  pinMode(XM, OUTPUT);                                                                                // Set pin XM to output mode
  pinMode(YP, OUTPUT);                                                                                // Set pin YP to output mode

  if (p.z > MINPRESSURE && p.z < MAXPRESSURE) {                                                       // If the pressured point is within valid pressure range, normalize the x and y value of the point
    p.x = map(p.x, TS_MINX, TS_MAXX, tft.width(), 0);
    p.y = map(p.y, TS_MINY, TS_MAXY, tft.height(), 0);
  }

  for (uint8_t i = 0; i < 2; i++) {                                                                   // Loop through the buttons and check if each button is pressed
    if (buttons[i].contains(p.x, p.y)) {                                                              // Button is pressed if the x and y value of the point is inside the button
      buttons[i].press(true);
    }
    else {                                                                                            // Else the button is considered unpressed
      buttons[i].press(false);
    }
  }

  for (uint8_t i = 0; i < 2; i++) {                                                                   // Loop through the buttons and set the display state based on the button pressed
    if (buttons[i].isPressed()) {                                                                     // If the current button is pressed, set button properties
      buttons[i].drawButton(true);                                                                    // Draw the inverse of the button to feedback to the user
      if (i == 0) {
        *displayStatePtr = 4;
      }
      else if (i == 1) {                                                                              // If the 'Exit' button is pressed, set display state to Main Menu
        *displayStatePtr = 1;
      }
    }
    else if (buttons[i].justReleased()) {                                                             // If the button has just been released, redraw the normal button
      buttons[i].drawButton();
    }
  }

  return;
}


/*****************************************************************************************************************************
  function name:            annunciate
  function inputs:          'redrawButton' integer to control whether the buttons are re-drawn on the displya or not
  function outputs:         Two pressable buttons on the display: one for exiting, and one for acknowledging an alarm
  function description:     Displays an 'exit' button and an 'ack' button. Scans for user button press and changes alarmAck vriable
                            and display state as necessary
  author:                   Irina Golub

   --This function is based on the 'phonecal-1.ino' example given in the course website--
*****************************************************************************************************************************/

void annunciation(int redrawButton, unsigned short* displayStatePtr) {
  Elegoo_GFX_Button buttons[2];                                                                       // Declare 2 buttons to use in main menu mode
  static Bool ackPressed = FALSE;

  buttons[0].initButton(&tft, BUTTON_X_4, BUTTON_Y_4, BUTTON_W_4, BUTTON_H, ILI9341_WHITE,
    ILI9341_ORANGE, ILI9341_WHITE, (char*)"Exit", BUTTON_TEXTSIZE);                                   // Initialize the Acknowledgement button

  buttons[1].initButton(&tft, BUTTON_X_4 + BUTTON_SPACING_X_2 + BUTTON_W_4, BUTTON_Y_4, BUTTON_W_4, BUTTON_H, ILI9341_WHITE,
    ILI9341_ORANGE, ILI9341_WHITE, (char*)"Ack", BUTTON_TEXTSIZE);                                    // Initialize the Acknowledgement button

  if (redrawButton) {
    buttons[0].drawButton();
    buttons[1].drawButton();
  }
  digitalWrite(13, HIGH);                                                                             // Set pin 13 to high
  TSPoint p = ts.getPoint();                                                                          // Get the currently pressured point
  digitalWrite(13, LOW);                                                                              // Reset pin 13 to low

  pinMode(XM, OUTPUT);                                                                                // Set pin XM to output mode
  pinMode(YP, OUTPUT);                                                                                // Set pin YP to output mode

  if (p.z > MINPRESSURE && p.z < MAXPRESSURE) {                                                       // If the pressured point is within valid pressure range, normalize the x and y value of the point
    p.x = map(p.x, TS_MINX, TS_MAXX, tft.width(), 0);
    p.y = map(p.y, TS_MINY, TS_MAXY, tft.height(), 0);
  }

  for (uint8_t i = 0; i < 2; i++) {                                                                   // Loop through the buttons and check if each button is pressed
    if (buttons[i].contains(p.x, p.y)) {                                                              // Button is pressed if the x and y value of the point is inside the button
      buttons[i].press(true);
    }
    else {                                                                                            // Else the button is considered unpressed
      buttons[i].press(false);
    }
  }

  for (uint8_t i = 0; i < 2; i++) {                                                                   // Loop through the buttons and set the display state based on the button pressed
    if (buttons[i].isPressed()) {                                                                     // If the current button is pressed, set button properties
      buttons[i].drawButton(true);                                                                    // Draw the inverse of the button to feedback to the user
      if (i == 0) {
        *displayStatePtr = 1;
      }
      else if (i == 1) {                                                                              // If the 'Exit' button is pressed, set display state to Main Menu
        ackPressed = TRUE;
        alarmAck = 1;                                                                                 // If Ackowledgement button is pressed, this is alarm acknowledgement
        alarmAckTime = 0;
      }
    }
    else if (ackPressed) {                                                                            // If the button has just been released, redraw the normal button
      buttons[1].drawButton();
      ackPressed = FALSE;
    }
  }

  return;
}


/*****************************************************************************************************************************
function name:                insert_task
function inputs:              A pointer to the TCB that is being inserted
function outputs:             none
function description:         inserts the specified task to the dynamic task queue right after the most recent task
author:                       Yifan Shao
*****************************************************************************************************************************/

void insert_task(TCB* task) {
  if (!tasks_head && !tasks_tail) {                                                                 // If both head and tail is null, add task as the only item in the task
    task->nextTask = NULL;                                                                          // Reset the next task pointer in case it is not reset when last deleted
    task->prevTask = NULL;                                                                          // Reset the previous task pointer in case it is not reset when last deleted
    tasks_head = task;                                                                              // Set the task head to the parameter task
    tasks_tail = task;                                                                              // Set the task tail to the parameter task
  }
  else {                                                                                            // Else insert the task behind the task
    tasks_tail->nextTask = task;                                                                    // Insert task as the next task after tasks_tail
    task->prevTask = tasks_tail;                                                                    // Set the previous task of input task as the tail
    task->nextTask = NULL;                                                                          // Reset the next task pointer in case it is not reset when last deleted
    tasks_tail = task;                                                                              // Set the input task as the new tails
  }

  return;
}


/*****************************************************************************************************************************
function name:            delete_task
function inputs:          pointer to the TCB to be deleted from task queue
function outputs:         none
function description:     deletes specified task from dynamic task queue
author:
*****************************************************************************************************************************/

void delete_task(TCB* task) {
  if (task->prevTask == NULL && task->nextTask == NULL) {                                             // Single node scenario, for which we set head and tail to null
    tasks_head = NULL;
    tasks_tail = NULL;
  }
  else if (task->prevTask == NULL) {                                                                  // Head scenario, we set head to the next value
    tasks_head = tasks_head->nextTask;                                                                // Set task head to the value after task head
    tasks_head->prevTask = NULL;
  }
  else if (task->nextTask == NULL) {                                                                  // Tail scenario, we set the tail to the previous value
    tasks_tail = task->prevTask;                                                                      // Set task tail to the value before task tail
    tasks_tail->nextTask = NULL;
  }
  else {                                                                                              // Middle scenario, we remove the task from the pointer prior and after it
    task->prevTask->nextTask = task->nextTask;                                                        // Set the next task pointer of the previous task to the task after the current task
    task->nextTask->prevTask = task->prevTask;                                                        // Set the previous task pointer of the next task to the task before the current task
  }

  task->nextTask = NULL;                                                                              // Set the next task pointer of the current task to null as the task is being removed from the queue
  task->prevTask = NULL;                                                                              // Set the previous task pointer of the current task to null as the task is being removed from the queue

  return;
}

void ekgprocessing() {
  Serial.println("Starting EKG processing");
  COMPLEX x[256];
  COMPLEX w[127];
  for (int i = 0; i < 256; i++) {
    x[i] = { (float)(ekgRawBuf[i]), 0 };
  }

  fft(x, w, (unsigned int)8);
  Serial.println("FFT on input completed");

  int maxIndex = 1;
  float curMaxAmp = 0;
  for (int i = 1; i < 256; i++) {
    if ((x[i].real * x[i].real + x[i].imag * x[i].imag) > curMaxAmp) {
      curMaxAmp = x[i].real * x[i].real + x[i].imag * x[i].imag;
      maxIndex = i;
    }
  }

  lastEKGProcessIndex = (lastEKGProcessIndex + 1) % EKG_FFT_BUF_LEN;
  ekgFreqBuf[lastEKGProcessIndex] = (maxIndex + 1) * 8000.0 / 256.0 / 6.28;
  Serial.println(": done ekg processing");
  return;
}



// FOLLOWING CODE ARE LIBRARY CODE FOR FFT PROCESSING
#ifdef  _C196_
#if _ARCHITECTURE_ != 'KD'
/* This will generate a warning, please check model() control. */
#pragma model(KD)
#endif
#endif

/**************************************************************************

fft - In-place radix 2 decimation in time FFT

x:  pointer to complex array of samples
w:  pointer to temp complex array with size of (n/2)-1
m:   FFT N length in power of 2, N = 2**m = fft length

void fft(COMPLEX *x, COMPLEX *w, int m)

*************************************************************************/

void fft(COMPLEX *x, COMPLEX *w, unsigned int m)
{
  COMPLEX u, temp, tm;
  COMPLEX *xi, *xip, *xj, *wptr;

  unsigned int n;

  unsigned int i, j, k, l, le, windex;

  float arg, w_real, w_imag, wrecur_real, wrecur_imag, wtemp_real;

  /* n = 2**m = fft length */

  n = 1 << m;
  le = n / 2;


  /* calculate the w values recursively */

  arg = 4.0*atan(1.0) / le;         /* PI/le calculation */
  wrecur_real = w_real = cos(arg);
  wrecur_imag = w_imag = -sin(arg);
  xj = w;
  for (j = 1; j < le; j++) {
    xj->real = (float)wrecur_real;
    xj->imag = (float)wrecur_imag;
    xj++;
    wtemp_real = wrecur_real * w_real - wrecur_imag * w_imag;
    wrecur_imag = wrecur_real * w_imag + wrecur_imag * w_real;
    wrecur_real = wtemp_real;
  }


  /* start fft */

  le = n;
  windex = 1;
  for (l = 0; l < m; l++) {
    le = le / 2;

    /* first iteration with no multiplies */

    for (i = 0; i < n; i = i + 2 * le) {
      xi = x + i;
      xip = xi + le;
      temp.real = xi->real + xip->real;
      temp.imag = xi->imag + xip->imag;
      xip->real = xi->real - xip->real;
      xip->imag = xi->imag - xip->imag;
      *xi = temp;
    }

    /* remaining iterations use stored w */

    wptr = w + windex - 1;
    for (j = 1; j < le; j++) {
      u = *wptr;
      for (i = j; i < n; i = i + 2 * le) {
        xi = x + i;
        xip = xi + le;
        temp.real = xi->real + xip->real;
        temp.imag = xi->imag + xip->imag;
        tm.real = xi->real - xip->real;
        tm.imag = xi->imag - xip->imag;
        xip->real = tm.real*u.real - tm.imag*u.imag;
        xip->imag = tm.real*u.imag + tm.imag*u.real;
        *xi = temp;
      }
      wptr = wptr + windex;
    }
    windex = 2 * windex;
  }

  /* rearrange data by bit reversing */

  j = 0;
  for (i = 1; i < (n - 1); i++) {
    k = n / 2;
    while (k <= j) {
      j = j - k;
      k = k / 2;
    }
    j = j + k;
    if (i < j) {
      xi = x + i;
      xj = x + j;
      temp = *xj;
      *xj = *xi;
      *xi = temp;
    }
  }
}
