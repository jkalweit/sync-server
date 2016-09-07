SOURCES.c= utils.c websockets.c webserver.c server.c
INCLUDES= 
CFLAGS= -I. -I/usr/include/openssl -W -Wshadow

SLIBS= -lcrypto -lssl
PROGRAM= server

OBJECTS= $(SOURCES.c:.c=.o)

.KEEP_STATE:

debug := CFLAGS= -g

all debug: $(PROGRAM)

$(PROGRAM): .FORCE $(INCLUDES) $(OBJECTS)
	$(LINK.c) -o $@ $(OBJECTS) $(SLIBS)

clean:
	rm -f $(PROGRAM) $(OBJECTS)

.PHONY: .FORCE
.FORCE:

