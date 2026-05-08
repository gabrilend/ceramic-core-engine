/*
 * String operations for driver-test map.
 * Each function is exposed via a dispatch on argv[1] (fn-name).
 * Reads a JSON string from argv[2], writes a JSON string to stdout.
 */
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>

/* {{{ strip_json_string
 * Remove surrounding double-quotes from a JSON string value.
 * Returns a malloc'd buffer the caller must free. */
static char *strip_json_string(const char *s) {
    int len = strlen(s);
    if (len >= 2 && s[0] == '"' && s[len - 1] == '"') {
        char *out = malloc(len - 1);
        memcpy(out, s + 1, len - 2);
        out[len - 2] = '\0';
        return out;
    }
    char *out = malloc(len + 1);
    strcpy(out, s);
    return out;
}
/* }}} */

/* {{{ uppercase */
static void uppercase(const char *json_in) {
    char *s = strip_json_string(json_in);
    int len = strlen(s);
    /* convert in-place */
    for (int i = 0; i < len; i++) {
        s[i] = toupper((unsigned char)s[i]);
    }
    /* output as a single JSON string (driver contract: one value per box) */
    printf("\"%s\"\n", s);
    free(s);
}
/* }}} */

/* {{{ main */
int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "strops: usage: strops <fn-name> <json-arg>\n");
        return 1;
    }
    const char *fn   = argv[1];
    const char *arg1 = argv[2];

    if (strcmp(fn, "uppercase") == 0) {
        uppercase(arg1);
        return 0;
    }

    fprintf(stderr, "strops: unknown function '%s'\n", fn);
    return 1;
}
/* }}} */
