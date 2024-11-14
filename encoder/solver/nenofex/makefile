MAJOR=1
MINOR=1
VERSION=$(MAJOR).$(MINOR)

TARGETS := main.o libnenofex.a

UNAME := $(shell uname)

ifeq ($(UNAME), Darwin)
# Mac OS X
SONAME=-install_name
TARGETS += libnenofex.$(VERSION).dylib
LIBS = -lpicosat
else
SONAME=-soname
TARGETS += libnenofex.so.$(VERSION)
LIBS = -lpicosat
endif

CFLAGS += -I../picosat
LFLAGS += -L../picosat

.SUFFIXES: .c .o .fpico

.c.fpico:
	$(CC) $(CFLAGS) -fPIC -c $< -o $@

.c.o:
	$(CC) $(CFLAGS) -c $< -o $@

nenofex: $(TARGETS)
	$(CC) $(CFLAGS) -o nenofex main.o -L. -lnenofex $(LIBS)

main.o: main.c mem.h nenofex.h

stack.o: stack.c stack.h mem.h
stack.fpico: stack.c stack.h mem.h

queue.o: queue.c queue.h mem.h
queue.fpico: queue.c queue.h mem.h

mem.o: mem.c mem.h
mem.fpico: mem.c mem.h

atpg.o: atpg.c stack.h queue.h mem.h nenofex_types.h
atpg.fpico: atpg.c stack.h queue.h mem.h nenofex_types.h

nenofex.o: nenofex.c nenofex_types.h stack.h mem.h
nenofex.fpico: nenofex.c nenofex_types.h stack.h mem.h

libnenofex.a: nenofex.o stack.o queue.o mem.o atpg.o ../picosat/picosat.o
	ar rc $@ $^
	ranlib $@

libnenofex.so.$(VERSION): nenofex.fpico stack.fpico queue.fpico mem.fpico atpg.fpico
	$(CC) $(LFLAGS) -shared -Wl,$(SONAME),libnenofex.so.$(MAJOR) $^ -o $@ $(LIBS)

libnenofex.$(VERSION).dylib: nenofex.fpico stack.fpico queue.fpico mem.fpico atpg.fpico
	$(CC) $(LFLAGS) -shared -Wl,$(SONAME),libnenofex.$(MAJOR).dylib $^ -o $@ $(LIBS)

clean:
	rm -f *.fpico *.out *.gcda *.gcno *.gcov *.o *.a

cleanall:
	rm -f ./nenofex *.so.$(VERSION) *.dylib *.fpico *.out *.gcda *.gcno *.gcov *.o *.a
