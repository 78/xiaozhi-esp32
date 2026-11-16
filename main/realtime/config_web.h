#ifndef ALIYUN_REALTIME_CONFIG_WEB_H
#define ALIYUN_REALTIME_CONFIG_WEB_H

#include <functional>
void SetWifiConfigAction(std::function<void()> action);
void StartAliyunConfigWeb(bool provisioning = false);
void StopAliyunConfigWeb();

#endif
