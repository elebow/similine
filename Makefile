CC=gcc
LFLAGS=-Wall -l popt
CFLAGS=-Wall -Wextra -Wconversion -Wshadow -Wcast-qual -Wno-write-strings -std=c99
APP=similine

${APP}: ${APP}.o
	${CC} ${LFLAGS} -o ${APP} ${APP}.o
	strip --strip-unneeded ${APP}

debug: ${APP}-dbg.o
	#${CC} ${LFLAGS} -lefence -g -o ${APP} ${APP}-dbg.o
	${CC} ${LFLAGS} -g -o ${APP} ${APP}-dbg.o

${APP}.o: ${APP}.c# ${APP}.h
	${CC} ${CFLAGS} -O2 -c ${APP}.c

${APP}-dbg.o: ${APP}.c# ${APP}.h
	${CC} ${CFLAGS} -g -DDEBUG -c ${APP}.c -o ${APP}-dbg.o

splint:
	splint +posixlib ${APP}.c

clangcheck:
	clang --analyze -std=c99 ${APP}.c

clean:
	rm -f *.o *.plist ${APP}
