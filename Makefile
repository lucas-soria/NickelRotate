include NickelHook/NickelHook.mk

override LIBRARY := src/libnickelrotate.so
override PKGCONF += Qt5Widgets
override SOURCES += src/policy.cc src/nickelrotate.cc
override CFLAGS += -Wall -Wextra -Werror -fvisibility=hidden
override CXXFLAGS += -Wall -Wextra -Werror -Wno-missing-field-initializers -fvisibility=hidden -fvisibility-inlines-hidden
override KOBOROOT += res/doc:/mnt/onboard/.adds/nickel-rotate/doc res/uninstall:/mnt/onboard/.adds/nickel-rotate/uninstall
VERSION ?= 0.1.9-dev

override SKIPCONFIGURE += strip test
strip:
	$(STRIP) --strip-unneeded $(LIBRARY)

test:
	mkdir -p tmp
	$(HOST_CXX) -std=c++11 -Wall -Wextra -Werror -fsanitize=address,undefined -g src/policy.cc test/policy_test.cc -o tmp/policy_test
	./tmp/policy_test

HOST_CXX ?= c++
.PHONY: strip test

include NickelHook/NickelHook.mk
