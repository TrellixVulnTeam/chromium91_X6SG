// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgpu/gpu_shader_module.h"

#include <dawn/webgpu.h>

#include "gpu/command_buffer/client/webgpu_interface.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_shader_module_descriptor.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/modules/webgpu/dawn_callback.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_compilation_info.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_compilation_message.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_device.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"

namespace blink {

// static
GPUShaderModule* GPUShaderModule::Create(
    GPUDevice* device,
    const GPUShaderModuleDescriptor* webgpu_desc,
    ExceptionState& exception_state) {
  DCHECK(device);
  DCHECK(webgpu_desc);

  std::string wgsl_code;
  WGPUShaderModuleWGSLDescriptor wgsl_desc = {};
  WGPUShaderModuleSPIRVDescriptor spirv_desc = {};
  std::string label;
  WGPUShaderModuleDescriptor dawn_desc = {};

  auto wgsl_or_spirv = webgpu_desc->code();
  if (wgsl_or_spirv.IsUSVString()) {
    wgsl_code = wgsl_or_spirv.GetAsUSVString().Utf8();

    wgsl_desc.chain.sType = WGPUSType_ShaderModuleWGSLDescriptor;
    wgsl_desc.source = wgsl_code.c_str();
    dawn_desc.nextInChain = reinterpret_cast<WGPUChainedStruct*>(&wgsl_desc);
  } else {
    DCHECK(wgsl_or_spirv.IsUint32Array());
    NotShared<DOMUint32Array> code = wgsl_or_spirv.GetAsUint32Array();

    uint32_t length_words = 0;
    if (!base::CheckedNumeric<uint32_t>(code->length())
             .AssignIfValid(&length_words)) {
      exception_state.ThrowRangeError(
          "The provided ArrayBuffer exceeds the maximum supported size "
          "(4294967295)");
      return nullptr;
    }

    spirv_desc.chain.sType = WGPUSType_ShaderModuleSPIRVDescriptor;
    spirv_desc.code = code->Data();
    spirv_desc.codeSize = length_words;
    dawn_desc.nextInChain = reinterpret_cast<WGPUChainedStruct*>(&spirv_desc);
  }

  if (webgpu_desc->hasLabel()) {
    label = webgpu_desc->label().Utf8();
    dawn_desc.label = label.c_str();
  }

  GPUShaderModule* shader = MakeGarbageCollected<GPUShaderModule>(
      device, device->GetProcs().deviceCreateShaderModule(device->GetHandle(),
                                                          &dawn_desc));
  shader->setLabel(webgpu_desc->label());
  return shader;
}

GPUShaderModule::GPUShaderModule(GPUDevice* device,
                                 WGPUShaderModule shader_module)
    : DawnObject<WGPUShaderModule>(device, shader_module) {}

void GPUShaderModule::OnCompilationInfoCallback(
    ScriptPromiseResolver* resolver,
    WGPUCompilationInfoRequestStatus status,
    const WGPUCompilationInfo* info) {
  if (status != WGPUCompilationInfoRequestStatus_Success || !info) {
    resolver->Reject(
        MakeGarbageCollected<DOMException>(DOMExceptionCode::kOperationError));
    return;
  }

  // Temporarily immediately create the CompilationInfo info and resolve the
  // promise.
  GPUCompilationInfo* result = MakeGarbageCollected<GPUCompilationInfo>();
  for (uint32_t i = 0; i < info->messageCount; ++i) {
    const WGPUCompilationMessage* message = &info->messages[i];
    result->AppendMessage(MakeGarbageCollected<GPUCompilationMessage>(
        message->message, message->type, message->lineNum, message->linePos));
  }

  resolver->Resolve(result);
}

ScriptPromise GPUShaderModule::compilationInfo(ScriptState* script_state) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();

  auto* callback =
      BindDawnCallback(&GPUShaderModule::OnCompilationInfoCallback,
                       WrapPersistent(this), WrapPersistent(resolver));

  GetProcs().shaderModuleGetCompilationInfo(
      GetHandle(), callback->UnboundCallback(), callback->AsUserdata());
  // WebGPU guarantees that promises are resolved in finite time so we
  // need to ensure commands are flushed.
  EnsureFlush();
  return promise;
}

}  // namespace blink
