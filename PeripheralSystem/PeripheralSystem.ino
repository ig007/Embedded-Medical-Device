#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "System.h"
#include "PeripheralSystem.h"

#include    <float.h>
#include    <math.h>


// Initialize measurement variables
unsigned short measurementSelect;
unsigned int temperatureRaw;
volatile int curPress;
unsigned int systolicPressRaw;
unsigned int diastolicPressRaw;
volatile unsigned long pressISRTime;
volatile short measureFlags;
unsigned int pulseRateRaw;
unsigned int pulseRateBuf[PULS_NUM_PERIOD];
unsigned short pulseRateIndex;
volatile int pulseRateSum;
unsigned int respRateRaw;
unsigned int respRateBuf[RESP_NUM_PERIOD];
unsigned short respRateIndex;
volatile int respRateSum;
volatile int pulseCount;
volatile int respCount;
volatile unsigned int EKGindex;
volatile char EKGRawBuf[EKG_SAMPLES];

// Initialize communication buffers
int comBufReady;
char comOutBuf[COM_BUF_LEN];
char comInBuf[COM_NUM_BUF][COM_BUF_LEN];
short comInBufNum;
short comInBufIndex;
unsigned short scheduleFlags;

void setup() {
    Serial.begin(9600); // Initialize serial port

    measurementSelect = 0;
    temperatureRaw = 75;
    curPress = 100;
    systolicPressRaw = 80;
    diastolicPressRaw = 80;
    pressISRTime = 0;
    measureFlags = SYST_READY | DIAS_READY | EKG_READY;
    pulseRateRaw = 60;
    int pulsePerPeriod = 3; //1.0 * RECORD_PERIOD;
    for(int i = 0; i < PULS_NUM_PERIOD; i ++) {
        pulseRateBuf[i] = pulsePerPeriod;
    }
    pulseRateIndex = 0;
    pulseRateSum = pulsePerPeriod * PULS_NUM_PERIOD;
    respRateRaw = 20;
    int respPerPeriod = 1; //1.0 / 3.0 * RECORD_PERIOD;
    for(int i = 0; i < RESP_NUM_PERIOD; i ++) {
        respRateBuf[i] = respPerPeriod;
    }
    respRateIndex = 0;
    respRateSum = respPerPeriod * RESP_NUM_PERIOD;
    pulseCount = 0;
    respCount = 0;

    comBufReady = 0;
    comInBufNum = 0;
    comInBufIndex = 0;
    scheduleFlags = 0;
  EKGindex = 0;

    noInterrupts();
    TCNT1 = 0; // Clear timer counter
    TCCR1A = 0; // Clear timer1 flags A
    TCCR1B = 0; // Clear timer1 flags B
    TCCR1B |= (1 << CS12); // Set clock prescaler = 1/256
    TCCR1B |= (1 << CS10); // 1/1024
    TCCR1B |= (1 << WGM12); // Set mode to compare
    OCR1A = ((unsigned int)15625 * RECORD_PERIOD) - 1; // Set timer to period sec
    TIMSK1 |= (1 << OCIE1A); // Turn on timer1 compare interrupt A
  
  TCNT2 = 0; // Clear timer2 counter
    TCCR2A = 0; // Clear timer2 flags A
    TCCR2B = 0; // Clear timer2 flags B
    TCCR2B |= (1 << CS11); // Set clock prescaler = 1/8
    TCCR2B |= (1 << CS10); // 1/64
    TCCR2B |= (1 << WGM12); // Set mode to compare
    OCR2A = EKG_PERIOD; // Compare to 24 cycles
    TIMSK2 |= (1 << OCIE2A); // Turn on timer1 compare interrupt A

    PCMSK0 |= (1 << PCINT0); // Enable interrupt D8
    PCMSK0 |= (1 << PCINT1); // Enable interrupt D9
    PCICR |= (1 << PCIE0); // Enable PCINT0
    interrupts();

    // Setup external interrupt for measuring pulse rate
    pinMode(LED_BUILTIN, OUTPUT);
    pinMode(PULS_IN_PIN, INPUT); // Set pin to be input
    attachInterrupt(digitalPinToInterrupt(PULS_IN_PIN), pulseISR, RISING); // Interrupt when signal changes to high
    pinMode(RESP_IN_PIN, INPUT);
    attachInterrupt(digitalPinToInterrupt(RESP_IN_PIN), respISR, RISING);
    pinMode(PRES_LED_PIN, OUTPUT);
    pinMode(PRES_INC_PIN, INPUT);
    pinMode(PRES_SW_PIN, INPUT);
    pinMode(PRES_MEAS_PIN, INPUT);
}

// ISR for pulse rate
void pulseISR() {
    pulseCount++; // Increment pulse count
}

void respISR() {
    respCount++;
}

ISR(TIMER1_COMPA_vect) {
    int pulseCount_ = pulseCount;
    pulseCount = 0;
    pulseRateSum += pulseCount_ - pulseRateBuf[pulseRateIndex];
    pulseRateBuf[pulseRateIndex] = pulseCount_;
    pulseRateIndex = pulseRateIndex >= PULS_NUM_PERIOD - 1 ? 0 : pulseRateIndex ++;
    int respCount_ = respCount;
    respCount = 0;
    respRateSum += respCount_ - respRateBuf[respRateIndex];
    respRateBuf[respRateIndex] = respCount_;
    respRateIndex = respRateIndex >= RESP_NUM_PERIOD - 1 ? 0 : respRateIndex ++;
}

ISR(TIMER2_COMPA_vect) {
    if(measureFlags & EKG_MEASURE) { //create boolean that says whether to read EKG or not
        if(EKGindex < 256) {
            EKGRawBuf[EKGindex] = analogRead(EKG_IN_PIN) / 4; // Read value and place in buffer
            //Serial.println((int)EKGRawBuf[EKGindex]);
            EKGindex++;                                   // Increment buffer index
        } else {
            EKGindex = 0;
            unsetFlag(measureFlags, EKG_MEASURE); // reset EKGindex
            setFlag(measureFlags, EKG_READY);
    }
  }
}

// Pressure
ISR(PCINT0_vect) {
    if(!digitalRead(PRES_INC_PIN)) {
        curPress *= (digitalRead(PRES_SW_PIN) ? 1.1 : 0.9);
        if(!(measureFlags & SYST_READY) && curPress >= 110 && curPress <= 150) {
            digitalWrite(PRES_LED_PIN, HIGH);
        } else if(!(measureFlags & DIAS_READY) && curPress >= 50 && curPress <= 80) {
            digitalWrite(PRES_LED_PIN, HIGH);
        } else {
            digitalWrite(PRES_LED_PIN, LOW);
        }
    }
    if(!digitalRead(PRES_MEAS_PIN)) {
        pressISRTime = millis();
    }
}

// Writes incoming data to local buffer
void serialEvent() {
    while(Serial.available()) { // While there is still data to read
        char in = (char)Serial.read(); // Read data
        if(in == MESSAGE_TERMINATOR) { // If it is the end of the message
            comInBuf[comInBufNum][comInBufIndex] = '\0'; // Null terminate string
            setFlag(comBufReady, 1 << comInBufNum); // Set flag that message is ready
            comInBufNum = (comInBufNum >= COM_NUM_BUF - 1 ? 0 : comInBufNum + 1); // Switch to the next buffer
            comInBufIndex = 0; // Reset buffer index
        } else {
            comInBuf[comInBufNum][comInBufIndex++]= in; // Write to buffer
        }
    }

    // Listens for and dispatches incoming requests
    if(comBufReady) { // If a message is ready
        for(int i = 0; i < COM_NUM_BUF; i ++) { // For each message
            if(comBufReady & (1 << i)) { // If it is ready
                // Parse message
                char* token;
                token = strtok(comInBuf[i], MESSAGE_TOKENIZER); // START_OF_MESSAGE
                token = strtok(NULL, MESSAGE_TOKENIZER); // Requesting Task
                token = strtok(NULL, MESSAGE_TOKENIZER); // Requested Function
                FuncID request = static_cast<FuncID>(atoi(token));
                token = strtok(NULL, MESSAGE_TOKENIZER); // Data
                void* data = parseData(token, request);
                token = strtok(NULL, MESSAGE_TOKENIZER); // END_OF_MESSAGE
                token = strtok(NULL, MESSAGE_TOKENIZER); // should be NULL

                if(request == PMeasure) { // If measure was requested
                    queueMeasure(data); // Measure
                } else if(request == PEKG) {
                    returnEKG(data);
                } else if(request == PWarning) {
                    toggleLED(data);
                }
                // Add other tasks here

                if(data) { // If data was allocated
                    free(data); // Free data
                }
            }
        }
        comBufReady = 0; // Reset message ready flags
    }

    return;
}

// Main scheduler loop
void loop() {
    // Poll delays
    if(pressISRTime && millis() > pressISRTime + 5) {
        pressISRTime = 0;
        if(curPress >= 110) {
            systolicPressRaw = curPress;
            setFlag(measureFlags, SYST_READY);
        } else if (curPress <= 80) {
            diastolicPressRaw = curPress;
            setFlag(measureFlags, DIAS_READY);
        }
    }

    if(scheduleFlags & (1 << PMeasure)) { // PMeasure response pending
        if((measureFlags & SYST_READY) && (measureFlags & DIAS_READY) && (measureFlags & EKG_READY)) {
            recordMeasurements(measurementSelect);
            char response[80];
            int len = snprintf(response, 80, "%d,%d,%d,%d,%d", temperatureRaw, systolicPressRaw, diastolicPressRaw, pulseRateRaw, respRateRaw); // Format the measurements into a string
            sendMessage(MeasureTask, PMeasure, response, len); // Send response message
            unsetFlag(scheduleFlags, 1 << PMeasure);
        }
    }
}

// Signals that measurements are to be made
void queueMeasure(void* input) {
    PMeasureRequestData* data = (PMeasureRequestData*)input; // Cast input to the proper struct
    measurementSelect = data->measurementSelect;
    if(measurementSelect & MEASURE_PRES) {
    unsetFlag(measureFlags, SYST_READY);
    unsetFlag(measureFlags, DIAS_READY);
    }
  if(measurementSelect & MEASURE_EKGC) {
      setFlag(measureFlags, EKG_MEASURE);
      unsetFlag(measureFlags, EKG_READY);
    }
    // Add other measurements that needs pending here
    setFlag(scheduleFlags, 1 << PMeasure);

    return;
}

// Records the measurements selected
void recordMeasurements(short measurementSelect) {
    if(measurementSelect & MEASURE_TEMP) {
        measureTemperature();
    }

    if(measurementSelect & MEASURE_PRES) {
        measurePressure();
    }

    if(measurementSelect & MEASURE_PULS) {
        measurePulse();
    }

    if(measurementSelect & MEASURE_RESP) {
        measureRespiration();
    }
  
  if(measurementSelect & MEASURE_EKGC) {
        measureEKG();
    }

    return;
}

void measureTemperature() {
    temperatureRaw = analogRead(TEMP_IN_PIN);

    return;
}

void measurePressure() {
    return;
}

void measurePulse() {
    pulseRateRaw = (unsigned int)(60.0 * pulseRateSum / PULS_NUM_PERIOD / RECORD_PERIOD);

    return;
}

void measureRespiration() {
    respRateRaw = (unsigned int)(60.0 * respRateSum / RESP_NUM_PERIOD / RECORD_PERIOD);

    return;
}

void measureEKG() {
  return;
}

void returnEKG(void* input) {
//  for(int i = 0; i < 256; i++) {
//    Serial.print(i);
//    Serial.print(": ");
//    Serial.println(EKGRawBuf[i]);
//  }
    int index = *(int*)input;    
    char response[EKG_BLOCK_SIZE + 3];
    int len = snprintf(response, 2, "%d,", index);
    memcpy(response + len, EKGRawBuf + index * EKG_BLOCK_SIZE, EKG_BLOCK_SIZE);
    sendMessage(EKGTask, PEKG, response, EKG_BLOCK_SIZE + len);
}

void toggleLED(void* input) {
    digitalWrite(LED_BUILTIN, *(char*)input ? HIGH : LOW);
}

// Add other tasks here

// Sends response message to system control
void sendMessage(int task, int request, char* data, int data_len) {
    int len = snprintf(comOutBuf, COM_BUF_LEN, "%s%s%d%s%d%s",
                       START_OF_MESSAGE, MESSAGE_TOKENIZER, task, MESSAGE_TOKENIZER,
                       request, MESSAGE_TOKENIZER); // Format the starting part of the message
    Serial.write(comOutBuf, len); // Write the starting part of the message
    Serial.write(data, data_len); // Write the data part of the message
    len = snprintf(comOutBuf, COM_BUF_LEN, "%s%s%c", MESSAGE_TOKENIZER, END_OF_MESSAGE, MESSAGE_TERMINATOR); // Format the ending part of the message
    Serial.write(comOutBuf, len); // Write the ending part of the message

    return;
}

// Parses data for the given function
// Assumes data is of the proper length
void* parseData(char* data, FuncID request) {
    void* parsed = NULL;
    if(request == PMeasure) { // If measure was requested
        PMeasureRequestData* d = (PMeasureRequestData*)malloc(sizeof(PMeasureRequestData)); // Allocate data struct
        d->measurementSelect = atoi(data); // Fill in struct
        parsed = (void*)d; // Set return to the struct
    }
    else if (request == PWarning) {
        char* d = (char*)malloc(sizeof(char));
        *d = data[0];
        parsed = (void*)d;
    } else if (request == PEKG) {
        int* i = (int*)malloc(sizeof(int));
        *i = atoi(data);
        parsed = (void*)i;
    }
    // Add other tasks here
    return parsed; // Return the data struct, if any
}

