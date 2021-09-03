
CFLAGS = `pkg-config --cflags gtk+-2.0` -Wall -g
LIBS =  `pkg-config --libs gtk+-2.0` -lm

CC = gcc
RM = /bin/rm -rf

OBJECTS = oos.o rng_analyse.o rng_model.o \
	lib_error.o lib_file.o lib_string.o lib_math.o \
	pl_misc.o pl_parse.o pl_scan.o pl_operators.o pl_print.o

XOBJECTS = xrng.o x_temp_graph.o x_widgets.o x_diagram.o x_browser.o lib_cairox.o

all:
	make rng
	make xrng
#	make rng_client

rng:	$(OBJECTS) rng.o
	$(RM) $@
	$(CC) -o $@ $(CFLAGS) $(LDFLAGS) $(OBJECTS) rng.o $(LIBS)

rng_scan:
	make rng_scan_generate
	make rng_scan_extract

rng_fit_ga:	$(OBJECTS) rng_fit_ga.o
	$(RM) $@
	$(CC) -o $@ $(CFLAGS) $(LDFLAGS) $(OBJECTS) rng_fit_ga.o $(LIBS)

rng_fit_gd:	$(OBJECTS) rng_fit_gd.o
	$(RM) $@
	$(CC) -o $@ $(CFLAGS) $(LDFLAGS) $(OBJECTS) rng_fit_gd.o $(LIBS)

rng_scan_generate:	$(OBJECTS) rng_scan_generate.o
	$(RM) $@
	$(CC) -o $@ $(CFLAGS) $(LDFLAGS) $(OBJECTS) rng_scan_generate.o $(LIBS)

rng_scan_extract:	$(OBJECTS) rng_scan_extract.o
	$(RM) $@
	$(CC) -o $@ $(CFLAGS) $(LDFLAGS) $(OBJECTS) rng_scan_extract.o $(LIBS)

xrng:	$(OBJECTS) $(XOBJECTS)
	$(RM) $@
	$(CC) -o $@ $(CFLAGS) $(LDFLAGS) $(OBJECTS) $(XOBJECTS) $(LIBS)

rng_client:	$(OBJECTS) rng_client.o /usr/local/lib/libexpress.a
	$(RM) $@
	$(CC) -o $@ $(CFLAGS) $(LDFLAGS) $(OBJECTS) rng_client.o $(LIBS) -lexpress

towse:	towse.o lib_math.o
	$(CC) -o $@ $(CFLAGS) $(LDFLAGS) towse.o lib_math.o -lm

tar:
	make clean
	tar cvfz ~/Dropbox/Source/rng_20130417.tgz *

oos_test:	$(OBJECTS) oos_test.o
	$(RM) $@
	$(CC) -o $@ $(CFLAGS) $(LDFLAGS) $(OBJECTS) oos_test.o $(LIBS)

rng_fit.o: rng_flags.h

clean:
	$(RM) *.o *~ core tmp.* */*~ NONE none
	$(RM) *.tgz xrng rng rng_client
	$(RM) rng_fit_ga rng_fit_gd rng_fit
	$(RM) oos_test towse
	$(RM) rng_scan rng_scan_generate rng_scan_extract


