/****************************************************************************
 Module
   DisplayLink.c  (leader PIC32)

 Revision
   1.0.0

 Description
   SPI1 leader (master), transmit only, 8-bit, SPI mode 0
   (CKP = 0 clock idles low, CKE = 1 data valid on the rising edge),
   500 kHz. These settings MUST match SPIFollower.c on the display PIC32.

   Chip select (RA0) is driven by this module as a plain output and held
   low for a whole message, so the follower sees one message per CS-low
   period. Each message is short (at most ~25 bytes = ~0.4 ms), so the
   send simply waits for the hardware to finish; that is short enough not
   to disturb the Events and Services framework.

   Only game-facing messages go to the display. The event-debug prints
   stay on PuTTY (DB_printf).

 Notes
   RA0 and RA1 are AN0/AN1 and RB14 is AN10 at reset: made digital here.
   PBCLK is 20 MHz (ME218 framework), so SPI1BRG = 19 gives 500 kHz.

 History
 When           Who     What/Why
 -------------- ---     --------
 11/14/26       CH      started coding
****************************************************************************/
/*----------------------------- Include Files -----------------------------*/
#include "../ProjectHeaders/DisplayLink.h"

#include <xc.h>
#include <stdarg.h>
#include <stdio.h>

/*----------------------------- Module Defines ----------------------------*/
#define MSG_STX         0x02
#define MSG_END         0x00
#define CMD_CLEAR       'C'
#define CMD_LINE        'L'

#define MAX_TEXT        20          // follower ignores anything longer
#define SPI_BRG_500KHZ  19          // 20 MHz / (2 * (19 + 1)) = 500 kHz

#define CS_LAT          LATAbits.LATA0
#define CS_ACTIVE       0
#define CS_IDLE         1
#define CS_SETUP_NOPS   20          // ~1 us at 20 MHz before/after a frame

/*---------------------------- Module Functions ---------------------------*/
static void BeginFrame(void);
static void SendByte(uint8_t b);
static void EndFrame(void);
static void DrainRx(void);
static void ShortDelay(void);

/*------------------------------ Module Code ------------------------------*/
/****************************************************************************
 Function
     InitDisplayLink

 Description
     RA0 = CS output (idle high), RA1 = SDO1, RB14 = SCK1, SPI1 as leader
****************************************************************************/
void InitDisplayLink(void)
{
  // pins digital
  ANSELAbits.ANSA0  = 0;
  ANSELAbits.ANSA1  = 0;
  ANSELBbits.ANSB14 = 0;

  CS_LAT = CS_IDLE;                 // set level before making it an output
  TRISAbits.TRISA0  = 0;            // CS out
  TRISAbits.TRISA1  = 0;            // SDO1 out
  TRISBbits.TRISB14 = 0;            // SCK1 out

  RPA1R = 0b0011;                   // PPS: SDO1 -> RPA1 (datasheet Table 11-2)

  // SPI1: leader, 8-bit, mode 0, enhanced (16-byte) buffer, transmit only
  SPI1CON  = 0;
  SPI1CON2 = 0;
  (void)SPI1BUF;
  SPI1BRG  = SPI_BRG_500KHZ;
  SPI1STATbits.SPIROV = 0;

  SPI1CONbits.MSTEN  = 1;           // leader
  SPI1CONbits.CKP    = 0;           // clock idles low
  SPI1CONbits.CKE    = 1;           // data valid on rising edge (mode 0)
  SPI1CONbits.SMP    = 0;
  SPI1CONbits.MODE16 = 0;           // 8-bit
  SPI1CONbits.MODE32 = 0;
  SPI1CONbits.DISSDI = 1;           // no data comes back
  SPI1CONbits.ENHBUF = 1;

  SPI1CONbits.ON = 1;
}

/****************************************************************************
 Function
     DisplayClear

 Description
     Blanks the whole display
****************************************************************************/
void DisplayClear(void)
{
  BeginFrame();
  SendByte(MSG_STX);
  SendByte(CMD_CLEAR);
  SendByte(MSG_END);
  EndFrame();
}

/****************************************************************************
 Function
     DisplayLine

 Description
     Replaces line Line (0 = top .. 3) with Text, centered, in Color.
     Text longer than MAX_TEXT characters is cut off.
****************************************************************************/
void DisplayLine(uint8_t Line, DisplayColor_t Color, const char *Text)
{
  uint8_t n = 0;

  if (Line >= DISPLAY_NUM_LINES)
  {
    return;
  }

  BeginFrame();
  SendByte(MSG_STX);
  SendByte(CMD_LINE);
  SendByte((uint8_t)('0' + Line));
  SendByte((uint8_t)Color);
  while ((*Text != '\0') && (n < MAX_TEXT))
  {
    uint8_t c = (uint8_t)*Text++;
    if ((c != MSG_STX) && (c != MSG_END))   // never send framing bytes
    {
      SendByte(c);
      n++;
    }
  }
  SendByte(MSG_END);
  EndFrame();
}

/****************************************************************************
 Function
     DisplayLinef

 Description
     printf-style version of DisplayLine, e.g.
       DisplayLinef(3, DISP_YELLOW, "%dPT %dS", userScore, secondsLeft);
****************************************************************************/
void DisplayLinef(uint8_t Line, DisplayColor_t Color, const char *Format, ...)
{
  char    text[MAX_TEXT + 1];
  va_list args;

  va_start(args, Format);
  vsnprintf(text, sizeof(text), Format, args);
  va_end(args);

  DisplayLine(Line, Color, text);
}

/***************************************************************************
 private functions
 ***************************************************************************/
static void BeginFrame(void)
{
  CS_LAT = CS_ACTIVE;
  ShortDelay();                     // let the follower see CS before clocks
}

static void SendByte(uint8_t b)
{
  while (SPI1STATbits.SPITBF)       // wait for room in the TX FIFO
  {
    DrainRx();
  }
  SPI1BUF = b;
  DrainRx();
}

// wait until every byte has fully left the shift register, then release CS
static void EndFrame(void)
{
  while (!SPI1STATbits.SPITBE || !SPI1STATbits.SRMT)
  {
    DrainRx();
  }
  DrainRx();
  ShortDelay();
  CS_LAT = CS_IDLE;
  ShortDelay();                     // small gap between messages
}

// the leader receives a (meaningless) byte for every byte it sends:
// throw them away so the receive FIFO never overflows
static void DrainRx(void)
{
  while (!SPI1STATbits.SPIRBE)
  {
    (void)SPI1BUF;
  }
  SPI1STATbits.SPIROV = 0;
}

static void ShortDelay(void)
{
  volatile uint8_t i;
  for (i = 0; i < CS_SETUP_NOPS; i++)
  {
    ;
  }
}

/*------------------------------ End of file ------------------------------*/
