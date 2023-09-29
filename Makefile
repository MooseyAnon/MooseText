mooseText: mooseText.c
	$(CC) mooseText.c -o mooseText -Wall -Wextra -pedantic -std=c99

clean:
	rm mooseText
