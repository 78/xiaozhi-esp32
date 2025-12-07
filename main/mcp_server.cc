/*
 * MCP Server Implementation
 * Reference: https://modelcontextprotocol.io/specification/2024-11-05
 */

#include "mcp_server.h"
#include <esp_log.h>
#include <esp_app_desc.h>
#include <algorithm>
#include <cstring>
#include <esp_pthread.h>
#include <qrcode.h>

#include "application.h"
#include "display.h"
#include "oled_display.h"
#include "board.h"
#include "settings.h"
#include "lvgl_theme.h"
#include "lvgl_display.h"
#include "esp32_music.h"
#include "esp32_radio.h"
#include "esp32_sd_music.h"
#include "wifi_station.h"
#include "system_info.h"

#define TAG "MCP"

McpServer::McpServer() {
}

McpServer::~McpServer() {
    for (auto tool : tools_) {
        delete tool;
    }
    tools_.clear();
}

void McpServer::AddCommonTools() {
    // *Important* To speed up the response time, we add the common tools to the beginning of
    // the tools list to utilize the prompt cache.
    // **Important** To improve response speed, we place commonly used tools at the beginning to leverage the prompt cache feature.

    // Backup the original tools list and restore it after adding the common tools.
    auto original_tools = std::move(tools_);
    auto& board = Board::GetInstance();

    // Do not add custom tools here.
    // Custom tools must be added in the board's InitializeTools function.

    AddTool("self.get_device_status",
        "Provides the real-time information of the device, including the current status of the audio speaker, screen, battery, network, etc.\n"
        "Use this tool for: \n"
        "1. Answering questions about current condition (e.g. what is the current volume of the audio speaker?)\n"
        "2. As the first step to control the device (e.g. turn up / down the volume of the audio speaker, etc.)",
        PropertyList(),
        [&board](const PropertyList& properties) -> ReturnValue {
            return board.GetDeviceStatusJson();
        });

    AddTool("self.network.ip2qrcode",
        "Print the QR code of the IP address connected to WiFi network.\n"
        "Use this tool when user asks about network connection, IP address and print QR code.\n"
        "Returns the new IP address, SSID, and connection status. Also displays IP address as QR code on LCD screen.",
        PropertyList(),
        [&board](const PropertyList& properties) -> ReturnValue {
            auto& wifi_station = WifiStation::GetInstance();
            ESP_LOGI(TAG, "Getting network status for IP address tool");
            cJSON* json = cJSON_CreateObject();
            cJSON_AddBoolToObject(json, "connected", wifi_station.IsConnected());
            
            if (wifi_station.IsConnected()) {
                std::string ip_address = wifi_station.GetIpAddress();
                cJSON_AddStringToObject(json, "ip_address", ip_address.c_str());
                cJSON_AddStringToObject(json, "ssid", wifi_station.GetSsid().c_str());
                cJSON_AddNumberToObject(json, "rssi", wifi_station.GetRssi());
                cJSON_AddNumberToObject(json, "channel", wifi_station.GetChannel());
                cJSON_AddStringToObject(json, "mac_address", SystemInfo::GetMacAddress().c_str());
                cJSON_AddStringToObject(json, "status", "connected");
                
                // Generate and display QR code for IP address
                auto display = board.GetDisplay();
                if (display) {
                    ESP_LOGI(TAG, "Generating QR code for IP address: %s", ip_address.c_str());                    
                    if (display->QRCodeIsSupported()) {
                        ip_address += "/ota";
                        display->SetIpAddress(ip_address);
                        // Capture display pointer for callback
                        static Display* s_display = display;
                        esp_qrcode_config_t qrcode_cfg = {
                            .display_func = [](esp_qrcode_handle_t qrcode) {
                                if (s_display && qrcode) {
                                    s_display->DisplayQRCode(qrcode, nullptr);
                                }
                            },
                            .max_qrcode_version = 10,
                            .qrcode_ecc_level = ESP_QRCODE_ECC_MED
                        };
                        
                        // Create URL format for QR code
                        std::string qr_text = "http://" + ip_address;
                        esp_err_t err = esp_qrcode_generate(&qrcode_cfg, qr_text.c_str());
                        if (err == ESP_OK) {
                            ESP_LOGI(TAG, "QR code generated and displayed for IP: %s", ip_address.c_str());
                            cJSON_AddBoolToObject(json, "qrcode_displayed", true);
                        } else {
                            ESP_LOGE(TAG, "Failed to generate QR code for IP address");
                            cJSON_AddBoolToObject(json, "qrcode_displayed", false);
                        }
                    } else {
                        display->SetChatMessage("assistant", ip_address.c_str());
                        vTaskDelay(pdMS_TO_TICKS(5000));
                        ESP_LOGW(TAG, "Display does not support QR code");
                        cJSON_AddBoolToObject(json, "qrcode_displayed", false);
                    }
                }
            } else {
                cJSON_AddStringToObject(json, "status", "disconnected");
                cJSON_AddStringToObject(json, "message", "Device is not connected to WiFi");
            }
            
            return json;
        });

    AddTool("self.audio_speaker.set_volume", 
        "Set the volume of the audio speaker. If the current volume is unknown, you must call `self.get_device_status` tool first and then call this tool.",
        PropertyList({
            Property("volume", kPropertyTypeInteger, 0, 100)
        }), 
        [&board](const PropertyList& properties) -> ReturnValue {
            auto codec = board.GetAudioCodec();
            codec->SetOutputVolume(properties["volume"].value<int>());
            return true;
        });
    
    auto backlight = board.GetBacklight();
    if (backlight) {
        AddTool("self.screen.set_brightness",
            "Set the brightness of the screen.",
            PropertyList({
                Property("brightness", kPropertyTypeInteger, 0, 100)
            }),
            [backlight](const PropertyList& properties) -> ReturnValue {
                uint8_t brightness = static_cast<uint8_t>(properties["brightness"].value<int>());
                backlight->SetBrightness(brightness, true);
                return true;
            });
    }

#ifdef HAVE_LVGL
    auto display = board.GetDisplay();
    if (display && display->GetTheme() != nullptr) {
        AddTool("self.screen.set_theme",
            "Set the theme of the screen. The theme can be `light` or `dark`.",
            PropertyList({
                Property("theme", kPropertyTypeString)
            }),
            [display](const PropertyList& properties) -> ReturnValue {
                auto theme_name = properties["theme"].value<std::string>();
                auto& theme_manager = LvglThemeManager::GetInstance();
                auto theme = theme_manager.GetTheme(theme_name);
                if (theme != nullptr) {
                    display->SetTheme(theme);
                    return true;
                }
                return false;
            });
            
        // Rotation display tool
        AddTool("self.screen.set_rotation",
            "Set the rotation of the screen display. The rotation can be 0, 90 or left, 180 of flip, 270 or right degrees.",
            PropertyList({
                Property("rotation_degree", kPropertyTypeInteger, 0, 270)
            }),
            [display](const PropertyList& properties) -> ReturnValue {
                int rotation_degree = properties["rotation_degree"].value<int>();
                if (rotation_degree == 0 || rotation_degree == 90 || rotation_degree == 180 || rotation_degree == 270) {
                    return display->SetRotation(rotation_degree, true);
                }
                return false;
            });  
    }

    auto camera = board.GetCamera();
    if (camera) {
        AddTool("self.camera.take_photo",
            "Take a photo and explain it. Use this tool after the user asks you to see something.\n"
            "Args:\n"
            "  `question`: The question that you want to ask about the photo.\n"
            "Return:\n"
            "  A JSON object that provides the photo information.",
            PropertyList({
                Property("question", kPropertyTypeString)
            }),
            [camera](const PropertyList& properties) -> ReturnValue {
                // Lower the priority to do the camera capture
                TaskPriorityReset priority_reset(1);

                if (!camera->Capture()) {
                    throw std::runtime_error("Failed to capture photo");
                }
                auto question = properties["question"].value<std::string>();
                return camera->Explain(question);
            });
    }
#endif

    auto music = Application::GetInstance().GetMusic();
     if (music) {
         AddTool("self.music.play_song",
				 "Play a specified song ONLINE. Đây là chế độ PHÁT NHẠC MẶC ĐỊNH.\n"
				 "Khi người dùng nói: 'phát nhạc', 'mở nhạc', 'phát bài hát', "
				 "'play music', 'play song', 'mở bài ...', AI phải ưu tiên dùng tool này.\n"
				 "\n"
				 "Chỉ dùng SD card nếu người dùng nói rõ: 'nhạc trong thẻ nhớ', "
				 "'nhạc offline', 'bài trong thẻ', 'SD card', 'chạy nhạc nội bộ', v.v.\n"
				 "\n"
				 "Args:\n"
				 "  song_name: Tên bài hát (bắt buộc)\n"
				 "  artist_name: Tên ca sĩ (tùy chọn)\n"
				 "Return:\n"
				 "  Phát bài hát online ngay lập tức.\n",
                 PropertyList({
                     Property("song_name", kPropertyTypeString),      // Song name (required)
                     Property("artist_name", kPropertyTypeString, "") // Artist name (optional, defaults to empty string)
                 }),
                 [music](const PropertyList &properties) -> ReturnValue {
                     auto song_name = properties["song_name"].value<std::string>();
                     auto artist_name = properties["artist_name"].value<std::string>();

                     if (!music->Download(song_name, artist_name))
                     {
                         return "{\"success\": false, \"message\": \"Failed to get music resource\"}";
                     }
                     auto download_result = music->GetDownloadResult();
                     ESP_LOGI(TAG, "Music details result: %s", download_result.c_str());
                     return "{\"success\": true, \"message\": \"Music started playing\"}";
                 });

         AddTool("self.music.set_display_mode",
                 "Set the display mode for music playback. You can choose to display spectrum or lyrics, for example when user says 'show spectrum' or 'display spectrum', 'show lyrics' or 'display lyrics', set the corresponding display mode.\n"
                 "Args:\n"
                 "  `mode`: Display mode, options are 'spectrum' or 'lyrics'.\n"
                 "Return:\n"
                 "  Setting result information.",
                 PropertyList({
                     Property("mode", kPropertyTypeString) // Display mode: "spectrum" or "lyrics"
                 }),
                 [music](const PropertyList &properties) -> ReturnValue {
                     auto mode_str = properties["mode"].value<std::string>();

                     // Convert to lowercase for comparison
                     std::transform(mode_str.begin(), mode_str.end(), mode_str.begin(), ::tolower);

                     if (mode_str == "spectrum")
                     {
                         // Set to spectrum display mode
                         auto esp32_music = static_cast<Esp32Music *>(music);
                         esp32_music->SetDisplayMode(Esp32Music::DISPLAY_MODE_SPECTRUM);
                         return "{\"success\": true, \"message\": \"Switched to spectrum display mode\"}";
                     }
                     else if (mode_str == "lyrics")
                     {
                         // Set to lyrics display mode
                         auto esp32_music = static_cast<Esp32Music *>(music);
                         esp32_music->SetDisplayMode(Esp32Music::DISPLAY_MODE_LYRICS);
                         return "{\"success\": true, \"message\": \"Switched to lyrics display mode\"}";
                     }
                     else
                     {
                         return "{\"success\": false, \"message\": \"Invalid display mode, please use 'spectrum' or 'lyrics'\"}";
                     }

                     return "{\"success\": false, \"message\": \"Failed to set display mode\"}";
                 });
     }

    auto radio = Application::GetInstance().GetRadio();
	if (radio) {
		AddTool("self.radio.play_station",
				"Play a radio station by name. Use this tool when user requests to play radio or listen to a specific station."
				"VOV mộc/mốc/mốt/mậu/máu/một/mút/mót/mục means VOV1 channel.\n"
				"Args:\n"
				"  `station_name`: The name of the radio station to play (e.g., 'VOV1', 'BBC', 'NPR').\n"
				"Return:\n"
				"  Playback status information. Starts playing the radio station immediately.",
				PropertyList({
					Property("station_name", kPropertyTypeString) // Station name (required)
				}),
				[radio](const PropertyList &properties) -> ReturnValue {
					auto station_name = properties["station_name"].value<std::string>();

					if (!radio->PlayStation(station_name))
					{
						return "{\"success\": false, \"message\": \"Failed to find or play radio station: " + station_name + "\"}";
					}
					return "{\"success\": true, \"message\": \"Radio station " + station_name + " started playing\"}";
				});

		AddTool("self.radio.play_url",
				"Play a radio stream from a custom URL. Use this tool when user provides a specific radio stream URL.\n"
				"Args:\n"
				"  `url`: The URL of the radio stream to play (required).\n"
				"  `name`: Custom name for the radio station (optional).\n"
				"Return:\n"
				"  Playback status information. Starts playing the radio stream immediately.",
				PropertyList({
					Property("url", kPropertyTypeString),     // Stream URL (required)
					Property("name", kPropertyTypeString, "") // Station name (optional)
				}),
				[radio](const PropertyList &properties) -> ReturnValue
				{
					auto url = properties["url"].value<std::string>();
					auto name = properties["name"].value<std::string>();

					if (!radio->PlayUrl(url, name))
					{
						return "{\"success\": false, \"message\": \"Failed to play radio stream from URL: " + url + "\"}";
					}
					return "{\"success\": true, \"message\": \"Radio stream started playing\"}";
				});

		AddTool("self.radio.stop",
				"Stop the currently playing radio stream.\n"
				"Return:\n"
				"  Stop status information.",
				PropertyList(),
				[radio](const PropertyList &properties) -> ReturnValue {
					if (!radio->Stop())
					{
						return "{\"success\": false, \"message\": \"Failed to stop radio\"}";
					}
					return "{\"success\": true, \"message\": \"Radio stopped\"}";
				});

		AddTool("self.radio.get_stations",
				"Get the list of available radio stations.\n"
				"Return:\n"
				"  JSON array of available radio stations.",
				PropertyList(),
				[radio](const PropertyList &properties) -> ReturnValue {
					auto stations = radio->GetStationList();
					std::string result = "{\"success\": true, \"stations\": [";
					for (size_t i = 0; i < stations.size(); ++i) {
						result += "\"" + stations[i] + "\"";
						if (i < stations.size() - 1) {
							result += ", ";
						}
					}
					result += "]}";
					return result;
				});

		AddTool("self.radio.set_display_mode",
				"Set the display mode for radio playback. You can choose to display spectrum or station info.\n"
				"Args:\n"
				"  `mode`: Display mode, options are 'spectrum' or 'info'.\n"
				"Return:\n"
				"  Setting result information.",
				PropertyList({
					Property("mode", kPropertyTypeString) // Display mode: "spectrum" or "info"
				}),
				[radio](const PropertyList &properties) -> ReturnValue {
					auto mode_str = properties["mode"].value<std::string>();

					// Convert to lowercase for comparison
					std::transform(mode_str.begin(), mode_str.end(), mode_str.begin(), ::tolower);

					if (mode_str == "spectrum")
					{
						// Set to spectrum display mode
						auto esp32_radio = static_cast<Esp32Radio *>(radio);
						esp32_radio->SetDisplayMode(Esp32Radio::DISPLAY_MODE_SPECTRUM);
						return "{\"success\": true, \"message\": \"Switched to spectrum display mode\"}";
					}
					else if (mode_str == "info")
					{
						// Set to info display mode
						auto esp32_radio = static_cast<Esp32Radio *>(radio);
						esp32_radio->SetDisplayMode(Esp32Radio::DISPLAY_MODE_INFO);
						return "{\"success\": true, \"message\": \"Switched to info display mode\"}";
					}
					else
					{
						return "{\"success\": false, \"message\": \"Invalid display mode, please use 'spectrum' or 'info'\"}";
					}
				});
	}
	
	auto sd_music = Application::GetInstance().GetSdMusic();
	if (sd_music) {

		// ================== 1) PLAYBACK CƠ BẢN ==================
		// Gộp: self.sdmusic.play, pause, stop, next, prev
		AddTool("self.sdmusic.playback",
				"Điều khiển phát nhạc từ THẺ NHỚ (SD card).\n"
				"KHÔNG dùng tool này khi người dùng chỉ nói: 'phát nhạc', 'mở bài', "
				"'play music', 'phát bài hát'.\n"
				"\n"
				"Tool này chỉ dùng khi người dùng nói rõ:\n"
				"- nhạc trong thẻ nhớ\n"
				"- nhạc offline\n"
				"- phát bài trong thẻ\n"
				"- SD card\n"
				"- chạy nhạc từ thẻ\n"
				"\n"
				"action = play | pause | stop | next | prev\n"
				"Return: trạng thái điều khiển SD card.\n",			
			PropertyList({
				Property("action", kPropertyTypeString),
			}),
			[sd_music](const PropertyList& props) -> ReturnValue {
				std::string action = props["action"].value<std::string>();

				// Giữ hành vi lỗi giống code gốc:
				// - play: trả JSON lỗi
				// - pause/stop/next/prev: trả false
				if (!sd_music) {
					if (action == "play") {
						return "{\"success\": false, \"message\": \"SD music module not available\"}";
					}
					return false;
				}

				if (action == "play") {
					if (sd_music->getTotalTracks() == 0) {
						if (!sd_music->loadTrackList()) {
							return "{\"success\": false, \"message\": \"No MP3 files found on SD card\"}";
						}
					}
					bool ok = sd_music->play();
					return ok ? "{\"success\": true, \"message\": \"Playback started\"}"
							  : "{\"success\": false, \"message\": \"Failed to play\"}";
				}

				if (action == "pause") {
					sd_music->pause();
					return true;
				}

				if (action == "stop") {
					sd_music->stop();
					return true;
				}

				if (action == "next") {
					return sd_music->next();
				}

				if (action == "prev") {
					return sd_music->prev();
				}

				// Hành vi mới, chỉ để an toàn
				return "{\"success\":false,\"message\":\"Unknown playback action\"}";
			}
		);

		// ================== 2) SHUFFLE / REPEAT MODE ==================
		// Gộp: self.sdmusic.shuffle, repeat
		AddTool(
			"self.sdmusic.mode",
			"Control playback mode: shuffle and repeat.\n"
			"action = shuffle | repeat\n"
			"For shuffle: `enabled` (bool)\n"
			"For repeat: `mode` = none | one | all",
			PropertyList({
				Property("action",  kPropertyTypeString),
				Property("enabled", kPropertyTypeBoolean),
				Property("mode",    kPropertyTypeString)
			}),
			[sd_music](const PropertyList& props) -> ReturnValue {
				if (!sd_music) return "SD music not available";

				std::string action = props["action"].value<std::string>();

				if (action == "shuffle") {
					bool enabled = props["enabled"].value<bool>();
					sd_music->shuffle(enabled);

					if (enabled) {
						if (sd_music->getTotalTracks() == 0) sd_music->loadTrackList();
						if (sd_music->getTotalTracks() == 0) return false;

						int idx = rand() % sd_music->getTotalTracks();
						sd_music->setTrack(idx);   // auto play() như code gốc
					}
					return true;
				}

				if (action == "repeat") {
					std::string mode = props["mode"].value<std::string>();

					if (mode == "none")      sd_music->repeat(Esp32SdMusic::RepeatMode::None);
					else if (mode == "one")  sd_music->repeat(Esp32SdMusic::RepeatMode::RepeatOne);
					else if (mode == "all")  sd_music->repeat(Esp32SdMusic::RepeatMode::RepeatAll);
					else return "Invalid repeat mode";

					return true;
				}

				return "Unknown mode action";
			}
		);

		// ================== 3) TRUY CẬP BÀI HÁT ==================
		// Gộp: set_track, get_track_info, list, current
		AddTool(
			"self.sdmusic.track",
			"Track-level operations.\n"
			"action = set | info | list | current\n"
			"  set:    needs `index`\n"
			"  info:   needs `index`\n"
			"  list:   return JSON { count }\n"
			"  current: return name string",
			PropertyList({
				Property("action", kPropertyTypeString),
				Property("index",  kPropertyTypeInteger, 0, 0, 9999)
			}),
			[sd_music](const PropertyList& props) -> ReturnValue {
				std::string action = props["action"].value<std::string>();

				// Hành vi khi !sd giống tool gốc, nhưng trả thêm field rỗng cho info
				if (!sd_music) {
					if (action == "set") {
						return false;
					}
					if (action == "info") {
						cJSON* json = cJSON_CreateObject();
						cJSON_AddStringToObject(json, "name", "");
						cJSON_AddStringToObject(json, "path", "");
						cJSON_AddStringToObject(json, "title", "");
						cJSON_AddStringToObject(json, "artist", "");
						cJSON_AddStringToObject(json, "album", "");
						cJSON_AddStringToObject(json, "genre", "");
						cJSON_AddStringToObject(json, "comment", "");
						cJSON_AddStringToObject(json, "year", "");
						cJSON_AddNumberToObject(json, "track_number", 0);
						cJSON_AddNumberToObject(json, "duration_ms", 0);
						cJSON_AddNumberToObject(json, "bitrate_kbps", 0);
						cJSON_AddNumberToObject(json, "file_size", 0);
						cJSON_AddBoolToObject(json, "has_cover", false);
						cJSON_AddNumberToObject(json, "cover_size", 0);
						cJSON_AddStringToObject(json, "cover_mime", "");
						return json;
					}
					if (action == "list") {
						cJSON* o = cJSON_CreateObject();
						cJSON_AddNumberToObject(o, "count", 0);
						return o;
					}
					if (action == "current") {
						return std::string("");
					}
					return "SD music module not available";
				}

				auto ensure_playlist = [sd_music]() {
					if (sd_music->getTotalTracks() == 0) {
						sd_music->loadTrackList();
					}
				};

				if (action == "set") {
					int index = props["index"].value<int>();
					ensure_playlist();
					return sd_music->setTrack(index);
				}

				if (action == "info") {
					ensure_playlist();
					int index = props["index"].value<int>();
					auto info = sd_music->getTrackInfo(index);

					cJSON* json = cJSON_CreateObject();
					cJSON_AddStringToObject(json, "name",  info.name.c_str());
					cJSON_AddStringToObject(json, "path",  info.path.c_str());
					cJSON_AddStringToObject(json, "title",  info.title.c_str());
					cJSON_AddStringToObject(json, "artist", info.artist.c_str());
					cJSON_AddStringToObject(json, "album",  info.album.c_str());
					cJSON_AddStringToObject(json, "genre",  info.genre.c_str());
					cJSON_AddStringToObject(json, "comment", info.comment.c_str());
					cJSON_AddStringToObject(json, "year",   info.year.c_str());
					cJSON_AddNumberToObject(json, "track_number", info.track_number);
					cJSON_AddNumberToObject(json, "duration_ms",  info.duration_ms);
					cJSON_AddNumberToObject(json, "bitrate_kbps", info.bitrate_kbps);
					cJSON_AddNumberToObject(json, "file_size",    (double)info.file_size);

					bool has_cover = (info.cover_size > 0);
					cJSON_AddBoolToObject(json, "has_cover", has_cover);
					cJSON_AddNumberToObject(json, "cover_size", (int)info.cover_size);
					cJSON_AddStringToObject(json, "cover_mime", info.cover_mime.c_str());
					return json;
				}

				if (action == "list") {
					cJSON* o = cJSON_CreateObject();
					ensure_playlist();
					cJSON_AddNumberToObject(o, "count", (int)sd_music->getTotalTracks());
					return o;
				}

				if (action == "current") {
					ensure_playlist();
					return sd_music->getCurrentTrack();
				}

				return "Unknown track action";
			}
		);

		// ================== 4) THƯ MỤC ==================
		// Gộp: play_directory, list_directories
		AddTool(
			"self.sdmusic.directory",
			"Directory-level operations.\n"
			"action = play | list\n"
			"  play: requires `directory`\n"
			"  list: list directories under current root",
			PropertyList({
				Property("action",    kPropertyTypeString),
				Property("directory", kPropertyTypeString)
			}),
			[sd_music](const PropertyList& props) -> ReturnValue {
				std::string action = props["action"].value<std::string>();

				// Hành vi khi !sd:
				// - play_directory: JSON lỗi
				// - list_directories: trả mảng rỗng
				if (!sd_music) {
					if (action == "play") {
						return "{\"success\": false, \"message\": \"SD music module not available\"}";
					}
					if (action == "list") {
						cJSON* arr = cJSON_CreateArray();
						return arr;
					}
					return "{\"success\": false, \"message\": \"SD music module not available\"}";
				}

				if (action == "play") {
					std::string dir = props["directory"].value<std::string>();

					if (!sd_music->playDirectory(dir)) {
						return "{\"success\": false, \"message\": \"Cannot play directory or directory has no MP3\"}";
					}
					return "{\"success\": true, \"message\": \"Playing directory\"}";
				}

				if (action == "list") {
					cJSON* arr = cJSON_CreateArray();
					auto list = sd_music->listDirectories();
					for (auto& d : list) {
						cJSON_AddItemToArray(arr, cJSON_CreateString(d.c_str()));
					}
					return arr;
				}

				return "{\"success\": false, \"message\": \"Unknown directory action\"}";
			}
		);

		// ================== 5) TÌM KIẾM / PLAY THEO TÊN ==================
		// Gộp: search, play_by_name
		AddTool(
			"self.sdmusic.search",
			"Search and play tracks by name.\n"
			"action = search | play\n"
			"  search: returns matching tracks (needs `keyword`)\n"
			"  play:   play by name (needs `keyword`)",
			PropertyList({
				Property("action",  kPropertyTypeString),
				Property("keyword", kPropertyTypeString)
			}),
			[sd_music](const PropertyList& props) -> ReturnValue {
				std::string action  = props["action"].value<std::string>();
				std::string keyword = props["keyword"].value<std::string>();

				if (!sd_music) {
					if (action == "search") {
						cJSON* arr = cJSON_CreateArray();
						return arr;
					}
					if (action == "play") {
						return "{\"success\": false, \"message\": \"SD music module not available\"}";
					}
					return "{\"success\": false, \"message\": \"SD music module not available\"}";
				}

				auto ensure_playlist = [sd_music]() {
					if (sd_music->getTotalTracks() == 0) {
						sd_music->loadTrackList();
					}
				};

				if (action == "search") {
					cJSON* arr = cJSON_CreateArray();
					ensure_playlist();

					auto list = sd_music->searchTracks(keyword);
					// (Optional) nếu muốn có index, có thể map path -> index ở đây
					for (auto& t : list) {
						cJSON* o = cJSON_CreateObject();
						cJSON_AddStringToObject(o, "name",  t.name.c_str());
						cJSON_AddStringToObject(o, "path",  t.path.c_str());
						cJSON_AddStringToObject(o, "title",  t.title.c_str());
						cJSON_AddStringToObject(o, "artist", t.artist.c_str());
						cJSON_AddStringToObject(o, "album",  t.album.c_str());
						cJSON_AddStringToObject(o, "genre",  t.genre.c_str());
						cJSON_AddStringToObject(o, "year",   t.year.c_str());
						cJSON_AddNumberToObject(o, "track_number", t.track_number);
						cJSON_AddNumberToObject(o, "duration_ms",  t.duration_ms);
						cJSON_AddNumberToObject(o, "bitrate_kbps", t.bitrate_kbps);
						bool has_cover = (t.cover_size > 0);
						cJSON_AddBoolToObject(o, "has_cover", has_cover);
						cJSON_AddNumberToObject(o, "cover_size", (int)t.cover_size);
						cJSON_AddStringToObject(o, "cover_mime", t.cover_mime.c_str());
						cJSON_AddItemToArray(arr, o);
					}
					return arr;
				}

				if (action == "play") {
					if (keyword.empty())
						return "{\"success\": false, \"message\": \"Keyword cannot be empty\"}";

					ensure_playlist();
					bool ok = sd_music->playByName(keyword);
					return ok
						? "{\"success\": true, \"message\": \"Playing song by name\"}"
						: "{\"success\": false, \"message\": \"Song not found\"}";
				}

				return "{\"success\": false, \"message\": \"Unknown search action\"}";
			}
		);

		// ================== 6) ĐẾM / PHÂN TRANG ==================
		// Gộp: count_in_directory, count_current_directory, list_page
		AddTool(
			"self.sdmusic.library",
			"Thông tin THƯ VIỆN BÀI HÁT (tracks), KHÔNG phải thư mục.\n"
			"action = count_dir | count_current | page\n"
			"  count_dir: đếm SỐ BÀI HÁT trong thư mục chỉ định\n"
			"  count_current: đếm SỐ BÀI HÁT trong thư mục hiện tại\n"
			"  page: phân trang DANH SÁCH BÀI HÁT\n"
			"Lưu ý: công cụ này liên quan tới BÀI HÁT.\n"
			"Nếu người dùng hỏi số THƯ MỤC, hãy dùng `self.sdmusic.directory`.",
			PropertyList({
				Property("action",    kPropertyTypeString),
				Property("directory", kPropertyTypeString),
				Property("page",      kPropertyTypeInteger, 1, 1, 10000),
				Property("page_size", kPropertyTypeInteger, 10, 1, 1000)
			}),
			[sd_music](const PropertyList& props) -> ReturnValue {
				std::string action = props["action"].value<std::string>();

				if (!sd_music) {
					if (action == "count_dir") {
						cJSON* o = cJSON_CreateObject();
						cJSON_AddNumberToObject(o, "count", 0);
						cJSON_AddStringToObject(o, "directory", "");
						return o;
					}
					if (action == "count_current") {
						cJSON* o = cJSON_CreateObject();
						cJSON_AddNumberToObject(o, "count", 0);
						return o;
					}
					if (action == "page") {
						cJSON* arr = cJSON_CreateArray();
						return arr;
					}
					return "{\"success\": false, \"message\": \"SD music module not available\"}";
				}

				auto ensure_playlist = [sd_music]() {
					if (sd_music->getTotalTracks() == 0) {
						sd_music->loadTrackList();
					}
				};

				if (action == "count_dir") {
					cJSON* o = cJSON_CreateObject();
					std::string dir = props["directory"].value<std::string>();
					size_t count = sd_music->countTracksInDirectory(dir);
					cJSON_AddStringToObject(o, "directory", dir.c_str());
					cJSON_AddNumberToObject(o, "count", (int)count);
					return o;
				}

				if (action == "count_current") {
					cJSON* o = cJSON_CreateObject();
					ensure_playlist();
					cJSON_AddNumberToObject(o, "count", (int)sd_music->countTracksInCurrentDirectory());
					return o;
				}

				if (action == "page") {
					cJSON* arr = cJSON_CreateArray();
					ensure_playlist();

					int page      = props["page"].value<int>();
					int page_size = props["page_size"].value<int>();
					if (page <= 0) page = 1;
					if (page_size <= 0) page_size = 10;

					size_t page_index = (size_t)(page - 1);
					auto list = sd_music->listTracksPage(page_index, (size_t)page_size);
					size_t start_index = page_index * (size_t)page_size;

					for (size_t i = 0; i < list.size(); ++i) {
						const auto& t = list[i];
						cJSON* o = cJSON_CreateObject();
						cJSON_AddNumberToObject(o, "index", (int)(start_index + i));
						cJSON_AddStringToObject(o, "name",  t.name.c_str());
						cJSON_AddStringToObject(o, "path",  t.path.c_str());
						cJSON_AddStringToObject(o, "title", t.title.c_str());
						cJSON_AddStringToObject(o, "artist", t.artist.c_str());
						cJSON_AddStringToObject(o, "album",  t.album.c_str());
						cJSON_AddStringToObject(o, "genre",  t.genre.c_str());
						cJSON_AddStringToObject(o, "year",   t.year.c_str());
						cJSON_AddNumberToObject(o, "track_number", t.track_number);
						cJSON_AddNumberToObject(o, "duration_ms",  t.duration_ms);
						cJSON_AddNumberToObject(o, "bitrate_kbps", t.bitrate_kbps);
						bool has_cover = (t.cover_size > 0);
						cJSON_AddBoolToObject(o, "has_cover", has_cover);
						cJSON_AddNumberToObject(o, "cover_size", (int)t.cover_size);
						cJSON_AddStringToObject(o, "cover_mime", t.cover_mime.c_str());
						cJSON_AddItemToArray(arr, o);
					}
					return arr;
				}

				return "{\"success\": false, \"message\": \"Unknown library action\"}";
			}
		);

		// ================== 7) GỢI Ý BÀI HÁT ==================
		// Gộp: suggest_next, suggest_similar
		AddTool(
			"self.sdmusic.suggest",
			"Song suggestion based on history / similarity.\n"
			"action = next | similar\n"
			"  next:    uses `max_results`\n"
			"  similar: uses `keyword` + `max_results`",
			PropertyList({
				Property("action",      kPropertyTypeString),
				Property("keyword",     kPropertyTypeString),
				Property("max_results", kPropertyTypeInteger, 5, 1, 50)
			}),
			[sd_music](const PropertyList& props) -> ReturnValue {
				cJSON* arr = cJSON_CreateArray();
				std::string action  = props["action"].value<std::string>();

				if (!sd_music) return arr;

				std::string keyword = props["keyword"].value<std::string>();
				int max_results     = props["max_results"].value<int>();
				if (max_results <= 0) max_results = 5;

				auto ensure_playlist = [sd_music]() {
					if (sd_music->getTotalTracks() == 0) {
						sd_music->loadTrackList();
					}
				};
				ensure_playlist();

				auto add_track_to_array = [arr](const Esp32SdMusic::TrackInfo& t) {
					cJSON* o = cJSON_CreateObject();
					cJSON_AddStringToObject(o, "name",  t.name.c_str());
					cJSON_AddStringToObject(o, "path",  t.path.c_str());
					cJSON_AddStringToObject(o, "title", t.title.c_str());
					cJSON_AddStringToObject(o, "artist", t.artist.c_str());
					cJSON_AddStringToObject(o, "album",  t.album.c_str());
					cJSON_AddStringToObject(o, "genre",  t.genre.c_str());
					cJSON_AddStringToObject(o, "year",   t.year.c_str());
					cJSON_AddNumberToObject(o, "track_number", t.track_number);
					cJSON_AddNumberToObject(o, "duration_ms",  t.duration_ms);
					cJSON_AddNumberToObject(o, "bitrate_kbps", t.bitrate_kbps);
					bool has_cover = (t.cover_size > 0);
					cJSON_AddBoolToObject(o, "has_cover", has_cover);
					cJSON_AddNumberToObject(o, "cover_size", (int)t.cover_size);
					cJSON_AddStringToObject(o, "cover_mime", t.cover_mime.c_str());
					cJSON_AddItemToArray(arr, o);
				};

				if (action == "next") {
					auto list = sd_music->suggestNextTracks((size_t)max_results);
					for (auto& t : list) {
						add_track_to_array(t);
					}
					return arr;
				}

				if (action == "similar") {
					auto list = sd_music->suggestSimilarTo(keyword, (size_t)max_results);
					for (auto& t : list) {
						add_track_to_array(t);
					}
					return arr;
				}

				// Không action hợp lệ → mảng rỗng
				return arr;
			}
		);

		// ================== 8) PROGRESS ==================
		// Nâng cấp: thêm track_path
		AddTool(
			"self.sdmusic.progress",
			"Get current playback progress and duration.",
			PropertyList(),
			[sd_music](const PropertyList&) -> ReturnValue {
				cJSON* o = cJSON_CreateObject();
				if (!sd_music) {
					cJSON_AddNumberToObject(o, "position_ms", 0);
					cJSON_AddNumberToObject(o, "duration_ms", 0);
					cJSON_AddStringToObject(o, "state", "stopped");
					cJSON_AddNumberToObject(o, "bitrate_kbps", 0);
					cJSON_AddStringToObject(o, "position_str", "00:00");
					cJSON_AddStringToObject(o, "duration_str", "00:00");
					cJSON_AddStringToObject(o, "track_name", "");
					cJSON_AddStringToObject(o, "track_path", "");
					return o;
				}

				auto prog  = sd_music->updateProgress();
				auto state = sd_music->getState();
				int  br    = sd_music->getBitrate();

				const char* s = "unknown";
				switch (state) {
					case Esp32SdMusic::PlayerState::Stopped:   s = "stopped";   break;
					case Esp32SdMusic::PlayerState::Preparing: s = "preparing"; break;
					case Esp32SdMusic::PlayerState::Playing:   s = "playing";   break;
					case Esp32SdMusic::PlayerState::Paused:    s = "paused";    break;
					case Esp32SdMusic::PlayerState::Error:     s = "error";     break;
				}

				cJSON_AddNumberToObject(o, "position_ms", (int)prog.position_ms);
				cJSON_AddNumberToObject(o, "duration_ms", (int)prog.duration_ms);
				cJSON_AddStringToObject(o, "state", s);
				cJSON_AddNumberToObject(o, "bitrate_kbps", br);
				cJSON_AddStringToObject(o, "position_str", sd_music->getCurrentTimeString().c_str());
				cJSON_AddStringToObject(o, "duration_str", sd_music->getDurationString().c_str());
				cJSON_AddStringToObject(o, "track_name", sd_music->getCurrentTrack().c_str());
				cJSON_AddStringToObject(o, "track_path", sd_music->getCurrentTrackPath().c_str());
				return o;
			}
		);

		// ================== 9) THỂ LOẠI (GENRE PLAYLIST) ==================
		AddTool(
			"self.sdmusic.genre",
			"Genre-based music operations.\n"
			"action = search | play | play_index | next\n"
			"  search:      list all tracks of a genre (needs `genre`)\n"
			"  play:        build genre playlist and play first track\n"
			"  play_index:  play the N-th track in genre playlist (needs `index`)\n"
			"  next:        play next track within current genre playlist",
			PropertyList({
				Property("action", kPropertyTypeString),
				Property("genre",  kPropertyTypeString),
				Property("index",  kPropertyTypeInteger, 0, 0, 9999)
			}),
			[sd_music](const PropertyList& props) -> ReturnValue {
				auto sd = Application::GetInstance().GetSdMusic();
				if (!sd) {
					return "{\"success\": false, \"message\": \"SD music module not available\"}";
				}

				std::string action = props["action"].value<std::string>();
				std::string genre  = props["genre"].value<std::string>();

				auto ensure_playlist = [sd_music]() {
					if (sd_music->getTotalTracks() == 0)
						sd_music->loadTrackList();
				};
				ensure_playlist();

				// Helper: lowercase ASCII only
				auto ascii_lower = [](std::string s) {
					for (char &c : s) {
						unsigned char u = (unsigned char)c;
						if (u < 128) c = std::tolower(u);
					}
					return s;
				};

				// ---------------------- SEARCH GENRE ----------------------
				if (action == "search") {
					cJSON* arr = cJSON_CreateArray();
					if (genre.empty()) return arr;

					auto all = sd_music->listTracks();
					std::string low = ascii_lower(genre);

					for (auto& t : all) {
						std::string g = ascii_lower(t.genre);

						if (g.find(low) != std::string::npos) {
							cJSON* o = cJSON_CreateObject();
							cJSON_AddStringToObject(o, "name",   t.name.c_str());
							cJSON_AddStringToObject(o, "path",   t.path.c_str());
							cJSON_AddStringToObject(o, "artist", t.artist.c_str());
							cJSON_AddStringToObject(o, "album",  t.album.c_str());
							cJSON_AddStringToObject(o, "genre",  t.genre.c_str());
							cJSON_AddNumberToObject(o, "duration_ms", t.duration_ms);
							cJSON_AddItemToArray(arr, o);
						}
					}
					return arr;
				}

				// ---------------------- PLAY GENRE ------------------------
				if (action == "play") {
					if (genre.empty())
						return "{\"success\": false, \"message\": \"Genre cannot be empty\"}";

					if (!sd_music->buildGenrePlaylist(genre))
						return "{\"success\": false, \"message\": \"No tracks found for this genre\"}";

					bool ok = sd_music->playGenreIndex(0);
					return ok
						? "{\"success\": true, \"message\": \"Playing first track of genre\"}"
						: "{\"success\": false, \"message\": \"Failed to play genre\"}";
				}

				// ---------------------- PLAY BY INDEX ---------------------
				if (action == "play_index") {
					int index = props["index"].value<int>();
					bool ok = sd_music->playGenreIndex(index);
					return ok
						? "{\"success\": true, \"message\": \"Playing track in genre list\"}"
						: "{\"success\": false, \"message\": \"Index invalid or genre list empty\"}";
				}

				// ---------------------- NEXT GENRE TRACK ------------------
				if (action == "next") {
					bool ok = sd_music->playNextGenre();
					return ok
						? "{\"success\": true, \"message\": \"Playing next track in genre\"}"
						: "{\"success\": false, \"message\": \"No next track or no active genre mode\"}";
				}

				return "{\"success\": false, \"message\": \"Unknown genre action\"}";
			}
		);

		// ================== 10) LIỆT KÊ GENRE SẴN CÓ ==================
		// Tool mới: trả danh sách tất cả genre duy nhất trong thư viện hiện tại
		AddTool(
			"self.sdmusic.genre_list",
			"List all unique genres available in the current SD music library.",
			PropertyList(),
			[sd_music](const PropertyList&) -> ReturnValue {
				cJSON* arr = cJSON_CreateArray();
				if (!sd_music) {
					return arr; // mảng rỗng nếu module không có
				}

				// Đảm bảo playlist đã load để listGenres() có dữ liệu
				if (sd_music->getTotalTracks() == 0) {
					sd_music->loadTrackList();
				}

				auto genres = sd_music->listGenres();
				for (auto& g : genres) {
					cJSON_AddItemToArray(arr, cJSON_CreateString(g.c_str()));
				}
				return arr;
			}
		);
	}
		
    // Restore the original tools list to the end of the tools list
    tools_.insert(tools_.end(), original_tools.begin(), original_tools.end());
}

void McpServer::AddUserOnlyTools() {
    // System tools
    AddUserOnlyTool("self.get_system_info",
        "Get the system information",
        PropertyList(),
        [this](const PropertyList& properties) -> ReturnValue {
            auto& board = Board::GetInstance();
            return board.GetSystemInfoJson();
        });

    AddUserOnlyTool("self.reboot", "Reboot the system",
        PropertyList(),
        [this](const PropertyList& properties) -> ReturnValue {
            auto& app = Application::GetInstance();
            app.Schedule([&app]() {
                ESP_LOGW(TAG, "User requested reboot");
                vTaskDelay(pdMS_TO_TICKS(1000));

                app.Reboot();
            });
            return true;
        });

    // Firmware upgrade
    AddUserOnlyTool("self.upgrade_firmware", "Upgrade firmware from a specific URL. This will download and install the firmware, then reboot the device.",
        PropertyList({
            Property("url", kPropertyTypeString, "The URL of the firmware binary file to download and install")
        }),
        [this](const PropertyList& properties) -> ReturnValue {
            auto url = properties["url"].value<std::string>();
            ESP_LOGI(TAG, "User requested firmware upgrade from URL: %s", url.c_str());
            
            auto& app = Application::GetInstance();
            app.Schedule([url, &app]() {
                auto ota = std::make_unique<Ota>();
                
                bool success = app.UpgradeFirmware(*ota, url);
                if (!success) {
                    ESP_LOGE(TAG, "Firmware upgrade failed");
                }
            });
            
            return true;
        });

    // Display control
#ifdef HAVE_LVGL
    auto display = dynamic_cast<LvglDisplay*>(Board::GetInstance().GetDisplay());
    if (display) {
        AddUserOnlyTool("self.screen.get_info", "Information about the screen, including width, height, etc.",
            PropertyList(),
            [display](const PropertyList& properties) -> ReturnValue {
                cJSON *json = cJSON_CreateObject();
                cJSON_AddNumberToObject(json, "width", display->width());
                cJSON_AddNumberToObject(json, "height", display->height());
                if (dynamic_cast<OledDisplay*>(display)) {
                    cJSON_AddBoolToObject(json, "monochrome", true);
                } else {
                    cJSON_AddBoolToObject(json, "monochrome", false);
                }
                return json;
            });

#if CONFIG_LV_USE_SNAPSHOT
        AddUserOnlyTool("self.screen.snapshot", "Snapshot the screen and upload it to a specific URL",
            PropertyList({
                Property("url", kPropertyTypeString),
                Property("quality", kPropertyTypeInteger, 80, 1, 100)
            }),
            [display](const PropertyList& properties) -> ReturnValue {
                auto url = properties["url"].value<std::string>();
                auto quality = properties["quality"].value<int>();

                std::string jpeg_data;
                if (!display->SnapshotToJpeg(jpeg_data, quality)) {
                    throw std::runtime_error("Failed to snapshot screen");
                }

                ESP_LOGI(TAG, "Upload snapshot %u bytes to %s", jpeg_data.size(), url.c_str());
                
                // 构造multipart/form-data请求体
                std::string boundary = "----ESP32_SCREEN_SNAPSHOT_BOUNDARY";
                
                auto http = Board::GetInstance().GetNetwork()->CreateHttp(3);
                http->SetHeader("Content-Type", "multipart/form-data; boundary=" + boundary);
                if (!http->Open("POST", url)) {
                    throw std::runtime_error("Failed to open URL: " + url);
                }
                {
                    // 文件字段头部
                    std::string file_header;
                    file_header += "--" + boundary + "\r\n";
                    file_header += "Content-Disposition: form-data; name=\"file\"; filename=\"screenshot.jpg\"\r\n";
                    file_header += "Content-Type: image/jpeg\r\n";
                    file_header += "\r\n";
                    http->Write(file_header.c_str(), file_header.size());
                }

                // JPEG数据
                http->Write((const char*)jpeg_data.data(), jpeg_data.size());

                {
                    // multipart尾部
                    std::string multipart_footer;
                    multipart_footer += "\r\n--" + boundary + "--\r\n";
                    http->Write(multipart_footer.c_str(), multipart_footer.size());
                }
                http->Write("", 0);

                if (http->GetStatusCode() != 200) {
                    throw std::runtime_error("Unexpected status code: " + std::to_string(http->GetStatusCode()));
                }
                std::string result = http->ReadAll();
                http->Close();
                ESP_LOGI(TAG, "Snapshot screen result: %s", result.c_str());
                return true;
            });
        
        AddUserOnlyTool("self.screen.preview_image", "Preview an image on the screen",
            PropertyList({
                Property("url", kPropertyTypeString)
            }),
            [display](const PropertyList& properties) -> ReturnValue {
                auto url = properties["url"].value<std::string>();
                auto http = Board::GetInstance().GetNetwork()->CreateHttp(3);

                if (!http->Open("GET", url)) {
                    throw std::runtime_error("Failed to open URL: " + url);
                }
                int status_code = http->GetStatusCode();
                if (status_code != 200) {
                    throw std::runtime_error("Unexpected status code: " + std::to_string(status_code));
                }

                size_t content_length = http->GetBodyLength();
                char* data = (char*)heap_caps_malloc(content_length, MALLOC_CAP_8BIT);
                if (data == nullptr) {
                    throw std::runtime_error("Failed to allocate memory for image: " + url);
                }
                size_t total_read = 0;
                while (total_read < content_length) {
                    int ret = http->Read(data + total_read, content_length - total_read);
                    if (ret < 0) {
                        heap_caps_free(data);
                        throw std::runtime_error("Failed to download image: " + url);
                    }
                    if (ret == 0) {
                        break;
                    }
                    total_read += ret;
                }
                http->Close();

                auto image = std::make_unique<LvglAllocatedImage>(data, content_length);
                display->SetPreviewImage(std::move(image));
                return true;
            });
#endif // CONFIG_LV_USE_SNAPSHOT
    }
#endif // HAVE_LVGL

    // Assets download url
    auto& assets = Assets::GetInstance();
    if (assets.partition_valid()) {
        AddUserOnlyTool("self.assets.set_download_url", "Set the download url for the assets",
            PropertyList({
                Property("url", kPropertyTypeString)
            }),
            [](const PropertyList& properties) -> ReturnValue {
                auto url = properties["url"].value<std::string>();
                Settings settings("assets", true);
                settings.SetString("download_url", url);
                return true;
            });
    }
}

void McpServer::AddTool(McpTool* tool) {
    // Prevent adding duplicate tools
    if (std::find_if(tools_.begin(), tools_.end(), [tool](const McpTool* t) { return t->name() == tool->name(); }) != tools_.end()) {
        ESP_LOGW(TAG, "Tool %s already added", tool->name().c_str());
        return;
    }

    ESP_LOGI(TAG, "Add tool: %s%s", tool->name().c_str(), tool->user_only() ? " [user]" : "");
    tools_.push_back(tool);
}

void McpServer::AddTool(const std::string& name, const std::string& description, const PropertyList& properties, std::function<ReturnValue(const PropertyList&)> callback) {
    AddTool(new McpTool(name, description, properties, callback));
}

void McpServer::AddUserOnlyTool(const std::string& name, const std::string& description, const PropertyList& properties, std::function<ReturnValue(const PropertyList&)> callback) {
    auto tool = new McpTool(name, description, properties, callback);
    tool->set_user_only(true);
    AddTool(tool);
}

void McpServer::ParseMessage(const std::string& message) {
    cJSON* json = cJSON_Parse(message.c_str());
    if (json == nullptr) {
        ESP_LOGE(TAG, "Failed to parse MCP message: %s", message.c_str());
        return;
    }
    ParseMessage(json);
    cJSON_Delete(json);
}

void McpServer::ParseCapabilities(const cJSON* capabilities) {
    auto vision = cJSON_GetObjectItem(capabilities, "vision");
    if (cJSON_IsObject(vision)) {
        auto url = cJSON_GetObjectItem(vision, "url");
        auto token = cJSON_GetObjectItem(vision, "token");
        if (cJSON_IsString(url)) {
            auto camera = Board::GetInstance().GetCamera();
            if (camera) {
                std::string url_str = std::string(url->valuestring);
                std::string token_str;
                if (cJSON_IsString(token)) {
                    token_str = std::string(token->valuestring);
                }
                camera->SetExplainUrl(url_str, token_str);
            }
        }
    }
}

void McpServer::ParseMessage(const cJSON* json) {
    // Check JSONRPC version
    auto version = cJSON_GetObjectItem(json, "jsonrpc");
    if (version == nullptr || !cJSON_IsString(version) || strcmp(version->valuestring, "2.0") != 0) {
        ESP_LOGE(TAG, "Invalid JSONRPC version: %s", version ? version->valuestring : "null");
        return;
    }
    
    // Check method
    auto method = cJSON_GetObjectItem(json, "method");
    if (method == nullptr || !cJSON_IsString(method)) {
        ESP_LOGE(TAG, "Missing method");
        return;
    }
    
    auto method_str = std::string(method->valuestring);
    if (method_str.find("notifications") == 0) {
        return;
    }
    
    // Check params
    auto params = cJSON_GetObjectItem(json, "params");
    if (params != nullptr && !cJSON_IsObject(params)) {
        ESP_LOGE(TAG, "Invalid params for method: %s", method_str.c_str());
        return;
    }

    auto id = cJSON_GetObjectItem(json, "id");
    if (id == nullptr || !cJSON_IsNumber(id)) {
        ESP_LOGE(TAG, "Invalid id for method: %s", method_str.c_str());
        return;
    }
    auto id_int = id->valueint;
    
    if (method_str == "initialize") {
        if (cJSON_IsObject(params)) {
            auto capabilities = cJSON_GetObjectItem(params, "capabilities");
            if (cJSON_IsObject(capabilities)) {
                ParseCapabilities(capabilities);
            }
        }
        auto app_desc = esp_app_get_description();
        std::string message = "{\"protocolVersion\":\"2024-11-05\",\"capabilities\":{\"tools\":{}},\"serverInfo\":{\"name\":\"" BOARD_NAME "\",\"version\":\"";
        message += app_desc->version;
        message += "\"}}";
        ReplyResult(id_int, message);
    } else if (method_str == "tools/list") {
        std::string cursor_str = "";
        bool list_user_only_tools = false;
        if (params != nullptr) {
            auto cursor = cJSON_GetObjectItem(params, "cursor");
            if (cJSON_IsString(cursor)) {
                cursor_str = std::string(cursor->valuestring);
            }
            auto with_user_tools = cJSON_GetObjectItem(params, "withUserTools");
            if (cJSON_IsBool(with_user_tools)) {
                list_user_only_tools = with_user_tools->valueint == 1;
            }
        }
        GetToolsList(id_int, cursor_str, list_user_only_tools);
    } else if (method_str == "tools/call") {
        if (!cJSON_IsObject(params)) {
            ESP_LOGE(TAG, "tools/call: Missing params");
            ReplyError(id_int, "Missing params");
            return;
        }
        auto tool_name = cJSON_GetObjectItem(params, "name");
        if (!cJSON_IsString(tool_name)) {
            ESP_LOGE(TAG, "tools/call: Missing name");
            ReplyError(id_int, "Missing name");
            return;
        }
        auto tool_arguments = cJSON_GetObjectItem(params, "arguments");
        if (tool_arguments != nullptr && !cJSON_IsObject(tool_arguments)) {
            ESP_LOGE(TAG, "tools/call: Invalid arguments");
            ReplyError(id_int, "Invalid arguments");
            return;
        }
        DoToolCall(id_int, std::string(tool_name->valuestring), tool_arguments);
    } else {
        ESP_LOGE(TAG, "Method not implemented: %s", method_str.c_str());
        ReplyError(id_int, "Method not implemented: " + method_str);
    }
}

void McpServer::ReplyResult(int id, const std::string& result) {
    std::string payload = "{\"jsonrpc\":\"2.0\",\"id\":";
    payload += std::to_string(id) + ",\"result\":";
    payload += result;
    payload += "}";
    Application::GetInstance().SendMcpMessage(payload);
}

void McpServer::ReplyError(int id, const std::string& message) {
    std::string payload = "{\"jsonrpc\":\"2.0\",\"id\":";
    payload += std::to_string(id);
    payload += ",\"error\":{\"message\":\"";
    payload += message;
    payload += "\"}}";
    Application::GetInstance().SendMcpMessage(payload);
}

void McpServer::GetToolsList(int id, const std::string& cursor, bool list_user_only_tools) {
    const int max_payload_size = 8000;
    std::string json = "{\"tools\":[";
    
    bool found_cursor = cursor.empty();
    auto it = tools_.begin();
    std::string next_cursor = "";
    
    while (it != tools_.end()) {
        // 如果我们还没有找到起始位置，继续搜索
        if (!found_cursor) {
            if ((*it)->name() == cursor) {
                found_cursor = true;
            } else {
                ++it;
                continue;
            }
        }

        if (!list_user_only_tools && (*it)->user_only()) {
            ++it;
            continue;
        }
        
        // 添加tool前检查大小
        std::string tool_json = (*it)->to_json() + ",";
        if (json.length() + tool_json.length() + 30 > max_payload_size) {
            // 如果添加这个tool会超出大小限制，设置next_cursor并退出循环
            next_cursor = (*it)->name();
            break;
        }
        
        json += tool_json;
        ++it;
    }
    
    if (json.back() == ',') {
        json.pop_back();
    }
    
    if (json.back() == '[' && !tools_.empty()) {
        // 如果没有添加任何tool，返回错误
        ESP_LOGE(TAG, "tools/list: Failed to add tool %s because of payload size limit", next_cursor.c_str());
        ReplyError(id, "Failed to add tool " + next_cursor + " because of payload size limit");
        return;
    }

    if (next_cursor.empty()) {
        json += "]}";
    } else {
        json += "],\"nextCursor\":\"" + next_cursor + "\"}";
    }
    
    ReplyResult(id, json);
}

void McpServer::DoToolCall(int id, const std::string& tool_name, const cJSON* tool_arguments) {
    auto tool_iter = std::find_if(tools_.begin(), tools_.end(), 
                                 [&tool_name](const McpTool* tool) { 
                                     return tool->name() == tool_name; 
                                 });
    
    if (tool_iter == tools_.end()) {
        ESP_LOGE(TAG, "tools/call: Unknown tool: %s", tool_name.c_str());
        ReplyError(id, "Unknown tool: " + tool_name);
        return;
    }

    PropertyList arguments = (*tool_iter)->properties();
    try {
        for (auto& argument : arguments) {
            bool found = false;
            if (cJSON_IsObject(tool_arguments)) {
                auto value = cJSON_GetObjectItem(tool_arguments, argument.name().c_str());
                if (argument.type() == kPropertyTypeBoolean && cJSON_IsBool(value)) {
                    argument.set_value<bool>(value->valueint == 1);
                    found = true;
                } else if (argument.type() == kPropertyTypeInteger && cJSON_IsNumber(value)) {
                    argument.set_value<int>(value->valueint);
                    found = true;
                } else if (argument.type() == kPropertyTypeString && cJSON_IsString(value)) {
                    argument.set_value<std::string>(value->valuestring);
                    found = true;
                }
            }

            if (!argument.has_default_value() && !found) {
                ESP_LOGE(TAG, "tools/call: Missing valid argument: %s", argument.name().c_str());
                ReplyError(id, "Missing valid argument: " + argument.name());
                return;
            }
        }
    } catch (const std::exception& e) {
        ESP_LOGE(TAG, "tools/call: %s", e.what());
        ReplyError(id, e.what());
        return;
    }

    // Use main thread to call the tool
    auto& app = Application::GetInstance();
    app.Schedule([this, id, tool_iter, arguments = std::move(arguments)]() {
        try {
            ReplyResult(id, (*tool_iter)->Call(arguments));
        } catch (const std::exception& e) {
            ESP_LOGE(TAG, "tools/call: %s", e.what());
            ReplyError(id, e.what());
        }
    });
}
