#pragma once

#include "redismodule.h"
#include <stdint.h>

typedef enum {
  OBJECT,
  NUMBER
} JsonValueType;

struct JsonValue;

typedef struct {
  const char* key;
  struct JsonValue* value;
} JsonKeyVal;

struct JsonObject {
  JsonKeyVal** elements;
  uint64_t size;
};

typedef struct JsonValue {
  union {
    long long number;
    struct JsonObject object;
  } value;
  JsonValueType type;
} JsonValue;

typedef struct {
  JsonValue* rootJson;
} RedisJsonValue;

JsonValue* allocNumber(long long num);
JsonValue* allocObject(uint64_t size);

void JsonTypeRdbSaveImpl(RedisModuleIO* rdb, JsonValue* value);
void JsonTypeRdbLoadImpl(RedisModuleIO* rdb, JsonValue* value);
void JsonTypeFreeImpl(JsonValue* value);
