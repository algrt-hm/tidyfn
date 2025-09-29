.POSIX:

CC = cc
TARGET = tidyfn
TEST_TARGET = tests

# Build if target.c etc. newer than ./target
$(TARGET): $(TARGET).c $(TARGET).h
	$(CC) -Wall -Wextra -O3 -o $(TARGET) $(TARGET).c

# Run it
run: $(TARGET)
	./$(TARGET)

# Format source code -- requires clang-format
format:
# Note: -i flag will edit in place
	clang-format -i \
	-style='{BasedOnStyle: llvm, ColumnLimit: 120, AlignAfterOpenBracket: Align}' \
	*.c

# Install it
install: $(TARGET)
	cp ./$(TARGET) "$(HOME)/bin/$(TARGET)"

run-tests: $(TEST_TARGET)
	./$(TEST_TARGET)

# Test it
$(TEST_TARGET): $(TEST_TARGET).c $(TARGET).c $(TARGET).h
	$(CC) -D TESTING -Wall -Wextra -O0 -o $(TEST_TARGET) $(TEST_TARGET).c $(TARGET).c