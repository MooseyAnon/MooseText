
current_dir := $(shell pwd)

mooseText: mooseText.c
	$(CC) \
		core.c \
		highlight.c \
		mooseText.c \
		search.c \
		termconf.c \
		io/*.c \
		ops/*c \
		-o mooseText \
		-Wall \
		-Wextra \
		-pedantic \
		-std=c99 \
		-I $(current_dir) \
		-I $(current_dir)/io \
		-I $(current_dir)/ops

clean:
	rm mooseText
