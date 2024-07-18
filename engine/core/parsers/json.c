#include "json.h"
#include "containers/darray.h"
#include "defines.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    const char *json;
    u64 json_length;

    u64 current_offset; // offset into the json string
} parse_state;

static json_value *json_parse_impl(parse_state *state, json_value *parent);

json_value *json_parse(const char *json, u64 length) {
    parse_state state = {
        .json = json,
        .json_length = length,
    };

    return json_parse_impl(&state, NULL);
}

void json_value_free(json_value *value) {
    switch (value->type) {
    case JSON_VALUE_OBJECT:
        for (u32 i = 0; i < value->u.object.length; i++) {
            json_value_free(value->u.object.values[i].value);
            free(value->u.object.values[i].key);
        }
        break;
    case JSON_VALUE_ARRAY:
        for (u32 i = 0; i < value->u.array.length; i++) {
            json_value_free(value->u.array.values[i]);
        }
        free(value->u.array.values);
        break;
    case JSON_VALUE_STRING:
        free(value->u.string.ptr);
        break;
    default:
        break;
    }
    free(value);
}

json_value *json_object_get_value(json_value *object, const char *key) {
    if (object->type != JSON_VALUE_OBJECT) {
        return NULL;
    }

    u32 key_length = strlen(key);

    for (u32 i = 0; i < object->u.object.length; i++) {
        if (object->u.object.values[i].key_length != key_length)
            continue;

        if (strncmp(object->u.object.values[i].key, key, object->u.object.values[i].key_length) ==
            0) {
            return object->u.object.values[i].value;
        }
    }

    return NULL;
}

static void skip_whitespace(parse_state *state) {
    while (state->current_offset < state->json_length) {
        char current_char = state->json[state->current_offset];
        switch (current_char) {
        case 0x20:
        case 0x0A:
        case 0x0D:
        case 0x09:
            state->current_offset++;
            break;
        default:
            return;
        }
    }
}

static void next(parse_state *state) { state->current_offset++; }
static char current(parse_state *state) {
    if (state->current_offset >= state->json_length)
        return '\0';
    return state->json[state->current_offset];
}

static b8 assert_current(parse_state *state, char expected) {
    if (state->current_offset >= state->json_length)
        return false;

    if (state->json[state->current_offset] == expected) {
        next(state);
        return true;
    }

    return false;
}

// current should be '"'
static b8 parse_string(parse_state *state, u32 *length, char **string) {
    if (!assert_current(state, '"')) {
        return false;
    }
    u64 string_start = state->current_offset;
    u64 string_length = 0;
    while ((current(state) != '"') && (state->json[state->current_offset - 1] != '\\')) {
        string_length++;
        next(state);
    }

    char buffer[string_length];
    u32 final_string_length = 0;

    for (u64 i = string_start; i < string_start + string_length; i++) {
        if (state->json[i] == '\\') {
            switch (state->json[++i]) {
            case '"':
                buffer[final_string_length++] = '\"';
                break;
            case '\\':
                buffer[final_string_length++] = '\\';
                break;
            case '/':
                buffer[final_string_length++] = '/';
                break;
            case 'b':
                buffer[final_string_length++] = '\b';
                break;
            case 'f':
                buffer[final_string_length++] = '\f';
                break;
            case 'n':
                buffer[final_string_length++] = '\n';
                break;
            case 'r':
                buffer[final_string_length++] = '\r';
                break;
            case 't':
                buffer[final_string_length++] = '\t';
                break;
            case 'u': {
                u16 unicode = 0;
                for (u8 j = 0; j < 4; j++) {
                    char h = state->json[++i];
                    u16 b = 0;
                    if ((h >= 'a') && (h <= 'f')) {
                        b = h - 'a' + 10;
                    } else if ((h >= 'A') && (h <= 'F')) {
                        b = h - 'A' + 10;
                    } else if ((h >= '0') && (h <= '9')) {
                        b = h - '0';
                    } else
                        return false;
                    unicode |= b << (3 - j) * 4;
                }
                buffer[final_string_length++] = (char)((unicode >> 8) & 0xff);
                buffer[final_string_length++] = (char)(unicode & 0xff);
            } break;
            default:
                return false;
            }
        }

        buffer[final_string_length++] = state->json[i];
    }

    if (!assert_current(state, '"')) {
        return false;
    }

    *length = final_string_length;
    *string = strndup(buffer, final_string_length);
    return true;
}

static json_value *json_parse_impl(parse_state *state, json_value *parent) {
    json_value *value = malloc(sizeof(*value));
    *value = (json_value){
        .parent = parent,
    };

    skip_whitespace(state);

    if (state->current_offset >= state->json_length) {
        fprintf(stderr, "JSON: Invalid string input\n");
        return NULL;
    }

    char current_char = current(state);
    if (current_char == '{') {
        next(state);
        skip_whitespace(state);

        json_object_member *members = darray_create(*members);
        while (current(state) != '}') {
            skip_whitespace(state);
            json_object_member member = {0};
            if (!parse_string(state, &member.key_length, &member.key)) {
                darray_destroy(members);
                fprintf(stderr, "JSON: expected key string at: %llu\n", state->current_offset);
                return NULL;
            }
            skip_whitespace(state);
            if (!assert_current(state, ':')) {
                darray_destroy(members);
                fprintf(stderr, "JSON: expected ':' at: %llu\n", state->current_offset);
                return NULL;
            }
            member.value = json_parse_impl(state, value);
            darray_push(members, member);
            if (!assert_current(state, ',')) {
                break;
            }
        }

        if (!assert_current(state, '}')) {
            darray_destroy(members);
            fprintf(stderr, "JSON: expected '}' at: %llu\n", state->current_offset);
            return NULL;
        }

        value->type = JSON_VALUE_OBJECT;
        value->u.object.length = darray_length(members);
        if (darray_length(members) > 0) {
            value->u.object.values = malloc(sizeof(*members) * darray_length(members));
            memcpy(value->u.object.values, members, sizeof(*members) * darray_length(members));
        } else {
            value->u.object.values = NULL;
        }

        darray_destroy(members);
    } else if (current_char == '[') {
        if (!assert_current(state, '[')) {
            return NULL;
        }

        skip_whitespace(state);

        json_value **values = darray_create(json_value *);

        while (current(state) != ']') {
            json_value *v = json_parse_impl(state, value);

            if (v == NULL) {
                darray_destroy(values);
                return NULL;
            }

            darray_push(values, v);

            if (!assert_current(state, ',')) {
                break;
            }
        }

        if (!assert_current(state, ']')) {
            darray_destroy(values);
            fprintf(stderr, "JSON: expected ']' at: %llu\n", state->current_offset);
            return NULL;
        }

        value->type = JSON_VALUE_ARRAY;
        value->u.array.length = darray_length(values);
        if (value->u.array.length > 0) {
            value->u.array.values = malloc(sizeof(json_value *) * value->u.array.length);
            memcpy(value->u.array.values, values, sizeof(json_value *) * value->u.array.length);
        }

        darray_destroy(values);
    } else if (current_char == '"') {
        value->type = JSON_VALUE_STRING;
        if (!parse_string(state, &value->u.string.length, &value->u.string.ptr)) {
            return NULL;
        }
    } else if (((current_char >= '0') && (current_char <= '9')) || (current_char == '-')) {
        b8 is_negative = false;
        if (current_char == '-') {
            is_negative = true;
            next(state);
        }
        u64 decimal_part_start = state->current_offset;
        u64 decimal_part_length = 0;
        while ((state->current_offset < state->json_length)) {
            char current_digit = current(state);
            if (current_digit < '0' || current_digit > '9') {
                break;
            }

            decimal_part_length++;

            next(state);
        }
        if (decimal_part_length < 1) {
            return NULL;
        }

        i64 decimal_part = 0;
        for (u32 exponent = 1, i = decimal_part_start + (decimal_part_length - 1);
             i >= decimal_part_start;
             i--, exponent *= 10) {
            decimal_part += (state->json[i] - '0') * exponent;
        }

        if (is_negative)
            decimal_part *= -1;

        if ((current(state) != '.') && (current(state) != 'e') && (current(state) != 'E')) {
            value->type = JSON_VALUE_INTEGER;
            value->u.integer = decimal_part;
            return value;
        }

        f64 number = (f64)decimal_part;

        if (current(state) == '.') {
            next(state);

            if (current(state) < '0' || current(state) > '9') {
                return NULL;
            }

            f64 exponent = 0.1;
            while ((state->current_offset < state->json_length)) {
                char current_digit = current(state);
                if (current_digit < '0' || current_digit > '9') {
                    break;
                }

                number += (f64)(current_digit - '0') * exponent;

                exponent *= 0.1;
                next(state);
            }
        }
        if ((current(state) == 'e') || (current(state) == 'E')) {
            next(state);

            if (current(state) < '0' || current(state) > '9') {
                return NULL;
            }

            u64 exponential_part_start = state->current_offset;
            u64 exponential_part_length = 0;
            while ((state->current_offset < state->json_length)) {
                char current_digit = current(state);
                if (current_digit < '0' || current_digit > '9') {
                    break;
                }

                exponential_part_length++;

                next(state);
            }
            if (exponential_part_length < 1) {
                return NULL;
            }

            u64 exponential_part = 0;
            for (u32 exponent = 1, i = exponential_part_length + exponential_part_start;
                 i >= exponential_part_start;
                 i--, exponent *= 10) {
                exponential_part += (state->json[i] - '0') * exponent;
            }
            f64 exponent = pow(10, (f64)exponential_part);
            number *= exponent;
        }

        value->type = JSON_VALUE_NUMBER;
        value->u.number = number;
    } else if (current_char == 't') {
        next(state);
        if (assert_current(state, 'r') && assert_current(state, 'u') &&
            assert_current(state, 'e')) {
            value->type = JSON_VALUE_BOOLEAN;
            value->u.boolean = true;
        } else {
            return NULL;
        }
    } else if (current_char == 'f') {
        next(state);
        if (assert_current(state, 'a') && assert_current(state, 'l') &&
            assert_current(state, 's') && assert_current(state, 'e')) {
            value->type = JSON_VALUE_BOOLEAN;
            value->u.boolean = false;
        } else {
            return NULL;
        }
    } else if (current_char == 'n') {
        next(state);
        if (assert_current(state, 'u') && assert_current(state, 'l') &&
            assert_current(state, 'l')) {
            value->type = JSON_VALUE_NULL;
        } else {
            return NULL;
        }
    }

    skip_whitespace(state);

    return value;
}
