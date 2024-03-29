#include <limits.h>
#include <Adafruit_NeoPixel.h>

// Software configuration.
#define COLOR_MIXED 0
#define COLOR_RED 1
#define COLOR_GREEN 2
#define COLOR_BLUE 3
#define COLOR COLOR_RED

#define HISTORY 1
#define HISTORY_PEAK 0
#define HISTORY_TIME 1
#define HISTORY_TRIGGER HISTORY_TIME

// Hardware configuration (proto board v2).
#define ANALOG_PIN A1
#define STROBE_PIN 10
#define RESET_PIN 9
#define DISPLAY_PIN 8

// The MSGEQ7 has 7 bands which it samples for analog values.
// for more information, read the `read_msgeq7` function.
#define BANDS 7

// We have 28 LEDs along the width of the synth, going 6 deep back
// away from the keys.
//
// TODO: Build out the back with ideally, 28x28 more.
#define DISPLAY_WIDTH 28
#define DISPLAY_DEPTH 7

// Our interface.
void read_msgeq7(int spectrum[BANDS]);
void update_display(int spectrums[DISPLAY_DEPTH][BANDS]);
void shift(int spectrums[DISPLAY_DEPTH][BANDS]);
void clone(int spectrums[DISPLAY_DEPTH][BANDS]);
uint32_t intensity_color(int intensity, int loudest);
unsigned int max_index(int spectrum[BANDS]);

// The display object.
//
// TODO: This is currently the ONLY reason we are using Arduino,
// and we should be able to rewrite this in pure C once I get the
// time to write a WS2812 library.
Adafruit_NeoPixel display = Adafruit_NeoPixel(
  DISPLAY_WIDTH * DISPLAY_DEPTH,
  DISPLAY_PIN,
  NEO_GRB + NEO_KHZ800);

#if HISTORY && HISTORY_TRIGGER == HISTORY_TIME
unsigned int tick = 0;
#endif

void setup() {
  Serial.begin(9600);
  pinMode(ANALOG_PIN, INPUT);
  pinMode(STROBE_PIN, OUTPUT);
  pinMode(RESET_PIN, OUTPUT);
  analogReference(DEFAULT);
  digitalWrite(RESET_PIN, LOW);
  digitalWrite(STROBE_PIN, HIGH);

  display.begin();
  display.setBrightness(160);
}

void loop() {
  int spectrums[DISPLAY_DEPTH][BANDS];
  read_msgeq7(spectrums[0]);

#if HISTORY
  shift(spectrums);
#else
  clone(spectrums);
#endif

  update_display(spectrums);
}

// This function updates it's input array with the latest values.
//
// The array's structure is defined by the MSGEQ7, and has the following
// values: [63Hz, 160Hz, 400Hz, 1kHz, 2.5kHz, 6.25kHz, 16kHz].
void read_msgeq7(int spectrum[BANDS]) {
  // TODO: Dynamically set this value somehow?
  int spectrumOffset[BANDS] = { 60, 74, 68, 60, 62, 60, 60  };

  digitalWrite(RESET_PIN, HIGH);
  digitalWrite(RESET_PIN, LOW);

  for (int i = 0; i < BANDS; i++) {
    digitalWrite(STROBE_PIN, LOW);
    delayMicroseconds(90);  // why was this working at 30?
    spectrum[i] = analogRead(ANALOG_PIN) - spectrumOffset[i];
    if (spectrum[i] < 0) spectrum[i] = 0;
    digitalWrite(STROBE_PIN, HIGH);
  }
}

// TODO: Smooth out the bars so only the two in the middle are the brightest,
// this more closely matches the shape of the frequency response on the
// datasheet.
void update_display(int spectrums[DISPLAY_DEPTH][BANDS]) {
  int intensity;
  int loudest_band;

  for (int s = 0; s < DISPLAY_DEPTH; s++) {
    loudest_band = max_index(spectrums[s]);

    for (int b = 0; b < BANDS; b++) {
      if (s % 2 == 0) {
        intensity = map(spectrums[s][(BANDS - 1) - b], 0, 1024, 0, 255);
      } else {
        intensity = map(spectrums[s][b], 0, 1024, 0, 255);
      }

      for (int i = 0; i < DISPLAY_WIDTH / BANDS; i++) {
        int display_index = (s * 28) + (b * 4) + i;
        uint32_t color;
        color = intensity_color(intensity, loudest_band);
        display.setPixelColor(display_index, color);
      }
    }
  }

  display.show();
}

// Shift the spectrums as in a FIFO, triggering by either color, or time.
#if HISTORY
void shift(int spectrums[DISPLAY_DEPTH][BANDS]) {
#if HISTORY_TRIGGER == HISTORY_PEAK
  if (max_index(spectrums[0]) != max_index(spectrums[1])) {
#elif HISTORY_TRIGGER == HISTORY_TIME
  tick++;
  if (tick % 10 == 0) {
#endif
    for (int s = 0; s < DISPLAY_DEPTH; s++) {
      memcpy(spectrums[DISPLAY_DEPTH - s],
             spectrums[(DISPLAY_DEPTH - s) - 1],
             BANDS * sizeof(int));
    }
  }
}
#endif

void clone(int spectrums[DISPLAY_DEPTH][BANDS]) {
  for (int s = 0; s < DISPLAY_DEPTH; s++) {
    memcpy(spectrums[DISPLAY_DEPTH - s],
           spectrums[0],
           BANDS * sizeof(int));
  }
}

// Given the audio intensity and the loudest band and return a color.
//
// We take the audio intensity to change the light's intensity. We need to
// know the loudest band to change the color of the whole strip.
uint32_t intensity_color(int intensity, int loudest) {
  int scaled_intensity = map(intensity, 0, 1024, 0, 255);

#if COLOR == COLOR_MIXED
  switch (loudest) {
    case 0:
    case 1:
      return display.Color(0, 0, scaled_intensity);
    case 2:
    case 3:
      return display.Color(0, scaled_intensity, 0);
    case 4:
    case 5:
      return display.Color(scaled_intensity, 0, 0);
    default:
      return display.Color(scaled_intensity,
                           scaled_intensity,
                           scaled_intensity);
  }

#elif COLOR == COLOR_RED
  return display.Color(scaled_intensity, 0, 0);
#elif COLOR == COLOR_GREEN
  return display.Color(0, scaled_intensity, 0);
#else
  return display.Color(0, 0, scaled_intensity);
#endif
}

// Helper function for determaining the band which is loudest. Nothing to
// interesting to see here.
unsigned int max_index(int spectrum[BANDS]) {
  int max_v = INT_MIN;
  int max_i = 0;
  for (int i = 0; i < BANDS; i++) {
    if (spectrum[i] > max_v) {
      max_v = spectrum[i];
      max_i = i;
    }
  }
  return max_i;
}
