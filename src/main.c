#include <pebble.h>
#include <ctype.h>
#define ANIM_DURATION_DOWN 1000
#define ANIM_DURATION_UP 700
#define ANIM_DELAY 300
#define DATE_ON_SCREEN_DURATION 4000
#define TOTAL_ANIM_DURATION 6000
#define ANIM_LENGTH 84
#define ANIM_DURATION_DIGITS 1000
#define	TOTAL_ANIM_DIGIT_DURATION 2000

#define WIDTH 144
#define HEIGHT 168
#define X_ORIGIN 0
#define Y_ORIGIN 0
#define HOR_OFFSET 72
#define VER_OFFSET 84

#define KEY_TEMPERATURE 2
#define KEY_CONDITIONS 3
#define KEY_TEMPUNITS 4
#define KEY_ANIMATE_NUMBERS 5
#define KEY_BT_CONNECT 6
#define KEY_BT_DISCONNECT 7

//#define DEBUG_MEMORY

TextLayer *memory_layer;
	
const int BATTERY_RESOURCE_IDS[10] = {
  RESOURCE_ID_IMAGE_BATTERY_10, RESOURCE_ID_IMAGE_BATTERY_20, RESOURCE_ID_IMAGE_BATTERY_30,
  RESOURCE_ID_IMAGE_BATTERY_40, RESOURCE_ID_IMAGE_BATTERY_50, RESOURCE_ID_IMAGE_BATTERY_60,
  RESOURCE_ID_IMAGE_BATTERY_70, RESOURCE_ID_IMAGE_BATTERY_80, RESOURCE_ID_IMAGE_BATTERY_90,
  RESOURCE_ID_IMAGE_BATTERY_100
};

const int DAYS_IDS[7] = {
  RESOURCE_ID_IMAGE_MONDAY, RESOURCE_ID_IMAGE_TUESDAY, RESOURCE_ID_IMAGE_WEDNESDAY,
  RESOURCE_ID_IMAGE_THURSDAY, RESOURCE_ID_IMAGE_FRIDAY, RESOURCE_ID_IMAGE_SATURDAY,
  RESOURCE_ID_IMAGE_SUNDAY
};

const int MININUMBERS_IDS[10] = {
  RESOURCE_ID_IMAGE_0_MINI, RESOURCE_ID_IMAGE_1_MINI, RESOURCE_ID_IMAGE_2_MINI,
  RESOURCE_ID_IMAGE_3_MINI, RESOURCE_ID_IMAGE_4_MINI, RESOURCE_ID_IMAGE_5_MINI,
	RESOURCE_ID_IMAGE_6_MINI, RESOURCE_ID_IMAGE_7_MINI, RESOURCE_ID_IMAGE_8_MINI,
  RESOURCE_ID_IMAGE_9_MINI
};

const int RED_NUMBERS_IDS[10] = {
  RESOURCE_ID_IMAGE_0_RED, RESOURCE_ID_IMAGE_1_RED, RESOURCE_ID_IMAGE_2_RED,
  RESOURCE_ID_IMAGE_3_RED, RESOURCE_ID_IMAGE_4_RED, RESOURCE_ID_IMAGE_5_RED,
	RESOURCE_ID_IMAGE_6_RED, RESOURCE_ID_IMAGE_7_RED, RESOURCE_ID_IMAGE_8_RED,
  RESOURCE_ID_IMAGE_9_RED
};

const int YELLOW_NUMBERS_IDS[10] = {
  RESOURCE_ID_IMAGE_0_YELLOW, RESOURCE_ID_IMAGE_1_YELLOW, RESOURCE_ID_IMAGE_2_YELLOW,
  RESOURCE_ID_IMAGE_3_YELLOW, RESOURCE_ID_IMAGE_4_YELLOW, RESOURCE_ID_IMAGE_5_YELLOW,
	RESOURCE_ID_IMAGE_6_YELLOW, RESOURCE_ID_IMAGE_7_YELLOW, RESOURCE_ID_IMAGE_8_YELLOW,
  RESOURCE_ID_IMAGE_9_YELLOW
};

const int BLUE_NUMBERS_IDS[10] = {
  RESOURCE_ID_IMAGE_0_BLUE, RESOURCE_ID_IMAGE_1_BLUE, RESOURCE_ID_IMAGE_2_BLUE,
  RESOURCE_ID_IMAGE_3_BLUE, RESOURCE_ID_IMAGE_4_BLUE, RESOURCE_ID_IMAGE_5_BLUE,
	RESOURCE_ID_IMAGE_6_BLUE, RESOURCE_ID_IMAGE_7_BLUE, RESOURCE_ID_IMAGE_8_BLUE,
  RESOURCE_ID_IMAGE_9_BLUE
};

const int GREEN_NUMBERS_IDS[10] = {
  RESOURCE_ID_IMAGE_0_GREEN, RESOURCE_ID_IMAGE_1_GREEN, RESOURCE_ID_IMAGE_2_GREEN,
  RESOURCE_ID_IMAGE_3_GREEN, RESOURCE_ID_IMAGE_4_GREEN, RESOURCE_ID_IMAGE_5_GREEN,
	RESOURCE_ID_IMAGE_6_GREEN, RESOURCE_ID_IMAGE_7_GREEN, RESOURCE_ID_IMAGE_8_GREEN,
  RESOURCE_ID_IMAGE_9_GREEN
};


static Window *s_main_window;



static GBitmap *hour_bitmap_dec, *hour_bitmap_unit, *minute_bitmap_dec, *minute_bitmap_unit;
static GBitmap *bitmap_temp_sign, *bitmap_temp_dec, *bitmap_temp_unit, *bitmap_temp_degrees;
static BitmapLayer *layer_temp_sign, *layer_temp_dec, *layer_temp_unit, *layer_temp_degrees;

static uint8_t battery_level;

static GBitmap *bitmap_date_day, *bitmap_date_dec, *bitmap_date_unit, *battery_bitmap;
static BitmapLayer *layer_date_day, *layer_date_dec, *layer_date_unit, *battery_layer;


static BitmapLayer *hour_layer_dec, *hour_layer_unit, *minute_layer_dec, *minute_layer_unit, *conditions_layer;
static GBitmap *conditions_bitmap;



static bool on_animation = false;
static bool animation_numbers_started = false;
static bool show_celsius = true;
static bool start = true;

static bool animatenumbers;
static bool btconnect;
static bool btdisconnect;

static int rand_minutes_unit = 0;
static int rand_minutes_dec = 0;
static int rand_hours_unit = 0;
static int rand_hours_dec = 0;

static char minutes_unit = 'a';
static char minutes_dec = 'a';
static char hours_unit = 'a';
static char hours_dec = 'a'; 

static int color1;
static int color2;
static int color3;
static int color4;


//temporizadores
AppTimer *timer;
AppTimer *timer2;
AppTimer *timer3;



void on_animation_stopped(Animation *anim, bool finished, void *context)
{
    //Free the memoery used by the Animation
	 // vibes_short_pulse();
    property_animation_destroy((PropertyAnimation*) anim);
}
 
void animate_layer(Layer *layer, GRect *start, GRect *finish, int duration, int delay)
{
    //Declare animation
    PropertyAnimation *anim = property_animation_create_layer_frame(layer, start, finish);
 
    //Set characteristics
    animation_set_duration((Animation*) anim, duration);
    animation_set_delay((Animation*) anim, delay);
 
    //Set stopped handler to free memory
    AnimationHandlers handlers = {
        //The reference to the stopped handler is the only one in the array
        .stopped = (AnimationStoppedHandler) on_animation_stopped
    };
    animation_set_handlers((Animation*) anim, handlers, NULL);
 
    //Start animation!
    animation_schedule((Animation*) anim);
}






static void bluetooth_callback(bool connected) {
  // Show icon if disconnected
 // layer_set_hidden(bitmap_layer_get_layer(s_bt_icon_layer), connected);
  if(!connected) {
    // Issue a vibrating alert
    if (btdisconnect) vibes_double_pulse();
  } else {
		if (btconnect) vibes_short_pulse();
	}
}


static void update_date () {
	char bufferday[]= "0";
	char bufferdayn[]= "01";
	 // Get a tm structure
  time_t temp = time(NULL); 
  struct tm *tick_time = localtime(&temp);
	
	strftime(bufferday, sizeof("0"), "%u", tick_time);
	strftime(bufferdayn, sizeof("01"), "%d", tick_time);
	
	// dias de la semana
	gbitmap_destroy(bitmap_date_day);
	int i = bufferday[0]  - '0'; 
	bitmap_date_day = gbitmap_create_with_resource(DAYS_IDS[i-1]);
	bitmap_layer_set_bitmap(layer_date_day, bitmap_date_day);	
	
	//cifras del día del mes
	// unidad
	gbitmap_destroy(bitmap_date_unit);
	int u = bufferdayn[1]  - '0'; 
	bitmap_date_unit = gbitmap_create_with_resource(MININUMBERS_IDS[u]);
	bitmap_layer_set_bitmap(layer_date_unit, bitmap_date_unit);
	//decena
	gbitmap_destroy(bitmap_date_dec);
	int d = bufferdayn[0]  - '0'; 
	bitmap_date_dec = gbitmap_create_with_resource(MININUMBERS_IDS[d]);
	bitmap_layer_set_bitmap(layer_date_dec, bitmap_date_dec);

}



static void update_battery() {
	//battery update
	int level;
	BatteryChargeState initial = battery_state_service_peek();
  battery_level = initial.charge_percent;	
	gbitmap_destroy(battery_bitmap);	
	level = battery_level/10;
	if (level == 10) level = 9; 
	battery_bitmap = gbitmap_create_with_resource(BATTERY_RESOURCE_IDS[level]);
	bitmap_layer_set_bitmap(battery_layer, battery_bitmap);
	

}


//void update_temp(void *data) {
void update_temp() {
	//	vibes_short_pulse();
		DictionaryIterator *iter;
		app_message_outbox_begin(&iter);
		// Add a key-value pair
		dict_write_uint8(iter, 0, 0);
		// Send the message!
		app_message_outbox_send();
	//	APP_LOG(APP_LOG_LEVEL_DEBUG, "Temperatura solicitada"); 
}


static void update_time() {
	

	
	
	
  // Get a tm structure
  time_t temp = time(NULL); 
  struct tm *tick_time = localtime(&temp);
	char buffer[]= "00:00";
	
  if(clock_is_24h_style() == true) {
    // Use 24 hour format
    strftime(buffer, sizeof("00:00"), "%H:%M", tick_time);
  } else {
    // Use 12 hour format
    strftime(buffer, sizeof("00:00"), "%I:%M", tick_time);
  }
	

	//horas y minutos
	
	#ifdef PBL_PLATFORM_BASALT
		int seconds = tick_time->tm_sec;
		int minutes = tick_time->tm_min;
		int hours = tick_time->tm_hour;
	#endif
	
	int h1 = buffer[0]  - '0'; 
	#ifdef PBL_PLATFORM_BASALT
  //	if  (((seconds == 0) && (minutes==0) && ((hours==0) | (hours==20) | (hours==10))) | (start == true)) {
		if ((hours_dec != buffer[0]) | (start == true)) {
			color1=rand()%4;
		}
			gbitmap_destroy(hour_bitmap_dec);
			switch(color1)
			{
				case 0:
					hour_bitmap_dec = gbitmap_create_with_resource(RED_NUMBERS_IDS[h1]);
				break;
				case 1:
					hour_bitmap_dec = gbitmap_create_with_resource(YELLOW_NUMBERS_IDS[h1]);
				break;
				case 2:
					hour_bitmap_dec = gbitmap_create_with_resource(BLUE_NUMBERS_IDS[h1]);
				break;
				case 3:
					hour_bitmap_dec = gbitmap_create_with_resource(GREEN_NUMBERS_IDS[h1]);
				break;
			}
			bitmap_layer_set_bitmap(hour_layer_dec, hour_bitmap_dec);
		//	bitmap_layer_set_compositing_mode(hour_layer_dec, GCompOpSet);
			hours_dec = buffer[0];
		
	#else
			gbitmap_destroy(hour_bitmap_dec);
			hour_bitmap_dec = gbitmap_create_with_resource(RED_NUMBERS_IDS[h1]);
			bitmap_layer_set_bitmap(hour_layer_dec, hour_bitmap_dec);	
	#endif
	layer_mark_dirty(bitmap_layer_get_layer(hour_layer_dec));
	
	int h0 = buffer[1]  - '0'; 	
	#ifdef PBL_PLATFORM_BASALT	
   //	if (((seconds == 0) && (minutes==0)) | (start == true)) {
		if ((hours_unit != buffer[1]) | (start == true)) {
			color2=rand()%4;
		}
			gbitmap_destroy(hour_bitmap_unit);
			switch(color2)
			{
				case 0:
					hour_bitmap_unit = gbitmap_create_with_resource(RED_NUMBERS_IDS[h0]);
				break;
				case 1:
					hour_bitmap_unit = gbitmap_create_with_resource(YELLOW_NUMBERS_IDS[h0]);
				break;
				case 2:
					hour_bitmap_unit = gbitmap_create_with_resource(BLUE_NUMBERS_IDS[h0]);
				break;
				case 3:
					hour_bitmap_unit = gbitmap_create_with_resource(GREEN_NUMBERS_IDS[h0]);
				break;
			}
			bitmap_layer_set_bitmap(hour_layer_unit, hour_bitmap_unit);
		//	bitmap_layer_set_compositing_mode(hour_layer_unit, GCompOpSet);
			hours_unit = buffer[1];
		
	#else
		gbitmap_destroy(hour_bitmap_unit);
		hour_bitmap_unit = gbitmap_create_with_resource(RED_NUMBERS_IDS[h0]);
		bitmap_layer_set_bitmap(hour_layer_unit, hour_bitmap_unit);
	#endif			
	layer_mark_dirty(bitmap_layer_get_layer(hour_layer_unit));		
	
	int m1 = buffer[3]  - '0'; 
	#ifdef PBL_PLATFORM_BASALT
   //	if (((seconds == 0) && ((minutes == 10) | (minutes== 20) | (minutes==30) | (minutes == 40) | (minutes== 50) | (minutes==0))) | (start == true)) {
		if ((minutes_dec != buffer[3]) | (start == true)) {
			color3=rand()%4;
		}
			gbitmap_destroy(minute_bitmap_dec);
			switch(color3)
			{
				case 0:
					minute_bitmap_dec = gbitmap_create_with_resource(RED_NUMBERS_IDS[m1]);
				break;
				case 1:
					minute_bitmap_dec = gbitmap_create_with_resource(YELLOW_NUMBERS_IDS[m1]);
				break;
				case 2:
					minute_bitmap_dec = gbitmap_create_with_resource(BLUE_NUMBERS_IDS[m1]);
				break;
				case 3:
					minute_bitmap_dec = gbitmap_create_with_resource(GREEN_NUMBERS_IDS[m1]);
				break;
			}
			bitmap_layer_set_bitmap(minute_layer_dec, minute_bitmap_dec);
		//	bitmap_layer_set_compositing_mode(minute_layer_dec, GCompOpSet);
			minutes_dec = buffer[3];
		
	#else
		gbitmap_destroy(minute_bitmap_dec);
		minute_bitmap_dec = gbitmap_create_with_resource(RED_NUMBERS_IDS[m1]);
		bitmap_layer_set_bitmap(minute_layer_dec, minute_bitmap_dec);
	#endif
	layer_mark_dirty(bitmap_layer_get_layer(minute_layer_dec));	
	
	int m0 = buffer[4]  - '0'; 
	#ifdef PBL_PLATFORM_BASALT
	//	if ((seconds == 0) | (start == true)) {
		if ((minutes_unit != buffer[4]) | (start == true)) {
			color4=rand()%4;
		}
			gbitmap_destroy(minute_bitmap_unit);
			switch(color4)
			{
				case 0:
					minute_bitmap_unit = gbitmap_create_with_resource(RED_NUMBERS_IDS[m0]);
				break;
				case 1:
					minute_bitmap_unit = gbitmap_create_with_resource(YELLOW_NUMBERS_IDS[m0]);
				break;
				case 2:
					minute_bitmap_unit = gbitmap_create_with_resource(BLUE_NUMBERS_IDS[m0]);
				break;
				case 3:
					minute_bitmap_unit = gbitmap_create_with_resource(GREEN_NUMBERS_IDS[m0]);
				break;

			}
			bitmap_layer_set_bitmap(minute_layer_unit, minute_bitmap_unit);
	//		bitmap_layer_set_compositing_mode(minute_layer_unit, GCompOpSet);
			minutes_unit = buffer[4];
		
	#else
		gbitmap_destroy(minute_bitmap_unit);
		minute_bitmap_unit = gbitmap_create_with_resource(RED_NUMBERS_IDS[m0]);
		bitmap_layer_set_bitmap(minute_layer_unit, minute_bitmap_unit);
	#endif		
	layer_mark_dirty(bitmap_layer_get_layer(minute_layer_unit));
			 
	start = false;
	//APP_LOG(APP_LOG_LEVEL_INFO, "Update");
}



void timer_callback(void *data) {
	
		// animar temperatura
		animate_layer(bitmap_layer_get_layer(layer_temp_sign), &GRect(3, ANIM_LENGTH -VER_OFFSET + 12, 13, 17), &GRect(3, -VER_OFFSET + 12, 13, 17), ANIM_DURATION_UP, ANIM_DELAY);
		animate_layer(bitmap_layer_get_layer(layer_temp_dec), &GRect(18, ANIM_LENGTH -VER_OFFSET + 12, 13, 17), &GRect(18, -VER_OFFSET + 12, 13, 17), ANIM_DURATION_UP, ANIM_DELAY);
		animate_layer(bitmap_layer_get_layer(layer_temp_unit), &GRect(33, ANIM_LENGTH -VER_OFFSET + 12, 13, 17), &GRect(33, -VER_OFFSET + 12, 13, 17), ANIM_DURATION_UP, ANIM_DELAY);
		animate_layer(bitmap_layer_get_layer(layer_temp_degrees), &GRect(48, ANIM_LENGTH -VER_OFFSET + 12, 21, 17), &GRect(48, -VER_OFFSET + 12, 21, 17), ANIM_DURATION_UP, ANIM_DELAY);
	
	  //animar date
		animate_layer(bitmap_layer_get_layer(layer_date_day), &GRect(25, ANIM_LENGTH + 6 - 42, 43, 17), &GRect(25,  6 - 42, 43, 17), ANIM_DURATION_UP, ANIM_DELAY);
		animate_layer(bitmap_layer_get_layer(layer_date_dec), &GRect(91, ANIM_LENGTH + 6 - 42, 13, 17), &GRect(91, 6 - 42, 13, 17), ANIM_DURATION_UP, ANIM_DELAY);
		animate_layer(bitmap_layer_get_layer(layer_date_unit), &GRect(105, ANIM_LENGTH + 6 - 42, 13, 17), &GRect(105, 6 - 42, 13, 17), ANIM_DURATION_UP, ANIM_DELAY);
		
		//animar bateria
		animate_layer(bitmap_layer_get_layer(battery_layer), &GRect(0, -10 + ANIM_LENGTH, 144, 6), &GRect(0, -10, 144, 6), ANIM_DURATION_UP, ANIM_DELAY);
	
	  //animar conditions
	  animate_layer(bitmap_layer_get_layer(conditions_layer), &GRect(HOR_OFFSET, ANIM_LENGTH - VER_OFFSET, HOR_OFFSET, 42), &GRect(HOR_OFFSET, -VER_OFFSET, HOR_OFFSET, 42), ANIM_DURATION_UP, ANIM_DELAY);
	
		//animar números	
		animate_layer(bitmap_layer_get_layer(hour_layer_dec), &GRect(X_ORIGIN, ANIM_LENGTH, HOR_OFFSET, VER_OFFSET), &GRect(X_ORIGIN, Y_ORIGIN, HOR_OFFSET, VER_OFFSET), ANIM_DURATION_UP, ANIM_DELAY);
		animate_layer(bitmap_layer_get_layer(hour_layer_unit), &GRect(HOR_OFFSET, ANIM_LENGTH, HOR_OFFSET, VER_OFFSET), &GRect(HOR_OFFSET, Y_ORIGIN, HOR_OFFSET, VER_OFFSET), ANIM_DURATION_UP, ANIM_DELAY);
		animate_layer(bitmap_layer_get_layer(minute_layer_dec), &GRect(X_ORIGIN, VER_OFFSET + ANIM_LENGTH , HOR_OFFSET, VER_OFFSET), &GRect(X_ORIGIN, VER_OFFSET , HOR_OFFSET, VER_OFFSET), ANIM_DURATION_UP, ANIM_DELAY);
		animate_layer(bitmap_layer_get_layer(minute_layer_unit), &GRect(HOR_OFFSET, VER_OFFSET + ANIM_LENGTH, HOR_OFFSET, VER_OFFSET), &GRect(HOR_OFFSET, VER_OFFSET, HOR_OFFSET, VER_OFFSET), ANIM_DURATION_UP, ANIM_DELAY);
	
}


void timer_callback2(void *data) {
		on_animation = false;
}

static void accel_tap_handler(AccelAxisType axis, int32_t direction)
{
	//update_time();
	
//	APP_LOG(APP_LOG_LEVEL_INFO, "Axis: %d", axis);
	
	if ((on_animation == false) && (axis == 1)) {
		on_animation = true;
		
		// animar temperatura
		animate_layer(bitmap_layer_get_layer(layer_temp_sign), &GRect(3, -VER_OFFSET + 12, 13, 17), &GRect(3, ANIM_LENGTH -VER_OFFSET + 12, 13, 17), ANIM_DURATION_DOWN, ANIM_DELAY);
		animate_layer(bitmap_layer_get_layer(layer_temp_dec), &GRect(18, -VER_OFFSET + 12, 13, 17), &GRect(18, ANIM_LENGTH -VER_OFFSET + 12, 13, 17), ANIM_DURATION_DOWN, ANIM_DELAY);
		animate_layer(bitmap_layer_get_layer(layer_temp_unit), &GRect(33, -VER_OFFSET + 12, 13, 17), &GRect(33, ANIM_LENGTH -VER_OFFSET + 12, 13, 17), ANIM_DURATION_DOWN, ANIM_DELAY);
		animate_layer(bitmap_layer_get_layer(layer_temp_degrees), &GRect(48, -VER_OFFSET + 12, 21, 17), &GRect(48, ANIM_LENGTH -VER_OFFSET + 12, 21, 17), ANIM_DURATION_DOWN, ANIM_DELAY);
		
	  // animar date
		animate_layer(bitmap_layer_get_layer(layer_date_day), &GRect(25,  6 - 42, 43, 17), &GRect(25, ANIM_LENGTH + 6 - 42, 43, 17), ANIM_DURATION_DOWN, ANIM_DELAY);
		animate_layer(bitmap_layer_get_layer(layer_date_dec), &GRect(91, 6 - 42, 13, 17), &GRect(91, ANIM_LENGTH + 6 - 42, 13, 17), ANIM_DURATION_DOWN, ANIM_DELAY);
		animate_layer(bitmap_layer_get_layer(layer_date_unit), &GRect(105, 6 - 42, 13, 17), &GRect(105, ANIM_LENGTH + 6 - 42, 13, 17), ANIM_DURATION_DOWN, ANIM_DELAY);
		
		//animar bateria
		animate_layer(bitmap_layer_get_layer(battery_layer), &GRect(0, -10, 144, 6), &GRect(0, -10 + ANIM_LENGTH, 144, 6), ANIM_DURATION_DOWN, ANIM_DELAY);
		
		//animar condiciones
		animate_layer(bitmap_layer_get_layer(conditions_layer), &GRect(HOR_OFFSET, -VER_OFFSET, HOR_OFFSET, 42), &GRect(HOR_OFFSET, ANIM_LENGTH - VER_OFFSET, HOR_OFFSET, 42), ANIM_DURATION_DOWN, ANIM_DELAY);
		
		//animar números
		animate_layer(bitmap_layer_get_layer(hour_layer_dec), &GRect(X_ORIGIN, Y_ORIGIN, HOR_OFFSET, VER_OFFSET), &GRect(X_ORIGIN, ANIM_LENGTH, HOR_OFFSET, VER_OFFSET), ANIM_DURATION_DOWN, ANIM_DELAY);
		animate_layer(bitmap_layer_get_layer(hour_layer_unit), &GRect(HOR_OFFSET, Y_ORIGIN, HOR_OFFSET, VER_OFFSET), &GRect(HOR_OFFSET, ANIM_LENGTH, HOR_OFFSET, VER_OFFSET), ANIM_DURATION_DOWN, ANIM_DELAY);
		animate_layer(bitmap_layer_get_layer(minute_layer_dec), &GRect(X_ORIGIN, VER_OFFSET , HOR_OFFSET, VER_OFFSET), &GRect(X_ORIGIN, VER_OFFSET + ANIM_LENGTH , HOR_OFFSET, VER_OFFSET), ANIM_DURATION_DOWN, ANIM_DELAY);
		animate_layer(bitmap_layer_get_layer(minute_layer_unit), &GRect(HOR_OFFSET, VER_OFFSET, HOR_OFFSET, VER_OFFSET), &GRect(HOR_OFFSET, VER_OFFSET + ANIM_LENGTH, HOR_OFFSET, VER_OFFSET), ANIM_DURATION_DOWN, ANIM_DELAY);	
	
		//suscribirse a temporizadores 
		timer = app_timer_register(DATE_ON_SCREEN_DURATION, (AppTimerCallback) timer_callback, NULL);
		timer2 = app_timer_register(TOTAL_ANIM_DURATION, (AppTimerCallback) timer_callback2, NULL);
	}
	else
	{
		//app_timer_reschedule(timer, 0);
		
	}
}



void timer_callback3(void *data) {
		on_animation = false;
}



static void main_window_load(Window *window) {
	srand(time(NULL));

	
  //Load bitmaps into GBitmap structures
	//troll = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_TROLL);
		
	//números
	hour_layer_dec = bitmap_layer_create(GRect(0, -84, 72, 84));
	layer_add_child(window_get_root_layer(window), bitmap_layer_get_layer(hour_layer_dec));	
	hour_layer_unit = bitmap_layer_create(GRect(144, 0, 72, 84));
	layer_add_child(window_get_root_layer(window), bitmap_layer_get_layer(hour_layer_unit));	
	minute_layer_dec = bitmap_layer_create(GRect(-73, 84, 72, 84));
	layer_add_child(window_get_root_layer(window), bitmap_layer_get_layer(minute_layer_dec));	
	minute_layer_unit = bitmap_layer_create(GRect(73, 168, 72, 84));
	layer_add_child(window_get_root_layer(window), bitmap_layer_get_layer(minute_layer_unit));	

	
	
	//Create temperature  layers
	layer_temp_sign= bitmap_layer_create(GRect(3, -84 + 12, 13, 17));
	layer_temp_dec= bitmap_layer_create(GRect(18, -84 + 12, 13, 17));
	layer_temp_unit= bitmap_layer_create(GRect(33, -84 + 12, 13, 17));
	layer_temp_degrees= bitmap_layer_create(GRect(48, -84 + 12, 21, 17));
	bitmap_temp_sign = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_MENOS_MINI);
	bitmap_temp_dec = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_MENOS_MINI);
	bitmap_temp_unit = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_MENOS_MINI);
	bitmap_temp_degrees = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_MENOS_MINI);
	bitmap_layer_set_bitmap(layer_temp_sign, bitmap_temp_sign);
	bitmap_layer_set_bitmap(layer_temp_dec, bitmap_temp_dec);
	bitmap_layer_set_bitmap(layer_temp_unit, bitmap_temp_unit);
	bitmap_layer_set_bitmap(layer_temp_degrees, bitmap_temp_degrees);
	
	layer_add_child(window_get_root_layer(window), bitmap_layer_get_layer(layer_temp_sign));
	layer_add_child(window_get_root_layer(window), bitmap_layer_get_layer(layer_temp_dec));
	layer_add_child(window_get_root_layer(window), bitmap_layer_get_layer(layer_temp_unit));
	layer_add_child(window_get_root_layer(window), bitmap_layer_get_layer(layer_temp_degrees));
	
	
	//Create date layers
	layer_date_day= bitmap_layer_create(GRect(25,  6 - 42, 43, 17));
	layer_date_dec= bitmap_layer_create(GRect(91, 6 - 42, 13, 17));
	layer_date_unit= bitmap_layer_create(GRect(105, 6 - 42, 13, 17));
	
	layer_add_child(window_get_root_layer(window), bitmap_layer_get_layer(layer_date_day));
	layer_add_child(window_get_root_layer(window), bitmap_layer_get_layer(layer_date_dec));
	layer_add_child(window_get_root_layer(window), bitmap_layer_get_layer(layer_date_unit));

	//create battery layers
	battery_layer= bitmap_layer_create(GRect(0, -10, 144, 6));
	layer_add_child(window_get_root_layer(window), bitmap_layer_get_layer(battery_layer));

	//conditions
	conditions_layer= bitmap_layer_create(GRect(72, -84, 72, 42));
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
	//BORRAR
	
	
	
  window_set_background_color (window, GColorBlack);
	#ifdef PBL_PLATFORM_BASALT
  	window_set_background_color(window, GColorBlack);
	#endif

	// Show the correct state of the BT connection from the start
	//bluetooth_callback(connection_service_peek_pebble_app_connection());
	
	//Check for saved option
  bool celsius = persist_read_bool(KEY_TEMPUNITS);
  //Option-specific setup
  if(celsius == true)
  {
		show_celsius=true;
  }
  else
  {
		show_celsius=false;
  }
	
	
	if (persist_exists(KEY_ANIMATE_NUMBERS)) {
		animatenumbers = persist_read_bool(KEY_ANIMATE_NUMBERS);		
	} else {
		animatenumbers = true;
	}
	
	
	if (persist_exists(KEY_BT_CONNECT)) {
		btconnect = persist_read_bool(KEY_BT_CONNECT);		
	} else {
		btconnect = true;
	}
	
	if (persist_exists(KEY_BT_DISCONNECT)) {
		btdisconnect = persist_read_bool(KEY_BT_DISCONNECT);		
	} else {
		btdisconnect = true;
	}
	
	
	update_battery();
	update_date();
	
	
	rand_minutes_unit = 1;
	rand_minutes_dec = 0;
	rand_hours_unit = 0;
	rand_hours_dec = 1;
	animate_layer(bitmap_layer_get_layer(hour_layer_dec), &GRect(-HOR_OFFSET + rand_hours_dec * HOR_OFFSET, -VER_OFFSET * rand_hours_dec, HOR_OFFSET, VER_OFFSET), &GRect(X_ORIGIN, Y_ORIGIN, HOR_OFFSET, VER_OFFSET), ANIM_DURATION_DIGITS, 0);
	animate_layer(bitmap_layer_get_layer(minute_layer_unit), &GRect(WIDTH - rand_minutes_unit * HOR_OFFSET, VER_OFFSET + VER_OFFSET * rand_minutes_unit, HOR_OFFSET, VER_OFFSET), &GRect(HOR_OFFSET, VER_OFFSET, HOR_OFFSET, VER_OFFSET), ANIM_DURATION_DIGITS, 0);	  
	animate_layer(bitmap_layer_get_layer(minute_layer_dec), &GRect(-HOR_OFFSET + rand_minutes_dec * HOR_OFFSET,  VER_OFFSET + rand_minutes_dec * VER_OFFSET, HOR_OFFSET, VER_OFFSET), &GRect(X_ORIGIN, VER_OFFSET , HOR_OFFSET, VER_OFFSET), ANIM_DURATION_DIGITS, 0);		
	animate_layer(bitmap_layer_get_layer(hour_layer_unit), &GRect(WIDTH - HOR_OFFSET * rand_hours_unit, -VER_OFFSET * rand_hours_unit , HOR_OFFSET, VER_OFFSET), &GRect(HOR_OFFSET, Y_ORIGIN, HOR_OFFSET, VER_OFFSET), ANIM_DURATION_DIGITS, 0);
	
	update_time();	
	
	
}



  

static void tick_segundos(struct tm *tick_time, TimeUnits units_changed) {
	//vibes_short_pulse();
	//Format the buffer string using tick_time as the time source
//	APP_LOG(APP_LOG_LEVEL_DEBUG, "(free: %d, used: %d)", heap_bytes_free(), heap_bytes_used());
	
	
	
	#ifdef DEBUG_MEMORY	
		static char buf[] = "1234512345";
		snprintf(buf, sizeof(buf), "%u %u", heap_bytes_used(), heap_bytes_free());
		text_layer_set_text(memory_layer, buf);
	#endif
	
	
	
	
	
	
	static char buffer[]= "00:00";
	strftime(buffer, sizeof("00:00"), "%H:%M", tick_time);

	int seconds = tick_time->tm_sec;
	int minutes = tick_time->tm_min;
	int hours = tick_time->tm_hour;

	//a las horas en punto y a y media
	if ((seconds==0) && (minutes == 00)) {
		update_temp();
		//actualizamos batería también
		update_battery();
		//
	}
	
		//a las horas en punto y a y media
	if ((seconds==0) && (minutes == 30))  {
		update_temp();
		//actualizamos batería también
		update_battery();
		//
	}	
			
	//a las cero horas
	if ((seconds==0) && (minutes == 0) && (hours == 00)) {
		update_date();
	}
	

	//a mitad de minuto aleatorizamos
	if(seconds == 30)
	{
		rand_minutes_unit = rand()%2;
	  rand_minutes_dec = rand()%2;
	  rand_hours_unit = rand()%2;
	  rand_hours_dec = rand()%2;
	}
	
	
	if(seconds == 0) update_time();
//	if(seconds == 2) update_time();  // prueba para evitar los blancos
	
	if (animatenumbers) {
		//el número sale de la pantalla
		if(seconds == 59)
		{
			//Slide 
			if (on_animation == false) {  //no se está mostrando la fecha
			//	on_animation = true;
			//	animation_numbers_started =  true;
				timer3 = app_timer_register(TOTAL_ANIM_DIGIT_DURATION, (AppTimerCallback) timer_callback3, NULL);
				//animate_layer(bitmap_layer_get_layer(minute_layer_unit), &GRect(73, 84, 72, 84), &GRect(144 - rand_minutes_unit * 73, 84 + 84 * rand_minutes_unit, 72, 84), ANIM_DURATION_DIGITS, 0);
				animate_layer(bitmap_layer_get_layer(minute_layer_unit), &GRect(HOR_OFFSET, VER_OFFSET, HOR_OFFSET, VER_OFFSET), &GRect(WIDTH - rand_minutes_unit * HOR_OFFSET, VER_OFFSET + VER_OFFSET * rand_minutes_unit, HOR_OFFSET, VER_OFFSET), ANIM_DURATION_DIGITS, 0);
				
				if ((minutes == 9) | (minutes== 19) | (minutes==29) | (minutes == 39) | (minutes== 49) | (minutes==59))
				{
					animate_layer(bitmap_layer_get_layer(minute_layer_dec), &GRect(X_ORIGIN, VER_OFFSET , HOR_OFFSET, VER_OFFSET), &GRect(-HOR_OFFSET + rand_minutes_dec * HOR_OFFSET,  VER_OFFSET + rand_minutes_dec * VER_OFFSET, HOR_OFFSET, VER_OFFSET), ANIM_DURATION_DIGITS, 0);
				}
				if (minutes==59)
				{
					animate_layer(bitmap_layer_get_layer(hour_layer_unit), &GRect(HOR_OFFSET, Y_ORIGIN, HOR_OFFSET, VER_OFFSET), &GRect(WIDTH - HOR_OFFSET * rand_hours_unit, -VER_OFFSET * rand_hours_unit , HOR_OFFSET, VER_OFFSET), ANIM_DURATION_DIGITS, 0);
					if ((hours==23) | (hours==19) | (hours==9))
					{
						animate_layer(bitmap_layer_get_layer(hour_layer_dec), &GRect(X_ORIGIN, Y_ORIGIN, HOR_OFFSET, VER_OFFSET), &GRect(-HOR_OFFSET + rand_hours_dec * HOR_OFFSET, -VER_OFFSET * rand_hours_dec, HOR_OFFSET, VER_OFFSET), ANIM_DURATION_DIGITS, 0);
					}
				}
			}
		}


		//el número entra
		if(seconds == 0) 
		{
			//Slide
			//update_time();
		//	if (animation_numbers_started == true){
			if (on_animation == false) {  //no se está mostrando la fecha	
				animate_layer(bitmap_layer_get_layer(minute_layer_unit), &GRect(WIDTH - rand_minutes_unit * HOR_OFFSET, VER_OFFSET + VER_OFFSET * rand_minutes_unit, HOR_OFFSET, VER_OFFSET), &GRect(HOR_OFFSET, VER_OFFSET, HOR_OFFSET, VER_OFFSET), ANIM_DURATION_DIGITS, 0);	  
				if ((minutes == 10) | (minutes== 20) | (minutes==30) | (minutes == 40) | (minutes== 50) | (minutes==0))
				{
				//animate_layer(bitmap_layer_get_layer(minute_layer_dec), &GRect(-73 + rand_minutes_dec * 73,  84 + rand_minutes_dec * 84, 72, 84), &GRect(0, 84 , 72, 84), ANIM_DURATION_DIGITS, 0);
				animate_layer(bitmap_layer_get_layer(minute_layer_dec), &GRect(-HOR_OFFSET + rand_minutes_dec * HOR_OFFSET,  VER_OFFSET + rand_minutes_dec * VER_OFFSET, HOR_OFFSET, VER_OFFSET), &GRect(X_ORIGIN, VER_OFFSET , HOR_OFFSET, VER_OFFSET), ANIM_DURATION_DIGITS, 0);
				
				}
				if (minutes==0)
				{
				animate_layer(bitmap_layer_get_layer(hour_layer_unit), &GRect(WIDTH - HOR_OFFSET * rand_hours_unit, -VER_OFFSET * rand_hours_unit , HOR_OFFSET, VER_OFFSET), &GRect(HOR_OFFSET, Y_ORIGIN, HOR_OFFSET, VER_OFFSET), ANIM_DURATION_DIGITS, 0);
					if ((hours==0) | (hours==20) | (hours==10))
					{
					animate_layer(bitmap_layer_get_layer(hour_layer_dec), &GRect(-HOR_OFFSET + rand_hours_dec * HOR_OFFSET, -VER_OFFSET * rand_hours_dec, HOR_OFFSET, VER_OFFSET), &GRect(X_ORIGIN, Y_ORIGIN, HOR_OFFSET, VER_OFFSET), ANIM_DURATION_DIGITS, 0);
					}
			//  }
			//	animation_numbers_started=false;
					
				}
			}
		}
	
	
	
	}
	
	
	
}



void process_tuple(Tuple *t)
{
	
  //Get key
	//vibes_short_pulse	();	
  char temperature_buffer[4];
  char conditions_buffer[4];
//	static char weather_layer_buffer[32];
  int key = t->key;
	int temp;
  //Decide what to do
  switch(key) {	
		  	
			case KEY_TEMPUNITS:
			//It's the tempunits key
			
				if(strcmp(t->value->cstring, "c") == 0)
				{
					//Set and save
					show_celsius = true;
					persist_write_bool(KEY_TEMPUNITS, true);
				}
				else if(strcmp(t->value->cstring, "f") == 0)
				{
					//Set and save 
					show_celsius = false;
					persist_write_bool(KEY_TEMPUNITS, false);
				}
				update_temp();
      break;

			case KEY_ANIMATE_NUMBERS:
				//animate_numbers
		//		Tuple *animate_numbers_t = t->value;
				if(t->value && t->value->int32 > 0) {
					animatenumbers = true;
					persist_write_bool(KEY_ANIMATE_NUMBERS, true);
				} else {
					animatenumbers = false;
					persist_write_bool(KEY_ANIMATE_NUMBERS, false);
				}
				
			break;
		
		
			case KEY_BT_CONNECT:
				//animate_numbers
		//		Tuple *animate_numbers_t = t->value;
		
	    //	APP_LOG(APP_LOG_LEVEL_INFO, "Connect");
				
		
				if(t->value && t->value->int32 > 0) {
					btconnect = true;
					persist_write_bool(KEY_BT_CONNECT, true);
				//	vibes_short_pulse	();	
				} else {
					btconnect = false;
					persist_write_bool(KEY_BT_CONNECT, false);
					//vibes_short_pulse	();	
				}
				
			break;
		
	
			case KEY_BT_DISCONNECT:
				//animate_numbers
		//		Tuple *animate_numbers_t = t->value;
				if(t->value && t->value->int32 > 0) {
					btdisconnect = true;
					persist_write_bool(KEY_BT_DISCONNECT, true);
				} else {
					btdisconnect = false;
					persist_write_bool(KEY_BT_DISCONNECT, false);
				}
				
			break;
		
	  	case KEY_TEMPERATURE: 
				temp = (int)t->value->int32;
				//temp = (int)t->value->int32;
				if (show_celsius == false) {
					temp = (1.8 * temp) + 32;
				}
		
		
				snprintf(temperature_buffer, sizeof(temperature_buffer), "%+03d", temp);
				//signo de la temperatura
				gbitmap_destroy(bitmap_temp_sign);
				if(temperature_buffer[0] == '+') {bitmap_temp_sign = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_MAS_MINI);}	
				if(temperature_buffer[0] == '-') {bitmap_temp_sign = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_MENOS_MINI);}	
			  bitmap_layer_set_bitmap(layer_temp_sign, bitmap_temp_sign);
				//decenas de la temperatura
				gbitmap_destroy(bitmap_temp_dec);
		    if(temperature_buffer[1] == '0') {bitmap_temp_dec = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_0_MINI);}	
				if(temperature_buffer[1] == '1') {bitmap_temp_dec = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_1_MINI);}	
				if(temperature_buffer[1] == '2') {bitmap_temp_dec = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_2_MINI);}	
				if(temperature_buffer[1] == '3') {bitmap_temp_dec = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_3_MINI);}	
				if(temperature_buffer[1] == '4') {bitmap_temp_dec = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_4_MINI);}	
				if(temperature_buffer[1] == '5') {bitmap_temp_dec = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_5_MINI);}	
				if(temperature_buffer[1] == '6') {bitmap_temp_dec = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_6_MINI);}	
				if(temperature_buffer[1] == '7') {bitmap_temp_dec = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_7_MINI);}
				if(temperature_buffer[1] == '8') {bitmap_temp_dec = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_8_MINI);}
				if(temperature_buffer[1] == '9') {bitmap_temp_dec = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_9_MINI);}
				bitmap_layer_set_bitmap(layer_temp_dec, bitmap_temp_dec);
				//unidades de la temperatura
				gbitmap_destroy(bitmap_temp_unit);
				if(temperature_buffer[2] == '0') {bitmap_temp_unit = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_0_MINI);}	
				if(temperature_buffer[2] == '1') {bitmap_temp_unit = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_1_MINI);}	
				if(temperature_buffer[2] == '2') {bitmap_temp_unit = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_2_MINI);}	
				if(temperature_buffer[2] == '3') {bitmap_temp_unit = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_3_MINI);}	
				if(temperature_buffer[2] == '4') {bitmap_temp_unit = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_4_MINI);}	
				if(temperature_buffer[2] == '5') {bitmap_temp_unit = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_5_MINI);}	
				if(temperature_buffer[2] == '6') {bitmap_temp_unit = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_6_MINI);}	
				if(temperature_buffer[2] == '7') {bitmap_temp_unit = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_7_MINI);}	
				if(temperature_buffer[2] == '8') {bitmap_temp_unit = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_8_MINI);}	
				if(temperature_buffer[2] == '9') {bitmap_temp_unit = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_9_MINI);}			
				bitmap_layer_set_bitmap(layer_temp_unit, bitmap_temp_unit);
				//grados
		
				gbitmap_destroy(bitmap_temp_degrees);
				if (show_celsius == false) {
					bitmap_temp_degrees = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_F_MINI);
				} else {
					bitmap_temp_degrees = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_C_MINI);
				}
					
		
				bitmap_layer_set_bitmap(layer_temp_degrees, bitmap_temp_degrees);	
      break;
		
    	
		
		  case KEY_CONDITIONS:
				snprintf(conditions_buffer, sizeof(conditions_buffer), "%s", t->value->cstring);
				//bitmap de las condiciones
				gbitmap_destroy(conditions_bitmap);
				if (strcmp(conditions_buffer,"01d") == 0) {conditions_bitmap = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_W_CLEAR);}		
				if (strcmp(conditions_buffer,"01n") == 0) {conditions_bitmap = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_W_CLEAR_NIGHT);}		
	  		if (strcmp(conditions_buffer,"02d") == 0) {conditions_bitmap = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_W_PARTLY_CLOUDY);}		
				if (strcmp(conditions_buffer,"02n") == 0) {conditions_bitmap = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_W_PARTLY_CLOUDY_NIGHT);}		
				if (strcmp(conditions_buffer,"03d") == 0) {conditions_bitmap = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_W_CLOUDY);}		
				if (strcmp(conditions_buffer,"03n") == 0) {conditions_bitmap = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_W_CLOUDY);}				
				if (strcmp(conditions_buffer,"04d") == 0) {conditions_bitmap = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_W_CLOUDY);}	
				if (strcmp(conditions_buffer,"04n") == 0) {conditions_bitmap = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_W_CLOUDY);}
				if (strcmp(conditions_buffer,"09d") == 0) {conditions_bitmap = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_W_DRIZZLE);}	
				if (strcmp(conditions_buffer,"09n") == 0) {conditions_bitmap = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_W_DRIZZLE);}							
				if (strcmp(conditions_buffer,"10d") == 0) {conditions_bitmap = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_W_RAIN);}
				if (strcmp(conditions_buffer,"10n") == 0) {conditions_bitmap = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_W_RAIN);}							
				if (strcmp(conditions_buffer,"11d") == 0) {conditions_bitmap = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_W_THUNDER);}
				if (strcmp(conditions_buffer,"11n") == 0) {conditions_bitmap = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_W_THUNDER);}
				if (strcmp(conditions_buffer,"13d") == 0) {conditions_bitmap = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_W_SNOW);}	
				if (strcmp(conditions_buffer,"13n") == 0) {conditions_bitmap = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_W_SNOW);}
				if (strcmp(conditions_buffer,"50d") == 0) {conditions_bitmap = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_W_FOG);}		
				if (strcmp(conditions_buffer,"50n") == 0) {conditions_bitmap = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_W_FOG);}
			  bitmap_layer_set_bitmap(conditions_layer, conditions_bitmap);																										 
			break;
		
	
  }
	
}




static void main_window_unload(Window *window) {
	

//	gbitmap_destroy(troll);

	//Destroy GBitmaps
	gbitmap_destroy(hour_bitmap_dec);
	gbitmap_destroy(hour_bitmap_unit);
	gbitmap_destroy(minute_bitmap_dec);
	gbitmap_destroy(minute_bitmap_unit);
	// Destroy TextLayer
  bitmap_layer_destroy(hour_layer_dec);
	bitmap_layer_destroy(hour_layer_unit);
	bitmap_layer_destroy(minute_layer_dec);
	bitmap_layer_destroy(minute_layer_unit);
	
	gbitmap_destroy(conditions_bitmap);
	bitmap_layer_destroy(conditions_layer);


	//Destroy temperature layers
	bitmap_layer_destroy(layer_temp_sign);
	bitmap_layer_destroy(layer_temp_dec);
	bitmap_layer_destroy(layer_temp_unit);
	bitmap_layer_destroy(layer_temp_degrees);
	
	//Destroy tempeatura bitmaps
	gbitmap_destroy(bitmap_temp_sign);
	gbitmap_destroy(bitmap_temp_dec);
	gbitmap_destroy(bitmap_temp_unit);
	gbitmap_destroy(bitmap_temp_degrees);

	//Destroy date bitmaps
	gbitmap_destroy(bitmap_date_day);
	gbitmap_destroy(bitmap_date_dec);
	gbitmap_destroy(bitmap_date_unit);
	//Destroy date layers
	bitmap_layer_destroy(layer_date_day);
	bitmap_layer_destroy(layer_date_dec);
	bitmap_layer_destroy(layer_date_unit);

	//battery
	bitmap_layer_destroy(battery_layer);
	gbitmap_destroy(battery_bitmap);
	

}


static void in_received_handler(DictionaryIterator *iter, void *context) 
{
		
    (void) context;
    //Get data
    Tuple *t = dict_read_first(iter);
    while(t != NULL)
    {		
			process_tuple(t);      
      //Get next
      t = dict_read_next(iter);
    }
}



static void inbox_dropped_callback(AppMessageResult reason, void *context) {
  APP_LOG(APP_LOG_LEVEL_ERROR, "Message dropped!");
}

static void outbox_failed_callback(DictionaryIterator *iterator, AppMessageResult reason, void *context) {
  APP_LOG(APP_LOG_LEVEL_ERROR, "Outbox send failed!");
}

static void outbox_sent_callback(DictionaryIterator *iterator, void *context) {
  APP_LOG(APP_LOG_LEVEL_INFO, "Outbox send success!");
}


static void init() {
  // Create main Window element and assign to pointer
  s_main_window = window_create();

  // Set handlers to manage the elements inside the Window
  window_set_window_handlers(s_main_window, (WindowHandlers) {
    .load = main_window_load,
    .unload = main_window_unload
  });

  // Show the Window on the watch, with animated=true
  window_stack_push(s_main_window, true);
 
	// Register with TickTimerService
  tick_timer_service_subscribe(SECOND_UNIT , tick_segundos);

	//Subscribe to AccelerometerService
	accel_tap_service_subscribe(accel_tap_handler);
	
	app_message_register_inbox_received((AppMessageInboxReceived) in_received_handler);
	
	// Register for Bluetooth connection updates
	connection_service_subscribe((ConnectionHandlers) {
		.pebble_app_connection_handler = bluetooth_callback
	});
	
	
	// Register callbacks
	app_message_register_inbox_dropped(inbox_dropped_callback);
	app_message_register_outbox_failed(outbox_failed_callback);
	app_message_register_outbox_sent(outbox_sent_callback);

	app_message_open(784, 784);
}

static void deinit() {
	 // Stop any animation in progress
  animation_unschedule_all();
  // Destroy Window
  window_destroy(s_main_window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}