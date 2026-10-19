#ifndef SEEDLESSDS_CONFIG_JSON_H
#define SEEDLESSDS_CONFIG_JSON_H
#include <stddef.h>
#include <stdint.h>

typedef enum seedless_json_type { SEEDLESS_JSON_OBJECT, SEEDLESS_JSON_ARRAY,
    SEEDLESS_JSON_STRING, SEEDLESS_JSON_INTEGER, SEEDLESS_JSON_BOOL,
    SEEDLESS_JSON_NULL, SEEDLESS_JSON_NUMBER } seedless_json_type_t;
typedef struct seedless_json_value seedless_json_value_t;
typedef struct seedless_json_member {
    char *key;
    seedless_json_value_t *value;
    struct seedless_json_member *next;
} seedless_json_member_t;
struct seedless_json_value {
    seedless_json_type_t type;
    union { char *string; int64_t integer; int boolean; } as;
    seedless_json_member_t *members;
    seedless_json_value_t *items;
    seedless_json_value_t *next;
};
typedef struct seedless_json_doc { seedless_json_value_t *root; size_t nodes; } seedless_json_doc_t;
int seedless_json_parse(const char *data, size_t length, seedless_json_doc_t *doc,
                        char *error, size_t error_size);
void seedless_json_free(seedless_json_doc_t *doc);
const seedless_json_value_t *seedless_json_get(const seedless_json_value_t *object,
                                                const char *key);
int seedless_json_write_string(const char *value, char *out, size_t capacity,
                               size_t *written);
#endif
