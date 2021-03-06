#ifndef _globals_h
#define _globals_h

#define RECONNECT_KEY 0
#define REQUEST_CALENDAR_KEY 1
#define CLOCK_STYLE_KEY 2
#define CALENDAR_RESPONSE_KEY 3
#define REQUEST_BATTERY_KEY 8
#define BATTERY_RESPONSE_KEY 9
#define REQUEST_MUSIC_KEY 25
#define MUSIC_RESPONSE_KEY 26             // v1.2

#define CLOCK_STYLE_12H 1
#define CLOCK_STYLE_24H 2



typedef struct {
  uint8_t index;
  char title[21];
  bool has_location;
  char location[21];
  bool all_day;
  char start_date[18];
  int32_t alarms[2];
} Event;

typedef struct {
  uint8_t state;
  int8_t level;
} BatteryStatus;

typedef struct {
  char artist[42];
  char title[42];
  char album[21];
} Music;


#endif




