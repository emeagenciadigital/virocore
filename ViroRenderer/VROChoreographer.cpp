//
//  VROChoreographer.cpp
//  ViroKit
//
//  Created by Raj Advani on 8/9/17.
//  Copyright © 2017 Viro Media. All rights reserved.
//

#include "VROChoreographer.h"
#include "VRORenderPass.h"
#include "VRODriver.h"
#include "VRORenderTarget.h"
#include "VROImageShaderProgram.h"
#include "VROImagePostProcess.h"
#include "VRORenderContext.h"
#include "VROMatrix4f.h"
#include "VROScene.h"
#include "VROLightingUBO.h"
#include "VROEye.h"
#include "VROLight.h"
#include "VROToneMappingRenderPass.h"
#include "VROShadowMapRenderPass.h"
#include "VROGaussianBlurRenderPass.h"
#include "VROPostProcessEffectFactory.h"
#include "VRORenderMetadata.h"
#include <vector>

#pragma mark - Initialization

VROChoreographer::VROChoreographer(std::shared_ptr<VRODriver> driver) :
    _driver(driver),
    _renderToTexture(false),
    _renderShadows(true),
    _blurScaling(0.25) {
        
    initTargets(driver);
        
    // We use HDR if gamma correction is enabled; this way we can
    // bottle up the gamma correction with the HDR shader
    _renderHDR = driver->isGammaCorrectionEnabled();
    _renderBloom = driver->isBloomEnabled();
    initHDR(driver);

    _postProcessEffectFactory = std::make_shared<VROPostProcessEffectFactory>();
}

VROChoreographer::~VROChoreographer() {
    
}

void VROChoreographer::initTargets(std::shared_ptr<VRODriver> driver) {
    std::vector<std::string> blitSamplers = { "source_texture" };
    std::vector<std::string> blitCode = {
        "uniform sampler2D source_texture;",
        "frag_color = texture(source_texture, v_texcoord);"
    };
    std::shared_ptr<VROShaderProgram> blitShader = VROImageShaderProgram::create(blitSamplers, blitCode, driver);
    _blitPostProcess = driver->newImagePostProcess(blitShader);
    _blitTarget = driver->newRenderTarget(VRORenderTargetType::ColorTexture, 1, 1);
    
    _renderToTextureTarget = driver->newRenderTarget(VRORenderTargetType::ColorTexture, 1, 1);
    _postProcessTarget = driver->newRenderTarget(VRORenderTargetType::ColorTexture, 1, 1);

    if (_renderShadows) {
        if (kDebugShadowMaps) {
            _shadowTarget = driver->newRenderTarget(VRORenderTargetType::DepthTexture, 1, kMaxLights);
        }
        else {
            _shadowTarget = driver->newRenderTarget(VRORenderTargetType::DepthTextureArray, 1, kMaxLights);
        }
    }
}

void VROChoreographer::initHDR(std::shared_ptr<VRODriver> driver) {
    if (!_renderHDR) {
        return;
    }
    
    if (_renderBloom) {
        _hdrTarget = driver->newRenderTarget(VRORenderTargetType::ColorTextureHDR16, 2, 1);
        _blurTargetA = driver->newRenderTarget(VRORenderTargetType::ColorTextureHDR16, 1, 1);
        _blurTargetB = driver->newRenderTarget(VRORenderTargetType::ColorTextureHDR16, 1, 1);
        _gaussianBlurPass = std::make_shared<VROGaussianBlurRenderPass>();
        
        std::vector<std::string> samplers = { "hdr_texture", "bloom_texture" };
        std::vector<std::string> code = {
            "uniform sampler2D hdr_texture;",
            "uniform sampler2D bloom_texture;",
            "highp vec3 hdr_color = texture(hdr_texture, v_texcoord).rgb;",
            "highp vec3 bloom_color = texture(bloom_texture, v_texcoord).rgb;",
            "frag_color = vec4(hdr_color + bloom_color, 1.0);",
        };
        _additiveBlendPostProcess = driver->newImagePostProcess(VROImageShaderProgram::create(samplers, code, driver));
    }
    else {
        _hdrTarget = driver->newRenderTarget(VRORenderTargetType::ColorTextureHDR16, 1, 1);
    }
    _toneMappingPass = std::make_shared<VROToneMappingRenderPass>(VROToneMappingType::Reinhard, driver);
}
        
void VROChoreographer::setViewport(VROViewport viewport, std::shared_ptr<VRODriver> &driver) {
    /*
     The display needs the full viewport, in case it's rendering to a translated
     half of a larger screen (e.g. as in VR).
     */
    driver->getDisplay()->setViewport(viewport);

    /*
     The render targets use an un-translated viewport. We simply blit over the final
     render target to the display, which will translate it to the correct location
     on the display because we gave the display the fully specified viewport.
     */
    VROViewport rtViewport = VROViewport(0, 0, viewport.getWidth(), viewport.getHeight());
    
    _blitTarget->setViewport(rtViewport);
    _postProcessTarget->setViewport(rtViewport);
    _renderToTextureTarget->setViewport(rtViewport);
    if (_hdrTarget) {
        _hdrTarget->setViewport(rtViewport);
    }
    if (_blurTargetA) {
        _blurTargetA->setViewport({ rtViewport.getX(), rtViewport.getY(), (int)(rtViewport.getWidth()  * _blurScaling),
                                                                          (int)(rtViewport.getHeight() * _blurScaling) });
    }
    if (_blurTargetB) {
        _blurTargetB->setViewport({ rtViewport.getX(), rtViewport.getY(), (int)(rtViewport.getWidth()  * _blurScaling),
                                                                          (int)(rtViewport.getHeight() * _blurScaling) });
    }
}

#pragma mark - Main Render Cycle

void VROChoreographer::render(VROEyeType eye, std::shared_ptr<VROScene> scene,
                              const std::shared_ptr<VRORenderMetadata> &metadata,
                              VRORenderContext *context,
                              std::shared_ptr<VRODriver> &driver) {
    
    if (!_renderShadows) {
        renderBasePass(scene, metadata, context, driver);
    }
    else {
        if (eye == VROEyeType::Left || eye == VROEyeType::Monocular) {
            renderShadowPasses(scene, context, driver);
        }
        renderBasePass(scene, metadata, context, driver);
    }
}

void VROChoreographer::renderShadowPasses(std::shared_ptr<VROScene> scene, VRORenderContext *context,
                                        std::shared_ptr<VRODriver> &driver) {
    
    const std::vector<std::shared_ptr<VROLight>> &lights = scene->getLights();
    
    // Get the max requested shadow map size; use that for our render target
    int maxSize = 0;
    for (const std::shared_ptr<VROLight> &light : lights) {
        if (!light->getCastsShadow()) {
            continue;
        }
        maxSize = std::max(maxSize, light->getShadowMapSize());
    }

    if (maxSize == 0) {
        // No lights are casting a shadow
        return;
    }
    _shadowTarget->setViewport({ 0, 0, maxSize, maxSize });
    
    std::map<std::shared_ptr<VROLight>, std::shared_ptr<VROShadowMapRenderPass>> activeShadowPasses;
    
    int i = 0;
    for (const std::shared_ptr<VROLight> &light : lights) {
        if (!light->getCastsShadow()) {
            continue;
        }
        
        std::shared_ptr<VROShadowMapRenderPass> shadowPass;
        
        // Get the shadow pass for this light if we already have one from the last frame;
        // otherwise, create a new one
        auto it = _shadowPasses.find(light);
        if (it == _shadowPasses.end()) {
            shadowPass = std::make_shared<VROShadowMapRenderPass>(light, driver);
        }
        else {
            shadowPass = it->second;
        }
        activeShadowPasses[light] = shadowPass;
        
        pglpush("Shadow Pass");
        if (!kDebugShadowMaps) {
            _shadowTarget->setTextureImageIndex(i, 0);
        }
        light->setShadowMapIndex(i);
        
        VRORenderPassInputOutput inputs;
        inputs[kRenderTargetSingleOutput] = _shadowTarget;
        shadowPass->render(scene, inputs, context, driver);
        
        driver->unbindShader();
        pglpop();
        
        ++i;
    }
    
    // If any shadow was rendered, set the shadow map in the context; otherwise
    // make it null
    if (i > 0) {
        context->setShadowMap(_shadowTarget->getTexture(0));
    }
    else {
        context->setShadowMap(nullptr);
    }
    
    // Shadow passes that weren't used this frame (e.g. are not in activeShadowPasses),
    // are removed
    _shadowPasses = activeShadowPasses;
}

void VROChoreographer::renderBasePass(std::shared_ptr<VROScene> scene,
                                      const std::shared_ptr<VRORenderMetadata> &metadata,
                                      VRORenderContext *context, std::shared_ptr<VRODriver> &driver) {
    VRORenderPassInputOutput inputs;
    if (_renderHDR) {
        std::shared_ptr<VRORenderTarget> toneMappedTarget;

        if (_renderBloom && metadata->requiresBloomPass()) {
            // Render the scene + bloom to the floating point HDR MRT target
            inputs[kRenderTargetSingleOutput] = _hdrTarget;
            _baseRenderPass->render(scene, inputs, context, driver);
            _hdrTarget->discardTransientBuffers();
            
            // Blur the image. The finished result will reside in _blurTargetB.
            inputs[kGaussianInput] = _hdrTarget;
            inputs[kGaussianPingPongA] = _blurTargetA;
            inputs[kGaussianPingPongB] = _blurTargetB;
            _gaussianBlurPass->render(scene, inputs, context, driver);
            
            // Additively blend the bloom back into the image, store in _postProcessTarget
            _additiveBlendPostProcess->blit(_hdrTarget, 0, _postProcessTarget, { _blurTargetB->getTexture(0) }, driver);
            _postProcessTarget->discardTransientBuffers();
            
            // Run additional post-processing on the normal HDR image
            bool postProcessed = _postProcessEffectFactory->handlePostProcessing(_postProcessTarget, _hdrTarget, driver);
            
            // Blend, tone map, and gamma correct
            if (postProcessed) {
                _hdrTarget->discardTransientBuffers();
                inputs[kToneMappingHDRInput] = _hdrTarget;
            }
            else {
                inputs[kToneMappingHDRInput] = _postProcessTarget;
            }
            if (_renderToTexture) {
                inputs[kToneMappingOutput] = _blitTarget;
                _toneMappingPass->render(scene, inputs, context, driver);
                _blitTarget->discardTransientBuffers();
                
                renderToTextureAndDisplay(_blitTarget, driver);
            }
            else {
                inputs[kToneMappingOutput] = driver->getDisplay();
                _toneMappingPass->render(scene, inputs, context, driver);
            }
        }
        else {
            // Render the scene to the floating point HDR target
            inputs[kRenderTargetSingleOutput] = _hdrTarget;
            _baseRenderPass->render(scene, inputs, context, driver);
            _hdrTarget->discardTransientBuffers();
            
            // Run additional post-processing on the HDR image
            bool postProcessed = _postProcessEffectFactory->handlePostProcessing(_hdrTarget, _postProcessTarget, driver);
            
            // Perform tone-mapping with gamma correction
            if (postProcessed) {
                _postProcessTarget->discardTransientBuffers();
                inputs[kToneMappingHDRInput] = _postProcessTarget;
            }
            else {
                inputs[kToneMappingHDRInput] = _hdrTarget;
            }
            if (_renderToTexture) {
                inputs[kToneMappingOutput] = _blitTarget;
                _toneMappingPass->render(scene, inputs, context, driver);
                _blitTarget->discardTransientBuffers();
                
                renderToTextureAndDisplay(_blitTarget, driver);
            }
            else {
                inputs[kToneMappingOutput] = driver->getDisplay();
                _toneMappingPass->render(scene, inputs, context, driver);
            }
        }
    }
    else if (_renderToTexture) {
        inputs[kRenderTargetSingleOutput] = _blitTarget;
        _baseRenderPass->render(scene, inputs, context, driver);
        _blitTarget->discardTransientBuffers();
        
        renderToTextureAndDisplay(_blitTarget, driver);
    }
    else {
        // Render to the display directly
        inputs[kRenderTargetSingleOutput] = driver->getDisplay();
        _baseRenderPass->render(scene, inputs, context, driver);
    }
}

#pragma mark - Render to Texture

void VROChoreographer::renderToTextureAndDisplay(std::shared_ptr<VRORenderTarget> input,
                                                 std::shared_ptr<VRODriver> driver) {
    // Flip/render the image to the RTT target
    input->blitColor(_renderToTextureTarget, true);
    
    // Blit direct to the display. We can't use the blitColor method here
    // because the display is multisampled (blitting to a multisampled buffer
    // is not supported).
    _blitPostProcess->blit(input, 0, driver->getDisplay(), {}, driver);
    if (_renderToTextureCallback) {
        _renderToTextureCallback();
    }
}

void VROChoreographer::setRenderToTextureEnabled(bool enabled) {
    _renderToTexture = enabled;
}

void VROChoreographer::setRenderToTextureCallback(std::function<void()> callback) {
    _renderToTextureCallback = callback;
}

void VROChoreographer::setRenderTexture(std::shared_ptr<VROTexture> texture) {
    _renderToTextureTarget->attachTexture(texture, 0);
}

std::shared_ptr<VROToneMappingRenderPass> VROChoreographer::getToneMapping() {
    return _toneMappingPass;
}

std::shared_ptr<VROPostProcessEffectFactory> VROChoreographer::getPostProcessEffectFactory(){
    return _postProcessEffectFactory;
}
