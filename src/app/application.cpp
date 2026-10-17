#include "app/application.h"

#include "capture/capture_source_catalog.h"

#include "capture/desktop_duplication_capture.h"

#include "capture/media_foundation_capture.h"

#include "capture/windows_graphics_capture.h"

#include "diagnostics/diagnostics_formatter.h"
#include "diagnostics/logger.h"
#include "platform/windows/os_probe.h"
#include "platform/windows/runtime_paths.h"
#include "ui/settings_store.h"

#include <mmsystem.h>
#include <sstream>
#include <objbase.h>
#include <roapi.h>

#include <algorithm>
#include <utility>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <string>

namespace fps::app {
namespace {

const wchar_t* RendererStatusText(
    graphics::RendererMode mode) {
    switch (mode) {
    case graphics::RendererMode::Hardware:
        return L"D3D11 Hardware";
    case graphics::RendererMode::WarpFallback:
        return L"D3D11 WARP Fallback";
    case graphics::RendererMode::Error:
        return L"D3D11 Renderer Error";
    }

    return L"D3D11 Renderer Error";
}

std::string CaptureFailureText(
    const capture::ICaptureSession& session) {
    char code[16]{};
    std::snprintf(
        code,
        sizeof(code),
        "0x%08X",
        static_cast<unsigned int>(
            static_cast<std::uint32_t>(
                session.LastError())));

    return session.StatusText() +
        " (" +
        code +
        ")";
}

std::wstring Utf8ToWide(
    const std::string& value) {
    if (value.empty()) {
        return {};
    }

    const int required =
        MultiByteToWideChar(
            CP_UTF8,
            MB_ERR_INVALID_CHARS,
            value.data(),
            static_cast<int>(
                value.size()),
            nullptr,
            0);

    if (required <= 0) {
        return
            L"[UTF-8 conversion failed]";
    }

    std::wstring output(
        static_cast<std::size_t>(
            required),
        L'\0');

    const int written =
        MultiByteToWideChar(
            CP_UTF8,
            MB_ERR_INVALID_CHARS,
            value.data(),
            static_cast<int>(
                value.size()),
            output.data(),
            required);

    if (written != required) {
        return
            L"[UTF-8 conversion failed]";
    }

    return output;
}

std::string WideStatusToUtf8(
    const std::wstring& value) {
    if (value.empty()) {
        return {};
    }

    const int required =
        WideCharToMultiByte(
            CP_UTF8,
            WC_ERR_INVALID_CHARS,
            value.data(),
            static_cast<int>(
                value.size()),
            nullptr,
            0,
            nullptr,
            nullptr);

    if (required <= 0) {
        return "Unknown";
    }

    std::string output(
        static_cast<std::size_t>(
            required),
        '\0');

    if (WideCharToMultiByte(
            CP_UTF8,
            WC_ERR_INVALID_CHARS,
            value.data(),
            static_cast<int>(
                value.size()),
            output.data(),
            required,
            nullptr,
            nullptr) !=
        required) {
        return "Unknown";
    }

    return output;
}

bool IsPreferenceCommand(
    ui::UiCommandType type) {
    switch (type) {
    case ui::UiCommandType::ApplyVideoSettings:
    case ui::UiCommandType::ToggleSettings:
    case ui::UiCommandType::ToggleStatusBar:
    case ui::UiCommandType::ToggleShowCursor:
    case ui::UiCommandType::SetHost:
    case ui::UiCommandType::SetSharePort:
    case ui::UiCommandType::SetRemotePort:
    case ui::UiCommandType::SelectAudioSource:
    case ui::UiCommandType::SelectAudioOutput:
    case ui::UiCommandType::ToggleReceiverMute:
    case ui::UiCommandType::SetReceiverVolume:
    case ui::UiCommandType::ToggleAlwaysOnTop:
    case ui::UiCommandType::ToggleBorderless:
        return true;

    default:
        return false;
    }
}

}  // namespace

void Application::RefreshDiagnostics() {
    if (diagnostics_service_.
            StartCollection(
                main_window_.Handle())) {
        main_window_.SetCollecting(true);

        diagnostics::Logger::Instance().
            Log(
                diagnostics::LogLevel::Info,
                "diagnostics",
                "collection started");
    }
}

void Application::OnDiagnosticsReady() {
    const auto snapshot =
        diagnostics_service_.Latest();

    main_window_.SetCollecting(false);

    if (snapshot == nullptr) {
        main_window_.SetDiagnosticsText(
            L"Diagnostics collection "
            L"completed without a snapshot.");
        return;
    }

    std::string text;
    text += "Renderer\n";
    text += "Mode: ";
    text += WideStatusToUtf8(
        renderer_status_);
    if(peer_)text += "\nPeer: "+peer_->Snapshot().status;
    if(peer_) {
        const auto p=peer_->Snapshot();std::ostringstream detail;
        detail<<"\nAudio\n"<<p.audio.status<<"\nSource: "<<p.audio.source<<"\nOutput endpoint: "<<p.audio.output
            <<"\nOpus 1.6.1 / 48kHz stereo / 20ms / constrained VBR128kbit/s"
            <<"\nInput native: "<<p.audio.input_format.native_rate<<"Hz / "<<p.audio.input_format.native_channels<<"ch"
            <<"\nAudio generation send/receive: "<<p.audio.outgoing_generation<<"/"<<p.audio.incoming_generation
            <<"\nEncoded/sent/received/decoded: "<<p.audio.encoded<<"/"<<p.audio.sent<<"/"<<p.audio.received<<"/"<<p.audio.decoded
            <<"\nJitter: "<<p.audio.jitter_depth<<"/6 (peak "<<p.audio.max_jitter_depth<<") | PLC "<<p.audio.plc<<" | Drops "<<p.audio.dropped
            <<"\nPlayed frames: "<<p.audio.played_frames<<" | Underflows "<<p.audio.underflows<<" | Overruns "<<p.audio.overruns
            <<"\nCapture->encode mean/max: "<<p.audio.capture_latency_mean_ms<<"/"<<p.audio.capture_latency_max_ms<<"ms"
            <<"\nLatest playout age (same-host clock): "<<p.audio.playout_age_ms<<"ms"
            <<"\n"<<p.audio.catalog.process_reason<<"\nTransport: localhost evaluation, ALPN friendpush/2";
        text+=detail.str();
    }
    text += "\n\n";
    text += diagnostics::
        FormatDiagnosticsText(
            *snapshot,
            true);

    const std::wstring wide_text =
        Utf8ToWide(text);

    const bool ui_updated =
        main_window_.SetDiagnosticsText(
            wide_text);

    const std::string update_message =
        "collection completed; chars=" +
        std::to_string(
            wide_text.size()) +
        "; ui_update=" +
        (ui_updated
             ? "true"
             : "false");

    diagnostics::Logger::Instance().
        Log(
            ui_updated
                ? diagnostics::LogLevel::Info
                : diagnostics::LogLevel::Error,
            "diagnostics",
            update_message);
}

void Application::SaveDiagnostics() {
    const auto snapshot =
        diagnostics_service_.Latest();

    if (snapshot == nullptr) {
        MessageBoxW(
            main_window_.Handle(),
            L"No completed diagnostics "
            L"snapshot is available yet.",
            L"FriendPushScreen",
            MB_OK |
                MB_ICONINFORMATION);
        return;
    }

    const auto path =
        main_window_.
            PromptDiagnosticsSavePath();

    if (!path.has_value()) {
        return;
    }

    if (diagnostics::SaveDiagnosticsUtf8(
            *snapshot,
            *path)) {
        MessageBoxW(
            main_window_.Handle(),
            L"Diagnostics report saved.",
            L"FriendPushScreen",
            MB_OK |
                MB_ICONINFORMATION);

        diagnostics::Logger::Instance().
            Log(
                diagnostics::LogLevel::Info,
                "diagnostics",
                "diagnostics report saved");
    } else {
        MessageBoxW(
            main_window_.Handle(),
            L"Could not save the "
            L"diagnostics report.",
            L"FriendPushScreen",
            MB_OK |
                MB_ICONERROR);

        diagnostics::Logger::Instance().
            Log(
                diagnostics::LogLevel::Error,
                "diagnostics",
                "diagnostics report save failed");
    }
}

void Application::PersistUiPreferences() {
    preferences_writer_.Submit(ui::PersistentPreferencesFromState(ui_state_),ui::DefaultSettingsPath());
}
void Application::ConfigurePeerAudio() {
    if(!peer_)return;audio::AudioPreferences preferences;
    preferences.source_id=ui_state_.preferences.audio_source_id.empty()?(ui_state_.preferences.audio_source==ui::AudioSourceKind::System?"system":"none"):ui_state_.preferences.audio_source_id;
    preferences.output_id=ui_state_.preferences.receiver_output_id;preferences.muted=ui_state_.preferences.receiver_muted;
    preferences.volume=ui_state_.preferences.receiver_volume;preferences.windows_build=ui_state_.windows_build;peer_->ConfigureAudio(std::move(preferences));
}
void Application::ApplyUiState() {
    ConfigurePeerAudio();
    main_window_.ApplyUiState(
        ui_state_);
    UpdateCaptureUi();
}

void Application::UpdateCaptureUi() {
    bool preview_active = false;

    if (capture_session_ != nullptr) {
        const auto state =
            capture_session_->State();

        preview_active =
            state ==
                capture::CaptureSessionState::Starting ||
            state ==
                capture::CaptureSessionState::Previewing;
    }

    main_window_.SetCaptureSources(
        capture_sources_,
        selected_source_id_,
        preview_active && local_preview_intent_,
        ui_state_.preferences.show_cursor,
        capture_status_);
    if(peer_&&peer_->IsActive())return;
    std::ostringstream video;
    if (raw_preview_) {
        video << "Raw preview diagnostic override: video codecs are bypassed.";
    } else if (codec_loop_ && !codec_loop_->EncoderInfo().candidate.name.empty()) {
        const auto& encoder = codec_loop_->EncoderInfo();
        const auto& decoder = codec_loop_->DecoderInfo();
        video << "Encoder: " << (encoder.gpu_frames ? "[GPU] " :
            encoder.cpu_frames ? "[CPU] " : "[waiting] ") << encoder.candidate.name
            << "\nDecoder: " << (decoder.gpu_frames ? "[GPU] " :
                decoder.cpu_frames ? "[CPU] " : "[waiting] ") << decoder.candidate.name;
        video << "\nTarget " << ui_state_.preferences.video.bitrate_mbps << " Mbps"
            << " | Keyframe " << ui_state_.preferences.video.keyframe_interval
            << (encoder.keyframe_interval_applied ? " frames" : " (not accepted)");
        video << " | Encoder low latency " << (ui_state_.preferences.video.low_latency ? "on" : "off");
        if (!encoder.low_latency_applied) video << " (not accepted)";
        video << "\nDecoder low latency on" << (decoder.low_latency_applied ? "" : " (not accepted)");
        if (codec_failed_) video << "\n" << codec_loop_->Error();
    } else if (codec_failed_ && codec_loop_) {
        video << "Codec error: " << codec_loop_->Error();
    } else {
        video << "H.264 send / receive codecs are independent. Local Preview is optional.";
    }
    if (force_software_codec_) video << "\nDiagnostic override: CPU encoder + CPU decoder forced.";
    main_window_.SetVideoCodecStatus(Utf8ToWide(video.str()));
}

void Application::RefreshCaptureSources() {
    auto refreshed =
        capture::BuildCaptureSourceCatalog(
            main_window_.Handle());

    if (selected_source_.has_value()) {
        auto selected =
            std::find_if(
                refreshed.begin(),
                refreshed.end(),
                [this](const auto& source) {
                    return source.id ==
                        selected_source_id_;
                });

        if (selected != refreshed.end()) {
            selected_source_ = *selected;
        } else {
            auto unavailable =
                *selected_source_;
            unavailable.availability =
                capture::CaptureAvailability::
                    Unavailable;
            selected_source_ = unavailable;
            refreshed.push_back(
                std::move(unavailable));

            if (capture_session_ != nullptr &&
                capture_session_->State() ==
                    capture::CaptureSessionState::
                        SourceLost) {
                capture_status_ =
                    capture_session_->
                        StatusText();
            }
        }
    }

    capture_sources_ =
        std::move(refreshed);

    UpdateCaptureUi();
}

std::unique_ptr<capture::ICaptureSession>
Application::CreateCaptureSession(
    const capture::CaptureSourceDescriptor& source) {
    std::unique_ptr<capture::ICaptureSession>
        session;

    switch (source.kind) {
    case capture::CaptureSourceKind::Monitor:
        session = std::make_unique<
            capture::DesktopDuplicationCaptureSession>(
                source,
                renderer_.Device());
        break;

    case capture::CaptureSourceKind::Window:
        session = std::make_unique<
            capture::WindowsGraphicsCaptureSession>(
                source,
                renderer_.Device());
        break;

    case capture::CaptureSourceKind::VideoDevice:
        session = std::make_unique<
            capture::MediaFoundationCaptureSession>(
                source);
        break;
    }

    if (session != nullptr) {
        session->SetCursorCaptureEnabled(
            ui_state_.preferences.show_cursor);
    }

    return session;
}

void Application::StopCaptureSession() {
    RecordCodecMetrics("stop");
    codec_loop_.reset();
    codec_start_tick_ = 0;
    if (timer_resolution_) { timeEndPeriod(1); timer_resolution_ = false; }
    codec_failed_ = false;
    if (capture_session_ != nullptr) {
        capture_session_->Stop();
        capture_session_.reset();
    }

    if(!displaying_peer_)last_preview_frame_ = {};
    last_capture_state_ =
        capture::CaptureSessionState::Stopped;
}

void Application::SelectCaptureSource(
    std::string_view source_id) {
    const auto found =
        std::find_if(
            capture_sources_.begin(),
            capture_sources_.end(),
            [source_id](const auto& source) {
                return source.id == source_id;
            });

    if (found == capture_sources_.end()) {
        capture_status_ =
            "Selected source is no longer available";
        UpdateCaptureUi();
        return;
    }

    if (found->availability !=
        capture::CaptureAvailability::Available) {
        capture_status_ =
            "Selected source is unavailable";
        UpdateCaptureUi();
        return;
    }

    const bool resume_share=peer_&&peer_->WantsShare();
    const bool resume_capture=local_preview_intent_||resume_share;
    if(resume_share)peer_->StopShare();
    StopCaptureSession();
    if(renderer_initialized_&&!displaying_peer_)renderer_.RenderPlaceholder();
    selected_source_=*found;selected_source_id_=found->id;capture_status_="Ready to preview / share";
    if(resume_capture)StartCapturePreview();
    if(resume_share&&capture_session_){std::string error;if(!peer_->StartShare(error))peer_error_=error;}

    UpdateCaptureUi();
}

void Application::StartCapturePreview() {
    if (!selected_source_.has_value()) {
        capture_status_ =
            "Select a source before previewing";
        UpdateCaptureUi();
        return;
    }

    if (selected_source_->availability !=
        capture::CaptureAvailability::Available) {
        capture_status_ =
            "Selected source is unavailable";
        UpdateCaptureUi();
        return;
    }

    StopCaptureSession();

    capture_status_ =
        "Starting local preview...";

    capture_session_ =
        CreateCaptureSession(
            *selected_source_);

    if (capture_session_ == nullptr) {
        capture_status_ =
            "No capture backend is available for this source";
        UpdateCaptureUi();
        return;
    }

    if (!capture_session_->Start()) {
        capture_status_ =
            CaptureFailureText(
                *capture_session_);
        last_capture_state_ =
            capture_session_->State();
        capture_session_.reset();
        if (renderer_initialized_ && !displaying_peer_) {
            renderer_.RenderPlaceholder();
        }
        UpdateCaptureUi();
        return;
    }

    timer_resolution_ = timeBeginPeriod(1) == TIMERR_NOERROR;
    last_capture_state_ =
        capture_session_->State();
    capture_status_ =
        capture_session_->StatusText();

    if (renderer_initialized_ && !displaying_peer_) {
        renderer_.RenderPlaceholder();
    }

    UpdateCaptureUi();
}

void Application::StopCapturePreview() {
    local_preview_intent_=false;
    codec_loop_.reset();codec_failed_=false;
    if(peer_&&peer_->WantsShare()){UpdateCaptureUi();return;}
    StopCaptureSession();

    if (renderer_initialized_ && !displaying_peer_) {
        renderer_.RenderPlaceholder();
    }

    capture_status_ =
        selected_source_.has_value()
            ? "Ready to preview"
            : "No source selected";

    UpdateCaptureUi();
}

void Application::ToggleCapturePreview() {
    if(local_preview_intent_)StopCapturePreview();
    else {local_preview_intent_=true;if(!capture_session_)StartCapturePreview();UpdateCaptureUi();}
}

bool Application::TickCapturePreview() {
    if(!renderer_initialized_||!capture_session_)return false;
    capture::CapturedFrame frame{};const bool acquired=capture_session_->TryAcquireFrame(frame)&&frame.IsValid();
    const auto state=capture_session_->State();
    if(state==capture::CaptureSessionState::SourceLost||state==capture::CaptureSessionState::Error) {
        if(peer_)peer_->StopShare();codec_loop_.reset();
        if(!displaying_peer_){last_preview_frame_={};renderer_.RenderPlaceholder();}
        if(state!=last_capture_state_){last_capture_state_=state;capture_status_=capture_session_->StatusText();UpdateCaptureUi();UpdatePeerUi(true);RefreshCaptureSources();}
        return false;
    }
    last_capture_state_=state;
    const bool sharing=peer_&&peer_->WantsShare();
    if(acquired&&sharing&&selected_source_) {
        auto protected_frame=frame;std::string error;
        if(!feedback_.Apply(protected_frame,*selected_source_,error)) {
            peer_->StopShare();peer_error_=error;capture_status_=error;UpdatePeerUi(true);return false;
        }
        if(!peer_->PushCapture(protected_frame)){capture_status_=peer_->Snapshot().error;UpdatePeerUi(true);}
    }
    if(!local_preview_intent_||(peer_&&peer_->IsReceiving()))return acquired;
    if(codec_failed_)return acquired;
    const auto sink=[this](const capture::CapturedFrame& decoded) {
        if(!renderer_.RenderFrame(decoded))return false;last_preview_frame_=decoded;displaying_peer_=false;return true;
    };
    bool ok=true;
    if(raw_preview_||sharing) {
        codec_loop_.reset();
        if(acquired)ok=sink(frame);
        capture_status_=sharing?"Local source preview | outgoing H.264":"Raw capture preview (diagnostic)";
    } else {
        if(acquired) {
            const auto width=(frame.width+1U)&~1U,height=(frame.height+1U)&~1U;
            if(!codec_loop_||codec_loop_->Width()!=width||codec_loop_->Height()!=height) {
                RecordCodecMetrics("resize");codec_start_tick_=GetTickCount64();codec_loop_=std::make_unique<media::VideoLocalLoop>();
                auto config=ui::MakeVideoCodecConfig(ui_state_.preferences.video,width,height);
                auto encoder=ui_state_.preferences.video.encoder_mode,decoder=ui_state_.preferences.video.decoder_mode;
                if(force_software_codec_){encoder=decoder=media::VideoBackendPolicy::ForceSoftware;config.encoder_id.clear();config.decoder_id.clear();}
                ok=codec_loop_->Initialize(renderer_.Device(),config,encoder,decoder);
            }
            if(ok)ok=codec_loop_->Push(frame,sink);
        }
        if(ok&&codec_loop_)ok=codec_loop_->Poll(sink);
        capture_status_=codec_loop_?codec_loop_->Status():"Waiting for capture to start H.264 local preview";
    }
    if(!ok) {
        codec_failed_=true;capture_status_=codec_loop_?codec_loop_->Status():"Preview renderer failed";
        last_preview_frame_={};renderer_.RenderPlaceholder();
        diagnostics::Logger::Instance().Log(diagnostics::LogLevel::Error,"codec",capture_status_);UpdateCaptureUi();return false;
    }
    if(GetTickCount64()-last_codec_status_tick_>=1000){last_codec_status_tick_=GetTickCount64();UpdateCaptureUi();RecordCodecMetrics("progress");}
    return acquired;
}

void Application::RecordCodecMetrics(const char* stage) {
    if (!codec_metrics_ || !codec_loop_ || codec_start_tick_ == 0) return;
    const auto& m = codec_loop_->Metrics();
    const auto tail = codec_loop_->TailLatency();
    const double seconds = (GetTickCount64() - codec_start_tick_) / 1000.0;
    std::ostringstream out;
    out << stage << " seconds=" << seconds << " capture=" << m.capture_frames
        << " encoded=" << m.encoded_access_units << " decoded=" << m.decoded_frames
        << " rendered=" << m.rendered_frames << " fps=" << (seconds > 0 ? m.rendered_frames / seconds : 0)
        << " queue_max=" << m.max_queue_depth << " drops=" << m.dropped_frames
        << " encode_p95_ms=" << tail.encode_p95_ms << " decode_p95_ms=" << tail.decode_p95_ms
        << " e2e_p95_ms=" << tail.end_to_end_p95_ms << " gpu_input=" << codec_loop_->EncoderInfo().gpu_frames
        << " gpu_output=" << codec_loop_->DecoderInfo().gpu_frames
        << " encoder=" << codec_loop_->EncoderInfo().candidate.name
        << " decoder=" << codec_loop_->DecoderInfo().candidate.name;
    diagnostics::Logger::Instance().Log(diagnostics::LogLevel::Info, "codec-metrics", out.str());
}

void Application::HandleUiCommand(
    const ui::UiCommand& command) {
    bool state_changed = false;

    switch (command.type) {
    case ui::UiCommandType::ToggleSettings:
        ui::ToggleSettingsVisible(
            ui_state_);
        state_changed = true;
        break;

    case ui::UiCommandType::ToggleStatusBar:
        ui::ToggleStatusBarVisible(ui_state_);
        state_changed = true;
        break;

    case ui::UiCommandType::ToggleShareRequest:ToggleShare();return;
    case ui::UiCommandType::ExportPeerIdentity:ExportPeerIdentity();return;
    case ui::UiCommandType::PairWithFriend:PairWithFriend();return;
    case ui::UiCommandType::DisconnectPeer:DisconnectPeer();return;

    case ui::UiCommandType::ApplyVideoSettings:
        if (!ui::IsValidVideoPreferences(command.video)) return;
        RecordCodecMetrics("settings-change");
        ui_state_.preferences.video = command.video;
        ConfigurePeerCodecs();
        codec_loop_.reset();
        codec_failed_ = false;
        codec_start_tick_ = 0;
        state_changed = true;
        break;

    case ui::UiCommandType::SelectCaptureSource:
        SelectCaptureSource(command.text);
        return;

    case ui::UiCommandType::ToggleCapturePreview:
        ToggleCapturePreview();
        return;

    case ui::UiCommandType::RefreshCaptureSources:
        RefreshCaptureSources();
        return;

    case ui::UiCommandType::ToggleShowCursor:
        ui::ToggleShowCursor(ui_state_);
        if (capture_session_ != nullptr) {
            capture_session_->
                SetCursorCaptureEnabled(
                    ui_state_.preferences.
                        show_cursor);
        }
        state_changed = true;
        break;

    case ui::UiCommandType::ToggleConnectionRequest:ConnectPeer();return;

    case ui::UiCommandType::SetHost:
        ui_state_.preferences.host =
            command.text;
        state_changed = true;
        break;

    case ui::UiCommandType::SetSharePort:
        if (command.value >= 1 &&
            command.value <= 65535) {
            ui_state_.preferences.share_port =
                static_cast<std::uint32_t>(
                    command.value);
            state_changed = true;
        }
        break;

    case ui::UiCommandType::SetRemotePort:
        if (command.value >= 1 &&
            command.value <= 65535) {
            ui_state_.preferences.remote_port =
                static_cast<std::uint32_t>(
                    command.value);
            state_changed = true;
        }
        break;

    case ui::UiCommandType::SelectAudioSource: {
        const auto found=std::find_if(ui_state_.audio_sources.begin(),ui_state_.audio_sources.end(),[&](const auto& item){return item.id==command.text&&item.supported;});
        if(found!=ui_state_.audio_sources.end()) {
            ui_state_.preferences.audio_source_id=found->id;
            ui_state_.preferences.audio_source=found->kind==audio::AudioSourceKind::None?ui::AudioSourceKind::None:found->kind==audio::AudioSourceKind::SystemLoopback?ui::AudioSourceKind::System:ui::AudioSourceKind::Device;
            state_changed=true;
        }
        break;
    }
    case ui::UiCommandType::SelectAudioOutput:
        if(command.text.empty()||std::any_of(ui_state_.audio_outputs.begin(),ui_state_.audio_outputs.end(),[&](const auto& item){return item.id==command.text;})) {
            ui_state_.preferences.receiver_output_id=command.text;state_changed=true;
        }
        break;
    case ui::UiCommandType::ToggleReceiverMute:
        ui::ToggleReceiverMute(
            ui_state_);
        state_changed = true;
        break;

    case ui::UiCommandType::SetReceiverVolume:
        ui::SetReceiverVolume(
            ui_state_,
            command.value);
        state_changed = true;
        break;

    case ui::UiCommandType::ToggleAlwaysOnTop:
        ui::ToggleAlwaysOnTop(
            ui_state_);
        state_changed = true;
        break;

    case ui::UiCommandType::ToggleBorderless:
        ui::ToggleBorderless(
            ui_state_);
        state_changed = true;
        break;

    case ui::UiCommandType::ToggleFullscreen:
        ui::ToggleFullscreen(
            ui_state_);
        state_changed = true;
        break;

    case ui::UiCommandType::ExitFullscreen:
        if (ui_state_.fullscreen) {
            ui::SetFullscreen(
                ui_state_,
                false);
            state_changed = true;
        }
        break;

    case ui::UiCommandType::ToggleDiagnostics:
        ui::ToggleDiagnosticsVisible(
            ui_state_);
        state_changed = true;
        break;

    case ui::UiCommandType::RefreshDiagnostics:
        RefreshDiagnostics();
        return;

    case ui::UiCommandType::SaveDiagnostics:
        SaveDiagnostics();
        return;
    }

    if (state_changed) {
        if (IsPreferenceCommand(
                command.type)) {
            PersistUiPreferences();
        }

        ApplyUiState();
    }
}


void Application::PeerError(const std::string& message) {
    peer_error_=message;
    diagnostics::Logger::Instance().Log(diagnostics::LogLevel::Error,"peer",message);
    MessageBoxW(main_window_.Handle(),Utf8ToWide(message).c_str(),L"FriendPushScreen",MB_OK|MB_ICONERROR);
    UpdatePeerUi(true);
}
void Application::ConfigurePeerCodecs() {
    if(!peer_)return;
    auto config=ui::MakeVideoCodecConfig(ui_state_.preferences.video,1920,1080);
    auto encode=ui_state_.preferences.video.encoder_mode,decode=ui_state_.preferences.video.decoder_mode;
    if(force_software_codec_){encode=decode=media::VideoBackendPolicy::ForceSoftware;config.encoder_id.clear();config.decoder_id.clear();}
    peer_->ConfigureCodecs(std::move(config),encode,decode);
}
void Application::ExportPeerIdentity() {
    if(!peer_){PeerError("Local peer identity is unavailable");return;}
    const auto path=main_window_.PromptPairingPath(true);if(!path)return;
    std::string error;
    if(!peer_->ExportIdentity(*path,error)){PeerError(error);return;}
    peer_error_.clear();UpdatePeerUi(true);
}
void Application::PairWithFriend() {
    if(!peer_){PeerError("Local peer identity is unavailable");return;}
    const auto path=main_window_.PromptPairingPath(false);if(!path)return;
    std::string error;auto peer=network::ReadPublicBundle(*path,error);
    if(!peer||!network::ValidateTrustedPeer(*peer_->Identity(),*peer,error)){PeerError(error);return;}
    std::string prompt="Verify this friend's full SHA-256 fingerprint using an independent channel before authorizing:\n\n"+
        peer->fingerprint+"\n\n"+peer->stable_name;
    if(const auto* previous=peer_->TrustedPeer())prompt+="\n\nThis replaces the authorized friend:\n"+previous->fingerprint;
    prompt+="\n\nAuthorize this exact identity?";
    if(MessageBoxW(main_window_.Handle(),Utf8ToWide(prompt).c_str(),L"Authorize Friend Identity",MB_YESNO|MB_ICONQUESTION|MB_DEFBUTTON2)!=IDYES)return;
    if(!peer_->AuthorizePeer(*peer,error)){PeerError(error);return;}
    if(!local_preview_intent_)StopCaptureSession();
    peer_error_.clear();ConfigurePeerCodecs();UpdatePeerUi(true);
}
void Application::ToggleShare() {
    if(!peer_){PeerError("Local peer identity is unavailable");return;}
    if(peer_->WantsShare()) {
        peer_->StopShare();if(!local_preview_intent_)StopCaptureSession();UpdatePeerUi(true);return;
    }
    if(!selected_source_||selected_source_->availability!=capture::CaptureAvailability::Available){PeerError("Select an available source before sharing");return;}
    if(selected_source_->kind==capture::CaptureSourceKind::Window&&
        capture::IsFriendPushViewer(reinterpret_cast<HWND>(selected_source_->native_handle))){PeerError("Cannot share a FriendPushScreen viewer window");return;}
    std::string error;
    if(!peer_->Listen(static_cast<std::uint16_t>(ui_state_.preferences.share_port),error)||!peer_->StartShare(error)){PeerError(error);return;}
    if(!capture_session_)StartCapturePreview();
    if(!capture_session_){peer_->StopShare();PeerError(capture_status_);return;}
    peer_error_.clear();UpdatePeerUi(true);
}
void Application::ConnectPeer() {
    if(!peer_){PeerError("Local peer identity is unavailable");return;}
    std::string error;
    if(!peer_->Connect(ui_state_.preferences.host,static_cast<std::uint16_t>(ui_state_.preferences.remote_port),
        static_cast<std::uint16_t>(ui_state_.preferences.share_port),error)){PeerError(error);return;}
    peer_error_.clear();UpdatePeerUi(true);
}
void Application::DisconnectPeer() {
    if(peer_)peer_->Disconnect();
    if(!local_preview_intent_)StopCaptureSession();
    displaying_peer_=false;last_preview_frame_={};if(renderer_initialized_)renderer_.RenderPlaceholder();
    peer_error_.clear();UpdatePeerUi(true);
}
bool Application::TickPeer() {
    if(!peer_)return false;
    const bool rendered=peer_->Poll([this](const capture::CapturedFrame& frame) {
        if(!renderer_.RenderFrame(frame, false))return false;
        last_preview_frame_=frame;displaying_peer_=true;return true;
    },[this]{
        if(displaying_peer_){displaying_peer_=false;last_preview_frame_={};if(renderer_initialized_)renderer_.RenderPlaceholder();}
    });
    UpdatePeerUi();return rendered;
}
void Application::UpdatePeerUi(bool force) {
    if(!peer_)return;
    const auto now=GetTickCount64();if(!force&&now-last_peer_ui_tick_<1000)return;last_peer_ui_tick_=now;
    const auto p=peer_->Snapshot();
    ui::PeerUiSnapshot state{p.active,p.connecting,p.connected,p.wants_share,p.sharing,p.receiving,
        p.status,p.sharing?"Sharing H.264 | sent "+std::to_string(p.outgoing.delivered)+" frames":
        (p.wants_share?"Waiting for paired friend / source frame":"Not sharing"),p.own_fingerprint,p.peer_fingerprint};
    if(!peer_error_.empty())state.status+=" | "+peer_error_;
    if(!p.audio.catalog.sources.empty()) {
        ui_state_.audio_sources=p.audio.catalog.sources;ui_state_.audio_outputs=p.audio.catalog.outputs;
    }
    ui_state_.audio_status=p.audio.status;
    if(!p.audio.catalog.process_reason.empty())ui_state_.audio_status+="\n"+p.audio.catalog.process_reason;
    ui_state_.audio_sending=p.audio.send_state==audio::AudioState::Active;
    ui_state_.audio_receiving=p.audio.receiving;ui_state_.audio_feedback_blocked=p.audio.feedback_blocked;
    ui::ApplyPeerUiSnapshot(ui_state_,state);
    ui_state_.peer_round_trip_ms=p.connected?p.round_trip_ms:std::nullopt;
    ui_state_.peer_frame_rates=peer_frame_rates_.Update(now,{p.session,p.outgoing_generation,p.incoming_generation,
        p.outgoing.delivered,p.incoming.delivered,p.sharing,p.receiving});
    main_window_.ApplyUiState(ui_state_);
    if(p.active||p.connected) {
        std::ostringstream video;
        video<<"Send: "<<(p.encoder_gpu_frames?"[GPU] ":p.encoder_cpu_frames?"[CPU] ":"")<<(p.encoder.empty()?"waiting for source / peer":p.encoder)
            <<"\nReceive: "<<(p.decoder_gpu_frames?"[GPU] ":p.decoder_cpu_frames?"[CPU] ":"")<<(p.decoder.empty()?"waiting for peer video":p.decoder)
            <<"\nSent "<<p.outgoing.delivered<<" | Received "<<p.incoming.delivered<<" | Queue "<<p.outbound_pending<<"/2"
            <<" | "<<ui_state_.preferences.video.bitrate_mbps<<" Mbps, GOP "<<ui_state_.preferences.video.keyframe_interval
            <<"\nEncoder latency "<<(ui_state_.preferences.video.low_latency?"on":"off")<<" | Decoder real-time on";
        if(!p.error.empty())video<<"\n"<<p.error;
        main_window_.SetVideoCodecStatus(Utf8ToWide(video.str()));
        if(codec_metrics_) {
            std::ostringstream log;
            log<<"connected="<<p.connected<<" sharing="<<p.sharing<<" receiving="<<p.receiving
                <<" ping_valid="<<p.round_trip_ms.has_value()<<" ping_ms="<<p.round_trip_ms.value_or(0)
                <<" send_fps_valid="<<ui_state_.peer_frame_rates.send_fps.has_value()<<" send_fps="<<ui_state_.peer_frame_rates.send_fps.value_or(0)
                <<" view_fps_valid="<<ui_state_.peer_frame_rates.view_fps.has_value()<<" view_fps="<<ui_state_.peer_frame_rates.view_fps.value_or(0)
                <<" session="<<p.session<<" out_generation="<<p.outgoing_generation<<" in_generation="<<p.incoming_generation
                <<" encoded="<<p.outgoing.encoded<<" sent="<<p.outgoing.delivered<<" decoded="<<p.incoming.decoded<<" rendered="<<p.incoming.delivered
                <<" sent_bytes="<<p.transport.sent_bytes<<" received_bytes="<<p.transport.received_bytes
                <<" tx_peak_count="<<p.max_outbound_pending<<" tx_peak_bytes="<<p.max_outbound_bytes
                <<" rx_peak_count="<<p.transport.max_receive_media_count<<" rx_peak_bytes="<<p.transport.max_receive_media_bytes
                <<" drops="<<p.transport.dropped_video<<" flow_recoveries="<<p.flow_recoveries<<" skipped_capture="<<p.skipped_capture<<" oldest_outbound_ms="<<p.oldest_outbound_ms<<" encoder_gpu="<<p.encoder_gpu_frames<<" encoder_cpu="<<p.encoder_cpu_frames
                <<" decoder_gpu="<<p.decoder_gpu_frames<<" decoder_cpu="<<p.decoder_cpu_frames
                <<" encode_p50_ms="<<p.encode_latency.p50_ms<<" encode_p95_ms="<<p.encode_latency.p95_ms<<" encode_p99_ms="<<p.encode_latency.p99_ms
                <<" decode_p50_ms="<<p.decode_latency.p50_ms<<" decode_p95_ms="<<p.decode_latency.p95_ms<<" decode_p99_ms="<<p.decode_latency.p99_ms
                <<" e2e_p50_ms="<<p.end_to_end.p50_ms<<" e2e_p95_ms="<<p.end_to_end.p95_ms<<" e2e_p99_ms="<<p.end_to_end.p99_ms
                <<" encoder="<<p.encoder<<" decoder="<<p.decoder;
            diagnostics::Logger::Instance().Log(diagnostics::LogLevel::Info,"peer-metrics",log.str());
            std::ostringstream audioLog;
            audioLog<<"send_state="<<static_cast<unsigned>(p.audio.send_state)<<" receiving="<<p.audio.receiving<<" guard="<<p.audio.feedback_blocked
                <<" muted="<<p.audio.muted<<" volume="<<ui_state_.preferences.receiver_volume
                <<" out_generation="<<p.audio.outgoing_generation<<" in_generation="<<p.audio.incoming_generation
                <<" captured_frames="<<p.audio.captured_frames<<" encoded="<<p.audio.encoded<<" sent="<<p.audio.sent<<" received="<<p.audio.received
                <<" decoded="<<p.audio.decoded<<" played_frames="<<p.audio.played_frames<<" plc="<<p.audio.plc<<" drops="<<p.audio.dropped
                <<" jitter="<<p.audio.jitter_depth<<" peak_jitter="<<p.audio.max_jitter_depth<<" underflows="<<p.audio.underflows<<" overruns="<<p.audio.overruns
                <<" render_peak_frames="<<p.audio.max_render_queue_frames<<" restarts="<<p.audio.source_restarts
                <<" capture_mean_ms="<<p.audio.capture_latency_mean_ms<<" capture_max_ms="<<p.audio.capture_latency_max_ms<<" playout_age_ms="<<p.audio.playout_age_ms
                <<" tx_count="<<p.transport.send_audio_count<<" rx_count="<<p.transport.receive_audio_count<<" tx_peak="<<p.transport.max_send_audio_count<<" rx_peak="<<p.transport.max_receive_audio_count
                <<" source="<<p.audio.source<<" output="<<p.audio.output<<" error="<<p.audio.error;
            diagnostics::Logger::Instance().Log(diagnostics::LogLevel::Info,"audio-metrics",audioLog.str());
        }
    } else UpdateCaptureUi();
}

int Application::Run(
    HINSTANCE instance,
    int show_command) {
    const HRESULT runtime_hr =
        RoInitialize(
            RO_INIT_SINGLETHREADED);

    const bool should_uninitialize =
        SUCCEEDED(runtime_hr);

    if (FAILED(runtime_hr) &&
        runtime_hr != RPC_E_CHANGED_MODE) {
        return 3;
    }

    wchar_t option[2]{};
    raw_preview_ = GetEnvironmentVariableW(L"FPS_RAW_PREVIEW", option, 2) == 1 && option[0] == L'1';
    option[0] = 0;
    force_software_codec_ = GetEnvironmentVariableW(L"FPS_FORCE_SOFTWARE_CODEC", option, 2) == 1 && option[0] == L'1';
    option[0] = 0;
    codec_metrics_ = GetEnvironmentVariableW(L"FPS_CODEC_METRICS", option, 2) == 1 && option[0] == L'1';

    const auto os =
        platform::windows::
            ProbeOperatingSystem();

    const std::uint32_t windows_build =
        os.build;

    const ui::UiPreferences preferences =
        ui::LoadPreferences(
            ui::DefaultSettingsPath(),
            ui::UiPreferences{});

    ui_state_ =
        ui::MakeStateFromPreferences(
            preferences,
            windows_build);
    {auto audioCatalog=audio::EnumerateAudioDevices(windows_build);ui_state_.audio_sources=std::move(audioCatalog.sources);ui_state_.audio_outputs=std::move(audioCatalog.outputs);ui_state_.audio_status=audioCatalog.process_reason;}

    main_window_.SetResizeCallback(
        [this](
            UINT width,
            UINT height) {
            if (!renderer_initialized_) {
                return;
            }

            renderer_.Resize(
                width,
                height);

            if (last_preview_frame_.IsValid()) {
                renderer_.RenderFrame(
                    last_preview_frame_, !displaying_peer_);
            } else {
                renderer_.RenderPlaceholder();
            }
        });

    main_window_.SetUiCommandCallback(
        [this](
            const ui::UiCommand& command) {
            HandleUiCommand(command);
        });

    main_window_.SetDiagnosticsReadyCallback(
        [this]() {
            OnDiagnosticsReady();
        });

    if (!main_window_.Create(
            instance,
            show_command)) {
        if (should_uninitialize) {
            RoUninitialize();
        }

        return 1;
    }

    renderer_initialized_ =
        renderer_.Initialize(
            main_window_.RenderHandle());

    renderer_status_ =
        RendererStatusText(
            renderer_.Mode());

    main_window_.SetStatusText(
        renderer_status_);

    if (renderer_initialized_) {
        RECT client{};

        if (GetClientRect(
                main_window_.RenderHandle(),
                &client) != FALSE) {
            renderer_.Resize(
                static_cast<UINT>(
                    client.right -
                    client.left),
                static_cast<UINT>(
                    client.bottom -
                    client.top));
        }

        renderer_.RenderPlaceholder();
    }

    if(renderer_initialized_) {
        peer_=std::make_unique<PeerVideoController>();std::string error;
        if(!peer_->Initialize(renderer_.Device(),platform::windows::DefaultDataRoot()/"peer",error)) {
            peer_error_=error;peer_.reset();ui_state_.peer_status=error;
            diagnostics::Logger::Instance().Log(diagnostics::LogLevel::Error,"peer",error);
        }else ConfigurePeerCodecs();
    }
    ApplyUiState();UpdatePeerUi(true);

    // Normalize persisted values against this
    // Windows runtime (for example an Application
    // Audio preference moved from a newer OS).
    PersistUiPreferences();

    RefreshCaptureSources();
    RefreshDiagnostics();

    MSG message{};
    bool quitting = false;
    while (!quitting) {
        // A 100 ms WM_TIMER limited the decoded path to 10 fps. Service UI
        // messages before each capture/codec step; Present paces active frames.
        for (unsigned count = 0; count < 64 && PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE); ++count) {
            if (message.message == WM_QUIT) { quitting = true; break; }
            if (main_window_.HandleShortcut(message)) continue;
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
        if (quitting) break;
        const bool peer_rendered=TickPeer();
        const bool capture_work=TickCapturePreview();
        if(!peer_rendered&&!capture_work) {
            const bool active=(capture_session_&&capture_session_->State()!=capture::CaptureSessionState::SourceLost&&
                capture_session_->State()!=capture::CaptureSessionState::Error)||(peer_&&peer_->IsReceiving());
            HANDLE wake=peer_?peer_->WakeEvent():nullptr;
            MsgWaitForMultipleObjectsEx(wake?1:0,wake?&wake:nullptr,active?1:INFINITE,QS_ALLINPUT,MWMO_INPUTAVAILABLE);
        }
    }

    StopCaptureSession();
    peer_.reset();
    if(!preferences_writer_.Flush())diagnostics::Logger::Instance().Log(diagnostics::LogLevel::Warning,"ui","could not persist final UI preferences");

    diagnostics_service_.WaitForIdle(
        std::chrono::seconds{5});

    if (should_uninitialize) {
        RoUninitialize();
    }

    return static_cast<int>(
        message.wParam);
}

}  // namespace fps::app
