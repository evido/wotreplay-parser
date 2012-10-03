CC=g++
#CC=clang
CFLAGS=-Iext/jsoncpp/include -Isrc -std=c++0x -m64
LDFLAGS=-lcrypto -ltbb -ltbbmalloc -lpng -lboost_filesystem -lboost_system
OBJS=obj/ext/jsoncpp/src/jsoncpp.o obj/src/packet.o obj/src/parser.o obj/src/main.o

default: all
	echo Built $(OBJS)

obj:
	@mkdir -p obj

obj/%.o: %.cpp
	@echo Building $@ from $<
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c -o $@ $<

all: obj $(OBJS)
	$(CC) $(CFLAGS) -o obj/wotreplay-parser $(OBJS) $(LDFLAGS)

clean:
	@rm -rf obj
