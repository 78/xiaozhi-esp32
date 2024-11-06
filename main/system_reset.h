#ifndef _SYSTEM_RESET_H
#define _SYSTEM_RESET_H

class SystemReset {
public:
    static SystemReset& GetInstance() {
        static SystemReset instance;
        return instance;
    }

    void CheckButtons();

private:
    SystemReset(); // 构造函数私有化
    SystemReset(const SystemReset&) = delete; // 禁用拷贝构造
    SystemReset& operator=(const SystemReset&) = delete; // 禁用赋值操作

    void ResetNvsFlash();
    void ResetToFactory();
    void RestartInSeconds(int seconds);
};


#endif
