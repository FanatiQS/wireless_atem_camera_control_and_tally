all: release

main: server.c udp.c timer.c cache.c relay.c ../src/atem.c

ERROR_FLAGS       = -Wall \
					-pedantic
MORE_ERROR_FLAGS  = -Weverything \
					-Wno-pedantic \
					-Wno-implicit-int-conversion \
					-Wno-sign-conversion \
					-Wno-missing-prototypes \
					-Wno-unused-parameter \
					-Wno-\#warnings \
					-Wno-cast-align \
					-Wno-padded
RELEASE_FLAGS     = -O2
DEBUG_FLAGS       = -DDEBUG \
					-g \
					$(ERROR_FLAGS) \
					$(MORE_ERROR_FLAGS)

release: CFLAGS = $(RELEASE_FLAGS)
release: main

debug: CFLAGS = $(DEBUG_FLAGS)
debug: main

clean:
	$(RM) main
