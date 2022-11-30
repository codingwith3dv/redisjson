#pragma once

#include "redismodule.h"
#include "value.h"

typedef struct {
  void* data;
  size_t cap;
  size_t len;
  size_t elemSize;
} Vector;

JsonValue** evalPath(
  RedisModuleCtx* ctx,
  JsonValue* value,
  RedisModuleString* path,
  size_t* outLen
);
