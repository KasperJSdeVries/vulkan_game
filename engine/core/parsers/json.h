#ifndef JSON_H
#define JSON_H

#include "defines.h"

typedef enum {
    JSON_VALUE_NONE,
    JSON_VALUE_OBJECT,
    JSON_VALUE_ARRAY,
    JSON_VALUE_STRING,
    JSON_VALUE_INTEGER,
    JSON_VALUE_NUMBER,
    JSON_VALUE_BOOLEAN,
    JSON_VALUE_NULL,
    JSON_VALUE_MAX_ENUM,
} json_value_type;

typedef struct {
    char *key;
    u32 key_length;

    struct _json_value *value;
} json_object_member;

typedef struct _json_value {
    struct _json_value *parent;

    json_value_type type;

    union {
        b8 boolean;
        i64 integer;
        f64 number;

        struct {
            u32 length;
            char *ptr;
        } string;

        struct {
            u32 length;
            json_object_member *values;
        } object;

        struct {
            u32 length;
            struct _json_value **values;
        } array;
    } u;
} json_value;

json_value *json_parse(const char *json, u64 length);
void json_value_free(json_value *);

json_value *json_object_get_value(json_value *object, const char *key);

#endif // JSON_H
