build:
	gcc mattime.c -g -l sqlite3 -o mattime

run:
	./mattime

clean:
	rm mattime
