// Studio-owned Windows process-tree loopback capture helper.
//
// API and activation sequence references:
// https://learn.microsoft.com/windows/win32/api/audioclientactivationparams/
// https://github.com/microsoft/Windows-classic-samples/tree/main/Samples/ApplicationLoopback

#include <audioclient.h>
#include <audioclientactivationparams.h>
#include <mmdeviceapi.h>
#include <windows.h>
#include <wrl.h>
#include <wrl/implements.h>

#include <algorithm>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <limits>
#include <memory>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

using Microsoft::WRL::ComPtr;
using Microsoft::WRL::FtmBase;
using Microsoft::WRL::Make;
using Microsoft::WRL::RuntimeClass;
using Microsoft::WRL::RuntimeClassFlags;
using Microsoft::WRL::ClassicCom;

namespace {

constexpr DWORD kSampleRate = 48'000;
constexpr WORD kChannels = 2;
constexpr WORD kBitsPerSample = 16;
constexpr std::uint64_t kMaximumDurationMs = 21'600'000;
constexpr std::uint64_t kMaximumWaveDataBytes =
    static_cast<std::uint64_t>(std::numeric_limits<DWORD>::max()) - 36;

constexpr DWORD FourCC(char first, char second, char third, char fourth) {
    return static_cast<DWORD>(static_cast<unsigned char>(first)) |
           (static_cast<DWORD>(static_cast<unsigned char>(second)) << 8) |
           (static_cast<DWORD>(static_cast<unsigned char>(third)) << 16) |
           (static_cast<DWORD>(static_cast<unsigned char>(fourth)) << 24);
}

struct Options {
    DWORD pid = 0;
    std::uint64_t expected_creation_time_100ns = 0;
    DWORD duration_ms = 0;
    std::wstring output;
    bool include_tree = false;
};

std::string HResultHex(HRESULT result) {
    std::ostringstream stream;
    stream << "0x" << std::uppercase << std::hex
           << static_cast<std::uint32_t>(result);
    return stream.str();
}

std::string Win32Message(DWORD error) {
    wchar_t* buffer = nullptr;
    const DWORD length = FormatMessageW(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
            FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr,
        error,
        0,
        reinterpret_cast<wchar_t*>(&buffer),
        0,
        nullptr);
    if (length == 0 || buffer == nullptr) {
        return "Win32 error " + std::to_string(error);
    }
    std::wstring wide(buffer, length);
    LocalFree(buffer);
    while (!wide.empty() && (wide.back() == L'\r' || wide.back() == L'\n')) {
        wide.pop_back();
    }
    const int needed = WideCharToMultiByte(
        CP_UTF8, 0, wide.c_str(), static_cast<int>(wide.size()), nullptr, 0,
        nullptr, nullptr);
    std::string utf8(static_cast<std::size_t>(std::max(needed, 0)), '\0');
    if (needed > 0) {
        WideCharToMultiByte(
            CP_UTF8, 0, wide.c_str(), static_cast<int>(wide.size()),
            utf8.data(), needed, nullptr, nullptr);
    }
    return utf8;
}

bool ParseUnsigned(std::wstring_view text, std::uint64_t maximum,
                   std::uint64_t* value) {
    if (text.empty()) {
        return false;
    }
    std::uint64_t parsed = 0;
    for (const wchar_t character : text) {
        if (character < L'0' || character > L'9') {
            return false;
        }
        const std::uint64_t digit = static_cast<std::uint64_t>(character - L'0');
        if (parsed > (maximum - digit) / 10) {
            return false;
        }
        parsed = parsed * 10 + digit;
    }
    if (parsed == 0) {
        return false;
    }
    *value = parsed;
    return true;
}

bool ParseOptions(int argc, wchar_t** argv, Options* options,
                  std::string* error) {
    bool saw_pid = false;
    bool saw_creation_time = false;
    bool saw_output = false;
    bool saw_duration = false;
    for (int index = 1; index < argc; ++index) {
        const std::wstring_view argument(argv[index]);
        if (argument == L"--include-tree") {
            if (options->include_tree) {
                *error = "--include-tree was provided more than once";
                return false;
            }
            options->include_tree = true;
            continue;
        }
        if (argument == L"--pid" ||
            argument == L"--expected-creation-time-100ns" ||
            argument == L"--output" ||
            argument == L"--duration-ms") {
            if (++index >= argc) {
                *error = "missing value after command-line option";
                return false;
            }
            if (argument == L"--pid") {
                std::uint64_t pid = 0;
                if (saw_pid ||
                    !ParseUnsigned(argv[index],
                                   std::numeric_limits<DWORD>::max(), &pid)) {
                    *error = "--pid must be one positive DWORD";
                    return false;
                }
                options->pid = static_cast<DWORD>(pid);
                saw_pid = true;
            } else if (argument == L"--expected-creation-time-100ns") {
                std::uint64_t creation_time = 0;
                if (saw_creation_time ||
                    !ParseUnsigned(
                        argv[index], std::numeric_limits<std::uint64_t>::max(),
                        &creation_time)) {
                    *error =
                        "--expected-creation-time-100ns must be one positive "
                        "64-bit integer";
                    return false;
                }
                options->expected_creation_time_100ns = creation_time;
                saw_creation_time = true;
            } else if (argument == L"--duration-ms") {
                std::uint64_t duration = 0;
                if (saw_duration ||
                    !ParseUnsigned(argv[index], kMaximumDurationMs, &duration)) {
                    *error = "--duration-ms must be between 1 and 21600000";
                    return false;
                }
                options->duration_ms = static_cast<DWORD>(duration);
                saw_duration = true;
            } else {
                if (saw_output || argv[index][0] == L'\0') {
                    *error = "--output must be provided exactly once";
                    return false;
                }
                options->output = argv[index];
                saw_output = true;
            }
            continue;
        }
        *error = "unknown command-line option";
        return false;
    }
    if (!saw_pid || !saw_creation_time || !saw_output || !saw_duration ||
        !options->include_tree) {
        *error =
            "required: --pid PID --expected-creation-time-100ns TICKS "
            "--include-tree --output FILE --duration-ms MS";
        return false;
    }
    return true;
}

class ActivationHandler final
    : public RuntimeClass<RuntimeClassFlags<ClassicCom>, FtmBase,
                          IActivateAudioInterfaceCompletionHandler> {
  public:
    explicit ActivationHandler(HANDLE completed_event)
        : completed_event_(completed_event) {}
    ~ActivationHandler() override {
        if (completed_event_ != nullptr) {
            CloseHandle(completed_event_);
        }
    }

    STDMETHODIMP ActivateCompleted(
        IActivateAudioInterfaceAsyncOperation* operation) override {
        ComPtr<IUnknown> activated;
        HRESULT activation_result = E_UNEXPECTED;
        const HRESULT operation_result =
            operation->GetActivateResult(&activation_result, &activated);
        result_ = FAILED(operation_result) ? operation_result : activation_result;
        if (SUCCEEDED(result_)) {
            result_ = activated.As(&audio_client_);
        }
        SetEvent(completed_event_);
        return S_OK;
    }

    HRESULT result() const { return result_; }
    ComPtr<IAudioClient> audio_client() const { return audio_client_; }
    HANDLE completed_event() const { return completed_event_; }

  private:
    HANDLE completed_event_ = nullptr;
    HRESULT result_ = E_PENDING;
    ComPtr<IAudioClient> audio_client_;
};

class WaveFile {
  public:
    ~WaveFile() {
        if (handle_ != INVALID_HANDLE_VALUE) {
            CloseHandle(handle_);
        }
    }

    bool Open(const std::wstring& path, const WAVEFORMATEX& format,
              std::string* error) {
        handle_ = CreateFileW(path.c_str(), GENERIC_WRITE, FILE_SHARE_READ,
                              nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL,
                              nullptr);
        if (handle_ == INVALID_HANDLE_VALUE) {
            *error = Win32Message(GetLastError());
            return false;
        }

        const DWORD riff = FourCC('R', 'I', 'F', 'F');
        const DWORD wave = FourCC('W', 'A', 'V', 'E');
        const DWORD fmt = FourCC('f', 'm', 't', ' ');
        const DWORD data = FourCC('d', 'a', 't', 'a');
        const DWORD zero = 0;
        const DWORD format_size = 16;
        return Write(&riff, sizeof(riff), error) &&
               Write(&zero, sizeof(zero), error) &&
               Write(&wave, sizeof(wave), error) &&
               Write(&fmt, sizeof(fmt), error) &&
               Write(&format_size, sizeof(format_size), error) &&
               Write(&format.wFormatTag, sizeof(format.wFormatTag), error) &&
               Write(&format.nChannels, sizeof(format.nChannels), error) &&
               Write(&format.nSamplesPerSec, sizeof(format.nSamplesPerSec),
                     error) &&
               Write(&format.nAvgBytesPerSec, sizeof(format.nAvgBytesPerSec),
                     error) &&
               Write(&format.nBlockAlign, sizeof(format.nBlockAlign), error) &&
               Write(&format.wBitsPerSample, sizeof(format.wBitsPerSample),
                     error) &&
               Write(&data, sizeof(data), error) &&
               Write(&zero, sizeof(zero), error);
    }

    bool WriteSamples(const BYTE* data, DWORD bytes, std::string* error) {
        if (data_bytes_ > kMaximumWaveDataBytes ||
            static_cast<std::uint64_t>(bytes) >
                kMaximumWaveDataBytes - data_bytes_) {
            *error = "WAV exceeded the RIFF 32-bit data-size limit";
            return false;
        }
        if (!Write(data, bytes, error)) {
            return false;
        }
        data_bytes_ += bytes;
        return true;
    }

    bool Finalize(std::string* error) {
        if (data_bytes_ > kMaximumWaveDataBytes) {
            *error = "WAV exceeded the RIFF 32-bit size limit";
            return false;
        }
        const DWORD data_size = static_cast<DWORD>(data_bytes_);
        const DWORD riff_size = data_size + 36;
        if (SetFilePointer(handle_, 4, nullptr, FILE_BEGIN) ==
            INVALID_SET_FILE_POINTER) {
            *error = Win32Message(GetLastError());
            return false;
        }
        if (!Write(&riff_size, sizeof(riff_size), error)) {
            return false;
        }
        if (SetFilePointer(handle_, 40, nullptr, FILE_BEGIN) ==
            INVALID_SET_FILE_POINTER) {
            *error = Win32Message(GetLastError());
            return false;
        }
        if (!Write(&data_size, sizeof(data_size), error)) {
            return false;
        }
        if (!FlushFileBuffers(handle_)) {
            *error = Win32Message(GetLastError());
            return false;
        }
        return true;
    }

    std::uint64_t data_bytes() const { return data_bytes_; }

  private:
    bool Write(const void* data, DWORD bytes, std::string* error) {
        DWORD written = 0;
        if (!WriteFile(handle_, data, bytes, &written, nullptr) ||
            written != bytes) {
            *error = Win32Message(GetLastError());
            return false;
        }
        return true;
    }

    HANDLE handle_ = INVALID_HANDLE_VALUE;
    std::uint64_t data_bytes_ = 0;
};

HRESULT ActivateProcessLoopback(DWORD pid, ComPtr<IAudioClient>* audio_client) {
    const HANDLE raw_event = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    if (raw_event == nullptr) {
        return HRESULT_FROM_WIN32(GetLastError());
    }

    auto handler = Make<ActivationHandler>(raw_event);
    if (!handler) {
        CloseHandle(raw_event);
        return E_OUTOFMEMORY;
    }

    AUDIOCLIENT_ACTIVATION_PARAMS parameters{};
    parameters.ActivationType = AUDIOCLIENT_ACTIVATION_TYPE_PROCESS_LOOPBACK;
    parameters.ProcessLoopbackParams.TargetProcessId = pid;
    parameters.ProcessLoopbackParams.ProcessLoopbackMode =
        PROCESS_LOOPBACK_MODE_INCLUDE_TARGET_PROCESS_TREE;

    PROPVARIANT activation_parameters{};
    activation_parameters.vt = VT_BLOB;
    activation_parameters.blob.cbSize = sizeof(parameters);
    activation_parameters.blob.pBlobData =
        reinterpret_cast<BYTE*>(&parameters);

    ComPtr<IActivateAudioInterfaceAsyncOperation> operation;
    HRESULT result = ActivateAudioInterfaceAsync(
        VIRTUAL_AUDIO_DEVICE_PROCESS_LOOPBACK, __uuidof(IAudioClient),
        &activation_parameters, handler.Get(), &operation);
    if (FAILED(result)) {
        return result;
    }
    const DWORD wait_result =
        WaitForSingleObject(handler->completed_event(), 15'000);
    if (wait_result != WAIT_OBJECT_0) {
        return wait_result == WAIT_TIMEOUT
                   ? HRESULT_FROM_WIN32(ERROR_TIMEOUT)
                   : HRESULT_FROM_WIN32(GetLastError());
    }
    result = handler->result();
    if (SUCCEEDED(result)) {
        *audio_client = handler->audio_client();
    }
    return result;
}

struct CaptureDiagnostics {
    std::uint64_t discontinuities = 0;
    std::uint64_t timestamp_errors = 0;
    std::uint64_t position_gaps = 0;
    std::uint64_t device_position_regressions = 0;
    bool has_previous_timeline = false;
    bool has_previous_device_position = false;
    UINT64 baseline_qpc_position = 0;
    UINT64 previous_qpc_position = 0;
    UINT64 last_qpc_position = 0;
    UINT64 previous_device_position = 0;
    UINT32 previous_packet_frames = 0;
    std::uint64_t client_frames_between_packet_starts = 0;

    double qpc_drift_ppm(DWORD sample_rate) const {
        if (!has_previous_timeline ||
            client_frames_between_packet_starts == 0 ||
            last_qpc_position < baseline_qpc_position) {
            return 0.0;
        }
        const double expected_100ns =
            static_cast<double>(client_frames_between_packet_starts) *
            10'000'000.0 / sample_rate;
        const double actual_100ns =
            static_cast<double>(last_qpc_position - baseline_qpc_position);
        return (actual_100ns - expected_100ns) / expected_100ns * 1'000'000.0;
    }
};

HRESULT DrainAudio(IAudioCaptureClient* capture_client,
                   const WAVEFORMATEX& format, WaveFile* output,
                   std::uint64_t* sample_frames,
                   CaptureDiagnostics* diagnostics,
                   std::string* output_error) {
    UINT32 packet_frames = 0;
    HRESULT result = capture_client->GetNextPacketSize(&packet_frames);
    while (SUCCEEDED(result) && packet_frames > 0) {
        BYTE* data = nullptr;
        DWORD flags = 0;
        UINT64 device_position = 0;
        UINT64 qpc_position = 0;
        result = capture_client->GetBuffer(
            &data, &packet_frames, &flags, &device_position, &qpc_position);
        if (FAILED(result)) {
            return result;
        }
        if ((flags & AUDCLNT_BUFFERFLAGS_DATA_DISCONTINUITY) != 0) {
            ++diagnostics->discontinuities;
        }
        if ((flags & AUDCLNT_BUFFERFLAGS_TIMESTAMP_ERROR) != 0) {
            ++diagnostics->timestamp_errors;
        }
        const bool timeline_qualified =
            (flags & (AUDCLNT_BUFFERFLAGS_DATA_DISCONTINUITY |
                      AUDCLNT_BUFFERFLAGS_TIMESTAMP_ERROR)) == 0;
        if (timeline_qualified && diagnostics->has_previous_timeline) {
            const UINT64 expected_delta =
                (static_cast<UINT64>(diagnostics->previous_packet_frames) *
                     10'000'000 +
                 format.nSamplesPerSec / 2) /
                format.nSamplesPerSec;
            const UINT64 actual_delta =
                qpc_position >= diagnostics->previous_qpc_position
                    ? qpc_position - diagnostics->previous_qpc_position
                    : 0;
            const UINT64 difference =
                actual_delta > expected_delta ? actual_delta - expected_delta
                                              : expected_delta - actual_delta;
            if (difference > 10'000) {
                ++diagnostics->position_gaps;
            }
            diagnostics->client_frames_between_packet_starts +=
                diagnostics->previous_packet_frames;
        }
        if (timeline_qualified &&
            diagnostics->has_previous_device_position &&
            device_position < diagnostics->previous_device_position) {
                ++diagnostics->device_position_regressions;
        }
        if (timeline_qualified) {
            if (!diagnostics->has_previous_timeline) {
                diagnostics->baseline_qpc_position = qpc_position;
                diagnostics->client_frames_between_packet_starts = 0;
            }
            diagnostics->has_previous_timeline = true;
            diagnostics->previous_qpc_position = qpc_position;
            diagnostics->last_qpc_position = qpc_position;
            diagnostics->previous_packet_frames = packet_frames;
            diagnostics->has_previous_device_position = true;
            diagnostics->previous_device_position = device_position;
        } else {
            diagnostics->has_previous_timeline = false;
            diagnostics->has_previous_device_position = false;
        }
        const DWORD bytes = packet_frames * format.nBlockAlign;
        bool write_ok = false;
        if ((flags & AUDCLNT_BUFFERFLAGS_SILENT) != 0 || data == nullptr) {
            std::vector<BYTE> silence(bytes, 0);
            write_ok = output->WriteSamples(silence.data(), bytes, output_error);
        } else {
            write_ok = output->WriteSamples(data, bytes, output_error);
        }
        const HRESULT release_result =
            capture_client->ReleaseBuffer(packet_frames);
        if (!write_ok) {
            return E_FAIL;
        }
        if (FAILED(release_result)) {
            return release_result;
        }
        *sample_frames += packet_frames;
        result = capture_client->GetNextPacketSize(&packet_frames);
    }
    return result;
}

int Run(const Options& options) {
    const HANDLE raw_process =
        OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | SYNCHRONIZE, FALSE,
                    options.pid);
    if (raw_process == nullptr) {
        std::cerr << "target process is not accessible: "
                  << Win32Message(GetLastError()) << '\n';
        return 3;
    }
    const std::unique_ptr<void, decltype(&CloseHandle)> process(
        raw_process, &CloseHandle);
    FILETIME creation_time{};
    FILETIME exit_time{};
    FILETIME kernel_time{};
    FILETIME user_time{};
    if (!GetProcessTimes(process.get(), &creation_time, &exit_time,
                         &kernel_time, &user_time)) {
        std::cerr << "target process identity query failed: "
                  << Win32Message(GetLastError()) << '\n';
        return 3;
    }
    const std::uint64_t actual_creation_time_100ns =
        (static_cast<std::uint64_t>(creation_time.dwHighDateTime) << 32) |
        creation_time.dwLowDateTime;
    if (actual_creation_time_100ns !=
        options.expected_creation_time_100ns) {
        std::cerr << "target process identity mismatch\n";
        return 3;
    }
    const DWORD initial_process_wait = WaitForSingleObject(process.get(), 0);
    if (initial_process_wait == WAIT_OBJECT_0) {
        std::cerr << "target process exited before capture activation\n";
        return 3;
    }
    if (initial_process_wait == WAIT_FAILED) {
        std::cerr << "target process wait failed: "
                  << Win32Message(GetLastError()) << '\n';
        return 3;
    }

    const HRESULT com_result =
        CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(com_result)) {
        std::cerr << "COM initialization failed: " << HResultHex(com_result)
                  << '\n';
        return 4;
    }
    const auto uninitialize_com =
        std::unique_ptr<void, void (*)(void*)>(
            reinterpret_cast<void*>(1), [](void*) { CoUninitialize(); });

    ComPtr<IAudioClient> audio_client;
    HRESULT result = ActivateProcessLoopback(options.pid, &audio_client);
    if (FAILED(result)) {
        std::cerr << "process-loopback activation failed: "
                  << HResultHex(result) << '\n';
        return 4;
    }

    WAVEFORMATEX format{};
    format.wFormatTag = WAVE_FORMAT_PCM;
    format.nChannels = kChannels;
    format.nSamplesPerSec = kSampleRate;
    format.wBitsPerSample = kBitsPerSample;
    format.nBlockAlign =
        format.nChannels * format.wBitsPerSample / 8;
    format.nAvgBytesPerSec =
        format.nSamplesPerSec * format.nBlockAlign;

    result = audio_client->Initialize(
        AUDCLNT_SHAREMODE_SHARED,
        AUDCLNT_STREAMFLAGS_LOOPBACK | AUDCLNT_STREAMFLAGS_EVENTCALLBACK |
            AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM |
            AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY,
        0, 0, &format, nullptr);
    if (FAILED(result)) {
        std::cerr << "audio client initialization failed: "
                  << HResultHex(result) << '\n';
        return 5;
    }

    ComPtr<IAudioCaptureClient> capture_client;
    result = audio_client->GetService(IID_PPV_ARGS(&capture_client));
    if (FAILED(result)) {
        std::cerr << "audio capture service failed: " << HResultHex(result)
                  << '\n';
        return 5;
    }

    const HANDLE raw_sample_event = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    if (raw_sample_event == nullptr) {
        std::cerr << "sample event creation failed: "
                  << Win32Message(GetLastError()) << '\n';
        return 5;
    }
    const std::unique_ptr<void, decltype(&CloseHandle)> sample_event(
        raw_sample_event, &CloseHandle);
    result = audio_client->SetEventHandle(sample_event.get());
    if (FAILED(result)) {
        std::cerr << "audio event registration failed: "
                  << HResultHex(result) << '\n';
        return 5;
    }

    WaveFile output;
    std::string output_error;
    if (!output.Open(options.output, format, &output_error)) {
        std::cerr << "WAV creation failed: " << output_error << '\n';
        return 7;
    }

    result = audio_client->Start();
    if (FAILED(result)) {
        std::cerr << "audio capture start failed: " << HResultHex(result)
                  << '\n';
        return 6;
    }

    const ULONGLONG started = GetTickCount64();
    std::uint64_t sample_frames = 0;
    CaptureDiagnostics diagnostics;
    while (true) {
        const DWORD process_wait = WaitForSingleObject(process.get(), 0);
        if (process_wait == WAIT_OBJECT_0) {
            std::cerr << "target process exited during capture\n";
            audio_client->Stop();
            return 3;
        }
        if (process_wait == WAIT_FAILED) {
            std::cerr << "target process wait failed: "
                      << Win32Message(GetLastError()) << '\n';
            audio_client->Stop();
            return 3;
        }
        const ULONGLONG elapsed = GetTickCount64() - started;
        if (elapsed >= options.duration_ms) {
            break;
        }
        const DWORD remaining =
            static_cast<DWORD>(options.duration_ms - elapsed);
        const HANDLE wait_handles[] = {sample_event.get(), process.get()};
        const DWORD wait_result =
            WaitForMultipleObjects(2, wait_handles, FALSE, remaining);
        if (wait_result == WAIT_TIMEOUT) {
            break;
        }
        if (wait_result == WAIT_OBJECT_0 + 1) {
            std::cerr << "target process exited during capture\n";
            audio_client->Stop();
            return 3;
        }
        if (wait_result != WAIT_OBJECT_0) {
            std::cerr << "audio sample wait failed: "
                      << Win32Message(GetLastError()) << '\n';
            audio_client->Stop();
            return 6;
        }
        result = DrainAudio(capture_client.Get(), format, &output,
                            &sample_frames, &diagnostics, &output_error);
        if (FAILED(result)) {
            std::cerr << "audio sample drain failed: " << HResultHex(result);
            if (!output_error.empty()) {
                std::cerr << ": " << output_error;
            }
            std::cerr << '\n';
            audio_client->Stop();
            return 6;
        }
    }

    result = audio_client->Stop();
    if (FAILED(result)) {
        std::cerr << "audio capture stop failed: " << HResultHex(result)
                  << '\n';
        return 6;
    }
    result = DrainAudio(capture_client.Get(), format, &output,
                        &sample_frames, &diagnostics, &output_error);
    if (FAILED(result)) {
        std::cerr << "final audio sample drain failed: " << HResultHex(result);
        if (!output_error.empty()) {
            std::cerr << ": " << output_error;
        }
        std::cerr << '\n';
        return 6;
    }
    if (!output.Finalize(&output_error)) {
        std::cerr << "WAV finalization failed: " << output_error << '\n';
        return 7;
    }

    std::cout << "{\"schema\":\"ai_studio.windows_process_loopback\","
              << "\"version\":2,\"status\":\"ok\",\"pid\":" << options.pid
              << ",\"targetCreationTime100ns\":"
              << actual_creation_time_100ns
              << ",\"durationMs\":" << options.duration_ms
              << ",\"sampleRate\":" << format.nSamplesPerSec
              << ",\"channels\":" << format.nChannels
              << ",\"bitsPerSample\":" << format.wBitsPerSample
              << ",\"sampleFrames\":" << sample_frames
              << ",\"dataBytes\":" << output.data_bytes()
              << ",\"discontinuities\":" << diagnostics.discontinuities
              << ",\"timestampErrors\":" << diagnostics.timestamp_errors
              << ",\"positionGaps\":" << diagnostics.position_gaps
              << ",\"devicePositionRegressions\":"
              << diagnostics.device_position_regressions
              << ",\"qpcDriftPpm\":"
              << diagnostics.qpc_drift_ppm(format.nSamplesPerSec) << "}\n";
    return 0;
}

}  // namespace

int wmain(int argc, wchar_t** argv) {
    Options options;
    std::string error;
    if (!ParseOptions(argc, argv, &options, &error)) {
        std::cerr << error << '\n';
        return 2;
    }
    return Run(options);
}
