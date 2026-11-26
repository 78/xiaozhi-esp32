#ifndef MCP_TOOLS_H
#define MCP_TOOLS_H

#include <esp_err.h>
#include <cstddef>
#include <string>

namespace McpTools {

inline constexpr std::size_t kTavilyApiKeyMaxLength = 128;

bool HasTavilyApiKey();
esp_err_t SaveTavilyApiKey(const std::string& key);

}  // namespace McpTools

#endif  // MCP_TOOLS_H
