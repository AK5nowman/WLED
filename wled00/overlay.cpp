#include "wled.h"

/*
 * Used to draw clock overlays over the strip
 */
#define OL_ANALOGCLOCK 1
#define OL_CRONIXIE 3
#ifdef WLED_ENABLE_SEVENSEG
  #define OL_SEVENSEG 4
#endif
 

void transitionOut(byte overlay)
{
  switch(overlay)
  {
    case OL_CRONIXIE:
      strip.getSegment(0).grouping = 1;
      break;
  }
}

void transitionIn(byte overlay)
{
  switch(overlay)
  {
    case OL_CRONIXIE:
      setCronixie();
      strip.getSegment(0).grouping = 10;
      break;
    #ifdef WLED_ENABLE_SEVENSEG
    case OL_SEVENSEG:
      overlayRefreshMs = 497;
      break;
    #endif
  }
}


void handleOverlays()
{
  if (millis() - overlayRefreshedTime > overlayRefreshMs)
  {
    if(overlayCurrent != overlayPrevious)
    {
      //Call Transitions if overlay has changed.
      transitionOut(overlayPrevious);
      transitionIn(overlayCurrent);
      overlayPrevious = overlayCurrent;
    }

    updateLocalTime();//Should this be called from wled::loop or other - Seems like more than just clock overlays depend on time being updated it should be regularly called?
    checkTimers();    //This doesn't appear to have anything to do with overlays?
    checkCountdown(); //This doesn't appear to have anything to do with overlays?

    if (overlayCurrent == OL_CRONIXIE) _overlayCronixie();//Diamex cronixie clock kit
    #ifdef WLED_ENABLE_SEVENSEG
    if(overlayCurrent == OL_SEVENSEG) _overlaySevenSegmentProcess();
    #endif


    overlayRefreshedTime = millis();
  }
}

void handleOverlayDraw() {
  if (!overlayCurrent) return;
  switch (overlayCurrent)
  {
    case OL_ANALOGCLOCK: _overlayAnalogClock(); break;
    case OL_CRONIXIE: _drawOverlayCronixie(); break;
    #ifdef WLED_ENABLE_SEVENSEG
      case OL_SEVENSEG: _overlaySevenSegmentDraw(); break;
    #endif
  }
}


void _overlayAnalogClock()
{
  int overlaySize = overlayMax - overlayMin +1;
  if (countdownMode)
  {
    _overlayAnalogCountdown(); return;
  }
  double hourP = ((double)(hour(localTime)%12))/12;
  double minuteP = ((double)minute(localTime))/60;
  hourP = hourP + minuteP/12;
  double secondP = ((double)second(localTime))/60;
  int hourPixel = floor(analogClock12pixel + overlaySize*hourP);
  if (hourPixel > overlayMax) hourPixel = overlayMin -1 + hourPixel - overlayMax;
  int minutePixel = floor(analogClock12pixel + overlaySize*minuteP);
  if (minutePixel > overlayMax) minutePixel = overlayMin -1 + minutePixel - overlayMax; 
  int secondPixel = floor(analogClock12pixel + overlaySize*secondP);
  if (secondPixel > overlayMax) secondPixel = overlayMin -1 + secondPixel - overlayMax;
  if (analogClockSecondsTrail)
  {
    if (secondPixel < analogClock12pixel)
    {
      strip.setRange(analogClock12pixel, overlayMax, 0xFF0000);
      strip.setRange(overlayMin, secondPixel, 0xFF0000);
    } else
    {
      strip.setRange(analogClock12pixel, secondPixel, 0xFF0000);
    }
  }
  if (analogClock5MinuteMarks)
  {
    int pix;
    for (int i = 0; i <= 12; i++)
    {
      pix = analogClock12pixel + round((overlaySize / 12.0) *i);
      if (pix > overlayMax) pix -= overlaySize;
      strip.setPixelColor(pix, 0x00FFAA);
    }
  }
  if (!analogClockSecondsTrail) strip.setPixelColor(secondPixel, 0xFF0000);
  strip.setPixelColor(minutePixel, 0x00FF00);
  strip.setPixelColor(hourPixel, 0x0000FF);
  overlayRefreshMs = 998;
}


void _overlayAnalogCountdown()
{
  if (now() < countdownTime)
  {
    long diff = countdownTime - now();
    double pval = 60;
    if (diff > 31557600L) //display in years if more than 365 days
    {
      pval = 315576000L; //10 years
    } else if (diff > 2592000L) //display in months if more than a month
    {
      pval = 31557600L; //1 year
    } else if (diff > 604800) //display in weeks if more than a week
    {
      pval = 2592000L; //1 month
    } else if (diff > 86400) //display in days if more than 24 hours
    {
      pval = 604800; //1 week
    } else if (diff > 3600) //display in hours if more than 60 minutes
    {
      pval = 86400; //1 day
    } else if (diff > 60) //display in minutes if more than 60 seconds
    {
      pval = 3600; //1 hour
    }
    int overlaySize = overlayMax - overlayMin +1;
    double perc = (pval-(double)diff)/pval;
    if (perc > 1.0) perc = 1.0;
    byte pixelCnt = perc*overlaySize;
    if (analogClock12pixel + pixelCnt > overlayMax)
    {
      strip.setRange(analogClock12pixel, overlayMax, ((uint32_t)colSec[3] << 24)| ((uint32_t)colSec[0] << 16) | ((uint32_t)colSec[1] << 8) | colSec[2]);
      strip.setRange(overlayMin, overlayMin +pixelCnt -(1+ overlayMax -analogClock12pixel), ((uint32_t)colSec[3] << 24)| ((uint32_t)colSec[0] << 16) | ((uint32_t)colSec[1] << 8) | colSec[2]);
    } else
    {
      strip.setRange(analogClock12pixel, analogClock12pixel + pixelCnt, ((uint32_t)colSec[3] << 24)| ((uint32_t)colSec[0] << 16) | ((uint32_t)colSec[1] << 8) | colSec[2]);
    }
  }
  overlayRefreshMs = 998;
}


/*
 * Support for the Cronixie clock
 */
 
#ifndef WLED_DISABLE_CRONIXIE
byte _digitOut[6] = {10,10,10,10,10,10};
 
byte getSameCodeLength(char code, int index, char const cronixieDisplay[])
{
  byte counter = 0;
  
  for (int i = index+1; i < 6; i++)
  {
    if (cronixieDisplay[i] == code)
    {
      counter++;
    } else {
      return counter;
    }
  }
  return counter;
}

void setCronixie()
{
  /*
   * digit purpose index
   * 0-9 | 0-9 (incl. random)
   * 10 | blank
   * 11 | blank, bg off
   * 12 | test upw.
   * 13 | test dnw.
   * 14 | binary AM/PM
   * 15 | BB upper +50 for no trailing 0
   * 16 | BBB
   * 17 | BBBB
   * 18 | BBBBB
   * 19 | BBBBBB
   * 20 | H
   * 21 | HH
   * 22 | HHH
   * 23 | HHHH
   * 24 | M
   * 25 | MM
   * 26 | MMM
   * 27 | MMMM
   * 28 | MMMMM
   * 29 | MMMMMM
   * 30 | S
   * 31 | SS
   * 32 | SSS
   * 33 | SSSS
   * 34 | SSSSS
   * 35 | SSSSSS
   * 36 | Y
   * 37 | YY
   * 38 | YYYY
   * 39 | I
   * 40 | II
   * 41 | W
   * 42 | WW
   * 43 | D
   * 44 | DD
   * 45 | DDD
   * 46 | V
   * 47 | VV
   * 48 | VVV
   * 49 | VVVV
   * 50 | VVVVV
   * 51 | VVVVVV
   * 52 | v
   * 53 | vv
   * 54 | vvv
   * 55 | vvvv
   * 56 | vvvvv
   * 57 | vvvvvv
   */

  //H HourLower | HH - Hour 24. | AH - Hour 12. | HHH Hour of Month | HHHH Hour of Year
  //M MinuteUpper | MM Minute of Hour | MMM Minute of 12h | MMMM Minute of Day | MMMMM Minute of Month | MMMMMM Minute of Year
  //S SecondUpper | SS Second of Minute | SSS Second of 10 Minute | SSSS Second of Hour | SSSSS Second of Day | SSSSSS Second of Week
  //B AM/PM | BB 0-6/6-12/12-18/18-24 | BBB 0-3... | BBBB 0-1.5... | BBBBB 0-1 | BBBBBB 0-0.5
  
  //Y YearLower | YY - Year LU | YYYY - Std.
  //I MonthLower | II - Month of Year 
  //W Week of Month | WW Week of Year
  //D Day of Week | DD Day Of Month | DDD Day Of Year

  DEBUG_PRINT("cset ");
  DEBUG_PRINTLN(cronixieDisplay);

  overlayRefreshMs = 1997; //Only refresh every 2secs if no seconds are displayed
  
  for (int i = 0; i < 6; i++)
  {
    dP[i] = 10;
    switch (cronixieDisplay[i])
    {
      case '_': dP[i] = 10; break; 
      case '-': dP[i] = 11; break; 
      case 'r': dP[i] = random(1,7); break; //random btw. 1-6
      case 'R': dP[i] = random(0,10); break; //random btw. 0-9
      //case 't': break; //Test upw.
      //case 'T': break; //Test dnw.
      case 'b': dP[i] = 14 + getSameCodeLength('b',i,cronixieDisplay); i = i+dP[i]-14; break; 
      case 'B': dP[i] = 14 + getSameCodeLength('B',i,cronixieDisplay); i = i+dP[i]-14; break;
      case 'h': dP[i] = 70 + getSameCodeLength('h',i,cronixieDisplay); i = i+dP[i]-70; break;
      case 'H': dP[i] = 20 + getSameCodeLength('H',i,cronixieDisplay); i = i+dP[i]-20; break;
      case 'A': dP[i] = 108; i++; break;
      case 'a': dP[i] = 58; i++; break;
      case 'm': dP[i] = 74 + getSameCodeLength('m',i,cronixieDisplay); i = i+dP[i]-74; break;
      case 'M': dP[i] = 24 + getSameCodeLength('M',i,cronixieDisplay); i = i+dP[i]-24; break;
      case 's': dP[i] = 80 + getSameCodeLength('s',i,cronixieDisplay); i = i+dP[i]-80; overlayRefreshMs = 497; break; //refresh more often bc. of secs
      case 'S': dP[i] = 30 + getSameCodeLength('S',i,cronixieDisplay); i = i+dP[i]-30; overlayRefreshMs = 497; break;
      case 'Y': dP[i] = 36 + getSameCodeLength('Y',i,cronixieDisplay); i = i+dP[i]-36; break; 
      case 'y': dP[i] = 86 + getSameCodeLength('y',i,cronixieDisplay); i = i+dP[i]-86; break; 
      case 'I': dP[i] = 39 + getSameCodeLength('I',i,cronixieDisplay); i = i+dP[i]-39; break;  //Month. Don't ask me why month and minute both start with M.
      case 'i': dP[i] = 89 + getSameCodeLength('i',i,cronixieDisplay); i = i+dP[i]-89; break; 
      //case 'W': break;
      //case 'w': break;
      case 'D': dP[i] = 43 + getSameCodeLength('D',i,cronixieDisplay); i = i+dP[i]-43; break;
      case 'd': dP[i] = 93 + getSameCodeLength('d',i,cronixieDisplay); i = i+dP[i]-93; break;
      case '0': dP[i] = 0; break;
      case '1': dP[i] = 1; break;
      case '2': dP[i] = 2; break;
      case '3': dP[i] = 3; break;
      case '4': dP[i] = 4; break;
      case '5': dP[i] = 5; break;
      case '6': dP[i] = 6; break;
      case '7': dP[i] = 7; break;
      case '8': dP[i] = 8; break;
      case '9': dP[i] = 9; break;
      //case 'V': break; //user var0
      //case 'v': break; //user var1
    }
  }
  DEBUG_PRINT("result ");
  for (int i = 0; i < 5; i++)
  {
    DEBUG_PRINT((int)dP[i]);
    DEBUG_PRINT(" ");
  }
  DEBUG_PRINTLN((int)dP[5]);

  _overlayCronixie(); //refresh
}

void _overlayCronixie()
{
  byte h = hour(localTime);
  byte h0 = h;
  byte m = minute(localTime);
  byte s = second(localTime);
  byte d = day(localTime);
  byte mi = month(localTime);
  int y = year(localTime);
  //this has to be changed in time for 22nd century
  y -= 2000; if (y<0) y += 30; //makes countdown work

  if (useAMPM && !countdownMode)
  {
    if (h>12) h-=12;
    else if (h==0) h+=12;
  }
  for (int i = 0; i < 6; i++)
  {
    if (dP[i] < 12) _digitOut[i] = dP[i];
    else {
      if (dP[i] < 65)
      {
        switch(dP[i])
        {
          case 21: _digitOut[i] = h/10; _digitOut[i+1] = h- _digitOut[i]*10; i++; break; //HH
          case 25: _digitOut[i] = m/10; _digitOut[i+1] = m- _digitOut[i]*10; i++; break; //MM
          case 31: _digitOut[i] = s/10; _digitOut[i+1] = s- _digitOut[i]*10; i++; break; //SS

          case 20: _digitOut[i] = h- (h/10)*10; break; //H
          case 24: _digitOut[i] = m/10; break; //M
          case 30: _digitOut[i] = s/10; break; //S
          
          case 43: _digitOut[i] = weekday(localTime); _digitOut[i]--; if (_digitOut[i]<1) _digitOut[i]= 7; break; //D
          case 44: _digitOut[i] = d/10; _digitOut[i+1] = d- _digitOut[i]*10; i++; break; //DD
          case 40: _digitOut[i] = mi/10; _digitOut[i+1] = mi- _digitOut[i]*10; i++; break; //II
          case 37: _digitOut[i] = y/10; _digitOut[i+1] = y- _digitOut[i]*10; i++; break; //YY
          case 39: _digitOut[i] = 2; _digitOut[i+1] = 0; _digitOut[i+2] = y/10; _digitOut[i+3] = y- _digitOut[i+2]*10; i+=3; break; //YYYY
          
          //case 16: _digitOut[i+2] = ((h0/3)&1)?1:0; i++; //BBB (BBBB NI)
          //case 15: _digitOut[i+1] = (h0>17 || (h0>5 && h0<12))?1:0; i++; //BB
          case 14: _digitOut[i] = (h0>11)?1:0; break; //B
        }
      } else
      {
        switch(dP[i])
        {
          case 71: _digitOut[i] = h/10; _digitOut[i+1] = h- _digitOut[i]*10; if(_digitOut[i] == 0) _digitOut[i]=10; i++; break; //hh
          case 75: _digitOut[i] = m/10; _digitOut[i+1] = m- _digitOut[i]*10; if(_digitOut[i] == 0) _digitOut[i]=10; i++; break; //mm
          case 81: _digitOut[i] = s/10; _digitOut[i+1] = s- _digitOut[i]*10; if(_digitOut[i] == 0) _digitOut[i]=10; i++; break; //ss
          //case 66: _digitOut[i+2] = ((h0/3)&1)?1:10; i++; //bbb (bbbb NI)
          //case 65: _digitOut[i+1] = (h0>17 || (h0>5 && h0<12))?1:10; i++; //bb
          case 64: _digitOut[i] = (h0>11)?1:10; break; //b

          case 93: _digitOut[i] = weekday(localTime); _digitOut[i]--; if (_digitOut[i]<1) _digitOut[i]= 7; break; //d
          case 94: _digitOut[i] = d/10; _digitOut[i+1] = d- _digitOut[i]*10; if(_digitOut[i] == 0) _digitOut[i]=10; i++; break; //dd
          case 90: _digitOut[i] = mi/10; _digitOut[i+1] = mi- _digitOut[i]*10; if(_digitOut[i] == 0) _digitOut[i]=10; i++; break; //ii
          case 87: _digitOut[i] = y/10; _digitOut[i+1] = y- _digitOut[i]*10; i++; break; //yy
          case 89: _digitOut[i] = 2; _digitOut[i+1] = 0; _digitOut[i+2] = y/10; _digitOut[i+3] = y- _digitOut[i+2]*10; i+=3; break; //yyyy
        }
      }
    }
  }
}

void _drawOverlayCronixie()
{
  byte offsets[] = {5, 0, 6, 1, 7, 2, 8, 3, 9, 4};
  
  for (uint16_t i = 0; i < 6; i++)
  {
    byte o = 10*i;
    byte excl = 10;
    if(_digitOut[i] < 10) excl = offsets[_digitOut[i]];
    excl += o;
    
    if (cronixieBacklight && _digitOut[i] <11)
    {
      uint32_t col = strip.gamma32(strip.getSegment(0).colors[1]);
      for (uint16_t j=o; j< o+10; j++) {
        if (j != excl) strip.setPixelColor(j, col);
      }
    } else
    {
      for (uint16_t j=o; j< o+10; j++) {
        if (j != excl) strip.setPixelColor(j, 0);
      }
    }
  }
}

#else // WLED_DISABLE_CRONIXIE
byte getSameCodeLength(char code, int index, char const cronixieDisplay[]) {}
void setCronixie() {}
void _overlayCronixie() {}
void _drawOverlayCronixie() {}
#endif


#ifdef WLED_ENABLE_SEVENSEG
//This will handle the actual pixel setting based on physical config and message.
void _overlaySevenSegmentDraw()
{
  
  //Start pixels at ssStartLED, Use ssLEDPerSegment, ssLEDPerPeriod, ssDisplayBuffer
  int indexLED = 0;
  for(int indexBuffer = 0; indexBuffer < WLED_SS_BUFFLEN; indexBuffer++)
  {
    if(ssDisplayBuffer[indexBuffer] == 0) break;
    else if(ssDisplayBuffer[indexBuffer] == '.')
    {
      //Won't ever turn off LED lights for a period. (or will we?)
      indexLED += ssLEDPerPeriod; 
      continue;
    }
    else if(ssDisplayBuffer[indexBuffer] == ':')
    {
      //Turn off colon if odd second?
      indexLED += ssLEDPerPeriod * 2;
    }
    else if(ssDisplayBuffer[indexBuffer] == ' ')
    {
      //Turn off all 7 segments.
      _overlaySevenSegmentLEDOutput(0, indexLED);
      indexLED += ssLEDPerSegment * 7;
    }
    else
    {
      //Turn off correct segments.
      _overlaySevenSegmentLEDOutput(_overlaySevenSegmentGetCharMask(ssDisplayBuffer[indexBuffer]), indexLED);
      indexLED += ssLEDPerSegment * 7;
    }
    
  }

}
void _overlaySevenSegmentLEDOutput(char mask, int indexLED)
{
  for(char index = 0; index < 7; index++)
  {
    if((mask & (0x40 >> index)) != (0x40 >> index))
    {
      for(int numPerSeg = 0; numPerSeg < ssLEDPerSegment;  numPerSeg++)
      {
        strip.setPixelColor(indexLED, 0x000000);
      }
    }
    indexLED += ssLEDPerSegment;
  }
}
//This should process what the digits should display with xxDraw is called. 
void _overlaySevenSegmentProcess()
{
  //Do time for now.
  if(!ssDoDisplayMessage)
  {
    //Format the ssDisplayBuffer based on ssDisplayMask
    for(int index = 0; index < WLED_SS_BUFFLEN; index++)
    {
      //Only look for time formatting if there are at least 2 characters left in the buffer.
      if((index < WLED_SS_BUFFLEN - 1) && (ssDisplayMask[index] == ssDisplayMask[index + 1]))
      {
        int timeVar = 0;
        switch(ssDisplayMask[index])
        {
          case 'h':
            timeVar = hourFormat12(localTime);
            break;
          case 'H':
            timeVar = hour(localTime);
            break;
          case 'k':
            timeVar = hour(localTime) + 1;
            break;
          case 'M':
          case 'm':
            timeVar = minute(localTime);
            break;
          case 'S':
          case 's':
            timeVar = second(localTime);
            break;

        }

        //Only want to leave a blank in the hour formatting. 
        if((ssDisplayMask[index] == 'h' || ssDisplayMask[index] == 'H' || ssDisplayMask[index] == 'k') && timeVar < 10)
          ssDisplayBuffer[index] = ' ';
        else
          ssDisplayBuffer[index] = 0x30 + (timeVar / 10);
        ssDisplayBuffer[index + 1] = 0x30 + (timeVar % 10);  

        //Need to increment the index because of the second digit.
        index++;
      }
      else 
      {
        ssDisplayBuffer[index] = (ssDisplayMask[index] == ':' ? ':' : ' ');
      }
    }
  }
  else
  {
    /* This will handle displaying a message and the scrolling of the message if its longer than the buffer length */
    
  }
}

char _overlaySevenSegmentGetCharMask(char var)
{
  //ssCharacterMask
  var -= 0x30;
  if(var > 0x30)
    var -= 0x20;
  else if(var > 0x40)
    var -= 0x10;
  
  char mask = ssCharacterMask[var];
 /*
  0 - EDCGFAB
  1 - EDCBAFG
  2 - GCDEFAB
  3 - GBAFEDC
  4 - FABGEDC
  5 - FABCDEG
  */
  switch(ssDisplayConfig)
  {
    case 1:
      mask = _overlaySevenSegmentSwapBits(mask, 0, 3, 1);
      mask = _overlaySevenSegmentSwapBits(mask, 1, 2, 1);
      break;
    case 2:
      mask = _overlaySevenSegmentSwapBits(mask, 3, 6, 1);
      mask = _overlaySevenSegmentSwapBits(mask, 4, 5, 1);
      break;
    case 3:
      mask = _overlaySevenSegmentSwapBits(mask, 0, 4, 3);
      mask = _overlaySevenSegmentSwapBits(mask, 3, 6, 1);
      mask = _overlaySevenSegmentSwapBits(mask, 4, 5, 1);
      break;
    case 4:
      mask = _overlaySevenSegmentSwapBits(mask, 0, 4, 3);
      break;
    case 5:
      mask = _overlaySevenSegmentSwapBits(mask, 0, 4, 3);
      mask = _overlaySevenSegmentSwapBits(mask, 0, 3, 1);
      mask = _overlaySevenSegmentSwapBits(mask, 1, 2, 1);
      break;
  }
  return mask;
}
char _overlaySevenSegmentSwapBits(char x, char p1, char p2, char n)
{
    /* Move all bits of first set to rightmost side */
    char set1 = (x >> p1) & ((1U << n) - 1);
 
    /* Move all bits of second set to rightmost side */
    char set2 = (x >> p2) & ((1U << n) - 1);
 
    /* Xor the two sets */
    char Xor = (set1 ^ set2);
 
    /* Put the Xor bits back to their original positions */
    Xor = (Xor << p1) | (Xor << p2);
 
    /* Xor the 'Xor' with the original number so that the 
    two sets are swapped */
    char result = x ^ Xor;
 
    return result;
}
#endif



