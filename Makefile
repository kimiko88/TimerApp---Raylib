# Makefile for Daily Focus Pro (Cross-Platform)

# Detect OS
ifeq ($(OS),Windows_NT)
    PLATFORM = Windows
    CC = g++
    EXT = .exe
    LIBS = -lraylib -lopengl32 -lgdi32 -lwinmm -lshell32
    CFLAGS = -O2 -Wall -Iinclude -D_WIN32
else
    UNAME_S := $(shell uname -s)
    ifeq ($(UNAME_S),Linux)
        PLATFORM = Linux
        CC = g++
        EXT = 
        LIBS = -lraylib -lGL -lm -lpthread -ldl -lrt -lX11
        CFLAGS = -O2 -Wall -Iinclude -D__linux__
    endif
    ifeq ($(UNAME_S),Darwin)
        PLATFORM = macOS
        CC = g++
        EXT = 
        LIBS = -lraylib -framework CoreVideo -framework IOKit -framework Cocoa -framework GLUT -framework OpenGL
        CFLAGS = -O2 -Wall -Iinclude -D__APPLE__
    endif
endif

SRC = main.cpp platform_helper.cpp
OBJ = $(SRC:.cpp=.o)
TARGET = DailyFocus$(EXT)

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(OBJ) -o $(TARGET) $(LIBS)

%.o: %.cpp
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f *.o $(TARGET)
