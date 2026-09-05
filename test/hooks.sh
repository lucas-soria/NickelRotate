#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
mkdir -p tmp
moc test/hooks_test.cc -o tmp/hooks_test.moc
c++ -std=c++11 -Wall -Wextra -Werror -Wno-missing-field-initializers \
    -fPIC -pthread -fsanitize=address,undefined -g -I NickelHook -I tmp \
    $(pkg-config --cflags Qt5Core Qt5Gui Qt5Widgets) \
    src/policy.cc test/hooks_test.cc -o tmp/hooks_test \
    $(pkg-config --libs Qt5Core Qt5Gui Qt5Widgets)
QT_QPA_PLATFORM=minimal ./tmp/hooks_test
