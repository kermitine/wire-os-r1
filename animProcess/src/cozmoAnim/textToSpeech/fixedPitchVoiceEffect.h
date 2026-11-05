#ifndef __cozmo_textToSpeech_fixedPitchVoiceEffect_h
#define __cozmo_textToSpeech_fixedPitchVoiceEffect_h

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <vector>

namespace Anki {
namespace Vector {
namespace TextToSpeech {

// A channel vocoder: speech supplies the changing spectral envelopes, while
// a harmonic carrier supplies a steady pitch. No pitch tracker, text changes,
// lookahead, or changes to sample count are needed. Reset once per utterance.
class FixedPitchVoiceEffect
{
public:
  void Reset(int sampleRate, int pitch_hz)
  {
    _enabled = sampleRate >= 8000 && sampleRate <= 192000 && pitch_hz >= 60 && pitch_hz <= 140;
    _bands = {};
    _carriers.clear();
    _samplesUntilNormalize = 4096;
    if (!_enabled) {
      return;
    }

    const double upper = std::min(5000., sampleRate * 0.42);
    const double logStep = std::log(upper / 80.) / (_bands.size() - 1);
    // Two envelope poles suppress the source's vocal vibration while tracking
    // syllables. A single rectifier/envelope pole leaves audible source pitch.
    _envelopeStep = static_cast<float>(1. - std::exp(-kTwoPi * 25. / sampleRate));
    for (size_t i = 0; i < _bands.size(); ++i) {
      const double frequency = 80. * std::exp(i * logStep);
      const double bandwidth = std::max(100., frequency * logStep);
      const double omega = kTwoPi * frequency / sampleRate;
      const double alpha = std::sin(omega) * bandwidth / (2. * frequency);
      auto& band = _bands[i];
      // RBJ constant-peak bandpass, transposed direct form II.
      band.b0 = static_cast<float>(alpha / (1. + alpha));
      band.a1 = static_cast<float>(-2. * std::cos(omega) / (1. + alpha));
      band.a2 = static_cast<float>((1. - alpha) / (1. + alpha));
    }

    std::array<double, 20> carrierEnergy{};
    for (int harmonic = 1; harmonic * pitch_hz <= upper; ++harmonic) {
      const double frequency = harmonic * pitch_hz;
      const double position = std::max(0., std::min(static_cast<double>(_bands.size() - 1),
                                                   std::log(frequency / 80.) / logStep));
      Carrier carrier;
      carrier.lower = static_cast<size_t>(position);
      carrier.upper = std::min(carrier.lower + 1, _bands.size() - 1);
      carrier.upperWeight = static_cast<float>(position - carrier.lower);
      const double omega = kTwoPi * frequency / sampleRate;
      carrier.stepSin = std::sin(omega);
      carrier.stepCos = std::cos(omega);
      // Spread phases to avoid a large, simultaneous peak of every harmonic.
      const double phase = harmonic * harmonic * 2.399963229728653;
      carrier.sine = std::sin(phase);
      carrier.cosine = std::cos(phase);
      const double lowerWeight = 1. - carrier.upperWeight;
      carrierEnergy[carrier.lower] += lowerWeight * lowerWeight;
      carrierEnergy[carrier.upper] += carrier.upperWeight * carrier.upperWeight;
      _carriers.push_back(carrier);
    }
    for (size_t i = 0; i < _bands.size(); ++i) {
      _bands[i].gain = carrierEnergy[i] > 0. ? static_cast<float>(1. / std::sqrt(carrierEnergy[i])) : 0.f;
    }
  }

  void Process(short* samples, size_t numSamples)
  {
    if (!_enabled || samples == nullptr) {
      return;
    }
    for (size_t i = 0; i < numSamples; ++i) {
      const float input = samples[i] / 32768.f;
      for (auto& band : _bands) {
        const float filtered = band.b0 * input + band.z1;
        band.z1 = -band.a1 * filtered + band.z2;
        band.z2 = -band.b0 * input - band.a2 * filtered;
        band.envelope1 += _envelopeStep * (std::abs(filtered) - band.envelope1);
        band.envelope2 += _envelopeStep * (band.envelope1 - band.envelope2);
      }
      double output = 0.;
      for (auto& carrier : _carriers) {
        const auto& lower = _bands[carrier.lower];
        const auto& upper = _bands[carrier.upper];
        const double amplitude = (1. - carrier.upperWeight) * lower.envelope2 * lower.gain +
                                  carrier.upperWeight * upper.envelope2 * upper.gain;
        output += carrier.sine * amplitude;
        const double sine = carrier.sine * carrier.stepCos + carrier.cosine * carrier.stepSin;
        carrier.cosine = carrier.cosine * carrier.stepCos - carrier.sine * carrier.stepSin;
        carrier.sine = sine;
      }
      // Rectified sine envelopes have mean 2/pi; pi/2 restores peak amplitude.
      // Bound the intermediate PCM before the downstream metallic coloration.
      const double pcm = 0.9 * std::tanh(output * 1.5707963267948966) * 32768.;
      samples[i] = static_cast<short>(std::round(pcm));
      if (--_samplesUntilNormalize == 0) {
        for (auto& carrier : _carriers) {
          const double magnitude = std::hypot(carrier.sine, carrier.cosine);
          carrier.sine /= magnitude;
          carrier.cosine /= magnitude;
        }
        _samplesUntilNormalize = 4096;
      }
    }
  }

private:
  struct Band {
    float b0 = 0.f, a1 = 0.f, a2 = 0.f;
    float z1 = 0.f, z2 = 0.f;
    float envelope1 = 0.f, envelope2 = 0.f;
    float gain = 0.f;
  };
  struct Carrier {
    size_t lower = 0, upper = 0;
    float upperWeight = 0.f;
    double sine = 0., cosine = 1.;
    double stepSin = 0., stepCos = 1.;
  };
  static constexpr double kTwoPi = 6.2831853071795864769;
  bool _enabled = false;
  float _envelopeStep = 0.f;
  size_t _samplesUntilNormalize = 4096;
  std::array<Band, 20> _bands{};
  std::vector<Carrier> _carriers;
};

} // namespace TextToSpeech
} // namespace Vector
} // namespace Anki

#endif
