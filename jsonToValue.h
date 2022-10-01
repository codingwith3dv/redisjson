#pragma once

#include "value.h"
#include "redismodule.h"

JsonValue* parseJson(
  RedisModuleCtx* ctx,
  const char* json
);
