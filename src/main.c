#include "pebble.h"
#include "common.h"

static Window* window;
static GBitmap *background_image_container;

// DCK
TextLayer *recife_text_layer;
TextLayer *taipei_text_layer;
char*     recife_buffer = "R00:01"; // using different strings so that the compiler wont optimize them to a single value!
char*     taipei_buffer = "T00:02";

static Layer *minute_display_layer;
static Layer *hour_display_layer;
static Layer *center_display_layer;
static Layer *second_display_layer;
static TextLayer *date_layer;
static char date_text[] = "Wed 13 ";
static bool bt_ok = false;
static uint8_t battery_level;
static bool battery_plugged;

// TBTR
TextLayer *battery_label_text_layer;
static char battery_level_buffer[] = "100%";

static GBitmap *icon_battery;
static GBitmap *icon_battery_charge;
static GBitmap *icon_bt;

static Layer *battery_layer;
static Layer *bt_layer;

bool g_conserve = false;

#ifdef INVERSE
static InverterLayer *full_inverse_layer;
#endif

static Layer *background_layer;
static Layer *window_layer;

const GPathInfo MINUTE_HAND_PATH_POINTS = { 4, (GPoint[] ) { { -4, 15 },
				{ 4, 15 }, { 4, -70 }, { -4, -70 }, } };

const GPathInfo HOUR_HAND_PATH_POINTS = { 4, (GPoint[] ) { { -4, 15 },
				{ 4, 15 }, { 4, -50 }, { -4, -50 }, } };

static GPath *hour_hand_path;
static GPath *minute_hand_path;

static AppTimer *timer_handle;
#define COOKIE_MY_TIMER 1
static int my_cookie = COOKIE_MY_TIMER;
#define ANIM_IDLE 0
#define ANIM_START 1
#define ANIM_HOURS 2
#define ANIM_MINUTES 3
#define ANIM_SECONDS 4
#define ANIM_DONE 5
int init_anim = ANIM_DONE;
int32_t second_angle_anim = 0;
unsigned int minute_angle_anim = 0;
unsigned int hour_angle_anim = 0;

//DCK
void update_buffer(char* buffer, struct tm *tick_time) {
  // Write the current hours and minutes into the buffer
  if(clock_is_24h_style() == true) {
    // Use 24 hour format
    strftime(buffer, sizeof("00:00"), "%H:%M", tick_time);
  } else {
    // Use 12 hour format
    strftime(buffer, sizeof("00:00"), "%I:%M", tick_time);
  }    
}
void update_double_time() {
  time_t temp = time(NULL);

  temp = temp - 5*60*60;
  update_buffer(recife_buffer+1, localtime(&temp)); // TODO: beware! pointer arithmetics!
  
  temp = temp + 5*60*60 + 6*60*60;
  update_buffer(taipei_buffer+1, localtime(&temp)); // TODO: beware! pointer arithmetics!
    
  // Display this time on the TextLayer
  text_layer_set_text(recife_text_layer, recife_buffer);  
  text_layer_set_text(taipei_text_layer, taipei_buffer);  
  
  //TBTR
  snprintf(battery_level_buffer, sizeof(battery_level_buffer), "%d%%", battery_level);
  text_layer_set_text(battery_label_text_layer, battery_level_buffer);  
}

void init_double_time(void) {
	recife_text_layer = text_layer_create(GRect(-77, 100, 144, 100));
	taipei_text_layer = text_layer_create(GRect(-30, 100, 144, 130));

	// Set the text, font, and text alignment
 	text_layer_set_font(recife_text_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18));
	text_layer_set_text_alignment(recife_text_layer, GTextAlignmentRight);
	text_layer_set_text_color(recife_text_layer, GColorWhite);
	text_layer_set_background_color(recife_text_layer, GColorClear);

  text_layer_set_font(taipei_text_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18));
	text_layer_set_text_alignment(taipei_text_layer, GTextAlignmentRight);
	text_layer_set_text_color(taipei_text_layer, GColorWhite);
	text_layer_set_background_color(taipei_text_layer, GColorClear);

	// Add the text layer to the window
	layer_add_child(window_get_root_layer(window), text_layer_get_layer(recife_text_layer));
	layer_add_child(window_get_root_layer(window), text_layer_get_layer(taipei_text_layer));

  // TBTR
	battery_label_text_layer = text_layer_create(GRect(0, 52, 144, 100));
  text_layer_set_font(battery_label_text_layer, fonts_get_system_font(FONT_KEY_GOTHIC_14));
	text_layer_set_text_color(battery_label_text_layer, GColorWhite);
	text_layer_set_background_color(battery_label_text_layer, GColorClear);
	text_layer_set_text_alignment(battery_label_text_layer, GTextAlignmentCenter);
	layer_add_child(window_get_root_layer(window), text_layer_get_layer(battery_label_text_layer ));
  
  update_double_time();
}

void deinit_double_time(void) {
	text_layer_destroy(recife_text_layer);
	text_layer_destroy(taipei_text_layer);  
	text_layer_destroy(battery_label_text_layer);  
}

void handle_timer(void* vdata) {

	int *data = (int *) vdata;

	if (*data == my_cookie) {
		if (init_anim == ANIM_START) {
			init_anim = ANIM_HOURS;
			timer_handle = app_timer_register(50 /* milliseconds */,
					&handle_timer, &my_cookie);
		} else if (init_anim == ANIM_HOURS) {
			layer_mark_dirty(hour_display_layer);
			timer_handle = app_timer_register(50 /* milliseconds */,
					&handle_timer, &my_cookie);
		} else if (init_anim == ANIM_MINUTES) {
			layer_mark_dirty(minute_display_layer);
			timer_handle = app_timer_register(50 /* milliseconds */,
					&handle_timer, &my_cookie);
		} else if (init_anim == ANIM_SECONDS) {
			layer_mark_dirty(second_display_layer);
			timer_handle = app_timer_register(50 /* milliseconds */,
					&handle_timer, &my_cookie);
		}
	}
  
  update_double_time();
}

void second_display_layer_update_callback(Layer *me, GContext* ctx) {
	(void) me;

	time_t now = time(NULL);
	struct tm *t = localtime(&now);

	int32_t second_angle = t->tm_sec * (0xffff / 60);
	int second_hand_length = 70;
	GPoint center = grect_center_point(&GRECT_FULL_WINDOW);
	GPoint second = GPoint(center.x, center.y - second_hand_length);

	if (init_anim < ANIM_SECONDS) {
		second = GPoint(center.x, center.y - 70);
	} else if (init_anim == ANIM_SECONDS) {
		second_angle_anim += 0xffff / 60;
		if (second_angle_anim >= second_angle) {
			init_anim = ANIM_DONE;
			second =
					GPoint(center.x + second_hand_length * sin_lookup(second_angle)/0xffff,
							center.y + (-second_hand_length) * cos_lookup(second_angle)/0xffff);
		} else {
			second =
					GPoint(center.x + second_hand_length * sin_lookup(second_angle_anim)/0xffff,
							center.y + (-second_hand_length) * cos_lookup(second_angle_anim)/0xffff);
		}
	} else {
		second =
				GPoint(center.x + second_hand_length * sin_lookup(second_angle)/0xffff,
						center.y + (-second_hand_length) * cos_lookup(second_angle)/0xffff);
	}

	graphics_context_set_stroke_color(ctx, GColorWhite);

	graphics_draw_line(ctx, center, second);
}

void center_display_layer_update_callback(Layer *me, GContext* ctx) {
	(void) me;

	GPoint center = grect_center_point(&GRECT_FULL_WINDOW);
	graphics_context_set_fill_color(ctx, GColorBlack);
	graphics_fill_circle(ctx, center, 4);
	graphics_context_set_fill_color(ctx, GColorWhite);
	graphics_fill_circle(ctx, center, 3);
  
  update_double_time();
}

void minute_display_layer_update_callback(Layer *me, GContext* ctx) {
	(void) me;

	time_t now = time(NULL);
	struct tm *t = localtime(&now);

	unsigned int angle = t->tm_min * 6 + t->tm_sec / 10;

	if (init_anim < ANIM_MINUTES) {
		angle = 0;
	} else if (init_anim == ANIM_MINUTES) {
		minute_angle_anim += 6;
		if (minute_angle_anim >= angle) {
			init_anim = ANIM_SECONDS;
		} else {
			angle = minute_angle_anim;
		}
	}

	gpath_rotate_to(minute_hand_path, (TRIG_MAX_ANGLE / 360) * angle);

	graphics_context_set_fill_color(ctx, GColorWhite);
	graphics_context_set_stroke_color(ctx, GColorBlack);

	gpath_draw_filled(ctx, minute_hand_path);
	gpath_draw_outline(ctx, minute_hand_path);
}

void hour_display_layer_update_callback(Layer *me, GContext* ctx) {
	(void) me;

	time_t now = time(NULL);
	struct tm *t = localtime(&now);

	unsigned int angle = t->tm_hour * 30 + t->tm_min / 2;

	if (init_anim < ANIM_HOURS) {
		angle = 0;
	} else if (init_anim == ANIM_HOURS) {
		if (hour_angle_anim == 0 && t->tm_hour >= 12) {
			hour_angle_anim = 360;
		}
		hour_angle_anim += 6;
		if (hour_angle_anim >= angle) {
			init_anim = ANIM_MINUTES;
		} else {
			angle = hour_angle_anim;
		}
	}

	gpath_rotate_to(hour_hand_path, (TRIG_MAX_ANGLE / 360) * angle);

	graphics_context_set_fill_color(ctx, GColorWhite);
	graphics_context_set_stroke_color(ctx, GColorBlack);

	gpath_draw_filled(ctx, hour_hand_path);
	gpath_draw_outline(ctx, hour_hand_path);
}

void draw_date() {

	time_t now = time(NULL);
	struct tm *t = localtime(&now);

	strftime(date_text, sizeof(date_text), "%a %d", t);

	text_layer_set_text(date_layer, date_text);
}

/*
 * Battery icon callback handler
 */
void battery_layer_update_callback(Layer *layer, GContext *ctx) {

  graphics_context_set_compositing_mode(ctx, GCompOpAssign);

  if (!battery_plugged) {
    graphics_draw_bitmap_in_rect(ctx, icon_battery, GRect(0, 0, 24, 12));
    graphics_context_set_stroke_color(ctx, GColorBlack);
    graphics_context_set_fill_color(ctx, GColorWhite);
    graphics_fill_rect(ctx, GRect(7, 4, (uint8_t)((battery_level / 100.0) * 11.0), 4), 0, GCornerNone);
  } else {
    graphics_draw_bitmap_in_rect(ctx, icon_battery_charge, GRect(0, 0, 24, 12));
  }
}



void battery_state_handler(BatteryChargeState charge) {
	battery_level = charge.charge_percent;
	battery_plugged = charge.is_plugged;
	layer_mark_dirty(battery_layer);
	/*
	if (!battery_plugged && battery_level < 20)
		conserve_power(true);
	else
		conserve_power(false);
	*/
}

/*
 * Bluetooth icon callback handler
 */
void bt_layer_update_callback(Layer *layer, GContext *ctx) {
  if (bt_ok)
  	graphics_context_set_compositing_mode(ctx, GCompOpAssign);
  else
  	graphics_context_set_compositing_mode(ctx, GCompOpClear);
  graphics_draw_bitmap_in_rect(ctx, icon_bt, GRect(0, 0, 9, 12));
}

void bt_connection_handler(bool connected) {
	bt_ok = connected;
	layer_mark_dirty(bt_layer);
}

void draw_background_callback(Layer *layer, GContext *ctx) {
	graphics_context_set_compositing_mode(ctx, GCompOpAssign);
	graphics_draw_bitmap_in_rect(ctx, background_image_container,
			GRECT_FULL_WINDOW);
}

void init() {

	// Window
	window = window_create();
	window_stack_push(window, true /* Animated */);
	window_layer = window_get_root_layer(window);

	// Background image
	background_image_container = gbitmap_create_with_resource(
			RESOURCE_ID_IMAGE_BACKGROUND);
	background_layer = layer_create(GRECT_FULL_WINDOW);
	layer_set_update_proc(background_layer, &draw_background_callback);
	layer_add_child(window_layer, background_layer);

	// Date setup
	date_layer = text_layer_create(GRect(27, 115, 90, 21));
	text_layer_set_text_color(date_layer, GColorWhite);
	text_layer_set_text_alignment(date_layer, GTextAlignmentCenter);
	text_layer_set_background_color(date_layer, GColorClear);
	text_layer_set_font(date_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18));
	layer_add_child(window_layer, text_layer_get_layer(date_layer));

  init_double_time();
	draw_date();
  update_double_time();

	// Status setup
	icon_battery = gbitmap_create_with_resource(RESOURCE_ID_BATTERY_ICON);
	icon_battery_charge = gbitmap_create_with_resource(RESOURCE_ID_BATTERY_CHARGE);
	icon_bt = gbitmap_create_with_resource(RESOURCE_ID_BLUETOOTH);

	BatteryChargeState initial = battery_state_service_peek();
	battery_level = initial.charge_percent;
	battery_plugged = initial.is_plugged;
	battery_layer = layer_create(GRect(5,132,24,12)); //24*12
	layer_set_update_proc(battery_layer, &battery_layer_update_callback);
	layer_add_child(window_layer, battery_layer);

	bt_ok = bluetooth_connection_service_peek();
	bt_layer = layer_create(GRect(9,147,9,12)); //9*12
	layer_set_update_proc(bt_layer, &bt_layer_update_callback);
	layer_add_child(window_layer, bt_layer);

	// Hands setup
	hour_display_layer = layer_create(GRECT_FULL_WINDOW);
	layer_set_update_proc(hour_display_layer,
			&hour_display_layer_update_callback);
	layer_add_child(window_layer, hour_display_layer);

	hour_hand_path = gpath_create(&HOUR_HAND_PATH_POINTS);
	gpath_move_to(hour_hand_path, grect_center_point(&GRECT_FULL_WINDOW));

	minute_display_layer = layer_create(GRECT_FULL_WINDOW);
	layer_set_update_proc(minute_display_layer,
			&minute_display_layer_update_callback);
	layer_add_child(window_layer, minute_display_layer);

	minute_hand_path = gpath_create(&MINUTE_HAND_PATH_POINTS);
	gpath_move_to(minute_hand_path, grect_center_point(&GRECT_FULL_WINDOW));

	center_display_layer = layer_create(GRECT_FULL_WINDOW);
	layer_set_update_proc(center_display_layer,
			&center_display_layer_update_callback);
	layer_add_child(window_layer, center_display_layer);

	second_display_layer = layer_create(GRECT_FULL_WINDOW);
	layer_set_update_proc(second_display_layer,
			&second_display_layer_update_callback);
	layer_add_child(window_layer, second_display_layer);

	// Configurable inverse
#ifdef INVERSE
	full_inverse_layer = inverter_layer_create(GRECT_FULL_WINDOW);
	layer_add_child(window_layer, inverter_layer_get_layer(full_inverse_layer));
#endif

}

void deinit() {

  // http://forums.getpebble.com/discussion/12927/app-crashes-on-2-1-double-free-detected
	//window_destroy(window);
	gbitmap_destroy(background_image_container);
	gbitmap_destroy(icon_battery);
	gbitmap_destroy(icon_battery_charge);
	gbitmap_destroy(icon_bt);
	text_layer_destroy(date_layer);
	layer_destroy(minute_display_layer);
	layer_destroy(hour_display_layer);
	layer_destroy(center_display_layer);
	layer_destroy(second_display_layer);
	layer_destroy(battery_layer);
	layer_destroy(bt_layer);
  deinit_double_time();

#ifdef INVERSE
	inverter_layer_destroy(full_inverse_layer);
#endif

	layer_destroy(background_layer);
	layer_destroy(window_layer);

	gpath_destroy(hour_hand_path);
	gpath_destroy(minute_hand_path);
}

void handle_tick(struct tm *tick_time, TimeUnits units_changed) {

	if (init_anim == ANIM_IDLE) {
		init_anim = ANIM_START;
		timer_handle = app_timer_register(50 /* milliseconds */, &handle_timer,
				&my_cookie);
	} else if (init_anim == ANIM_DONE) {
    update_double_time();
		if (tick_time->tm_sec % 10 == 0) {
			layer_mark_dirty(minute_display_layer);

			if (tick_time->tm_sec == 0) {
				if (tick_time->tm_min % 2 == 0) {
					layer_mark_dirty(hour_display_layer);
					if (tick_time->tm_min == 0 && tick_time->tm_hour == 0) {
						draw_date();
					}
				}
			}
		}

		layer_mark_dirty(second_display_layer);
	}
}

void conserve_power(bool conserve) {
	if (conserve == g_conserve)
		return;
	g_conserve = conserve;
	if (conserve) {
		tick_timer_service_unsubscribe();
		tick_timer_service_subscribe(MINUTE_UNIT, &handle_tick);
		layer_set_hidden(second_display_layer, true);
	} else {
		tick_timer_service_unsubscribe();
		tick_timer_service_subscribe(SECOND_UNIT, &handle_tick);
		layer_set_hidden(second_display_layer, false);
	}
}



/*
 * Main - or main as it is known
 */
int main(void) {
	init();
	tick_timer_service_subscribe(SECOND_UNIT, &handle_tick);
	bluetooth_connection_service_subscribe(&bt_connection_handler);
	battery_state_service_subscribe	(&battery_state_handler);

	conserve_power(true);

	app_event_loop();
	deinit();
}

