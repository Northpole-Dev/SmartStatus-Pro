#include <pebble.h>
#include "globals.h"

#define STRING_LENGTH 255
#define NUM_WEATHER_IMAGES	2

enum {CALENDAR_LAYER, MUSIC_LAYER, NUM_LAYERS};

static void updateCalendar();
static void handle_timer(void *data);

static Window *window;

static PropertyAnimation *ani_out, *ani_in;

static Layer *animated_layer[NUM_LAYERS], *weather_layer;
static Layer *battery_layer, *battery_pbl_layer;

static TextLayer *text_date_layer, *text_time_layer;

static TextLayer *calendar_date_layer, *calendar_text_layer;
static TextLayer *music_artist_layer, *music_song_layer;
 
static BitmapLayer *background_image, *weather_image, *battery_image_layer, *battery_pbl_image_layer;

static int active_layer;

static int weather_img, batteryPercent, batteryPblPercent;

static Event event;
static Music music;


GBitmap *bg_image, *battery_image, *battery_pbl_image;
GBitmap *weather_status_imgs[NUM_WEATHER_IMAGES];



const int WEATHER_IMG_IDS[] = {	
  RESOURCE_ID_IMAGE_CONNECT,
  RESOURCE_ID_IMAGE_DISCONNECT
};




static void down_click_handler(ClickRecognizerRef recognizer, void *context) {
	//slide layers in/out

	property_animation_destroy((PropertyAnimation*)ani_in);
	property_animation_destroy((PropertyAnimation*)ani_out);


	ani_out = property_animation_create_layer_frame(animated_layer[active_layer], &GRect(0, 124, 143, 45), &GRect(-138, 124, 143, 45));
	animation_schedule((Animation*)ani_out);


	active_layer = (active_layer + 1) % (NUM_LAYERS);

	ani_in = property_animation_create_layer_frame(animated_layer[active_layer], &GRect(138, 124, 144, 45), &GRect(0, 124, 144, 45));
	animation_schedule((Animation*)ani_in);


}

static void click_config_provider(void *context) {
  window_single_click_subscribe(BUTTON_ID_DOWN, down_click_handler);
}

static void window_load(Window *window) {
	updateCalendar();
	app_timer_register(1000, &handle_timer, (void *)REQUEST_BATTERY_KEY);

}

static void window_unload(Window *window) {
	DictionaryIterator *iter;
	app_message_outbox_begin(&iter);
	if (!iter) return;
	dict_write_uint8(iter, REQUEST_MUSIC_KEY, 0);
	app_message_outbox_send();
	
}


void battery_layer_update_callback(Layer *me, GContext* ctx) {
	
	//draw the remaining battery percentage
	graphics_context_set_stroke_color(ctx, GColorBlack);
	graphics_context_set_fill_color(ctx, GColorWhite);

	graphics_fill_rect(ctx, GRect(2+16-(int)((batteryPercent/100.0)*16.0), 2, (int)((batteryPercent/100.0)*16.0), 8), 0, GCornerNone);
	
}

void battery_pbl_layer_update_callback(Layer *me, GContext* ctx) {
	
	//draw the remaining pebble battery percentage
	graphics_context_set_stroke_color(ctx, GColorBlack);
	graphics_context_set_fill_color(ctx, GColorWhite);

	graphics_fill_rect(ctx, GRect(2+16-(int)((batteryPblPercent/100.0)*16.0), 2, (int)((batteryPblPercent/100.0)*16.0), 8), 0, GCornerNone);
	
}


void handle_minute_tick(struct tm *tick_time, TimeUnits units_changed) {
  // Need to be static because they're used by the system later.
  static char time_text[] = "00:00";
  static char date_text[] = "Xxxxxxxxx 00";

  char *time_format;


  // TODO: Only update the date when it's changed.
  strftime(date_text, sizeof(date_text), "%a, %b %e", tick_time);
  text_layer_set_text(text_date_layer, date_text);


  if (clock_is_24h_style()) {
    time_format = "%R";
  } else {
    time_format = "%I:%M";
  }

  strftime(time_text, sizeof(time_text), time_format, tick_time);

  // Kludge to handle lack of non-padded hour format string
  // for twelve hour clock.
  if (!clock_is_24h_style() && (time_text[0] == '0')) {
    memmove(time_text, &time_text[1], sizeof(time_text) - 1);
  }

  text_layer_set_text(text_time_layer, time_text);

  if (tick_time->tm_min % 10 == 0) {
    updateCalendar();
    app_timer_register(1000, &handle_timer, (void *)REQUEST_BATTERY_KEY);
  }
}


void bluetoothChanged(bool connected) {

	if (connected) {
		bitmap_layer_set_bitmap(weather_image, weather_status_imgs[0]);
	} else {
		bitmap_layer_set_bitmap(weather_image, weather_status_imgs[NUM_WEATHER_IMAGES-1]);
		vibes_double_pulse();
	}
	
}


void batteryChanged(BatteryChargeState batt) {
	
	batteryPblPercent = batt.charge_percent;
	layer_mark_dirty(battery_layer);
	
}


static void init(void) {
  window = window_create();
  window_set_fullscreen(window, true);
  window_set_click_config_provider(window, click_config_provider);
  window_set_window_handlers(window, (WindowHandlers) {
    .load = window_load,
    .unload = window_unload
  });
  const bool animated = true;
  window_stack_push(window, animated);

	//init weather images
	for (int i=0; i<NUM_WEATHER_IMAGES; i++) {
	  	weather_status_imgs[i] = gbitmap_create_with_resource(WEATHER_IMG_IDS[i]);
	}
	
  	bg_image = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_BACKGROUND);


  	Layer *window_layer = window_get_root_layer(window);

	//init background image
  	GRect bg_bounds = layer_get_frame(window_layer);

	background_image = bitmap_layer_create(bg_bounds);
	layer_add_child(window_layer, bitmap_layer_get_layer(background_image));
	bitmap_layer_set_bitmap(background_image, bg_image);
	

	//init weather layer and add weather image, and battery indicator
	weather_layer = layer_create(GRect(0, 78, 144, 45));
	layer_add_child(window_layer, weather_layer);

	battery_image = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_BATTERY_PHONE);
	battery_pbl_image = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_BATTERY_PEBBLE);

	battery_image_layer = bitmap_layer_create(GRect(100, 7, 37, 14));
	layer_add_child(weather_layer, bitmap_layer_get_layer(battery_image_layer));
	bitmap_layer_set_bitmap(battery_image_layer, battery_image);

	battery_pbl_image_layer = bitmap_layer_create(GRect(100, 23, 37, 14));
	layer_add_child(weather_layer, bitmap_layer_get_layer(battery_pbl_image_layer));
	bitmap_layer_set_bitmap(battery_pbl_image_layer, battery_pbl_image);


	batteryPercent = 0;

	battery_layer = layer_create(GRect(102, 8, 19, 11));
	layer_set_update_proc(battery_layer, battery_layer_update_callback);
	layer_add_child(weather_layer, battery_layer);

	BatteryChargeState pbl_batt = battery_state_service_peek();
	batteryPblPercent = pbl_batt.charge_percent;

	battery_pbl_layer = layer_create(GRect(102, 24, 19, 11));
	layer_set_update_proc(battery_pbl_layer, battery_pbl_layer_update_callback);
	layer_add_child(weather_layer, battery_pbl_layer);


	if (bluetooth_connection_service_peek()) {
		weather_img = 0;
	} else {
		weather_img = NUM_WEATHER_IMAGES - 1;
	}

	weather_image = bitmap_layer_create(GRect(5, 2, 40, 40)); 
	layer_add_child(weather_layer, bitmap_layer_get_layer(weather_image));
	bitmap_layer_set_bitmap(weather_image, weather_status_imgs[weather_img]);


	//init layers for time and date
	text_date_layer = text_layer_create(bg_bounds);
	text_layer_set_text_alignment(text_date_layer, GTextAlignmentCenter);
	text_layer_set_text_color(text_date_layer, GColorWhite);
	text_layer_set_background_color(text_date_layer, GColorClear);
	layer_set_frame(text_layer_get_layer(text_date_layer), GRect(0, 45, 144, 30));
	text_layer_set_font(text_date_layer, fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_ROBOTO_CONDENSED_21)));
	layer_add_child(window_layer, text_layer_get_layer(text_date_layer));


	text_time_layer = text_layer_create(bg_bounds);
	text_layer_set_text_alignment(text_time_layer, GTextAlignmentCenter);
	text_layer_set_text_color(text_time_layer, GColorWhite);
	text_layer_set_background_color(text_time_layer, GColorClear);
	layer_set_frame(text_layer_get_layer(text_time_layer), GRect(0, -5, 144, 50));
	text_layer_set_font(text_time_layer, fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_ROBOTO_BOLD_SUBSET_49)));
	layer_add_child(window_layer, text_layer_get_layer(text_time_layer));


	//init calendar layer
	animated_layer[CALENDAR_LAYER] = layer_create(GRect(0, 124, 144, 45));
	layer_add_child(window_layer, animated_layer[CALENDAR_LAYER]);
	
	calendar_date_layer = text_layer_create(GRect(6, 0, 132, 21));
	text_layer_set_text_alignment(calendar_date_layer, GTextAlignmentLeft);
	text_layer_set_text_color(calendar_date_layer, GColorWhite);
	text_layer_set_background_color(calendar_date_layer, GColorClear);
	text_layer_set_font(calendar_date_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18));
	layer_add_child(animated_layer[CALENDAR_LAYER], text_layer_get_layer(calendar_date_layer));


	calendar_text_layer = text_layer_create(GRect(6, 15, 132, 28));
	text_layer_set_text_alignment(calendar_text_layer, GTextAlignmentLeft);
	text_layer_set_text_color(calendar_text_layer, GColorWhite);
	text_layer_set_background_color(calendar_text_layer, GColorClear);
	text_layer_set_font(calendar_text_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
	layer_add_child(animated_layer[CALENDAR_LAYER], text_layer_get_layer(calendar_text_layer));
	
	
	
	//init music layer
	animated_layer[MUSIC_LAYER] = layer_create(GRect(144, 124, 144, 45));
	layer_add_child(window_layer, animated_layer[MUSIC_LAYER]);
	
	music_artist_layer = text_layer_create(GRect(6, 0, 132, 21));
	text_layer_set_text_alignment(music_artist_layer, GTextAlignmentLeft);
	text_layer_set_text_color(music_artist_layer, GColorWhite);
	text_layer_set_background_color(music_artist_layer, GColorClear);
	text_layer_set_font(music_artist_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18));
	layer_add_child(animated_layer[MUSIC_LAYER], text_layer_get_layer(music_artist_layer));


	music_song_layer = text_layer_create(GRect(6, 15, 132, 28));
	text_layer_set_text_alignment(music_song_layer, GTextAlignmentLeft);
	text_layer_set_text_color(music_song_layer, GColorWhite);
	text_layer_set_background_color(music_song_layer, GColorClear);
	text_layer_set_font(music_song_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
	layer_add_child(animated_layer[MUSIC_LAYER], text_layer_get_layer(music_song_layer));
	text_layer_set_text(music_song_layer, "No Music Found");


	active_layer = CALENDAR_LAYER;

  	tick_timer_service_subscribe(MINUTE_UNIT, handle_minute_tick);

	bluetooth_connection_service_subscribe(bluetoothChanged);
	battery_state_service_subscribe(batteryChanged);

}

static void deinit(void) {
	
	
	property_animation_destroy((PropertyAnimation*)ani_in);
	property_animation_destroy((PropertyAnimation*)ani_out);
	

	
	bitmap_layer_destroy(background_image);
	layer_destroy(weather_layer);
	bitmap_layer_destroy(battery_image_layer);
	bitmap_layer_destroy(battery_pbl_image_layer);
	layer_destroy(battery_layer);
	layer_destroy(battery_pbl_layer);
	bitmap_layer_destroy(weather_image);
	text_layer_destroy(text_date_layer);
	text_layer_destroy(text_time_layer);
	text_layer_destroy(calendar_date_layer);
	text_layer_destroy(calendar_text_layer);
	text_layer_destroy(music_artist_layer);
	text_layer_destroy(music_song_layer);
	

	for (int i=0; i<NUM_LAYERS; i++) {
		if (animated_layer[i]!=NULL)
			layer_destroy(animated_layer[i]);
	}

	for (int i=0; i<NUM_WEATHER_IMAGES; i++) {
	  	gbitmap_destroy(weather_status_imgs[i]);
	}
	

	gbitmap_destroy(bg_image);
	gbitmap_destroy(battery_image);
	gbitmap_destroy(battery_pbl_image);


	tick_timer_service_unsubscribe();
	bluetooth_connection_service_unsubscribe();
	battery_state_service_unsubscribe();

  
  window_destroy(window);
}


static void updateCalendar() {
	DictionaryIterator *iter;
	app_message_outbox_begin(&iter);
	if (!iter) return;
	dict_write_int8(iter, REQUEST_CALENDAR_KEY, 0);
	uint8_t clock_style = clock_is_24h_style() ? CLOCK_STYLE_24H : CLOCK_STYLE_12H;
	dict_write_uint8(iter, CLOCK_STYLE_KEY, clock_style);
	app_message_outbox_send();
}

static void handle_timer(void *data) {
	DictionaryIterator *iter;
	app_message_outbox_begin(&iter);

	if (!iter) {
		app_timer_register(1000, &handle_timer, data);
		return;
	}

	dict_write_uint8(iter, (int)data, 1);
	app_message_outbox_send();

	if ((int)data == REQUEST_BATTERY_KEY) {
		app_timer_register(1000, &handle_timer, (void *)REQUEST_MUSIC_KEY);
	}
}


void rcv(DictionaryIterator *received, void *context) {
	// Got a message callback
	Tuple *t;


	t=dict_find(received, BATTERY_RESPONSE_KEY); 
	if (t!=NULL) {
		BatteryStatus battery_status;
		memcpy(&battery_status, t->value->data, sizeof(BatteryStatus));

		batteryPercent = battery_status.level;
		layer_mark_dirty(battery_layer);

		return;
	}

	t=dict_find(received, CALENDAR_RESPONSE_KEY); 
	if (t!=NULL) {
		if (t->length % sizeof(Event) == 1) {
			uint8_t count = t->value->data[0];

			if (count == 1) {
				memcpy(&event, &t->value->data[1], sizeof(Event));

				text_layer_set_text(calendar_date_layer, event.start_date);
				text_layer_set_text(calendar_text_layer, event.title);
			}
		}

		return;
	}


	t=dict_find(received, MUSIC_RESPONSE_KEY); 
	if (t!=NULL) {
		if (t->type == TUPLE_BYTE_ARRAY) {
			memcpy(&music, t->value->data, sizeof(Music));

			text_layer_set_text(music_artist_layer, music.artist);
			text_layer_set_text(music_song_layer, music.title);
		}

		return;
	}


	t=dict_find(received, RECONNECT_KEY);
	if (t!=NULL) {
		updateCalendar();
		app_timer_register(1000, handle_timer, (void *)REQUEST_BATTERY_KEY);
	}

}

int main(void) {
	app_message_open(124, 256);
	app_message_register_inbox_received(rcv);
	
  init();


  app_event_loop();
  app_message_deregister_callbacks();

  deinit();

}
