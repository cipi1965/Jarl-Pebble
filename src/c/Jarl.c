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
#define WEATHER_EXTRA_KEY 103

#define WIDTH PBL_DISPLAY_WIDTH
#define HEIGHT PBL_DISPLAY_HEIGHT
#define X_ORIGIN 0
#define Y_ORIGIN 0
#define HOR_OFFSET PBL_DISPLAY_WIDTH / 2
#define VER_OFFSET PBL_DISPLAY_HEIGHT / 2

// #define DEBUG_MEMORY

// What a spot of the info panel shows. The numbers are the values of the
// Slot settings in src/pkjs/config.js, so don't renumber them.
enum Complication
{
  C_NONE = 0,
  C_WEATHER_ICON = 1, // top row only: the icon is 42 px tall
  C_TEMPERATURE = 2,
  C_DAY_NAME = 3,
  C_DAY_NUMBER = 4,
  C_MONTH = 5,
  C_WEEK = 6,
  C_DAY_OF_YEAR = 7,
  C_BATTERY = 8,
  C_STEPS = 9,
  C_DISTANCE = 10,
  C_HEART_RATE = 11,
  C_TEMP_HIGH = 12,
  C_TEMP_LOW = 13,
  C_HUMIDITY = 14,
  C_RAIN_CHANCE = 15,
  C_SUNRISE = 16,
  C_SUNSET = 17,
  C_COUNT
};

// Panel spots: top left, top right, middle left, middle right
#define SLOT_COUNT 4

typedef struct ClaySettings
{
  bool UseCelsius;
  bool AnimateNumbers;
  bool VibrateOnConnect;
  bool VibrateOnDisconnect;
  GColor Colors[4];
  // appended last so settings saved by older versions still load
  uint8_t WeatherInterval; // minutes between weather updates
  uint8_t Slots[SLOT_COUNT]; // enum Complication for each panel spot
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

// Day and month names drawn with the Jarl Mini font (fixed English, like the
// old day bitmaps; not taken from the locale, so only font glyphs are used)
static const char *const DAY_NAMES[7] = {"SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"};
static const char *const MONTH_NAMES[12] = {"JAN", "FEB", "MAR", "APR", "MAY", "JUN",
                                            "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"};

// Icon glyphs of the Jarl Mini font (tools/jarl_mini_glyphs.txt)
#define ICON_HEART "♥"
#define ICON_DROP "\U0001F4A7"
#define ICON_UMBRELLA "☂"
#define ICON_UP "▲"
#define ICON_DOWN "▼"

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
// Pebble Time 2 (200x228) gets the larger 18x24 variant.
#if PBL_DISPLAY_WIDTH >= 200
#define MINI_FONT RESOURCE_ID_FONT_JARL_MINI_BIG_24
#define MINI_GW 18   /* glyph width */
#define MINI_H 24    /* glyph height */
#define MINI_GAP 3   /* space after each glyph */
#define MINI_DEG_W 8 /* degree sign width */
#define DATE_Y 46    /* centred between weather icon and battery bar */
#else
#define MINI_FONT RESOURCE_ID_FONT_JARL_MINI_17
#define MINI_GW 13
#define MINI_H 17
#define MINI_GAP 2
#define MINI_DEG_W 6
#define DATE_Y 48
#endif
#define ICON_H 42 /* weather icon height = height of the top panel row */
static GFont mini_font;

// One layer per panel spot, each half the screen wide. It draws its text
// centred, or the weather icon.
static Layer *slot_layers[SLOT_COUNT];
static char slot_text[SLOT_COUNT][16];

static uint8_t battery_level = 255;

static GBitmap *battery_bitmap;
static BitmapLayer *battery_layer;

static GBitmap *conditions_bitmap;

// Weather from the phone; extra fields are unknown until the first update
// from a version that sends them.
#define NO_VALUE INT16_MIN
typedef struct WeatherExtra
{
  int16_t high, low;       // Celsius
  int16_t humidity, rain;  // percent
  int16_t sunrise, sunset; // minutes after local midnight
} WeatherExtra;
static WeatherExtra weather_extra = {NO_VALUE, NO_VALUE, NO_VALUE, NO_VALUE, NO_VALUE, NO_VALUE};
static int last_celsius;
static bool have_temperature = false;

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

  // the panel as it was before the spots became configurable
  settings.Slots[0] = C_TEMPERATURE;
  settings.Slots[1] = C_WEATHER_ICON;
  settings.Slots[2] = C_DAY_NAME;
  settings.Slots[3] = C_DAY_NUMBER;
}

static bool slot_uses_weather(uint8_t c)
{
  return c == C_WEATHER_ICON || c == C_TEMPERATURE || (c >= C_TEMP_HIGH && c <= C_SUNSET);
}

static bool panel_uses_weather()
{
  for (int i = 0; i < SLOT_COUNT; i++)
    if (slot_uses_weather(settings.Slots[i]))
      return true;
  return false;
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

static void update_battery()
{
  BatteryChargeState state = battery_state_service_peek();
  if (state.charge_percent == battery_level)
    return; // same bitmap; skip reloading it
  battery_level = state.charge_percent;
  int level = battery_level / 10;
  if (level == 10)
    level = 9;
  gbitmap_destroy(battery_bitmap);
  battery_bitmap = gbitmap_create_with_resource(BATTERY_RESOURCE_IDS[level]);
  bitmap_layer_set_bitmap(battery_layer, battery_bitmap);
}

// ISO 8601 week number (weeks start on Monday, week 1 holds the first Thursday)
static int iso_weeks_in_year(int year)
{
  int p = (year + year / 4 - year / 100 + year / 400) % 7;
  int y = year - 1;
  int q = (y + y / 4 - y / 100 + y / 400) % 7;
  return (p == 4 || q == 3) ? 53 : 52;
}

static int iso_week(const struct tm *t)
{
  int weekday = t->tm_wday == 0 ? 7 : t->tm_wday; // Monday = 1 ... Sunday = 7
  int year = t->tm_year + 1900;
  int week = (t->tm_yday + 1 - weekday + 10) / 7;
  if (week < 1)
    return iso_weeks_in_year(year - 1);
  if (week > iso_weeks_in_year(year))
    return 1;
  return week;
}

// Temperature in the display unit; the phone always sends Celsius.
static int display_temp(int celsius)
{
  if (settings.UseCelsius)
    return celsius;
  // integer math (no soft-float library); F*10 = C*18 + 320, rounded
  int f10 = celsius * 18 + 320;
  return (f10 >= 0 ? f10 + 5 : f10 - 5) / 10;
}

static int clamp(int v, int lo, int hi)
{
  return v < lo ? lo : v > hi ? hi : v;
}

static void format_clock_time(char *buf, size_t size, int minutes)
{
  int h = minutes / 60, m = minutes % 60;
  if (clock_is_24h_style())
  {
    snprintf(buf, size, "%02d:%02d", h, m);
    return;
  }
  h %= 12;
  snprintf(buf, size, "%d:%02d", h == 0 ? 12 : h, m);
}

#if defined(PBL_HEALTH)
static bool health_available(HealthMetric metric, time_t start, time_t end)
{
  return health_service_metric_accessible(metric, start, end) & HealthServiceAccessibilityMaskAvailable;
}

static void format_steps(char *buf, size_t size)
{
  time_t now = time(NULL);
  if (!health_available(HealthMetricStepCount, time_start_of_today(), now))
  {
    snprintf(buf, size, "--");
    return;
  }
  int steps = (int)health_service_sum_today(HealthMetricStepCount);
  if (steps < 10000)
    snprintf(buf, size, "%d", steps); // "8421"
  else if (steps < 100000)
    snprintf(buf, size, "%d.%dK", steps / 1000, steps / 100 % 10); // "12.3K"
  else
    snprintf(buf, size, "%dK", steps / 1000);
}

static void format_distance(char *buf, size_t size)
{
  time_t now = time(NULL);
  if (!health_available(HealthMetricWalkedDistanceMeters, time_start_of_today(), now))
  {
    snprintf(buf, size, "--");
    return;
  }
  int meters = (int)health_service_sum_today(HealthMetricWalkedDistanceMeters);
  bool imperial = health_service_get_measurement_system_for_display(HealthMetricWalkedDistanceMeters) ==
                  MeasurementSystemImperial;
  // tenths of a km or mile
  int tenths = imperial ? (int)((int64_t)meters * 10 * 1000 / 1609344) : meters / 100;
  const char *unit = imperial ? "MI" : "KM";
  if (tenths < 100)
    snprintf(buf, size, "%d.%d%s", tenths / 10, tenths % 10, unit); // "3.2KM"
  else
    snprintf(buf, size, "%d%s", clamp(tenths / 10, 0, 99), unit); // "12KM"
}

static void format_heart_rate(char *buf, size_t size)
{
  time_t now = time(NULL);
  int bpm = 0;
  if (health_available(HealthMetricHeartRateBPM, now, now))
    bpm = (int)health_service_peek_current_value(HealthMetricHeartRateBPM);
  if (bpm > 0)
    snprintf(buf, size, ICON_HEART "%d", bpm);
  else
    snprintf(buf, size, ICON_HEART "--");
}
#endif

static void format_complication(uint8_t c, char *buf, size_t size, const struct tm *t)
{
  buf[0] = '\0';
  switch (c)
  {
  case C_TEMPERATURE:
    if (!have_temperature)
    {
      snprintf(buf, size, "----");
      break;
    }
    {
      // sign, two digits and unit: only two digits fit next to the unit
      int temp = clamp(display_temp(last_celsius), -99, 99);
      snprintf(buf, size, "%c%02d°%c", temp < 0 ? '-' : '+', temp < 0 ? -temp : temp,
               settings.UseCelsius ? 'C' : 'F');
    }
    break;
  case C_DAY_NAME:
    snprintf(buf, size, "%s", DAY_NAMES[t->tm_wday]);
    break;
  case C_DAY_NUMBER:
    snprintf(buf, size, "%02d", t->tm_mday);
    break;
  case C_MONTH:
    snprintf(buf, size, "%s", MONTH_NAMES[t->tm_mon]);
    break;
  case C_WEEK:
    snprintf(buf, size, "W%02d", iso_week(t));
    break;
  case C_DAY_OF_YEAR:
    snprintf(buf, size, "D%03d", t->tm_yday + 1);
    break;
  case C_BATTERY:
    snprintf(buf, size, "%d%%", battery_state_service_peek().charge_percent);
    break;
#if defined(PBL_HEALTH)
  case C_STEPS:
    format_steps(buf, size);
    break;
  case C_DISTANCE:
    format_distance(buf, size);
    break;
  case C_HEART_RATE:
    format_heart_rate(buf, size);
    break;
#else
  case C_STEPS:
  case C_DISTANCE:
    snprintf(buf, size, "--");
    break;
  case C_HEART_RATE:
    snprintf(buf, size, ICON_HEART "--");
    break;
#endif
  case C_TEMP_HIGH:
  case C_TEMP_LOW:
  {
    int16_t celsius = c == C_TEMP_HIGH ? weather_extra.high : weather_extra.low;
    const char *icon = c == C_TEMP_HIGH ? ICON_UP : ICON_DOWN;
    if (celsius == NO_VALUE)
      snprintf(buf, size, "%s--", icon);
    else
      snprintf(buf, size, "%s%d°", icon, clamp(display_temp(celsius), -99, 199));
    break;
  }
  case C_HUMIDITY:
  case C_RAIN_CHANCE:
  {
    int16_t percent = c == C_HUMIDITY ? weather_extra.humidity : weather_extra.rain;
    const char *icon = c == C_HUMIDITY ? ICON_DROP : ICON_UMBRELLA;
    if (percent == NO_VALUE)
      snprintf(buf, size, "%s--", icon);
    else
      snprintf(buf, size, "%s%d%%", icon, clamp(percent, 0, 100));
    break;
  }
  case C_SUNRISE:
  case C_SUNSET:
  {
    int16_t minutes = c == C_SUNRISE ? weather_extra.sunrise : weather_extra.sunset;
    if (minutes == NO_VALUE)
      snprintf(buf, size, "--:--");
    else
      format_clock_time(buf, size, minutes);
    break;
  }
  default: // C_NONE, C_WEATHER_ICON (drawn as a bitmap)
    break;
  }
}

// Recompute the text of every panel spot. Runs when the panel slides in and
// when its data changes, so nothing is read while the panel is hidden.
static void refresh_panel()
{
  time_t now = time(NULL);
  struct tm *t = localtime(&now);
  for (int i = 0; i < SLOT_COUNT; i++)
  {
    format_complication(settings.Slots[i], slot_text[i], sizeof(slot_text[i]), t);
    layer_mark_dirty(slot_layers[i]);
  }
}

static void slot_update_proc(Layer *layer, GContext *ctx)
{
  int i = *(int *)layer_get_data(layer);
  GRect bounds = layer_get_bounds(layer);

  if (settings.Slots[i] == C_WEATHER_ICON)
  {
    if (conditions_bitmap)
    {
      GSize size = gbitmap_get_bounds(conditions_bitmap).size;
      graphics_draw_bitmap_in_rect(ctx, conditions_bitmap,
                                   GRect((bounds.size.w - size.w) / 2, (bounds.size.h - size.h) / 2,
                                         size.w, size.h));
    }
    return;
  }

  const char *text = slot_text[i];
  if (!text[0])
    return;
  // The text size includes the gap after the last glyph: leave it out when
  // centring, and keep it in the box or the last glyph is dropped.
  GSize size = graphics_text_layout_get_content_size(text, mini_font, GRect(0, 0, 2 * WIDTH, MINI_H),
                                                     GTextOverflowModeFill, GTextAlignmentLeft);
  int x = (bounds.size.w - (size.w - MINI_GAP)) / 2;
  int y = (bounds.size.h - MINI_H) / 2;
  graphics_context_set_text_color(ctx, GColorWhite);
  graphics_draw_text(ctx, text, mini_font, GRect(x, y, size.w + MINI_GAP, MINI_H),
                     GTextOverflowModeFill, GTextAlignmentLeft, NULL);
}

// Frame of panel spot i, on screen or hidden above it
static GRect slot_frame(int i, bool shown)
{
  int x = (i % 2) * HOR_OFFSET;
  if (i < 2)
    return GRect(x, shown ? 0 : -VER_OFFSET, HOR_OFFSET, ICON_H);
  return GRect(x, shown ? DATE_Y : DATE_Y - ANIM_LENGTH, HOR_OFFSET, MINI_H);
}

void update_temp()
{
  if (!panel_uses_weather())
    return; // no spot shows weather: don't ask the phone for it

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
  // animate panel spots
  for (int i = 0; i < SLOT_COUNT; i++)
  {
    GRect from = slot_frame(i, true), to = slot_frame(i, false);
    animate_layer(slot_layers[i], &from, &to, ANIM_DURATION_UP, ANIM_DELAY);
  }

  // animate battery
  animate_layer(bitmap_layer_get_layer(battery_layer), &GRect((WIDTH - 144) / 2, -10 + ANIM_LENGTH, 144, 6), &GRect((WIDTH - 144) / 2, -10, 144, 6), ANIM_DURATION_UP, ANIM_DELAY);

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

    // fresh steps, heart rate, battery and date for the panel
    update_battery();
    refresh_panel();

    // animate panel spots
    for (int i = 0; i < SLOT_COUNT; i++)
    {
      GRect from = slot_frame(i, false), to = slot_frame(i, true);
      animate_layer(slot_layers[i], &from, &to, ANIM_DURATION_DOWN, ANIM_DELAY);
    }

    // animate battery
    animate_layer(
      bitmap_layer_get_layer(battery_layer),
      &GRect((WIDTH - 144) / 2, -10, 144, 6),
      &GRect((WIDTH - 144) / 2, -10 + ANIM_LENGTH, 144, 6),
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

  // Info panel spots (hidden above the screen until the panel slides in)
  mini_font = fonts_load_custom_font(resource_get_handle(MINI_FONT));
  for (int i = 0; i < SLOT_COUNT; i++)
  {
    slot_layers[i] = layer_create_with_data(slot_frame(i, false), sizeof(int));
    *(int *)layer_get_data(slot_layers[i]) = i;
    layer_set_update_proc(slot_layers[i], slot_update_proc);
    layer_add_child(window_get_root_layer(window), slot_layers[i]);
  }

  // create battery layers
  battery_layer = bitmap_layer_create(GRect(0, -10, 144, 6));
  layer_add_child(window_get_root_layer(window), bitmap_layer_get_layer(battery_layer));

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
  refresh_panel();

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
    refresh_panel();
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

  // Destroy info panel spots
  for (int i = 0; i < SLOT_COUNT; i++)
    layer_destroy(slot_layers[i]);
  fonts_unload_custom_font(mini_font);

  // battery
  bitmap_layer_destroy(battery_layer);
  gbitmap_destroy(battery_bitmap);
}

static void show_temperature(int celsius)
{
  last_celsius = celsius;
  have_temperature = true;
}

static void show_conditions(int condition)
{
  // ignore codes we have no icon for instead of reading past WEATHER_IDS
  if (condition < 0 || condition >= (int)ARRAY_LENGTH(WEATHER_IDS))
  {
    APP_LOG(APP_LOG_LEVEL_WARNING, "Unknown weather condition %d", condition);
    return;
  }
  gbitmap_destroy(conditions_bitmap);
  conditions_bitmap = gbitmap_create_with_resource(WEATHER_IDS[condition]);
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
  persist_read_data(WEATHER_EXTRA_KEY, &weather_extra, sizeof(weather_extra));
  refresh_panel();
}

// Clay sends select values as strings
static int tuple_int(const Tuple *t)
{
  return t->type == TUPLE_CSTRING ? atoi(t->value->cstring) : (int)t->value->int32;
}

// Read one extra weather field; the phone sends -1000 when it has no value.
static bool read_weather_field(DictionaryIterator *iter, uint32_t key, int16_t *field)
{
  Tuple *t = dict_find(iter, key);
  if (!t)
    return false;
  int value = (int)t->value->int32;
  *field = value <= -1000 ? NO_VALUE : value;
  return true;
}

static void in_received_handler(DictionaryIterator *iter, void *context)
{
  // Only write to flash when settings actually arrived (not on weather updates)
  bool settings_changed = false;

  Tuple *temp_units_t = dict_find(iter, MESSAGE_KEY_TempUnits);
  if (temp_units_t)
  {
    settings.UseCelsius = strcmp(temp_units_t->value->cstring, "c") == 0;
    settings_changed = true; // the panel redraws in the new unit below
  }

  bool had_weather = panel_uses_weather();
  const uint32_t keys[SLOT_COUNT] = {MESSAGE_KEY_Slot1, MESSAGE_KEY_Slot2, MESSAGE_KEY_Slot3, MESSAGE_KEY_Slot4};
  for (int i = 0; i < SLOT_COUNT; i++)
  {
    Tuple *slot_t = dict_find(iter, keys[i]);
    if (!slot_t)
      continue;
    int c = tuple_int(slot_t);
    // the weather icon only fits the top row
    if (c < 0 || c >= C_COUNT || (c == C_WEATHER_ICON && i >= 2))
      continue;
    settings.Slots[i] = c;
    settings_changed = true;
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
    int minutes = tuple_int(weather_interval_t);
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

  WeatherExtra extra = weather_extra;
  bool got_extra = read_weather_field(iter, MESSAGE_KEY_TempHigh, &extra.high);
  got_extra |= read_weather_field(iter, MESSAGE_KEY_TempLow, &extra.low);
  got_extra |= read_weather_field(iter, MESSAGE_KEY_Humidity, &extra.humidity);
  got_extra |= read_weather_field(iter, MESSAGE_KEY_RainChance, &extra.rain);
  got_extra |= read_weather_field(iter, MESSAGE_KEY_Sunrise, &extra.sunrise);
  got_extra |= read_weather_field(iter, MESSAGE_KEY_Sunset, &extra.sunset);
  if (got_extra && memcmp(&extra, &weather_extra, sizeof(extra)) != 0)
  {
    weather_extra = extra;
    persist_write_data(WEATHER_EXTRA_KEY, &weather_extra, sizeof(weather_extra));
  }

  // a spot now shows weather: fetch it instead of waiting for the next update
  if (!had_weather && panel_uses_weather())
    update_temp();

  refresh_panel();
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