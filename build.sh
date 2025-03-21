#!/bin/sh
cmake --build build && gdb -q -batch -ex run build/lib/spock -ex bt 