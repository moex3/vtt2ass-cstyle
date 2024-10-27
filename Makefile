CFLAGS += $(shell pkg-config --cflags harfbuzz)
LIBS += $(shell pkg-config --libs harfbuzz)
CFLAGS += $(shell pkg-config --cflags freetype2)
LIBS += $(shell pkg-config --libs freetype2)
LIBS += -lm

INCLUDES = subm/argparse

v2a: src/*.c subm/argparse/argparse.c
	${CC} $^ ${CFLAGS} -I${INCLUDES} -ggdb -std=gnu11 -o $@ ${LIBS} -fsanitize=address -fsanitize=leak -fsanitize=undefined

r: v2a
	./v2a

g: v2a
	gdb ./v2a

clean:
	-rm -- v2a

re: clean v2a
