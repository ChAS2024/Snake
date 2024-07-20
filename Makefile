run: main.c
	gcc -Wall -o run main.c -lcurses

clean:
	rm run