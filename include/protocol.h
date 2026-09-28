#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stddef.h>
#include <stdint.h>

typedef enum
{
  MESSAGE_VISION,
  MESSAGE_TRAFFIC_COUNT
} MessageType;

typedef struct
{
  int64_t frame_id;
  int64_t timestamp_ms;
  int64_t class_id;
  char* class_name;
  double confidence;
  int64_t bbox_x;
  int64_t bbox_y;
  int64_t bbox_width;
  int64_t bbox_height;
} VisionData;

typedef struct
{
  int64_t period_start_ms;
  int64_t period_end_ms;
  int64_t car_count;
  int64_t motorcycle_count;
  int64_t bus_count;
  int64_t truck_count;
} TrafficCountData;

typedef struct
{
  MessageType type;
  char* message_id;
  char* device_id;
  VisionData vision;
  TrafficCountData traffic_count;
} ProtocolMessage;

const char* protocol_parse(const char* payload, size_t length,
                           ProtocolMessage* message);
char* protocol_make_ack(const char* message_id, const char* error_code);
void protocol_free(ProtocolMessage* message);

#endif
