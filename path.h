#pragma once

#include "redismodule.h"
#include "value.h"

JsonValue** evalPath(
  RedisModuleCtx* ctx,
  JsonValue* value,
  RedisModuleString* path
);
