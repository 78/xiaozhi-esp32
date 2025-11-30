#include "news_client.h"
#include "board.h"
#include <esp_log.h>
#include <cJSON.h>
#include <cstring>

#define TAG "NewsClient"

// Vietnamese News RSS endpoints
static const char *VNEXPRESS_RSS = "https://vnexpress.net/rss";
static const char *TUOITRE_RSS = "https://tuoitre.vn/rss";
static const char *DANTRI_RSS = "https://dantri.com.vn/rss";

// Category mappings for different news sources
static const char *VNEXPRESS_CATEGORIES[] = {
    "thoi-su.rss",
    "goc-nhin.rss",
    "the-gioi.rss",
    "kinh-doanh.rss",
    "giai-tri.rss",
    "the-thao.rss",
    "phap-luat.rss",
    "giao-duc.rss",
    "suc-khoe.rss",
    "gia-dinh.rss",
    "du-lich.rss",
    "so-hoa.rss",
    "xe.rss"};

NewsClient::NewsClient() : initialized_(false)
{
}

NewsClient::~NewsClient()
{
}

void NewsClient::Initialize()
{
    initialized_ = true;
    ESP_LOGI(TAG, "Vietnamese News Client initialized");
}

std::vector<NewsArticle> NewsClient::GetLatestNews(const std::string &category, int limit)
{
    if (!initialized_)
    {
        ESP_LOGE(TAG, "Client not initialized");
        return {};
    }

    std::vector<NewsArticle> all_news;

    // Get news from VnExpress
    auto vnexpress_news = GetVnExpressNews(category, limit / 3 + 1);
    all_news.insert(all_news.end(), vnexpress_news.begin(), vnexpress_news.end());

    // Get news from Tuoi Tre
    auto tuoitre_news = GetTuoiTreNews(category, limit / 3 + 1);
    all_news.insert(all_news.end(), tuoitre_news.begin(), tuoitre_news.end());

    // Get news from Dan Tri
    auto dantri_news = GetDanTriNews(category, limit / 3 + 1);
    all_news.insert(all_news.end(), dantri_news.begin(), dantri_news.end());

    // Limit results
    if (all_news.size() > static_cast<size_t>(limit))
    {
        all_news.resize(limit);
    }

    ESP_LOGI(TAG, "Retrieved %d news articles from Vietnamese sources", all_news.size());
    return all_news;
}

std::vector<NewsArticle> NewsClient::SearchNews(const std::string &keyword, int limit)
{
    if (!initialized_)
    {
        ESP_LOGE(TAG, "Client not initialized");
        return {};
    }

    // For now, return latest news (in real implementation could filter by keyword)
    auto news = GetLatestNews("", limit);

    // Filter articles that contain the keyword (simple implementation)
    std::vector<NewsArticle> filtered_news;
    for (const auto &article : news)
    {
        if (article.title.find(keyword) != std::string::npos ||
            article.summary.find(keyword) != std::string::npos)
        {
            filtered_news.push_back(article);
        }
    }

    ESP_LOGI(TAG, "Found %d articles matching keyword: %s", filtered_news.size(), keyword.c_str());
    return filtered_news;
}

std::string NewsClient::GetTrendingTopics()
{
    if (!initialized_)
    {
        ESP_LOGE(TAG, "Client not initialized");
        return "{}";
    }

    // Create trending topics JSON
    cJSON *json = cJSON_CreateObject();
    cJSON *topics = cJSON_CreateArray();

    // Add some trending topics (this could be dynamically fetched in real implementation)
    cJSON_AddItemToArray(topics, cJSON_CreateString("Thời sự"));
    cJSON_AddItemToArray(topics, cJSON_CreateString("Kinh tế"));
    cJSON_AddItemToArray(topics, cJSON_CreateString("Thể thao"));
    cJSON_AddItemToArray(topics, cJSON_CreateString("Giải trí"));
    cJSON_AddItemToArray(topics, cJSON_CreateString("Công nghệ"));

    cJSON_AddItemToObject(json, "trending_topics", topics);
    cJSON_AddStringToObject(json, "source", "Vietnamese News Sources");

    char *json_string = cJSON_Print(json);
    std::string result(json_string);
    free(json_string);
    cJSON_Delete(json);

    return result;
}

std::string NewsClient::MakeRequest(const std::string &url)
{
    auto &board = Board::GetInstance();
    auto network = board.GetNetwork();
    if (!network)
    {
        ESP_LOGE(TAG, "Network not available");
        return "";
    }

    auto http = network->CreateHttp(15); // 15 second timeout for news
    if (!http)
    {
        ESP_LOGE(TAG, "Failed to create HTTP client");
        return "";
    }

    // Set headers
    http->SetHeader("Accept", "application/xml, text/xml, application/rss+xml");
    http->SetHeader("User-Agent", "XiaoZhi-ESP32/1.0 NewsReader");

    ESP_LOGI(TAG, "Making request to: %s", url.c_str());

    if (!http->Open("GET", url))
    {
        ESP_LOGE(TAG, "Failed to open HTTP connection to: %s", url.c_str());
        return "";
    }

    http->Write("", 0);

    int status_code = http->GetStatusCode();
    if (status_code != 200)
    {
        ESP_LOGE(TAG, "HTTP request failed with status code: %d", status_code);
        http->Close();
        return "";
    }

    std::string response = http->ReadAll();
    http->Close();

    ESP_LOGD(TAG, "Response length: %d bytes", response.length());
    return response;
}

std::vector<NewsArticle> NewsClient::ParseNewsResponse(const std::string &response, int limit)
{
    std::vector<NewsArticle> articles;

    if (response.empty())
    {
        return articles;
    }

    // Simple XML parsing for RSS feeds
    // Look for <item> tags and extract title, description, link
    size_t pos = 0;
    int count = 0;

    while (count < limit && (pos = response.find("<item>", pos)) != std::string::npos)
    {
        size_t end_pos = response.find("</item>", pos);
        if (end_pos == std::string::npos)
            break;

        std::string item = response.substr(pos, end_pos - pos);
        NewsArticle article;

        // Extract title
        size_t title_start = item.find("<title>");
        if (title_start != std::string::npos)
        {
            title_start += 7; // length of "<title>"
            size_t title_end = item.find("</title>", title_start);
            if (title_end != std::string::npos)
            {
                article.title = item.substr(title_start, title_end - title_start);

                // Clean up CDATA tags
                if (article.title.find("<![CDATA[") == 0)
                {
                    article.title = article.title.substr(9);
                }
                if (article.title.rfind("]]>") == article.title.length() - 3)
                {
                    article.title = article.title.substr(0, article.title.length() - 3);
                }
            }
        }

        // Extract description/summary
        size_t desc_start = item.find("<description>");
        if (desc_start != std::string::npos)
        {
            desc_start += 13; // length of "<description>"
            size_t desc_end = item.find("</description>", desc_start);
            if (desc_end != std::string::npos)
            {
                article.summary = item.substr(desc_start, desc_end - desc_start);

                // Clean up CDATA tags
                if (article.summary.find("<![CDATA[") == 0)
                {
                    article.summary = article.summary.substr(9);
                }
                if (article.summary.rfind("]]>") == article.summary.length() - 3)
                {
                    article.summary = article.summary.substr(0, article.summary.length() - 3);
                }

                // Truncate summary if too long
                if (article.summary.length() > 200)
                {
                    article.summary = article.summary.substr(0, 200) + "...";
                }
            }
        }

        // Extract link
        size_t link_start = item.find("<link>");
        if (link_start != std::string::npos)
        {
            link_start += 6; // length of "<link>"
            size_t link_end = item.find("</link>", link_start);
            if (link_end != std::string::npos)
            {
                article.url = item.substr(link_start, link_end - link_start);
            }
        }

        // Extract pubDate
        size_t date_start = item.find("<pubDate>");
        if (date_start != std::string::npos)
        {
            date_start += 9; // length of "<pubDate>"
            size_t date_end = item.find("</pubDate>", date_start);
            if (date_end != std::string::npos)
            {
                article.published_time = item.substr(date_start, date_end - date_start);
            }
        }

        if (!article.title.empty())
        {
            articles.push_back(article);
            count++;
        }

        pos = end_pos;
    }

    return articles;
}

std::vector<NewsArticle> NewsClient::GetVnExpressNews(const std::string &category, int limit)
{
    std::string url = VNEXPRESS_RSS;

    if (!category.empty())
    {
        // Map category to VnExpress RSS URL
        for (const char *cat : VNEXPRESS_CATEGORIES)
        {
            if (std::string(cat).find(category) != std::string::npos)
            {
                url = std::string(VNEXPRESS_RSS) + "/" + cat;
                break;
            }
        }
    }

    std::string response = MakeRequest(url);
    auto articles = ParseNewsResponse(response, limit);

    // Set source for all articles
    for (auto &article : articles)
    {
        article.source = "VnExpress";
        article.category = category.empty() ? "general" : category;
    }

    ESP_LOGI(TAG, "Retrieved %d articles from VnExpress", articles.size());
    return articles;
}

std::vector<NewsArticle> NewsClient::GetTuoiTreNews(const std::string &category, int limit)
{
    std::string url = TUOITRE_RSS;

    if (!category.empty())
    {
        url += "/" + category + ".rss";
    }

    std::string response = MakeRequest(url);
    auto articles = ParseNewsResponse(response, limit);

    // Set source for all articles
    for (auto &article : articles)
    {
        article.source = "Tuổi Trẻ";
        article.category = category.empty() ? "general" : category;
    }

    ESP_LOGI(TAG, "Retrieved %d articles from Tuoi Tre", articles.size());
    return articles;
}

std::vector<NewsArticle> NewsClient::GetDanTriNews(const std::string &category, int limit)
{
    std::string url = DANTRI_RSS;

    if (!category.empty())
    {
        url += "/" + category + ".rss";
    }

    std::string response = MakeRequest(url);
    auto articles = ParseNewsResponse(response, limit);

    // Set source for all articles
    for (auto &article : articles)
    {
        article.source = "Dân Trí";
        article.category = category.empty() ? "general" : category;
    }

    ESP_LOGI(TAG, "Retrieved %d articles from Dan Tri", articles.size());
    return articles;
}