#include <pebble.h>

#define RATE 30 // button refresh rate
#define MIN 16 // minimum tempo
#define MAX 512 // maximum tempo
#define INCREMENT 2 // how much tempo changes on button press

#define INITIAL_TEMPO 128
#define INITIAL_BEAT 0
#define INITIAL_VIBE_DURATION 48

#define KEY_TEMPO 0 // for persistent storage and app_message
#define KEY_VIBE_DURATION 1
#define KEY_BEAT 2

#define FORCE_COLOR false

static Window *main_window; // ouch! these right here sure are pointy
static Window *settings_window;
static TextLayer *status_layer;
static TextLayer *output_layer;
static TextLayer *bpm_layer;
static TextLayer *beat_layer;
static AppTimer *timer;

static int tempo; // variables and things
static int beat;
static int beat_counter;
static char tempo_string [4];
static char beat_string [4];
static bool state;
static bool color;

static VibePattern vibe; // variables for vibrations
static VibePattern beat_vibe;
static uint32_t vibe_segments [1];
static uint32_t beat_vibe_segments [1];
static int vibe_duration;

static void metronome_loop() { // runs every beat
  timer = app_timer_register(60000 / tempo, metronome_loop, NULL);
  if(state) {
    if(beat) {
      beat_counter++;
      if(beat == beat_counter){
        vibes_enqueue_custom_pattern(beat_vibe);
        beat_counter = 0;
      } else {
        vibes_enqueue_custom_pattern(vibe);
      }
    } else {
      vibes_enqueue_custom_pattern(vibe);
    }
  }
}

static bool get_color() { // is color supported?
  if (FORCE_COLOR) {
    return true;
  } else {
    switch(watch_info_get_model()) {
      case WATCH_INFO_MODEL_PEBBLE_ORIGINAL:
        return false;
      case WATCH_INFO_MODEL_PEBBLE_STEEL:
        return false;
      case WATCH_INFO_MODEL_UNKNOWN:
        return false;
      default:
        return true;
    }
  }
}

static void set_vibe(int vibe_duration) {
  vibe_segments[0] = (uint32_t) {vibe_duration};
  beat_vibe_segments[0] = (uint32_t) {vibe_duration} * 3;
  vibe = (VibePattern) {
    .durations = vibe_segments,
    .num_segments = 1
  };
  beat_vibe = (VibePattern) {
    .durations = beat_vibe_segments,
    .num_segments = 1
  };
}
static void set_tempo(int new) {
  snprintf(tempo_string, sizeof tempo_string, "%d", new);
  text_layer_set_text(output_layer, tempo_string);
  tempo = new;
}
static void set_beat(int new) {
  snprintf(beat_string, sizeof beat_string, "%d", new);
  text_layer_set_text(beat_layer, beat_string);
  beat = new;
}
static void set_state(bool new) {
  state = new;
  text_layer_set_text(status_layer, (state ? "ON" : "OFF"));
  if(color) {
    text_layer_set_text_color(status_layer, (state ? GColorGreen : GColorRed));
  } else {
    text_layer_set_text_color(status_layer, GColorWhite);
  }
}

static void inbox_received_handler(DictionaryIterator *iterator, void *context) { // i have no idea what i'm doing here, please fork
  Tuple *tuple_vibe_duration = dict_find(iterator, KEY_VIBE_DURATION);
  if(tuple_vibe_duration) {
    vibe_duration = atoi(tuple_vibe_duration->value->cstring);
  }
  set_vibe(vibe_duration);
}

static void tempo_up(ClickRecognizerRef recognizer, void *context) { // click handlers
  if(tempo < MAX) {
    set_tempo(tempo + INCREMENT);
  }
}
static void toggle_metronome(ClickRecognizerRef recognizer, void *context) {
  set_state(!state);
}
static void tempo_down(ClickRecognizerRef recognizer, void *context) {
  if (tempo > MIN) {
    set_tempo(tempo - INCREMENT);
  }
}
static void beat_up(ClickRecognizerRef recognizer, void *context) {
  set_beat(beat + 1);
}
static void submit(ClickRecognizerRef recognizer, void *context) {
  window_stack_remove(settings_window, true);
}
static void beat_down(ClickRecognizerRef recognizer, void *context) {
  if (beat > 0) {
    set_beat(beat - 1);
  }
}
static void long_click_handler(ClickRecognizerRef recognizer, void *context) {
  window_stack_push(settings_window, true);
}

static void main_click_config_provider(void *context) { // register the click handlers
  window_single_repeating_click_subscribe(BUTTON_ID_UP, RATE, tempo_up);
  window_single_click_subscribe(BUTTON_ID_SELECT, toggle_metronome);
  window_single_repeating_click_subscribe(BUTTON_ID_DOWN, RATE, tempo_down);
  window_long_click_subscribe(BUTTON_ID_SELECT, 0, long_click_handler, NULL);
}

static void settings_click_config_provider(void *context) {
  window_single_click_subscribe(BUTTON_ID_UP, beat_up);
  window_single_click_subscribe(BUTTON_ID_SELECT, submit);
  window_single_click_subscribe(BUTTON_ID_DOWN, beat_down);
}

static void main_window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect window_bounds = layer_get_bounds(window_layer);

  // status layer
  status_layer = text_layer_create(GRect(0, window_bounds.size.h / 2 - (42 / 2) - 28 - 5, window_bounds.size.w, 42));
  text_layer_set_font(status_layer, fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD));
  text_layer_set_background_color(status_layer, GColorClear);
  text_layer_set_text_alignment(status_layer, GTextAlignmentCenter);
  layer_add_child(window_layer, text_layer_get_layer(status_layer));
  
  // output layer
  output_layer = text_layer_create(GRect(0, window_bounds.size.h / 2 - (42 / 2) - 5, window_bounds.size.w, 42));
  text_layer_set_font(output_layer, fonts_get_system_font(FONT_KEY_BITHAM_42_BOLD));
  text_layer_set_background_color(output_layer, GColorClear);
  text_layer_set_text_color(output_layer, GColorWhite);
  text_layer_set_text_alignment(output_layer, GTextAlignmentCenter);
  layer_add_child(window_layer, text_layer_get_layer(output_layer));
  
  // bpm layer
  bpm_layer = text_layer_create(GRect(0, window_bounds.size.h / 2 + (42 / 2) - 5, window_bounds.size.w, 42));
  text_layer_set_font(bpm_layer, fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD));
  text_layer_set_background_color(bpm_layer, GColorClear);
  text_layer_set_text_color(bpm_layer, GColorWhite);
  text_layer_set_text_alignment(bpm_layer, GTextAlignmentCenter);
  text_layer_set_text(bpm_layer, "BPM");
  layer_add_child(window_layer, text_layer_get_layer(bpm_layer));

  // read stuff
  vibe_duration = (persist_exists(KEY_VIBE_DURATION) ? persist_read_int(KEY_VIBE_DURATION) : INITIAL_VIBE_DURATION);
  set_vibe(vibe_duration);
  set_tempo(persist_exists(KEY_TEMPO) ? persist_read_int(KEY_TEMPO) : INITIAL_TEMPO);
  beat = persist_exists(KEY_BEAT) ? persist_read_int(KEY_BEAT) : INITIAL_BEAT;

  color = get_color();
  set_state(true);
  metronome_loop();
}

static void main_window_unload(Window *window) { // destroy text layers
  text_layer_destroy(output_layer);
  text_layer_destroy(bpm_layer);
  text_layer_destroy(status_layer);
}

static void settings_window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect window_bounds = layer_get_bounds(window_layer);

  // beat layer
  beat_layer = text_layer_create(GRect(0, window_bounds.size.h / 2 - (42 / 2) - 5, window_bounds.size.w, 42));
  text_layer_set_font(beat_layer, fonts_get_system_font(FONT_KEY_BITHAM_42_BOLD));
  text_layer_set_background_color(beat_layer, GColorClear);
  text_layer_set_text_color(beat_layer, GColorWhite);
  text_layer_set_text_alignment(beat_layer, GTextAlignmentCenter);
  layer_add_child(window_layer, text_layer_get_layer(beat_layer));

  set_beat(beat);
}

static void settings_window_unload(Window *window) {
  text_layer_destroy(beat_layer);
}

static void init() {
  // create main window
  main_window = window_create();
  window_set_background_color(main_window, GColorBlack);
  window_set_window_handlers(main_window, (WindowHandlers) {
    .load = main_window_load,
    .unload = main_window_unload
  });

  settings_window = window_create();
  window_set_background_color(settings_window, GColorBlack);
  window_set_window_handlers(settings_window, (WindowHandlers) {
    .load = settings_window_load,
    .unload = settings_window_unload
    });
  
  app_message_register_inbox_received(inbox_received_handler);
  app_message_open(app_message_inbox_size_maximum(), app_message_outbox_size_maximum());
  
  window_set_click_config_provider(main_window, main_click_config_provider);
  window_set_click_config_provider(settings_window, settings_click_config_provider);
  window_stack_push(main_window, true);
}

static void deinit() {
  vibes_cancel();
  app_timer_cancel(timer);
  
  persist_write_int(KEY_TEMPO, tempo); // save for later
  persist_write_int(KEY_VIBE_DURATION, vibe_duration);
  persist_write_int(KEY_BEAT, beat);
  
  window_destroy(main_window); // destroy main window
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}