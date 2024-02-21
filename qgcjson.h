#ifndef __QGCJSON_H__
#define __QGCJSON_H__

#include <stddef.h>

typedef enum { VALUE_STRING, VALUE_NUMBER, VALUE_OBJECT, VALUE_ARRAY, VALUE_TRUE, VALUE_FALSE, VALUE_NULL } value_type;

typedef struct json_value json_value;
typedef struct json_member json_member;

struct json_value {
    union {
        struct { json_member* members; size_t size, capacity; } obj;
        struct { json_value* values; size_t size, capacity; } arr;  
        struct { char* s; size_t length; } str;
        double num;
    };
    value_type type;
};
void free_value(json_value* val);
value_type get_value_type(json_value* val);

const char* get_value_string(const json_value* val);
size_t get_value_string_length(const json_value* val);
void set_value_string(json_value* val, const char* s, size_t len);

#define value_init(v) do { (v)->type = VALUE_NULL; } while(0)

struct json_member {
    char* key;
    size_t key_length;
    json_value value;
};

typedef enum {
    PARSE_OK = 0,

    PARSE_INVALID_VALUE,
    PARSE_EXPECT_VALUR,
    PARSE_ROOT_NOT_SINGULAR,

    PARSE_NUMBER_TOO_BIG,

    PARSE_INVALID_STRING_CHAR,
    PARSE_INVALID_STRING_ESCAPE,
    PARSE_INVALID_UNICODE_HEX,
    PARSE_INVALID_UNICODE_SURROGATE,
    PARSE_MISS_QUOTATION_MARK  // "
} parse_result;

parse_result json_parse(json_value* val, const char* json);

#endif // __QGCJSON_H__