// @license
// Copyright (c) Audanika. All Rights Reserved.
//
// Use of this source code is governed by terms that can be
// found in the LICENSE file in the root of this package.

#include "aud_dsp_effects.h"

#include "aud_effects_node.hpp"

AUD_EXPORT int32_t aud_dsp_effects_register(const AudHostApi* host) {
  if (host == nullptr || host->register_node_type == nullptr) {
    return AUD_ERROR_INVALID_ARGUMENT;
  }
  if (host->abi_major != AUD_ABI_VERSION_MAJOR) return AUD_ERROR_ABI_MAJOR;
  if (host->struct_size < sizeof(AudHostApi)) return AUD_ERROR_ABI_MINOR;
  const AudNodeDescriptor* const types[] = {
      &aud_effects::freeverbDescriptor(),   &aud_effects::plateDescriptor(),
      &aud_effects::stereoDelayDescriptor(), &aud_effects::tempoDelayDescriptor(),
      &aud_effects::limiterDescriptor(),    &aud_effects::tremoloDescriptor(),
  };
  for (const AudNodeDescriptor* type : types) {
    const int32_t result = host->register_node_type(host->host, type);
    if (result != AUD_OK) return result;
  }
  return AUD_OK;
}
