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
	rm -f tests/*_flat.c
	rm -f tests/*.txt

format:
	clang-format --Wno-error=unknown -style=file -i $(SRCS) $(HDRS)

format-check:
	clang-format --Wno-error=unknown -style=file --dry-run -Werror $(SRCS) $(HDRS)

# Regenerate the clangd compilation database (committed for convenience;
# rerun after moving the checkout since it embeds absolute paths).
compile_commands.json: $(SRCS) $(HDRS) Makefile
	@printf '[\n' > $@
	@total=$(words $(SRCS)); n=1; \
	for f in $(SRCS); do \
		comma=,; [ $$n -eq $$total ] && comma=; o=$${f%.c}.o; \
		printf '  {\n    "directory": "%s",\n' "$(CURDIR)" >> $@; \
		printf '    "command": "%s %s -c -o %s %s",\n' \
			"$(CC)" "$(CFLAGS)" "$$o" "$$f" >> $@; \
		printf '    "file": "%s/%s"\n  }%s\n' "$(CURDIR)" "$$f" "$$comma" >> $@; \
		n=$$((n + 1)); \
	done
	@printf ']\n' >> $@

doc:
	doxygen Doxyfile

test: all
	@set -e; \
	for t in $(TESTS); do \
		$(CC) -std=gnu11 tests/$$t.c -o tests/$${t}_orig; \
		./$(TARGET) tests/$$t.c -o tests/$${t}_flat.c; \
		$(CC) -std=gnu11 tests/$${t}_flat.c -o tests/$${t}_flat; \
		tests/$${t}_orig > tests/$${t}_o.txt; \
		tests/$${t}_flat > tests/$${t}_f.txt; \
		if diff -q tests/$${t}_o.txt tests/$${t}_f.txt > /dev/null; then \
			echo "$$t MATCH"; \
		else \
			echo "$$t MISMATCH"; exit 1; \
		fi; \
	done; \
	$(CC) -std=gnu11 tests/test_argv.c -o tests/argv_orig; \
	./$(TARGET) tests/test_argv.c -o tests/argv_flat.c; \
	$(CC) -std=gnu11 tests/argv_flat.c -o tests/argv_flat; \
	tests/argv_orig hello world > tests/argv_o.txt; \
	tests/argv_flat hello world > tests/argv_f.txt; \
	if diff -q tests/argv_o.txt tests/argv_f.txt > /dev/null; then \
		echo "test_argv MATCH"; \
	else \
		echo "test_argv MISMATCH"; exit 1; \
	fi

.PHONY: all clean format format-check test doc
