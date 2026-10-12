#include "bigint_io.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

void trim_whitespace(char *str) {
    if (!str) return;

    /* Trim leading whitespace */
    char *start = str;
    while (*start && isspace((unsigned char)*start)) {
        start++;
    }

    /* Trim trailing whitespace */
    char *end = start + strlen(start);
    while (end > start && isspace((unsigned char)*(end - 1))) {
        end--;
    }
    *end = '\0';

    if (start != str) {
        memmove(str, start, (size_t)(end - start + 1));
    }
}

char *read_input_line(FILE *stream) {
    if (!stream) return NULL;

    size_t capacity = 128;
    size_t len = 0;
    char *buf = (char *)malloc(capacity);
    if (!buf) return NULL;

    int c;
    while ((c = fgetc(stream)) != EOF && c != '\n') {
        if (len + 1 >= capacity) {
            size_t new_cap = capacity * 2;
            char *new_buf = (char *)realloc(buf, new_cap);
            if (!new_buf) {
                free(buf);
                return NULL;
            }
            buf = new_buf;
            capacity = new_cap;
        }
        buf[len++] = (char)c;
    }

    if (len == 0 && c == EOF) {
        free(buf);
        return NULL;
    }

    buf[len] = '\0';
    return buf;
}

int bigint_from_string(BigInteger *a, const char *str) {
    if (!a || !str) return 0;

    /* Skip leading whitespace */
    while (*str && isspace((unsigned char)*str)) {
        str++;
    }

    if (!*str) {
        return 0;
    }

    int sign = 1;
    if (*str == '-') {
        sign = -1;
        str++;
    } else if (*str == '+') {
        str++;
    }

    /* Skip leading zeros */
    int had_zeros = 0;
    while (*str == '0') {
        had_zeros = 1;
        str++;
    }

    /* If no characters remain */
    if (!*str) {
        if (had_zeros) {
            /* Input was "0", "+0", "-000", etc. */
            bigint_clear(a);
            return 1;
        }
        /* Bare "+" or "-" with no digits is invalid */
        return 0;
    }

    /* Must start with a digit */
    if (!isdigit((unsigned char)*str)) {
        return 0;
    }

    /* Validate all remaining characters are digits */
    const char *start = str;
    while (*str) {
        if (!isdigit((unsigned char)*str)) {
            return 0;
        }
        str++;
    }

    size_t num_digits = (size_t)(str - start);
    size_t num_limbs = (num_digits + BASE_DIGITS - 1) / BASE_DIGITS;

    if (!bigint_reserve(a, num_limbs)) {
        return 0;
    }

    bigint_clear(a);
    a->size = num_limbs;
    a->sign = sign;

    /* Parse from right to left in chunks of 9 */
    const char *ptr = str; /* points to end of string */
    for (size_t limb_idx = 0; limb_idx < num_limbs; limb_idx++) {
        size_t chunk_len = (num_digits >= BASE_DIGITS ? BASE_DIGITS : num_digits);
        ptr -= chunk_len;
        num_digits -= chunk_len;

        uint32_t val = 0;
        for (size_t i = 0; i < chunk_len; i++) {
            val = val * 10 + (uint32_t)(ptr[i] - '0');
        }
        a->digits[limb_idx] = val;
    }

    bigint_normalize(a);
    return 1;
}

char *bigint_to_string(const BigInteger *a) {
    if (!a) return NULL;

    if (bigint_is_zero(a) || a->size == 0 || (a->size == 1 && a->digits[0] == 0)) {
        char *zero_str = (char *)malloc(2);
        if (zero_str) {
            zero_str[0] = '0';
            zero_str[1] = '\0';
        }
        return zero_str;
    }

    /* Allocate buffer: 1 (sign) + 9 * a->size + 1 ('\0') */
    size_t max_len = 2 + a->size * BASE_DIGITS;
    char *buf = (char *)malloc(max_len);
    if (!buf) return NULL;

    size_t pos = 0;
    if (a->sign < 0) {
        buf[pos++] = '-';
    }

    /* Print most significant limb unpadded */
    pos += (size_t)sprintf(buf + pos, "%u", a->digits[a->size - 1]);

    /* Print remaining limbs zero-padded to 9 digits */
    for (size_t i = a->size - 1; i > 0; i--) {
        pos += (size_t)sprintf(buf + pos, "%09u", a->digits[i - 1]);
    }

    buf[pos] = '\0';
    return buf;
}
