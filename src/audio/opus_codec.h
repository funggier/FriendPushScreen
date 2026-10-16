#pragma once
#include "audio/audio_model.h"
namespace fps::audio {
class OpusCodec {
public:
    OpusCodec();
    ~OpusCodec();
    OpusCodec(const OpusCodec&)=delete;
    OpusCodec& operator=(const OpusCodec&)=delete;
    bool Start(std::string&);
    void Stop();
    std::vector<std::uint8_t> Encode(std::span<const float>,std::string&);
    std::vector<float> Decode(std::span<const std::uint8_t>,std::string&);
private: struct Impl;std::unique_ptr<Impl> impl_;
};
}
