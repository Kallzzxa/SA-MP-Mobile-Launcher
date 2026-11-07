#include "../../app/src/main/cpp/samp/java/jni_env_scope.h"
#include <cassert>
struct Env {};
struct VM {
    int result = JNI_OK;
    bool attachOk = true;
    int attachments = 0, detachments = 0;
    Env env;
    int GetEnv(void** out, int version) {
        assert(version == JNI_VERSION_1_6);
        *out = result == JNI_OK ? &env : nullptr;
        return result;
    }
    int AttachCurrentThread(Env** out, void*) {
        ++attachments;
        *out = attachOk ? &env : nullptr;
        return attachOk ? JNI_OK : JNI_ERR;
    }
    int DetachCurrentThread() { ++detachments; return JNI_OK; }
};
int main() {
    { ScopedJniEnv<VM, Env> scope(nullptr); assert(!scope.get()); }
    VM vm;
    { ScopedJniEnv<VM, Env> scope(&vm); assert(scope.get() == &vm.env); }
    assert(vm.attachments == 0 && vm.detachments == 0);
    vm.result = JNI_EDETACHED;
    { ScopedJniEnv<VM, Env> scope(&vm); assert(scope.get() == &vm.env); }
    assert(vm.attachments == 1 && vm.detachments == 1);
    vm.attachOk = false;
    { ScopedJniEnv<VM, Env> scope(&vm); assert(!scope.get()); }
    assert(vm.attachments == 2 && vm.detachments == 1);
    vm.result = JNI_ERR;
    { ScopedJniEnv<VM, Env> scope(&vm); assert(!scope.get()); }
    assert(vm.attachments == 2 && vm.detachments == 1);
}
