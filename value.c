#include "value.h"
#include "redismodule.h"

JsonValue* allocNumber(long long num) {
  JsonValue* value = RedisModule_Calloc(1, sizeof(JsonValue));
  value->value.number = num;
  value->type = NUMBER;
  return value;
}

JsonValue* allocObject(size_t size) {
  JsonValue* value = RedisModule_Calloc(1, sizeof(JsonValue));
  value->value.object.elements = RedisModule_Calloc(size, sizeof(JsonKeyVal*));
  value->value.object.size = size;
  value->type = OBJECT;
  return value;
}
