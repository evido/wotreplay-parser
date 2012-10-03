CC=g++
#CC=clang
CFLAGS=-Iext/jsoncpp/include -Isrc -std=c++0x
LDFLAGS=-ltbb -ltbbmalloc -lpng -lssl -lboost_filesystem -lboost_system
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
	$(CC) $(LDFLAGS) -o obj/wotreplay-parser $(OBJS)

clean:
	@rm -rf obj
