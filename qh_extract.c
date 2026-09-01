/* compile: gcc -O2 -Wall -Wextra -std=c11 -s -o qh_extract qh_extract.c */

/*
 * qh_extract.c
 *
 * Extracts quoted text from src and appends it to dst.
 *
 * Supported quote types:
 *
 *	``` ... ```
 *	`   ... `
 *	'   ... '
 *	"   ... "
 *
 * Usage:
 *
 *	qh_extract <src> <dst>
 *	qh_extract <src> <dst> -g
 *
 * Modes:
 *
 *	default / flat:
 *		Every non-empty line of every extracted entry is written
 *		as a separate line, similar to ~/.bash_history.
 *
 *	-g / grouped:
 *		Every extracted entry is kept intact. Entries are separated
 *		by ASCII Record Separator (0x1E) followed by '\n'.
 *
 * Parsing order:
 *
 *	1. ```...``` blocks
 *	2. `...` blocks
 *	3. '...' blocks
 *	4. "..." blocks
 *
 * Triple-backtick blocks are removed before processing the remaining
 * text. This prevents their contents from subsequently being parsed
 * as ordinary backtick/single/double quoted text.
 *
 * Quotes support backslash escaping:
 *
 *	"hello \"world\""
 *	'it\'s'
 *	`foo\`bar`
 *
 * Leading/trailing '\n' characters are stripped from every entry.
 * Spaces are NOT stripped.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <regex.h>

#define QH_RS 0x1E

typedef enum {
	MODE_FLAT,
	MODE_GROUPED
} qh_mode_t;

typedef struct {
	char **items;
	size_t count;
	size_t cap;
} EntryList;

/* ============================================================
 * Utility
 * ============================================================ */

static void die(const char *msg)
{
	perror(msg);
	exit(EXIT_FAILURE);
}

static void *xmalloc(size_t size)
{
	void *p = malloc(size);
	if (!p) die("malloc");
	return p;
}

static void *xrealloc(void *ptr, size_t size)
{
	void *p = realloc(ptr, size);
	if (!p) die("realloc");
	return p;
}

/* ============================================================
 * Entry list
 * ============================================================ */

static void entrylist_init(EntryList *l)
{
	l->items = NULL;
	l->count = 0;
	l->cap = 0;
}

static void entrylist_push(EntryList *l, const char *data, size_t len) {
	const char *start = data;
	const char *end = data + len;

/*
 * Equivalent to Python .strip('\n').
 * Only LF characters are stripped.
 * Spaces, tabs, CR, etc. remain untouched.
 */
	while (start < end && *start == '\n')
		start++;

	while (end > start && *(end - 1) == '\n')
		end--;

	size_t stripped_len = (size_t)(end - start);

/*
 * Ignore completely empty entries.
 */
	if (stripped_len == 0)
		return;

	if (l->count == l->cap) {
		size_t new_cap = l->cap ? l->cap * 2 : 16;
		l->items = xrealloc(l->items, new_cap * sizeof(*l->items));
		l->cap = new_cap;
	}

	char *copy = xmalloc(stripped_len + 1);

	memcpy(copy, start, stripped_len);
	copy[stripped_len] = '\0';

	l->items[l->count++] = copy;
}

static void entrylist_free(EntryList *l) {
	for (size_t i = 0; i < l->count; i++)
		free(l->items[i]);

	free(l->items);

	l->items = NULL;
	l->count = 0;
	l->cap = 0;
}

/* ============================================================
 * File handling
 * ============================================================ */

static char *read_whole_file(const char *path, size_t *out_len) {
	FILE *f = fopen(path, "rb");

	if (!f) {
		perror(path);
		exit(EXIT_FAILURE);
	}

	if (fseek(f, 0, SEEK_END) != 0) {
		fclose(f);
		die("fseek");
	}

	long size = ftell(f);

	if (size < 0) {
		fclose(f);
		die("ftell");
	}

	if (fseek(f, 0, SEEK_SET) != 0) {
		fclose(f);
		die("fseek");
	}

	char *buf = xmalloc((size_t)size + 1);

	size_t n = fread(buf, 1, (size_t)size, f);

	if (ferror(f)) {
		fclose(f);
		free(buf);
		die("fread");
	}

	fclose(f);

	buf[n] = '\0';
	*out_len = n;

	return buf;
}

/* ============================================================
 * POSIX regular expressions
 * ============================================================ */

static regex_t re_triple;
static regex_t re_backtick;
static regex_t re_single;
static regex_t re_double;

static void regex_compile(regex_t *re, const char *pattern)
{
	int rc = regcomp(re, pattern, REG_EXTENDED);

	if (rc != 0) {
		char errbuf[256];
		regerror(rc, re, errbuf, sizeof(errbuf));
		fprintf(stderr, "regex compile error for /%s/: %s\n", pattern, errbuf);
		exit(EXIT_FAILURE);
	}
}

static void regex_init(void)
{
	regex_compile(&re_triple, "```");
	regex_compile(&re_backtick, "`");
	regex_compile(&re_single, "'");
	regex_compile(&re_double, "\"");
}

static void regex_free(void)
{
	regfree(&re_triple);
	regfree(&re_backtick);
	regfree(&re_single);
	regfree(&re_double);
}

static int regex_find(const regex_t *re, const char *text, size_t len,
			size_t from, size_t *match_start, size_t *match_end) {
	if (from >= len)
		return 0;

	regmatch_t m;

	int rc = regexec(re, text + from, 1, &m, 0);

	if (rc == REG_NOMATCH)
		return 0;

	if (rc != 0) {
		char errbuf[256];
		regerror(rc, re, errbuf, sizeof(errbuf));
		fprintf(stderr, "regex execution error: %s\n", errbuf);
		exit(EXIT_FAILURE);
	}

	size_t start = from + (size_t)m.rm_so;
	size_t end = from + (size_t)m.rm_eo;

	if (start > len || end > len || end < start)
		return 0;

	*match_start = start;
	*match_end = end;

	return 1;
}

/* ============================================================
 * Quote scanner
 * ============================================================ */

static int find_closing_quote(const char *text, size_t len,
			size_t content_start,const char *delimiter,
			size_t delimiter_len, size_t *close_start) {
	size_t i = content_start;

	while (i < len) {

/*
 * Backslash escapes the next byte.
 *
 * Examples:
 *
 *	\"
 *	\'
 *	\`
 *	\\
 */
		if (text[i] == '\\') {
			if (i + 1 < len)
				i += 2;
			else
				i++;
			continue;
		}

		if (i + delimiter_len <= len &&
					memcmp(text + i, delimiter, delimiter_len) == 0) {
			*close_start = i;
			return 1;
		}

		i++;
	}

	return 0;
}

/* ============================================================
 * Triple-backtick pass
 * ============================================================ */

static char *extract_triple_backticks(const char *text,
					size_t len, EntryList *entries, size_t *out_len) {
	char *rest = xmalloc(len + 1);

	size_t rest_len = 0;
	size_t pos = 0;

	while (pos < len) {
		size_t open_start;
		size_t open_end;

		if (!regex_find(&re_triple, text, len, pos, &open_start, &open_end)) {
			memcpy(rest + rest_len, text + pos, len - pos);
			rest_len += len - pos;
			break;
		}

		size_t close_start;

		if (!find_closing_quote(text, len, open_end, "```", 3, &close_start)) {

/*
 * No closing triple-backtick.
 * Leave it untouched as normal text.
 */
			memcpy(rest + rest_len, text + pos, len - pos);

			rest_len += len - pos;
			break;
		}

/*
 * Copy text before the opening delimiter.
 */
		if (open_start > pos) {
			size_t n = open_start - pos;
			memcpy(rest + rest_len, text + pos, n);
			rest_len += n;
		}

/*
 * Extract the content between the delimiters.
 */
		size_t content_start = open_end;
		size_t content_len = close_start - content_start;

		entrylist_push(entries, text + content_start, content_len
		);

/*
 * Continue after the closing delimiter.
 */
		pos = close_start + 3;
	}

	rest[rest_len] = '\0';
	*out_len = rest_len;

	return rest;
}

/* ============================================================
 * Generic quote pass
 * ============================================================ */

static char *extract_delimited_quotes(const char *text,
			size_t len, const regex_t *regex, const char *delimiter,
			size_t delimiter_len, EntryList *entries, size_t *out_len) {
	char *rest = xmalloc(len + 1);

	size_t rest_len = 0;
	size_t pos = 0;

	while (pos < len) {
		size_t open_start;
		size_t open_end;

		if (!regex_find(regex, text, len, pos, &open_start, &open_end)) {
			memcpy(rest + rest_len, text + pos, len - pos);
			rest_len += len - pos;
			break;
		}

		size_t close_start;

		if (!find_closing_quote(text, len, open_end, delimiter,
										delimiter_len,&close_start)) {
/*
 * No closing delimiter.
 * Leave the remaining text untouched.
 */
			memcpy(rest + rest_len, text + pos, len - pos);
			rest_len += len - pos;
			break;
		}

/*
 * Copy text before the opening delimiter.
 */
		if (open_start > pos) {
			size_t n = open_start - pos;
			memcpy(rest + rest_len, text + pos, n);
			rest_len += n;
		}

/*
 * Extract the content between the delimiters.
 */
		size_t content_start = open_end;
		size_t content_len = close_start - content_start;
		entrylist_push(entries, text + content_start, content_len);

/*
 * Continue after the closing delimiter.
 */
		pos = close_start + delimiter_len;
	}

	rest[rest_len] = '\0';
	*out_len = rest_len;

	return rest;
}

/* ============================================================
 * Output
 * ============================================================ */

static void write_entry(FILE *out, const char *entry, qh_mode_t mode) {
	if (mode == MODE_GROUPED) {

		fputs(entry, out);
		fputc(QH_RS, out);
		fputc('\n', out);

		return;
	}

/*
 * Flat mode:
 * split on LF and skip empty lines.
 */
	const char *line_start = entry;

	for (const char *p = entry;; p++) {
		if (*p == '\n' || *p == '\0') {
			size_t line_len = (size_t)(p - line_start);

			if (line_len > 0) {
				fwrite(line_start,1,line_len,out);
				fputc('\n', out);
			}

			if (*p == '\0')
				break;

			line_start = p + 1;
		}
	}
}

/* ============================================================
 * Main
 * ============================================================ */

int main(int argc, char **argv) {
	if (argc != 3 && argc != 4) {
		fprintf(stderr,"\\ qh_extract \\ Quote History Extractor 2\n");
		fprintf(stderr,"                 😃 by Mario Lohajner 2026\n\n");
		
		fprintf(stderr,"usage: %s <src> <dst> [-g]\n", argv[0]);
		fprintf(stderr,"  -g         grouped mode - entries kept intact, RS-separated\n");
		fprintf(stderr,"  (no flag)  flat mode - one line per item\n");
		return EXIT_FAILURE;
	}

	if (argc == 4 && strcmp(argv[3], "-g") != 0) {
		fprintf(stderr,"unknown flag: %s (only -g is supported)\n", argv[3]);
		return EXIT_FAILURE;
	}

	const char *src_path = argv[1];
	const char *dst_path = argv[2];

	qh_mode_t mode = (argc == 4) ? MODE_GROUPED : MODE_FLAT;
	regex_init();

/*
 * Read the source file.
 */
	size_t len;

	char *text = read_whole_file(src_path, &len);

	EntryList entries;

	entrylist_init(&entries);

/*
 * Pass 1:
 *	```...```
 */
	size_t rest_len;

	char *rest = extract_triple_backticks(text, len, &entries, &rest_len);
	free(text);

/*
 * Pass 2:
 *	`...`
 */
	size_t rest2_len;

	char *rest2 = extract_delimited_quotes(rest, rest_len, &re_backtick,
			"`", 1, &entries, &rest2_len);
	free(rest);

/*
 * Pass 3:
 *	'...'
 */
	size_t rest3_len;

	char *rest3 = extract_delimited_quotes(rest2, rest2_len, &re_single,
			"'", 1, &entries, &rest3_len);
	free(rest2);

/*
 * Pass 4:
 *	"..."
 */
	size_t rest4_len;

	char *rest4 = extract_delimited_quotes(rest3, rest3_len, &re_double,
			"\"", 1, &entries, &rest4_len);

	free(rest3);
	free(rest4);

/*
 * Append extracted entries to the destination file.
 */
	if (entries.count > 0) {
		FILE *out = fopen(dst_path, "a");
		if (!out) {
			perror(dst_path);

			entrylist_free(&entries);
			regex_free();

			return EXIT_FAILURE;
		}

		for (size_t i = 0;i < entries.count;i++) {
			write_entry(
				out,
				entries.items[i],
				mode
			);
		}

		if (fclose(out) != 0) {
			perror("fclose");

			entrylist_free(&entries);
			regex_free();

			return EXIT_FAILURE;
		}
	}

	entrylist_free(&entries);
	regex_free();

	return EXIT_SUCCESS;
}
