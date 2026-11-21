#ifndef PETOI_RUNTIME_LOCALE_H_
#define PETOI_RUNTIME_LOCALE_H_

#include <atomic>
#include <string_view>

#include "runtime_locale_policy.h"

class NetworkInterface;
namespace petoi {
class RuntimeLocale {
public:
    RuntimeLocale();
    void Prepare(NetworkInterface* network);
    const char* Code() const;
    const char* WakeModelHint() const;
    const char* Text(const char* text) const;
    std::string_view Sound(std::string_view sound) const;

private:
    std::atomic<ProductLocale> locale_{ProductLocale::Chinese};
};
}  // namespace petoi
#endif
