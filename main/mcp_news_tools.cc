#include "mcp_news_tools.h"
#include <esp_log.h>
#include <cJSON.h>
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <ctime>
#include <locale>
#include <codecvt>
#include "board.h"
#include "settings.h"
#include "assets/lang_config.h"

#define TAG "NewsTools"

// Default configuration
static const int DEFAULT_CACHE_DURATION = 15; // 15 minutes
static const int DEFAULT_MAX_CACHE_SIZE = 50;
static const int DEFAULT_MAX_RESULTS = 10;

// Global news manager instance
static std::unique_ptr<NewsManager> g_news_manager;

// Language mapping for better internationalization
static const std::map<std::string, std::string> LANGUAGE_MAPPING = {
    {"zh", "zh"}, {"zh-cn", "zh"}, {"zh-tw", "zh"}, {"chinese", "zh"},
    {"en", "en"}, {"english", "en"},
    {"fr", "fr"}, {"french", "fr"},
    {"es", "es"}, {"spanish", "es"},
    {"de", "de"}, {"german", "de"},
    {"it", "it"}, {"italian", "it"},
    {"pt", "pt"}, {"portuguese", "pt"},
    {"ru", "ru"}, {"russian", "ru"},
    {"ja", "ja"}, {"japanese", "ja"},
    {"ko", "ko"}, {"korean", "ko"},
    {"ar", "ar"}, {"arabic", "ar"},
    {"hi", "hi"}, {"hindi", "hi"}
};

// Country mapping for news sources
static const std::map<std::string, std::string> COUNTRY_MAPPING = {
    {"cn", "cn"}, {"china", "cn"}, {"chinese", "cn"},
    {"us", "us"}, {"usa", "us"}, {"united states", "us"},
    {"gb", "gb"}, {"uk", "gb"}, {"united kingdom", "gb"},
    {"fr", "fr"}, {"france", "fr"},
    {"de", "de"}, {"germany", "de"},
    {"jp", "jp"}, {"japan", "jp"},
    {"kr", "kr"}, {"korea", "kr"},
    {"in", "in"}, {"india", "in"},
    {"br", "br"}, {"brazil", "br"},
    {"ru", "ru"}, {"russia", "ru"},
    {"ca", "ca"}, {"canada", "ca"},
    {"au", "au"}, {"australia", "au"}
};

// ============================================================================
// NewsManager Implementation
// ============================================================================

NewsManager::NewsManager() {
    // Load configuration from settings
    Settings settings("news", false);
    cache_duration_minutes_ = settings.GetInt("cache_duration", DEFAULT_CACHE_DURATION);
    max_cache_size_ = settings.GetInt("max_cache_size", DEFAULT_MAX_CACHE_SIZE);
    enable_cache_ = settings.GetInt("enable_cache", 1) == 1;
    
    ESP_LOGI(TAG, "Cache: %s, Duration: %d min, Max size: %d", 
             enable_cache_ ? "enabled" : "disabled", 
             cache_duration_minutes_, max_cache_size_);
}

void NewsManager::SetCacheDuration(int minutes) {
    cache_duration_minutes_ = minutes;
    Settings settings("news", true);
    settings.SetInt("cache_duration", minutes);
}

void NewsManager::SetMaxCacheSize(int size) {
    max_cache_size_ = size;
    Settings settings("news", true);
    settings.SetInt("max_cache_size", size);
}

void NewsManager::EnableCache(bool enable) {
    enable_cache_ = enable;
    Settings settings("news", true);
    settings.SetInt("enable_cache", enable ? 1 : 0);
}

void NewsManager::AddProvider(std::unique_ptr<NewsProvider> provider) {
    if (provider && provider->IsAvailable()) {
        providers_.push_back(std::move(provider));
        ESP_LOGI(TAG, "Added news provider: %s", providers_.back()->GetName().c_str());
    } else {
        ESP_LOGW(TAG, "Failed to add news provider: not available");
    }
}

std::vector<std::string> NewsManager::GetAvailableProviders() const {
    std::vector<std::string> names;
    for (const auto& provider : providers_) {
        names.push_back(provider->GetName());
    }
    return names;
}

NewsResponse NewsManager::SearchNews(const NewsSearchParams& params) {
    ESP_LOGI(TAG, "Searching news for: '%s' (category: %s, lang: %s, max: %d)", 
             params.query.c_str(), params.category.c_str(), 
             params.language.c_str(), params.max_results);
    
    // Check cache
    if (enable_cache_) {
        std::string cache_key = GenerateCacheKey(params);
        std::lock_guard<std::mutex> lock(cache_mutex_);
        
        if (IsCacheValid(cache_key)) {
            ESP_LOGI(TAG, "Returning cached results for: %s", params.query.c_str());
            auto& cached_response = cache_[cache_key];
            cached_response.cached = true;
            return cached_response;
        }
    }
    
    // Search from all providers
    NewsResponse response = SearchFromAllProviders(params);
    
    // Cache results
    if (enable_cache_ && !response.articles.empty()) {
        std::string cache_key = GenerateCacheKey(params);
        std::lock_guard<std::mutex> lock(cache_mutex_);
        
        CleanupCache();
        response.cached = false;
        response.timestamp = GetCurrentTimestamp();
        cache_[cache_key] = response;
        
        ESP_LOGI(TAG, "Cached %d articles for query: %s", 
                 response.articles.size(), params.query.c_str());
    }
    
    return response;
}

NewsResponse NewsManager::GetHeadlines(const std::string& category, 
                                       const std::string& country, 
                                       int max_results,
                                       const std::string& language) {
    NewsSearchParams params;
    params.category = category;
    params.country = MapCountryToProvider(country);
    params.max_results = max_results;
    params.sort_by = "publishedAt";
    params.language = MapLanguageToProvider(language);
    
    return SearchNews(params);
}

NewsResponse NewsManager::GetTrendingTopics(const std::string& language, int max_results) {
    NewsSearchParams params;
    params.language = MapLanguageToProvider(language);
    params.max_results = max_results;
    params.sort_by = "relevancy";
    
    return SearchNews(params);
}

NewsResponse NewsManager::SearchFromAllProviders(const NewsSearchParams& params) {
    std::vector<NewsResponse> responses;
    
    for (const auto& provider : providers_) {
        try {
            ESP_LOGD(TAG, "Searching with provider: %s", provider->GetName().c_str());
            NewsResponse response = provider->SearchNews(params);
            if (!response.articles.empty()) {
                responses.push_back(response);
            }
        } catch (const std::exception& e) {
            ESP_LOGW(TAG, "Provider %s failed: %s", 
                     provider->GetName().c_str(), e.what());
        }
    }
    
    if (responses.empty()) {
        ESP_LOGW(TAG, "No results from any provider");
        return NewsResponse{};
    }
    
    return MergeResults(responses);
}

std::string NewsManager::GenerateCacheKey(const NewsSearchParams& params) {
    std::ostringstream oss;
    oss << params.query << "|" 
        << params.category << "|" 
        << params.language << "|" 
        << params.country << "|" 
        << params.max_results << "|" 
        << params.sort_by;
    return oss.str();
}

bool NewsManager::IsCacheValid(const std::string& cache_key) {
    auto it = cache_.find(cache_key);
    if (it == cache_.end()) {
        return false;
    }
    
    // Check cache age
    auto now = std::chrono::system_clock::now();
    auto cache_time = ParseTimestamp(it->second.timestamp);
    auto age = std::chrono::duration_cast<std::chrono::minutes>(now - cache_time);
    
    return age.count() < cache_duration_minutes_;
}

void NewsManager::CleanupCache() {
    if (cache_.size() <= max_cache_size_) {
        return;
    }
    
    // Remove oldest entries
    std::vector<std::pair<std::string, std::chrono::system_clock::time_point>> entries;
    
    for (const auto& entry : cache_) {
        auto cache_time = ParseTimestamp(entry.second.timestamp);
        entries.emplace_back(entry.first, cache_time);
    }
    
    std::sort(entries.begin(), entries.end(), 
              [](const auto& a, const auto& b) { return a.second < b.second; });
    
    int to_remove = cache_.size() - max_cache_size_;
    for (int i = 0; i < to_remove; ++i) {
        cache_.erase(entries[i].first);
    }
    
    ESP_LOGI(TAG, "Cleaned up cache, removed %d entries", to_remove);
}

NewsResponse NewsManager::MergeResults(const std::vector<NewsResponse>& responses) {
    NewsResponse merged;
    merged.total_results = 0;
    merged.source_apis = "";
    
    // Merge all articles
    for (const auto& response : responses) {
        merged.articles.insert(merged.articles.end(), 
                              response.articles.begin(), 
                              response.articles.end());
        merged.total_results += response.total_results;
        
        if (!merged.source_apis.empty()) {
            merged.source_apis += ", ";
        }
        merged.source_apis += response.source_apis;
    }
    
    // Sort by relevance and date
    std::sort(merged.articles.begin(), merged.articles.end(),
              [](const NewsArticle& a, const NewsArticle& b) {
                  if (a.relevance_score != b.relevance_score) {
                      return a.relevance_score > b.relevance_score;
                  }
                  return a.published_at > b.published_at;
              });
    
    // Remove duplicates based on URL
    auto it = std::unique(merged.articles.begin(), merged.articles.end(),
                         [](const NewsArticle& a, const NewsArticle& b) {
                             return a.url == b.url;
                         });
    merged.articles.erase(it, merged.articles.end());
    
    ESP_LOGI(TAG, "Merged %d articles from %d providers", 
             (int)merged.articles.size(), (int)responses.size());
    
    return merged;
}

std::string NewsManager::FormatResponseForAI(const NewsResponse& response) {
    cJSON* root = cJSON_CreateObject();
    
    cJSON_AddNumberToObject(root, "total_results", response.total_results);
    cJSON_AddStringToObject(root, "search_query", response.search_query.c_str());
    cJSON_AddStringToObject(root, "timestamp", response.timestamp.c_str());
    cJSON_AddStringToObject(root, "source_apis", response.source_apis.c_str());
    cJSON_AddBoolToObject(root, "cached", response.cached);
    
    cJSON* articles = cJSON_CreateArray();
    for (const auto& article : response.articles) {
        cJSON* article_obj = cJSON_CreateObject();
        cJSON_AddStringToObject(article_obj, "title", article.title.c_str());
        cJSON_AddStringToObject(article_obj, "description", article.description.c_str());
        cJSON_AddStringToObject(article_obj, "url", article.url.c_str());
        cJSON_AddStringToObject(article_obj, "source", article.source.c_str());
        cJSON_AddStringToObject(article_obj, "published_at", article.published_at.c_str());
        cJSON_AddStringToObject(article_obj, "category", article.category.c_str());
        cJSON_AddNumberToObject(article_obj, "relevance_score", article.relevance_score);
        
        cJSON_AddItemToArray(articles, article_obj);
    }
    cJSON_AddItemToObject(root, "articles", articles);
    
    char* json_str = cJSON_PrintUnformatted(root);
    std::string result(json_str);
    cJSON_free(json_str);
    cJSON_Delete(root);
    
    return result;
}



std::vector<std::string> NewsManager::GetSupportedCategories() const {
    return {"general", "business", "technology", "sports", "health", "science"};
}

std::vector<std::string> NewsManager::GetSupportedLanguages() const {
    return {"zh", "en", "ja", "ko", "fr", "es", "de", "it", "pt", "ru", "ar", "hi"};
}

std::vector<std::string> NewsManager::GetSupportedCountries() const {
    return {"cn", "us", "gb", "jp", "kr", "in", "fr", "de", "br", "ru", "ca", "au"};
}

std::string NewsManager::MapLanguageToProvider(const std::string& language) {
    if (language.empty()) {
        // Use system language
        return std::string(Lang::CODE);
    }
    
    auto it = LANGUAGE_MAPPING.find(ToLower(language));
    if (it != LANGUAGE_MAPPING.end()) {
        return it->second;
    }
    return std::string(Lang::CODE); // Default to system language
}

std::string NewsManager::MapCountryToProvider(const std::string& country) {
    if (country.empty()) {
        // Smart default based on system language
        std::string lang = ToLower(std::string(Lang::CODE));
        if (lang.find("zh") != std::string::npos) return "cn";
        if (lang.find("ja") != std::string::npos) return "jp";
        if (lang.find("fr") != std::string::npos) return "fr";
        if (lang.find("de") != std::string::npos) return "de";
        if (lang.find("ko") != std::string::npos) return "kr";
        return "us"; // Default for English and others
    }
    
    auto it = COUNTRY_MAPPING.find(ToLower(country));
    if (it != COUNTRY_MAPPING.end()) {
        return it->second;
    }
    return "us"; // Default to US (most international RSS sources)
}

// ============================================================================
// Utility Functions
// ============================================================================

std::string GetCurrentTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::gmtime(&time_t), "%Y-%m-%dT%H:%M:%SZ");
    return ss.str();
}

std::chrono::system_clock::time_point ParseTimestamp(const std::string& timestamp) {
    std::tm tm = {};
    std::istringstream ss(timestamp);
    ss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
    return std::chrono::system_clock::from_time_t(std::mktime(&tm));
}

std::string UrlEncode(const std::string& str) {
    std::string encoded;
    for (char c : str) {
        if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            encoded += c;
        } else if (c == ' ') {
            encoded += "%20";
        } else {
            char hex[4];
            snprintf(hex, sizeof(hex), "%%%02X", (unsigned char)c);
            encoded += hex;
        }
    }
    return encoded;
}

std::string ToLower(const std::string& str) {
    std::string lower = str;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    return lower;
}

std::vector<std::string> SplitString(const std::string& str, const std::string& delimiters) {
    std::vector<std::string> tokens;
    std::string::size_type start = str.find_first_not_of(delimiters);
    
    while (start != std::string::npos) {
        std::string::size_type end = str.find_first_of(delimiters, start);
        if (end == std::string::npos) {
            end = str.length();
        }
        tokens.push_back(str.substr(start, end - start));
        start = str.find_first_not_of(delimiters, end);
    }
    
    return tokens;
}

// ============================================================================
// MCP Tools Implementation
// ============================================================================

void AddNewsMcpTools() {
    if (!g_news_manager) {
        g_news_manager = std::make_unique<NewsManager>();
        
        // Add free news provider (no API key required)
        g_news_manager->AddProvider(std::make_unique<FreeNewsProvider>());
        
        ESP_LOGI(TAG, "Initialized news manager with %d providers", 
                 (int)g_news_manager->GetAvailableProviders().size());
    }
    
    auto& mcp_server = McpServer::GetInstance();
    
    // Main news search tool
    mcp_server.AddTool("self.news.search",
        "Search for news articles by keyword or topic. 搜索关键词或主题的新闻文章。\n\n"
        "Usage examples / 使用示例:\n"
        "- 'Search for latest news about Trump' / '搜索特朗普的最新消息'\n"
        "- 'What are the technology news?' / '有什么科技新闻?'\n"
        "- 'News about climate change' / '关于气候变化的新闻'\n\n"
        "Returns articles with title, description, source and publication date.\n"
        "返回包含标题、描述、来源和发布日期的文章。",
        PropertyList({
            Property("query", kPropertyTypeString),
            Property("category", kPropertyTypeString, "general"),
            Property("language", kPropertyTypeString, Lang::CODE),
            Property("country", kPropertyTypeString, ""),
            Property("max_results", kPropertyTypeInteger, 10, 1, 20),
            Property("sort_by", kPropertyTypeString, "relevancy")
        }),
        [](const PropertyList& properties) -> ReturnValue {
            if (!g_news_manager) {
                return "{\"error\":\"News manager not initialized\"}";
            }
            
            NewsSearchParams params;
            params.query = properties["query"].value<std::string>();
            params.category = properties["category"].value<std::string>();
            params.language = g_news_manager->MapLanguageToProvider(properties["language"].value<std::string>());
            params.country = g_news_manager->MapCountryToProvider(properties["country"].value<std::string>());
            params.max_results = properties["max_results"].value<int>();
            params.sort_by = properties["sort_by"].value<std::string>();
            
            NewsResponse response = g_news_manager->SearchNews(params);
            return g_news_manager->FormatResponseForAI(response);
        });
    
    // Headlines tool
    mcp_server.AddTool("self.news.get_headlines",
        "Get the main headlines of the day. Use this tool when the user asks for 'latest news' or 'what's happening' without specifying a particular topic.\n\n"
        "The tool returns the most important articles of the moment, sorted by popularity.",
        PropertyList({
            Property("category", kPropertyTypeString, "general"),
            Property("country", kPropertyTypeString, ""),
            Property("language", kPropertyTypeString, Lang::CODE),
            Property("max_results", kPropertyTypeInteger, 10, 1, 20)
        }),
        [](const PropertyList& properties) -> ReturnValue {
            if (!g_news_manager) {
                return "{\"error\":\"News manager not initialized\"}";
            }
            
            std::string category = properties["category"].value<std::string>();
            std::string country = properties["country"].value<std::string>();
            std::string language = properties["language"].value<std::string>();
            int max_results = properties["max_results"].value<int>();
            
            NewsResponse response = g_news_manager->GetHeadlines(category, country, max_results, language);
            return g_news_manager->FormatResponseForAI(response);
        });
    
    // Trending topics tool
    mcp_server.AddTool("self.news.get_trending",
        "Get trending and popular topics. Use this tool when the user asks for 'what's trending' or 'popular topics'.\n\n"
        "The tool returns the most discussed topics currently.",
        PropertyList({
            Property("language", kPropertyTypeString, Lang::CODE),
            Property("max_results", kPropertyTypeInteger, 5, 1, 10)
        }),
        [](const PropertyList& properties) -> ReturnValue {
            if (!g_news_manager) {
                return "{\"error\":\"News manager not initialized\"}";
            }
            
            std::string language = properties["language"].value<std::string>();
            int max_results = properties["max_results"].value<int>();
            
            NewsResponse response = g_news_manager->GetTrendingTopics(language, max_results);
            return g_news_manager->FormatResponseForAI(response);
        });    
    } 