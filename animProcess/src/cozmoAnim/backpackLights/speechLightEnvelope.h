#ifndef __cozmoAnim_speechLightEnvelope_h
#define __cozmoAnim_speechLightEnvelope_h

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <mutex>

namespace Anki {
namespace Vector {
namespace Anim {

// Analyze the final mono PCM before queuing it for playback. Look up the envelope
// with the audio player's frame counter, so synthesis/streaming speed cannot move
// the lights ahead of the voice. Only the small envelope is retained, not PCM.
class SpeechLightEnvelope
{
public:
  void Append(const float* samples, size_t count, uint32_t sampleRate)
  {
    if (samples == nullptr || count == 0 || sampleRate == 0) {
      return;
    }
    std::lock_guard<std::mutex> lock(_mutex);
    if (_windowFrames == 0) {
      _windowFrames = std::max(uint32_t{1}, sampleRate / 100); // 10 ms
    }
    for (size_t i = 0; i < count; ++i) {
      const double sample = samples[i];
      _sumSquares += sample * sample;
      if (++_pendingFrames == _windowFrames) {
        _releaseLevel = std::max(Level(_sumSquares, _pendingFrames), _releaseLevel * 0.75f);
        _levels.push_back(_releaseLevel);
        _sumSquares = 0.;
        _pendingFrames = 0;
      }
    }
  }

  // Called on the animation thread with the number of frames consumed by audio.
  // Drop old windows as playback advances, keeping long SDK streams bounded.
  float GetLevel(uint32_t framesPlayed)
  {
    std::lock_guard<std::mutex> lock(_mutex);
    if (framesPlayed == 0 || _windowFrames == 0) {
      return 0.f;
    }
    const uint64_t window = (framesPlayed - 1) / _windowFrames;
    while (!_levels.empty() && _firstWindow < window) {
      _levels.pop_front();
      ++_firstWindow;
    }
    if (window < _firstWindow) {
      return 0.f;
    }
    if (!_levels.empty()) {
      return _levels.front();
    }
    if (window == _firstWindow && _pendingFrames > 0) {
      return std::max(Level(_sumSquares, _pendingFrames), _releaseLevel * 0.75f);
    }
    return 0.f;
  }

  // Center LED responds first; the other two fill in on stronger syllables.
  // Colors use the robot's 0xRRGGBB00 format.
  static std::array<uint32_t, 3> Colors(float level)
  {
    const float thresholds[] = {0.25f, 0.f, 0.55f};
    std::array<uint32_t, 3> colors{};
    for (size_t i = 0; i < colors.size(); ++i) {
      const float brightness = std::max(0.f, std::min(1.f, (level - thresholds[i]) / 0.45f));
      const auto red = static_cast<uint32_t>(255.f * brightness);
      const auto green = static_cast<uint32_t>(80.f * brightness);
      colors[i] = (red << 24) | (green << 16);
    }
    return colors;
  }

private:
  static float Level(double sumSquares, uint32_t frames)
  {
    // Suppress the noise floor, then bring ordinary speech into the useful LED
    // range. Fast attack and a short release follow syllables without flicker.
    const float rms = static_cast<float>(std::sqrt(sumSquares / frames));
    return std::max(0.f, std::min(1.f, (rms - 0.008f) / 0.172f));
  }

  std::mutex _mutex;
  std::deque<float> _levels;
  uint64_t _firstWindow = 0;
  uint32_t _windowFrames = 0;
  uint32_t _pendingFrames = 0;
  double _sumSquares = 0.;
  float _releaseLevel = 0.f;
};

// Delay observed playback levels by the speaker's output-buffer latency. This
// queue also keeps the final light changes alive after Wwise completes an event.
// Used only on the animation thread.
class SpeechLightPlayback
{
public:
  void Push(float level, uint32_t now_ms, uint32_t latency_ms)
  {
    _pending.push_back({now_ms + latency_ms, level, true});
  }

  void Finish(uint32_t now_ms, uint32_t latency_ms)
  {
    _finished = true;
    _pending.push_back({now_ms + latency_ms, 0.f, false});
  }

  void Update(uint32_t now_ms)
  {
    while (!_pending.empty() && static_cast<int32_t>(now_ms - _pending.front().time_ms) >= 0) {
      const auto& frame = _pending.front();
      _level = frame.level;
      _playing = frame.playing;
      _lastOutput_ms = frame.time_ms;
      _pending.pop_front();
    }
    // Do not leave a bright syllable stuck on during a stream underrun.
    if (now_ms - _lastOutput_ms >= 100) {
      _playing = false;
    }
  }

  bool IsPlaying() const { return _playing; }
  float GetLevel() const { return _playing ? _level : 0.f; }
  bool IsDone() const { return _finished && _pending.empty() && !_playing; }

private:
  struct Frame {
    uint32_t time_ms;
    float level;
    bool playing;
  };
  std::deque<Frame> _pending;
  uint32_t _lastOutput_ms = 0;
  float _level = 0.f;
  bool _playing = false;
  bool _finished = false;
};

} // namespace Anim
} // namespace Vector
} // namespace Anki

#endif
