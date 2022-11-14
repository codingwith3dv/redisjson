cmake_minimum_required(VERSION 3.10)

project(redisjson)

set(SOURCES
  redisjson.c
  value.c
  rdbValue.c
  jsonToValue.c
)

add_library(redisjson SHARED ${SOURCES})
