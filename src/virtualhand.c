#include <pebble.h>

static Window *s_window;
static TextLayer *s_text_layer;

#define KEY_ACCEL_DATA_X 0
#define KEY_ACCEL_DATA_Y 1
#define KEY_ACCEL_DATA_Z 2
#define KEY_ACCEL_DATA_TS 3

#define KEY_ACCEL_DATA_COUNT 0
#define KEY_ACCEL_DATA 1

static void accel_data_handler(AccelData* data, uint32_t num_samples) {
  // Send samples straight to phone
  DictionaryIterator* iter;
  app_message_outbox_begin(&iter);

  uint8_t sample_size = 3 * sizeof(int16_t)      // x y z component
			 + 1 * sizeof(char)      // vibration on / off
			 + 1 * sizeof(uint64_t); // timestamp
  uint16_t sz = sample_size * num_samples;
  uint8_t* bytes = malloc(sz);

  // pack binary data for sending
  for (uint8_t i = 0; i<num_samples; i++) {
    int16_t* x_ptr = (int16_t*)&bytes[i * sample_size];
    *x_ptr = data->x;
    int16_t* y_ptr = (int16_t*)&bytes[i * sample_size + 2];
    *y_ptr = data->y;
    int16_t* z_ptr = (int16_t*)&bytes[i * sample_size + 4];
    *z_ptr = data->z;
    
    bytes[i * sample_size + 6] = data->did_vibrate ? 1 : 0;
    
    uint64_t* ts_ptr = (uint64_t*)&bytes[i * sample_size + 7];
    *ts_ptr = data->timestamp;
  }

  dict_write_uint8(iter, KEY_ACCEL_DATA_COUNT, num_samples);
  dict_write_data(iter, KEY_ACCEL_DATA, bytes, sz);

  app_message_outbox_send();
  APP_LOG(APP_LOG_LEVEL_INFO, "Sent sample");

  free(bytes);
}

static void prv_window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);

  s_text_layer = text_layer_create(GRect(0, 72, bounds.size.w, 20));
  text_layer_set_text(s_text_layer, "Press a button");
  text_layer_set_text_alignment(s_text_layer, GTextAlignmentCenter);
  layer_add_child(window_layer, text_layer_get_layer(s_text_layer));
}

static void prv_window_unload(Window *window) {
  text_layer_destroy(s_text_layer);
}

static void prv_init(void) {
  s_window = window_create();
  window_set_window_handlers(s_window, (WindowHandlers) {
    .load = prv_window_load,
    .unload = prv_window_unload,
  });
  const bool animated = true;
  window_stack_push(s_window, animated);

  // Register accelerometer handling
  accel_service_set_sampling_rate(ACCEL_SAMPLING_25HZ);
  accel_data_service_subscribe(1, accel_data_handler);

  // Register communication
  app_comm_set_sniff_interval(SNIFF_INTERVAL_REDUCED);
  app_message_open(64, 64);
}

static void prv_deinit(void) {
  window_destroy(s_window);
  accel_data_service_unsubscribe();
}

int main(void) {
  prv_init();

  app_event_loop();

  prv_deinit();
}
