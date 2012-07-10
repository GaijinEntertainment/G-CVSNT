# At home I run Linux, and this Makefile (along with test.cpp) is
# for writing and debugging wxWindows dialogs under Unix.  They
# tend to work fine with Windows when you've finished!

CC = gcc

PROGRAM = ProgressDialog

OBJECTS = $(PROGRAM).o test.o

# implementation

.SUFFIXES:	.o .cpp

.cpp.o :
	$(CC) -c `wx-config --cflags` -o $@ $<

all:    $(PROGRAM)

$(PROGRAM):	$(OBJECTS)
	$(CC) -o $(PROGRAM) $(OBJECTS) `wx-config --libs`

clean:
	rm -f *.o $(PROGRAM)

