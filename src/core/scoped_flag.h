#pragma once

namespace firmware_upgrade {
// Worker-local suppression never consumes the user's cancellation request.
class ScopedFlag {
public:
    explicit ScopedFlag(bool& flag) : flag_(flag), previous_(flag) { flag_ = true; }
    ~ScopedFlag() { flag_ = previous_; }
    ScopedFlag(const ScopedFlag&) = delete;
    ScopedFlag& operator=(const ScopedFlag&) = delete;
private:
    bool& flag_;
    bool previous_;
};
}
