
.PHONY: all clean

all: test

MY_FLAGS := -Wall -Wextra -Wformat=2 -fno-omit-frame-pointer -Wformat=2 \
			-g -I. \
			-D_GNU_SOURCE -DHAVE_UCONTEXT_T -DBIT64 -D_AMD64_ \
			-DPAL_IMPLEMENTATION -DHAVE_GREGSET_T

CFLAGS += $(MY_FLAGS)

CXXFLAGS += $(MY_FLAGS) -std=c++11

ASFLAGS += $(MY_FLAGS)

OBJS := test.o context2.o context.o

HEADERS := asmconstants.h context.h pal_mstypes.h \
		   pal.h palinternal.h unixasmmacros.inc unixasmmacrosamd64.inc \
		   Makefile

test.o: $(HEADERS)
context.o: $(HEADERS)
context2.o: $(HEADERS)

test: $(OBJS)

clean:
	rm test $(OBJS)
