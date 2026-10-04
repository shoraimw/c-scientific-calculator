CC ?= gcc
CFLAGS ?= -std=c11 -O2 -Wall -Wextra

ifeq ($(OS),Windows_NT)
LIBS = -lraylib -lopengl32 -lgdi32 -lwinmm
TARGET = scientific_calculator.exe
else
LIBS = -lraylib -lm -lpthread -ldl -lrt -lX11
TARGET = scientific_calculator
endif

all:
	$(CC) $(CFLAGS) main.c -o $(TARGET) $(LIBS)

clean:
	rm -f scientific_calculator scientific_calculator.exe
