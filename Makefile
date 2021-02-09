
.PHONY: clean

# ============================================================================ #

CC := /grid/common/pkgs/gcc/v9.3.0p1/bin/gcc

DEBUG_STYLE   := gdb
DEBUG_CFLAGS  := -g${DEBUG_STYLE}
DEBUG_LDFLAGS := -g${DEBUG_STYLE}

LIB_CFLAGS  := -fPIC ${DEBUG_CFLAGS}
LIB_LDFLAGS := -shared ${DEBUG_LDFLAGS}

CFLAGS += --std=c18


# ---------------------------------------------------------------------------- #

%_sh.o: CFLAGS += ${LIB_CFLAGS}
%_sh.o: %.c %.h
	${CC} ${CFLAGS} -c $< -o $@;


lib%.so: LDFLAGS += ${LIB_LDFLAGS}
lib%.so: %_sh.o
	${CC} ${LDFLAGS} -Wl,-soname,$@ -o $@ $^;


# ---------------------------------------------------------------------------- #

libelfutils.so: LDFLAGS += ${LIB_LDFLAGS}
libelfutils.so: dev_lst_sh.o util_sh.o
	${CC} ${LDFLAGS} -Wl,-soname,$@ -o $@ $^;


# ---------------------------------------------------------------------------- #

clean:
	rm -rf *.o *.so;


# ============================================================================ #
