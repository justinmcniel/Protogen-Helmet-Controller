/*
 * MaxMatrix
 * Version 1.0 Feb 2013
 * Copyright 2013 Oscar Kin-Chung Au
 * Modified by Cryptographic Kangaroo (optimizing flexibility)
 */

#ifndef _MaxMatrix_H_
#define _MaxMatrix_H_

#include "Arduino.h"

#define max7219_reg_noop        0x00
#define max7219_reg_digit0      0x01
#define max7219_reg_digit1      0x02
#define max7219_reg_digit2      0x03
#define max7219_reg_digit3      0x04
#define max7219_reg_digit4      0x05
#define max7219_reg_digit5      0x06
#define max7219_reg_digit6      0x07
#define max7219_reg_digit7      0x08
#define max7219_reg_decodeMode  0x09
#define max7219_reg_intensity   0x0a
#define max7219_reg_scanLimit   0x0b
#define max7219_reg_shutdown    0x0c
#define max7219_reg_displayTest 0x0f

#ifndef PanelWidth
#define PanelWidth 8
#endif

#ifndef PanelHeight
#define PanelHeight 8
#endif

#ifndef MaximumSerialPanels
#define MaximumSerialPanels 14
#endif

#ifndef DefaultIntensity
#define DefaultIntensity 0x0f //15, max
#endif

#define MaximumCols PanelWidth*MaximumSerialPanels
#define MaximumRows PanelHeight

#define MaximumIntensity 0x0f
#define MinimumIntensity 0x00

#define ByteSize 8

class MaxMatrix
{
  private:
    byte data;
    byte load;
    byte clock;
    byte numPanels;
    byte cols; //number of columns on this connection
#if MaximumRows == ByteSize
    byte buffer[MaximumCols];
    #define BufferSize MaximumCols
#elif (MaximumCols*MaximumRows)%ByteSize == 0
    byte buffer[(int)((MaximumCols*MaximumRows)/ByteSize)];
    #define BufferSize (int)((MaximumCols*MaximumRows)/ByteSize)
#else
    byte buffer[(int)((MaximumCols*MaximumRows)/ByteSize) + 1];
    #define BufferSize (int)((MaximumCols*MaximumRows)/ByteSize) + 1
#endif
	
  public:
    MaxMatrix(byte data, byte load, byte clock, byte numPanels);
    
    void init();
    void clear();
    void setCommand(byte command, byte value);
    void setIntensity(byte intensity);
    inline void setBufferColumn(byte col, byte value);
    void setColumn(byte col, byte value);
    void setColumnAll(byte col, byte value);
    void setDot(byte col, byte row, byte value);
    void writeSprite(int x, int y, const byte* sprite);
    void writeSprite(int x, int y, int w, int h, const byte* sprite);
    void setPanelBuffer(int panelNum, const byte* pixelDat); // the length of pixelDat (in bits) must be >= PanelHeight*PanelWidth
    
    void shiftLeft(bool rotate = false, bool fill_zero = true);
    void shiftRight(bool rotate = false, bool fill_zero = true);
    void shiftUp(bool rotate = false);
    void shiftDown(bool rotate = false);
    
    void reload(); // Was Private
};

#endif
