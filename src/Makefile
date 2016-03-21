CC := g++
CFLAGS := -Wall -m64
CLIB := -lpthread -lrt

OBJECTS = Lock.o \
          UserPriorities.o \
          QueueStatistics.o \
          Parser.o

OBJ = $(patsubst %, objects/%, $(OBJECTS))

.PHONY: all prepare clean main main_local

all: prepare $(OBJ) main main_local

prepare:
	@mkdir -p objects

$(OBJ): objects/%.o : %.cc %.hh
	$(CC) $(COPTS) -c $< -lcppunit -o $@

main: $(OBJ)
	$(CC) $(CFLAGS) $@.cc $(OBJ) $(CLIB) -o scheduler

main_local: $(OBJ)
	$(CC) $(CFLAGS) -Wno-unused-variable $@.cc $(OBJ) $(CLIB) -o exec_local

clean:
	rm -rf objects/ scheduler
