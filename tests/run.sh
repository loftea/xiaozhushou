#!/bin/sh
set -eu
TEST_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
TEST_BUILD=$(mktemp -d)
trap 'rm -rf "$TEST_BUILD"' EXIT HUP INT TERM
"${CXX:-c++}" -std=c++11 -Wall -Wextra -Werror -pedantic \
  "$TEST_DIR/reminder_test.cpp" -o "$TEST_BUILD/reminder-test"
"$TEST_BUILD/reminder-test"
