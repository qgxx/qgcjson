#include "qgcjson.h"

#include <assert.h>
#include <errno.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

typedef struct parse_helper {
    const char* json;
    char* stack;
    size_t size, top;
} parse_helper;
void* helper_push(parse_helper* ph, size_t size);
void* helper_pop(parse_helper* ph, size_t size);

void parse_whitespace(parse_helper* ph);
parse_result parse_value(parse_helper* ph, json_value* val);

parse_result parse_string(parse_helper* ph, char** str, size_t* len);
void set_string(json_value* val, const char* s, size_t len);
const char* parse_hex4(const char* p, unsigned* codepoint);
void encode_utf8(parse_helper* ph, unsigned codepoint);

parse_result parse_value_string(parse_helper* ph, json_value* val);
parse_result parse_value_number(parse_helper* ph, json_value* val);
parse_result parse_value_object();
parse_result parse_value_array();
parse_result parse_value_true(parse_helper* ph, json_value* val);
parse_result parse_value_false(parse_helper* ph, json_value* val);
parse_result parse_value_null(parse_helper* ph, json_value* val);

#define HELPER_STACK_INITIAL_SIZE 256

#define EXPECT(ph, ch) do { assert(*ph->json == (ch)); ph->json++; } while(0)
#define ISDIGIT(ch) ((ch) >= '0' && (ch) <= '9') 
#define ISDIGIT1TO9(ch) ((ch) >= '1' && (ch) <= '9')
#define PUTC(ph, ch) do { *(char*)helper_push(ph, sizeof(char)) = (ch); } while(0)

parse_result json_parse(json_value* val, const char* json) {
    parse_helper ph;
    parse_result ret;
    assert(val != NULL);
    ph.json = json;
    ph.stack = NULL;
    ph.size = ph.top = 0;
    value_init(val);

    parse_whitespace(&ph);
    if ((ret = parse_value(&ph, val)) == PARSE_OK) {
        parse_whitespace(&ph);
        if (*ph.json != '\0') {
            ret = PARSE_ROOT_NOT_SINGULAR;
            val->type = VALUE_NULL;
        }
    }
    assert(ph.top == 0);
    free(ph.stack);
    return ret;
}

void* helper_push(parse_helper* ph, size_t size) {
    void* ret;
    assert(size > 0);
    if (ph->top + size >= ph->size) {
        if (ph->size == 0) ph->size = HELPER_STACK_INITIAL_SIZE;
        while (ph->top + size >= ph->size) ph->size += ph->size >> 1;

        ph->stack = (char*)realloc(ph->stack, ph->size);
    }
    ret = ph->stack + ph->top;
    ph->top += size;
    return ret;
}

void* helper_pop(parse_helper* ph, size_t size) {
    assert(size <= ph->top);
    return (ph->stack + (ph->top -= size));
}

void parse_whitespace(parse_helper* ph) {
    const char* p = ph->json;
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
    ph->json = p;
}

parse_result parse_value(parse_helper* ph, json_value* val) {
    switch (*ph->json) {
        case 't': return parse_value_true(ph, val);
        case 'f': return parse_value_false(ph, val);
        case 'n': return parse_value_null(ph, val);
        default: return parse_value_number(ph, val);
        case '"': return parse_value_string(ph, val);
        case '\0': return PARSE_EXPECT_VALUR;
    }
}

#define PARSE_STRING_ERROR(ret) do { ph->top = head; return ret; } while(0)

parse_result parse_string(parse_helper* ph, char** str, size_t* len) {
    size_t head = ph->top;
    const char* p;
    unsigned codepoint, low_surrogate;
    EXPECT(ph, '\"');
    p = ph->json;
    for(;;) {
        char ch = *p++;
        switch (ch) {
            case '\"': 
                *len = ph->top - head;
                *str = helper_pop(ph, *len);
                ph->json = p;
                return PARSE_OK;
            case '\\':
                switch (*p++) {
                    case '\"': PUTC(ph, '\"'); break;
                    case '\\': PUTC(ph, '\\'); break;
                    case '/': PUTC(ph, '/'); break;
                    case 'b': PUTC(ph, '\b'); break;
                    case 'f': PUTC(ph, '\f'); break;
                    case 'n': PUTC(ph, '\n'); break;
                    case 'r': PUTC(ph, '\r'); break;
                    case 't': PUTC(ph, '\t'); break;
                    case 'u': 
                        if (!(p = parse_hex4(p, &codepoint))) 
                            PARSE_STRING_ERROR(PARSE_INVALID_UNICODE_HEX);
                        if (codepoint >= 0xD800 && codepoint <= 0xDBFF) {
                            if (*p++ != '\\') PARSE_STRING_ERROR(PARSE_INVALID_UNICODE_SURROGATE);
                            if (*p++ != 'u') PARSE_STRING_ERROR(PARSE_INVALID_UNICODE_SURROGATE);
                            if (!(p = parse_hex4(p, &low_surrogate)))
                                PARSE_STRING_ERROR(PARSE_INVALID_UNICODE_HEX);
                            if (low_surrogate < 0xDC00 || low_surrogate > 0xDFFF) 
                                PARSE_STRING_ERROR(PARSE_INVALID_UNICODE_SURROGATE);
                            codepoint = (((codepoint - 0xD800) << 10) | (low_surrogate - 0xDC00)) + 0x10000;
                        }
                        encode_utf8(ph, codepoint);
                        break;
                    default:
                        PARSE_STRING_ERROR(PARSE_INVALID_STRING_ESCAPE);
                }
                break;
            case '\0':
                PARSE_STRING_ERROR(PARSE_MISS_QUOTATION_MARK);
            default:
                if ((unsigned char)ch < 0x20) PARSE_STRING_ERROR(PARSE_INVALID_STRING_CHAR);
                PUTC(ph, ch);
        }
    }
}

void set_value_string(json_value* val, const char* s, size_t len) {
    assert(val != NULL && (s != NULL || len == 0));
    free_value(val);
    val->str.s = (char*)malloc(len + 1);
    memcpy(val->str.s, s, len);
    val->str.s[len] = '\0';
    val->str.length = len;
    val->type = VALUE_STRING;
}

const char* parse_hex4(const char* p, unsigned* codepoint) {
    *codepoint = 0;
    for (int i = 0; i < 4; ++i) {
        char ch = *p++;
        *codepoint <<= 4;
        if (ch >= '0' && ch <= '9') *codepoint |= (ch - '0');
        else if (ch >= 'a' && ch <= 'f') *codepoint |= (ch - 'a' + 10);
        else if (ch >= 'A' && ch <= 'F') *codepoint |= (ch - 'A' + 10);
        else return NULL;
    }
    return p;
}

void encode_utf8(parse_helper* ph, unsigned codepoint) {
    if (codepoint <= 0x7F) PUTC(ph, (codepoint & 0xFF));
    else if (codepoint <= 0x7FF) {
        PUTC(ph, 0xC0 | ((codepoint >> 6) & 0xFF));
        PUTC(ph, 0x80 | (codepoint & 0x3F));
    }
    else if (codepoint <= 0xFFFF) {
        PUTC(ph, 0xE0 | ((codepoint >> 12) & 0xFF));
        PUTC(ph, 0x80 | ((codepoint >> 6) & 0x3F));
        PUTC(ph, 0x80 | (codepoint & 0x3F));
    }
    else {
        assert(codepoint <= 0x10FFFF);
        PUTC(ph, 0xF0 | ((codepoint >> 18) & 0xFF));
        PUTC(ph, 0x80 | ((codepoint >> 12) & 0x3F));
        PUTC(ph, 0x80 | ((codepoint >> 6) & 0x3F));
        PUTC(ph, 0x80 | (codepoint & 0x3F));
    }
}

void free_value(json_value* val) {
    assert(val != NULL);
    switch (val->type) {
        case VALUE_STRING:
            free(val->str.s);
            break;
        default:
            break;
    }
    val->type = VALUE_NULL;
}

value_type get_value_type(json_value* val) {
    assert(val != NULL);
    return val->type;
}

const char* get_value_string(const json_value* val) {
    assert(val != NULL && val->type == VALUE_STRING);
    return val->str.s;
}

size_t get_value_string_length(const json_value* val) {
    assert(val != NULL && val->type == VALUE_STRING);
    return val->str.length;
}

parse_result parse_value_string(parse_helper* ph, json_value* val) {
    parse_result ret;
    char* str;
    size_t str_len;
    if ((ret = parse_string(ph, &str, &str_len)) == PARSE_OK) set_value_string(val, str, str_len);
    return ret;
}

parse_result parse_value_number(parse_helper* ph, json_value* val) {
    const char *p = ph->json;
    if (*p == '-') p++;
    if (*p == '0') p++;
    else {
        if (!ISDIGIT1TO9(*p)) return PARSE_INVALID_VALUE;
        for (p++; ISDIGIT(*p); p++);
    }
    if (*p == '.') {
        p++;
        if (!ISDIGIT(*p)) return PARSE_INVALID_VALUE;
        for (p++; ISDIGIT(*p); p++);
    }
    if (*p == 'e' || *p == 'E') {
        p++;
        if (*p == '+' || *p == '-') p++;
        if (!ISDIGIT(*p)) return PARSE_INVALID_VALUE;
        for (p++; ISDIGIT(*p); p++);
    }

    errno = 0;
    val->num = strtod(ph->json, NULL);
    if (errno == ERANGE && (val->num == HUGE_VAL || val->num == -HUGE_VAL)) 
        return PARSE_NUMBER_TOO_BIG;
    ph->json = p;
    val->type = VALUE_NUMBER;
    return PARSE_OK;
}

parse_result parse_value_true(parse_helper* ph, json_value* val) {
    EXPECT(ph, 't');
    if (ph->json[0] == 'r' && ph->json[1] == 'u' && ph->json[2] == 'e') {
        ph->json += 3;
        val->type = VALUE_TRUE;
        return PARSE_OK;
    }
    return PARSE_INVALID_VALUE;
}

parse_result parse_value_false(parse_helper* ph, json_value* val) {
    EXPECT(ph, 'f');
    if (ph->json[0] == 'a' && ph->json[1] == 'l' && ph->json[2] == 's' && ph->json[3] == 'e') {
        ph->json += 4;
        val->type = VALUE_FALSE;
        return PARSE_OK;
    }
    return PARSE_INVALID_VALUE;
}

parse_result parse_value_null(parse_helper* ph, json_value* val) {
    EXPECT(ph, 'n');
    if (ph->json[0] == 'u' && ph->json[1] == 'l' && ph->json[2] == 'l') {
        ph->json += 3;
        val->type = VALUE_NULL;
        return PARSE_OK;
    }
    return PARSE_INVALID_VALUE;
}