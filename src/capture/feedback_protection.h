#pragma once
#include "capture/capture_frame.h"
#include "capture/capture_source.h"
#include <span>
#include <string>
namespace fps::capture {
struct ScreenRect {std::int64_t left{},top{},right{},bottom{};bool operator==(const ScreenRect&)const=default;};
struct PixelRect {
    std::uint32_t left{},top{},right{},bottom{};
    bool operator==(const PixelRect&)const=default;
};
std::vector<PixelRect> ClipFeedbackMasks(ScreenRect capture,std::span<const ScreenRect> windows,std::uint32_t width,std::uint32_t height);
bool MaskFeedbackPixels(CapturedFrame&,std::span<const PixelRect>,std::string&);
bool IsFriendPushViewer(HWND);
bool ConfigureCaptureExclusion(HWND);
class FeedbackProtection {
public:
    bool ApplyMasks(CapturedFrame&,std::span<const PixelRect>,std::string&);
    bool Apply(CapturedFrame&,const CaptureSourceDescriptor&,std::string&);
private:
    Microsoft::WRL::ComPtr<ID3D11Texture2D> masked_,black_;
    std::vector<std::pair<ULONGLONG,ScreenRect>> history_;
    std::uint32_t width_{},height_{};
};
}
