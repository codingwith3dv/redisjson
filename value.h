#pragma once

#include "redismodule.h"
#include <stdint.h>

typedef enum {
  OBJECT,
  ARRAY,
  NUMBER,
  STRING
} JsonValueType;

struct JsonValue;

typedef struct {
  const char* key;
  struct JsonValue* value;
} JsonKeyVal;

struct JsonObject {
  JsonKeyVal** elements;
  size_t size;
};

typedef struct {
  struct JsonValue** array;
  size_t size;
} JsonArray;

typedef struct {
  const char* data;
  size_t size;
} JsonString;

typedef struct JsonValue {
  union {
    long long number;
    JsonString string;
    struct JsonObject object;
    JsonArray array;
  } value;
  JsonValueType type;
} JsonValue;

typedef struct {
  JsonValue* rootJson;
} RedisJsonValue;

JsonValue* allocNumber(long long num);
JsonValue* allocObject(size_t size);

void JsonTypeRdbSaveImpl(RedisModuleIO* rdb, JsonValue* value);
void JsonTypeRdbLoadImpl(RedisModuleIO* rdb, JsonValue* value);
void JsonTypeFreeImpl(JsonValue* value);
