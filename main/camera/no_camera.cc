#include "no_camera.h"

NoCamera::NoCamera() {}

NoCamera::~NoCamera() {}

bool NoCamera::Lock(int timeout_ms) {
    return true;
}

void NoCamera::Unlock() {}
