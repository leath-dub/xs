include config.mk

xs: xs.o
	$(CC) -o $@ xs.o $(XSLDFLAGS)
