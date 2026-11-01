#ifndef __cozmo_textToSpeech_tacticalDroidVoiceEffect_h
#define __cozmo_textToSpeech_tacticalDroidVoiceEffect_h

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

namespace Anki {
namespace Vector {
namespace TextToSpeech {

// A restrained tactical-droid coloration for mono, signed 16-bit TTS audio.
// Keep state across synthesis chunks and reset once per utterance. This does
// not resample audio, change its duration, or synthesize a replacement voice.
class TacticalDroidVoiceEffect
{
public:
  void Reset(int sampleRate, bool enabled)
  {
    _enabled = enabled && sampleRate >= 8000 && sampleRate <= 192000;
    _previousInput = _highPass = _lowPass1 = _lowPass2 = 0.f;
    _phase = 0.f;
    _delayIndex = 0;
    _delay.clear();
    if (!_enabled) {
      return;
    }

    // Retain body and consonants while taking the edge off the small speaker.
    _highPassCoefficient = std::exp(-kTwoPi * 140.f / sampleRate);
    _lowPassCoefficient = 1.f - std::exp(-kTwoPi * 3600.f / sampleRate);
    _phaseStep = kTwoPi * 65.f / sampleRate;
    _delay.assign(std::max<size_t>(1, static_cast<size_t>(sampleRate * 0.006f)), 0.f);
  }

  void Process(short* samples, size_t numSamples)
  {
    if (!_enabled || samples == nullptr) {
      return;
    }

    for (size_t i = 0; i < numSamples; ++i) {
      const float input = samples[i] / 32768.f;
      _highPass = _highPassCoefficient * (_highPass + input - _previousInput);
      _previousInput = input;
      _lowPass1 += _lowPassCoefficient * (_highPass - _lowPass1);
      _lowPass2 += _lowPassCoefficient * (_lowPass1 - _lowPass2);

      // A short, feed-forward comb adds metal without an audible echo or a
      // feedback tail. Partial ring modulation keeps the words intelligible.
      const float resonant = 0.84f * _lowPass2 + 0.16f * _delay[_delayIndex];
      _delay[_delayIndex] = _lowPass2;
      _delayIndex = (_delayIndex + 1) % _delay.size();
      const float metallic = resonant * (0.78f + 0.22f * std::cos(_phase));
      _phase += _phaseStep;
      if (_phase >= kTwoPi) {
        _phase -= kTwoPi;
      }

      // Gentle saturation adds texture with output headroom. Clamp before
      // converting back to PCM, even for full-scale input and transients.
      const float output = 0.82f * std::tanh(1.4f * metallic);
      const float pcm = std::max(-32768.f, std::min(32767.f, output * 32768.f));
      samples[i] = static_cast<short>(std::round(pcm));
    }
  }

private:
  static constexpr float kTwoPi = 6.2831853071795864769f;
  bool _enabled = false;
  float _highPassCoefficient = 0.f;
  float _lowPassCoefficient = 0.f;
  float _previousInput = 0.f;
  float _highPass = 0.f;
  float _lowPass1 = 0.f;
  float _lowPass2 = 0.f;
  float _phase = 0.f;
  float _phaseStep = 0.f;
  std::vector<float> _delay;
  size_t _delayIndex = 0;
};

} // namespace TextToSpeech
} // namespace Vector
} // namespace Anki

#endif
