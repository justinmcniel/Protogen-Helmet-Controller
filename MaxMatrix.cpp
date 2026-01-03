/*
 * MaxMatrix
 * Version 1.0 Feb 2013
 * Copyright 2013 Oscar Kin-Chung Au
 */


#include "Arduino.h"
#include "MaxMatrix.h"

MaxMatrix::MaxMatrix(byte _data, byte _load, byte _clock, byte _numPanels) 
{
	data = _data;
	load = _load;
	clock = _clock;
	numPanels = min(_numPanels, MaximumSerialPanels);
  cols = numPanels*PanelWidth;
	for (int i=0; i<BufferSize; i++)
		buffer[i] = 0;
}

void MaxMatrix::init()
{
	pinMode(data,  OUTPUT);
	pinMode(clock, OUTPUT);
	pinMode(load,  OUTPUT);
	digitalWrite(clock, HIGH); 

  /// TODO: Find out how these work and if I want to use them
	setCommand(max7219_reg_scanLimit, 0x07);      
	setCommand(max7219_reg_decodeMode, 0x00);  // using an led matrix (not digits)
	setCommand(max7219_reg_shutdown, 0x01);    // not in shutdown mode
	setCommand(max7219_reg_displayTest, 0x00); // no display test
	
	// empty registers, turn all LEDs off
	clear();
	
	setIntensity(DefaultIntensity);    // the first 0x0f is the value you can set
}

void MaxMatrix::setIntensity(byte intensity)
{
	setCommand(max7219_reg_intensity, intensity);
}

void MaxMatrix::clear()
{
  /// TODO: figure out why this is 8
	for (int i=0; i<8; i++) 
		setColumnAll(i,0);
		
	for (int i=0; i<cols; i++)
		buffer[i] = 0;
}

void MaxMatrix::setCommand(byte command, byte value)
{
	digitalWrite(load, LOW);    
	for (int i=0; i<numPanels; i++) 
	{
		shiftOut(data, clock, MSBFIRST, command);
		shiftOut(data, clock, MSBFIRST, value);
	}
	digitalWrite(load, LOW);
	digitalWrite(load, HIGH);
}

void MaxMatrix::setBufferColumn(byte col, byte value)
{
  buffer[col] = value;
}

void MaxMatrix::setColumn(byte col, byte value)
{
	int panelItIsOn = col / PanelWidth;
	int colToSet = col % PanelWidth;
	digitalWrite(load, LOW);    
	for (int i=0; i<numPanels; i++) 
	{
		if (i == panelItIsOn)
		{
			shiftOut(data, clock, MSBFIRST, colToSet + 1);
			shiftOut(data, clock, MSBFIRST, value);
		}
		else
		{
			shiftOut(data, clock, MSBFIRST, 0);
			shiftOut(data, clock, MSBFIRST, 0);
		}
	}
	digitalWrite(load, LOW);
	digitalWrite(load, HIGH);
	
	setBufferColumn(col, value);
}

void MaxMatrix::setColumnAll(byte col, byte value)
{
	digitalWrite(load, LOW);    
	for (int i=0; i<numPanels; i++) 
	{
		shiftOut(data, clock, MSBFIRST, col + 1);
		shiftOut(data, clock, MSBFIRST, value);
		buffer[col * i] = value;
	}
	digitalWrite(load, LOW);
	digitalWrite(load, HIGH);
}

void MaxMatrix::setDot(byte col, byte row, byte value)
{
    bitWrite(buffer[col], row, value);

	int panelItIsOn = col / PanelWidth;
	int colToSet = col % PanelWidth;
	digitalWrite(load, LOW);    
	for (int i=0; i<numPanels; i++) 
	{
		if (i == panelItIsOn)
		{
			shiftOut(data, clock, MSBFIRST, colToSet + 1);
			shiftOut(data, clock, MSBFIRST, buffer[col]);
		}
		else
		{
			shiftOut(data, clock, MSBFIRST, 0);
			shiftOut(data, clock, MSBFIRST, 0);
		}
	}
	digitalWrite(load, LOW);
	digitalWrite(load, HIGH);
}

void MaxMatrix::writeSprite(int x, int y, int w, int h, const byte* sprite)
{	
	if (h == PanelHeight && y == 0)
		for (int i=0; i<w; i++)
		{
			int c = x + i;
      /// TODO: should it be the maximum, or the number in the instance?
			if (c>=0 && c<MaximumCols)
				setColumn(c, sprite[i+2]); //the +2 skips the metadata
		}
	else
		for (int i=0; i<w; i++)
			for (int j=0; j<h; j++)
			{
				int c = x + i;
				int r = y + j;
        /// TODO: should it be the maximum, or the number in the instance?
				if (c>=0 && c<MaximumCols && r>=0 && r<MaximumRows) /// Need to modify this conditional to add more panels
					setDot(c, r, bitRead(sprite[i+2], j));
			}
}

void MaxMatrix::writeSprite(int x, int y, const byte* sprite)
{
	int w = sprite[0];
	int h = sprite[1];
	
	if (h == PanelHeight && y == 0)
		for (int i=0; i<w; i++)
		{
			int c = x + i;
      /// TODO: should it be the maximum, or the number in the instance?
			if (c>=0 && c<MaximumCols)
				setColumn(c, sprite[i+2]); //the +2 skips the metadata
		}
	else
		for (int i=0; i<w; i++)
			for (int j=0; j<h; j++)
			{
				int c = x + i;
				int r = y + j;
        /// TODO: should it be the maximum, or the number in the instance?
				if (c>=0 && c<MaximumCols && r>=0 && r<MaximumRows) /// Need to modify this conditional to add more panels
					setDot(c, r, bitRead(sprite[i+2], j));
			}
}

void MaxMatrix::setPanelBuffer(int panelStart, const byte* pixelDat)
{
	for (int i=0; i<PanelWidth; i++)
	{
		int c = panelStart + i;
		if (c>=0 && c<MaximumCols)
			setBufferColumn(c, pixelDat[i]);
	}
}

void MaxMatrix::reload()
{
	for (int i=0; i<PanelWidth; i++)
	{
		int col = i;
		digitalWrite(load, LOW);    
		for (int j=0; j<numPanels; j++) 
		{
			shiftOut(data, clock, MSBFIRST, i + 1);
			shiftOut(data, clock, MSBFIRST, buffer[col]);
			col += PanelWidth;
		}
		digitalWrite(load, LOW);
		digitalWrite(load, HIGH);
	}
}

void MaxMatrix::shiftLeft(bool rotate, bool fill_zero)
{
	byte old = buffer[0];
	int i;
	for (i=0; i<BufferSize; i++)
		buffer[i] = buffer[i+1];
	if (rotate) buffer[BufferSize-1] = old;
	else if (fill_zero) buffer[BufferSize-1] = 0;
	
	reload();
}

void MaxMatrix::shiftRight(bool rotate, bool fill_zero)
{
	int last = BufferSize-1;
	byte old = buffer[last];
	int i;
	for (i=last; i>0; i--)
		buffer[i] = buffer[i-1];
	if (rotate) buffer[0] = old;
	else if (fill_zero) buffer[0] = 0;
	
	reload();
}

void MaxMatrix::shiftUp(bool rotate)
{
	for (int i=0; i<BufferSize; i++)
	{
		bool b = buffer[i] & 1;
		buffer[i] >>= 1;
		if (rotate) bitWrite(buffer[i], 7, b);
	}
	reload();
}

void MaxMatrix::shiftDown(bool rotate)
{
	for (int i=0; i<BufferSize; i++)
	{
		bool b = buffer[i] & 128;
		buffer[i] <<= 1;
		if (rotate) bitWrite(buffer[i], 0, b);
	}
	reload();
}


