#!/bin/sh
  cmake . -B build && 
  cmake --build build && 
  (cd build/lib && ./spock)