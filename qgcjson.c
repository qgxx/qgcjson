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
parse_result parse_value_object(parse_helper* ph, json_value* val);
parse_result parse_value_array(parse_helper* ph, json_value* val);
parse_result parse_value_true(parse_helper* ph, json_value* val);
parse_result parse_value_false(parse_helper* ph, json_value* val);
parse_result parse_value_null(parse_helper* ph, json_value* val);

generate_result stringify_value(parse_helper* ph, const json_value* val, int isFile);

generate_result stringify_value_string(parse_helper* ph, const char* str, size_t len);
generate_result stringify_value_array(parse_helper* ph, const json_value* val, int isFile);
generate_result stringify_value_object(parse_helper* ph, const json_value* val, int isFile);

#define HELPER_STACK_INITIAL_SIZE 256

#define EXPECT(ph, ch) do { assert(*ph->json == (ch)); ph->json++; } while(0)
#define ISDIGIT(ch) ((ch) >= '0' && (ch) <= '9') 
#define ISDIGIT1TO9(ch) ((ch) >= '1' && (ch) <= '9')
#define PUTC(ph, ch) do { *(char*)helper_push(ph, sizeof(char)) = (ch); } while(0)
#define PUTV(ph, v) do { memcpy(helper_push(ph, sizeof(json_value)), &v, sizeof(json_value)); sz++; } while(0)
#define PUTM(ph, m) do { memcpy(helper_push(ph, sizeof(json_member)), &m, sizeof(json_member)); sz++; } while(0)
#define PUTS(ph, s, len) do { memcpy(helper_push(ph, len), s, len); } while(0)

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

parse_result jsonfile_parse(json_value *val, const char* path) {
    FILE* json_file = fopen(path, "r");
    parse_result ret = PARSE_OK;
    if (json_file == NULL) {
        return CAN_NOT_OPEN_FILE;
    }
    fseek(json_file, 0, SEEK_END);
    long sz = ftell(json_file);
    fseek(json_file, 0, SEEK_SET);
    char* json = (char*)malloc(sz + 1);
    fread(json, 1, sz, json_file);
    fclose(json_file);

    long p = 0;
    for (long i = 0; i < sz; i++) 
        if (json[i] == '\n') sz--;
    json[sz] = '\0';
    ret = json_parse(val, json);
    free(json);
    return ret;
}

generate_result json_generate(const json_value* val, char** json, size_t* len, int isFile) {
    assert(val != NULL && json != NULL);
    parse_helper ph;
    generate_result ret = STRINGIFY_OK;
    ph.stack = (char*)malloc(ph.size = HELPER_STACK_INITIAL_SIZE);
    ph.top = 0;
    if ((ret = stringify_value(&ph, val, isFile)) != STRINGIFY_OK) {
        free(ph.stack);
        *json = NULL;
        return ret;
    }
    *len = ph.top;
    PUTC(&ph, '\0');
    *json = ph.stack;
    return ret;
}

generate_result jsonfile_generate(const json_value* val, const char* path) {
    generate_result ret;
    char* json;
    size_t len;
    if ((ret = json_generate(val, &json, &len, 1)) != STRINGIFY_OK) return ret;

    FILE* file = fopen(path, "w");
    if (file == NULL) {
        return CAN_NOT_OPEN_FILE_W;
    }
    fseek(file, 0, SEEK_SET);
    fwrite(json, 1, len, file);
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
        case '[': return parse_value_array(ph, val);
        case '{': return parse_value_object(ph, val);
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
        case VALUE_ARRAY:
            for (size_t i = 0; i < val->arr.size; ++i) free_value(get_value_array_element(val, i));
            free(val->arr.values);
            break;
        case VALUE_OBJECT:
            for (size_t i = 0; i < val->obj.size; ++i) {
                free(val->obj.members[i].key);
                free_value(&val->obj.members[i].value);
            }
            free(val->obj.members);
            break;
        default:
            break;
    }
    val->type = VALUE_NULL;
}

value_type get_value_type(const json_value* val) {
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

parse_result parse_value_object(parse_helper* ph, json_value* val) {
    EXPECT(ph, '{');
    size_t sz = 0;
    int ret = PARSE_OK;
    parse_whitespace(ph);
    if (*ph->json == '}') {
        ph->json++;
        val->type = VALUE_OBJECT;
        val->obj.members = NULL;
        val->obj.size = 0;
        return ret;
    }
    
    json_member member;
    member.key = NULL;
    for (;;) {
        value_init(&member.value);
        /* key */
        if (*ph->json != '"') {
            ret = PARSE_MISS_MEMBER_KEY;
            break;
        }
        char* str;
        if ((ret = parse_string(ph, &str, &member.key_length)) != PARSE_OK) break;
        memcpy(member.key = (char*)malloc(member.key_length + 1), str, member.key_length);
        member.key[member.key_length] = '\0';
        parse_whitespace(ph);
        if (*ph->json != ':') {
            ret = PARSE_MISS_MEMBER_COLON;
            break;
        }
        ph->json++;

        /* value */
        parse_whitespace(ph);
        if ((ret = parse_value(ph, &member.value)) != PARSE_OK) break;
        LS(&member) = RS(&member) = NULL;
        PUTM(ph, member);

        parse_whitespace(ph);
        if (*ph->json == ',') {
            ph->json++;
            parse_whitespace(ph);
        }
        else if (*ph->json == '}') {
            ph->json++;
            val->type = VALUE_OBJECT;
            val->obj.size = sz;
            sz *= sizeof(json_member);
            memcpy(val->obj.members = (json_member*)malloc(sz), helper_pop(ph, sz), sz);
            json_member* root = &val->obj.members[0];
            for (size_t i = 1; i < val->obj.size; i++) down_member(root, &val->obj.members[i]);
            return ret;
        }
        else {
            ret = PARSE_MISS_COMMA_OR_CURLY_BRACKET;
            break;
        }
    }
    free(member.key);
    for (int i = 0; i < sz; ++i) {
        json_member* m = (json_member*)helper_pop(ph, sizeof(json_member));
        free(m->key);
        free_value(&m->value);
    }
    val->type = VALUE_NULL;
    return ret;
}

parse_result parse_value_array(parse_helper* ph, json_value* val) {
    EXPECT(ph, '[');
    size_t sz = 0;
    int ret = PARSE_OK;
    parse_whitespace(ph);
    if (*ph->json == ']') {
        ph->json++;
        val->type = VALUE_ARRAY;
        val->arr.values = NULL;
        val->arr.size = 0;
        return PARSE_OK;
    }
    for (;;) {
        json_value sub_v;
        value_init(&sub_v);
        if ((ret = parse_value(ph, &sub_v)) != PARSE_OK) break;
        PUTV(ph, sub_v);

        parse_whitespace(ph);
        if (*ph->json == ',') {
            *ph->json++;
            parse_whitespace(ph);
        }  
        else if (*ph->json == ']') {
            *ph->json++;
            val->type = VALUE_ARRAY;
            val->arr.size = sz;
            sz *= sizeof(json_value);
            memcpy(val->arr.values = (json_value*)malloc(sz), helper_pop(ph, sz), sz);
            return PARSE_OK;
        }
        else {
            ret = PARSE_MISS_COMMA_OR_SQUARE_BRACKET;
            break;
        } 
    }
    for (size_t i = 0; i < sz; i++) free_value((json_value*)helper_pop(ph, sizeof(json_value)));
    return ret;
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

size_t get_value_array_size(const json_value* val) {
    assert(val != NULL && val->type == VALUE_ARRAY);
    return val->arr.size;
}

size_t get_value_array_capacity(const json_value* val) {
    assert(val != NULL && val->type == VALUE_ARRAY);
    return val->arr.capacity;
}

void set_value_array(json_value* val, size_t capacity) {
    assert(val != NULL);
    free_value(val);
    val->type = VALUE_OBJECT;
    val->arr.capacity = capacity;
    val->arr.size = 0;
    val->arr.values = capacity > 0 ? (json_value*)malloc(capacity * sizeof(json_value)) : NULL;
}

void set_value_object(json_value* val, size_t capacity) {
    assert(val != NULL);
    free_value(val);
    val->type = VALUE_OBJECT;
    val->obj.capacity = capacity;
    val->obj.size = 0;
    val->obj.members = capacity > 0 ? (json_member*)malloc(capacity * sizeof(json_member)) : NULL;
}

void reverse_value_array(json_value* val, size_t capacity) {
    assert(val != NULL && val->type == VALUE_ARRAY && capacity >= val->arr.size);
    val->arr.values = (json_value*)realloc(val->arr.values, capacity * sizeof(capacity));
    val->arr.capacity = capacity;
}

void shrink_value_array(json_value* val) {
    assert(val != NULL && val->type == VALUE_ARRAY);
    val->arr.values = (json_value*)realloc(val->arr.values, val->arr.size * sizeof(json_value));
    val->arr.capacity = val->arr.size;
}

void clear_value_array(json_value* val) {
    assert(val != NULL && val->type == VALUE_ARRAY);
    for (size_t i = 0; i < val->arr.size; i++) free_value(&val->arr.values[i]);
    val->arr.size = 0;
}

void array_push_back(json_value* val, const json_value* e) {
    assert(val != NULL && e != NULL && val->type == VALUE_ARRAY);
    if (val->arr.size >= val->arr.capacity) val->arr.capacity += val->arr.capacity >> 1;
    val->arr.values = (json_value*)realloc(val->arr.values, val->arr.capacity * sizeof(json_value));
    value_copy(&val->arr.values[val->arr.size++], e);
}

json_value* array_pop_back(json_value* val) {
    assert(val != NULL && val->type == VALUE_ARRAY);
    return &val->arr.values[--(val->arr.size)];
}

json_value* get_value_array_element(const json_value* val, size_t idx) {
    assert(val != NULL && val->type == VALUE_ARRAY && val->arr.size > idx);
    return &val->arr.values[idx];
}

size_t get_value_object_size(const json_value* val) {
    assert(val != NULL && val->type == VALUE_OBJECT);
    return val->obj.size;
}

json_member* get_value_object_member(const json_value* val, size_t idx) {
    assert(val != NULL && val->type == VALUE_OBJECT && idx < val->obj.size);
    return &val->obj.members[idx];
}

generate_result stringify_value(parse_helper* ph, const json_value* val, int isFile) {
    int ret = STRINGIFY_OK;
    switch (val->type) {
        case VALUE_NULL: PUTS(ph, "null", 4); break;
        case VALUE_TRUE: PUTS(ph, "true", 4); break;
        case VALUE_FALSE: PUTS(ph, "false", 5); break;
        case VALUE_NUMBER: 
            ph->top -= 32 - sprintf(helper_push(ph, 32), "%.17g", val->num);
            break;
        case VALUE_STRING:
            ret = stringify_value_string(ph, val->str.s, val->str.length);
            break;
        case VALUE_ARRAY:
            ret = stringify_value_array(ph, val, isFile);
            break;
        case VALUE_OBJECT:
            ret = stringify_value_object(ph, val, isFile);
            break;
        default:
            ret = STRINGIFY_INVALID_VALUE;
            break;
    }
    return ret;
}

generate_result stringify_value_string(parse_helper* ph, const char* str, size_t len) {
    static const char hex_digits[] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'};
    int ret = STRINGIFY_OK;
    size_t sz = len * 6 + 2;
    char* p;
    char* head;
    p = head = helper_push(ph, sz);
    *p++ = '"';
    for (size_t i = 0; i < len; ++i) {
        unsigned char ch = (unsigned char)str[i];
        switch (ch) {
            case '\\': *p++ = '\\'; *p++ = '\\'; break;
            case '\"': *p++ = '\\'; *p++ = '\"'; break;
            case '\b': *p++ = '\\'; *p++ = 'b'; break;
            case '\n': *p++ = '\\'; *p++ = 'n'; break;
            case '\t': *p++ = '\\'; *p++ = 't'; break;
            case '\r': *p++ = '\\'; *p++ = 'r'; break;
            case '\f': *p++ = '\\'; *p++ = 'f'; break;
            default:
                if (ch < 0x20) {  // json not include 0x00-0x20
                    *p++ = '\\'; *p++ = 'u'; *p++ = '0'; *p++ = '0';
                    *p++ = hex_digits[ch >> 4];
                    *p++ = hex_digits[ch & 15];
                }
                else *p++ = ch;
                break;
        }
    }
    *p++ = '"';
    ph->top -= (sz - (p - head));
    return ret;
}

generate_result stringify_value_array(parse_helper* ph, const json_value* val, int isFile) {
    generate_result ret = STRINGIFY_OK;
    PUTC(ph, '[');
    if (isFile) PUTC(ph, '\n');
    for (size_t i = 0; i < val->arr.size; i++) {
        if (isFile) PUTC(ph, '\n');
        if (i > 0) PUTC(ph, ',');
        stringify_value(ph, &val->arr.values[i], isFile);
    }
    if (isFile) PUTC(ph, '\n');
    PUTC(ph, ']');
    return ret;
}

generate_result stringify_value_object(parse_helper* ph, const json_value* val, int isFile) {
    generate_result ret = STRINGIFY_OK;
    PUTC(ph, '{');
    if (isFile) PUTS(ph, "\n    ", 5);
    for (size_t i = 0; i < val->obj.size; i++) {
        if (i > 0) {
            PUTC(ph, ',');
            if (isFile) PUTS(ph, "\n    ", 5);
        }
        stringify_value_string(ph, val->obj.members[i].key, val->obj.members[i].key_length);
        PUTC(ph, ':');
        if (isFile) PUTC(ph, ' ');
        stringify_value(ph, &val->obj.members[i].value, isFile);
    }
    if (isFile) PUTC(ph, '\n');
    PUTC(ph, '}');
    return ret;
}

json_value* get_member_value(json_member* m) {
    assert(m != NULL);
    return &m->value;
}

void down_member(json_member* r, json_member* m) {
    if (strcmp(r->key, m->key) < 0) {
        if (RS(r) == NULL) {
            RS(r) = m;
            return;
        }
        down_member(RS(r), m);
    }
    else {
        if (LS(r) == NULL) {
            LS(r) = m;
            return;
        }
        down_member(LS(r), m);
    }
}

double get_value_number(const json_value* val) {
    assert(val != NULL && val->type == VALUE_NUMBER);
    return val->num;
}

void set_value_number(json_value* val, double num) {
    assert(val != NULL);
    free_value(val);
    val->type = VALUE_NUMBER;
    val->num = num;
}

void set_value_null(json_value* val) {
    assert(val != NULL);
    free_value(val);
    val->type = VALUE_NULL;
}

void set_value_true(json_value* val) {
    assert(val != NULL);
    free_value(val);
    val->type = VALUE_TRUE;
}

void set_value_false(json_value* val) {
    assert(val != NULL);
    free_value(val);
    val->type = VALUE_FALSE;
}

#if 1
void value_copy(json_value* dst, const json_value* src) {
    assert(dst != NULL && src != NULL && dst != src);
    free_value(dst);
    switch (src->type) {
        case VALUE_NUMBER:
            set_value_number(dst, src->num);
            break;
        case VALUE_NULL:
            set_value_null(dst);
            break;
        case VALUE_TRUE:
            set_value_true(dst);
            break;
        case VALUE_FALSE:
            set_value_false(dst);
            break;
        case VALUE_STRING:
            set_value_string(dst, src->str.s, src->str.length);
            break;
        case VALUE_ARRAY:
            set_value_array(dst, src->arr.capacity);
            for (size_t i = 0; i < src->arr.size; i++) {
                json_value v;
                value_init(&v);
                value_copy(&v, &src->arr.values[i]);
                memcpy(&dst->arr.values[dst->arr.size++], &v, sizeof(json_value));
            }
            break;
        case VALUE_OBJECT:
            set_value_object(dst, src->obj.capacity);
            for (size_t i = 0; i < src->obj.size; i++) {
                json_member m;
                m.key = (char*)malloc(src->obj.members[i].key_length + 1);
                m.key_length = src->obj.members[i].key_length;
                memcpy(m.key, src->obj.members[0].key, m.key_length + 1);
                m.key[m.key_length] = '\0';
                json_value v;
                value_init(&v);
                value_copy(&v, &src->obj.members[i].value);
                m.value = v;
                memcpy(&dst->obj.members[dst->obj.size++], &m, sizeof(json_member));
            }
            dst->obj.size = dst->obj.size;
            break;
        default:
            value_init(dst);
            break;
    }
}
#endif

void value_move(json_value* dst, json_value* src) {
    assert(dst != NULL && src != NULL);
    free_value(dst);
    memcpy(dst, src, sizeof(json_value));
    value_init(src);
}

int value_is_equal(const json_value* lhs, const json_value* rhs) {
    assert(lhs != NULL && rhs != NULL);
    if (lhs->type != rhs->type) return 0;
    switch (lhs->type) {
        case VALUE_NUMBER:
            return lhs->num == rhs->type;
        case VALUE_STRING:
            return (lhs->str.length == rhs->str.length && memcmp(lhs->str.s, rhs->str.s, rhs->str.length + 1) == 0);
        case VALUE_ARRAY:
            if (lhs->arr.size != rhs->arr.size) return 0;
            for (size_t i = 0; i < rhs->arr.size; i++) 
                if (!value_is_equal(&lhs->arr.values[i], &rhs->arr.values[i])) return 0;
            return 1;
        case VALUE_OBJECT:
            if (lhs->obj.size != rhs->obj.size) return 0;
            for (size_t i = 0; i < lhs->obj.size; i++) {
                if (lhs->obj.members[i].key_length != rhs->obj.members[i].key_length) return 0;
                if (memcmp(lhs->obj.members[i].key, rhs->obj.members[i].key, rhs->obj.members[i].key_length) != 0) return 0;
                if (!value_is_equal(&lhs->obj.members[i].value, &rhs->obj.members[i].value)) return 0;
            }
            return 1;
        default:
            return 1;
    }
}