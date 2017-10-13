/*
 * Copyright © 2016 Viro Media. All rights reserved.
 */
/*
 * Java JNI wrapper for linking the following classes across the bridge:
 *
 * Java JNI Wrapper     : com.viro.renderer.RenderContextJni.java
 * Cpp JNI wrapper      : RenderContext_JNI.cpp
 * Cpp Object           : RenderContext
 */
package com.viro.renderer.jni;

/**
 * ViroContext provides context for the Viro application. It is tied to a specific EGL Context
 * and is required for rendering tasks.
 */
public class ViroContext {

    protected long mNativeRef;

    /**
     * Construct a new ViroContext with the native handle to the Renderer.
     *
     * @param mRenderRef The native peer.
     * @hide
     */
    ViroContext(long mRenderRef){
        mNativeRef = nativeCreateViroContext(mRenderRef);
    }

    @Override
    protected void finalize() throws Throwable {
        try {
            dispose();
        } finally {
            super.finalize();
        }
    }

    /**
     * Release native resources associated with this ViroContext.
     */
    public void dispose() {
        if (mNativeRef != 0) {
            nativeDeleteViroContext(mNativeRef);
            mNativeRef = 0;
        }
    }

    private native long nativeCreateViroContext(long mNativeRenderer);
    private native void nativeDeleteViroContext(long mNativeContextRef);
    private native void nativeGetCameraOrientation(long mNativeContextRef, CameraCallback callback);

    /**
     * @hide
     * @param callback
     */
    public void getCameraOrientation(CameraCallback callback) {
        nativeGetCameraOrientation(mNativeRef, callback);
    }

}