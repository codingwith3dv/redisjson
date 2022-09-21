cmake_minimum_required(VERSION 3.10)

project(rejsonc)

set(SOURCES
  rejsonc.c
  value.c
  rdbValue.c
)

add_library(rejsonc SHARED ${SOURCES})
