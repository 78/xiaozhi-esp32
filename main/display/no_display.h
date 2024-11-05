#ifndef _NO_DISPLAY_H_
#define _NO_DISPLAY_H_

#include "display.h"

class NoDisplay : public Display {
private:
    virtual void Lock() override;
    virtual void Unlock() override;

public:
    NoDisplay();
    ~NoDisplay();
};

#endif
