//
//  AnimationChain_JNI.cpp
//  ViroRenderer
//
//  Copyright © 2017 Viro Media. All rights reserved.
//

#include <jni.h>
#include <memory>
#include <VROAnimationChain.h>
#include <PersistentRef.h>
#include <VROStringUtil.h>
#include "AnimationGroup_JNI.h"
#include "Node_JNI.h"
#include "VROLog.h"
#include <VROPlatformUtil.h>

#define JNI_METHOD(return_type, method_name) \
  JNIEXPORT return_type JNICALL              \
      Java_com_viro_core_internal_AnimationChain_##method_name

namespace AnimationChain {
    inline jlong jptr(std::shared_ptr<VROAnimationChain> ptr) {
        PersistentRef<VROAnimationChain> *persistentRef = new PersistentRef<VROAnimationChain>(ptr);
        return reinterpret_cast<intptr_t>(persistentRef);
    }

    inline std::shared_ptr<VROAnimationChain> native(jlong ptr) {
        PersistentRef<VROAnimationChain> *persistentRef = reinterpret_cast<PersistentRef<VROAnimationChain> *>(ptr);
        return persistentRef->get();
    }
}

extern "C" {

JNI_METHOD(jlong, nativeCreateAnimationChain)(JNIEnv *env, jclass clazz, jstring executionType) {
    std::vector<std::shared_ptr<VROExecutableAnimation>> emptyChain;
    VROAnimationChainExecution execution = VROAnimationChainExecution::Serial;

    if (VROStringUtil::strcmpinsensitive(VROPlatformGetString(executionType, env), "parallel")) {
        execution = VROAnimationChainExecution::Parallel;
    }

    std::shared_ptr<VROAnimationChain> animationChain = std::make_shared<VROAnimationChain>(emptyChain, execution);
    return AnimationChain::jptr(animationChain);
}

JNI_METHOD(jlong, nativeCopyAnimation)(JNIEnv *env, jobject obj, jlong nativeRef) {
    std::shared_ptr<VROAnimationChain> chain = AnimationChain::native(nativeRef);
    return AnimationChain::jptr(std::dynamic_pointer_cast<VROAnimationChain>(chain->copy()));
}

JNI_METHOD(void, nativeAddAnimationChain)(JNIEnv *env, jobject obj, jlong nativeRef, jlong chainRef) {
    AnimationChain::native(nativeRef)->addAnimation(AnimationChain::native(chainRef));
}

JNI_METHOD(void, nativeAddAnimationGroup)(JNIEnv *env, jobject obj, jlong nativeRef, jlong groupRef) {
    AnimationChain::native(nativeRef)->addAnimation(AnimationGroup::native(groupRef));
}

JNI_METHOD(void, nativeDestroyAnimationChain)(JNIEnv *env, jobject obj, jlong nativeRef) {
    delete reinterpret_cast<PersistentRef<VROAnimationChain> *>(nativeRef);
}

} // extern "C"