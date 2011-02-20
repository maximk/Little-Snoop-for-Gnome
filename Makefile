
.PHONY: default

default: little-snoop

little-snoop: little-snoop.c
	gcc -o $@ $@.c `pkg-config --cflags --libs gtk+-2.0 libsoup-2.4`

