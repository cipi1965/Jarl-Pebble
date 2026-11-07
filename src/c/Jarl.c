#include <pebble.h>
#include <ctype.h>
#include "gbitmap_color_palette_manipulator.h"

#define ANIM_DURATION_DOWN 1000
#define ANIM_DURATION_UP 700
#define ANIM_DELAY 300
#define DATE_ON_SCREEN_DURATION 4000
#define TOTAL_ANIM_DURATION 6000
#define ANIM_LENGTH 84
#define ANIM_DURATION_DIGITS 1000
#define TOTAL_ANIM_DIGIT_DURATION 2000

#define SETTINGS_KEY 100
// last weather received, shown at startup before the phone answers
#define WEATHER_TEMP_KEY 101
#define WEATHER_COND_KEY 102

#define WIDTH PBL_DISPLAY_WIDTH
#define HEIGHT PBL_DISPLAY_HEIGHT
#define X_ORIGIN 0
#define Y_ORIGIN 0
#define HOR_OFFSET PBL_DISPLAY_WIDTH / 2
#define VER_OFFSET PBL_DISPLAY_HEIGHT / 2

// #define DEBUG_MEMORY

typedef struct ClaySettings
{
  bool UseCelsius;
  bool AnimateNumbers;
  bool VibrateOnConnect;
  bool VibrateOnDisconnect;
  GColor Colors[4];
  // appended last so settings saved by older versions still load
  uint8_t WeatherInterval; // minutes between weather updates
} ClaySettings;

struct tc_status {
  char current_char;
  int color_idx;
  GBitmap* bitmap;
  BitmapLayer* layer;
};

TextLayer *memory_layer;

const int BATTERY_RESOURCE_IDS[10] = {
    RESOURCE_ID_IMAGE_BATTERY_10, RESOURCE_ID_IMAGE_BATTERY_20, RESOURCE_ID_IMAGE_BATTERY_30,
    RESOURCE_ID_IMAGE_BATTERY_40, RESOURCE_ID_IMAGE_BATTERY_50, RESOURCE_ID_IMAGE_BATTERY_60,
    RESOURCE_ID_IMAGE_BATTERY_70, RESOURCE_ID_IMAGE_BATTERY_80, RESOURCE_ID_IMAGE_BATTERY_90,
    RESOURCE_ID_IMAGE_BATTERY_100};

// Day names drawn with the Jarl Mini font (fixed English, like the old
// day bitmaps; not taken from the locale, so only font glyphs are used)
static const char *const DAY_NAMES[7] = {"SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"};

const int NUMBERS_IDS[10] = {
    RESOURCE_ID_NUMBER_0, RESOURCE_ID_NUMBER_1, RESOURCE_ID_NUMBER_2,
    RESOURCE_ID_NUMBER_3, RESOURCE_ID_NUMBER_4, RESOURCE_ID_NUMBER_5,
    RESOURCE_ID_NUMBER_6, RESOURCE_ID_NUMBER_7, RESOURCE_ID_NUMBER_8,
    RESOURCE_ID_NUMBER_9};

const int WEATHER_IDS[10] = {
  [0] = RESOURCE_ID_IMAGE_W_CLEAR,
  [1] = RESOURCE_ID_IMAGE_W_CLEAR_NIGHT,
  [2] = RESOURCE_ID_IMAGE_W_PARTLY_CLOUDY,
  [3] = RESOURCE_ID_IMAGE_W_PARTLY_CLOUDY_NIGHT,
  [4] = RESOURCE_ID_IMAGE_W_CLOUDY,
  [5] = RESOURCE_ID_IMAGE_W_FOG,
  [6] = RESOURCE_ID_IMAGE_W_DRIZZLE,
  [7] = RESOURCE_ID_IMAGE_W_RAIN,
  [8] = RESOURCE_ID_IMAGE_W_SNOW,
  [9] = RESOURCE_ID_IMAGE_W_THUNDER,
};

static Window *s_main_window;

// Info panel text, drawn with the Jarl Mini pixel font (13x17 glyphs, 2 px gap)
#define MINI_H 17
#define TEMP_X (((WIDTH / 2) - 39 - 21 - 6) / 2)
#define TEMP_W 68 /* "+21°C" incl. the 2 px gap after the last glyph */
#define DAY_X ((WIDTH / 2 - 43) / 2)
#define DAY_W 45 /* "MON" + gap */
#define DAYNUM_X ((WIDTH / 2) + ((WIDTH / 2) - 28) / 2)
#define DAYNUM_W 30 /* "07" + gap */
static GFont mini_font;
static TextLayer *temp_layer, *day_layer, *daynum_layer;
static char temp_text[10] = "----";
static char daynum_text[3];

static uint8_t battery_level;

static GBitmap *battery_bitmap;
static BitmapLayer *battery_layer;

static BitmapLayer *conditions_layer;
static GBitmap *conditions_bitmap;

static bool on_animation = false;
static bool start = true;
static bool redraw_all_digits = false; // set when the digit colors change

static int rand_minutes_unit = 0;
static int rand_minutes_dec = 0;
static int rand_hours_unit = 0;
static int rand_hours_dec = 0;

static struct tc_status hours_dec = {
  .current_char = 'a'
};
static struct tc_status hours_unit = {
  .current_char = 'a'
};
static struct tc_status minutes_dec = {
  .current_char = 'a'
};
static struct tc_status minutes_unit = {
  .current_char = 'a'
};

// temporizadores
AppTimer *timer;
AppTimer *timer2;
AppTimer *timer3;

ClaySettings settings;

static void load_default_settings()
{
  settings.AnimateNumbers = true;
  settings.UseCelsius = true;
  settings.VibrateOnConnect = false;
  settings.VibrateOnDisconnect = true;

  settings.Colors[0] = GColorDarkCandyAppleRed;
  settings.Colors[1] = GColorVividCerulean;
  settings.Colors[2] = GColorYellow;
  settings.Colors[3] = GColorGreen;

  settings.WeatherInterval = 30;
}

static void load_settings()
{
  load_default_settings();

  persist_read_data(SETTINGS_KEY, &settings, sizeof(settings));
}

void on_animation_stopped(Animation *anim, bool finished, void *context)
{
  // Free the memoery used by the Animation
  //  vibes_short_pulse();
  property_animation_destroy((PropertyAnimation *)anim);
}

void animate_layer(Layer *layer, GRect *start, GRect *finish, int duration, int delay)
{
  // Declare animation
  PropertyAnimation *anim = property_animation_create_layer_frame(layer, start, finish);

  // Set characteristics
  animation_set_duration((Animation *)anim, duration);
  animation_set_delay((Animation *)anim, delay);

  // Set stopped handler to free memory
  AnimationHandlers handlers = {
      // The reference to the stopped handler is the only one in the array
      .stopped = (AnimationStoppedHandler)on_animation_stopped};
  animation_set_handlers((Animation *)anim, handlers, NULL);

  // Start animation!
  animation_schedule((Animation *)anim);
}

static void bluetooth_callback(bool connected)
{
  // Show icon if disconnected
  // layer_set_hidden(bitmap_layer_get_layer(s_bt_icon_layer), connected);
  if (!connected)
  {
    // Issue a vibrating alert
    if (settings.VibrateOnDisconnect)
      vibes_double_pulse();
  }
  else
  {
    if (settings.VibrateOnConnect)
      vibes_short_pulse();
  }
}

static void update_date()
{
  time_t temp = time(NULL);
  struct tm *tick_time = localtime(&temp);

  text_layer_set_text(day_layer, DAY_NAMES[tick_time->tm_wday]);
  strftime(daynum_text, sizeof(daynum_text), "%d", tick_time);
  text_layer_set_text(daynum_layer, daynum_text);
}

static void update_battery()
{
  // battery update
  int level;
  BatteryChargeState initial = battery_state_service_peek();
  battery_level = initial.charge_percent;
  gbitmap_destroy(battery_bitmap);
  level = battery_level / 10;
  if (level == 10)
    level = 9;
  battery_bitmap = gbitmap_create_with_resource(BATTERY_RESOURCE_IDS[level]);
  bitmap_layer_set_bitmap(battery_layer, battery_bitmap);
}

void update_temp()
{
  DictionaryIterator *iter;
  // Fails when the phone is disconnected or a send is in flight; iter is
  // then not valid, so don't write to it.
  AppMessageResult result = app_message_outbox_begin(&iter);
  if (result != APP_MSG_OK)
  {
    APP_LOG(APP_LOG_LEVEL_WARNING, "Weather request skipped: %d", (int)result);
    return;
  }
  // Add a key-value pair
  dict_write_uint8(iter, 0, 0);
  // Send the message!
  app_message_outbox_send();
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Asked temperature update");
}

static void update_time_component(/* inout */ struct tc_status* status, char buffer, int number)
{
  bool changed = (status->current_char != buffer) || start;
  // Usually only one digit changes per minute: skip reloading and redrawing
  // the others (resource load + recolor + redraw cost CPU time and battery).
  if (!changed && !redraw_all_digits)
    return;

#ifdef PBL_COLOR
  if (changed)
  {
    status->color_idx = rand() % 4;
  }
  gbitmap_destroy(status->bitmap);
  status->bitmap = gbitmap_create_with_resource(NUMBERS_IDS[number]);
  replace_gbitmap_color(GColorWhite, settings.Colors[status->color_idx], status->bitmap, NULL);
  bitmap_layer_set_bitmap(status->layer, status->bitmap);
#else
  gbitmap_destroy(status->bitmap);
  status->bitmap = gbitmap_create_with_resource(NUMBERS_IDS[number]);
  bitmap_layer_set_bitmap(status->layer, status->bitmap);
#endif
  status->current_char = buffer;
  layer_mark_dirty(bitmap_layer_get_layer(status->layer));
}

static void update_time()
{
  // Get a tm structure
  time_t temp = time(NULL);
  struct tm *tick_time = localtime(&temp);
  char buffer[] = "00:00";

  if (clock_is_24h_style() == true)
  {
    // Use 24 hour format
    strftime(buffer, sizeof("00:00"), "%H:%M", tick_time);
  }
  else
  {
    // Use 12 hour format
    strftime(buffer, sizeof("00:00"), "%I:%M", tick_time);
  }

  int h1 = buffer[0] - '0';
  update_time_component(&hours_dec, buffer[0], h1);
  // 12h mode: no leading zero ("9:41", not "09:41")
  layer_set_hidden(bitmap_layer_get_layer(hours_dec.layer),
                   !clock_is_24h_style() && buffer[0] == '0');

  int h0 = buffer[1] - '0';
  update_time_component(&hours_unit, buffer[1], h0);

  int m1 = buffer[3] - '0';
  update_time_component(&minutes_dec, buffer[3], m1);

  int m0 = buffer[4] - '0';
  update_time_component(&minutes_unit, buffer[4], m0);

  start = false;
  redraw_all_digits = false;
}

void timer_callback(void *data)
{
  // animate temperature
  animate_layer(text_layer_get_layer(temp_layer), &GRect(TEMP_X, ANIM_LENGTH - VER_OFFSET + 12, TEMP_W, MINI_H), &GRect(TEMP_X, -VER_OFFSET + 12, TEMP_W, MINI_H), ANIM_DURATION_UP, ANIM_DELAY);

  // animate date
  animate_layer(text_layer_get_layer(day_layer), &GRect(DAY_X, ANIM_LENGTH + 6 - 42, DAY_W, MINI_H), &GRect(DAY_X, 6 - 42, DAY_W, MINI_H), ANIM_DURATION_UP, ANIM_DELAY);
  animate_layer(text_layer_get_layer(daynum_layer), &GRect(DAYNUM_X, ANIM_LENGTH + 6 - 42, DAYNUM_W, MINI_H), &GRect(DAYNUM_X, 6 - 42, DAYNUM_W, MINI_H), ANIM_DURATION_UP, ANIM_DELAY);

  // animate battery
  animate_layer(bitmap_layer_get_layer(battery_layer), &GRect((WIDTH - 144) / 2, -10 + ANIM_LENGTH, 144, 6), &GRect((WIDTH - 144) / 2, -10, 144, 6), ANIM_DURATION_UP, ANIM_DELAY);

  // animate conditions
  animate_layer(bitmap_layer_get_layer(conditions_layer), &GRect(HOR_OFFSET, 0, HOR_OFFSET, 42), &GRect(HOR_OFFSET, -VER_OFFSET, HOR_OFFSET, 42), ANIM_DURATION_UP, ANIM_DELAY);

  // animate time
  animate_layer(bitmap_layer_get_layer(hours_dec.layer), &GRect(X_ORIGIN, ANIM_LENGTH, HOR_OFFSET, VER_OFFSET), &GRect(X_ORIGIN, Y_ORIGIN, HOR_OFFSET, VER_OFFSET), ANIM_DURATION_UP, ANIM_DELAY);
  animate_layer(bitmap_layer_get_layer(hours_unit.layer), &GRect(HOR_OFFSET, ANIM_LENGTH, HOR_OFFSET, VER_OFFSET), &GRect(HOR_OFFSET, Y_ORIGIN, HOR_OFFSET, VER_OFFSET), ANIM_DURATION_UP, ANIM_DELAY);
  animate_layer(bitmap_layer_get_layer(minutes_dec.layer), &GRect(X_ORIGIN, VER_OFFSET + ANIM_LENGTH, HOR_OFFSET, VER_OFFSET), &GRect(X_ORIGIN, VER_OFFSET, HOR_OFFSET, VER_OFFSET), ANIM_DURATION_UP, ANIM_DELAY);
  animate_layer(bitmap_layer_get_layer(minutes_unit.layer), &GRect(HOR_OFFSET, VER_OFFSET + ANIM_LENGTH, HOR_OFFSET, VER_OFFSET), &GRect(HOR_OFFSET, VER_OFFSET, HOR_OFFSET, VER_OFFSET), ANIM_DURATION_UP, ANIM_DELAY);
}

void timer_callback2(void *data)
{
  on_animation = false;
}

static void accel_tap_handler(AccelAxisType axis, int32_t direction)
{
  // update_time();

  //  APP_LOG(APP_LOG_LEVEL_INFO, "Axis: %d", axis);

  if ((on_animation == false) && (axis == ACCEL_AXIS_Y))
  {
    on_animation = true;

    // animate temperature
    animate_layer(
      text_layer_get_layer(temp_layer),
      &GRect(TEMP_X, -VER_OFFSET + 12, TEMP_W, MINI_H),
      &GRect(TEMP_X, 12, TEMP_W, MINI_H),
      ANIM_DURATION_DOWN, ANIM_DELAY
    );

    // animate date
    animate_layer(
      text_layer_get_layer(day_layer),
      &GRect(DAY_X, 6 - 42, DAY_W, MINI_H),
      &GRect(DAY_X, ANIM_LENGTH + 6 - 42, DAY_W, MINI_H),
      ANIM_DURATION_DOWN, ANIM_DELAY
    );
    animate_layer(
      text_layer_get_layer(daynum_layer),
      &GRect(DAYNUM_X, 6 - 42, DAYNUM_W, MINI_H),
      &GRect(DAYNUM_X, ANIM_LENGTH + 6 - 42, DAYNUM_W, MINI_H),
      ANIM_DURATION_DOWN, ANIM_DELAY
    );

    // animate battery
    animate_layer(
      bitmap_layer_get_layer(battery_layer),
      &GRect((WIDTH - 144) / 2, -10, 144, 6),
      &GRect((WIDTH - 144) / 2, -10 + ANIM_LENGTH, 144, 6),
      ANIM_DURATION_DOWN, ANIM_DELAY
    );

    // animate weather conditions
    animate_layer(
      bitmap_layer_get_layer(conditions_layer),
      &GRect(HOR_OFFSET, -VER_OFFSET, HOR_OFFSET, 42),
      &GRect(HOR_OFFSET, 0, HOR_OFFSET, 42),
      ANIM_DURATION_DOWN, ANIM_DELAY
    );

    // animate time
    animate_layer(bitmap_layer_get_layer(hours_dec.layer), &GRect(X_ORIGIN, Y_ORIGIN, HOR_OFFSET, VER_OFFSET), &GRect(X_ORIGIN, ANIM_LENGTH, HOR_OFFSET, VER_OFFSET), ANIM_DURATION_DOWN, ANIM_DELAY);
    animate_layer(bitmap_layer_get_layer(hours_unit.layer), &GRect(HOR_OFFSET, Y_ORIGIN, HOR_OFFSET, VER_OFFSET), &GRect(HOR_OFFSET, ANIM_LENGTH, HOR_OFFSET, VER_OFFSET), ANIM_DURATION_DOWN, ANIM_DELAY);
    animate_layer(bitmap_layer_get_layer(minutes_dec.layer), &GRect(X_ORIGIN, VER_OFFSET, HOR_OFFSET, VER_OFFSET), &GRect(X_ORIGIN, VER_OFFSET + ANIM_LENGTH, HOR_OFFSET, VER_OFFSET), ANIM_DURATION_DOWN, ANIM_DELAY);
    animate_layer(bitmap_layer_get_layer(minutes_unit.layer), &GRect(HOR_OFFSET, VER_OFFSET, HOR_OFFSET, VER_OFFSET), &GRect(HOR_OFFSET, VER_OFFSET + ANIM_LENGTH, HOR_OFFSET, VER_OFFSET), ANIM_DURATION_DOWN, ANIM_DELAY);

    // suscribirse a temporizadores
    timer = app_timer_register(DATE_ON_SCREEN_DURATION, timer_callback, NULL);
    timer2 = app_timer_register(TOTAL_ANIM_DURATION, timer_callback2, NULL);
  }
  else
  {
    // app_timer_reschedule(timer, 0);
  }
}

void timer_callback3(void *data)
{
  on_animation = false;
}

static TextLayer *make_mini_text_layer(Window *window, GRect frame)
{
  TextLayer *layer = text_layer_create(frame);
  text_layer_set_background_color(layer, GColorClear);
  text_layer_set_text_color(layer, GColorWhite);
  text_layer_set_font(layer, mini_font);
  text_layer_set_text_alignment(layer, GTextAlignmentLeft);
  layer_add_child(window_get_root_layer(window), text_layer_get_layer(layer));
  return layer;
}

static void main_window_load(Window *window)
{
  srand(time(NULL));

  // Load bitmaps into GBitmap structures

  // números
  hours_dec.layer = bitmap_layer_create(GRect(0, -84, 72, 84));
  layer_add_child(window_get_root_layer(window), bitmap_layer_get_layer(hours_dec.layer));
  hours_unit.layer = bitmap_layer_create(GRect(144, 0, 72, 84));
  layer_add_child(window_get_root_layer(window), bitmap_layer_get_layer(hours_unit.layer));
  minutes_dec.layer = bitmap_layer_create(GRect(-73, 84, 72, 84));
  layer_add_child(window_get_root_layer(window), bitmap_layer_get_layer(minutes_dec.layer));
  minutes_unit.layer = bitmap_layer_create(GRect(73, 168, 72, 84));
  layer_add_child(window_get_root_layer(window), bitmap_layer_get_layer(minutes_unit.layer));

  // Info panel text (hidden above the screen until the panel slides in)
  mini_font = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_JARL_MINI_17));
  temp_layer = make_mini_text_layer(window, GRect(TEMP_X, -VER_OFFSET + 12, TEMP_W, MINI_H));
  text_layer_set_text(temp_layer, temp_text);
  day_layer = make_mini_text_layer(window, GRect(DAY_X, 6 - 42, DAY_W, MINI_H));
  daynum_layer = make_mini_text_layer(window, GRect(DAYNUM_X, 6 - 42, DAYNUM_W, MINI_H));

  // create battery layers
  battery_layer = bitmap_layer_create(GRect(0, -10, 144, 6));
  layer_add_child(window_get_root_layer(window), bitmap_layer_get_layer(battery_layer));

  // conditions
  conditions_layer = bitmap_layer_create(GRect(72, -84, 72, 42));
  layer_add_child(window_get_root_layer(window), bitmap_layer_get_layer(conditions_layer));

// BORRAR
#ifdef DEBUG_MEMORY
  memory_layer = text_layer_create(GRect(0, 148, 144, 20));
  // Set the text, font, and text alignment
  text_layer_set_text(memory_layer, "Memory");
  text_layer_set_font(memory_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
  text_layer_set_text_alignment(memory_layer, GTextAlignmentCenter);

  // Add the text layer to the window
  layer_add_child(window_get_root_layer(window), text_layer_get_layer(memory_layer));
#endif
  // BORRAR

  window_set_background_color(window, GColorBlack);
#ifdef PBL_COLOR
  window_set_background_color(window, GColorBlack);
#endif

  // Show the correct state of the BT connection from the start
  // bluetooth_callback(connection_service_peek_pebble_app_connection());

  update_battery();
  update_date();

  rand_minutes_unit = 1;
  rand_minutes_dec = 0;
  rand_hours_unit = 0;
  rand_hours_dec = 1;
  animate_layer(bitmap_layer_get_layer(hours_dec.layer), &GRect(-HOR_OFFSET + rand_hours_dec * HOR_OFFSET, -VER_OFFSET * rand_hours_dec, HOR_OFFSET, VER_OFFSET), &GRect(X_ORIGIN, Y_ORIGIN, HOR_OFFSET, VER_OFFSET), ANIM_DURATION_DIGITS, 0);
  animate_layer(bitmap_layer_get_layer(minutes_unit.layer), &GRect(WIDTH - rand_minutes_unit * HOR_OFFSET, VER_OFFSET + VER_OFFSET * rand_minutes_unit, HOR_OFFSET, VER_OFFSET), &GRect(HOR_OFFSET, VER_OFFSET, HOR_OFFSET, VER_OFFSET), ANIM_DURATION_DIGITS, 0);
  animate_layer(bitmap_layer_get_layer(minutes_dec.layer), &GRect(-HOR_OFFSET + rand_minutes_dec * HOR_OFFSET, VER_OFFSET + rand_minutes_dec * VER_OFFSET, HOR_OFFSET, VER_OFFSET), &GRect(X_ORIGIN, VER_OFFSET, HOR_OFFSET, VER_OFFSET), ANIM_DURATION_DIGITS, 0);
  animate_layer(bitmap_layer_get_layer(hours_unit.layer), &GRect(WIDTH - HOR_OFFSET * rand_hours_unit, -VER_OFFSET * rand_hours_unit, HOR_OFFSET, VER_OFFSET), &GRect(HOR_OFFSET, Y_ORIGIN, HOR_OFFSET, VER_OFFSET), ANIM_DURATION_DIGITS, 0);

  update_time();
}

// Slide the digits that are about to change off screen. Runs once per minute,
// at second 59, from an AppTimer (instead of waking up every second).
static AppTimer *slide_out_timer = NULL;

static void slide_out_callback(void *data)
{
  slide_out_timer = NULL;
  if (!settings.AnimateNumbers || on_animation)
    return; // disabled, or the date/weather panel is showing

  time_t now = time(NULL);
  struct tm *tick_time = localtime(&now);
  // Timer ran late and the minute already rolled over: the digits below would
  // be computed for the new minute, so skip this slide-out.
  if (tick_time->tm_sec < 30)
    return;
  int minutes = tick_time->tm_min;
  int hours = tick_time->tm_hour;

  timer3 = app_timer_register(TOTAL_ANIM_DIGIT_DURATION, timer_callback3, NULL);
  animate_layer(bitmap_layer_get_layer(minutes_unit.layer), &GRect(HOR_OFFSET, VER_OFFSET, HOR_OFFSET, VER_OFFSET), &GRect(WIDTH - rand_minutes_unit * HOR_OFFSET, VER_OFFSET + VER_OFFSET * rand_minutes_unit, HOR_OFFSET, VER_OFFSET), ANIM_DURATION_DIGITS, 0);

  if (minutes % 10 == 9)
  {
    animate_layer(bitmap_layer_get_layer(minutes_dec.layer), &GRect(X_ORIGIN, VER_OFFSET, HOR_OFFSET, VER_OFFSET), &GRect(-HOR_OFFSET + rand_minutes_dec * HOR_OFFSET, VER_OFFSET + rand_minutes_dec * VER_OFFSET, HOR_OFFSET, VER_OFFSET), ANIM_DURATION_DIGITS, 0);
  }
  if (minutes == 59)
  {
    animate_layer(bitmap_layer_get_layer(hours_unit.layer), &GRect(HOR_OFFSET, Y_ORIGIN, HOR_OFFSET, VER_OFFSET), &GRect(WIDTH - HOR_OFFSET * rand_hours_unit, -VER_OFFSET * rand_hours_unit, HOR_OFFSET, VER_OFFSET), ANIM_DURATION_DIGITS, 0);
    if ((hours == 23) | (hours == 19) | (hours == 9))
    {
      animate_layer(bitmap_layer_get_layer(hours_dec.layer), &GRect(X_ORIGIN, Y_ORIGIN, HOR_OFFSET, VER_OFFSET), &GRect(-HOR_OFFSET + rand_hours_dec * HOR_OFFSET, -VER_OFFSET * rand_hours_dec, HOR_OFFSET, VER_OFFSET), ANIM_DURATION_DIGITS, 0);
    }
  }
}

// Arm a one-shot timer for one second before the next minute (like the old
// per-second tick at :59), computed with millisecond precision.
static void schedule_slide_out(void)
{
  if (slide_out_timer)
  {
    app_timer_cancel(slide_out_timer);
    slide_out_timer = NULL;
  }
  if (!settings.AnimateNumbers)
    return;
  time_t now_s;
  uint16_t now_ms;
  time_ms(&now_s, &now_ms);
  int32_t ms_to_next_minute = (60 - localtime(&now_s)->tm_sec) * 1000 - now_ms;
  int32_t delay = ms_to_next_minute - 1000;
  if (delay > 0)
    slide_out_timer = app_timer_register(delay, slide_out_callback, NULL);
}

// Slide the new digits back in (same directions as the slide-out at :59).
static void slide_in(int minutes, int hours)
{
  if (!settings.AnimateNumbers || on_animation)
    return; // disabled, or the date/weather panel is showing

  animate_layer(bitmap_layer_get_layer(minutes_unit.layer), &GRect(WIDTH - rand_minutes_unit * HOR_OFFSET, VER_OFFSET + VER_OFFSET * rand_minutes_unit, HOR_OFFSET, VER_OFFSET), &GRect(HOR_OFFSET, VER_OFFSET, HOR_OFFSET, VER_OFFSET), ANIM_DURATION_DIGITS, 0);
  if (minutes % 10 == 0)
  {
    animate_layer(bitmap_layer_get_layer(minutes_dec.layer), &GRect(-HOR_OFFSET + rand_minutes_dec * HOR_OFFSET, VER_OFFSET + rand_minutes_dec * VER_OFFSET, HOR_OFFSET, VER_OFFSET), &GRect(X_ORIGIN, VER_OFFSET, HOR_OFFSET, VER_OFFSET), ANIM_DURATION_DIGITS, 0);
  }
  if (minutes == 0)
  {
    animate_layer(bitmap_layer_get_layer(hours_unit.layer), &GRect(WIDTH - HOR_OFFSET * rand_hours_unit, -VER_OFFSET * rand_hours_unit, HOR_OFFSET, VER_OFFSET), &GRect(HOR_OFFSET, Y_ORIGIN, HOR_OFFSET, VER_OFFSET), ANIM_DURATION_DIGITS, 0);
    if ((hours == 0) | (hours == 20) | (hours == 10))
    {
      animate_layer(bitmap_layer_get_layer(hours_dec.layer), &GRect(-HOR_OFFSET + rand_hours_dec * HOR_OFFSET, -VER_OFFSET * rand_hours_dec, HOR_OFFSET, VER_OFFSET), &GRect(X_ORIGIN, Y_ORIGIN, HOR_OFFSET, VER_OFFSET), ANIM_DURATION_DIGITS, 0);
    }
  }
}

// Called once per minute (MINUTE_UNIT) instead of every second.
static void tick_minutos(struct tm *tick_time, TimeUnits units_changed)
{
#ifdef DEBUG_MEMORY
  static char buf[] = "1234512345";
  snprintf(buf, sizeof(buf), "%u %u", heap_bytes_used(), heap_bytes_free());
  text_layer_set_text(memory_layer, buf);
#endif

  int minutes = tick_time->tm_min;
  int hours = tick_time->tm_hour;

  // a las horas en punto y a y media: weather and battery
  if ((minutes == 0) || (minutes == 30))
  {
    update_battery();
  }

  int interval = settings.WeatherInterval > 0 ? settings.WeatherInterval : 30;
  if ((hours * 60 + minutes) % interval == 0)
  {
    update_temp();
  }

  // a las cero horas
  if (units_changed & DAY_UNIT)
  {
    update_date();
  }

  update_time();
  slide_in(minutes, hours);

  // new random directions for the next slide-out / slide-in pair
  rand_minutes_unit = rand() % 2;
  rand_minutes_dec = rand() % 2;
  rand_hours_unit = rand() % 2;
  rand_hours_dec = rand() % 2;

  schedule_slide_out();
}

static void main_window_unload(Window *window)
{

  // Destroy GBitmaps
  gbitmap_destroy(hours_dec.bitmap);
  gbitmap_destroy(hours_unit.bitmap);
  gbitmap_destroy(minutes_dec.bitmap);
  gbitmap_destroy(minutes_unit.bitmap);

  // Destroy TextLayer
  bitmap_layer_destroy(hours_dec.layer);
  bitmap_layer_destroy(hours_unit.layer);
  bitmap_layer_destroy(minutes_dec.layer);
  bitmap_layer_destroy(minutes_unit.layer);

  gbitmap_destroy(conditions_bitmap);
  bitmap_layer_destroy(conditions_layer);

  // Destroy info panel text
  text_layer_destroy(temp_layer);
  text_layer_destroy(day_layer);
  text_layer_destroy(daynum_layer);
  fonts_unload_custom_font(mini_font);

  // battery
  bitmap_layer_destroy(battery_layer);
  gbitmap_destroy(battery_bitmap);
}

static void set_layer_bitmap(GBitmap **bitmap, BitmapLayer *layer, uint32_t resource_id)
{
  gbitmap_destroy(*bitmap);
  *bitmap = gbitmap_create_with_resource(resource_id);
  bitmap_layer_set_bitmap(layer, *bitmap);
}

// Draw the temperature (received in Celsius) as sign, two digits and unit.
static int last_celsius;
static bool have_temperature = false;

static void show_temperature(int celsius)
{
  last_celsius = celsius;
  have_temperature = true;

  int temp = celsius;
  if (settings.UseCelsius == false)
  {
    // integer math (no soft-float library); F*10 = C*18 + 320, rounded
    int f10 = temp * 18 + 320;
    temp = (f10 >= 0 ? f10 + 5 : f10 - 5) / 10;
  }
  // only two digits fit on screen
  if (temp > 99)
    temp = 99;
  if (temp < -99)
    temp = -99;

  int magnitude = temp < 0 ? -temp : temp;
  snprintf(temp_text, sizeof(temp_text), "%c%02d°%c",
           temp < 0 ? '-' : '+', magnitude, settings.UseCelsius ? 'C' : 'F');
  text_layer_set_text(temp_layer, temp_text);
}

static void show_conditions(int condition)
{
  // ignore codes we have no icon for instead of reading past WEATHER_IDS
  if (condition < 0 || condition >= (int)ARRAY_LENGTH(WEATHER_IDS))
  {
    APP_LOG(APP_LOG_LEVEL_WARNING, "Unknown weather condition %d", condition);
    return;
  }
  set_layer_bitmap(&conditions_bitmap, conditions_layer, WEATHER_IDS[condition]);
}

// Avoid flash writes when the weather hasn't changed.
static void persist_int_if_changed(uint32_t key, int32_t value)
{
  if (!persist_exists(key) || persist_read_int(key) != value)
    persist_write_int(key, value);
}

static void restore_weather()
{
  if (persist_exists(WEATHER_TEMP_KEY))
    show_temperature((int)persist_read_int(WEATHER_TEMP_KEY));
  if (persist_exists(WEATHER_COND_KEY))
    show_conditions((int)persist_read_int(WEATHER_COND_KEY));
}

static void in_received_handler(DictionaryIterator *iter, void *context)
{
  // Only write to flash when settings actually arrived (not on weather updates)
  bool settings_changed = false;

  Tuple *temp_units_t = dict_find(iter, MESSAGE_KEY_TempUnits);
  if (temp_units_t)
  {
    settings.UseCelsius = strcmp(temp_units_t->value->cstring, "c") == 0;
    settings_changed = true;
    // redraw in the new unit locally; the phone always sends Celsius
    if (have_temperature)
      show_temperature(last_celsius);
  }

  Tuple *animate_numbers_t = dict_find(iter, MESSAGE_KEY_AnimateNumbers);
  if (animate_numbers_t)
  {
    settings.AnimateNumbers = animate_numbers_t->value->int32 > 0;
    settings_changed = true;
    schedule_slide_out();
  }

  Tuple *weather_interval_t = dict_find(iter, MESSAGE_KEY_WeatherInterval);
  if (weather_interval_t)
  {
    // Clay sends select values as strings
    int minutes = weather_interval_t->type == TUPLE_CSTRING
                      ? atoi(weather_interval_t->value->cstring)
                      : (int)weather_interval_t->value->int32;
    if (minutes == 30 || minutes == 60 || minutes == 120)
    {
      settings.WeatherInterval = minutes;
      settings_changed = true;
    }
  }

  Tuple *vibrate_on_connect_t = dict_find(iter, MESSAGE_KEY_VibrateOnConnect);
  if (vibrate_on_connect_t)
  {
    settings.VibrateOnConnect = vibrate_on_connect_t->value->int32 > 0;
    settings_changed = true;
  }

  Tuple *vibrate_on_disconnect_t = dict_find(iter, MESSAGE_KEY_VibrateOnDisconnect);
  if (vibrate_on_disconnect_t)
  {
    settings.VibrateOnDisconnect = vibrate_on_disconnect_t->value->int32 > 0;
    settings_changed = true;
  }

  Tuple *color_1_t = dict_find(iter, MESSAGE_KEY_Color1);
  if (color_1_t)
  {
    settings.Colors[0] = GColorFromHEX(color_1_t->value->int32);
  }

  Tuple *color_2_t = dict_find(iter, MESSAGE_KEY_Color2);
  if (color_2_t)
  {
    settings.Colors[1] = GColorFromHEX(color_2_t->value->int32);
  }

  Tuple *color_3_t = dict_find(iter, MESSAGE_KEY_Color3);
  if (color_3_t)
  {
    settings.Colors[2] = GColorFromHEX(color_3_t->value->int32);
  }

  Tuple *color_4_t = dict_find(iter, MESSAGE_KEY_Color4);
  if (color_4_t)
  {
    settings.Colors[3] = GColorFromHEX(color_4_t->value->int32);
  }

  if (color_1_t || color_2_t || color_3_t || color_4_t)
  {
    settings_changed = true;
    redraw_all_digits = true; // recolor digits now, not only when they change
    update_time();
  }

  if (settings_changed)
    persist_write_data(SETTINGS_KEY, &settings, sizeof(settings));

  Tuple *temperature_t = dict_find(iter, MESSAGE_KEY_Temperature);
  if (temperature_t)
  {
    show_temperature((int)temperature_t->value->int32);
    persist_int_if_changed(WEATHER_TEMP_KEY, temperature_t->value->int32);
  }

  Tuple *conditions_t = dict_find(iter, MESSAGE_KEY_Conditions);
  if (conditions_t)
  {
    show_conditions((int)conditions_t->value->int32);
    persist_int_if_changed(WEATHER_COND_KEY, conditions_t->value->int32);
  }
}

static void inbox_dropped_callback(AppMessageResult reason, void *context)
{
  APP_LOG(APP_LOG_LEVEL_ERROR, "Message dropped!");
}

static void outbox_failed_callback(DictionaryIterator *iterator, AppMessageResult reason, void *context)
{
  APP_LOG(APP_LOG_LEVEL_ERROR, "Outbox send failed!");
}

static void outbox_sent_callback(DictionaryIterator *iterator, void *context)
{
  APP_LOG(APP_LOG_LEVEL_INFO, "Outbox send success!");
}

static void init()
{
  load_settings();

  // Create main Window element and assign to pointer
  s_main_window = window_create();

  // Set handlers to manage the elements inside the Window
  window_set_window_handlers(s_main_window, (WindowHandlers){
                                                .load = main_window_load,
                                                .unload = main_window_unload});

  // Show the Window on the watch, with animated=true
  window_stack_push(s_main_window, true);
  restore_weather();

  // Register with TickTimerService
  // MINUTE_UNIT: wake once a minute; the :59 slide-out uses a one-shot AppTimer
  tick_timer_service_subscribe(MINUTE_UNIT, tick_minutos);
  schedule_slide_out();

  // Subscribe to AccelerometerService
  accel_tap_service_subscribe(accel_tap_handler);

  app_message_register_inbox_received(in_received_handler);

  // Register for Bluetooth connection updates
  connection_service_subscribe((ConnectionHandlers){
      .pebble_app_connection_handler = bluetooth_callback});

  // Register callbacks
  app_message_register_inbox_dropped(inbox_dropped_callback);
  app_message_register_outbox_failed(outbox_failed_callback);
  app_message_register_outbox_sent(outbox_sent_callback);

  // Largest inbox message is the Clay settings dict (~90 bytes); the only
  // outbox message is a 1-byte weather request.
  app_message_open(256, 32);
}

static void deinit()
{
  tick_timer_service_unsubscribe();
  if (slide_out_timer)
    app_timer_cancel(slide_out_timer);
  // Stop any animation in progress
  animation_unschedule_all();
  // Destroy Window
  window_destroy(s_main_window);
}

int main(void)
{
  init();
  app_event_loop();
  deinit();
}