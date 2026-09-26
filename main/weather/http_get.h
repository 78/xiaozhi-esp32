#ifndef HTTP_GET_H
#define HTTP_GET_H

#include <string>

struct HttpResult {
    int status = 0;  // HTTP status code; 0 on transport failure
    std::string body;
};

HttpResult HttpGet(const std::string& url, int timeout_ms = 5000);

// Percent-encode a string for use in a URL query value (UTF-8 bytes are
// encoded one by one, so Chinese city names are safe).
std::string UrlEncode(const std::string& value);

#endif  // HTTP_GET_H
