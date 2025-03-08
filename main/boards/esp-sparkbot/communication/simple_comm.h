#ifndef __SIMPLE_COMM_H__
#define __SIMPLE_COMM_H__

#include <vector>
#include <string>
#include <functional>
#include <memory>

namespace iot {

class SimpleComm {
public:

    using RecvCallback = std::function<void(const std::vector<uint8_t>&)>;

    virtual ~SimpleComm() = default;
    virtual int Init() = 0;
    virtual int Send(const std::string& str) = 0;
    virtual void SetRecvCallback(RecvCallback callback) = 0;
};


} // namespace iot

#endif