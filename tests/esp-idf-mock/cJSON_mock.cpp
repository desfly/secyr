#include "cJSON.h"

#include <cstdlib>

extern "C" {

cJSON* cJSON_CreateObject(void) { return static_cast<cJSON*>(std::calloc(1, sizeof(cJSON))); }
void cJSON_Delete(cJSON* item) { std::free(item); }
cJSON* cJSON_AddObjectToObject(cJSON*, const char*) { return cJSON_CreateObject(); }
cJSON* cJSON_AddStringToObject(cJSON*, const char*, const char*) { return cJSON_CreateObject(); }
cJSON* cJSON_AddNumberToObject(cJSON*, const char*, double) { return cJSON_CreateObject(); }
cJSON* cJSON_AddBoolToObject(cJSON*, const char*, int) { return cJSON_CreateObject(); }
char* cJSON_PrintUnformatted(const cJSON*) { return nullptr; }
void cJSON_free(void* object) { std::free(object); }
cJSON* cJSON_ParseWithLength(const char*, size_t) { return nullptr; }
const cJSON* cJSON_GetObjectItemCaseSensitive(const cJSON*, const char*) { return nullptr; }
int cJSON_IsBool(const cJSON*) { return 0; }
int cJSON_IsTrue(const cJSON*) { return 0; }
int cJSON_IsString(const cJSON*) { return 0; }
int cJSON_IsNumber(const cJSON*) { return 0; }
int cJSON_IsObject(const cJSON*) { return 0; }

}
