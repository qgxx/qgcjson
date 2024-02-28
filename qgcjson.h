#ifndef __QGCJSON_H__
#define __QGCJSON_H__

#include <stddef.h>
#include <stdio.h>

typedef enum { VALUE_STRING, VALUE_NUMBER, VALUE_OBJECT, VALUE_ARRAY, VALUE_TRUE, VALUE_FALSE, VALUE_NULL } value_type;

typedef struct json_value json_value;
typedef struct json_member json_member;

struct json_value {
    union {
        struct { json_value* values; size_t size, capacity; } arr;  
        struct { json_member* members; size_t size, capacity; } obj;
        struct { char* s; size_t length; } str;
        double num;
    };
    value_type type;
};
void free_value(json_value* val);
value_type get_value_type(const json_value* val);

const char* get_value_string(const json_value* val);
size_t get_value_string_length(const json_value* val);
void set_value_string(json_value* val, const char* s, size_t len);

double get_value_number(const json_value* val);
void set_value_number(json_value* val, double num);

void set_value_null(json_value* val);
void set_value_true(json_value* val);
void set_value_false(json_value* val);

size_t get_value_array_size(const json_value* val);
size_t get_value_array_capacity(const json_value* val);
json_value* get_value_array_element(const json_value* val, size_t idx);
void set_value_array(json_value* val, size_t capacity);
void reverse_value_array(json_value* val, size_t capacity);
void shrink_value_array(json_value* val);
void clear_value_array(json_value* val);
void array_push_back(json_value* val, const json_value* e);
void array_push_front(json_value* val, const json_value* e);
json_value* array_pop_back(json_value* val);
json_value* array_pop_front(json_value* val);
void array_insert_element(json_value* val, size_t idx);
void array_delete_element(json_value* val, size_t idx);
void array_erase_element(json_value* val, size_t idx);
void array_clear_element(json_value* val);

size_t get_value_object_size(const json_value* val);
size_t get_value_object_capacity(const json_value* val);
json_member* get_value_object_member(const json_value* val, size_t idx);
void set_value_object(json_value* val, size_t capacity);
void reverse_value_object(json_value* val, size_t capacity);
void shrink_value_object(json_value* val);
int object_find_member(const json_value* val, const char* key, size_t len);
void insert_member(json_value* v, json_member* m);
void remove_member(json_value* v, const char* key, size_t len);

void value_copy(json_value* dst, const json_value* src);
void value_move(json_value* dst, json_value* src);
int value_is_equal(const json_value* lhs, const json_value* rhs);

#define value_init(v) do { (v)->type = VALUE_NULL; } while(0)
#define ARRAAY_VALUE(val, idx) (val)->arr.values[idx]
#define OBJECT_MEMBER(val, idx) (val)->obj.members[idx]

struct json_member {
    char* key;
    size_t key_length;
    json_value value;
    json_member* sons[2];
};
const char* get_member_key(const json_member* m, size_t* len);
json_value* get_member_value(json_member* m);
void down_member(json_member* f, json_member* m);
json_member* search_member(json_member* f, const char* key, size_t len);
void rebuild_member_tree(json_value* val);

void member_copy(json_member* dst, const json_member* src, json_value* dstr);
void member_move(json_member* dst, json_member* src, json_value* dstr);
int member_is_equal(const json_member* lhs, const json_member* rhs);

#define LS(member) (member)->sons[0]
#define RS(member) (member)->sons[1]

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
    PARSE_MISS_QUOTATION_MARK,  // '"'

    PARSE_MISS_COMMA_OR_SQUARE_BRACKET,  // ',' or ']'

    PARSE_MISS_MEMBER_KEY,
    PARSE_MISS_MEMBER_COLON,
    PARSE_MISS_COMMA_OR_CURLY_BRACKET,

    CAN_NOT_OPEN_FILE
} parse_result;

typedef enum {
    STRINGIFY_OK = 0,
    
    STRINGIFY_INVALID_VALUE,

    CAN_NOT_OPEN_FILE_W
} generate_result;

parse_result json_parse(json_value* val, const char* json);
parse_result jsonfile_parse(json_value *val, const char* path);
generate_result json_generate(const json_value* val, char** json, size_t* len, int isFile);
generate_result jsonfile_generate(const json_value* val, const char* path);

#endif //__QGCJSON_H__