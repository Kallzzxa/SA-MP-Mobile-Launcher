#pragma once
#include <jni.h>

// Attach native threads only for the duration of a Java callback. Java-owned
// threads must never be detached by us. Method/class references are cached by
// the activity; attaching a native thread does not give it the app classloader.
template <typename VM, typename Env>
class ScopedJniEnv {
public:
    explicit ScopedJniEnv(VM* vm) : vm_(vm) {
        if (!vm_) return;
        const auto status = vm_->GetEnv(reinterpret_cast<void**>(&env_), JNI_VERSION_1_6);
        if (status == JNI_EDETACHED) {
            env_ = nullptr;
            if (vm_->AttachCurrentThread(&env_, nullptr) == JNI_OK && env_) attached_ = true;
            else env_ = nullptr;
        } else if (status != JNI_OK) {
            env_ = nullptr;
        }
    }
    ~ScopedJniEnv() { if (attached_) vm_->DetachCurrentThread(); }
    ScopedJniEnv(const ScopedJniEnv&) = delete;
    ScopedJniEnv& operator=(const ScopedJniEnv&) = delete;
    Env* get() const { return env_; }
private:
    VM* vm_;
    Env* env_ = nullptr;
    bool attached_ = false;
};
