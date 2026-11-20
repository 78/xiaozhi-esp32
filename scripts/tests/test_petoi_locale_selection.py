"""Run production model selection and locale resolution bodies on the host."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]


class PetoiLocaleSelectionTests(unittest.TestCase):
    def test_real_locale_resolvers_preserve_unmapped_text_and_audio(self):
        compiler = shutil.which('g++') or shutil.which('clang++')
        if not compiler:
            self.skipTest('requires a host C++17 compiler')
        source = (ROOT / 'main/boards/petoi/bittle-ai-c3/runtime_locale.cc').read_text(encoding='utf-8')
        defaults = source[source.index('#if CONFIG_LANGUAGE_ZH_CN'):
                          source.index('#if CONFIG_PETOI_LOCALE_TEST_ENGLISH')]
        methods = source[source.index('const char* RuntimeLocale::Code()'):source.index('}  // namespace petoi')]
        driver = r'''
#include <cstdio>
#include <cstring>
#include <string>
#include "boards/petoi/bittle-ai-c3/runtime_locale.h"
namespace petoi {
''' + defaults + r'''
ProductLocale initial=kBaseLocale;
RuntimeLocale::RuntimeLocale():locale_(initial) {}
namespace locale_assets {
struct TextPair { const char* source; const char* alternate; };
constexpr bool chinese_base=kBaseLocale==ProductLocale::Chinese;
constexpr char chinese_text[]="提示 %s", english_text[]="Hint %s";
constexpr TextPair kTextPairs[]={{chinese_base ? chinese_text : english_text,
                                 chinese_base ? english_text : chinese_text}};
constexpr char base_audio[]="base", alternate_audio[]="alternate";
struct SoundPair { const char* source_start; const char* source_end; const char* alternate_start; const char* alternate_end; };
constexpr SoundPair kSoundPairs[]={{base_audio,base_audio+4,alternate_audio,alternate_audio+9}};
}
''' + methods + r'''
}
#define CHECK(value) do { if(!(value)) { std::fprintf(stderr,"check line %d\n",__LINE__); return 1; } } while(0)
int main() {
    using namespace petoi;
    using namespace locale_assets;
    RuntimeLocale base;
    initial=chinese_base ? ProductLocale::English : ProductLocale::Chinese;
    RuntimeLocale alternate;
    CHECK(std::string(base.Code())==(chinese_base ? "zh-CN" : "en-US"));
    CHECK(std::string(alternate.Code())==(chinese_base ? "en-US" : "zh-CN"));
    CHECK(std::string(base.WakeModelHint())==(chinese_base ? "nihaoxiaozhi" : "hiesp"));
    CHECK(std::string(alternate.WakeModelHint())==(chinese_base ? "hijason" : "nihaoxiaozhi"));
    std::string text=kTextPairs[0].source;
    CHECK(base.Text(text.c_str())==text.c_str());
    CHECK(std::string(alternate.Text(text.c_str()))==kTextPairs[0].alternate);
    const char* opaque="SSID Hint %s";
    CHECK(alternate.Text(opaque)==opaque);
    CHECK(alternate.Text(nullptr)==nullptr);
    auto sound=std::string_view(base_audio,4);
    CHECK(base.Sound(sound)==sound);
    CHECK(alternate.Sound(sound)=="alternate");
    std::string unrelated(sound);
    CHECK(alternate.Sound(unrelated).data()==unrelated.data());
    CHECK(alternate.Sound(sound.substr(1)).data()==sound.data()+1);
}
'''
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory)
            cpp, binary = path / 'test.cc', path / 'test.exe'
            cpp.write_text(driver, encoding='utf-8')
            for chinese_base in (0, 1):
                with self.subTest(chinese_base=chinese_base):
                    compiled = subprocess.run(
                        [compiler, '-std=c++17', '-Wall', '-Wextra', '-Werror',
                         f'-DCONFIG_LANGUAGE_ZH_CN={chinese_base}',
                         f'-DCONFIG_SR_WN_WN9S_HIESP={1 - chinese_base}',
                         '-I', str(ROOT / 'main'), str(cpp), '-o', str(binary)],
                        capture_output=True, text=True)
                    self.assertEqual(compiled.returncode, 0, compiled.stderr)
                    subprocess.run([str(binary)], check=True, capture_output=True, text=True)

    def test_requested_model_is_selected_and_missing_model_is_not_silently_replaced(self):
        compiler = shutil.which('g++') or shutil.which('clang++')
        if not compiler:
            self.skipTest('requires a host C++17 compiler')
        source = (ROOT / 'main/audio/wake_words/esp_wake_word.cc').read_text(encoding='utf-8')
        method = source[source.index('bool EspWakeWord::Initialize('):source.index('void EspWakeWord::OnWakeWordDetected(')]
        driver = r'''
#include <cstdio>
#include <cstring>
#include <string>
#define CONFIG_RUNTIME_LOCALE 1
#define ESP_WN_PREFIX "wn"
#define DET_MODE_95 95
#define TAG "fixture"
template<typename... T> void Log(const char*,const char*,T&&...) {}
#define ESP_LOGI Log
#define ESP_LOGE Log
struct AudioCodec {};
struct srmodel_list_t { int num=2; const char* names[2]={"wn9s_nihaoxiaozhi","wn9s_hijason"}; };
srmodel_list_t fixture;
srmodel_list_t* esp_srmodel_init(const char*) { return &fixture; }
char* esp_srmodel_filter(srmodel_list_t* models,const char* prefix,const char* hint) {
    for(int i=0;i<models->num;++i)
        if(std::strstr(models->names[i],prefix) && (!hint || std::strstr(models->names[i],hint)))
            return const_cast<char*>(models->names[i]);
    return nullptr;
}
struct WakeData {};
std::string selected;
struct esp_wn_iface_t {
    WakeData* create(const char* name,int) { static WakeData data; selected=name;return &data; }
    int get_samp_rate(WakeData*) { return 16000; }
    int get_samp_chunksize(WakeData*) { return 160; }
};
esp_wn_iface_t iface;
void* esp_wn_handle_from_name(const char*) { return &iface; }
struct Board {
    const char* hint=nullptr;
    static Board& GetInstance() { static Board b; return b; }
    const char* GetWakeWordModelHint() const { return hint; }
};
struct EspWakeWord {
    AudioCodec* codec_=nullptr; bool owns_models_=false;
    srmodel_list_t* wakenet_model_=nullptr;
    esp_wn_iface_t* wakenet_iface_=nullptr; WakeData* wakenet_data_=nullptr;
    bool Initialize(AudioCodec*,srmodel_list_t*);
};
''' + method + r'''
#define CHECK(value) do { if(!(value)) { std::fprintf(stderr,"check line %d\n",__LINE__); return 1; } } while(0)
int main() {
    Board::GetInstance().hint="hijason";
    EspWakeWord en; CHECK(en.Initialize(nullptr,&fixture)); CHECK(selected=="wn9s_hijason");
    Board::GetInstance().hint="nihaoxiaozhi";
    EspWakeWord zh; CHECK(zh.Initialize(nullptr,&fixture)); CHECK(selected=="wn9s_nihaoxiaozhi");
    Board::GetInstance().hint=nullptr;
    EspWakeWord standard; CHECK(standard.Initialize(nullptr,&fixture)); CHECK(selected==fixture.names[0]);
    Board::GetInstance().hint="missing";
    EspWakeWord missing; CHECK(!missing.Initialize(nullptr,&fixture)); CHECK(!missing.wakenet_data_);
}
'''
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory)
            cpp, binary = path / 'test.cc', path / 'test.exe'
            cpp.write_text(driver, encoding='utf-8')
            subprocess.run([compiler, '-std=c++17', '-Wall', '-Wextra', '-Werror', str(cpp), '-o', str(binary)],
                           check=True, capture_output=True, text=True)
            subprocess.run([str(binary)], check=True, capture_output=True, text=True)


if __name__ == '__main__':
    unittest.main()
