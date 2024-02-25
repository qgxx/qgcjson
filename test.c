#include "qgcjson.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main_ret = 0;
int total_count = 0;
int pass_count = 0;

#define EXPECT_EQ(equality, expect, actual, format)\
    do {\
        total_count++;\
        if (equality) pass_count++;\
        else {\
            main_ret = 1;\
            fprintf(stderr, "%s:%d: expect: " format ",actual: " format ";\n", __FILE__, __LINE__, expect, actual);\
        }\
    } while(0)

#define EXPECT_EQ_INT(expect, actual) EXPECT_EQ((expect) == (actual), expect, actual, "%d")
#define EXPECT_EQ_DOUBLE(expect, actual) EXPECT_EQ((expect) == (actual), expect, actual, "%.17g")
#define EXPECT_EQ_STRING(expect, actual, act_len) \
    EXPECT_EQ(sizeof(expect) - 1 == act_len && memcmp(expect, actual, act_len + 1) == 0, expect, actual, "%s")
#if defined(_MSC_VER)
#define EXPECT_EQ_SIZE_T(expect, actual) EXPECT_EQ((expect) == (actual), (size_t)expect, (size_t)actual, "%Iu")
#else
#define EXPECT_EQ_SIZE_T(expect, actual) EXPECT_EQ((expect) == (actual), (size_t)expect, (size_t)actual, "%zu")
#endif

void test_parse_null() {
    json_value v;
    v.type = VALUE_NULL;
    EXPECT_EQ_INT(PARSE_OK, json_parse(&v, "null"));
    EXPECT_EQ_INT(VALUE_NULL, v.type);
}

void test_parse_boolean() {
    json_value v;
    v.type = VALUE_NULL;
    EXPECT_EQ_INT(PARSE_OK, json_parse(&v, "true"));
    EXPECT_EQ_INT(VALUE_TRUE, v.type);
    EXPECT_EQ_INT(PARSE_OK, json_parse(&v, "false"));
    EXPECT_EQ_INT(VALUE_FALSE, v.type);
}

#define TEST_NUMBER(expect, json)\
    do {\
        json_value v;\
        (v).type = VALUE_NULL;\
        EXPECT_EQ_INT(PARSE_OK, json_parse(&v, json));\
        EXPECT_EQ_INT(VALUE_NUMBER, (v).type);\
        EXPECT_EQ_DOUBLE(expect, (v).num);\
    } while(0)

void test_parse_number() {
    TEST_NUMBER(0.0, "0");
    TEST_NUMBER(0.0, "-0");
    TEST_NUMBER(0.0, "-0.0");
    TEST_NUMBER(1.0, "1");
    TEST_NUMBER(-1.0, "-1");
    TEST_NUMBER(1.5, "1.5");
    TEST_NUMBER(-1.5, "-1.5");
    TEST_NUMBER(3.1416, "3.1416");
    TEST_NUMBER(1E10, "1E10");
    TEST_NUMBER(1e10, "1e10");
    TEST_NUMBER(1E+10, "1E+10");
    TEST_NUMBER(1E-10, "1E-10");
    TEST_NUMBER(-1E10, "-1E10");
    TEST_NUMBER(-1e10, "-1e10");
    TEST_NUMBER(-1E+10, "-1E+10");
    TEST_NUMBER(-1E-10, "-1E-10");
    TEST_NUMBER(1.234E+10, "1.234E+10");
    TEST_NUMBER(1.234E-10, "1.234E-10");
    TEST_NUMBER(0.0, "1e-10000"); /* must underflow */

    TEST_NUMBER(1.0000000000000002, "1.0000000000000002"); /* the smallest number > 1 */
    TEST_NUMBER( 4.9406564584124654e-324, "4.9406564584124654e-324"); /* minimum denormal */
    TEST_NUMBER(-4.9406564584124654e-324, "-4.9406564584124654e-324");
    TEST_NUMBER( 2.2250738585072009e-308, "2.2250738585072009e-308");  /* Max subnormal double */
    TEST_NUMBER(-2.2250738585072009e-308, "-2.2250738585072009e-308");
    TEST_NUMBER( 2.2250738585072014e-308, "2.2250738585072014e-308");  /* Min normal positive double */
    TEST_NUMBER(-2.2250738585072014e-308, "-2.2250738585072014e-308");
    TEST_NUMBER( 1.7976931348623157e+308, "1.7976931348623157e+308");  /* Max double */
    TEST_NUMBER(-1.7976931348623157e+308, "-1.7976931348623157e+308");
}

#define TEST_STRING(expect, json)\
    do {\
        json_value v;\
        value_init(&v);\
        EXPECT_EQ_INT(PARSE_OK, json_parse(&v, json));\
        EXPECT_EQ_INT(VALUE_STRING, get_value_type(&v));\
        EXPECT_EQ_STRING(expect, get_value_string(&v), get_value_string_length(&v));\
        free_value(&v);\
    } while(0)

void test_parse_string() {  
    TEST_STRING("", "\"\"");
    TEST_STRING("Hello", "\"Hello\"");
    TEST_STRING("Hello\nWorld", "\"Hello\\nWorld\"");
    TEST_STRING("\" \\ / \b \f \n \r \t", "\"\\\" \\\\ \\/ \\b \\f \\n \\r \\t\"");
    TEST_STRING("Hello\0World", "\"Hello\\u0000World\"");
    TEST_STRING("\x24", "\"\\u0024\"");         /* Dollar sign U+0024 */
    TEST_STRING("\xC2\xA2", "\"\\u00A2\"");     /* Cents sign U+00A2 */
    TEST_STRING("\xE2\x82\xAC", "\"\\u20AC\""); /* Euro sign U+20AC */
    TEST_STRING("\xF0\x9D\x84\x9E", "\"\\uD834\\uDD1E\"");  /* G clef sign U+1D11E */
    TEST_STRING("\xF0\x9D\x84\x9E", "\"\\ud834\\udd1e\"");  /* G clef sign U+1D11E */
}

void test_parse_array() {
    size_t i, j;
    json_value v;

    value_init(&v);
    EXPECT_EQ_INT(PARSE_OK, json_parse(&v, "[ ]"));
    EXPECT_EQ_INT(VALUE_ARRAY, get_value_type(&v));
    EXPECT_EQ_SIZE_T(0, get_value_array_size(&v));
    free_value(&v);

    value_init(&v);
    EXPECT_EQ_INT(PARSE_OK, json_parse(&v, "[ null , false , true , 123 , \"abc\" ]"));
    EXPECT_EQ_INT(VALUE_ARRAY, get_value_type(&v));
    EXPECT_EQ_SIZE_T(5, get_value_array_size(&v));
    EXPECT_EQ_INT(VALUE_NULL,   get_value_type(get_value_array_element(&v, 0)));
    EXPECT_EQ_INT(VALUE_FALSE,  get_value_type(get_value_array_element(&v, 1)));
    EXPECT_EQ_INT(VALUE_TRUE,   get_value_type(get_value_array_element(&v, 2)));
    EXPECT_EQ_INT(VALUE_NUMBER, get_value_type(get_value_array_element(&v, 3)));
    EXPECT_EQ_INT(VALUE_STRING, get_value_type(get_value_array_element(&v, 4)));
    EXPECT_EQ_DOUBLE(123.0, (get_value_array_element(&v, 3)->num));
    EXPECT_EQ_STRING("abc", get_value_string(get_value_array_element(&v, 4)), get_value_string_length(get_value_array_element(&v, 4)));
    free_value(&v);

    value_init(&v);
    EXPECT_EQ_INT(PARSE_OK, json_parse(&v, "[ [ ] , [ 0 ] , [ 0 , 1 ] , [ 0 , 1 , 2 ] ]"));
    EXPECT_EQ_INT(VALUE_ARRAY, get_value_type(&v));
    EXPECT_EQ_SIZE_T(4, get_value_array_size(&v));
    for (i = 0; i < 4; i++) {
        json_value* a = get_value_array_element(&v, i);
        EXPECT_EQ_INT(VALUE_ARRAY, get_value_type(a));
        EXPECT_EQ_SIZE_T(i, get_value_array_size(a));
        for (j = 0; j < i; j++) {
            json_value* e = get_value_array_element(a, j);
            EXPECT_EQ_INT(VALUE_NUMBER, get_value_type(e));
            EXPECT_EQ_DOUBLE((double)j, (e)->num);
        }
    }
    free_value(&v);
}

void test_parse_object() {
    json_value v;
    size_t i;

    value_init(&v);
    EXPECT_EQ_INT(PARSE_OK, json_parse(&v, " { } "));
    EXPECT_EQ_INT(VALUE_OBJECT, get_value_type(&v));
    EXPECT_EQ_SIZE_T(0, get_value_object_size(&v));
    free_value(&v);

    value_init(&v);
    EXPECT_EQ_INT(PARSE_OK, json_parse(&v,
        " { "
        "\"n\" : null , "
        "\"f\" : false , "
        "\"t\" : true , "
        "\"i\" : 123 , "
        "\"s\" : \"abc\", "
        "\"a\" : [ 1, 2, 3 ],"
        "\"o\" : { \"1\" : 1, \"2\" : 2, \"3\" : 3 }"
        " } "
    ));
    EXPECT_EQ_INT(VALUE_OBJECT, get_value_type(&v));
    EXPECT_EQ_SIZE_T(7, get_value_object_size(&v));
}

#define TEST_ERROR(error, json)\
    do {\
        json_value v;\
        value_init(&v);\
        EXPECT_EQ_INT(error, json_parse(&v, json));\
        EXPECT_EQ_INT(VALUE_NULL, get_value_type(&v));\
        free_value(&v);\
    } while(0)

void test_parse_expect_value() {
    TEST_ERROR(PARSE_EXPECT_VALUR, "");
    TEST_ERROR(PARSE_EXPECT_VALUR, " ");
}

void test_parse_invalid_value() {
    TEST_ERROR(PARSE_INVALID_VALUE, "nul");
    TEST_ERROR(PARSE_INVALID_VALUE, "?");

    /* invalid number */
    TEST_ERROR(PARSE_INVALID_VALUE, "+0");
    TEST_ERROR(PARSE_INVALID_VALUE, "+1");
    TEST_ERROR(PARSE_INVALID_VALUE, ".123"); /* at least one digit before '.' */
    TEST_ERROR(PARSE_INVALID_VALUE, "1.");   /* at least one digit after '.' */
    TEST_ERROR(PARSE_INVALID_VALUE, "INF");
    TEST_ERROR(PARSE_INVALID_VALUE, "inf");
    TEST_ERROR(PARSE_INVALID_VALUE, "NAN");
    TEST_ERROR(PARSE_INVALID_VALUE, "nan");

    /* invalid value in array */
    TEST_ERROR(PARSE_INVALID_VALUE, "[1,]");
    TEST_ERROR(PARSE_INVALID_VALUE, "[\"a\", nul]");
}

void test_parse_root_not_singular() {
    TEST_ERROR(PARSE_ROOT_NOT_SINGULAR, "null x");

    /* invalid number */
    TEST_ERROR(PARSE_ROOT_NOT_SINGULAR, "0123"); /* after zero should be '.' or nothing */
    TEST_ERROR(PARSE_ROOT_NOT_SINGULAR, "0x0");
    TEST_ERROR(PARSE_ROOT_NOT_SINGULAR, "0x123");
}

void test_parse_number_too_big() {
    TEST_ERROR(PARSE_NUMBER_TOO_BIG, "1e309");
    TEST_ERROR(PARSE_NUMBER_TOO_BIG, "-1e309");
}

void test_parse_invalid_string_escape() {
    TEST_ERROR(PARSE_INVALID_STRING_ESCAPE, "\"\\v\"");
    TEST_ERROR(PARSE_INVALID_STRING_ESCAPE, "\"\\'\"");
    TEST_ERROR(PARSE_INVALID_STRING_ESCAPE, "\"\\0\"");
    TEST_ERROR(PARSE_INVALID_STRING_ESCAPE, "\"\\x12\"");
}

void test_parse_invalid_string_char() {
    TEST_ERROR(PARSE_INVALID_STRING_CHAR, "\"\x01\"");
    TEST_ERROR(PARSE_INVALID_STRING_CHAR, "\"\x1F\"");
}

void test_parse_invalid_unicode_hex() {
    TEST_ERROR(PARSE_INVALID_UNICODE_HEX, "\"\\u\"");
    TEST_ERROR(PARSE_INVALID_UNICODE_HEX, "\"\\u0\"");
    TEST_ERROR(PARSE_INVALID_UNICODE_HEX, "\"\\u01\"");
    TEST_ERROR(PARSE_INVALID_UNICODE_HEX, "\"\\u012\"");
    TEST_ERROR(PARSE_INVALID_UNICODE_HEX, "\"\\u/000\"");
    TEST_ERROR(PARSE_INVALID_UNICODE_HEX, "\"\\uG000\"");
    TEST_ERROR(PARSE_INVALID_UNICODE_HEX, "\"\\u0/00\"");
    TEST_ERROR(PARSE_INVALID_UNICODE_HEX, "\"\\u0G00\"");
    TEST_ERROR(PARSE_INVALID_UNICODE_HEX, "\"\\u00/0\"");
    TEST_ERROR(PARSE_INVALID_UNICODE_HEX, "\"\\u00G0\"");
    TEST_ERROR(PARSE_INVALID_UNICODE_HEX, "\"\\u000/\"");
    TEST_ERROR(PARSE_INVALID_UNICODE_HEX, "\"\\u000G\"");
    TEST_ERROR(PARSE_INVALID_UNICODE_HEX, "\"\\u 123\"");
}

void test_parse_invalid_unicode_surrogate() {
    TEST_ERROR(PARSE_INVALID_UNICODE_SURROGATE, "\"\\uD800\"");
    TEST_ERROR(PARSE_INVALID_UNICODE_SURROGATE, "\"\\uDBFF\"");
    TEST_ERROR(PARSE_INVALID_UNICODE_SURROGATE, "\"\\uD800\\\\\"");
    TEST_ERROR(PARSE_INVALID_UNICODE_SURROGATE, "\"\\uD800\\uDBFF\"");
    TEST_ERROR(PARSE_INVALID_UNICODE_SURROGATE, "\"\\uD800\\uE000\"");
}

void test_parse_miss_quotation_mark() {
    TEST_ERROR(PARSE_MISS_QUOTATION_MARK, "\"");
    TEST_ERROR(PARSE_MISS_QUOTATION_MARK, "\"abc");
}

void test_parse() {
    test_parse_null();
    test_parse_boolean();
    test_parse_number();
    test_parse_string();
    test_parse_array();
    test_parse_object();
    test_parse_expect_value();
    test_parse_invalid_value();
    test_parse_root_not_singular();
    test_parse_number_too_big();
    test_parse_invalid_string_escape();
    test_parse_invalid_string_char();
    test_parse_invalid_unicode_surrogate();
    test_parse_invalid_unicode_hex();
    test_parse_miss_quotation_mark();
}

#define TEST_ROUNDTRIP(json)\
    do {\
        json_value v;\
        char* json2;\
        size_t length;\
        value_init(&v);\
        EXPECT_EQ_INT(PARSE_OK, json_parse(&v, json));\
        EXPECT_EQ_INT(STRINGIFY_OK, json_generate(&v, &json2, &length));\
        EXPECT_EQ_STRING(json, json2, length);\
        free_value(&v);\
        free(json2);\
    } while(0)

void test_stringify_number() {
    TEST_ROUNDTRIP("0");
    TEST_ROUNDTRIP("-0");
    TEST_ROUNDTRIP("1");
    TEST_ROUNDTRIP("-1");
    TEST_ROUNDTRIP("1.5");
    TEST_ROUNDTRIP("-1.5");
    TEST_ROUNDTRIP("3.25");
    TEST_ROUNDTRIP("1e+20");
    TEST_ROUNDTRIP("1.234e+20");
    TEST_ROUNDTRIP("1.234e-20");

    TEST_ROUNDTRIP("1.0000000000000002"); /* the smallest number > 1 */
    TEST_ROUNDTRIP("4.9406564584124654e-324"); /* minimum denormal */
    TEST_ROUNDTRIP("-4.9406564584124654e-324");
    TEST_ROUNDTRIP("2.2250738585072009e-308");  /* Max subnormal double */
    TEST_ROUNDTRIP("-2.2250738585072009e-308");
    TEST_ROUNDTRIP("2.2250738585072014e-308");  /* Min normal positive double */
    TEST_ROUNDTRIP("-2.2250738585072014e-308");
    TEST_ROUNDTRIP("1.7976931348623157e+308");  /* Max double */
    TEST_ROUNDTRIP("-1.7976931348623157e+308");
}

void test_stringify_string() {
    TEST_ROUNDTRIP("\"\"");
    TEST_ROUNDTRIP("\"Hello\"");
    TEST_ROUNDTRIP("\"Hello\\nWorld\"");
    TEST_ROUNDTRIP("\"\\\" \\\\ / \\b \\f \\n \\r \\t\"");
    TEST_ROUNDTRIP("\"Hello\\u0000World\"");
}

void test_stringify_array() {
    TEST_ROUNDTRIP("[]");
    TEST_ROUNDTRIP("[null,false,true,123,\"abc\",[1,2,3]]");
}

void test_stringify_object() {
    TEST_ROUNDTRIP("{}");
    TEST_ROUNDTRIP("{\"n\":null,\"f\":false,\"t\":true,\"i\":123,\"s\":\"abc\",\"a\":[1,2,3],\"o\":{\"1\":1,\"2\":2,\"3\":3}}");
}

void test_generate() {
    TEST_ROUNDTRIP("null");
    TEST_ROUNDTRIP("false");
    TEST_ROUNDTRIP("true");
    test_stringify_number();
    test_stringify_string();
    test_stringify_array();
    test_stringify_object();
}

int main() {
    test_parse(); 
    test_generate();
    printf("%d/%d (%3.2f%%) passed\n", pass_count, total_count, pass_count * 100.0 / total_count);
    return main_ret;
}