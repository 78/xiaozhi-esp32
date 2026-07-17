#ifndef CHOREO_H
#define CHOREO_H

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include <esp_err.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// choreo - 舞蹈编排模块
//
// 功能：
//  - 从 mmap assets 分区加载 *.json 舞蹈编排（cJSON 解析）
//  - 按节拍（BPM）对齐音乐与动作
//  - 支持 speed 全局速度缩放、offset_ms 整体偏移
//  - 播放配乐 OGG Opus / WAV（mmap 只读，I2S 输出）
//  - 通过 MCP 工具 "舞蹈编排" 调用
//
// 音乐格式：JSON "music" 字段指定路径，支持 .ogg（OGG Opus 24kHz mono 60ms）
//           和 .wav（16-bit PCM mono 24kHz）
//
// 数据存储：舞蹈文件（JSON + OGG/WAV）在构建时通过 build_default_assets.py
//           --extra_files 打包进官方 mmap 格式的 assets.bin。
//           运行时只读，不支持上传/下载。
//           舞蹈列表通过 assets/choreo_index.json 索引文件维护。
//  {
//    "name":      "dance_01",
//    "music":     "/assets/dance_01.wav",
//    "bpm":       120,
//    "speed":     1.0,
//    "offset_ms": 0,
//    "steps": [
//      { "type": "pause",      "duration_ms": 2000 },
//      { "type": "walk",       "params": { "steps": 4, "direction": "forward" }},
//      { "type": "sway_fb",   "params": { "amplitude": 15, "cycles": 2 }},
//      { "type": "sway_lr",   "params": { "amplitude": 45, "cycles": 2, "dir": 1 }},
//      { "type": "walk",       "params": { "steps": 8, "direction": "forward" }},
//      { "type": "sway_wave", "params": { "amplitude": 20, "cycles": 2, "step_ms": 350 }},
//      { "type": "pause",      "duration_ms": 1000 },
//      { "type": "sway_march", "params": { "amplitude": 20, "cycles": 3, "half_ms": 300 }}
//    ]
//  }
// ============================================================================

/* 舞步类型枚举 */
typedef enum {
    CHOREO_STEP_PAUSE       = 0,   // 静止等待
    CHOREO_STEP_WALK        = 1,   // 行走（forward/backward/left/right）
    CHOREO_STEP_SWAY_FB    = 2,   // 前后摇摆
    CHOREO_STEP_SWAY_LR    = 3,   // 左右摇摆
    CHOREO_STEP_SWAY_TWIST = 4,   // 旋转摇摆
    CHOREO_STEP_SWAY_UPDOWN= 5,   // 上下摇摆
    CHOREO_STEP_SWAY_SIDE_L= 6,   // 左侧侧摇
    CHOREO_STEP_SWAY_SIDE_R= 7,   // 右侧侧摇
    CHOREO_STEP_SWAY_WAVE  = 8,   // 波浪步
    CHOREO_STEP_SWAY_MARCH = 9,   // 原地踏步
    CHOREO_STEP_SWAY_NOD   = 10,  // 侧向点头
    CHOREO_STEP_SWAY_TREMBLE=11,  // 颤抖
    CHOREO_STEP_MAX
} choreo_step_type_t;

/* 行走方向 */
typedef enum {
    CHOREO_DIR_FORWARD  = 0,
    CHOREO_DIR_BACKWARD = 1,
    CHOREO_DIR_LEFT     = 2,
    CHOREO_DIR_RIGHT    = 3,
} choreo_direction_t;

/* 单个舞步参数（union 节省内存）*/
typedef struct {
    choreo_step_type_t type;
    int  duration_ms;   // PAUSE 用；其他类型可填 0（由动作本身决定）
    float speed;         // 本步速度倍率（最终 speed = global_speed * step_speed）
    /* 动作参数 */
    union {
        /* WALK */
        struct {
            int  steps;
            choreo_direction_t direction;
        } walk;
        /* SWAY 通用 */
        struct {
            int amplitude;
            int cycles;
            int dir;        // 仅 LR/SIDE 有效：1=右倾先，-1=左倾先
            int step_ms;    // 仅 WAVE 有效
            int half_ms;    // 仅 MARCH/NOD 有效
        } sway;
    } p;
} choreo_step_t;

/* 完整舞蹈编排 */
typedef struct {
    char  name[64];       // 舞蹈名称（JSON "name"）
    char  music[128];     // 音乐文件路径（JSON "music"）
    int   bpm;            // 音乐 BPM
    float speed;          // 全局速度倍率（JSON "speed"，默认 1.0）
    int   offset_ms;      // 整体偏移 ms（JSON "offset_ms"，默认 0）
    int   step_count;     // steps[] 长度
    choreo_step_t* steps; // 舞步数组（动态分配）
} choreo_routine_t;

/*============================================================================
  舞蹈编排列表（从 mmap assets 的 choreo_index.json 读取）
============================================================================*/

/* 舞蹈编排名称列表 */
typedef struct {
    int    count;      // 名称数量
    char** names;      // 名称数组（JSON 内部 "name" 字段，显示用，每个字符串独立 malloc）
    char** filenames;  // 文件名数组（带 .json 后缀，如 "jiaqiwu.json"，每个字符串独立 malloc）
} choreo_name_list_t;

/* 从 choreo_index.json 读取舞蹈编排名称列表
   调用者必须通过 choreo_name_list_free() 释放 */
choreo_name_list_t choreo_list_names(void);

/* 释放 choreo_name_list_t 占用的内存 */
void               choreo_name_list_free(choreo_name_list_t* list);

/*============================================================================
  初始化 / 释放
============================================================================*/

/* 初始化模块（mmap assets 分区由官方 Assets 模块管理，此处仅做日志）*/
esp_err_t choreo_init(void);

/* 释放编排内存 */
void      choreo_free(choreo_routine_t* routine);

/*============================================================================
  JSON 解析
============================================================================*/

/* 从 /assets/ 中查找 "name" 匹配的 .json 文件并加载舞蹈编排
   成功返回 choreo_routine_t*（调用者负责 choreo_free）
   失败返回 NULL */
choreo_routine_t* choreo_load(const char* name);

/*============================================================================
  执行舞蹈（阻塞，在调用者任务中运行）
============================================================================*/

/* 执行完整舞蹈：
   1. 打开 music 文件，启动 WAV 播放任务
   2. 等待 offset_ms
   3. 按 steps[] 顺序执行舞步（根据 BPM 计算节拍对齐）
   4. 舞蹈结束后等待音乐播完，停止播放
   5. 归位

   注意：此函数会阻塞直到舞蹈完成或收到 emergency_stop。
         应在独立 FreeRTOS 任务中调用。

   返回 ESP_OK / ESP_FAIL
*/
esp_err_t choreo_play(choreo_routine_t* routine);

/* 非阻塞版本：创建任务执行 choreo_play
   task_name 可选，NULL 则自动生成 */
esp_err_t choreo_play_async(choreo_routine_t* routine, const char* task_name);

/* 停止当前舞蹈（设置 emergency_stop 标志，等待任务退出）*/
void      choreo_stop(void);

/* 查询状态 */
bool      choreo_is_playing(void);

/*============================================================================
  WAV 播放（内部使用，也可单独调用）
============================================================================*/

/* 播放 mmap assets 中的 WAV 文件（阻塞，在调用者任务中）
   格式要求：16-bit PCM, monaural, 24kHz（与 I2S 输出一致） */
esp_err_t choreo_wav_play(const char* filepath);

/* 异步播放 WAV（创建任务）*/
esp_err_t choreo_wav_play_async(const char* filepath);

/* 异步播放 OGG Opus（创建任务，自动解码）
   格式要求：OGG Opus 24kHz/mono/60ms */
esp_err_t choreo_opus_play_async(const char* filepath);

/* 停止 WAV 播放 */
void      choreo_wav_stop(void);

/* 查询 WAV 是否正在播放 */
bool      choreo_wav_is_playing(void);

/*============================================================================
  音频输出回调（由 board 注册，避免 choreo 直接管理 I2S 与 codec 冲突）
============================================================================*/

/* PCM 写入回调：返回实际写入的样本数，<0 表示错误 */
typedef int (*choreo_audio_write_cb_t)(const int16_t* data, int samples);

/* 音频控制回调：使能/关闭输出（I2S channel enable/disable）*/
typedef void (*choreo_audio_ctrl_cb_t)(bool enable);

/* 注册音频回调。注册后 choreo 不再创建 I2S 通道，直接通过 codec 输出。
   应在 choreo_init() 之后、首次播放之前调用。 */
void choreo_set_audio_callbacks(choreo_audio_ctrl_cb_t ctrl, choreo_audio_write_cb_t write);

#ifdef __cplusplus
}
#endif

#endif /* CHOREO_H */
