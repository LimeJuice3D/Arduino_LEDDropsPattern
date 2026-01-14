#include <FastLED.h>
#include <math.h>
#include <stdio.h>

#define NUM_LEDS 50
#define LED_PIN 4

#define FALLOFF 1
#define LERP_SPEED 0.05
#define BRIGHTNESS 50
#define NEIGHBOUR_LERP_THRESHOLD 30
#define NEIGHBOUR_LERP_SPEED 0.5
#define BASE_COLOUR 0xde4710

#define SPAWN_DELAY_MIN 800
#define SPAWN_DELAY_MAX 1200
#define DIM_DELAY_MIN 20
#define DIM_DELAY_MAX 30
#define CYCLE_DELAY_MIN 40
#define CYCLE_DELAY_MAX 60

CRGB leds[NUM_LEDS];
CRGB vleds[NUM_LEDS]; // target virtual LED board which the real LEDs interpolate towards

unsigned long prev_spawn_mills = 0;
unsigned long prev_dim_mills = 0;
unsigned long prev_cycle_mills = 0;

struct influence {
  int dist;
  CRGB col;
};

void setup() {
  FastLED.addLeds<WS2812B, LED_PIN, RGB>(leds, NUM_LEDS);
  FastLED.setBrightness(BRIGHTNESS);
  
  for (int i = 0; i < NUM_LEDS; i++) {
    leds[i] = 0;
  }
}

void loop() {

  long spawn_delay = random(SPAWN_DELAY_MIN, SPAWN_DELAY_MAX);
  long dim_delay = random(DIM_DELAY_MIN, DIM_DELAY_MAX);
  long cycle_delay = random(CYCLE_DELAY_MIN, CYCLE_DELAY_MAX);

  unsigned long curr_mills = millis();

  if (curr_mills - prev_cycle_mills >= cycle_delay) {
    prev_cycle_mills = curr_mills;

    for (int i = 0; i < NUM_LEDS; i++) {
      struct influence arr[FALLOFF * 2];

      int arr_size = get_influences(arr, i);
      update_led(arr, arr_size, i);
    }
  }

  if (curr_mills - prev_dim_mills >= dim_delay) {
    prev_dim_mills = curr_mills;

    for (int i = 0; i < NUM_LEDS; i++) {
      dim_led(&vleds[i], 1);
      dim_led(&leds[i], 1);
    }
  }

  // Every so often, a random LED changes to a random colour
  if (curr_mills - prev_spawn_mills >= spawn_delay) {
    prev_spawn_mills = curr_mills;
    vleds[random(0, NUM_LEDS)] = BASE_COLOUR;
  }

  FastLED.show();
}

void dim_led(CRGB *led, int val) {
  if (led->r != 0) led->r = dim_no_overflow(led->r, val);
  if (led->g != 0) led->g = dim_no_overflow(led->g, val);
  if (led->b != 0) led->b = dim_no_overflow(led->b, val);
}

int dim_no_overflow(int curr, int val) {
  if (val > curr) {
    return 0;
  } else {
    return curr -= val;
  }
}

void update_led(struct influence arr[], int arr_size, int pos) {
  // average out all influences into a single colour
  CRGB avg = 0;

  int r_avg = 0;
  int g_avg = 0;
  int b_avg = 0;
  int total_strength = 0;

  for (int i = 0; i < arr_size; i++) {
    int strength = FALLOFF - arr[i].dist;
    r_avg += arr[i].col.r * strength;
    g_avg += arr[i].col.g * strength;
    b_avg += arr[i].col.b * strength;
    total_strength += strength;
  }

  avg.r = r_avg / total_strength;
  avg.g = g_avg / total_strength;
  avg.b = b_avg / total_strength;

  // find linear interpolation between current colour and target colour
  leds[pos].r += floor((avg.r - leds[pos].r) * LERP_SPEED * total_strength);
  leds[pos].g += floor((avg.g - leds[pos].g) * LERP_SPEED * total_strength);
  leds[pos].b += floor((avg.b - leds[pos].b) * LERP_SPEED * total_strength);

  // find the strongest neighbour and lerp to it
  CRGB for_neighbour = 0;
  CRGB back_neighbour = 0;
  CRGB strongest_neighbour;

  if (pos + 1 < NUM_LEDS) {
    for_neighbour = leds[pos+1];
  }

  if (pos - 1 >= 0) {
    back_neighbour = leds[pos-1];
  }

  int for_val = for_neighbour.r + for_neighbour.g + for_neighbour.b;
  int back_val = back_neighbour.r + back_neighbour.g + back_neighbour.b;
  int strongest_val;

  if (for_val > back_val) {
    strongest_neighbour = for_neighbour;
    strongest_val = for_val;
  } else {
    strongest_neighbour = back_neighbour;
    strongest_val = back_val;
  }

  int curr_val = leds[pos].r + leds[pos].g + leds[pos].b;

  // lerp to strongest neighbour if the difference between current and neighbour exceeds the neighbour lerp threshold
  if (strongest_val - curr_val > NEIGHBOUR_LERP_THRESHOLD) {
    leds[pos].r += floor((strongest_neighbour.r - leds[pos].r) * NEIGHBOUR_LERP_SPEED);
    leds[pos].g += floor((strongest_neighbour.g - leds[pos].g) * NEIGHBOUR_LERP_SPEED);
    leds[pos].b += floor((strongest_neighbour.b - leds[pos].b) * NEIGHBOUR_LERP_SPEED);
  }

  return;
}

int get_influences(struct influence arr[], int pos) {
  int arr_size = 0;

  for (int i = pos; i < pos + FALLOFF; i++) {
    if (i >= NUM_LEDS) break;

    if (vleds[i] != 0) {
      arr[arr_size] = { .dist = i - pos, .col = vleds[i] };
      arr_size++;
    }
  }

  for (int j = pos; j > pos - FALLOFF; j--) {
    if (j < 0) break;

    if (vleds[j] != 0) {
      arr[arr_size] = { .dist = pos - j, .col = vleds[j] };
      arr_size++;
    }
  }

  return arr_size;
}
