#ifndef WEB_SEARCH_FORMAT_H_
#define WEB_SEARCH_FORMAT_H_

#include <cstddef>
#include <expected>
#include <string>

#include "cJSON.h"

// Ceiling for the raw Tavily response body. Responses run ~7 KB for five
// results; the cap keeps a hostile or broken endpoint from exhausting the
// heap on boards without PSRAM.
inline constexpr size_t kMaxWebSearchResponseBytes = 16384;

// Truncate to at most max_bytes without splitting a UTF-8 character: when the
// cut lands inside a multibyte sequence, the partial character is dropped.
inline std::string TruncateUtf8(std::string text, size_t max_bytes) {
    if (text.size() <= max_bytes) {
        return text;
    }
    size_t cut = max_bytes;
    while (cut > 0 && (static_cast<unsigned char>(text[cut]) & 0xC0) == 0x80) {
        --cut;
    }
    text.resize(cut);
    return text;
}

// Read the response body with a hard byte ceiling. Works for chunked
// responses without a Content-Length because it consumes the stream. Returns
// an error instead of the data when the ceiling is exceeded.
template <typename HttpT>
std::expected<std::string, std::string> ReadBounded(HttpT& http, size_t max_bytes) {
    std::string body;
    char buffer[1024];
    while (true) {
        auto read = http.Read(buffer, sizeof(buffer));
        if (!read) {
            return std::unexpected("Failed to read response body: " + read.error().ToString());
        }
        if (*read == 0) {
            return body;
        }
        if (body.size() + static_cast<size_t>(*read) > max_bytes) {
            return std::unexpected("Response body exceeds " + std::to_string(max_bytes) + " bytes");
        }
        body.append(buffer, static_cast<size_t>(*read));
    }
}

// Parse a Tavily search response and format it as tool output text. Snippet
// and result truncation happen on UTF-8 character boundaries so the MCP reply
// always decodes.
inline std::expected<std::string, std::string> FormatWebSearchResults(
    const std::string& response_json, const std::string& query) {
    constexpr size_t kSnippetMaxBytes = 300;
    constexpr size_t kResultMaxBytes = 5000;
    cJSON *resp_json = cJSON_Parse(response_json.c_str());
    if (!resp_json) {
        return std::unexpected("Failed to parse Tavily response");
    }
    std::string result;

    cJSON *answer = cJSON_GetObjectItem(resp_json, "answer");
    if (cJSON_IsString(answer) && answer->valuestring) {
        result += "Answer: ";
        result += answer->valuestring;
        result += "\n\n";
    }
    cJSON *results = cJSON_GetObjectItem(resp_json, "results");
    if (cJSON_IsArray(results)) {
        int count = cJSON_GetArraySize(results);
        for (int i = 0; i < count; i++) {
            cJSON *item = cJSON_GetArrayItem(results, i);
            cJSON *title = cJSON_GetObjectItem(item, "title");
            cJSON *url = cJSON_GetObjectItem(item, "url");
            cJSON *content = cJSON_GetObjectItem(item, "content");
            if (i > 0) result += "\n---\n";
            result += std::to_string(i + 1) + ". ";
            if (cJSON_IsString(title) && title->valuestring) {
                result += title->valuestring;
            }
            result += "\n";
            if (cJSON_IsString(url) && url->valuestring) {
                result += "   URL: ";
                result += url->valuestring;
                result += "\n";
            }
            if (cJSON_IsString(content) && content->valuestring) {
                result += "   ";
                std::string content_str = content->valuestring;
                std::string clipped = TruncateUtf8(content_str, kSnippetMaxBytes);
                if (clipped.size() < content_str.size()) {
                    clipped += "...";
                }
                result += clipped;
                result += "\n";
            }
        }
    }
    cJSON_Delete(resp_json);

    if (result.empty()) {
        result = "No results found for: " + query;
    }
    if (result.length() > kResultMaxBytes) {
        result = TruncateUtf8(result, kResultMaxBytes);
        result += "\n... (truncated)";
    }
    return result;
}

#endif  // WEB_SEARCH_FORMAT_H_
