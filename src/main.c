/**
 * @file main.c
 * @brief Command-line driver for the cflatten obfuscator.
 */

/* cflatten.c - Control-flow flattening obfuscator for C programs (C port).
 *
 * Takes a C program as input and generates a semantically equivalent C program
 * with control flow completely flattened into a dispatcher loop:
 *
 *   int __cf_state = 0;
 *   while (__cf_state != -1) {
 *     switch (__cf_state) {
 *       case 0: ... __cf_state += 1; break;
 *       case 1: __cf_state += ((cond)) ? (1) : (3); break;
 *     }
 *   }
 *
 * Usage: cflatten [input.c] [-o output.c]
 * Only the standard C library is used (built as gnu11).
 */

#include "cff/flatten.h"
#include "utils/strbuf.h"
#include "utils/util.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * @brief Slurp a stream fully.
 *
 * @param f Stream to read.
 *
 * @return New NUL-terminated buffer.
 */
char *read_all(FILE *f)
{
	StrBuf b;

	sb_init(&b);
	char tmp[8192];
	size_t n;

	/* Short reads just mean "try again": loop until fread says zero. */
	while ((n = fread(tmp, 1, sizeof(tmp), f)) > 0)
		sb_putn(&b, tmp, n);
	return b.data;
}

/**
 * @brief Flatten a C program to a dispatcher program.
 *
 * @param argc Argument count.
 * @param argv Arguments.
 *
 * @return Exit status.
 */
int main(int argc, char **argv)
{
	const char *infile = NULL, *outfile = NULL;

	for (int i = 1; i < argc; i++) {
		if ((streq(argv[i], "-o") || streq(argv[i], "--output")) &&
		    i + 1 < argc) {
			outfile = argv[++i];
		} else if (streq(argv[i], "-h") || streq(argv[i], "--help")) {
			printf("Usage: %s [input.c] [-o output.c]\n", argv[0]);
			return 0;
		} else if (argv[i][0] == '-' && outfile == NULL &&

			   strlen(argv[i]) > 1) {
			/* A lone "-" means stdin; anything longer is an
			 * unknown flag. */
			fprintf(stderr, "unknown option %s\n", argv[i]);
			return 1;
		} else if (!infile)

			infile = argv[i];
		else {
			fprintf(stderr, "too many inputs\n");
			return 1;
		}
	}

	char *src = NULL;

	if (infile) {
		FILE *f = fopen(infile, "rb");

		if (!f) {
			perror("fopen input");
			return 1;
		}

		src = read_all(f);
		fclose(f);
	} else {
		src = read_all(stdin);
	}

	char *out = flatten_program(src);

	if (outfile) {
		FILE *f = fopen(outfile, "wb");

		if (!f) {
			perror("fopen output");
			return 1;
		}

		fwrite(out, 1, strlen(out), f);
		fclose(f);
	} else {
		fwrite(out, 1, strlen(out), stdout);
	}

	return 0;
}
