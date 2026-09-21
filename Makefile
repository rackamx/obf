CC      = gcc
CFLAGS  = -std=gnu11 -Wall -Wextra -O2 -Isrc
TARGET  = cflatten
SRCS    = src/main.c \
          src/parse/token.c src/parse/segment.c src/parse/toplevel.c \
          src/parse/ast.c src/parse/parser.c \
          src/cff/hoister.c src/cff/lowerer.c src/cff/emit.c \
          src/cff/flatten.c \
          src/utils/util.c src/utils/strbuf.c src/utils/strvec.c
OBJS    = $(SRCS:.c=.o)
HDRS    = src/parse/token.h src/parse/segment.h src/parse/toplevel.h \
          src/parse/ast.h src/parse/parser.h \
          src/cff/hoister.h src/cff/lowerer.h src/cff/emit.h \
          src/cff/flatten.h \
          src/utils/util.h src/utils/strbuf.h src/utils/strvec.h

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

check-docs:
	python3 scripts/check-docs.py $(SRCS) $(HDRS)

check: format-check check-docs test

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

.PHONY: all clean format format-check test doc check-docs check
