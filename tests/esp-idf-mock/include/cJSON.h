#pragma once

#include <cstddef>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct cJSON {
    struct cJSON* next;
    struct cJSON* prev;
    struct cJSON* child;
    int type;
    char* valuestring;
    int valueint;
    double valuedouble;
    char* string;
} cJSON;

cJSON* cJSON_CreateObject(void);
cJSON* cJSON_AddObjectToObject(cJSON* object, const char* name);
cJSON* cJSON_AddStringToObject(cJSON* object, const char* name, const char* string);
cJSON* cJSON_AddNumberToObject(cJSON* object, const char* name, double number);
cJSON* cJSON_AddBoolToObject(cJSON* object, const char* name, int boolean);
const cJSON* cJSON_GetObjectItemCaseSensitive(const cJSON* object, const char* string);
cJSON* cJSON_ParseWithLength(const char* value, std::size_t buffer_length);
char* cJSON_PrintUnformatted(const cJSON* item);
void cJSON_Delete(cJSON* item);
void cJSON_free(void* object);
int cJSON_IsBool(const cJSON* item);
int cJSON_IsTrue(const cJSON* item);
int cJSON_IsString(const cJSON* item);
int cJSON_IsNumber(const cJSON* item);
int cJSON_IsObject(const cJSON* item);

#ifdef __cplusplus
}
#endif
