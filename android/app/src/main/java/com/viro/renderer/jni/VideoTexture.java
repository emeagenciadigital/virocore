/**
 * Copyright © 2016 Viro Media. All rights reserved.
 */
package com.viro.renderer.jni;

import java.lang.ref.WeakReference;

/**
 * Java JNI wrapper for linking the following classes below across the bridge.
 *
 * Java JNI Wrapper     : com.viro.renderer.jni.VideoTextureJni.java
 * Cpp JNI wrapper      : VideoTexture_JNI.cpp
 * Cpp Object           : VROVideoTextureAVP.cpp
 */
public class VideoTexture {
    private static long INVALID_REF = Long.MAX_VALUE;
    protected long mNativeRef = INVALID_REF;
    protected long mNativeDelegateRef = INVALID_REF;

    public VideoTexture(ViroContext viroContext) {
        mNativeRef = nativeCreateVideoTexture(viroContext.mNativeRef, null);
        mNativeDelegateRef = nativeCreateVideoDelegate();
        nativeAttachDelegate(mNativeRef, mNativeDelegateRef);
    }

    public VideoTexture(ViroContext viroContext, String stereoType) {
        mNativeRef = nativeCreateVideoTexture(viroContext.mNativeRef, stereoType);
        mNativeDelegateRef = nativeCreateVideoDelegate();
        nativeAttachDelegate(mNativeRef, mNativeDelegateRef);
    }

    public void delete() {
        nativeDeleteVideoTexture(mNativeRef);
        mNativeRef = INVALID_REF;

        nativeDeleteVideoDelegate(mNativeDelegateRef);
        mNativeDelegateRef = INVALID_REF;
    }

    public void loadSource(String url, ViroContext viroContext){
        nativeLoadSource(mNativeRef, url, viroContext.mNativeRef);
    }
    public boolean isReady(){
        return mNativeRef != INVALID_REF;
    }

    public void pause () {
        nativePause(mNativeRef);
    }
    public void play (){
        nativePlay(mNativeRef);
    }
    public void setMuted(boolean muted){
        nativeSetMuted(mNativeRef, muted);
    }
    public void setVolume(float volume) {
        nativeSetVolume(mNativeRef, volume);
    }
    public void setLoop(boolean loop) {
        nativeSetLoop(mNativeRef, loop);
    }
    public void seekToTime(float seconds){
        nativeSeekToTime(mNativeRef, seconds);
    }

    /**
     * Native Functions called into JNI
     */
    public native long nativeCreateVideoTexture(long renderContext, String stereoType);
    public native long nativeCreateVideoDelegate();
    public native void nativeAttachDelegate(long nativeTexture, long nativeDelegate);
    public native void nativeDeleteVideoTexture(long nativeTexture);
    public native void nativeDeleteVideoDelegate(long nativeDelegate);
    private native void nativePause(long nativeTexture);
    private native void nativePlay(long nativeTexture);
    private native void nativeSetMuted(long nativeTexture, boolean muted);
    private native void nativeSetVolume(long nativeTexture, float volume);
    private native void nativeSetLoop(long nativeTexture, boolean loop);
    private native void nativeSeekToTime(long nativeTexture, float seconds);
    private native void nativeLoadSource(long nativeTexture, String url, long renderContext);

    /**
     * Delegate Callback functions called from JNI
     */
    private WeakReference<VideoDelegate> mDelegate = null;
    public interface VideoDelegate {
        void onVideoBufferStart();
        void onVideoBufferEnd();
        void onVideoFinish();
        // notification that the texture is ready to be loaded
        void onReady();
        void onVideoFailed(String error);
        void onVideoUpdatedTime(float seconds, float totalDuration);
    }

    public void setVideoDelegate(VideoDelegate delegate) {
        mDelegate = new WeakReference<VideoDelegate>(delegate);
        if (isReady()){
            delegate.onReady();
        }
    }

    public void playerWillBuffer() {
        if (mDelegate != null && mDelegate.get() != null && mNativeRef != INVALID_REF) {
            mDelegate.get().onVideoBufferStart();
        }
    }

    public void playerDidBuffer() {
        if (mDelegate != null && mDelegate.get() != null && mNativeRef != INVALID_REF) {
            mDelegate.get().onVideoBufferEnd();
        }
    }

    public void playerDidFinishPlaying() {
        if (mDelegate != null && mDelegate.get() != null && mNativeRef != INVALID_REF) {
            mDelegate.get().onVideoFinish();
        }
    }

    public void onVideoFailed(String error) {
        if (mDelegate != null && mDelegate.get() != null && mNativeRef != INVALID_REF) {
            mDelegate.get().onVideoFailed(error);
        }
    }

    public void onReady() {
        if (mDelegate != null && mDelegate.get() != null && mNativeRef != INVALID_REF) {
            mDelegate.get().onReady();
        }
    }

    public void onVideoUpdatedTime(float currentTimeInSeconds, float totalTimeInSeconds) {
        if (mDelegate != null && mDelegate.get() != null){
            mDelegate.get().onVideoUpdatedTime(currentTimeInSeconds, totalTimeInSeconds);
        }
    }
}
