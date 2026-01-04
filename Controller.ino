/* 
Made by Justin McNiel as an update to v3.1 of the Protogen OS made by @Feronium
Designed to be more maintainable (better named variables, also a ton of macros for readability without affecting runtime)
Designed to allow for more connections, with the intent of being able to send Max7291 commands to different parts individually
  ie. dimming just the eyes, setting different parts at different brightnesses to account for different colored LEDs
Adding in more sensors
  ie. boop sensor, exterior microphone, etc.
Future ideas include (may require porting to an rpi for some of these, the program would become too large)
  - full wireless connectivity
  - ambient light sensor
  - amplifier support
  - internal microphone support
  - phone call support
  - song name/artist name displays
  - AVRCP Controls (controlling media through buttons on the proto head)
  - eye tracking
*/

#include <math.h>
#include <arduinoFFT.h>
#include <EEPROM.h>

#include "MaxMatrix.h"

#define DEBUG
//#define PositionPrints

#ifndef DEBUG
#define RELEASE
#endif

//#define CYCLEMODE

#define BUTTON1PIN 2
#define BUTTON1HANDLER GetBooped
void Button1Interrupt();
#define BUTTON1DEBOUNCEMS 100 //250
#define BOOPEDPIN BUTTON1PIN

#define BUTTON2PIN 3
#define BUTTON2HANDLER CycleProfiles
void Button2Interrupt();
#define BUTTON2DEBOUNCEMS 250

void Noop();
#define BUTTON3PIN A6
#define BUTTON3THRESHOLD 512
#define BUTTON3ONHANDLER ShockedOn
#define BUTTON3OFFHANDLER ShockedOff
#define BUTTON3DEBOUNCEMS 500
int Button3Value = -1;
#define BUTTON3ONCHANGE() (Button3Value ? BUTTON3ONHANDLER() : BUTTON3OFFHANDLER())

#define SWITCH1PIN A5
#define SWITCH1ONHANDLER VisualizerOn
#define SWITCH1OFFHANDLER VisualizerOff
#define SWITCH1DEBOUNCEMS 100
int Switch1Value = -1;
#define SWITCH1ONCHANGE() (Switch1Value ? SWITCH1ONHANDLER() : SWITCH1OFFHANDLER())


typedef void VoidFunc();
enum SpecialExpressionOrder //last is highest priority
{
  BoopOrder,
  VisualizerOrder,
  SPECIALEXPRESSIONNUM,
};
VoidFunc *specialExpressionHandlers[SPECIALEXPRESSIONNUM];
unsigned char specialExpressionsActive = 0;
#define GETSPECIALEXPRESSIONMASK(specialExpressionEnum) (0b1 << specialExpressionEnum)
inline void EndSpecialExpressions();

#define DIN0 10
#define CS0 11
#define CLK0 12

#define DIN1 7
#define CS1 8
#define CLK1 9

#define INDICATORREDPIN 6
#define INDICATORGREENPIN 5
#define INDICATORBLUEPIN 4
#define SetIndicator(expr) { \
  if ((expr + 1) & 0b001) \
  { digitalWrite(INDICATORREDPIN, HIGH); } \
  else \
  { digitalWrite(INDICATORREDPIN, LOW); } \
  if ((expr + 1) & 0b010) \
  { digitalWrite(INDICATORGREENPIN, HIGH); } \
  else \
  { digitalWrite(INDICATORGREENPIN, LOW); } \
  if ((expr + 1) & 0b100) \
  { digitalWrite(INDICATORBLUEPIN, HIGH); } \
  else \
  { digitalWrite(INDICATORBLUEPIN, LOW); } \
}

#define MICPIN A7

MaxMatrix matrixHandler1(DIN0, CS0, CLK0, 10);
MaxMatrix matrixHandler2(DIN1, CS1, CLK1, 4);

unsigned long currentTime = 0;

#define SYNC(){ \
  matrixHandler1.reload(); \
  matrixHandler2.reload(); \
}
#define INIT(){ \
  matrixHandler1.init(); \
  matrixHandler2.init(); \
}
#define CLEAR(){ \
  matrixHandler1.clear(); \
}

#define SETINTENSITY(intensity){ \
  matrixHandler1.setIntensity(intensity*0.25); \
  matrixHandler2.setIntensity(intensity); \
}

#define EXPRESSION_SAVE_POS 0

typedef char PanelData[PanelHeight];

struct MatchingPanels
{
  PanelData left;
  PanelData right;
};

#define NosePanelLeft 40
#define NosePanelRight 32
#define NosePanelCount 1 // the number of panels on one side
#define NoseOffsetLeft PanelWidth*NosePanelLeft
#define NoseOffsetRight PanelWidth*NoseOffsetRight
#define NoseHandler matrixHandler1
typedef MatchingPanels NoseSprite[NosePanelCount];
void DrawNose(NoseSprite nose);

#define EyePanelLeft 16//, 104
#define EyePanelRight 0//, 8
#define EyePanelCount 2 // the number of panels on one side
#define EyeOffsetLeft PanelWidth*EyePanelLeft
#define EyeOffsetRight PanelWidth*EyeOffsetRight
#define EyeHandler matrixHandler2
typedef MatchingPanels EyeSprite[EyePanelCount];
void DrawEye(NoseSprite nose);

#define MouthPanelLeft 48//, 56, 64, 72
#define MouthPanelRight 0//, 8, 16, 24
#define MouthPanelCount 4 // the number of panels on one side
#define MouthOffsetLeft PanelWidth*MouthPanelLeft
#define MouthOffsetRight PanelWidth*MouthPanelRight
#define MouthHandler matrixHandler1
typedef MatchingPanels MouthSprite[MouthPanelCount];
void DrawMouth(NoseSprite nose);

#define TargetFrameRate 0 //0 = unlimited
unsigned long prevFrameMS = 0;
unsigned short worstFrameTime = 0;
//#define OutputFrameInfo
int frameCount = 0;

class Expression
{
  public:
    enum Expressions
    {
      Default,
      Happy,
      Glitch,
      Angry,
      Spooked,
      EXPRESSION_MAX, //moving expressieons to after this will disable them
      Visualizer, //Moved to a special Expression
    };
};
Expression::Expressions currentExpression = Expression::Happy;

//Default Sprites
const NoseSprite DefaultNose = {
  {
    { 0b00000000, 0b00000000, 0b00000001, 0b00000011, 0b00000011, 0b00111111, 0b01111110, 0b00000000 },// left
    { 0b00000000, 0b00000000, 0b10000000, 0b11000000, 0b11000000, 0b11111100, 0b01111110, 0b00000000 } // right
  }};

const MouthSprite DefaultMouth = {
  {
    {0b00100000, 0b01111000, 0b11011110, 0b11000111, 0b11111111, 0b00000000, 0b00000000, 0b00000000}, //left 1
    {0b00001000, 0b00011110, 0b01111011, 0b11100011, 0b11111111, 0b00000000, 0b00000000, 0b00000000} //right 1
  },
  {
    {0b00000000, 0b00000000, 0b00000000, 0b00000001, 0b00000111, 0b00011110, 0b01111000, 0b11100000},//left 2
    {0b00000000, 0b00000000, 0b00000000, 0b10000000, 0b11100000, 0b01111000, 0b00011110, 0b00000111} //right 2
  },
  {
    {0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b11100000, 0b01111000, 0b00011110, 0b00000111},//left 3
    {0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00000111, 0b00011110, 0b01111000, 0b11100000} //right 3
  },
  {
    {0b00000000, 0b00000000, 0b00000000, 0b00000111, 0b00011111, 0b01111000, 0b11100000, 0b10000000},//left 4
    {0b00000000, 0b00000000, 0b00000000, 0b11100000, 0b11111000, 0b00011110, 0b00000111, 0b00000001}  //right 4
  }};

#define GlitchMouthFrames 2
const MouthSprite GlitchMouth[GlitchMouthFrames] = {
  {
    {
      {0b10100101, 0b11010001, 0b01000111, 0b00001010, 0b10001001, 0b11010100, 0b00001100, 0b00000000},//left
      {0b00001100, 0b00000000, 0b11010100, 0b10001001, 0b00010100, 0b01000111, 0b11010001, 0b10100101} //right
    },
    {
      {0b00000000, 0b00000000, 0b00000000, 0b00000000, 0b00010010, 0b10010110, 0b01001011, 0b01110100},//left
      {0b01110100, 0b00000000, 0b01001011, 0b10010110, 0b00010010, 0b00000000, 0b00000000, 0b00000000} //right
    },
    {
      {0b00000000, 0b00000000, 0b00000000, 0b01110100, 0b00101100, 0b01001011, 0b10101001, 0b00101001},//left
      {0b00101001, 0b10101001, 0b01001011, 0b00101100, 0b01110100, 0b00000000, 0b00000000, 0b00000000} //right
    },
    {
      {0b00000000, 0b00000000, 0b10101001, 0b01011001, 0b11101001, 0b01010111, 0b01011101, 0b00001011},//left
      {0b00000000, 0b00000000, 0b10010101, 0b10011010, 0b10010111, 0b11101010, 0b10111010, 0b11010000} //right
    }
  },
  {
    {
      {0b00000000, 0b00000000, 0b00000000, 0b10101011, 0b10100001, 0b01010111, 0b11101000, 0b00100000},//left
      {0b00000000, 0b00000000, 0b00000000, 0b11010101, 0b10000101, 0b11101010, 0b00010111, 0b00000100} //right
    },
    {
      {0b00000000, 0b00000000, 0b00000000, 0b00000101, 0b00010010, 0b10000110, 0b01010100, 0b10100010},//left
      {0b10100010, 0b01010100, 0b10000110, 0b00010010, 0b00000101, 0b00000000, 0b00000000, 0b00000000} //right
    },
    {
      {0b00000000, 0b00000000, 0b00000000, 0b00001000, 0b10100000, 0b01101001, 0b10011010, 0b01001101},//left
      {0b01001101, 0b10011010, 0b01101001, 0b10100000, 0b00001000, 0b00000000, 0b00000000, 0b00000000} //right
    },
    {
      {0b00000000, 0b00000000, 0b00010001, 0b00000110, 0b01010101, 0b11011000, 0b10101001, 0b10000000},//left
      {0b00000000, 0b00000000, 0b10001000, 0b10101010, 0b00011011, 0b10010101, 0b00000001, 0b00000000} //right
    }
  }};

#define GetGlitchMouth() (GlitchMouth[(currentTime/83)%GlitchMouthFrames])

const EyeSprite HappyEye = { // Was the default eye
  {
    {0b00000000, 0b11100000, 0b11111000, 0b11111110, 0b00000111, 0b00000001, 0b00000000, 0b00000000}, //right 1
    {0b00000000, 0b00000111, 0b00011111, 0b01111111, 0b11100000, 0b10000000, 0b00000000, 0b00000000},//left 1
  },
  {
    {0b00001111, 0b00111111, 0b01111111, 0b11111111, 0b11110000, 0b01100000, 0b00000000, 0b00000000}, //right 2
    {0b11110000, 0b11111100, 0b11111110, 0b11111111, 0b00001111, 0b00000110, 0b00000000, 0b00000000},//left 2
  }};

const EyeSprite DefaultEye = {
  {
    {0b00000000, 0b00000000, 0b11000000, 0b11110000, 0b11111000, 0b11111100, 0b11111000, 0b00000000},//left
    {0b00000000, 0b00000000, 0b00000011, 0b00001111, 0b00011111, 0b00111111, 0b00011111, 0b00000000} //right
  },
  {
    {0b00000000, 0b00000111, 0b00011111, 0b00111111, 0b01111111, 0b01111111, 0b00111111, 0b00000000},//left
    {0b00000000, 0b11100000, 0b11111000, 0b11111100, 0b11111110, 0b11111110, 0b11111100, 0b00000000} //right
  }};

const EyeSprite AngryEye = {
  {
    {0b00000000, 0b11111000, 0b11111100, 0b11111100, 0b11111000, 0b11110000, 0b11000000, 0b00000000},//right 2
    {0b00000000, 0b00011111, 0b00111111, 0b00111111, 0b00011111, 0b00001111, 0b00000011, 0b00000000} //left 2
  },
  {
    {0b00000000, 0b00111111, 0b01111111, 0b00111111, 0b00011111, 0b00000111, 0b00000001, 0b00000000},//right 1
    {0b00000000, 0b11111100, 0b11111110, 0b11111100, 0b11111000, 0b11100000, 0b10000000, 0b00000000} //left 1
  }};

const EyeSprite SpookedEye = {
  {
    {0b10000000, 0b11100000, 0b11100000, 0b11110000, 0b11110000, 0b11100000, 0b11100000, 0b10000000},//left 1
    {0b00000001, 0b00000111, 0b00000111, 0b00001111, 0b00001111, 0b00000111, 0b00000111, 0b00000001} //right 1
  },
  {
    {0b00000001, 0b00000111, 0b00000111, 0b00001111, 0b00001111, 0b00000111, 0b00000111, 0b00000001},//left 2
    {0b10000000, 0b11100000, 0b11100000, 0b11110000, 0b11110000, 0b11100000, 0b11100000, 0b10000000} //right 2
  }};

//Custom Sprites
const NoseSprite RooNose = {
  {
    { 0b00000000, 0b10000000, 0b11111100, 0b11111110, 0b01111110, 0b00111110, 0b00001100, 0b00000000 }, // left
    { 0b00000000, 0b00000001, 0b00111111, 0b01111111, 0b01111110, 0b01111100, 0b00110000, 0b00000000 }// right
  }};

#define BoopedEyesFrames 4
const EyeSprite BoopedEyes[BoopedEyesFrames] = {
  {
    {
      {0b00000000, 0b00100000, 0b01000000, 0b10000000, 0b10000000, 0b01000000, 0b00100000, 0b00000000},//left
      {0b00000000, 0b00000100, 0b00000010, 0b00000001, 0b00000001, 0b00000010, 0b00000100, 0b00000000} //right
    },
    {
      {0b00000000, 0b00000100, 0b00000010, 0b00000001, 0b00000001, 0b00000010, 0b00000100, 0b00000000},//left
      {0b00000000, 0b00100000, 0b01000000, 0b10000000, 0b10000000, 0b01000000, 0b00100000, 0b00000000} //right
    }
  },
  {
    {
      {0b00000000, 0b00010000, 0b00100000, 0b11000000, 0b11000000, 0b00100000, 0b00010000, 0b00000000},//left
      {0b00000000, 0b00000010, 0b00000001, 0b00000000, 0b00000000, 0b00000001, 0b00000010, 0b00000000} //right
    },
    {
      {0b00000000, 0b00000010, 0b00000001, 0b00000000, 0b00000000, 0b00000001, 0b00000010, 0b00000000},//left
      {0b00000000, 0b00010000, 0b00100000, 0b11000000, 0b11000000, 0b00100000, 0b00010000, 0b00000000} //right
    }
  },
  {
    {
      {0b00000000, 0b00100000, 0b01000000, 0b10000000, 0b10000000, 0b01000000, 0b00100000, 0b00000000},//left
      {0b00000000, 0b00000100, 0b00000010, 0b00000001, 0b00000001, 0b00000010, 0b00000100, 0b00000000} //right
    },
    {
      {0b00000000, 0b00000100, 0b00000010, 0b00000001, 0b00000001, 0b00000010, 0b00000100, 0b00000000},//left
      {0b00000000, 0b00100000, 0b01000000, 0b10000000, 0b10000000, 0b01000000, 0b00100000, 0b00000000} //right
    }
  },
  {
    {
      {0b00000000, 0b01000000, 0b10000000, 0b00000000, 0b00000000, 0b10000000, 0b01000000, 0b00000000},//left
      {0b00000000, 0b00001000, 0b00000100, 0b00000011, 0b00000011, 0b00000100, 0b00001000, 0b00000000} //right
    },
    {
      {0b00000000, 0b00001000, 0b00000100, 0b00000011, 0b00000011, 0b00000100, 0b00001000, 0b00000000},//left
      {0b00000000, 0b01000000, 0b10000000, 0b00000000, 0b00000000, 0b10000000, 0b01000000, 0b00000000} //right
    }
  }};
#define GetBoopedFrame() ( BoopedEyes[(currentTime/83)%BoopedEyesFrames] )
const MouthSprite CustomMouth = {
  {
    {0b00000000, 0b00000000, 0b00010000, 0b01111000, 0b11101100, 0b11000110, 0b11111110, 0b00000011},//left
    {0b00000000, 0b00000000, 0b00001000, 0b00011110, 0b00110111, 0b01100011, 0b01111111, 0b11000000} //right
  },
  {
    {0b00000000, 0b00000000, 0b00000000, 0b00000111, 0b00011111, 0b11111001, 0b11100000, 0b00000000},//left
    {0b00000000, 0b00000000, 0b00000000, 0b11100000, 0b11111000, 0b10011111, 0b00000111, 0b00000000} //right
  },
  {
    {0b00000000, 0b10000000, 0b11100000, 0b01111000, 0b00011110, 0b00000111, 0b00000001, 0b00000000},//left
    {0b00000000, 0b00000001, 0b00000111, 0b00011110, 0b01111000, 0b11100000, 0b10000000, 0b00000000} //right
  },
  {
    {0b00000110, 0b00111111, 0b01111001, 0b11100000, 0b00000000, 0b00000000, 0b00000000, 0b00000000},//left
    {0b01100000, 0b11111100, 0b10011110, 0b00000111, 0b00000000, 0b00000000, 0b00000000, 0b00000000} //right
  }};

//Doing functions will allow for annimations, like blinking
#define DEFAULTNOSE() (RooNose)
#define DEFAULTMOUTH() (CustomMouth)
#define DEFAULTEYES() (DefaultEye)

MouthSprite visualizerMouth;
int ReadAudio() ///TODO: Refactor this
{
#define SHOULDPRESAMPLE
  while (!(_SFR_BYTE(ADCSRA) & _BV(ADIF)))
  {
    //pass
  } // wait for read to be ready
  int res = ADCL;
  res += ADCH << 8;
  bitSet(ADCSRA, ADIF); // clear the read flag
  bitSet(ADCSRA, ADSC); // start the next ADC conversion

  return res;
}

arduinoFFT FFT = arduinoFFT();

#define VISUALIZERMICSAMPLES 64
double visualizerRealComponent[VISUALIZERMICSAMPLES];
double visualizerImagComponent[VISUALIZERMICSAMPLES];
char visualizerValues[PanelWidth*MouthPanelCount];
#define VISUALIZERSPECTRALATTACKRATE 0.5
#define VISUALIZERSMOOTH(prev, new) (prev*(1 - VISUALIZERSPECTRALATTACKRATE)+new*VISUALIZERSPECTRALATTACKRATE)
#define MAPAUDIOSAMPLE(value) (map(value, 200, 550, 0, 100)) //maps it to our custom range (0-100)
#define log2(x) (log(x)/log(2))
#define LOGVISUALIZERSENSITIVITY
//#define LOGVISUALIZERSAMPLERATE

#ifdef LOGVISUALIZERSAMPLERATE
double totSampleTime = 0.0;
unsigned long sampledFrames = 0;
#endif
#define SENSITIVITYINTERPOLATIONRATE 0.2
float sensitivityInterpolator = 0.1f;

void DrawVisualizer() 
{
  //Sample the microphone

#ifdef SHOULDPRESAMPLE
  ReadAudio(); //pre-sample
#endif
  int avg = 0;

#ifdef LOGVISUALIZERSAMPLERATE
  unsigned long startTime = millis();
#endif

  for(int i = 0; i < VISUALIZERMICSAMPLES; i++)
  {
    int val = 0;
    val = ReadAudio(); //sometimes I get a lot of zeros, may end up removing this loop

    val = MAPAUDIOSAMPLE(val);

    visualizerRealComponent[i] = val;
    visualizerImagComponent[i] = 0;
    avg += val;
  }
#ifdef LOGVISUALIZERSAMPLERATE
  unsigned long endTime = millis();
  if(endTime - startTime > 0)
  {
    totSampleTime += endTime - startTime;
    sampledFrames++;
    Serial.print("Avg Sample Rate (hz): ");
    Serial.println(VISUALIZERMICSAMPLES/((totSampleTime/sampledFrames)/1000.0d));
  }
#endif

  avg /= VISUALIZERMICSAMPLES;

  float sensitivity = log2(max(1, avg))*(2/sqrt(100 - min(99, avg))) - 0.3;
  sensitivityInterpolator = sensitivityInterpolator*(1-SENSITIVITYINTERPOLATIONRATE) + sensitivity*SENSITIVITYINTERPOLATIONRATE;
  sensitivity = max(0, min(10, sensitivityInterpolator));
#if !defined(RELEASE) && defined(LOGVISUALIZERSENSITIVITY)
  Serial.print("Sens; ");
  Serial.print(sensitivity);
  Serial.print(", \tAvg: ");
  Serial.println(avg);
#endif
  
  FFT.Windowing(visualizerRealComponent, VISUALIZERMICSAMPLES, FFT_WIN_TYP_FLT_TOP, FFT_FORWARD);
  FFT.Compute(visualizerRealComponent, visualizerImagComponent, VISUALIZERMICSAMPLES, FFT_FORWARD);
  FFT.ComplexToMagnitude(visualizerRealComponent, visualizerImagComponent, VISUALIZERMICSAMPLES);

  for (int panelNum = 0; panelNum < MouthPanelCount; panelNum++)
  {
    for (int rowNum = 0; rowNum < PanelHeight; rowNum++)
    {
      visualizerMouth[panelNum].left[rowNum]  = 0; //clear it
      visualizerMouth[panelNum].right[rowNum] = 0; //clear it
    }

    for (int colNum = 0; colNum < PanelWidth; colNum++)
    {
      //int val = random()%(PanelHeight + 1); //disabled random in favor of it actually working
      int colInd = panelNum*PanelWidth + colNum;
      if(colInd == 0)
      {
        continue; //the first 2 cols are always maxed for some reason
      }

      int val = constrain(visualizerRealComponent[colInd+2]/sensitivity, 0, PanelHeight*10);
      val = map(val, 0, PanelHeight*10, 0, PanelHeight);

      visualizerValues[colInd] = VISUALIZERSMOOTH(visualizerValues[colInd], val);
  
      int leftColMask = 0b1 << colNum;
      int rightColMask = (0b1 << (PanelWidth - 1)) >> colNum;
      for(int rowNum = 0; rowNum < visualizerValues[colInd]; rowNum++)
      {
        visualizerMouth[panelNum].left[rowNum]  |= leftColMask;
        visualizerMouth[panelNum].right[rowNum] |= rightColMask;
      }
    }
  }

  DrawMouth(visualizerMouth);
}

//Handlers
#define BOOPEDDURATION 500
unsigned long boopedTimerStart = 0;
bool boopedTimerStarted = false;
void GetBooped()
{
  specialExpressionsActive |= GETSPECIALEXPRESSIONMASK(BoopOrder);
}

void BoopedHandler()
{
  bool boopActive = digitalRead(BOOPEDPIN) == 1;
  
  if(!boopActive && !boopedTimerStarted)
  {
    boopedTimerStarted = true;
    boopedTimerStart = currentTime;
  }

  if( boopActive || (boopedTimerStarted && (((long)currentTime - (long)boopedTimerStart) < BOOPEDDURATION)) )
  {
    //draw booped
    DrawEye(GetBoopedFrame());
  }
  else
  {
    specialExpressionsActive &= ~GETSPECIALEXPRESSIONMASK(BoopOrder);
    boopedTimerStarted = false;
  }
}


Expression::Expressions expressionBeforeShocked = Expression::Default;
void ShockedOn()
{
  expressionBeforeShocked = currentExpression;
  currentExpression = Expression::Spooked;
  SetIndicator(currentExpression);
  #ifdef DEBUG
  Serial.print("Shocked now On, was: ");
  Serial.println(expressionBeforeShocked);
  #endif
}

void ShockedOff()
{
  currentExpression = expressionBeforeShocked;
  SetIndicator(currentExpression);
  #ifdef DEBUG
  Serial.print("Shocked now Off, returning to: ");
  Serial.println(currentExpression);
  #endif
}

void DrawNose(NoseSprite nose)
{
  for(int i = 0; i < NosePanelCount; i++)
  {
#ifdef PositionPrints
    Serial.print("Nose Left: ");
    Serial.println(NosePanelLeft + i*PanelWidth);
    Serial.print("Nose Right: ");
    Serial.println(NosePanelRight + i*PanelWidth);
#endif
    NoseHandler.setPanelBuffer(NosePanelLeft + i*PanelWidth,  nose[NosePanelCount-1 - i].left);
    NoseHandler.setPanelBuffer(NosePanelRight + i*PanelWidth, nose[i].right);
  }
}

void DrawEye(EyeSprite eye)
{
  for(int i = 0; i < EyePanelCount; i++)
  {
#ifdef PositionPrints
    Serial.print("Eye Left: ");
    Serial.println(EyePanelLeft + i*PanelWidth);
    Serial.print("Eye Right: ");
    Serial.println(EyePanelRight + i*PanelWidth);
#endif
    EyeHandler.setPanelBuffer(EyePanelLeft + i*PanelWidth,  eye[EyePanelCount-1 - i].left);
    EyeHandler.setPanelBuffer(EyePanelRight + i*PanelWidth, eye[i].right);
  }
}

void DrawMouth(MouthSprite mouth)
{
  for(int i = 0; i < MouthPanelCount; i++)
  {
#ifdef PositionPrints
    Serial.print("Mouth Left: ");
    Serial.println(MouthPanelLeft + i*PanelWidth);
    Serial.print("Mouth Right: ");
    Serial.println(MouthPanelRight + i*PanelWidth);
#endif
    MouthHandler.setPanelBuffer(MouthPanelLeft + i*PanelWidth,  mouth[MouthPanelCount-1 - i].left);
    MouthHandler.setPanelBuffer(MouthPanelRight + i*PanelWidth, mouth[i].right);
  }
}

unsigned long button3Debounce = 0;
void HandleButton3()
{
  bool tmp = analogRead(BUTTON3PIN) > BUTTON3THRESHOLD;
  if((tmp != Button3Value) && ((currentTime - button3Debounce) > BUTTON3DEBOUNCEMS))
  {
    Button3Value = tmp;
    button3Debounce = currentTime;
    BUTTON3ONCHANGE();
  }
}

unsigned long switch1Debounce = 0;
void HandleSwitch1()
{
  int tmp = digitalRead(SWITCH1PIN);
  if((tmp != Switch1Value) && ((currentTime - switch1Debounce) > SWITCH1DEBOUNCEMS))
  {
    Serial.print("Changing to : ");
    Serial.println(tmp);
    Switch1Value = tmp;
    SWITCH1ONCHANGE();
  }
}

void VisualizerOn()
{
#ifndef RELEASE
  Serial.println("VisualizerOn");
#endif
  specialExpressionsActive |= GETSPECIALEXPRESSIONMASK(VisualizerOrder);
}

void VisualizerOff()
{
#ifndef RELEASE
  Serial.println("VisualizerOff");
#endif
  specialExpressionsActive &= ~GETSPECIALEXPRESSIONMASK(VisualizerOrder);
}

void setup()
{
  currentExpression = (Expression::Expressions)EEPROM.read(EXPRESSION_SAVE_POS);
  expressionBeforeShocked = currentExpression;
  pinMode(INDICATORREDPIN, OUTPUT);
  pinMode(INDICATORGREENPIN, OUTPUT);
  pinMode(INDICATORBLUEPIN, OUTPUT);
  SetIndicator(currentExpression);

  specialExpressionHandlers[BoopOrder] = BoopedHandler;
  specialExpressionHandlers[VisualizerOrder] = DrawVisualizer;

#ifndef Release
  Serial.begin(57600);
#endif
  randomSeed(analogRead(0));

  INIT();
  SETINTENSITY(15);
  //EyeHandler.setIntensity(15);
  
  pinMode(BUTTON1PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(BUTTON1PIN), Button1Interrupt, RISING);

  pinMode(BUTTON2PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(BUTTON2PIN), Button2Interrupt, FALLING);

  pinMode(SWITCH1PIN, INPUT);//interrupts cannot be attached to this one for some reason, prob cuz it's an offbrand board

  CLEAR();

  int tmp = analogRead(MICPIN); //Forces the arduino to prep the library before we need it
}

void loop()
{
  currentTime = millis();
#if TargetFrameRate != 0 || defined (OutputFrameInfo)
  short int frameTime = -1; //allows for ~32s frame time
  frameTime = currentTime - prevFrameMS;
  prevFrameMS = currentTime;
#endif

#if TargetFrameRate != 0
  #define MS_PER_FRAME 1000/TargetFrameRate
  if(MS_PER_FRAME - frameTime > 0)
  {
    delay(MS_PER_FRAME - frameTime);
  }
#endif

#ifdef OutputFrameInfo
  if(frameCount >= 1)
  {
    worstFrameTime = max(worstFrameTime, frameTime);
#ifndef Release
    Serial.print("Target FPS: ");
    Serial.print(TargetFrameRate);
    Serial.print(" Average FPS: ");
    Serial.print(frameCount/(currentTime/1000));
    Serial.print(" Frame Time: ");
    Serial.print(frameTime);
    Serial.print(" Worst Frame Time: ");
    Serial.println(worstFrameTime);
#endif
  }
#endif

  HandleSwitch1();
  HandleButton3();

  //Expression
  switch(currentExpression)
  {
    case Expression::Glitch:
      DrawNose(DEFAULTNOSE());
      DrawEye(GetBoopedFrame());
      //DrawEye(DEFAULTEYES());
      DrawMouth(GetGlitchMouth());
      break;
    case Expression::Happy:
      DrawNose(DEFAULTNOSE());
      DrawEye(HappyEye);
      DrawMouth(DEFAULTMOUTH());
      break;
    case Expression::Angry:
      DrawNose(DEFAULTNOSE());
      DrawEye(AngryEye);
      DrawMouth(DEFAULTMOUTH());
      break;
    case Expression::Spooked:
      DrawNose(DEFAULTNOSE());
      DrawEye(SpookedEye);
      DrawMouth(DEFAULTMOUTH());
      break;
    case Expression::Visualizer:
      DrawVisualizer();
      DrawNose(DEFAULTNOSE());
      DrawEye(DEFAULTEYES());
      break;
    case Expression::Default:
    default:
      DrawNose(DEFAULTNOSE());    //all default sprites passed
      DrawEye(DEFAULTEYES());     //all default sprites passed
      DrawMouth(DEFAULTMOUTH());  //all default sprites passed
  };
  
  for(int i = 0; i < SPECIALEXPRESSIONNUM; i++)
  {
    if(specialExpressionsActive & GETSPECIALEXPRESSIONMASK(i))
    {
      specialExpressionHandlers[i]();
    }
  }
  
  SYNC();
  frameCount++;
#ifdef CYCLEMODE
  if(frameCount%100 == 0)
  {
    CycleProfiles();
  }
#endif
}

unsigned long button1Debounce = 0;
void Button1Interrupt()
{
  currentTime = millis();
  if((currentTime - button1Debounce) > BUTTON1DEBOUNCEMS)
  {
    button1Debounce = currentTime; //Only update when we trigger the function
    BUTTON1HANDLER();
  }
}

unsigned long button2Debounce = 0;
void Button2Interrupt()
{
  currentTime = millis();
  if((currentTime - button2Debounce) > BUTTON2DEBOUNCEMS)
  {
    button2Debounce = currentTime; //Only update when we trigger the function
    BUTTON2HANDLER();
  }
}

void CycleProfiles()
{
  currentExpression = (Expression::Expressions)((((int)currentExpression)+1)%Expression::EXPRESSION_MAX);

  SetIndicator(currentExpression);

  EEPROM.write(EXPRESSION_SAVE_POS, (int)currentExpression);
#ifndef RELEASE
  Serial.print("CyclingProfiles: ");
  Serial.println(currentExpression);
#endif
}

void EndSpecialExpressions()
{
  specialExpressionsActive = 0;
}

void Noop() { }
