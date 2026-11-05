// Standalone regression checks; no robot SDK or audio device is required.
// See docs/development/calm-voice.md for build commands.
#include "cozmoAnim/textToSpeech/tacticalDroidVoiceEffect.h"
#include "cozmoAnim/textToSpeech/speechProsody.h"

#include <iostream>
#include <stdexcept>

using Anki::Vector::TextToSpeech::TacticalDroidVoiceEffect;
using Anki::Vector::TextToSpeech::FixedPitchVoiceEffect;
using Anki::Vector::TextToSpeech::WithNeutralSentenceEnding;
using Anki::Vector::TextToSpeech::WithReducedWordEmphasis;
using Samples = std::vector<short>;

namespace {
constexpr double kTwoPi = 6.2831853071795864769;

void Require(bool condition, const char* message)
{
  if (!condition) {
    throw std::runtime_error(message);
  }
}

Samples Tone(int sampleRate, double frequency, double amplitude = 10000.)
{
  Samples samples(sampleRate);
  for (size_t i = 0; i < samples.size(); ++i) {
    samples[i] = static_cast<short>(std::round(amplitude * std::sin(kTwoPi * frequency * i / sampleRate)));
  }
  return samples;
}

double Rms(const Samples& samples)
{
  double energy = 0.;
  const size_t begin = samples.size() / 4; // Skip the startup transient.
  for (size_t i = begin; i < samples.size(); ++i) {
    energy += static_cast<double>(samples[i]) * samples[i];
  }
  return std::sqrt(energy / (samples.size() - begin));
}

double SpectralMagnitude(const Samples& samples, int sampleRate, double frequency)
{
  double real = 0.;
  double imaginary = 0.;
  const size_t begin = sampleRate / 2;
  for (size_t i = begin; i < samples.size(); ++i) {
    const double phase = kTwoPi * frequency * i / sampleRate;
    real += samples[i] * std::cos(phase);
    imaginary -= samples[i] * std::sin(phase);
  }
  return std::sqrt(real * real + imaginary * imaginary);
}

void CheckBypass()
{
  const Samples original = {-32768, -20000, -1, 0, 1, 20000, 32767};
  TacticalDroidVoiceEffect effect;
  auto samples = original;
  effect.Process(samples.data(), samples.size());
  Require(samples == original, "Default effect must pass through PCM exactly");
  for (const int rate : {0, -1, 7999, 192001, 22050}) {
    samples = original;
    effect.Reset(rate, rate != 22050);
    effect.Process(samples.data(), samples.size());
    effect.Process(nullptr, 0);
    Require(samples == original, "Disabled or invalid-rate effect must pass through PCM exactly");
  }
}

void CheckSilenceAndHeadroom()
{
  for (const int rate : {8000, 22050, 44100, 48000, 192000}) {
    TacticalDroidVoiceEffect effect;
    effect.Reset(rate, true);
    Samples silence(rate, 0);
    effect.Process(nullptr, 100);
    effect.Process(silence.data(), 0);
    effect.Process(silence.data(), silence.size());
    Require(silence == Samples(rate, 0), "Effect must not generate sound from silence");

    Samples fullScale(rate);
    for (size_t i = 0; i < fullScale.size(); ++i) {
      // Mix sustained steps with alternating extremes to exercise transients.
      fullScale[i] = ((i / 101 + i % 2) % 2 == 0) ? -32768 : 32767;
    }
    effect.Process(fullScale.data(), fullScale.size());
    for (const short sample : fullScale) {
      Require(std::abs(static_cast<int>(sample)) <= 26870,
              "Effect must retain headroom for full-scale input at every supported rate");
    }
  }
}

void CheckChunkContinuityAndReset()
{
  auto original = Tone(22050, 233., 24000.);
  for (size_t i = 0; i < original.size(); ++i) {
    if (i % 4096 < 200) {
      original[i] = 0; // Include quiet spans, like word breaks.
    }
  }
  TacticalDroidVoiceEffect effect;
  effect.Reset(22050, true);
  auto whole = original;
  effect.Process(whole.data(), whole.size());
  Require(whole != original, "Enabled effect must color voiced audio");

  for (const size_t chunkSize : {size_t(1), size_t(17), size_t(2048)}) {
    effect.Reset(22050, true);
    auto chunked = original;
    for (size_t i = 0; i < chunked.size(); i += chunkSize) {
      effect.Process(chunked.data() + i, std::min(chunkSize, chunked.size() - i));
    }
    Require(chunked == whole, "Synthesis chunk boundaries must not change the output");
  }

  effect.Reset(48000, true);
  auto differentRate = Tone(48000, 500.);
  effect.Process(differentRate.data(), differentRate.size());
  effect.Reset(22050, true);
  auto restarted = original;
  effect.Process(restarted.data(), restarted.size());
  Require(restarted == whole, "New utterances must not inherit filter or carrier state");

  effect.Reset(22050, false);
  auto bypassed = original;
  effect.Process(bypassed.data(), bypassed.size());
  Require(bypassed == original, "Disabling an active effect must restore exact passthrough");
}

void CheckSpeechBandAndMetallicSidebands()
{
  TacticalDroidVoiceEffect effect;
  auto bass = Tone(22050, 30.);
  auto voice = Tone(22050, 1000.);
  auto treble = Tone(22050, 9000.);
  for (auto* signal : {&bass, &voice, &treble}) {
    effect.Reset(22050, true);
    effect.Process(signal->data(), signal->size());
  }
  const double voiceRms = Rms(voice);
  Require(voiceRms > 3000., "Speech frequencies must remain audible");
  Require(Rms(bass) < voiceRms * 0.4, "Sub-bass must be attenuated");
  Require(Rms(treble) < voiceRms * 0.4, "Harsh high frequencies must be attenuated");
  const double carrier = SpectralMagnitude(voice, 22050, 1000.);
  Require(SpectralMagnitude(voice, 22050, 935.) > carrier * 0.07 &&
          SpectralMagnitude(voice, 22050, 1065.) > carrier * 0.07,
          "Metallic modulation must produce sidebands while retaining the speech carrier");
}

Samples HarmonicVoice(int sampleRate, double pitch)
{
  Samples samples(sampleRate);
  for (size_t i = 0; i < samples.size(); ++i) {
    double value = 0.;
    for (int harmonic = 1; harmonic <= 15; ++harmonic) {
      value += std::sin(kTwoPi * pitch * harmonic * i / sampleRate) / harmonic;
    }
    samples[i] = static_cast<short>(std::round(9000. * value));
  }
  return samples;
}

void CheckFixedPitch()
{
  constexpr int sampleRate = 22050;
  // Model a low syllable followed by a higher one, like the reported upspeak.
  // Check both the vocoder and its actual downstream metallic processing.
  for (const int metallicMix : {-1, 0, 100}) {
    for (const double sourcePitch : {120., 210.}) {
      auto samples = HarmonicVoice(sampleRate, sourcePitch);
      if (metallicMix >= 0) {
        TacticalDroidVoiceEffect effect;
        effect.Reset(sampleRate, true, 85, metallicMix);
        effect.Process(samples.data(), samples.size());
      } else {
        FixedPitchVoiceEffect effect;
        effect.Reset(sampleRate, 85);
        effect.Process(samples.data(), samples.size());
      }
      double carrier = 0.;
      for (int harmonic = 1; harmonic <= 8; ++harmonic) {
        carrier = std::max(carrier, SpectralMagnitude(samples, sampleRate, harmonic * 85.));
      }
      Require(Rms(samples) > 500., "Fixed-pitch speech must retain useful output level");
      Require(SpectralMagnitude(samples, sampleRate, sourcePitch) < carrier * 0.05,
              "Source pitch must be replaced, including after metallic processing");
      // A different target must move the carrier; this is not a hard-coded EQ.
      FixedPitchVoiceEffect otherTarget;
      otherTarget.Reset(sampleRate, 100);
      samples = HarmonicVoice(sampleRate, sourcePitch);
      otherTarget.Process(samples.data(), samples.size());
      Require(SpectralMagnitude(samples, sampleRate, 100.) >
              SpectralMagnitude(samples, sampleRate, 85.) * 5.,
              "Configured pitch must set the acoustic carrier frequency");
    }
  }
}

void CheckFixedPitchStreaming(int metallicMix)
{
  const auto original = HarmonicVoice(22050, 173.);
  TacticalDroidVoiceEffect effect;
  effect.Reset(22050, true, 85, metallicMix);
  auto whole = original;
  effect.Process(whole.data(), whole.size());
  for (const size_t chunkSize : {size_t(1), size_t(17), size_t(2048)}) {
    effect.Reset(22050, true, 85, metallicMix);
    auto chunked = original;
    for (size_t i = 0; i < chunked.size(); i += chunkSize) {
      effect.Process(chunked.data() + i, std::min(chunkSize, chunked.size() - i));
    }
    Require(chunked == whole, "Fixed-pitch output must be independent of synthesis chunk boundaries");
  }
  effect.Reset(48000, true, 100, metallicMix);
  auto otherRate = Tone(48000, 300.);
  effect.Process(otherRate.data(), otherRate.size());
  effect.Reset(22050, true, 85, metallicMix);
  auto restarted = original;
  effect.Process(restarted.data(), restarted.size());
  Require(restarted == whole, "Reset must clear vocoder envelopes and oscillators");
  effect.Reset(22050, false, 85, metallicMix);
  auto bypassed = original;
  effect.Process(bypassed.data(), bypassed.size());
  Require(bypassed == original, "Disabling the metallic effect must also bypass the vocoder exactly");

  FixedPitchVoiceEffect vocoder;
  for (const int pitch : {0, -1, 59, 141}) {
    vocoder.Reset(22050, pitch);
    bypassed = original;
    vocoder.Process(bypassed.data(), bypassed.size());
    Require(bypassed == original, "Absent or invalid robotic pitch must bypass the vocoder exactly");
  }
  for (const int rate : {0, 7999, 192001}) {
    vocoder.Reset(rate, 85);
    bypassed = original;
    vocoder.Process(bypassed.data(), bypassed.size());
    Require(bypassed == original, "Unsupported sample rate must bypass the vocoder exactly");
  }
  for (const int rate : {8000, 22050, 48000, 192000}) {
    for (const int pitch : {60, 85, 140}) {
      effect.Reset(rate, true, pitch, metallicMix);
      Samples silence(rate, 0);
      effect.Process(nullptr, 100);
      effect.Process(silence.data(), 0);
      effect.Process(silence.data(), silence.size());
      Require(silence == Samples(rate, 0), "Fixed carrier must not generate speech from silence");
      auto loud = Tone(rate, 233., 32767.);
      for (size_t i = 0; i < loud.size(); i += 13) {
        loud[i] = i % 2 ? -32768 : 32767;
      }
      effect.Process(loud.data(), loud.size());
      for (const short sample : loud) {
        Require(std::abs(static_cast<int>(sample)) <= (metallicMix == 100 ? 26870 : 29490),
                "Vocoder and metallic effect must retain PCM headroom");
      }
      effect.Process(silence.data(), silence.size());
      for (size_t i = silence.size() / 2; i < silence.size(); ++i) {
        Require(std::abs(static_cast<int>(silence[i])) <= 1,
                "Carrier must decay when speech stops rather than drone through pauses");
      }
    }
  }
}

void CheckSpeechDetail()
{
  constexpr int sampleRate = 22050;
  TacticalDroidVoiceEffect effect;
  for (const double frequency : {3100., 6000.}) {
    const auto original = Tone(sampleRate, frequency);
    auto clean = original;
    effect.Reset(sampleRate, true, 85, 0);
    effect.Process(clean.data(), clean.size());
    const double originalDetail = SpectralMagnitude(original, sampleRate, frequency);
    const double cleanDetail = SpectralMagnitude(clean, sampleRate, frequency);
    Require(cleanDetail > originalDetail * 0.65,
            "Clarity mode must retain original high-frequency detail rather than vocode consonants");
    if (frequency == 3100.) {
      auto metallic = original;
      effect.Reset(sampleRate, true, 85, 100);
      effect.Process(metallic.data(), metallic.size());
      const double cleanSideband = SpectralMagnitude(clean, sampleRate, frequency - 85.);
      Require(cleanSideband < cleanDetail * 0.02 &&
              cleanSideband < SpectralMagnitude(metallic, sampleRate, frequency - 85.) * 0.3,
              "Zero metallic mix must remove the extra ring-modulator sidebands");
    }
  }
  // With neither vocoding nor metal, do not filter or attenuate the signal.
  const auto original = Tone(sampleRate, 173.);
  auto clean = original;
  effect.Reset(sampleRate, true, 0, 0);
  effect.Process(clean.data(), clean.size());
  Require(clean == original, "Zero pitch and zero metal must give exact PCM passthrough");
  auto negativeMix = original;
  effect.Reset(sampleRate, true, 0, -1);
  effect.Process(negativeMix.data(), negativeMix.size());
  Require(negativeMix == original, "Negative metallic mix must clamp to zero");
  auto fullMix = original;
  effect.Reset(sampleRate, true, 0, 100);
  effect.Process(fullMix.data(), fullMix.size());
  auto excessiveMix = original;
  effect.Reset(sampleRate, true, 0, 101);
  effect.Process(excessiveMix.data(), excessiveMix.size());
  Require(excessiveMix == fullMix, "Excessive metallic mix must clamp to 100 percent");
}

void CheckWordStress()
{
  Require(WithReducedWordEmphasis(WithNeutralSentenceEnding("quarter")) == "\\emph=0\\quarter.",
          "Standalone quarter must be de-emphasized and end as a statement");
  Require(WithReducedWordEmphasis(WithNeutralSentenceEnding("A quarter past three!")) ==
          "\\emph=0\\A \\emph=0\\quarter \\emph=0\\past \\emph=0\\three.",
          "De-emphasis must also apply to quarter and other words inside sentences");
  Require(WithReducedWordEmphasis(WithNeutralSentenceEnding("Would you like another quarter?")) ==
          "\\emph=0\\Would \\emph=0\\you \\emph=0\\like \\emph=0\\another \\emph=0\\quarter?",
          "Reduced word stress must preserve question punctuation");
  Require(WithReducedWordEmphasis("  Don't\tchange $72.50 at 3:30\nPM.  ") ==
          "  \\emph=0\\Don't\t\\emph=0\\change \\emph=0\\$72.50 \\emph=0\\at \\emph=0\\3:30\n\\emph=0\\PM.  ",
          "Adding tags must preserve contractions, numeric tokens, and whitespace");
  Require(WithReducedWordEmphasis(u8"\u201cCaf\u00e9 is open.\u201d") ==
          u8"\\emph=0\\\u201cCaf\u00e9 \\emph=0\\is \\emph=0\\open.\u201d",
          "Tagging must preserve UTF-8 names and quotes");
  for (const std::string text : {"", " \t\n", "...", "?!", u8"\u2026"}) {
    Require(WithReducedWordEmphasis(text) == text, "Empty and punctuation-only text must stay intact");
  }
  for (const std::string text : {"\\pit=90\\Ready", "<speak>Ready!</speak>", "x > y",
                                 "\\emph=1\\Ready!", "C:\\robot", "1 < 2"}) {
    Require(WithReducedWordEmphasis(WithNeutralSentenceEnding(text)) == text,
            "Caller markup must own its delivery and bypass automatic text changes");
  }
}

void CheckMinutePronunciationContext()
{
  Require(WithReducedWordEmphasis("One minute.") == "\\emph=0\\One minute.",
          "Time-unit minute must remain adjacent to its quantity without an inserted tag");
  Require(WithReducedWordEmphasis("Your one minute timer is ready.") ==
          "\\emph=0\\Your \\emph=0\\one minute timer \\emph=0\\is \\emph=0\\ready.",
          "Keep minute's context together and resume reduced stress afterward");
  Require(WithReducedWordEmphasis("A minute detail matters.") ==
          "\\emph=0\\A minute detail \\emph=0\\matters.",
          "Adjective minute must retain its following noun without forced respelling");
  Require(WithReducedWordEmphasis("Minute details matter.") == "Minute details \\emph=0\\matter.",
          "Sentence-initial minute must also retain its context");
  Require(WithReducedWordEmphasis(WithNeutralSentenceEnding("minute")) == "minute.",
          "Standalone minute must keep native word stress");
  Require(WithReducedWordEmphasis("Wait 1 MINUTE, please.") == "\\emph=0\\Wait \\emph=0\\1 MINUTE, please.",
          "Minute context protection must handle case and punctuation");
  Require(WithReducedWordEmphasis("Two minutes remaining.") == "\\emph=0\\Two minutes remaining.",
          "Plural minutes must retain quantity and following context");
  Require(WithReducedWordEmphasis("A one-minute timer.") == "\\emph=0\\A one-minute timer.",
          "Hyphenated minute compounds must preserve native stress");
  Require(WithReducedWordEmphasis("Minuteman and minutely.") ==
          "\\emph=0\\Minuteman \\emph=0\\and \\emph=0\\minutely.",
          "The minute exception must not match longer words");
}
} // namespace

int main()
{
  try {
    CheckBypass();
    CheckSilenceAndHeadroom();
    CheckChunkContinuityAndReset();
    CheckSpeechBandAndMetallicSidebands();
    CheckFixedPitch();
    for (const int metallicMix : {0, 25, 100}) {
      CheckFixedPitchStreaming(metallicMix);
    }
    CheckSpeechDetail();
    CheckWordStress();
    CheckMinutePronunciationContext();
    std::cout << "PASS: bypass, silence, headroom, chunk continuity, reset, speech band, metallic sidebands, fixed pitch, vocoder streaming, consonant detail, metallic mix, word stress, minute context\n";
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "FAIL: " << error.what() << '\n';
    return 1;
  }
}
