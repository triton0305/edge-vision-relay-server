#include "protocol.h"

#include <cjson/cJSON.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

static const char* get_string(const cJSON* object, const char* name)
{
  const cJSON* item = cJSON_GetObjectItemCaseSensitive(object, name);

  if (!cJSON_IsString(item) || !item->valuestring || !item->valuestring[0])
  {
    return NULL;
  }

  return item->valuestring;
}

static int get_integer(const cJSON* object, const char* name, int64_t* value)
{
  const cJSON* item = cJSON_GetObjectItemCaseSensitive(object, name);

  if (!cJSON_IsNumber(item) || !isfinite(item->valuedouble) ||
      item->valuedouble < 0 || item->valuedouble > 9007199254740991.0 ||
      (double)(int64_t)item->valuedouble != item->valuedouble)
  {
    return -1;
  }

  *value = (int64_t)item->valuedouble;
  return 0;
}

static char* copy_string(const char* source)
{
  size_t length = strlen(source) + 1;
  char* copy = malloc(length);

  if (copy)
  {
    memcpy(copy, source, length);
  }

  return copy;
}

static const char* parse_vision(const cJSON* data, VisionData* vision)
{
  const cJSON* bbox = cJSON_GetObjectItemCaseSensitive(data, "bbox");
  const cJSON* confidence = cJSON_GetObjectItemCaseSensitive(data, "confidence");
  const char* name = get_string(data, "class_name");

  if (get_integer(data, "frame_id", &vision->frame_id) ||
      get_integer(data, "timestamp_ms", &vision->timestamp_ms) ||
      get_integer(data, "class_id", &vision->class_id) || !name ||
      (strcmp(name, "car") && strcmp(name, "motorcycle") &&
       strcmp(name, "bus") && strcmp(name, "truck")) ||
      !cJSON_IsNumber(confidence) || !isfinite(confidence->valuedouble) ||
      confidence->valuedouble < 0 || confidence->valuedouble > 1 ||
      !cJSON_IsObject(bbox) || get_integer(bbox, "x", &vision->bbox_x) ||
      get_integer(bbox, "y", &vision->bbox_y) ||
      get_integer(bbox, "width", &vision->bbox_width) ||
      get_integer(bbox, "height", &vision->bbox_height) ||
      !vision->bbox_width || !vision->bbox_height)
  {
    return "invalid_data";
  }

  vision->class_name = copy_string(name);

  if (!vision->class_name)
  {
    return "invalid_data";
  }

  vision->confidence = confidence->valuedouble;
  return NULL;
}

const char* protocol_parse(const char* payload, size_t length,
                           ProtocolMessage* message)
{
  memset(message, 0, sizeof(*message));
  cJSON* root = memchr(payload, 0, length) == NULL ?
    cJSON_ParseWithLengthOpts(payload, length + 1, NULL, 1) : NULL;
  const char* error = NULL;

  if (!cJSON_IsObject(root))
  {
    error = "invalid_json";
  }
  else
  {
    const char* id = get_string(root, "message_id");
    const char* type = get_string(root, "type");
    const char* device = get_string(root, "device_id");
    const cJSON* version = cJSON_GetObjectItemCaseSensitive(root, "version");
    const cJSON* data = cJSON_GetObjectItemCaseSensitive(root, "data");

    if (id)
    {
      message->message_id = copy_string(id);
    }

    if (!id || !message->message_id || !type || !device ||
        !cJSON_IsNumber(version) || version->valuedouble != 1 ||
        !cJSON_IsObject(data))
    {
      error = "invalid_message";
    }
    else
    {
      message->device_id = copy_string(device);

      if (!message->device_id)
      {
        error = "invalid_message";
      }
      else if (strcmp(type, "vision") == 0)
      {
        message->type = MESSAGE_VISION;
        error = parse_vision(data, &message->vision);
      }
      else
      {
        error = "unsupported_type";
      }
    }
  }

  cJSON_Delete(root);
  return error;
}

char* protocol_make_ack(const char* message_id, const char* error_code)
{
  cJSON* ack = cJSON_CreateObject();

  if (!ack || !cJSON_AddNumberToObject(ack, "version", 1) ||
      !cJSON_AddStringToObject(ack, "type", "ack") ||
      !cJSON_AddStringToObject(ack, "message_id", message_id) ||
      !cJSON_AddStringToObject(ack, "status", error_code ? "error" : "ok") ||
      (error_code && !cJSON_AddStringToObject(ack, "error_code", error_code)))
  {
    cJSON_Delete(ack);
    return NULL;
  }

  char* payload = cJSON_PrintUnformatted(ack);
  cJSON_Delete(ack);
  return payload;
}

void protocol_free(ProtocolMessage* message)
{
  free(message->message_id);
  free(message->device_id);
  free(message->vision.class_name);
}
