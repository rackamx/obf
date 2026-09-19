CC      = gcc
CFLAGS  = -std=gnu11 -Wall -Wextra -O2
TARGET  = cflatten
SRCS    = src/main.c src/parser.c src/flatten.c src/util.c
OBJS    = $(SRCS:.c=.o)
HDRS    = src/util.h src/parser.h src/flatten.h

TESTS   = test1 test2 test3 test4 test5 test_static

all: $(TARGET)

$(TARGET): $(SRCS) $(HDRS)
	$(CC) $(CFLAGS) -o $@ $(SRCS)

%.o: %.c $(HDRS)
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(TARGET) $(OBJS)
	rm -rf doc
	rm -f tests/test*_orig tests/test*_flat tests/test*_o2_orig tests/test*_o2_flat
	rm -f tests/argv_orig tests/argv_flat
	rm -f tests/*.txt

format:
	clang-format --Wno-error=unknown -style=file -i $(SRCS) $(HDRS)

format-check:
	clang-format --Wno-error=unknown -style=file --dry-run -Werror $(SRCS) $(HDRS)

doc:
	doxygen Doxyfile

test: all
	@set -e; \
	for t in $(TESTS); do \
		$(CC) -std=gnu11 tests/$$t.c -o /tmp/$${t}_orig; \
		./$(TARGET) tests/$$t.c -o /tmp/$${t}_flat.c; \
		$(CC) -std=gnu11 /tmp/$${t}_flat.c -o /tmp/$${t}_flat; \
		/tmp/$${t}_orig > /tmp/$${t}_o.txt; \
		/tmp/$${t}_flat > /tmp/$${t}_f.txt; \
		if diff -q /tmp/$${t}_o.txt /tmp/$${t}_f.txt > /dev/null; then \
			echo "$$t MATCH"; \
		else \
			echo "$$t MISMATCH"; exit 1; \
		fi; \
	done; \
	$(CC) -std=gnu11 tests/test_argv.c -o /tmp/argv_orig; \
	./$(TARGET) tests/test_argv.c -o /tmp/argv_flat.c; \
	$(CC) -std=gnu11 /tmp/argv_flat.c -o /tmp/argv_flat; \
	/tmp/argv_orig hello world > /tmp/argv_o.txt; \
	/tmp/argv_flat hello world > /tmp/argv_f.txt; \
	if diff -q /tmp/argv_o.txt /tmp/argv_f.txt > /dev/null; then \
		echo "test_argv MATCH"; \
	else \
		echo "test_argv MISMATCH"; exit 1; \
	fi

.PHONY: all clean format format-check test doc
