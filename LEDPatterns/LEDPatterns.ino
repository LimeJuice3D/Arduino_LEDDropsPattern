#include <FastLED.h>
#include <math.h>

#define NUM_LEDS 50
#define LED_PIN 4
#define BUTTON_PIN 2

// Modes
#define DROPS 0
#define READING 1
#define SNAKE 2
#define NUM_MODES 3

// Common constants
#define BRIGHTNESS 255
#define FALLOFF 1
#define DIM_AMOUNT 1
#define LERP_SPEED 0.05
#define NEIGHBOUR_LERP_THRESHOLD 30
#define NEIGHBOUR_LERP_SPEED 0.2
#define SPAWN_DELAY_MIN 500
#define SPAWN_DELAY_MAX 700
#define DIM_DELAY_MIN 20
#define DIM_DELAY_MAX 20
#define CYCLE_DELAY_MIN 40
#define CYCLE_DELAY_MAX 60

// Drops pattern constants
#define DROPS_COLOUR 0xf77c3e
#define LIFETIME_DELAY_MIN 500
#define LIFETIME_DELAY_MAX 700

// Reading pattern constants
#define READING_COLOUR 0xFFA930

// Snake pattern constants
#define SNAKE_COLOUR 0x2791F5
#define EXPLODE_COLOUR 0xf77c3e
#define MAX_SNAKES 2
#define SNAKE_TRAVEL_TIME 200
#define TRAVEL_LEFT 0
#define TRAVEL_RIGHT 1
#define EXPLOSION_DELAY_MIN 1000
#define EXPLOSION_DELAY_MAX 2000

CRGB leds[NUM_LEDS];
CRGB vleds[NUM_LEDS]; // target virtual LED board which the real LEDs interpolate towards
long vled_lifetime[NUM_LEDS];
int vled_dir[NUM_LEDS];

bool is_exploded_led[NUM_LEDS];
long explode_lifetime[NUM_LEDS];

unsigned long prev_spawn_mills = 0;
unsigned long prev_dim_mills = 0;
unsigned long prev_cycle_mills = 0;
unsigned long prev_snake_mills = 0;

long spawn_delay = random(SPAWN_DELAY_MIN, SPAWN_DELAY_MAX);
long dim_delay = random(DIM_DELAY_MIN, DIM_DELAY_MAX);
long cycle_delay = random(CYCLE_DELAY_MIN, CYCLE_DELAY_MAX);
long snake_delay = SNAKE_TRAVEL_TIME;

struct influence {
  int dist;
  CRGB col;
};

int mode = SNAKE;
bool button_held = false;

struct snake {
  int index;
  int direction;
};

struct snake snakes[MAX_SNAKES];
int num_snakes = 0;

void setup() {
  FastLED.addLeds<WS2812B, LED_PIN, RGB>(leds, NUM_LEDS);
  FastLED.setBrightness(BRIGHTNESS);

  pinMode(BUTTON_PIN, INPUT);

  reset();
}

void loop() {
  // Logic for switching modes using button
  int buttonState = digitalRead(BUTTON_PIN);

  if (buttonState == HIGH && !button_held) {
    mode = (mode + 1) % NUM_MODES;
    button_held = true;
    reset();
  } else if (buttonState == LOW) {
    button_held = false;
  }

  // Apply mode
  switch (mode) {
    case DROPS:
      drops_pattern();
      break;
    case READING:
      reading_pattern();
      break;
    case SNAKE:
      snake_pattern();
      break;
    default:
      reset();
      break;
  }

  FastLED.show();
}

void snake_pattern(void) {
  unsigned long curr_mills = millis();

  update_leds(curr_mills);
  dim_leds(curr_mills);

  // Explode colliding snakes
  for (int i = 0; i < num_snakes; i++) {
    for (int j = i + 1; j < num_snakes; j++) {
      if (abs(snakes[i].index - snakes[j].index) <= 1) {
        is_exploded_led[snakes[i].index] = true;
        explode_lifetime[snakes[i].index] = curr_mills + random(EXPLOSION_DELAY_MIN, EXPLOSION_DELAY_MAX);
        remove_snake(i);
        remove_snake(j);
      }
    }
  }

  // Move snakes
  if (curr_mills - prev_snake_mills >= snake_delay) {
    snake_delay = curr_mills + SNAKE_TRAVEL_TIME;

    for (int i = 0; i < num_snakes; i++) {
      if (snakes[i].direction == TRAVEL_LEFT) {
        if (snakes[i].index <= 0) {
          snakes[i].direction = TRAVEL_RIGHT;
        } else {
          snakes[i].index -= 1;
        }
      }

      if (snakes[i].direction == TRAVEL_RIGHT) {
        if (snakes[i].index >= NUM_LEDS - 1) {
          snakes[i].direction = TRAVEL_LEFT;
        } else {
          snakes[i].index += 1;
        }
      }
    }
  }

  // Spawn new snakes
  if (curr_mills - prev_spawn_mills >= spawn_delay) {
    spawn_delay = random(SPAWN_DELAY_MIN, SPAWN_DELAY_MAX);
    prev_spawn_mills = curr_mills;

    if (num_snakes < MAX_SNAKES) {
      snakes[num_snakes].index = random(0, NUM_LEDS);
      snakes[num_snakes].direction = random(TRAVEL_LEFT, TRAVEL_RIGHT + 1);
      num_snakes++;
    }
  }

  // update snakes on vled
  for (int i = 0; i < NUM_LEDS; i++) {
    vleds[i] = 0;

    if (is_exploded_led[i]) {
      vleds[i] = EXPLODE_COLOUR;
    }
  }

  for (int i = 0; i < num_snakes; i++) {
    vleds[snakes[i].index] = SNAKE_COLOUR;
  }

  // turn is_exploded_led off after some time
  for (int i = 0; i < NUM_LEDS; i++) {
    if (is_exploded_led[i] && curr_mills >= explode_lifetime[i]) {
      is_exploded_led[i] = false;
    }
  }

  return;
}

void remove_snake(int index) {
  for (int i = index; i < num_snakes - 1; i++) {
    snakes[i] = snakes[i + 1];
  }

  num_snakes--;

  return;
}

bool vled_dir_facing(int dir1, int dir2) {
  if (dir1 == TRAVEL_LEFT && dir2 == TRAVEL_RIGHT) return true;
  if (dir1 == TRAVEL_RIGHT && dir2 == TRAVEL_LEFT) return true;

  return false;
}

void reading_pattern(void) {
  for (int i = 0; i < NUM_LEDS; i++) {
    leds[i] = READING_COLOUR;
  }

  return;
}

void drops_pattern(void) {
  unsigned long curr_mills = millis();

  update_leds(curr_mills);
  dim_leds(curr_mills);

  // Every so often, a random vLED changes to a random colour
  if (curr_mills - prev_spawn_mills >= spawn_delay) {
    spawn_delay = random(SPAWN_DELAY_MIN, SPAWN_DELAY_MAX);
    prev_spawn_mills = curr_mills;

    int vled_index = random(0, NUM_LEDS);

    vleds[vled_index] = DROPS_COLOUR;
    vled_lifetime[vled_index] = curr_mills + random(LIFETIME_DELAY_MIN, LIFETIME_DELAY_MAX);
  }

  // turn vled off after some time
  for (int i = 0; i < NUM_LEDS; i++) {
    if (vleds[i] != 0 && curr_mills >= vled_lifetime[i]) {
      vleds[i] = 0;
    }
  }
}

void dim_leds(unsigned long curr_mills) {
  // dim LEDs
  if (curr_mills - prev_dim_mills >= dim_delay) {
    dim_delay = random(DIM_DELAY_MIN, DIM_DELAY_MAX);
    prev_dim_mills = curr_mills;

    for (int i = 0; i < NUM_LEDS; i++) {
      if (leds[i].r != 0) leds[i].r = dim_no_overflow(leds[i].r);
      if (leds[i].g != 0) leds[i].g = dim_no_overflow(leds[i].g);
      if (leds[i].b != 0) leds[i].b = dim_no_overflow(leds[i].b);
    }
  }
}

void update_leds(unsigned long curr_mills) {
  // update LED values 
  if (curr_mills - prev_cycle_mills >= cycle_delay) {
    cycle_delay = random(CYCLE_DELAY_MIN, CYCLE_DELAY_MAX);
    prev_cycle_mills = curr_mills;

    for (int i = 0; i < NUM_LEDS; i++) {
      struct influence arr[FALLOFF * 2];

      int arr_size = get_influences(arr, i);
      influence_leds(arr, arr_size, i);
    }
  }
}

void reset(void) {
  for (int i = 0; i < NUM_LEDS; i++) {
    leds[i] = 0;
    vleds[i] = 0;
    vled_lifetime[i] = 0;
  }

  return;
}

int dim_no_overflow(int curr) {
  if (DIM_AMOUNT > curr) {
    return 0;
  } else {
    return curr -= DIM_AMOUNT;
  }
}

void influence_leds(struct influence arr[], int arr_size, int pos) {
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
