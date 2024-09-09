#ifndef _SYSTEM_RESET_H
#define _SYSTEM_RESET_H

class SystemReset {
public:
    SystemReset();

    void CheckButtons();

private:
    void ResetNvsFlash();
    void ResetToFactory();
    void RestartInSeconds(int seconds);
};


#endif
