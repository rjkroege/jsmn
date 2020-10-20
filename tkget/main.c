#include "../jsmn.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/*
 * It would be possible to generalize this into "jsgrep".
 * It would be a comparatively small extension.
 * Maybe later.
*/

/* Function realloc_it() is a wrapper function for standard realloc()
 * with one difference - it frees old memory pointer in case of realloc
 * failure. Thus, DO NOT use old data pointer in anyway after call to
 * realloc_it(). If your code has some kind of fallback algorithm if
 * memory can't be re-allocated - use standard realloc() instead.
 */
static inline void
*realloc_it(void *ptrmem, size_t size)
{
	void *p = realloc(ptrmem, size);
	if (!p) {
		free(ptrmem);
		fprintf(stderr, "realloc(): errno=%d\n", errno);
	}
	return p;
}

// Find a specific key in a token json file.
// t is a tree of jsmntok_t objects packed into an array.
static int
findaccesstoken(const char *js, jsmntok_t *t, size_t count)
{
	int i, j;
	jsmntok_t *key, *val;

	if (count == 0) {
		return 0;
	}

	switch(t->type) {
	case JSMN_PRIMITIVE:
		return 1;
	case JSMN_STRING:
		return 1;
	case JSMN_OBJECT:
		j = 0;
		for (i = 0; i < t->size; i++) {
			key = t + 1 + j;
			// the actual key contents is [js+key->start,js+key->end)
			// printf("key %.*s\n", key->end - key->start, js + key->start);

			// Update the tree position for key's tree.
			j += findaccesstoken(js, key, count - j);

			// TODO(rjk): This could be an argument. See jsgrep idea.
			if (strncmp("access_token", js+key->start, key->end - key->start) == 0) {
				// The value is the next token.
				if (key->size > 0) {
					val = t + 1 + j;
					printf("%.*s\n", val->end - val->start, js + val->start);
				}
			}

			if (key->size > 0) {
				// Skip value's subtree of nodes.
				j += findaccesstoken(js, t + 1 + j, count - j);
			}
		}
		return j + 1;
	case JSMN_ARRAY:
		for (i = 0; i < t->size; i++) {
			j += findaccesstoken(js, t + 1 + j, count - j);
		}
		return j + 1;
	case JSMN_UNDEFINED:
		return 0;
	}
	return 0;
}

int main()
{
	int r;
	int eof_expected = 0;
	char *js = NULL;
	size_t jslen = 0;
	char buf[BUFSIZ];

	jsmn_parser p;
	jsmntok_t *tok;
	size_t tokcount = 2;

	/* Prepare parser */
	jsmn_init(&p);

	/* Allocate some tokens as a start */
	tok = malloc(sizeof(*tok) * tokcount);
	if (tok == NULL) {
		fprintf(stderr, "malloc(): errno=%d\n", errno);
		return 3;
	}

	for (;;) {
		/* Read another chunk */
		r = fread(buf, 1, sizeof(buf), stdin);
		if (r < 0) {
			fprintf(stderr, "fread(): %d, errno=%d\n", r, errno);
			return 1;
		}
		if (r == 0) {
			if (eof_expected != 0) {
				return 0;
			} else {
				fprintf(stderr, "fread(): unexpected EOF\n");
				return 2;
			}
		}

		js = realloc_it(js, jslen + r + 1);
		if (js == NULL) {
			return 3;
		}
		strncpy(js + jslen, buf, r);
		jslen = jslen + r;

again:
		r = jsmn_parse(&p, js, jslen, tok, tokcount);
		if (r < 0) {
			if (r == JSMN_ERROR_NOMEM) {
				tokcount = tokcount * 2;
				tok = realloc_it(tok, sizeof(*tok) * tokcount);
				if (tok == NULL) {
					return 3;
				}
				goto again;
			}
		} else {
			findaccesstoken(js, tok, p.toknext);
			eof_expected = 1;
		}
	}

	return EXIT_SUCCESS;
}
