#include "media/mft_video_codec_catalog.h"
#include "media/mft_video_transform_activation.h"
#include "media/video_codec_model.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <mfapi.h>
#include <objbase.h>

#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {

const char* DirectionName(
    fps::media::VideoCodecDirection direction) {
    return direction ==
            fps::media::VideoCodecDirection::Encode
        ? "encode"
        : "decode";
}

const char* BackendName(
    fps::media::VideoBackendClassification value) {
    switch (value) {
    case fps::media::VideoBackendClassification::Hardware:
        return "hardware";
    case fps::media::VideoBackendClassification::Software:
        return "software";
    case fps::media::VideoBackendClassification::Unknown:
        return "unknown";
    }

    return "unknown";
}

std::string HrHex(HRESULT hr) {
    std::ostringstream stream;
    stream
        << "0x"
        << std::hex
        << std::uppercase
        << static_cast<unsigned long>(hr);
    return stream.str();
}

void Probe(
    fps::media::VideoCodecKind codec,
    fps::media::VideoCodecDirection direction) {
    const auto descriptors =
        fps::media::
            EnumerateMftVideoCodecDescriptors(
                codec,
                direction);

    std::cout
        << fps::media::VideoCodecKindName(codec)
        << ' '
        << DirectionName(direction)
        << ": "
        << descriptors.size()
        << " candidate(s)\n";

    for (std::size_t index = 0;
         index < descriptors.size();
         ++index) {
        const auto& descriptor =
            descriptors[index];

        Microsoft::WRL::ComPtr<IMFActivate>
            activation;
        Microsoft::WRL::ComPtr<IMFTransform>
            transform;

        const HRESULT hr =
            fps::media::
                ActivateMftVideoCodecDescriptor(
                    descriptor,
                    activation,
                    transform);

        std::cout
            << "  [" << index << "] "
            << descriptor.candidate.name
            << " | "
            << BackendName(
                   descriptor.candidate.classification)
            << " | d3d11="
            << (descriptor.candidate.d3d11_aware
                    ? "yes"
                    : "no")
            << " | async="
            << (descriptor.asynchronous
                    ? "yes"
                    : "no")
            << " | activate="
            << (SUCCEEDED(hr)
                    ? "PASS"
                    : "FAIL")
            << " ("
            << HrHex(hr)
            << ")\n";

        if (activation != nullptr) {
            activation->ShutdownObject();
        }
    }
}

}  // namespace

void ProbeD3D11DecoderInventory();

int main() {
    const HRESULT co_hr =
        CoInitializeEx(
            nullptr,
            COINIT_MULTITHREADED);

    const bool uninitialize =
        SUCCEEDED(co_hr);

    if (FAILED(co_hr) &&
        co_hr != RPC_E_CHANGED_MODE) {
        std::cerr
            << "CoInitializeEx failed: "
            << HrHex(co_hr)
            << '\n';
        return 2;
    }

    const HRESULT mf_hr =
        MFStartup(
            MF_VERSION,
            MFSTARTUP_FULL);

    if (FAILED(mf_hr)) {
        std::cerr
            << "MFStartup failed: "
            << HrHex(mf_hr)
            << '\n';

        if (uninitialize) {
            CoUninitialize();
        }

        return 3;
    }

    const std::vector<
        fps::media::VideoCodecKind>
        codecs{
            fps::media::VideoCodecKind::H264,
            fps::media::VideoCodecKind::HEVC,
            fps::media::VideoCodecKind::AV1};

    for (const auto codec : codecs) {
        Probe(
            codec,
            fps::media::VideoCodecDirection::Encode);
        Probe(
            codec,
            fps::media::VideoCodecDirection::Decode);
    }

    ProbeD3D11DecoderInventory();

    MFShutdown();

    if (uninitialize) {
        CoUninitialize();
    }

    return 0;
}
