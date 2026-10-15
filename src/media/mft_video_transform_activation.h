#pragma once

#include "media/mft_video_codec_catalog.h"

#include <mfidl.h>
#include <mftransform.h>
#include <wrl/client.h>

namespace fps::media {

bool MatchesMftVideoCodecDescriptor(IMFAttributes* activation,
    const MftVideoCodecDescriptor& descriptor);

HRESULT ActivateMftVideoCodecDescriptor(
    const MftVideoCodecDescriptor& descriptor,
    Microsoft::WRL::ComPtr<IMFActivate>& activation,
    Microsoft::WRL::ComPtr<IMFTransform>& transform);

}  // namespace fps::media
