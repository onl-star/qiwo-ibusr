ifeq (${PREFIX},)
	PREFIX=/usr
endif
sharedir = $(PREFIX)/share
libexecdir = $(PREFIX)/lib

ifeq (${builddir},)
	builddir=build
endif

ifeq ($(shell id -u),0)
	SUDO =
else
	SUDO ?= sudo
endif

all: ibus-engine-rime

# ── Auto-setup: install missing system deps ────────────────────

setup:
	@echo "*** Checking system dependencies..."
	@for pkg in ibus-1.0 librime libnotify gtk+-3.0 libsecret-1; do \
		if ! pkg-config --exists $$pkg 2>/dev/null; then \
			echo "--> $$pkg missing, installing..."; \
			if   command -v apt     >/dev/null; then $(SUDO) apt install -y ibus libibus-1.0-dev librime-dev librime-data libnotify-dev libgtk-3-dev libsecret-1-dev cmake gcc pkg-config python3; break; \
			elif command -v dnf     >/dev/null; then $(SUDO) dnf install -y ibus ibus-devel librime librime-devel rime-data libnotify-devel gtk3-devel libsecret-devel cmake gcc pkg-config python3; break; \
			elif command -v pacman  >/dev/null; then $(SUDO) pacman -S --needed --noconfirm ibus librime rime-data libnotify gtk3 libsecret cmake gcc pkg-config python; break; \
			elif command -v zypper  >/dev/null; then $(SUDO) zypper install -y ibus ibus-devel librime-devel rime-data libnotify-devel gtk3-devel libsecret-devel cmake gcc pkg-config python3; break; \
			else echo "ERROR: unknown package manager, install ibus/librime/libnotify/gtk3/libsecret dev packages manually"; exit 1; \
			fi; \
		fi; \
	done
	@echo "--> All system dependencies present."
	@if [ -f .gitmodules ] || [ -d .git ]; then \
		git submodule update --init --recursive 2>/dev/null || true; \
	fi

ibus-engine-rime: setup
	mkdir -p $(builddir)
	(cd $(builddir); cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_DATADIR=$(sharedir) -DCMAKE_INSTALL_LIBEXECDIR=$(libexecdir) .. && make)
	@echo ':)'

ibus-engine-rime-static: setup
	mkdir -p $(builddir)
	(cd $(builddir); cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_DATADIR=$(sharedir) -DCMAKE_INSTALL_LIBEXECDIR=$(libexecdir) -DBUILD_STATIC=ON .. && make)
	@echo ':)'

install:
	(cd $(builddir); make install)

uninstall:
	(cd $(builddir); xargs rm < install_manifest.txt)

clean:
	if  [ -e $(builddir) ]; then rm -R $(builddir); fi
