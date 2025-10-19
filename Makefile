.PHONY: all build install clean config

# Default install prefix
PREFIX ?= $(HOME)/.local

all: build

config:
	cmake -B build -DCMAKE_INSTALL_PREFIX=$(PREFIX)

build: config
	cmake --build build

install: build
	cmake --install build

clean:
	rm -rf build

rebuild: clean build