CXX = clang++
CXXFLAGS = -std=c++17 -Wall -Wextra -Iinclude -I/opt/homebrew/include `sdl2-config --cflags`
LDFLAGS = `sdl2-config --libs` -lSDL2_ttf -lSDL2_gfx -lSDL2_image
SRC = $(wildcard src/*.cpp)
OBJ = $(SRC:.cpp=.o)
BIN = test/test
TEST = test/main.cpp

all: $(BIN)

$(BIN): $(OBJ) $(TEST)
	$(CXX) $(CXXFLAGS) $(OBJ) $(TEST) -o $(BIN) $(LDFLAGS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

run: all
	./$(BIN)

clean:
	rm -rf $(OBJ) $(BIN)

.PHONY: all run clean
