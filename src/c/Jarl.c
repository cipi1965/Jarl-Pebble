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

const int DAYS_IDS[7] = {
    RESOURCE_ID_IMAGE_MONDAY, RESOURCE_ID_IMAGE_TUESDAY, RESOURCE_ID_IMAGE_WEDNESDAY,
    RESOURCE_ID_IMAGE_THURSDAY, RESOURCE_ID_IMAGE_FRIDAY, RESOURCE_ID_IMAGE_SATURDAY,
    RESOURCE_ID_IMAGE_SUNDAY};

const int MININUMBERS_IDS[10] = {
    RESOURCE_ID_IMAGE_0_MINI, RESOURCE_ID_IMAGE_1_MINI, RESOURCE_ID_IMAGE_2_MINI,
    RESOURCE_ID_IMAGE_3_MINI, RESOURCE_ID_IMAGE_4_MINI, RESOURCE_ID_IMAGE_5_MINI,
    RESOURCE_ID_IMAGE_6_MINI, RESOURCE_ID_IMAGE_7_MINI, RESOURCE_ID_IMAGE_8_MINI,
    RESOURCE_ID_IMAGE_9_MINI};

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

static GBitmap *bitmap_temp_sign, *bitmap_temp_dec, *bitmap_temp_unit, *bitmap_temp_degrees;
static BitmapLayer *layer_temp_sign, *layer_temp_dec, *layer_temp_unit, *layer_temp_degrees;

static uint8_t battery_level;

static GBitmap *bitmap_date_day, *bitmap_date_dec, *bitmap_date_unit, *battery_bitmap;
static BitmapLayer *layer_date_day, *layer_date_dec, *layer_date_unit, *battery_layer;

static BitmapLayer *conditions_layer;
static GBitmap *conditions_bitmap;

static bool on_animation = false;
static bool animation_numbers_started = false;
static bool start = true;

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
  char bufferday[] = "0";
  char bufferdayn[] = "01";
  // Get a tm structure
  time_t temp = time(NULL);
  struct tm *tick_time = localtime(&temp);

  strftime(bufferday, sizeof("0"), "%u", tick_time);
  strftime(bufferdayn, sizeof("01"), "%d", tick_time);

  // dias de la semana
  gbitmap_destroy(bitmap_date_day);
  int i = bufferday[0] - '0';
  bitmap_date_day = gbitmap_create_with_resource(DAYS_IDS[i - 1]);
  bitmap_layer_set_bitmap(layer_date_day, bitmap_date_day);

  // cifras del día del mes
  //  unidad
  gbitmap_destroy(bitmap_date_unit);
  int u = bufferdayn[1] - '0';
  bitmap_date_unit = gbitmap_create_with_resource(MININUMBERS_IDS[u]);
  bitmap_layer_set_bitmap(layer_date_unit, bitmap_date_unit);
  // decena
  gbitmap_destroy(bitmap_date_dec);
  int d = bufferdayn[0] - '0';
  bitmap_date_dec = gbitmap_create_with_resource(MININUMBERS_IDS[d]);
  bitmap_layer_set_bitmap(layer_date_dec, bitmap_date_dec);
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

// void update_temp(void *data) {
void update_temp()
{
  //  vibes_short_pulse();
  DictionaryIterator *iter;
  app_message_outbox_begin(&iter);
  // Add a key-value pair
  dict_write_uint8(iter, 0, 0);
  // Send the message!
  app_message_outbox_send();
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Asked temperature update");
}

static void update_time_component(/* inout */ struct tc_status* status, char buffer, int number)
{
#ifdef PBL_COLOR
  if ((status->current_char != buffer) | (start == true))
  {
    status->color_idx = rand() % 4;
  }
  gbitmap_destroy(status->bitmap);
  status->bitmap = gbitmap_create_with_resource(NUMBERS_IDS[number]);
  replace_gbitmap_color(GColorWhite, settings.Colors[status->color_idx], status->bitmap, NULL);
  bitmap_layer_set_bitmap(status->layer, status->bitmap);

  status->current_char = buffer;

#else
  gbitmap_destroy(status->bitmap);
  status->bitmap = gbitmap_create_with_resource(NUMBERS_IDS[number]);
  bitmap_layer_set_bitmap(status->layer, status->bitmap);
#endif
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

  int h0 = buffer[1] - '0';
  update_time_component(&hours_unit, buffer[1], h0);

  int m1 = buffer[3] - '0';
  update_time_component(&minutes_dec, buffer[3], m1);

  int m0 = buffer[4] - '0';
  update_time_component(&minutes_unit, buffer[4], m0);

  start = false;
  // APP_LOG(APP_LOG_LEVEL_INFO, "Update");
}

void timer_callback(void *data)
{
  // animate temperature
  int first_temp_symbol_x = ((WIDTH / 2) - 39 - 21 - 6) / 2;
  animate_layer(bitmap_layer_get_layer(layer_temp_sign), &GRect(first_temp_symbol_x, ANIM_LENGTH - VER_OFFSET + 12, 13, 17), &GRect(first_temp_symbol_x, -VER_OFFSET + 12, 13, 17), ANIM_DURATION_UP, ANIM_DELAY);
  animate_layer(bitmap_layer_get_layer(layer_temp_dec), &GRect(first_temp_symbol_x + 13 + 2, 12, 13, 17), &GRect(first_temp_symbol_x + 13 + 2, -VER_OFFSET + 12, 13, 17), ANIM_DURATION_UP, ANIM_DELAY);
  animate_layer(bitmap_layer_get_layer(layer_temp_unit), &GRect(first_temp_symbol_x + 26 + 4, 12, 13, 17), &GRect(first_temp_symbol_x + 26 + 4, -VER_OFFSET + 12, 13, 17), ANIM_DURATION_UP, ANIM_DELAY);
  animate_layer(bitmap_layer_get_layer(layer_temp_degrees), &GRect(first_temp_symbol_x + 39 + 6, 12, 21, 17), &GRect(first_temp_symbol_x + 39 + 6, -VER_OFFSET + 12, 21, 17), ANIM_DURATION_UP, ANIM_DELAY);

  // animate date
  int date_day_x = (WIDTH / 2 - 43) / 2;
  animate_layer(bitmap_layer_get_layer(layer_date_day), &GRect(date_day_x, ANIM_LENGTH + 6 - 42, 43, 17), &GRect(date_day_x, 6 - 42, 43, 17), ANIM_DURATION_UP, ANIM_DELAY);
  int first_day_number_x = (WIDTH / 2) + ((WIDTH / 2) - 26) / 2;
  animate_layer(bitmap_layer_get_layer(layer_date_dec), &GRect(first_day_number_x, ANIM_LENGTH + 6 - 42, 13, 17), &GRect(first_day_number_x, 6 - 42, 13, 17), ANIM_DURATION_UP, ANIM_DELAY);
  animate_layer(bitmap_layer_get_layer(layer_date_unit), &GRect(first_day_number_x + 13 + 1, ANIM_LENGTH + 6 - 42, 13, 17), &GRect(first_day_number_x + 13 + 1, 6 - 42, 13, 17), ANIM_DURATION_UP, ANIM_DELAY);

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
    int first_temp_symbol_x = ((WIDTH / 2) - 39 - 21 - 6) / 2;
    animate_layer(
      bitmap_layer_get_layer(layer_temp_sign),
      &GRect(first_temp_symbol_x, -VER_OFFSET + 12, 13, 17),
      &GRect(first_temp_symbol_x, 12, 13, 17),
      ANIM_DURATION_DOWN, ANIM_DELAY
    );
    animate_layer(
      bitmap_layer_get_layer(layer_temp_dec),
      &GRect(first_temp_symbol_x + 13 + 2, -VER_OFFSET + 12, 13, 17),
      &GRect(first_temp_symbol_x + 13 + 2, 12, 13, 17),
      ANIM_DURATION_DOWN, ANIM_DELAY
    );
    animate_layer(
      bitmap_layer_get_layer(layer_temp_unit),
      &GRect(first_temp_symbol_x + 26 + 4, -VER_OFFSET + 12, 13, 17),
      &GRect(first_temp_symbol_x + 26 + 4, 12, 13, 17),
      ANIM_DURATION_DOWN, ANIM_DELAY
    );
    animate_layer(
      bitmap_layer_get_layer(layer_temp_degrees),
      &GRect(first_temp_symbol_x + 39 + 6, -ANIM_LENGTH + 12, 21, 17),
      &GRect(first_temp_symbol_x + 39 + 6, 12, 21, 17),
      ANIM_DURATION_DOWN, ANIM_DELAY
    );

    // animate date
    int date_day_x = (WIDTH / 2 - 43) / 2;
    animate_layer(
      bitmap_layer_get_layer(layer_date_day),
      &GRect(date_day_x, 6 - 42, 43, 17),
      &GRect(date_day_x, ANIM_LENGTH + 6 - 42, 43, 17),
      ANIM_DURATION_DOWN, ANIM_DELAY
    );
    int first_day_number_x = (WIDTH / 2) + ((WIDTH / 2) - 26) / 2;
    animate_layer(
      bitmap_layer_get_layer(layer_date_dec),
      &GRect(first_day_number_x, 6 - 42, 13, 17),
      &GRect(first_day_number_x, ANIM_LENGTH + 6 - 42, 13, 17),
      ANIM_DURATION_DOWN, ANIM_DELAY
    );
    animate_layer(
      bitmap_layer_get_layer(layer_date_unit),
      &GRect(first_day_number_x + 13 + 1, 6 - 42, 13, 17),
      &GRect(first_day_number_x + 13 + 1, ANIM_LENGTH + 6 - 42, 13, 17),
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

static void main_window_load(Window *window)
{
  srand(time(NULL));

  // Load bitmaps into GBitmap structures
  // troll = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_TROLL);

  // números
  hours_dec.layer = bitmap_layer_create(GRect(0, -84, 72, 84));
  layer_add_child(window_get_root_layer(window), bitmap_layer_get_layer(hours_dec.layer));
  hours_unit.layer = bitmap_layer_create(GRect(144, 0, 72, 84));
  layer_add_child(window_get_root_layer(window), bitmap_layer_get_layer(hours_unit.layer));
  minutes_dec.layer = bitmap_layer_create(GRect(-73, 84, 72, 84));
  layer_add_child(window_get_root_layer(window), bitmap_layer_get_layer(minutes_dec.layer));
  minutes_unit.layer = bitmap_layer_create(GRect(73, 168, 72, 84));
  layer_add_child(window_get_root_layer(window), bitmap_layer_get_layer(minutes_unit.layer));

  // Create temperature  layers
  layer_temp_sign = bitmap_layer_create(GRect(3, -84 + 12, 13, 17));
  layer_temp_dec = bitmap_layer_create(GRect(18, -84 + 12, 13, 17));
  layer_temp_unit = bitmap_layer_create(GRect(33, -84 + 12, 13, 17));
  layer_temp_degrees = bitmap_layer_create(GRect(48, -84 + 12, 21, 17));
  bitmap_temp_sign = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_MINUS_MINI);
  bitmap_temp_dec = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_MINUS_MINI);
  bitmap_temp_unit = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_MINUS_MINI);
  bitmap_temp_degrees = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_MINUS_MINI);
  bitmap_layer_set_bitmap(layer_temp_sign, bitmap_temp_sign);
  bitmap_layer_set_bitmap(layer_temp_dec, bitmap_temp_dec);
  bitmap_layer_set_bitmap(layer_temp_unit, bitmap_temp_unit);
  bitmap_layer_set_bitmap(layer_temp_degrees, bitmap_temp_degrees);

  layer_add_child(window_get_root_layer(window), bitmap_layer_get_layer(layer_temp_sign));
  layer_add_child(window_get_root_layer(window), bitmap_layer_get_layer(layer_temp_dec));
  layer_add_child(window_get_root_layer(window), bitmap_layer_get_layer(layer_temp_unit));
  layer_add_child(window_get_root_layer(window), bitmap_layer_get_layer(layer_temp_degrees));

  // Create date layers
  layer_date_day = bitmap_layer_create(GRect(25, 6 - 42, 43, 17));
  layer_date_dec = bitmap_layer_create(GRect(91, 6 - 42, 13, 17));
  layer_date_unit = bitmap_layer_create(GRect(105, 6 - 42, 13, 17));

  layer_add_child(window_get_root_layer(window), bitmap_layer_get_layer(layer_date_day));
  layer_add_child(window_get_root_layer(window), bitmap_layer_get_layer(layer_date_dec));
  layer_add_child(window_get_root_layer(window), bitmap_layer_get_layer(layer_date_unit));

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

static void tick_segundos(struct tm *tick_time, TimeUnits units_changed)
{
  // vibes_short_pulse();
  // Format the buffer string using tick_time as the time source
  // APP_LOG(APP_LOG_LEVEL_DEBUG, "(free: %d, used: %d)", heap_bytes_free(), heap_bytes_used());

#ifdef DEBUG_MEMORY
  static char buf[] = "1234512345";
  snprintf(buf, sizeof(buf), "%u %u", heap_bytes_used(), heap_bytes_free());
  text_layer_set_text(memory_layer, buf);
#endif

  static char buffer[] = "00:00";
  strftime(buffer, sizeof("00:00"), "%H:%M", tick_time);

  int seconds = tick_time->tm_sec;
  int minutes = tick_time->tm_min;
  int hours = tick_time->tm_hour;

  // a las horas en punto y a y media
  if ((seconds == 0) && (minutes == 00))
  {
    update_temp();
    // actualizamos batería también
    update_battery();
    //
  }

  // a las horas en punto y a y media
  if ((seconds == 0) && (minutes == 30))
  {
    update_temp();
    // actualizamos batería también
    update_battery();
    //
  }

  // a las cero horas
  if ((seconds == 0) && (minutes == 0) && (hours == 00))
  {
    update_date();
  }

  // a mitad de minuto aleatorizamos
  if (seconds == 30)
  {
    rand_minutes_unit = rand() % 2;
    rand_minutes_dec = rand() % 2;
    rand_hours_unit = rand() % 2;
    rand_hours_dec = rand() % 2;
  }

  if (seconds == 0)
    update_time();
  //  if(seconds == 2) update_time();  // prueba para evitar los blancos

  if (settings.AnimateNumbers)
  {
    // el número sale de la pantalla
    if (seconds == 59)
    {
      // Slide
      if (on_animation == false)
      { // no se está mostrando la fecha
        //  on_animation = true;
        //  animation_numbers_started =  true;
        timer3 = app_timer_register(TOTAL_ANIM_DIGIT_DURATION, timer_callback3, NULL);
        // animate_layer(bitmap_layer_get_layer(minute_layer_unit), &GRect(73, 84, 72, 84), &GRect(144 - rand_minutes_unit * 73, 84 + 84 * rand_minutes_unit, 72, 84), ANIM_DURATION_DIGITS, 0);
        animate_layer(bitmap_layer_get_layer(minutes_unit.layer), &GRect(HOR_OFFSET, VER_OFFSET, HOR_OFFSET, VER_OFFSET), &GRect(WIDTH - rand_minutes_unit * HOR_OFFSET, VER_OFFSET + VER_OFFSET * rand_minutes_unit, HOR_OFFSET, VER_OFFSET), ANIM_DURATION_DIGITS, 0);

        if ((minutes == 9) | (minutes == 19) | (minutes == 29) | (minutes == 39) | (minutes == 49) | (minutes == 59))
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
    }

    // el número entra
    if (seconds == 0)
    {
      // Slide
      // update_time();
      //   if (animation_numbers_started == true){
      if (on_animation == false)
      { // no se está mostrando la fecha
        animate_layer(bitmap_layer_get_layer(minutes_unit.layer), &GRect(WIDTH - rand_minutes_unit * HOR_OFFSET, VER_OFFSET + VER_OFFSET * rand_minutes_unit, HOR_OFFSET, VER_OFFSET), &GRect(HOR_OFFSET, VER_OFFSET, HOR_OFFSET, VER_OFFSET), ANIM_DURATION_DIGITS, 0);
        if ((minutes == 10) | (minutes == 20) | (minutes == 30) | (minutes == 40) | (minutes == 50) | (minutes == 0))
        {
          // animate_layer(bitmap_layer_get_layer(minute_layer_dec), &GRect(-73 + rand_minutes_dec * 73,  84 + rand_minutes_dec * 84, 72, 84), &GRect(0, 84 , 72, 84), ANIM_DURATION_DIGITS, 0);
          animate_layer(bitmap_layer_get_layer(minutes_dec.layer), &GRect(-HOR_OFFSET + rand_minutes_dec * HOR_OFFSET, VER_OFFSET + rand_minutes_dec * VER_OFFSET, HOR_OFFSET, VER_OFFSET), &GRect(X_ORIGIN, VER_OFFSET, HOR_OFFSET, VER_OFFSET), ANIM_DURATION_DIGITS, 0);
        }
        if (minutes == 0)
        {
          animate_layer(bitmap_layer_get_layer(hours_unit.layer), &GRect(WIDTH - HOR_OFFSET * rand_hours_unit, -VER_OFFSET * rand_hours_unit, HOR_OFFSET, VER_OFFSET), &GRect(HOR_OFFSET, Y_ORIGIN, HOR_OFFSET, VER_OFFSET), ANIM_DURATION_DIGITS, 0);
          if ((hours == 0) | (hours == 20) | (hours == 10))
          {
            animate_layer(bitmap_layer_get_layer(hours_dec.layer), &GRect(-HOR_OFFSET + rand_hours_dec * HOR_OFFSET, -VER_OFFSET * rand_hours_dec, HOR_OFFSET, VER_OFFSET), &GRect(X_ORIGIN, Y_ORIGIN, HOR_OFFSET, VER_OFFSET), ANIM_DURATION_DIGITS, 0);
          }
          //  }
          //  animation_numbers_started=false;
        }
      }
    }
  }
}

static void main_window_unload(Window *window)
{
  //  gbitmap_destroy(troll);

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

  // Destroy temperature layers
  bitmap_layer_destroy(layer_temp_sign);
  bitmap_layer_destroy(layer_temp_dec);
  bitmap_layer_destroy(layer_temp_unit);
  bitmap_layer_destroy(layer_temp_degrees);

  // Destroy tempeatura bitmaps
  gbitmap_destroy(bitmap_temp_sign);
  gbitmap_destroy(bitmap_temp_dec);
  gbitmap_destroy(bitmap_temp_unit);
  gbitmap_destroy(bitmap_temp_degrees);

  // Destroy date bitmaps
  gbitmap_destroy(bitmap_date_day);
  gbitmap_destroy(bitmap_date_dec);
  gbitmap_destroy(bitmap_date_unit);
  // Destroy date layers
  bitmap_layer_destroy(layer_date_day);
  bitmap_layer_destroy(layer_date_dec);
  bitmap_layer_destroy(layer_date_unit);

  // battery
  bitmap_layer_destroy(battery_layer);
  gbitmap_destroy(battery_bitmap);
}

static void in_received_handler(DictionaryIterator *iter, void *context)
{
  Tuple *temp_units_t = dict_find(iter, MESSAGE_KEY_TempUnits);
  if (temp_units_t)
  {
    settings.UseCelsius = strcmp(temp_units_t->value->cstring, "c") == 0;
    update_temp();
  }

  Tuple *animate_numbers_t = dict_find(iter, MESSAGE_KEY_AnimateNumbers);
  if (animate_numbers_t)
  {
    settings.AnimateNumbers = animate_numbers_t->value->int32 > 0;
  }

  Tuple *vibrate_on_connect_t = dict_find(iter, MESSAGE_KEY_VibrateOnConnect);
  if (vibrate_on_connect_t)
  {
    settings.VibrateOnConnect = vibrate_on_connect_t->value->int32 > 0;
  }

  Tuple *vibrate_on_disconnect_t = dict_find(iter, MESSAGE_KEY_VibrateOnDisconnect);
  if (vibrate_on_disconnect_t)
  {
    settings.VibrateOnDisconnect = vibrate_on_disconnect_t->value->int32 > 0;
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

  persist_write_data(SETTINGS_KEY, &settings, sizeof(settings));

  Tuple *temperature_t = dict_find(iter, MESSAGE_KEY_Temperature);
  if (temperature_t)
  {
    char temperature_buffer[4];
    int temp = (int)temperature_t->value->int32;
    if (settings.UseCelsius == false)
    {
      temp = (1.8 * temp) + 32;
    }

    snprintf(temperature_buffer, sizeof(temperature_buffer), "%+03d", temp);
    // signo de la temperatura
    gbitmap_destroy(bitmap_temp_sign);
    if (temperature_buffer[0] == '+')
    {
      bitmap_temp_sign = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_PLUS_MINI);
    }
    if (temperature_buffer[0] == '-')
    {
      bitmap_temp_sign = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_MINUS_MINI);
    }
    bitmap_layer_set_bitmap(layer_temp_sign, bitmap_temp_sign);
    // decenas de la temperatura
    gbitmap_destroy(bitmap_temp_dec);
    if (temperature_buffer[1] == '0')
    {
      bitmap_temp_dec = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_0_MINI);
    }
    if (temperature_buffer[1] == '1')
    {
      bitmap_temp_dec = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_1_MINI);
    }
    if (temperature_buffer[1] == '2')
    {
      bitmap_temp_dec = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_2_MINI);
    }
    if (temperature_buffer[1] == '3')
    {
      bitmap_temp_dec = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_3_MINI);
    }
    if (temperature_buffer[1] == '4')
    {
      bitmap_temp_dec = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_4_MINI);
    }
    if (temperature_buffer[1] == '5')
    {
      bitmap_temp_dec = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_5_MINI);
    }
    if (temperature_buffer[1] == '6')
    {
      bitmap_temp_dec = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_6_MINI);
    }
    if (temperature_buffer[1] == '7')
    {
      bitmap_temp_dec = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_7_MINI);
    }
    if (temperature_buffer[1] == '8')
    {
      bitmap_temp_dec = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_8_MINI);
    }
    if (temperature_buffer[1] == '9')
    {
      bitmap_temp_dec = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_9_MINI);
    }
    bitmap_layer_set_bitmap(layer_temp_dec, bitmap_temp_dec);
    // unidades de la temperatura
    gbitmap_destroy(bitmap_temp_unit);
    if (temperature_buffer[2] == '0')
    {
      bitmap_temp_unit = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_0_MINI);
    }
    if (temperature_buffer[2] == '1')
    {
      bitmap_temp_unit = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_1_MINI);
    }
    if (temperature_buffer[2] == '2')
    {
      bitmap_temp_unit = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_2_MINI);
    }
    if (temperature_buffer[2] == '3')
    {
      bitmap_temp_unit = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_3_MINI);
    }
    if (temperature_buffer[2] == '4')
    {
      bitmap_temp_unit = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_4_MINI);
    }
    if (temperature_buffer[2] == '5')
    {
      bitmap_temp_unit = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_5_MINI);
    }
    if (temperature_buffer[2] == '6')
    {
      bitmap_temp_unit = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_6_MINI);
    }
    if (temperature_buffer[2] == '7')
    {
      bitmap_temp_unit = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_7_MINI);
    }
    if (temperature_buffer[2] == '8')
    {
      bitmap_temp_unit = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_8_MINI);
    }
    if (temperature_buffer[2] == '9')
    {
      bitmap_temp_unit = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_9_MINI);
    }
    bitmap_layer_set_bitmap(layer_temp_unit, bitmap_temp_unit);
    // grados

    gbitmap_destroy(bitmap_temp_degrees);
    if (settings.UseCelsius == false)
    {
      bitmap_temp_degrees = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_F_MINI);
    }
    else
    {
      bitmap_temp_degrees = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_C_MINI);
    }

    bitmap_layer_set_bitmap(layer_temp_degrees, bitmap_temp_degrees);
  }

  Tuple *conditions_t = dict_find(iter, MESSAGE_KEY_Conditions);
  if (conditions_t)
  {
    int32_t condition = conditions_t->value->int32;

    gbitmap_destroy(conditions_bitmap);
    conditions_bitmap = gbitmap_create_with_resource(WEATHER_IDS[condition]);
    bitmap_layer_set_bitmap(conditions_layer, conditions_bitmap);
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

  // Register with TickTimerService
  tick_timer_service_subscribe(SECOND_UNIT, tick_segundos);

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

  app_message_open(784, 784);
}

static void deinit()
{
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