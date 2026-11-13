/****************************************************************************
 Module
   Potentiometer.c

 Revision
   1.0.0

 Description
   Hardware + event checker for the potentiometer (CHEF Task 1).
     Pot ends -> 3.3V and GNDREF, wiper -> RB13 / AN11 (pin 24)
     Reading: 0 (wiper at GND end) .. 1023 (wiper at 3.3V end)
   Check4PotChange() reads the ADC every SAMPLE_TIME ms, averages the last
   NUM_AVG readings, and posts ES_POT_CHANGED to TaskService only when
   that average has moved by at least CHANGE_THRESHOLD (+/-5) counts since
   the last posted value. EventParam = the new (averaged) reading 0..1023.
   Averaging + threshold keep normal ADC noise (about +/-2 counts) from
   producing events while nobody is touching the pot.

 Notes
   RB13 is AN11, so ANSELB bit 13 stays set (analog).
   Never connect the pot to 5V: the ADC input must stay at or below 3.3V.

 History
 When           Who     What/Why
 -------------- ---     --------
 11/12/26       CH      average NUM_AVG readings before the +/-5 check
 11/12/26       CH      started coding (moved ADC setup out of main.c and
                        the reading out of TestHarnessService0.c)
****************************************************************************/
/*----------------------------- Include Files -----------------------------*/
#include "../ProjectHeaders/Potentiometer.h"

#include <xc.h>

#include "ES_Configure.h"
#include "ES_Framework.h"
#include "PIC32_AD_Lib.h"
#include "TaskService.h"

/*----------------------------- Module Defines ----------------------------*/
#define POT_AN_CHANNEL    11      // AN11 = RB13
#define SAMPLE_TIME       20      // ms between ADC reads
#define CHANGE_THRESHOLD  5       // average must move >= 5 counts to post
#define NUM_AVG           8       // readings averaged (8 x 20 ms = 160 ms)

/*---------------------------- Module Variables ---------------------------*/
static uint16_t CurrentPot;       // latest reading
static uint16_t LastPostedPot;    // reading in the last ES_POT_CHANGED
static uint16_t LastSampleTime;
static uint16_t Samples[NUM_AVG]; // last NUM_AVG raw readings
static uint8_t  SampleIndex;      // where the next reading goes
static uint16_t SampleSum;        // running sum of Samples[]

/*---------------------------- Module Functions ---------------------------*/
static uint16_t ReadPot(void);
static uint16_t AddSample(uint16_t Raw);

/*------------------------------ Module Code ------------------------------*/
/****************************************************************************
 Function
     InitPotentiometer

 Description
     RB13 as an analog input, ADC auto-scanning AN11 only. Records the
     starting reading so the first sample doesn't produce a false change.
****************************************************************************/
void InitPotentiometer(void)
{
  ANSELBbits.ANSB13 = 1;     // analog (AN11)
  TRISBbits.TRISB13 = 1;     // input
  ADC_ConfigAutoScan(1u << POT_AN_CHANNEL);

  // fill the averaging buffer with the starting reading
  uint16_t first = ReadPot();
  uint8_t  i;
  for (i = 0; i < NUM_AVG; i++)
  {
    Samples[i] = first;
  }
  SampleSum      = first * NUM_AVG;
  SampleIndex    = 0;

  CurrentPot     = first;
  LastPostedPot  = CurrentPot;
  LastSampleTime = ES_Timer_GetTime();
}

/****************************************************************************
 Function
     Check4PotChange

 Returns
     bool: true if a change was detected and posted

 Description
     Event checker. Called by the framework on every pass; only reads the
     ADC once every SAMPLE_TIME ms.
****************************************************************************/
bool Check4PotChange(void)
{
  bool     ReturnVal = false;
  uint16_t Now       = ES_Timer_GetTime();

  if ((uint16_t)(Now - LastSampleTime) >= SAMPLE_TIME)
  {
    LastSampleTime = Now;
    CurrentPot     = AddSample(ReadPot());   // averaged reading

    uint16_t diff = (CurrentPot > LastPostedPot) ?
                    (CurrentPot - LastPostedPot) : (LastPostedPot - CurrentPot);

    if (diff >= CHANGE_THRESHOLD)
    {
      ES_Event_t ThisEvent;
      ThisEvent.EventType  = ES_POT_CHANGED;
      ThisEvent.EventParam = CurrentPot;   // 0..1023
      PostTaskService(ThisEvent);
      LastPostedPot = CurrentPot;
      ReturnVal = true;
    }
  }

  return ReturnVal;
}

/****************************************************************************
 Function
     QueryCurrentPot

 Returns
     uint16_t: latest averaged reading, 0..1023 (updated every SAMPLE_TIME ms)
****************************************************************************/
uint16_t QueryCurrentPot(void)
{
  return CurrentPot;
}

/***************************************************************************
 private functions
 ***************************************************************************/
static uint16_t ReadPot(void)
{
  uint32_t adcResults[1];
  ADC_MultiRead(adcResults);
  return (uint16_t)adcResults[0];
}

// moving average: replace the oldest reading with Raw, return the average
static uint16_t AddSample(uint16_t Raw)
{
  SampleSum -= Samples[SampleIndex];
  Samples[SampleIndex] = Raw;
  SampleSum += Raw;

  SampleIndex++;
  if (SampleIndex >= NUM_AVG)
  {
    SampleIndex = 0;
  }
  return (uint16_t)((SampleSum + (NUM_AVG / 2)) / NUM_AVG);   // rounded
}

/*------------------------------ End of file ------------------------------*/