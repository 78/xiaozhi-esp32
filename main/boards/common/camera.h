#ifndef CAMERA_H
#define CAMERA_H

#include <string>

class Camera {
public:
    virtual ~Camera() = default;

    virtual void SetExplainUrl(const std::string& url, const std::string& token) = 0;
    virtual bool Capture() = 0;
    virtual bool SetHMirror(bool enabled) = 0;
    virtual bool SetVFlip(bool enabled) = 0;
    virtual bool SetSwapBytes(bool enabled) { return false; }  // Optional, default no-op
    // Optional capture resolution for vision/explain. Accepted names include:
    // QQVGA, QVGA, HVGA, VGA, SVGA, XGA, HD, SXGA, UXGA (case-insensitive).
    // Empty string is a no-op (returns true). Non-empty default returns false (unsupported).
    virtual bool SetFrameSize(const std::string& frame_size) { return frame_size.empty(); }
    virtual std::string Explain(const std::string& question) = 0;
};

#endif  // CAMERA_H
