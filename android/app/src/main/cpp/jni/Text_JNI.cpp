//
//  Text_JNI.cpp
//  ViroRenderer
//
//  Copyright © 2016 Viro Media. All rights reserved.
//

#include <jni.h>
#include <memory>
#include "PersistentRef.h"
#include <VROTypefaceAndroid.h>
#include <VROStringUtil.h>
#include "VROText.h"
#include "Node_JNI.h"
#include "TextDelegate_JNI.h"
#include "ViroContext_JNI.h"
#include "VROPlatformUtil.h"

#define JNI_METHOD(return_type, method_name) \
  JNIEXPORT return_type JNICALL              \
      Java_com_viro_renderer_jni_Text_##method_name

namespace Text {
    inline jlong jptr(std::shared_ptr<VROText> shared_node) {
        PersistentRef<VROText> *native_text = new PersistentRef<VROText>(shared_node);
        return reinterpret_cast<intptr_t>(native_text);
    }

    inline std::shared_ptr<VROText> native(jlong ptr) {
        PersistentRef<VROText> *persistentText = reinterpret_cast<PersistentRef<VROText> *>(ptr);
        return persistentText->get();
    }
}

VROTextHorizontalAlignment getHorizontalAlignmentEnum(const std::string& strName) {
    // Depending on string, return the right enum
    if (VROStringUtil::strcmpinsensitive(strName, "Right")) {
        return VROTextHorizontalAlignment::Right;
    } else if (VROStringUtil::strcmpinsensitive(strName, "Center")) {
        return VROTextHorizontalAlignment::Center;
    } else {
        // Default to left alignment
        return VROTextHorizontalAlignment::Left;
    }

}

VROTextVerticalAlignment getVerticalAlignmentEnum(const std::string& strName) {
    // Depending on string, return the right enum
    if (VROStringUtil::strcmpinsensitive(strName, "Bottom")) {
        return VROTextVerticalAlignment::Bottom;
    } else if (VROStringUtil::strcmpinsensitive(strName, "Center")) {
        return VROTextVerticalAlignment::Center;
    } else {
        // Default to Top alignment
        return VROTextVerticalAlignment::Top;
    }

}

VROLineBreakMode getLineBreakModeEnum(const std::string& strName) {
    // Depending on string, return the right enum
    if (VROStringUtil::strcmpinsensitive(strName, "WordWrap")) {
        return VROLineBreakMode::WordWrap;
    } else if (VROStringUtil::strcmpinsensitive(strName, "CharWrap")) {
        return VROLineBreakMode::CharWrap;
    } else if (VROStringUtil::strcmpinsensitive(strName, "Justify")) {
        return VROLineBreakMode::Justify;
    } else {
        // Default to none
        return VROLineBreakMode::None;
    }
}

VROTextClipMode getTextClipModeEnum(const std::string& strName) {
    // Depending on string, return the right enum
    if (VROStringUtil::strcmpinsensitive(strName, "ClipToBounds")) {
        return VROTextClipMode::ClipToBounds;
    } else {
        return VROTextClipMode::None;
    }
}

extern "C" {

JNI_METHOD(jlong, nativeCreateText)(JNIEnv *env,
                                    jobject object,
                                    jlong context_j,
                                    jstring text,
                                    jstring fontFamilyName,
                                    jint size,
                                    jlong color,
                                    jfloat width,
                                    jfloat height,
                                    jstring horizontalAlignment,
                                    jstring verticalAlignment,
                                    jstring lineBreakMode,
                                    jstring clipMode,
                                    jint maxLines) {
    // Get the text string
    const jchar *rawText = env->GetStringChars(text, NULL);
    jsize rawLen = env->GetStringLength(text);

    std::wstring strText;
    strText.assign(rawText, rawText + rawLen);
    env->ReleaseStringChars(text, rawText);

    // Get the color
    float a = ((color >> 24) & 0xFF) / 255.0;
    float r = ((color >> 16) & 0xFF) / 255.0;
    float g = ((color >> 8) & 0xFF) / 255.0;
    float b = (color & 0xFF) / 255.0;
    VROVector4f vecColor(r, g, b, a);

    // Get horizontal alignment
    const char *cStrHAlignment = env->GetStringUTFChars(horizontalAlignment, NULL);
    std::string strHAlignment(cStrHAlignment);
    VROTextHorizontalAlignment horizontalAlignmentEnum = getHorizontalAlignmentEnum(strHAlignment);
    env->ReleaseStringUTFChars(horizontalAlignment, cStrHAlignment);

    // Get vertical alignment
    const char *cStrVAlignment = env->GetStringUTFChars(verticalAlignment, NULL);
    std::string strVAlignment(cStrVAlignment);
    VROTextVerticalAlignment verticalAlignmentEnum = getVerticalAlignmentEnum(strVAlignment);
    env->ReleaseStringUTFChars(verticalAlignment, cStrVAlignment);

    // Get line break mode
    const char *cStrLineBreakMode = env->GetStringUTFChars(lineBreakMode, NULL);
    std::string strLineBreakMode(cStrLineBreakMode);
    VROLineBreakMode lineBreakModeEnum = getLineBreakModeEnum(strLineBreakMode);
    env->ReleaseStringUTFChars(lineBreakMode, cStrLineBreakMode);

    // Get clip mode
    const char *cStrClipMode = env->GetStringUTFChars(clipMode, NULL);
    std::string strClipMode(cStrClipMode);
    VROTextClipMode clipModeEnum = getTextClipModeEnum(strClipMode);
    env->ReleaseStringUTFChars(clipMode, cStrClipMode);

    const char *cStrFontFamilyName = env->GetStringUTFChars(fontFamilyName, NULL);
    std::string strFontFamilyName(cStrFontFamilyName);
    env->ReleaseStringUTFChars(fontFamilyName, cStrFontFamilyName);

    std::shared_ptr<ViroContext> context = ViroContext::native(context_j);
    std::shared_ptr<VRODriver> driver = context->getDriver();
    std::shared_ptr<VROTypeface> typeface = driver.get()->newTypeface(strFontFamilyName, size);

    std::shared_ptr<VROText> vroText = std::make_shared<VROText>(strText, typeface, vecColor, width,
                                                                 height, horizontalAlignmentEnum,
                                                                 verticalAlignmentEnum,
                                                                 lineBreakModeEnum,
                                                                 clipModeEnum, maxLines);

    // Update text on renderer thread (glyph creation requires this)
    VROPlatformDispatchAsyncRenderer([vroText] {
        vroText->update();
    });

    return Text::jptr(vroText);
}

JNI_METHOD(void, nativeDestroyText)(JNIEnv *env,
                                   jclass clazz,
                                   jlong native_text_ref) {
    delete reinterpret_cast<PersistentRef<VROText> *>(native_text_ref);
}

} // extern "C"

