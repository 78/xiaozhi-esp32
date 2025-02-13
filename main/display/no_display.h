#ifndef _NO_DISPLAY_H_
#define _NO_DISPLAY_H_

#include "display.h"

class NoDisplay : public Display {
private:
    virtual bool Lock(int timeout_ms = 0) override;
    virtual void Unlock() override;

public:
    NoDisplay();
    ~NoDisplay();
};

#endif
