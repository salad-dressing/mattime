build:
	gcc mattime.c -g -l sqlite3 -lm -o mattime

run:
	./mattime

clean:
	rm mattime
