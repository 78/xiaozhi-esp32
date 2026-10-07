#ifndef CJSON_UTILS_H
#define CJSON_UTILS_H

#include <cJSON.h>

#include <memory>

struct CJsonDeleter {
    void operator()(cJSON* value) const {
        if (value != nullptr) {
            cJSON_Delete(value);
        }
    }
};

struct CJsonStringDeleter {
    void operator()(char* value) const {
        if (value != nullptr) {
            cJSON_free(value);
        }
    }
};

using CJsonUniquePtr = std::unique_ptr<cJSON, CJsonDeleter>;
using CJsonStringUniquePtr = std::unique_ptr<char, CJsonStringDeleter>;

#endif  // CJSON_UTILS_H
