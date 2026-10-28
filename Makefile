# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Marcel Berghaus (Gigoslav) <berghaus.marcel@gmail.com>
# Standard Linux/FHS Makefile for rog-power-manager

pkgname := rog-power-manager
VERSION := 0.3.0

PREFIX ?= /usr
BINDIR ?= $(PREFIX)/bin
UDEVDIR ?= $(PREFIX)/lib/udev/rules.d
TMPFILESDIR ?= $(PREFIX)/lib/tmpfiles.d
SYSTEMD_USER_DIR ?= $(PREFIX)/lib/systemd/user
APPDIR ?= $(PREFIX)/share/applications
DOCDIR ?= $(PREFIX)/share/doc/$(pkgname)
LICENSEDIR ?= $(PREFIX)/share/licenses/$(pkgname)

.PHONY: all install uninstall check

all:
	@echo "$(pkgname) v$(VERSION) - Run 'make install DESTDIR=...' or './install.sh'"

check:
	python3 -m py_compile rog-power-manager.py
	bash -n rog-power-helper
	bash -n install.sh

install:
	install -Dm755 rog-power-manager.py "$(DESTDIR)$(BINDIR)/rog-power-manager"
	ln -sf rog-power-manager "$(DESTDIR)$(BINDIR)/rog-auto-power"
	ln -sf rog-power-manager "$(DESTDIR)$(BINDIR)/auto-power-profile.py"
	install -Dm755 rog-power-helper "$(DESTDIR)$(BINDIR)/rog-power-helper"
	ln -sf rog-power-helper "$(DESTDIR)$(BINDIR)/rog-gpu-mux-helper"
	install -Dm644 99-rog-power-manager.rules "$(DESTDIR)$(UDEVDIR)/99-rog-power-manager.rules"
	install -Dm644 rog-power-manager.conf "$(DESTDIR)$(TMPFILESDIR)/rog-power-manager.conf"
	sed 's|ExecStart=%h/.local/bin/rog-power-manager|ExecStart=$(BINDIR)/rog-power-manager|' rog-power-manager.service > rog-power-manager.service.pkg
	install -Dm644 rog-power-manager.service.pkg "$(DESTDIR)$(SYSTEMD_USER_DIR)/rog-power-manager.service"
	rm -f rog-power-manager.service.pkg
	install -Dm644 rog-power-manager.desktop "$(DESTDIR)$(APPDIR)/rog-power-manager.desktop"
	install -Dm644 LICENSE "$(DESTDIR)$(LICENSEDIR)/LICENSE"
	install -Dm644 NOTICE "$(DESTDIR)$(LICENSEDIR)/NOTICE"
	install -Dm644 AUTHORS "$(DESTDIR)$(DOCDIR)/AUTHORS"
	install -Dm644 CHANGELOG.md "$(DESTDIR)$(DOCDIR)/CHANGELOG.md"
	install -Dm644 README.md "$(DESTDIR)$(DOCDIR)/README.md"
	install -Dm644 README.de.md "$(DESTDIR)$(DOCDIR)/README.de.md"

uninstall:
	rm -f "$(DESTDIR)$(BINDIR)/rog-power-manager"
	rm -f "$(DESTDIR)$(BINDIR)/rog-auto-power"
	rm -f "$(DESTDIR)$(BINDIR)/auto-power-profile.py"
	rm -f "$(DESTDIR)$(BINDIR)/rog-power-helper"
	rm -f "$(DESTDIR)$(BINDIR)/rog-gpu-mux-helper"
	rm -f "$(DESTDIR)$(UDEVDIR)/99-rog-power-manager.rules"
	rm -f "$(DESTDIR)$(TMPFILESDIR)/rog-power-manager.conf"
	rm -f "$(DESTDIR)$(SYSTEMD_USER_DIR)/rog-power-manager.service"
	rm -f "$(DESTDIR)$(APPDIR)/rog-power-manager.desktop"
	rm -rf "$(DESTDIR)$(LICENSEDIR)"
	rm -rf "$(DESTDIR)$(DOCDIR)"
