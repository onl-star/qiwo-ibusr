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
	@bash scripts/setup-linux-deps.sh --need-cargo --need-git

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
