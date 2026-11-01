// Standalone regression checks; no robot SDK or audio device is required.
// See docs/development/calm-voice.md for build commands.
#include "cozmoAnim/textToSpeech/tacticalDroidVoiceEffect.h"
#include "cozmoAnim/textToSpeech/speechProsody.h"

#include <iostream>
#include <stdexcept>

using Anki::Vector::TextToSpeech::TacticalDroidVoiceEffect;
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
} // namespace

int main()
{
  try {
    CheckBypass();
    CheckSilenceAndHeadroom();
    CheckChunkContinuityAndReset();
    CheckSpeechBandAndMetallicSidebands();
    CheckWordStress();
    std::cout << "PASS: bypass, silence, headroom, chunk continuity, reset, speech band, metallic sidebands, word stress\n";
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "FAIL: " << error.what() << '\n';
    return 1;
  }
}
