#include <pebble.h>

static Window *s_window;
static TextLayer *s_text_layer;

#define SAMPLE_RATE           ACCEL_SAMPLING_100HZ
#define SAMPLE_BATCH_SIZE     4
#define INBOX_SIZE            64
#define OUTBOX_SIZE           64

#define KEY_ACCEL_DATA        1

static AccelData avg(AccelData* ints, uint32_t sz) {
  int16_t sumx, sumy, sumz, avgx, avgy, avgz;
  sumx = sumy = sumz = 0;
  for (uint8_t i=0; i<sz; i++) {
    sumx += ints[i].x;
    sumy += ints[i].y;
    sumz += ints[i].z;
  }
  avgx = sumx / sz;
  avgy = sumy / sz;
  avgz = sumz / sz;
  
  return (AccelData){.x = avgx, .y = avgy, .z = avgz};
}

static void accel_data_handler(AccelData* data, uint32_t num_samples) {
  // Send samples straight to phone
  DictionaryIterator* iter;
  app_message_outbox_begin(&iter);
  
  // Average samples for smoother measurement, so we're sampling at 100hz but transmitting smoother
  AccelData avg_data = avg(data, num_samples);

  uint8_t sample_size = 3 * sizeof(int16_t)      // x y z component
			 + 1 * sizeof(char)      // vibration on / off
			 + 1 * sizeof(uint64_t); // timestamp
  uint8_t* bytes = malloc(sample_size);

  // pack binary data for sending
  int16_t* x_ptr = (int16_t*)&bytes[0];
  *x_ptr = avg_data.x;
  int16_t* y_ptr = (int16_t*)&bytes[2];
  *y_ptr = avg_data.y;
  int16_t* z_ptr = (int16_t*)&bytes[4];
  *z_ptr = avg_data.z;

  bytes[6] = data->did_vibrate ? 1 : 0;

  uint64_t* ts_ptr = (uint64_t*)&bytes[7];
  *ts_ptr = data->timestamp;

  dict_write_data(iter, KEY_ACCEL_DATA, bytes, sample_size);

  app_message_outbox_send();

  free(bytes);
}

static void window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);

  s_text_layer = text_layer_create(GRect(0, 72, bounds.size.w, 20));
  text_layer_set_text(s_text_layer, "Sampling");
  text_layer_set_text_alignment(s_text_layer, GTextAlignmentCenter);
  layer_add_child(window_layer, text_layer_get_layer(s_text_layer));
}

static void window_unload(Window *window) {
  text_layer_destroy(s_text_layer);
}

static void init(void) {
  s_window = window_create();
  window_set_window_handlers(s_window, (WindowHandlers) {
    .load = window_load,
    .unload = window_unload,
  });
  const bool animated = true;
  window_stack_push(s_window, animated);

  // Register accelerometer handling
  accel_service_set_sampling_rate(SAMPLE_RATE);
  accel_data_service_subscribe(SAMPLE_BATCH_SIZE, accel_data_handler);

  // Register communication
  app_comm_set_sniff_interval(SNIFF_INTERVAL_REDUCED);
  app_message_open(INBOX_SIZE , OUTBOX_SIZE);
}

static void deinit(void) {
  window_destroy(s_window);
  accel_data_service_unsubscribe();
  app_comm_set_sniff_interval(SNIFF_INTERVAL_NORMAL);
}

int main(void) {
  init();

  app_event_loop();

  deinit();
}
