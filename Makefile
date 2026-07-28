# SPDX-License-Identifier: MIT

TARGET := wii-fi-tester
APP_DIR := dist/apps/wii-fi-tester
SOURCES := main probe diagnosis presentation report
OBJECTS := $(addsuffix .o,$(SOURCES))
DEPS := $(OBJECTS:.o=.d)
C_SOURCE_FILES := $(addprefix source/,$(addsuffix .c,$(SOURCES)))
FORMAT_FILES := $(wildcard source/*.c source/*.h)

include $(DEVKITPPC)/wii_rules

LD := $(CC)

CFLAGS := -O2 -g -Wall -Wextra -Werror -Wformat=2 -Wshadow \
	-Wconversion -Wsign-conversion -Wnull-dereference -std=gnu11 \
	$(MACHDEP) -I$(LIBOGC_INC)
LDFLAGS := $(MACHDEP) -Wl,-Map,$(TARGET).map
LIBPATHS := -L$(LIBOGC_LIB)
LIBS := -lfat -lwiiuse -lbte -logc -lm

.PHONY: all check clean format package

all: $(TARGET).dol

$(TARGET).elf: $(OBJECTS)

%.o: source/%.c
	$(CC) -MMD -MP -MF $(@:.o=.d) $(CFLAGS) -c $< -o $@

package: $(TARGET).dol
	mkdir -p $(APP_DIR)
	cp $(TARGET).dol $(APP_DIR)/boot.dol
	cp app/meta.xml $(APP_DIR)/meta.xml
	cp LICENSE THIRD_PARTY_NOTICES.md $(APP_DIR)/

check: all
	clang-format --dry-run --Werror $(FORMAT_FILES)
	$(CC) -fsyntax-only -fanalyzer $(CFLAGS) $(C_SOURCE_FILES)

format:
	clang-format -i $(FORMAT_FILES)

clean:
	rm -f $(OBJECTS) $(DEPS) $(TARGET).elf $(TARGET).dol $(TARGET).map
	rm -rf dist

-include $(DEPS)
