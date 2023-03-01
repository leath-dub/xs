# sss version
VERSION = 1.0.2-alpha

# paths
PREFIX = /usr/local
MANPREFIX = $(PREFIX)/share/man

PKG_CONFIG = pkg-config

# INCS = `$(PKG_CONFIG) --cflags libpng` \

LIBS = `$(PKG_CONFIG) --libs xcb-cursor` \
	   `$(PKG_CONFIG) --libs xcb-image` \
	   `$(PKG_CONFIG) --libs xcb` \
	   `$(PKG_CONFIG) --libs xcb-shm` \
	   `$(PKG_CONFIG) --libs xcb-randr`

# flags
XSCPPFLAGS = -DVERSION=\"$(VERSION)\" -D_XOPEN_SOURCE=600
XSCFLAGS = $(INCS) $(XSCPPFLAGS) $(CPPFLAGS) $(CFLAGS)
XSLDFLAGS = $(LIBS) $(LDFLAGS)
