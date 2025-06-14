CC := gcc
CFLAG := -Wall -O3 -DTEXT_ENABLED
LFLAG := -lc -lm text.o sbmp.o
all: wave wave32 wave16 wave8
wave: expr.o wave.c sbmp.o text.o
	$(CC) $(CFLAG) wave.c expr.o -o wave $(LFLAG)
wave32: expr.o wave.c sbmp.o text.o
	$(CC) $(CFLAG) -DU32BE wave.c expr.o -o wave32 $(LFLAG)
wave16: expr.o wave.c sbmp.o text.o
	$(CC) $(CFLAG) -DU16BE wave.c expr.o -o wave16 $(LFLAG)
wave8: expr.o wave.c sbmp.o text.o
	$(CC) $(CFLAG) -DU8 wave.c expr.o -o wave8 $(LFLAG)
expr.o: expr.c expr.h
	$(CC) $(CFLAG) expr.c -c -o expr.o
sbmp.o: texts/sbmp.c
	$(CC) $(CFLAG) texts/sbmp.c -c -o sbmp.o
text.o: texts/text.c
	$(CC) $(CFLAG) texts/text.c -c -o text.o
.PHONY:
leak: expr.c wave.c expr.h
	$(CC) -Wall -Og -fsanitize=address -g wave.c expr.c -o wave $(LFLAG)
.PHONY:
clean:
	rm -f wave wave32 wave16 wave8 expr.o sbmp.o text.o
