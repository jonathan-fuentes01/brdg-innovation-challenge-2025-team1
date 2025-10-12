#include <Preferences.h>

const int PIN_RELAY = 23; 
const int PIN_MOISTURE_SEN = 34; 

const int PIN_LED_NEED = 18; 
const int PIN_LED_WATER = 19; 
const int PIN_LED_YELLOW = 21; 

const int PIN_BTN_UP = 33;
const int PIN_BTN_DOWN = 25;

const int RELAY_ON  = HIGH;       
const int RELAY_OFF = LOW;

int dryThresh = 1900;
const int hystGap = 250; // hysteresis gap, hovers around a certain threshold (wetThresh = dryThresh - hystGap)
uint32_t minOnMs = 60UL * 1000; // minimum time on for pump (1 min)
uint32_t minOffMs = 5UL * 60UL * 1000; // min pump OFF (5 min)
uint32_t maxOnMs = 8UL * 60UL * 1000; // max time on for the pump (8 mins)
const int stepSize = 10; // when you press the button and it steps up or down :D
const int dryMin = 200; 
const int dryMax = 3900;

Preferences prefs; // saving initial preferences
bool pumpOn = false;
uint32_t lastChange = 0;
uint32_t lastAdjustMs = 0;
bool pendingSave = false;

int medianReadMoisture(){ // returning the median values for the moisture sensor  (filtering noise for the sensor measurement)
  const int N = 7;
  int v[N]; // initialize integer array of size 7 for analog reads
  for (int i = 0; i < N; i++){ 
    v[i] = analogRead(PIN_MOISTURE_SEN); 
    delay(5);
  }
  for (int i = 1; i < N; i++){ //sorting the array
    int k = v[i];
    j = i - 1; 
    while(j >= 0 && v[j] > k){ 
      v[j + 1] = v[j]; 
      j--; 
    } 
    v[j + 1] = k; 
  }
  return v[N/2]; // returns the median from the sorted array
}

void setPump(bool on){ // logic for turning the pump on
  pumpOn = on;
  digitalWrite(PIN_RELAY, on ? RELAY_ON : RELAY_OFF);
  digitalWrite(PIN_LED_WATER, on ? HIGH : LOW);
  lastChange = millis(); // tracking last state change for the pump
}

void loadPrefs(){ // loading values after system reboot
  prefs.begin("irrigation", false); // initialize namespace "irrigation" in non-volatile storage in read/write mode
  dryThresh = prefs.getInt("dry", dryThresh); // read dry threshold values under "dry" and return the current value, default returned value is dryThresh
  prefs.end(); 
}

void maybeSavePrefs(){ //save dryness threshold to flash memory
  if (pendingSave && (millis() - lastAdjustMs > 10000UL)){ // if change was made that needs to be saved and 10 seconds since last adjustment
    prefs.begin("irrigation", false); 
    prefs.putInt("dry", dryThresh); // save value under dryThresh
    prefs.end();
    pendingSave = false; // reset flag
    for (int i = 0; i < 2; i++){ 
      digitalWrite(PIN_LED_NEED, HIGH); delay(80); 
      digitalWrite(PIN_LED_NEED, LOW);  delay(80); 
    }
  }
}

bool readButtonOnce(int pin){
  static uint32_t lastMs[40] = {0}; 
  static bool stableState[40] = {1};
  static bool lastRead[40] = {1};

  const uint32_t now = millis(); //current time
  bool rawPressed = (digitalRead(pin) == LOW); //read negative logic button press

  if (rawPressed != lastRead[pin]) { //debounce state change logic
    lastMs[pin] = now;
    lastRead[pin] = rawPressed;
  }
  if ((now - lastMs[pin]) > 30 && (lastRead[pin] != stableState[pin])) { // debounce 30ms for buttons
    stableState[pin] = lastRead[pin];
    if (stableState[pin] == true){
      return true;
    }
  }
  return false;
}

void setup(){
  Serial.begin(115200); // baud rate
  analogReadResolution(12);

  pinMode(PIN_RELAY, OUTPUT);       
  digitalWrite(PIN_RELAY, RELAY_OFF); // make sure the relay is turned off initially

  pinMode(PIN_LED_NEED, OUTPUT); // initialize all of the LEDs as outputs with positive logic    
  digitalWrite(PIN_LED_NEED, LOW);

  pinMode(PIN_LED_WATER, OUTPUT);   
  digitalWrite(PIN_LED_WATER, LOW);

  pinMode(PIN_LED_YELLOW, OUTPUT);  
  digitalWrite(PIN_LED_YELLOW, LOW);

  pinMode(PIN_BTN_UP, INPUT_PULLUP); // initialize internal pull ups for the buttons
  pinMode(PIN_BTN_DOWN, INPUT_PULLUP);

  loadPrefs(); // load preferences

  lastChange = millis();
}

void loop(){
  uint32_t now = millis();

  if (readButtonOnce(PIN_BTN_UP)){ 
    dryThresh = constrain(dryThresh + stepSize, dryMin, dryMax); //adding step-size, but keeping it between dryMin and dryMax
    lastAdjustMs = now; 
    pendingSave = true; // mark that you need to save these values 
    Serial.printf("Raised Dryness Threshold: dryThresh=%d\n", dryThresh);
  }
  if (readButtonOnce(PIN_BTN_DOWN)){ 
    dryThresh = constrain(dryThresh - stepSize, dryMin, dryMax); 
    lastAdjustMs = now; 
    pendingSave = true; 
    Serial.printf("Lowered Dryness Threshold: dryThresh=%d\n", dryThresh);
  }

  maybeSavePrefs(); //updating the values from the pending saves

  // sensor reading
  int soil = medianReadMoisture();
  int wetThresh = dryThresh - hystGap;

  // boolean logic for LED states
  bool needWater = (soil > dryThresh);
  bool maybeNeedWater = (soil > wetThresh) && !needWater;

  digitalWrite(PIN_LED_NEED, needWater ? HIGH : LOW);
  digitalWrite(PIN_LED_YELLOW, maybeNeedWater ? HIGH : LOW);

  if (!pumpOn){ // pump off
    bool offLongEnough = (now - lastChange) >= minOffMs; // checks to see if the pump has been on for long enough
    if (offLongEnough && needWater){
      setPump(true); //turn on pump
    }
  } 
  else {
    bool onLongEnough = (now - lastChange) >= minOnMs; // minimum time for pump running
    bool hitSafetyCap = (now - lastChange) >= maxOnMs; // maximum time of pump running
    if ((onLongEnough && soil < wetThresh) || hitSafetyCap){
      setPump(false);
      if (hitSafetyCap){
        Serial.println("Safety cap reached; pump OFF.");
      }
    }
  }

// outputs for the serial monitor
  static uint32_t lastPrint = 0;
  if (now - lastPrint > 3000) {
    Serial.printf("Soil=%d Dry=%d Wet=%d Pump=%d NeedsWater=%d WaterPerchance=%d\n", soil, dryThresh, wetThresh, pumpOn, needWater, maybeNeedWater);
    lastPrint = now;
  }
  delay(150);
}