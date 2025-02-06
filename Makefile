# Set the C++ compiler and flags
CXX = g++
CXXFLAGS = -std=c++17 -Wall -IInclude

# Determine the executable extension based on OS.
ifeq ($(OS),Windows_NT)
	EXE_EXT = .exe
else
	EXE_EXT =
endif

# Define target names using the extension
TARGET = orderbook$(EXE_EXT)
TEST_TARGET = orderbook_tests$(EXE_EXT)

# List source files
SRC := $(wildcard Src/*.cpp)
MAIN_SRC := Src/main.cpp
APP_SRC := $(filter-out $(MAIN_SRC), $(SRC))
APP_OBJ := $(APP_SRC:.cpp=.o)
MAIN_OBJ := $(MAIN_SRC:.cpp=.o)

# Test source and object files
TEST_SRC := $(wildcard Tests/*.cpp)
TEST_OBJ := $(TEST_SRC:.cpp=.o)
TEST_SHARED_OBJ := $(APP_OBJ)

# Build the main executable
all: $(TARGET)

$(TARGET): $(MAIN_OBJ) $(APP_OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $(MAIN_OBJ) $(APP_OBJ)

# Rule to compile .cpp files to .o files.
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Build the test executable
$(TEST_TARGET): $(TEST_OBJ) $(TEST_SHARED_OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $(TEST_OBJ) $(TEST_SHARED_OBJ)

# Run tests
test: $(TEST_TARGET)
ifeq ($(OS),Windows_NT)
	$(TEST_TARGET)
else
	./$(TEST_TARGET)
endif

# Clean generated files
clean:
ifeq ($(OS),Windows_NT)
	@echo Cleaning project...
	@for /R %%f in (*.o) do del /F /Q "%%f"
	@for /R %%f in (*.exe) do del /F /Q "%%f"
	@echo Clean complete.
else
	@echo Cleaning project...
	@find . -name "*.o" -delete
	@rm -f $(TARGET) $(TEST_TARGET)
	@echo Clean complete.
endif
