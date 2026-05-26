#include <stdint.h>

uint64_t util_get_time_ms(void);
uint64_t util_get_time_us(void);
void util_sleep_ms(int64_t ms);
void util_sleep_us(int64_t us);
char *util_get_string_from_file(const char *path);

char* util_base64_encode(const unsigned char *in, unsigned int inlen, unsigned int *outlen);
unsigned char* util_base64_decode(const char *in, unsigned int inlen, unsigned int *outlen);

static inline int seq_uint64_after(uint64_t a, uint64_t b) { return (int)((int64_t)(a - b) > 0); }
static inline int seq_uint64_after_eq(uint64_t a, uint64_t b) { return (int)((int64_t)(a - b) >= 0); }
#define seq_uint64_before(a, b) seq_uint64_after (b, a)
#define seq_uint64_before_eq(a, b) seq_uint64_after_eq (b, a)
#define seq_uint64_in_range(x, a, b) (seq_uint64_after_eq(x, a) && seq_uint64_before_eq(x, b))

static inline int seq_uint32_after(uint32_t a, uint32_t b) { return (int)((int32_t)(a - b) > 0); }
static inline int seq_uint32_after_eq(uint32_t a, uint32_t b) { return (int)((int32_t)(a - b) >= 0); }
#define seq_uint32_before(a, b) seq_uint32_after (b, a)
#define seq_uint32_before_eq(a, b) seq_uint32_after_eq (b, a)
#define seq_uint32_in_range(x, a, b) (seq_uint32_after_eq(x, a) && seq_uint32_before_eq(x, b))