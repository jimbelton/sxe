#include <errno.h>
#include <malloc.h>
#include <math.h>
#include <string.h>
#include <tap.h>

#include "mockfail.h"
#include "sxe-jitson.h"
#include "sxe-test.h"

int
main(void)
{
    struct sxe_jitson *jitson;
    struct sxe_jitson *member;
    char              *json_out;
    unsigned           len;
    size_t             start_memory = test_memory();

    plan_tests(151);

    diag("Memory allocation failure tests");
    {
        MOCKFAIL_START_TESTS(1, MOCK_FAIL_STACK_NEW_OBJECT);
        is(sxe_jitson_new("0"), NULL, "Failed to allocate the thread stack object");
        MOCKFAIL_END_TESTS();

        MOCKFAIL_START_TESTS(1, MOCK_FAIL_STACK_NEW_JITSONS);
        is(sxe_jitson_new("0"), NULL, "Failed to allocate the thread stack's initial jitsons");
        MOCKFAIL_END_TESTS();

        MOCKFAIL_START_TESTS(1, MOCK_FAIL_STACK_NEXT);
        is(sxe_jitson_new("{\"one\":1}"), NULL, "Failed to realloc the thread stack's jitsons on a string");
        MOCKFAIL_END_TESTS();

        MOCKFAIL_START_TESTS(1, MOCK_FAIL_STACK_NEXT);
        is(sxe_jitson_new("\"01234567\""), NULL, "Failed to realloc the thread stack's jitsons on a long string");
        MOCKFAIL_END_TESTS();

        MOCKFAIL_START_TESTS(1, MOCK_FAIL_STACK_NEXT);
        is(sxe_jitson_new("[0,1]"), NULL, "Failed to realloc the thread stack's jitsons");
        MOCKFAIL_END_TESTS();

        jitson = sxe_jitson_new("999999999");
        MOCKFAIL_START_TESTS(1, MOCK_FAIL_STACK_NEXT_AFTER_GET);
        is(sxe_jitson_new("0"), NULL, "Failed to allocate a new stack after getting the parsed object");
        MOCKFAIL_END_TESTS();
        MOCKFAIL_START_TESTS(1, MOCK_FAIL_STACK_NEXT);
        is(sxe_jitson_new("{\"x\": 0}"), NULL, "Failed to realloc the thread stack's jitsons on string inside an object");
        MOCKFAIL_END_TESTS();

        MOCKFAIL_START_TESTS(1, sxe_factory_reserve);
        is(sxe_jitson_to_json(jitson, NULL), NULL, "Failed to encode large to JSON on realloc failure");
        MOCKFAIL_END_TESTS();
        sxe_jitson_free(jitson);
    }

    diag("Happy path parsing");
    {
        ok(jitson = sxe_jitson_new("0"),                        "Parsed '0' (error %s)", strerror(errno));
        is(sxe_jitson_get_type(jitson), SXE_JITSON_TYPE_NUMBER, "'0' is a number");
        ok(sxe_jitson_get_number(jitson) == 0.0 ,               "Value %f == 0.0", sxe_jitson_get_number(jitson));
        sxe_jitson_free(jitson);

        ok(jitson = sxe_jitson_new(" 666\t"),                   "Parsed ' 666\\t' (error %s)", strerror(errno));
        is(sxe_jitson_get_type(jitson), SXE_JITSON_TYPE_NUMBER, "'666' is a number");
        ok(sxe_jitson_get_number(jitson) == 666.0,              "Value %f == 666.0", sxe_jitson_get_number(jitson));
        sxe_jitson_free(jitson);

        ok(jitson = sxe_jitson_new(" -0.1"),                    "Parsed '-0.1' (error %s)", strerror(errno));
        is(sxe_jitson_get_type(jitson), SXE_JITSON_TYPE_NUMBER, "'-0.1'' is a number");
        ok(sxe_jitson_get_number(jitson) == -0.1,               "Value %f == -0.1", sxe_jitson_get_number(jitson));
        sxe_jitson_free(jitson);

        ok(jitson = sxe_jitson_new("1E-100"),                   "Parsed '1E=100' (error %s)", strerror(errno));
        is(sxe_jitson_get_type(jitson), SXE_JITSON_TYPE_NUMBER, "1E100' is a number");
        ok(sxe_jitson_get_number(jitson) == 1E-100,             "Value %f == 1E100", sxe_jitson_get_number(jitson));
        sxe_jitson_free(jitson);

        ok(jitson = sxe_jitson_new("\"\""),                                "Parsed '\"\"' (error %s)", strerror(errno));
        is(sxe_jitson_get_type(jitson),            SXE_JITSON_TYPE_STRING, "'\"\"' is a string");
        is_eq(sxe_jitson_get_string(jitson, NULL), "",                     "Correct value");
        sxe_jitson_free(jitson);

        ok(jitson = sxe_jitson_new(" \"x\"\n"),                            "Parsed ' \"x\"\\n' (error %s)", strerror(errno));
        is(sxe_jitson_get_type(jitson),            SXE_JITSON_TYPE_STRING, "' \"x\"\\n' is a string");
        is_eq(sxe_jitson_get_string(jitson, &len), "x",                    "Correct value");
        is(len,                                    1,                      "Correct length");
        sxe_jitson_free(jitson);

        ok(jitson = sxe_jitson_new("\"\\\"\\\\\\/\\b\\f\\n\\r\\t\""),      "Parsed '\"\\\"\\\\\\/\\b\\f\\n\\r\\t\"' (error %s)",
           strerror(errno));
        is(sxe_jitson_get_type(jitson),            SXE_JITSON_TYPE_STRING, "'\"\\\"\\\\\\/\\b\\f\\n\\r\\t\"' is a string");
        is_eq(sxe_jitson_get_string(jitson, NULL), "\"\\/\b\f\n\r\t",      "Correct value");
        is_eq(json_out = sxe_jitson_to_json(jitson, NULL), "\"\\u0022\\u005c/\\u0008\\u000c\\u000a\\u000d\\u0009\"",
              "Control characters are correctly encoded");
        sxe_jitson_free(jitson);
        free(json_out);

        ok(jitson = sxe_jitson_new("\"\\u20aC\""),                          "Parsed '\"\\u20aC\"' (error %s)", strerror(errno));
        is(sxe_jitson_get_type(jitson),             SXE_JITSON_TYPE_STRING, "'\"\\u20aC\"' is a string");
        is_eq(sxe_jitson_get_string(jitson, NULL),  "\xE2\x82\xAC",         "Correct UTF-8 value");
        is_eq(json_out = sxe_jitson_to_json(jitson, NULL), "\"\xE2\x82\xAC\"", "Valid UTC code points are not escaped");
        sxe_jitson_free(jitson);
        free(json_out);

        ok(jitson = sxe_jitson_new(" {\t} "),                    "Parsed ' {\\t} ' (error %s)", strerror(errno));
        is(sxe_jitson_get_type(jitson),  SXE_JITSON_TYPE_OBJECT, "' {\\t} ' is an object" );
        is(jitson->size,                 0,                      "Correct size");
        is_eq(sxe_jitson_to_json(jitson, NULL), "{}",            "Encoded back to JSON correctly");
        sxe_jitson_free(jitson);

        ok(jitson = sxe_jitson_new("{\"key\":\"value\"}"),  "Parsed '{\"key\":\"value\"}' (error %s)", strerror(errno));
        is(sxe_jitson_get_type(jitson), SXE_JITSON_TYPE_OBJECT, "'{\"key\":\"value\"}' is an object");
        is(jitson->size,                1,                      "Correct size");
        sxe_jitson_free(jitson);

        ok(jitson = sxe_jitson_new("[1, 2,4]"),             "Parsed '[1, 2,4]' (error %s)", strerror(errno));
        is(sxe_jitson_get_type(jitson), SXE_JITSON_TYPE_ARRAY, "'[1, 2,4]' is an array" );
        is(jitson->size,                3,                      "Correct size");
        sxe_jitson_free(jitson);

        ok(jitson = sxe_jitson_new("[]"),                  "Parsed '[]' (error %s)", strerror(errno));
        is(sxe_jitson_get_type(jitson), SXE_JITSON_TYPE_ARRAY, "'[]' is an array" );
        is(jitson->size,                0,                     "Correct size");
        sxe_jitson_free(jitson);
    }

    diag("Test identifiers and parsing of terminals");
    {
        char         test_id[2] = "\0";
        const char * next;
        int          i;

        for (i = -128; i <= 127; i++) {
            test_id[0] = (char)i;
            next = sxe_jitson_parse_identifier(test_id);

            if (i == '.' || i == '_' || (i >= '0' && i <= '9') || (i >= 'a' && i <= 'z') || (i >= 'A' && i <= 'Z')) {
                if (next == test_id)
                    break;
            } else if (next != test_id)
                break;
        }

        is(i, 128, "All identifier characters are correctly classified");

        ok(jitson = sxe_jitson_new("true"),                   "Parsed 'true' (error %s)", strerror(errno));
        is(sxe_jitson_get_type(jitson), SXE_JITSON_TYPE_BOOL, "'true' is a boolean" );
        is(sxe_jitson_get_bool(jitson), true,                 "Correct value");
        sxe_jitson_free(jitson);

        ok(jitson = sxe_jitson_new("false"),                          "Parsed 'false' (error %s)", strerror(errno));
        is(sxe_jitson_get_type(jitson),         SXE_JITSON_TYPE_BOOL, "'false' is a boolean" );
        is(sxe_jitson_get_bool(jitson),         false,                "Correct value");
        is_eq(sxe_jitson_to_json(jitson, NULL), "false",              "Encoded back to JSON correctly");
        sxe_jitson_free(jitson);

        ok(jitson = sxe_jitson_new("null"),                   "Parsed 'null' (error %s)", strerror(errno));
        is(sxe_jitson_get_type(jitson), SXE_JITSON_TYPE_NULL, "'null' is the null type" );
        sxe_jitson_free(jitson);
    }

    diag("Cover edge cases of paraing");
    {
        ok(!sxe_jitson_new("{0:1}"),    "Failed to parse non-string key '{0:1}' (error %s)",           strerror(errno));
        ok(!sxe_jitson_new("\""),       "Failed to parse unterminated string '\"' (error %s)",         strerror(errno));
        ok(!sxe_jitson_new("\"\\"),     "Failed to parse unterminated escape '\"\\' (error %s)",       strerror(errno));
        ok(!sxe_jitson_new("\"\\u"),    "Failed to parse truncated unicode escape '\"\\u' (error %s)", strerror(errno));
        ok(!sxe_jitson_new(""),         "Failed to parse empty string '' (error %s)",                  strerror(errno));
        ok(!sxe_jitson_new("{\"k\"0}"), "Failed to parse object missing colon '{\"k\"0}' (error %s)",  strerror(errno));
        ok(!sxe_jitson_new("{\"k\":}"), "Failed to parse object missing value '{\"k\":}' (error %s)",  strerror(errno));
        ok(!sxe_jitson_new("{\"k\":0"), "Failed to parse object missing close '{\"k\":0' (error %s)",  strerror(errno));
        ok(!sxe_jitson_new("[0"),       "Failed to parse array missing close '[0' (error %s)",         strerror(errno));
        ok(!sxe_jitson_new("0."),       "Failed to parse invalid fraction '0.' (error %s)",            strerror(errno));
        ok(!sxe_jitson_new("1.0E"),     "Failed to parse invalid exponent '1.0E' (error %s)",          strerror(errno));
        ok(!sxe_jitson_new("fathead"),  "Failed to parse invalid token 'fathead' (error %s)",          strerror(errno));
        ok(!sxe_jitson_new("n"),        "Failed to parse invalid token 'n' (error %s)",                strerror(errno));
        ok(!sxe_jitson_new("twit"),     "Failed to parse invalid token 'twit' (error %s)",             strerror(errno));

        char json[65562];    // Big enough for an object with a > 64K member name
        json[len = 0] = '{';
        json[++len]   = '"';

        while (len < 65538)
            json[++len] = 'm';

        json[++len] = '"';
        ok(!sxe_jitson_new(json), "Failed to parse member name of 64K chanracters (error %s)", strerror(errno));
        errno = 0;
    }

    diag("Cover type to string");
    {
        is_eq(sxe_jitson_type_to_str(SXE_JITSON_TYPE_INVALID), "INVALID", "INVALID type");
        is_eq(sxe_jitson_type_to_str(SXE_JITSON_TYPE_NUMBER),  "NUMBER",  "NUMBER");
        is_eq(sxe_jitson_type_to_str(SXE_JITSON_TYPE_STRING),  "STRING",  "STRING");
        is_eq(sxe_jitson_type_to_str(SXE_JITSON_TYPE_OBJECT),  "OBJECT",  "OBJECT");
        is_eq(sxe_jitson_type_to_str(SXE_JITSON_TYPE_ARRAY),   "ARRAY",   "ARRAY");
        is_eq(sxe_jitson_type_to_str(SXE_JITSON_TYPE_BOOL),    "BOOL",    "BOOL");
        is_eq(sxe_jitson_type_to_str(SXE_JITSON_TYPE_NULL),    "NULL",    "NULL");
        is_eq(sxe_jitson_type_to_str(SXE_JITSON_TYPE_MEMBER),  "MEMBER",  "MEMBER internal type");
    }

    diag("Test object membership function and reencoding");
    {
        ok(jitson = sxe_jitson_new("{\"a\": 1, \"biglongname\": \"B\", \"c\": [2, 3], \"d\" : {\"e\": 4}, \"f\": true}"),
           "Parsed complex object (error %s)", strerror(errno));

        MOCKFAIL_START_TESTS(1, MOCK_FAIL_OBJECT_GET_MEMBER);
        ok(!sxe_jitson_object_get_member(jitson, "a", 0),                   "Can't access object on failure to calloc index");
        MOCKFAIL_END_TESTS();

        ok(member = sxe_jitson_object_get_member(jitson, "a", 0),           "Object has a member 'a'");
        is(sxe_jitson_get_number(member), 1,                                "Member is the number 1");
        ok(!sxe_jitson_object_get_member(jitson, "biglongname", 1),         "Object has no member 'b'");
        ok(member = sxe_jitson_object_get_member(jitson, "biglongname", 0), "Object has a member 'biglongname'");
        is_eq(sxe_jitson_get_string(member, NULL), "B",                     "Member is the string 'B'");
        ok(member = sxe_jitson_object_get_member(jitson, "c", 0),           "Object has a member 'c'");
        is(sxe_jitson_get_size(member), 2,                                  "Member is an array of 2 elements");
        ok(member = sxe_jitson_object_get_member(jitson, "d", 1),           "Object has a member 'd'");
        is(sxe_jitson_get_size(member), 1,                                  "Member is an object with 1 member");
        ok(member = sxe_jitson_object_get_member(jitson, "f", 0),           "Object has a member 'f'");
        ok(sxe_jitson_get_bool(member),                                     "Member is 'true'");
        is(sxe_jitson_get_size(member), 0,                                  "Can't take the size of a number, bool, or null");

        is_eq(json_out = sxe_jitson_to_json(jitson, NULL),
              "{\"biglongname\":\"B\",\"a\":1,\"c\":[2,3],\"f\":true,\"d\":{\"e\":4}}",
              "Encoder spat out same JSON as we took in");

        sxe_jitson_free(jitson);
        free(json_out);
    }

    diag("Test array element function and reencoding");
    {
        struct sxe_jitson *element;

        ok(jitson = sxe_jitson_new("[0, \"anotherlongstring\", {\"member\": null}, true]"),
           "Parsed complex array (error %s)", strerror(errno));

        MOCKFAIL_START_TESTS(1, MOCK_FAIL_ARRAY_GET_ELEMENT);
        ok(!sxe_jitson_array_get_element(jitson, 0),                        "Can't access array on failure to malloc index");
        MOCKFAIL_END_TESTS();

        ok(element = sxe_jitson_array_get_element(jitson, 0),               "Array has a element 0");
        is(sxe_jitson_get_number(element), 0,                               "Element is the number 0");
        ok(element = sxe_jitson_array_get_element(jitson, 1),               "Array has a element 1");
        is_eq(sxe_jitson_get_string(element, NULL), "anotherlongstring",    "Element is the string 'anotherlongstring'");
        ok(element = sxe_jitson_array_get_element(jitson, 2),               "Array has a element 2");
        is(sxe_jitson_get_type(element), SXE_JITSON_TYPE_OBJECT,            "Elememt is an object");
        ok(element = sxe_jitson_array_get_element(jitson, 3),               "Array has a element 3");
        ok(sxe_jitson_get_bool(element),                                    "Element is 'true'");
        ok(!sxe_jitson_array_get_element(jitson, 4),                        "Object has no element 4");

        is_eq(json_out = sxe_jitson_to_json(jitson, NULL), "[0,\"anotherlongstring\",{\"member\":null},true]",
              "Encoder spat out same JSON as we took in");
        sxe_jitson_free(jitson);
        free(json_out);
    }

    diag("Test bug fixes against regressions");
    {
         ok(jitson = sxe_jitson_new("{\"A.D.\": 1, \"x\": 0}"),   "Parsed problem member name (error %s)", strerror(errno));
         ok(member = sxe_jitson_object_get_member(jitson, "A.D.", 0), "Object has a member 'A.D.'");
         is(sxe_jitson_get_type(member), SXE_JITSON_TYPE_NUMBER,      "A.D.'s value is a number");
    }

    diag("Test simple construction");
    {
        struct sxe_jitson primitive[1];

        primitive->type = SXE_JITSON_TYPE_INVALID;
        ok(!sxe_jitson_test(primitive), "invalid tests false");
        is(errno, EINVAL,               "errno is EINVAL");
        errno = 0;
        ok(!sxe_jitson_to_json(primitive, NULL), "Can't convert invalid jitson to JSON");
        is(errno, EINVAL,                        "errno is EINVAL");
        errno = 0;

        sxe_jitson_make_null(primitive);
        is(sxe_jitson_get_type(primitive), SXE_JITSON_TYPE_NULL, "null is null");
        ok(!sxe_jitson_test(primitive),                          "null tests false");

        sxe_jitson_make_bool(primitive, true);
        is(sxe_jitson_get_type(primitive), SXE_JITSON_TYPE_BOOL, "true is a bool");
        ok(sxe_jitson_test(primitive),                           "true tests true");

        sxe_jitson_make_number(primitive, 1.3E100);
        is(sxe_jitson_get_type(primitive), SXE_JITSON_TYPE_NUMBER, "1.3E100 is a number");
        ok(sxe_jitson_test(primitive),                             "1.3E100 tests true");

        sxe_jitson_make_string_ref(primitive, "hello, world");
        is(sxe_jitson_get_type(primitive), SXE_JITSON_TYPE_STRING,    "A string_ref is a string");
        is_eq(sxe_jitson_get_string(primitive, NULL), "hello, world", "String_refs values can be retrieved");
        is(primitive->size, 0,                                        "String_refs don't know their lengths on creation");
        ok(sxe_jitson_test(primitive),                                "Non-empty string ref is true");
        is(sxe_jitson_get_size(primitive), 12,                        "String_ref is 12 characters");
        is(primitive->size, 12,                                       "String_refs cache their lengths");
        sxe_jitson_make_string_ref(primitive, "");
        ok(!sxe_jitson_test(primitive),                               "Empty string_ref tests false");
    }

    diag("Test object construction");
    {
        struct sxe_jitson_stack *stack = sxe_jitson_stack_get_thread();

        ok(sxe_jitson_stack_open_collection(stack, SXE_JITSON_TYPE_OBJECT), "Opened an object on the stack");
        sxe_jitson_stack_close_collection(stack);
        ok(jitson = sxe_jitson_stack_get_jitson(stack),                     "Got the object from the stack");
        is_eq(json_out = sxe_jitson_to_json(jitson, NULL), "{}",            "Look, it's an empty object");
        free(json_out);
        sxe_jitson_free(jitson);

        ok(sxe_jitson_stack_open_collection(stack, SXE_JITSON_TYPE_OBJECT),                "Opened an object on the stack");
        ok(sxe_jitson_stack_add_member_name(stack, "null", SXE_JITSON_TYPE_IS_REF),        "Added a member name reference");
        ok(sxe_jitson_stack_add_null(stack),                                               "Added a null value");
        ok(sxe_jitson_stack_add_member_name(stack, "bool", SXE_JITSON_TYPE_IS_COPY),       "Added a member name by copy");
        ok(sxe_jitson_stack_add_bool(stack, true),                                         "Added boolean true");
        ok(sxe_jitson_stack_add_member_name(stack, "number", SXE_JITSON_TYPE_IS_COPY),     "Added a member name by copy");
        ok(sxe_jitson_stack_add_number(stack, 1.14159),                                    "Added a number");
        ok(sxe_jitson_stack_add_member_name(stack, "hello.world", SXE_JITSON_TYPE_IS_REF), "Added long member name reference");
        ok(sxe_jitson_stack_add_string(stack, "hello, world", SXE_JITSON_TYPE_IS_REF),     "Added long string reference");
        sxe_jitson_stack_close_collection(stack);
        ok(jitson = sxe_jitson_stack_get_jitson(stack), "Got the object from the stack");
        is_eq(json_out = sxe_jitson_to_json(jitson, NULL),
              "{\"number\":1.14159,\"bool\":true,\"hello.world\":\"hello, world\",\"null\":null}", "Got the expected object");
        free(json_out);
        sxe_jitson_free(jitson);
    }

    diag("Test array construction and string edge cases");
    {
        struct sxe_jitson_stack *stack = sxe_jitson_stack_get_thread();

        ok(sxe_jitson_stack_open_collection(stack, SXE_JITSON_TYPE_ARRAY),  "Opened an array on the stack");
        sxe_jitson_stack_close_collection(stack);
        ok(jitson = sxe_jitson_stack_get_jitson(stack),                     "Got the array from the stack");
        is_eq(json_out = sxe_jitson_to_json(jitson, NULL), "[]",            "Look, it's an empty array");
        free(json_out);
        sxe_jitson_free(jitson);

        ok(sxe_jitson_stack_open_collection(stack, SXE_JITSON_TYPE_ARRAY),          "Opened an array on the stack");
        ok(sxe_jitson_stack_add_string(stack, "shortly",  SXE_JITSON_TYPE_IS_COPY), "Added a copy of a short string");
        ok(sxe_jitson_stack_add_string(stack, "longerly", SXE_JITSON_TYPE_IS_COPY), "Added a copy of a longer string");
        ok(sxe_jitson_stack_add_string(stack, "longer than 23 characters", SXE_JITSON_TYPE_IS_COPY),
           "Added a copy of a long string needing more than 2 tokens");
        sxe_jitson_stack_close_collection(stack);
        ok(jitson = sxe_jitson_stack_get_jitson(stack), "Got the array from the stack");
        is_eq(json_out = sxe_jitson_to_json(jitson, NULL), "[\"shortly\",\"longerly\",\"longer than 23 characters\"]",
              "Got the expected JSON");
        free(json_out);
        sxe_jitson_free(jitson);
    }


    sxe_jitson_stack_free_thread();    // Currently, just for coverage

    if (test_memory() != start_memory)
        diag("Memory in use is %zu, not %zu as it was at the start of the test program", test_memory(), start_memory);

    return exit_status();
}
