#include "no_display.h"

NoDisplay::NoDisplay() {}

NoDisplay::~NoDisplay() {}

bool NoDisplay::Lock(int timeout_ms) {
    return true;
}

void NoDisplay::Unlock() {}
