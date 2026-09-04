/*
  Adaptive SDR Sonar Transmitter Payload — STM32F411CEU6 (Blackpill)
  SIH260580

  Pots simulate environmental sensors (depth, turbidity, salinity), driving
  an adaptation algorithm that computes frequency/duration/amplitude instead
  of the pots setting those directly. Debug output is comma-separated,
  normalized 0-100, for Arduino IDE's Serial Plotter.

  Board settings (Arduino IDE):
    Tools > Board             : Generic STM32F4 series
    Tools > Board part number : BlackPill F411CE
    Tools > USB support       : CDC (generic Serial supersede U(S)ART)
    Tools > Upload method     : STM32CubeProgrammer (SWD) -- via ST-Link

  Pinout:
    MCP4921 CS   -> PB12
    MCP4921 SCK  -> PB13   (SPI2_SCK)
    MCP4921 SDI  -> PB15   (SPI2_MOSI)
    MCP4921 LDAC -> GND    (tied low in hardware)
    Pot 1 (depth, meters)  -> PA1 (ADC1_IN1)
    Pot 2 (turbidity, %)   -> PA2 (ADC1_IN2)
    Pot 3 (salinity, PSU)  -> PA3 (ADC1_IN3)
*/

#include <SPI.h>

#define POT_DEPTH_PIN     PA1
#define POT_TURBIDITY_PIN PA2
#define POT_SALINITY_PIN  PA3
#define DAC_CS_PIN        PB12

#define MOSI2 PB15
#define MISO2 PB14
#define SCLK2 PB13
#define SS2   PB12
SPIClass dacSPI(MOSI2, MISO2, SCLK2, SS2);

#define SAMPLE_RATE_HZ   200000UL
#define BUFFER_SIZE      512

#define FREQ_MIN_HZ      2000.0f
#define FREQ_MAX_HZ      18000.0f
#define CHIRP_BW_HZ      4000.0f
#define DUR_MIN_S        0.0005f
#define DUR_MAX_S        0.0025f

#define DEPTH_MIN_M      0.0f
#define DEPTH_MAX_M      100.0f
#define TURBIDITY_MIN    0.0f
#define TURBIDITY_MAX    100.0f
#define SALINITY_MIN_PSU 0.0f
#define SALINITY_MAX_PSU 40.0f

uint16_t chirpBuffer[BUFFER_SIZE];
volatile uint16_t sampleIndex = 0;
volatile bool bufferFinished = false;

HardwareTimer *sampleTimer;
SPISettings dacSPISettings(20000000, MSBFIRST, SPI_MODE0);

float currentCenterFreq = 10000.0f;
float currentDuration   = 0.0015f;
float currentAmplitude  = 0.65f;

float currentDepth = 0.0f, currentTurbidity = 0.0f, currentSalinity = 0.0f;
int rawDepth = 0, rawTurbidity = 0, rawSalinity = 0;

unsigned long lastPrintMs = 0;

#define DAC_CS_HIGH()  (GPIOB->BSRR = (1UL << 12))
#define DAC_CS_LOW()   (GPIOB->BSRR = (1UL << (12 + 16)))

inline void dacWrite(uint16_t value12bit) {
  uint16_t word = 0x3000 | (value12bit & 0x0FFF);
  DAC_CS_LOW();
  dacSPI.transfer16(word);
  DAC_CS_HIGH();
}

inline float hann(uint32_t n, uint32_t N) {
  return 0.5f - 0.5f * cosf(2.0f * PI * (float)n / (float)(N - 1));
}

void generateChirp(float centerFreq, float duration, float amplitude) {
  float fStart = centerFreq - (CHIRP_BW_HZ / 2.0f);
  float fEnd   = centerFreq + (CHIRP_BW_HZ / 2.0f);
  float k = (fEnd - fStart) / duration;

  uint32_t activeSamples = (uint32_t)(duration * SAMPLE_RATE_HZ);
  if (activeSamples > BUFFER_SIZE) activeSamples = BUFFER_SIZE;

  for (uint32_t n = 0; n < BUFFER_SIZE; n++) {
    if (n < activeSamples) {
      float t = (float)n / (float)SAMPLE_RATE_HZ;
      float phase = 2.0f * PI * (fStart * t + 0.5f * k * t * t);
      float w = hann(n, activeSamples);
      float sample = sinf(phase) * w * amplitude;
      chirpBuffer[n] = (uint16_t)(2048.0f + sample * 2047.0f);
    } else {
      chirpBuffer[n] = 2048;
    }
  }
}

void onSampleTick() {
  dacWrite(chirpBuffer[sampleIndex]);
  sampleIndex++;
  if (sampleIndex >= BUFFER_SIZE) {
    sampleIndex = 0;
    bufferFinished = true;
  }
}

void updateFromEnvironment() {
  rawDepth     = analogRead(POT_DEPTH_PIN);
  rawTurbidity = analogRead(POT_TURBIDITY_PIN);
  rawSalinity  = analogRead(POT_SALINITY_PIN);

  currentDepth     = DEPTH_MIN_M      + (rawDepth     / 4095.0f) * (DEPTH_MAX_M - DEPTH_MIN_M);
  currentTurbidity = TURBIDITY_MIN    + (rawTurbidity / 4095.0f) * (TURBIDITY_MAX - TURBIDITY_MIN);
  currentSalinity  = SALINITY_MIN_PSU + (rawSalinity  / 4095.0f) * (SALINITY_MAX_PSU - SALINITY_MIN_PSU);

  float depthNorm     = currentDepth / DEPTH_MAX_M;
  float turbidityNorm = currentTurbidity / TURBIDITY_MAX;
  float salinityNorm  = currentSalinity / SALINITY_MAX_PSU;

  float difficulty = (0.5f * turbidityNorm) + (0.35f * depthNorm) + (0.15f * salinityNorm);
  currentCenterFreq = FREQ_MAX_HZ - difficulty * (FREQ_MAX_HZ - FREQ_MIN_HZ);

  currentDuration = DUR_MIN_S + depthNorm * (DUR_MAX_S - DUR_MIN_S);

  float amplitudeDemand = (0.6f * depthNorm) + (0.4f * salinityNorm);
  currentAmplitude = 0.3f + amplitudeDemand * 0.7f;
}

void setup() {
  Serial.begin(115200);
  uint32_t t0 = millis();
  while (!Serial && millis() - t0 < 3000) { }

  pinMode(DAC_CS_PIN, OUTPUT);
  digitalWrite(DAC_CS_PIN, HIGH);
  analogReadResolution(12);

  dacSPI.begin();
  dacSPI.beginTransaction(dacSPISettings);

  generateChirp(currentCenterFreq, currentDuration, currentAmplitude);

  sampleTimer = new HardwareTimer(TIM3);
  sampleTimer->setOverflow(SAMPLE_RATE_HZ, HERTZ_FORMAT);
  sampleTimer->attachInterrupt(onSampleTick);
  sampleTimer->resume();

  Serial.println("Adaptive sonar payload started.");
}

void loop() {
  updateFromEnvironment();

  if (bufferFinished) {
    bufferFinished = false;
    generateChirp(currentCenterFreq, currentDuration, currentAmplitude);
  }

  unsigned long now = millis();
  if (now - lastPrintMs >= 50) {
    lastPrintMs = now;

    float depthNorm     = currentDepth / DEPTH_MAX_M;
    float turbidityNorm = currentTurbidity / TURBIDITY_MAX;
    float salinityNorm  = currentSalinity / SALINITY_MAX_PSU;
    float freqNorm       = (currentCenterFreq - FREQ_MIN_HZ) / (FREQ_MAX_HZ - FREQ_MIN_HZ);
    float durNorm         = (currentDuration - DUR_MIN_S) / (DUR_MAX_S - DUR_MIN_S);

    Serial.print("Depth:");     Serial.print(depthNorm * 100.0f, 1);     Serial.print(",   ");
    Serial.print("Turbidity:"); Serial.print(turbidityNorm * 100.0f, 1); Serial.print(",   ");
    Serial.print("Salinity:");  Serial.print(salinityNorm * 100.0f, 1);  Serial.print(",   ");
    Serial.print("Freq:");      Serial.print(freqNorm * 100.0f, 1);      Serial.print(",   ");
    Serial.print("Duration:");  Serial.print(durNorm * 100.0f, 1);       Serial.print(",   ");
    Serial.print("Amplitude:"); Serial.println(currentAmplitude * 100.0f, 1);
  }
}
