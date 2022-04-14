# audiospy Makefile

AUS_DIR := .
FFBASE_DIR := ../ffbase
FFOS_DIR := ../ffos
FFAUDIO_DIR := ../ffaudio

include $(FFBASE_DIR)/test/makeconf

CFLAGS := -Wall -Wextra -Wno-unused-parameter -Wno-sign-compare
CFLAGS += -I$(AUS_DIR)/src -I$(FFOS_DIR) -I$(FFBASE_DIR) -I$(FFAUDIO_DIR)
LINKFLAGS :=
ifeq "$(OPT)" "0"
	CFLAGS += -DFF_DEBUG -O0 -g
else
	CFLAGS += -O3 -fno-strict-aliasing
	LINKFLAGS += -s
endif

CFLAGS += -DFFAUDIO_INTERFACE_DEFAULT_PTR="&ff$(FFAUDIO_API)"

ifeq "$(OS)" "windows"
	LINKFLAGS += -lws2_32
endif

ifeq "$(FFAUDIO_API)" "alsa"
	LINKFLAGS += -lasound
else ifeq "$(FFAUDIO_API)" "pulse"
	LINKFLAGS += -lpulse
else ifeq "$(FFAUDIO_API)" "jack"
	LINKFLAGS += -ljack
else ifeq "$(FFAUDIO_API)" "wasapi"
	LINKFLAGS += -lole32
else ifeq "$(FFAUDIO_API)" "dsound"
	LINKFLAGS += -ldsound -ldxguid
else ifeq "$(FFAUDIO_API)" "coreaudio"
	LINKFLAGS += -framework CoreFoundation -framework CoreAudio
else ifeq "$(FFAUDIO_API)" "oss"
	LINKFLAGS += -lm
endif

all: audiospy_sv audiospy_cl

DEPS := $(wildcard $(AUS_DIR)/src/*.h) \
	$(wildcard $(FFBASE_DIR)/ffbase/*.h) \
	$(wildcard $(FFOS_DIR)/FFOS/*.h) \
	$(wildcard $(FFAUDIO_DIR)/ffaudio/*.h) \
	$(AUS_DIR)/Makefile

%.o: $(AUS_DIR)/src/%.c $(DEPS)
	$(C) $(CFLAGS) -DFFBASE_HAVE_FFERR_STR $< -o $@

%.o: $(FFAUDIO_DIR)/ffaudio/%.c
	$(C) $(CFLAGS) $< -o $@

audiospy_sv: \
		$(FFAUDIO_API).o \
		server.o
	$(LINK) $+ $(LINKFLAGS) $(LINK_PTHREAD) -o $@

audiospy_cl: \
		$(FFAUDIO_API).o \
		client.o
	$(LINK) $+ $(LINKFLAGS) $(LINK_PTHREAD) -o $@

clean:
	rm -fv audiospy_sv audiospy_cl *.o
