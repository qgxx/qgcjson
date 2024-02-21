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
        v.type = VALUE_NULL;\
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

void test_parse() {
    test_parse_null();
    test_parse_boolean();
    test_parse_number();
    test_parse_string();
}

int main() {
    test_parse();
    printf("%d/%d (%3.2f%%) passed\n", pass_count, total_count, pass_count * 100.0 / total_count);
    return main_ret;
}