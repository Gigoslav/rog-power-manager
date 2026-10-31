# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Marcel Berghaus (Gigoslav) <berghaus.marcel@gmail.com>
# Standard Linux/FHS Makefile for rog-power-manager (Native C++ / Qt6)

pkgname := rog-power-manager
VERSION := 0.4.1

PREFIX ?= /usr
BINDIR ?= $(PREFIX)/bin
BUILD_DIR ?= build

.PHONY: all build install uninstall clean

all: build

build:
	mkdir -p $(BUILD_DIR)
	cd $(BUILD_DIR) && cmake -DCMAKE_INSTALL_PREFIX=$(PREFIX) -DCMAKE_BUILD_TYPE=Release .. && $(MAKE) -j$$(nproc)

install: build
	install -Dm755 $(BUILD_DIR)/rog-power-manager "$(DESTDIR)$(BINDIR)/rog-power-manager"
	ln -sf rog-power-manager "$(DESTDIR)$(BINDIR)/rog-auto-power"
	ln -sf rog-power-manager "$(DESTDIR)$(BINDIR)/auto-power-profile.py"
	install -Dm755 rog-power-helper "$(DESTDIR)$(BINDIR)/rog-power-helper"
	ln -sf rog-power-helper "$(DESTDIR)$(BINDIR)/rog-gpu-mux-helper"
	mkdir -p "$(DESTDIR)/usr/local/bin"
	ln -sf /usr/bin/rog-power-helper "$(DESTDIR)/usr/local/bin/rog-gpu-mux-helper"
	install -Dm644 99-rog-power-manager.rules "$(DESTDIR)$(PREFIX)/lib/udev/rules.d/99-rog-power-manager.rules"
	install -Dm644 rog-power-manager.conf "$(DESTDIR)$(PREFIX)/lib/tmpfiles.d/rog-power-manager.conf"
	install -Dm644 rog-power-manager.service "$(DESTDIR)$(PREFIX)/lib/systemd/user/rog-power-manager.service"
	install -Dm644 rog-power-manager.desktop "$(DESTDIR)$(PREFIX)/share/applications/rog-power-manager.desktop"
	install -Dm644 LICENSE "$(DESTDIR)$(PREFIX)/share/licenses/$(pkgname)/LICENSE"
	install -Dm644 NOTICE "$(DESTDIR)$(PREFIX)/share/licenses/$(pkgname)/NOTICE"
	install -Dm644 AUTHORS "$(DESTDIR)$(PREFIX)/share/doc/$(pkgname)/AUTHORS"
	install -Dm644 CHANGELOG.md "$(DESTDIR)$(PREFIX)/share/doc/$(pkgname)/CHANGELOG.md"
	install -Dm644 README.md "$(DESTDIR)$(PREFIX)/share/doc/$(pkgname)/README.md"
	install -Dm644 README.de.md "$(DESTDIR)$(PREFIX)/share/doc/$(pkgname)/README.de.md"

uninstall:
	rm -f "$(DESTDIR)$(BINDIR)/rog-power-manager"
	rm -f "$(DESTDIR)$(BINDIR)/rog-auto-power"
	rm -f "$(DESTDIR)$(BINDIR)/auto-power-profile.py"
	rm -f "$(DESTDIR)$(BINDIR)/rog-power-helper"
	rm -f "$(DESTDIR)$(BINDIR)/rog-gpu-mux-helper"
	rm -f "$(DESTDIR)/usr/local/bin/rog-gpu-mux-helper"
	rm -f "$(DESTDIR)$(PREFIX)/lib/udev/rules.d/99-rog-power-manager.rules"
	rm -f "$(DESTDIR)$(PREFIX)/lib/tmpfiles.d/rog-power-manager.conf"
	rm -f "$(DESTDIR)$(PREFIX)/lib/systemd/user/rog-power-manager.service"
	rm -f "$(DESTDIR)$(PREFIX)/share/applications/rog-power-manager.desktop"
	rm -rf "$(DESTDIR)$(PREFIX)/share/licenses/$(pkgname)"
	rm -rf "$(DESTDIR)$(PREFIX)/share/doc/$(pkgname)"

clean:
	rm -rf $(BUILD_DIR) pkg *.pkg.tar.* __pycache__
