// mbedtls 3.x hides struct internals; the ECP keypair fields (grp/Q/d) have
// no public accessors that fit signing + point export, so use the sanctioned
// escape hatch (same pattern as ESP-IDF's own examples). MUST precede every
// mbedtls include in this translation unit — including the ones pulled in by
// device_identity.h below.
#define MBEDTLS_ALLOW_PRIVATE_ACCESS

#include "device_identity.h"

#ifdef CONFIG_DEVICE_JWT_AUTH

#include "settings.h"
#include "system_info.h"

#include <esp_log.h>
#include <esp_random.h>
#include <mbedtls/base64.h>
#include <mbedtls/ecdsa.h>
#include <mbedtls/ecp.h>
#include <mbedtls/sha256.h>
#include <cstring>
#include <ctime>
#include <vector>

#define TAG "DeviceIdentity"

namespace {

constexpr char kNvsNamespace[] = "devauth";
constexpr char kNvsKey[] = "pk_der_b64";
// Contract constants from robo-worker src/auth/device.ts — do not drift.
constexpr char kAudience[] = "robo-worker/device";
constexpr int kJwtTtlSec = 120; // DEVICE_JWT_MAX_TTL_SEC
// Wall clock sanity floor (Sep 2020): anything below means settimeofday from
// the OTA server_time has not happened yet this boot.
constexpr time_t kSaneClockFloor = 1600000000;

std::string Base64Url(const uint8_t* data, size_t len) {
    size_t out_len = 0;
    // Query required size first (returns MBEDTLS_ERR_BASE64_BUFFER_TOO_SMALL).
    mbedtls_base64_encode(nullptr, 0, &out_len, data, len);
    std::vector<uint8_t> buf(out_len);
    if (mbedtls_base64_encode(buf.data(), buf.size(), &out_len, data, len) != 0) {
        return "";
    }
    std::string out(reinterpret_cast<char*>(buf.data()), out_len);
    for (auto& ch : out) {
        if (ch == '+') ch = '-';
        else if (ch == '/') ch = '_';
    }
    while (!out.empty() && out.back() == '=') out.pop_back();
    return out;
}

std::string Base64Std(const uint8_t* data, size_t len) {
    size_t out_len = 0;
    mbedtls_base64_encode(nullptr, 0, &out_len, data, len);
    std::vector<uint8_t> buf(out_len);
    if (mbedtls_base64_encode(buf.data(), buf.size(), &out_len, data, len) != 0) {
        return "";
    }
    return std::string(reinterpret_cast<char*>(buf.data()), out_len);
}

std::vector<uint8_t> Base64Decode(const std::string& in) {
    size_t out_len = 0;
    mbedtls_base64_decode(nullptr, 0, &out_len, reinterpret_cast<const uint8_t*>(in.data()), in.size());
    std::vector<uint8_t> buf(out_len);
    if (mbedtls_base64_decode(buf.data(), buf.size(), &out_len, reinterpret_cast<const uint8_t*>(in.data()),
                              in.size()) != 0) {
        return {};
    }
    buf.resize(out_len);
    return buf;
}

}  // namespace

DeviceIdentity& DeviceIdentity::GetInstance() {
    static DeviceIdentity instance;
    return instance;
}

bool DeviceIdentity::ClockLooksSane() {
    return time(nullptr) > kSaneClockFloor;
}

bool DeviceIdentity::EnsureKey() {
    std::lock_guard<std::mutex> lock(mutex_);
    return EnsureKeyLocked();
}

bool DeviceIdentity::EnsureKeyLocked() {
    if (key_ready_) return true;

    if (!drbg_ready_) {
        mbedtls_entropy_init(&entropy_);
        mbedtls_ctr_drbg_init(&drbg_);
        const char* pers = "tuni-device-identity";
        int ret = mbedtls_ctr_drbg_seed(&drbg_, mbedtls_entropy_func, &entropy_,
                                        reinterpret_cast<const unsigned char*>(pers), strlen(pers));
        if (ret != 0) {
            ESP_LOGE(TAG, "ctr_drbg_seed failed: -0x%04x", -ret);
            return false;
        }
        drbg_ready_ = true;
    }

    mbedtls_pk_init(&pk_);
    if (LoadFromNvs()) {
        key_ready_ = true;
        return true;
    }
    mbedtls_pk_free(&pk_);
    mbedtls_pk_init(&pk_);
    if (GenerateAndPersist()) {
        key_ready_ = true;
        return true;
    }
    mbedtls_pk_free(&pk_);
    return false;
}

bool DeviceIdentity::LoadFromNvs() {
    Settings settings(kNvsNamespace, false);
    std::string der_b64 = settings.GetString(kNvsKey);
    if (der_b64.empty()) return false;
    auto der = Base64Decode(der_b64);
    if (der.empty()) {
        ESP_LOGW(TAG, "stored key is not valid base64 — regenerating");
        return false;
    }
    int ret = mbedtls_pk_parse_key(&pk_, der.data(), der.size(), nullptr, 0, mbedtls_ctr_drbg_random, &drbg_);
    if (ret != 0) {
        ESP_LOGW(TAG, "stored key failed to parse (-0x%04x) — regenerating", -ret);
        return false;
    }
    ESP_LOGI(TAG, "device identity key loaded from NVS");
    return true;
}

bool DeviceIdentity::GenerateAndPersist() {
    int ret = mbedtls_pk_setup(&pk_, mbedtls_pk_info_from_type(MBEDTLS_PK_ECKEY));
    if (ret != 0) {
        ESP_LOGE(TAG, "pk_setup failed: -0x%04x", -ret);
        return false;
    }
    ret = mbedtls_ecp_gen_key(MBEDTLS_ECP_DP_SECP256R1, mbedtls_pk_ec(pk_), mbedtls_ctr_drbg_random, &drbg_);
    if (ret != 0) {
        ESP_LOGE(TAG, "ecp_gen_key failed: -0x%04x", -ret);
        return false;
    }

    // mbedtls writes DER at the END of the buffer and returns the length.
    uint8_t der[256];
    ret = mbedtls_pk_write_key_der(&pk_, der, sizeof(der));
    if (ret <= 0) {
        ESP_LOGE(TAG, "pk_write_key_der failed: -0x%04x", -ret);
        return false;
    }
    const uint8_t* der_start = der + sizeof(der) - ret;
    std::string der_b64 = Base64Std(der_start, ret);
    if (der_b64.empty()) return false;

    Settings settings(kNvsNamespace, true);
    settings.SetString(kNvsKey, der_b64);
    ESP_LOGI(TAG, "device identity key generated and persisted (first boot)");
    return true;
}

std::string DeviceIdentity::GetPublicJwkJson() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!EnsureKeyLocked()) return "";

    mbedtls_ecp_keypair* kp = mbedtls_pk_ec(pk_);
    // Uncompressed point: 0x04 || X(32) || Y(32)
    uint8_t point[65];
    size_t olen = 0;
    int ret = mbedtls_ecp_point_write_binary(&kp->grp, &kp->Q, MBEDTLS_ECP_PF_UNCOMPRESSED, &olen, point,
                                             sizeof(point));
    if (ret != 0 || olen != 65) {
        ESP_LOGE(TAG, "point export failed: -0x%04x olen=%u", -ret, static_cast<unsigned>(olen));
        return "";
    }
    std::string x = Base64Url(point + 1, 32);
    std::string y = Base64Url(point + 33, 32);
    if (x.empty() || y.empty()) return "";
    return std::string("{\"kty\":\"EC\",\"crv\":\"P-256\",\"x\":\"") + x + "\",\"y\":\"" + y + "\"}";
}

std::string DeviceIdentity::SignOtaJwt() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!EnsureKeyLocked()) return "";

    std::string device_id = SystemInfo::GetMacAddress();  // lowercase colon MAC = fleet Device-Id
    if (device_id.empty()) return "";

    // jti: 16 random bytes as 32 hex chars (well under the 128-char cap).
    uint8_t rnd[16];
    esp_fill_random(rnd, sizeof(rnd));
    char jti[33];
    for (int i = 0; i < 16; i++) snprintf(&jti[i * 2], 3, "%02x", rnd[i]);

    time_t now = time(nullptr);
    char payload[320];
    // sub == iss is enforced server-side; aud/ttl fixed by contract (§5.1).
    snprintf(payload, sizeof(payload),
             "{\"sub\":\"%s\",\"iss\":\"%s\",\"aud\":\"%s\",\"jti\":\"%s\",\"iat\":%lld,\"exp\":%lld}",
             device_id.c_str(), device_id.c_str(), kAudience, jti, static_cast<long long>(now),
             static_cast<long long>(now + kJwtTtlSec));

    static const char kHeader[] = "{\"alg\":\"ES256\",\"typ\":\"JWT\"}";
    std::string signing_input =
        Base64Url(reinterpret_cast<const uint8_t*>(kHeader), sizeof(kHeader) - 1) + "." +
        Base64Url(reinterpret_cast<const uint8_t*>(payload), strlen(payload));

    uint8_t hash[32];
    if (mbedtls_sha256(reinterpret_cast<const uint8_t*>(signing_input.data()), signing_input.size(), hash, 0) != 0) {
        return "";
    }

    // Sign to (r, s) MPIs, then serialize as the raw 64-byte r||s form WebCrypto
    // verifies (mbedtls' pk_sign would emit DER, which the server rejects).
    mbedtls_ecp_keypair* kp = mbedtls_pk_ec(pk_);
    mbedtls_mpi r, s;
    mbedtls_mpi_init(&r);
    mbedtls_mpi_init(&s);
    int ret = mbedtls_ecdsa_sign(&kp->grp, &r, &s, &kp->d, hash, sizeof(hash), mbedtls_ctr_drbg_random, &drbg_);
    if (ret != 0) {
        ESP_LOGE(TAG, "ecdsa_sign failed: -0x%04x", -ret);
        mbedtls_mpi_free(&r);
        mbedtls_mpi_free(&s);
        return "";
    }
    uint8_t sig[64];
    ret = mbedtls_mpi_write_binary(&r, sig, 32);
    int ret2 = mbedtls_mpi_write_binary(&s, sig + 32, 32);
    mbedtls_mpi_free(&r);
    mbedtls_mpi_free(&s);
    if (ret != 0 || ret2 != 0) return "";

    return signing_input + "." + Base64Url(sig, sizeof(sig));
}

#endif  // CONFIG_DEVICE_JWT_AUTH
