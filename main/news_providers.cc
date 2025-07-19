#include "mcp_news_tools.h"
#include <esp_log.h>
#include <cJSON.h>
#include <http.h>
#include "board.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <cstring>

#define TAG "NewsProviders"

// ============================================================================
// Free News Provider Implementation (No API Key Required)
// ============================================================================

// RSS Feed URLs for different categories and languages
// Reliable sources with HTTPS and good stability
const std::map<std::string, std::vector<std::string>> RSS_FEEDS = {
    {"general", {
        "https://feeds.skynews.com/feeds/rss/world.xml",
        "https://www.yahoo.com/news/rss",
        "https://www.theguardian.com/world/rss",
        "https://feeds.feedburner.com/time/world",
        "https://feeds.reuters.com/reuters/topNews"
    }},
    {"technology", {
        "https://www.wired.com/feed/rss",
        "https://techcrunch.com/feed/",
        "https://feeds.feedburner.com/TechCrunch",
        "https://feeds.feedburner.com/oreilly/radar/atom10"
    }},
    {"business", {
        "https://feeds.a.dj.com/rss/RSSWorldNews.xml",
        "https://feeds.bloomberg.com/markets/news.rss",
        "https://www.theguardian.com/business/rss"
    }},
    {"sports", {
        "https://www.espn.com/espn/rss/news",
        "https://feeds.skynews.com/feeds/rss/sports.xml",
        "https://www.theguardian.com/sport/rss"
    }},
    {"science", {
        "https://feeds.nature.com/nature/rss/current",
        "https://www.sciencedaily.com/rss/all.xml",
        "https://www.theguardian.com/science/rss"
    }},
    {"health", {
        "https://feeds.medicalnewstoday.com/medical-news-today",
        "https://www.theguardian.com/society/health/rss"
    }}
};


// Improved HTTP client settings
const int HTTP_TIMEOUT_MS = 10000;  // 10 seconds
const int MAX_RETRIES = 3;
const int RETRY_DELAY_MS = 2000;    // 2 seconds between retries

// Helper function to configure HTTP headers for RSS requests
void ConfigureHttpHeaders(Http* http) {
    if (!http) return;
    
    // Set user agent to appear as a legitimate RSS reader
    http->SetHeader("User-Agent", "XiaoZhi RSS Reader/1.0 (+https://github.com/m5stack/xiaozhi-esp32)");
    
    // Accept RSS/XML content types
    http->SetHeader("Accept", "application/rss+xml, application/atom+xml, application/xml, text/xml, */*");
    
    // Accept encoding
    http->SetHeader("Accept-Encoding", "gzip, deflate");
    
    // Accept language
    http->SetHeader("Accept-Language", "fr-FR,fr;q=0.9,en-US;q=0.8,en;q=0.7,zh-CN;q=0.6,zh;q=0.5");
    
    // Connection settings
    http->SetHeader("Connection", "close");
    
    // Cache control
    http->SetHeader("Cache-Control", "no-cache");
}

// FreeNewsProvider implementation

FreeNewsProvider::FreeNewsProvider() {
    ESP_LOGI(TAG, "Initialized Free News provider (no API key required)");
}

NewsResponse FreeNewsProvider::SearchNews(const NewsSearchParams& params) {
    ESP_LOGI(TAG, "Searching news with query: %s, category: %s, language: %s", 
             params.query.c_str(), params.category.c_str(), params.language.c_str());
    
    NewsResponse response;
    response.search_query = params.query;
    response.source_apis = "FreeNews (RSS)";
    response.timestamp = GetCurrentTimestamp();
    
    // Get RSS feeds for the category and language
    std::vector<std::string> feeds = GetRssFeedsForCategory(params.category, params.language);
    
    if (feeds.empty()) {
        ESP_LOGW(TAG, "No RSS feeds found for category: %s, language: %s", 
                 params.category.c_str(), params.language.c_str());
        response.total_results = 0;
        return response;
    }
    
    // Try each feed until we get some articles
    for (const auto& feed_url : feeds) {
        ESP_LOGI(TAG, "Trying RSS feed: %s", feed_url.c_str());
        
        // Retry logic for each feed
        bool success = false;
        std::string response_data;
        
        for (int retry = 0; retry < MAX_RETRIES && !success; retry++) {
            if (retry > 0) {
                ESP_LOGI(TAG, "Retry attempt %d for feed: %s", retry, feed_url.c_str());
                vTaskDelay(pdMS_TO_TICKS(RETRY_DELAY_MS));
            }
        
        auto& board = Board::GetInstance();
        auto http = board.CreateHttp();
            
            // Configure HTTP headers for better compatibility
            ConfigureHttpHeaders(http);
        
        if (!http->Open("GET", feed_url)) {
                ESP_LOGW(TAG, "Failed to open HTTP connection to RSS feed: %s (attempt %d)", 
                         feed_url.c_str(), retry + 1);
            continue;
        }
        
        int status_code = http->GetStatusCode();
            
            // Handle redirections manually if needed
            if (status_code == 301 || status_code == 302 || status_code == 303 || status_code == 307 || status_code == 308) {
                ESP_LOGI(TAG, "Handling redirect %d for feed: %s", status_code, feed_url.c_str());
                
                // For now, we log the redirect but don't follow it automatically
                // as the HTTP client implementation may vary
                ESP_LOGI(TAG, "Received redirect %d - skipping for now", status_code);
                http->Close();
                continue;
            }
            
        if (status_code != 200) {
                ESP_LOGW(TAG, "RSS feed request failed with status: %d for %s (attempt %d)", 
                         status_code, feed_url.c_str(), retry + 1);
            http->Close();
            continue;
        }
        
            response_data = http->ReadAll();
        http->Close();
        
        if (response_data.empty()) {
                ESP_LOGW(TAG, "Empty response from RSS feed: %s (attempt %d)", 
                         feed_url.c_str(), retry + 1);
                continue;
            }
            
            success = true;
            ESP_LOGI(TAG, "Successfully retrieved RSS data from: %s (size: %d bytes)", 
                     feed_url.c_str(), (int)response_data.length());
        }
        
        if (!success) {
            ESP_LOGW(TAG, "Failed to retrieve RSS feed after %d attempts: %s", MAX_RETRIES, feed_url.c_str());
            continue;
        }
        
        // Parse RSS response
        NewsResponse feed_response = ParseRssResponse(response_data, params);
        
        ESP_LOGI(TAG, "Parsed %d articles from feed: %s", (int)feed_response.articles.size(), feed_url.c_str());
        
        // Merge articles
        for (const auto& article : feed_response.articles) {
            response.articles.push_back(article);
        }
        
        // If we have enough articles, stop
        if (response.articles.size() >= static_cast<size_t>(params.max_results)) {
            break;
        }
    }
    
    response.total_results = response.articles.size();
    ESP_LOGI(TAG, "Found %d articles from RSS feeds", (int)response.articles.size());
    
    // If no articles found, explicit error message
    if (response.articles.empty()) {
        ESP_LOGW(TAG, "No articles found in RSS feeds for query: %s", params.query.c_str());
        ESP_LOGW(TAG, "All RSS sources failed. Check network connectivity.");
    } else {
        ESP_LOGI(TAG, "Success! Articles retrieved from RSS feeds in real-time");
    }
    
    return response;
}

NewsResponse FreeNewsProvider::ParseRssResponse(const std::string& xml_data, 
                                               const NewsSearchParams& params) {
    NewsResponse response;
    response.search_query = params.query;
    response.source_apis = "FreeNews (RSS)";
    
    // Improved XML parsing for RSS feeds
    std::vector<NewsArticle> articles;
    
    // Convert to lowercase for case-insensitive searching
    std::string xml_lower = ToLower(xml_data);
    
    ESP_LOGI(TAG, "Parsing RSS/Atom feed (size: %d bytes)", (int)xml_data.length());
    
    // Try different RSS/Atom formats
    size_t pos = 0;
    bool is_atom = xml_lower.find("<feed") != std::string::npos;
    
    std::string item_tag = is_atom ? "entry" : "item";
    std::string title_tag = "title";
    std::string desc_tag = is_atom ? "summary" : "description";
    std::string link_tag = "link";
    std::string date_tag = is_atom ? "updated" : "pubdate";
    
    ESP_LOGI(TAG, "Detected feed format: %s", is_atom ? "Atom" : "RSS");
    ESP_LOGI(TAG, "Looking for <%s> tags in feed", item_tag.c_str());
    
    // Find all item/entry tags
    std::string item_start_tag = "<" + item_tag;
    std::string item_end_tag = "</" + item_tag + ">";
    
    int item_count = 0;
    while ((pos = xml_lower.find(item_start_tag, pos)) != std::string::npos) {
        item_count++;
        ESP_LOGD(TAG, "Found %s #%d at position %d", item_tag.c_str(), item_count, (int)pos);
        // Find the end of the opening tag (could have attributes)
        size_t tag_end = xml_data.find(">", pos);
        if (tag_end == std::string::npos) {
            break;
        }
        
        size_t item_end = xml_lower.find(item_end_tag, tag_end);
        
        if (item_end == std::string::npos) {
            ESP_LOGW(TAG, "Malformed XML: no closing tag for %s", item_tag.c_str());
            break;
        }
        
        std::string item_content = xml_data.substr(tag_end + 1, item_end - tag_end - 1);
        
        NewsArticle article;
        bool valid_article = false;
        
        // Extract title with multiple fallbacks
        std::string title = ExtractXmlTag(item_content, title_tag);
        if (!title.empty()) {
            article.title = CleanHtmlTags(title);
            if (article.title.length() > 5) { // Basic validation
                valid_article = true;
            }
        }
        
        // Extract description/summary with multiple fallbacks
        std::string description = ExtractXmlTag(item_content, desc_tag);
        if (description.empty() && !is_atom) {
            // Try content:encoded for RSS
            description = ExtractXmlTag(item_content, "content:encoded");
        }
        if (description.empty()) {
            // Try content for Atom
            description = ExtractXmlTag(item_content, "content");
        }
        if (!description.empty()) {
            article.description = CleanHtmlTags(description);
        }
        
        // Extract link (different handling for Atom vs RSS)
        std::string link;
        if (is_atom) {
            // Atom feeds use <link href="url" />
            size_t link_pos = item_content.find("<link");
            if (link_pos != std::string::npos) {
                size_t href_pos = item_content.find("href=\"", link_pos);
                if (href_pos != std::string::npos) {
                    href_pos += 6; // Skip 'href="'
                    size_t href_end = item_content.find("\"", href_pos);
                    if (href_end != std::string::npos) {
                        link = item_content.substr(href_pos, href_end - href_pos);
                    }
                }
            }
        } else {
            // RSS feeds use <link>url</link>
            link = ExtractXmlTag(item_content, link_tag);
        }
        
        if (!link.empty()) {
            article.url = link;
        }
        
        // Extract publication date
        std::string pubDate = ExtractXmlTag(item_content, date_tag);
        if (pubDate.empty() && !is_atom) {
            // Try dc:date for some RSS feeds
            pubDate = ExtractXmlTag(item_content, "dc:date");
        }
        if (!pubDate.empty()) {
            article.published_at = pubDate;
        }
        
        // Extract source/author
        std::string source;
        if (is_atom) {
            source = ExtractXmlTag(item_content, "author");
            if (source.empty()) {
                source = ExtractXmlTag(item_content, "name");
            }
        } else {
            source = ExtractXmlTag(item_content, "source");
            if (source.empty()) {
                source = ExtractXmlTag(item_content, "dc:creator");
            }
        }
        
        if (!source.empty()) {
            article.source = CleanHtmlTags(source);
        } else {
            article.source = "RSS Feed";
        }
        
        // Set category and language
        article.category = params.category;
        article.language = params.language;
        
        // Calculate relevance score - more lenient scoring
        double score = CalculateRelevanceScore(article, params.query);
        article.relevance_score = score;
        
        // More lenient article acceptance: accept all valid articles for general browsing
        // Only filter by relevance if there's a specific query
        bool should_include = valid_article;
        if (!params.query.empty() && params.query != "general" && params.query != "headlines") {
            should_include = valid_article && score > 0.0;
        }
        
        if (should_include) {
            articles.push_back(article);
            ESP_LOGD(TAG, "Added article: %s (score: %.2f)", 
                     article.title.length() > 50 ? 
                     std::string(article.title.substr(0, 47) + "...").c_str() : 
                     article.title.c_str(), score);
        }
        
        pos = item_end + item_end_tag.length();
        
        // Limit the number of articles to avoid memory issues
        if (articles.size() >= 50) {
            ESP_LOGI(TAG, "Reached article limit, stopping parsing");
            break;
        }
    }
    
    response.articles = articles;
    response.total_results = articles.size();
    
    ESP_LOGI(TAG, "Successfully parsed %d articles from %s feed", 
             (int)articles.size(), is_atom ? "Atom" : "RSS");
    
    return response;
}

std::vector<std::string> FreeNewsProvider::GetRssFeedsForCategory(const std::string& category, 
                                                                 const std::string& language) {

    
    // Get feeds for the requested category
    auto it = RSS_FEEDS.find(category);
    if (it != RSS_FEEDS.end()) {
        return it->second;
    }
    
    // Fallback to general feeds
    auto general_it = RSS_FEEDS.find("general");
    if (general_it != RSS_FEEDS.end()) {
        return general_it->second;
    }
    
    return {};
}

std::string FreeNewsProvider::CleanHtmlTags(const std::string& html) {
    std::string cleaned = html;
    
    // Remove HTML tags
    size_t pos = 0;
    while ((pos = cleaned.find('<', pos)) != std::string::npos) {
        size_t end_pos = cleaned.find('>', pos);
        if (end_pos != std::string::npos) {
            cleaned.erase(pos, end_pos - pos + 1);
        } else {
            // Malformed HTML, just remove the '<'
            cleaned.erase(pos, 1);
        }
    }
    // Decode HTML entities - comprehensive list
    struct HtmlEntity {
        const char* entity;
        const char* replacement;
    };
    
    static const HtmlEntity entities[] = {
        {"&amp;", "&"},
        {"&lt;", "<"},
        {"&gt;", ">"},
        {"&quot;", "\""},
        {"&apos;", "'"},
        {"&nbsp;", " "},
        {"&ndash;", "-"},
        {"&mdash;", "-"},
        {"&lsquo;", "'"},
        {"&rsquo;", "'"},
        {"&ldquo;", "\""},
        {"&rdquo;", "\""},
        {"&hellip;", "..."},
        {"&copy;", "(c)"},
        {"&reg;", "(R)"},
        {"&trade;", "(TM)"},
        {"&#8211;", "-"},
        {"&#8212;", "-"},
        {"&#8216;", "'"},
        {"&#8217;", "'"},
        {"&#8220;", "\""},
        {"&#8221;", "\""},
        {"&#8230;", "..."},
        {"&#39;", "'"},
        {"&#x27;", "'"},
        {"&#x2F;", "/"}
    };
    
    for (const auto& entity : entities) {
        size_t entity_pos = 0;
        while ((entity_pos = cleaned.find(entity.entity, entity_pos)) != std::string::npos) {
            cleaned.replace(entity_pos, strlen(entity.entity), entity.replacement);
            entity_pos += strlen(entity.replacement);
        }
    }
    
    // Remove extra whitespace and normalize
    std::string result;
    result.reserve(cleaned.length());
    
    bool last_was_space = false;
    for (char c : cleaned) {
        if (c == '\n' || c == '\r' || c == '\t') {
            c = ' ';
    }
    
        if (c == ' ') {
            if (!last_was_space) {
                result += c;
                last_was_space = true;
            }
        } else {
            result += c;
            last_was_space = false;
        }
    }
    
    // Trim leading and trailing whitespace
    size_t start = result.find_first_not_of(" \t\n\r");
    if (start == std::string::npos) return "";
    size_t end = result.find_last_not_of(" \t\n\r");
    
    return result.substr(start, end - start + 1);
}

// Helper function to extract XML tags - Enhanced version
std::string ExtractXmlTag(const std::string& content, const std::string& tag_name) {
    std::string lower_content = ToLower(content);
    std::string lower_tag = ToLower(tag_name);
    
    // Try different tag formats
    std::vector<std::string> tag_formats = {
        "<" + lower_tag + ">",
        "<" + lower_tag + " ",
        "<" + tag_name + ">",
        "<" + tag_name + " "
    };
    
    for (const auto& format : tag_formats) {
        size_t start = lower_content.find(format);
        if (start != std::string::npos) {
            // Find the actual opening tag end (handle attributes)
            size_t tag_end = content.find(">", start);
            if (tag_end == std::string::npos) continue;
            
            // Check if it's a self-closing tag
            if (content[tag_end - 1] == '/') {
                continue; // Skip self-closing tags
            }
            
            // Find the closing tag
            std::string closing_tag = "</" + lower_tag + ">";
            std::string closing_tag_alt = "</" + tag_name + ">";
            
            size_t end = lower_content.find(closing_tag, tag_end);
            if (end == std::string::npos) {
                end = lower_content.find(closing_tag_alt, tag_end);
            }
            
            if (end != std::string::npos) {
                // Extract content between tags
                size_t content_start = tag_end + 1;
                std::string extracted = content.substr(content_start, end - content_start);
                
                // Handle CDATA sections
                if (extracted.find("<![CDATA[") == 0) {
                    size_t cdata_start = 9; // Length of "<![CDATA["
                    size_t cdata_end = extracted.find("]]>");
                    if (cdata_end != std::string::npos) {
                        extracted = extracted.substr(cdata_start, cdata_end - cdata_start);
                    }
                }
                
                // Trim whitespace
                size_t first = extracted.find_first_not_of(" \t\n\r");
                if (first == std::string::npos) return "";
                size_t last = extracted.find_last_not_of(" \t\n\r");
                extracted = extracted.substr(first, last - first + 1);
                
                return extracted;
            }
        }
    }
    
    return "";
}

// Calculate relevance score for articles
double CalculateRelevanceScore(const NewsArticle& article, const std::string& query) {
    if (query.empty() || query == "general" || query == "headlines") {
        return 1.0; // Default score for general browsing
    }
    
    double score = 0.0;
    std::string query_lower = ToLower(query);
    std::string title_lower = ToLower(article.title);
    std::string desc_lower = ToLower(article.description);
    
    // Split query into individual words
    std::vector<std::string> query_words = SplitString(query_lower, " ");
    
    for (const auto& word : query_words) {
        if (word.length() < 2) continue; // Skip very short words
        
        // Exact word match in title (high score)
        if (title_lower.find(word) != std::string::npos) {
            score += 3.0;
        }
        
        // Exact word match in description (medium score)
        if (desc_lower.find(word) != std::string::npos) {
            score += 1.5;
        }
        
        // Partial matches for common terms
        if (word.length() >= 4) {
            // Check for partial matches (first 4 characters)
            std::string word_prefix = word.substr(0, 4);
            if (title_lower.find(word_prefix) != std::string::npos) {
                score += 1.0;
            }
            if (desc_lower.find(word_prefix) != std::string::npos) {
                score += 0.5;
            }
        }
    }
    
    // Boost score for recent articles (basic heuristic)
    if (!article.published_at.empty()) {
        score += 0.5; // Bonus for having publication date
    }
    
    return score;
}

std::vector<std::string> FreeNewsProvider::GetSupportedCategories() const {
    return {"general", "business", "technology", "sports", "health", "science"};
}

std::vector<std::string> FreeNewsProvider::GetSupportedLanguages() const {
    return {"zh", "en", "ja", "ko", "fr", "es", "de", "it", "pt", "ru", "ar", "hi"};
}

std::vector<std::string> FreeNewsProvider::GetSupportedCountries() const {
    return {"cn", "us", "gb", "jp", "kr", "in", "fr", "de", "br", "ru", "ca", "au"};
}

// ============================================================================
// Utility Functions - Defined in mcp_news_tools.cc
// ============================================================================

// These functions are already defined in mcp_news_tools.cc
// No need to redefine them here to avoid multiple definition errors 