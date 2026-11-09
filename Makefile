# make           build build/buttons with the local compiler
# make aarch64   static aarch64 build in build/aarch64/ (needs aarch64-linux-gnu-gcc)
# make release   dist/handheld-directinput-aarch64.tar.gz with the prebuilt binary
VERSION ?= dev
CFLAGS ?= -O2 -Wall -Wextra
AARCH64_CC ?= aarch64-linux-gnu-gcc
FILES = LICENSE README.md Makefile install.sh handheld-directinput src udev systemd

all: build/buttons

build/buttons: src/buttons.c
	@mkdir -p build
	$(CC) $(CFLAGS) -o $@ $<

aarch64: build/aarch64/buttons

build/aarch64/buttons: src/buttons.c
	@mkdir -p build/aarch64
	$(AARCH64_CC) $(CFLAGS) -static -o $@ $<

release: build/aarch64/buttons
	rm -rf dist
	mkdir -p dist/handheld-directinput/bin
	cp -r $(FILES) dist/handheld-directinput/
	cp build/aarch64/buttons dist/handheld-directinput/bin/
	echo $(VERSION) > dist/handheld-directinput/VERSION
	tar -C dist --owner=0 --group=0 -czf dist/handheld-directinput-aarch64.tar.gz handheld-directinput
	cd dist && sha256sum handheld-directinput-aarch64.tar.gz > handheld-directinput-aarch64.tar.gz.sha256

clean:
	rm -rf build dist

.PHONY: all aarch64 release clean
