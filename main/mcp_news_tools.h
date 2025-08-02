#ifndef MCP_NEWS_TOOLS_H
#define MCP_NEWS_TOOLS_H

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <map>
#include <mutex>
#include <chrono>
#include "mcp_server.h"

// Structure to represent a news article
struct NewsArticle {
    std::string title;
    std::string description;
    std::string content;
    std::string url;
    std::string source;
    std::string published_at;
    std::string category;
    std::string language;
    double relevance_score;
    std::vector<std::string> keywords;
};

// Structure for search parameters
struct NewsSearchParams {
    std::string query;
    std::string category;
    std::string language;
    std::string country;
    std::string sort_by;
    int max_results;
    std::string date_from;
    std::string date_to;
    bool include_content;
};

// Structure for news response
struct NewsResponse {
    std::vector<NewsArticle> articles;
    int total_results;
    std::string search_query;
    std::string timestamp;
    std::string source_apis;
    bool cached;
};

// Interface for news providers
class NewsProvider {
public:
    virtual ~NewsProvider() = default;
    virtual std::string GetName() const = 0;
    virtual bool IsAvailable() const = 0;
    virtual NewsResponse SearchNews(const NewsSearchParams& params) = 0;
    virtual std::vector<std::string> GetSupportedCategories() const = 0;
    virtual std::vector<std::string> GetSupportedLanguages() const = 0;
    virtual std::vector<std::string> GetSupportedCountries() const = 0;
};

// Main news manager
class NewsManager {
private:
    std::vector<std::unique_ptr<NewsProvider>> providers_;
    std::map<std::string, NewsResponse> cache_;
    std::mutex cache_mutex_;
    int cache_duration_minutes_;
    int max_cache_size_;
    bool enable_cache_;
    
    // Private methods
    NewsResponse SearchFromAllProviders(const NewsSearchParams& params);
    std::string GenerateCacheKey(const NewsSearchParams& params);
    bool IsCacheValid(const std::string& cache_key);
    void CleanupCache();
    NewsResponse MergeResults(const std::vector<NewsResponse>& responses);


public:
    NewsManager();
    ~NewsManager() = default;
    
    // Configuration
    void SetCacheDuration(int minutes);
    void SetMaxCacheSize(int size);
    void EnableCache(bool enable);
    
    // News search methods
    NewsResponse SearchNews(const NewsSearchParams& params);
    NewsResponse GetHeadlines(const std::string& category = "general", 
                             const std::string& country = "", 
                             int max_results = 10,
                             const std::string& language = "");
    NewsResponse GetTrendingTopics(const std::string& language = "", int max_results = 5);
    
    // Provider management
    void AddProvider(std::unique_ptr<NewsProvider> provider);
    std::vector<std::string> GetAvailableProviders() const;
    
    // Utilities
    std::vector<std::string> GetSupportedCategories() const;
    std::vector<std::string> GetSupportedLanguages() const;
    std::vector<std::string> GetSupportedCountries() const;

    std::string FormatResponseForAI(const NewsResponse& response);
    
    // Language mapping
    std::string MapLanguageToProvider(const std::string& language);
    std::string MapCountryToProvider(const std::string& country);
};

// Free News Provider (No API Key Required)
class FreeNewsProvider : public NewsProvider {
private:
    std::string base_url_;
    
    // Private methods
    NewsResponse ParseRssResponse(const std::string& xml_data, const NewsSearchParams& params);
    std::vector<std::string> GetRssFeedsForCategory(const std::string& category, const std::string& language);
    std::string CleanHtmlTags(const std::string& html);
    
public:
    FreeNewsProvider();
    std::string GetName() const override { return "FreeNews"; }
    bool IsAvailable() const override { return true; }  // Always available
    NewsResponse SearchNews(const NewsSearchParams& params) override;
    std::vector<std::string> GetSupportedCategories() const override;
    std::vector<std::string> GetSupportedLanguages() const override;
    std::vector<std::string> GetSupportedCountries() const override;
};

// Utility functions
std::string GetCurrentTimestamp();
std::chrono::system_clock::time_point ParseTimestamp(const std::string& timestamp);
std::string UrlEncode(const std::string& str);
std::string ToLower(const std::string& str);
std::vector<std::string> SplitString(const std::string& str, const std::string& delimiters);
std::string ExtractXmlTag(const std::string& content, const std::string& tag_name);
double CalculateRelevanceScore(const NewsArticle& article, const std::string& query);

// MCP tools initialization
void AddNewsMcpTools();

#endif // MCP_NEWS_TOOLS_H 