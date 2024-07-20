run: main.c
	gcc -o run main.c -lcurses

clean:
	rm run