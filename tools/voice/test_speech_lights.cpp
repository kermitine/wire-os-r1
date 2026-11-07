#include "cozmoAnim/backpackLights/speechLightEnvelope.h"

#include <atomic>
#include <cstdlib>
#include <iostream>
#include <thread>
#include <vector>

using Anki::Vector::Anim::SpeechLightEnvelope;
using Anki::Vector::Anim::SpeechLightPlayback;

static void Require(bool condition, const char* message)
{
  if (!condition) {
    std::cerr << "FAIL: " << message << '\n';
    std::exit(1);
  }
}

static std::vector<float> Speech(uint32_t rate)
{
  std::vector<float> samples(rate, 0.f);
  for (size_t i = rate / 5; i < rate * 2 / 5; ++i) {
    samples[i] = static_cast<float>(0.1 * std::sin(6.28318530718 * 170. * i / rate));
  }
  for (size_t i = rate * 3 / 5; i < rate * 4 / 5; ++i) {
    samples[i] = static_cast<float>(0.5 * std::sin(6.28318530718 * 85. * i / rate));
  }
  return samples;
}

static void CheckSpeech(uint32_t rate)
{
  const auto samples = Speech(rate);
  SpeechLightEnvelope envelope;
  envelope.Append(samples.data(), samples.size(), rate);
  Require(envelope.GetLevel(0) == 0.f, "Prepared audio must not light up before playback");
  Require(envelope.GetLevel(rate / 10) == 0.f, "Leading silence stays dark even with future speech prepared");
  const float quiet = envelope.GetLevel(rate * 3 / 10);
  Require(quiet > 0.08f && quiet < 0.25f, "Quiet syllables remain visible at the low end of the meter");
  Require(envelope.GetLevel(rate * 59 / 100) < 0.01f, "Lights decay during a pause");
  Require(envelope.GetLevel(rate * 7 / 10) > quiet, "Loud syllables light more LEDs");
  Require(envelope.GetLevel(rate) < 0.01f, "Trailing silence fades to dark");

  // SDK chunks, Acapela chunks, and arbitrary transport boundaries must all
  // yield the same envelope at the same playback position.
  for (const size_t chunkSize : {size_t{1}, size_t{137}, size_t{1024}}) {
    SpeechLightEnvelope whole, chunked;
    whole.Append(samples.data(), samples.size(), rate);
    for (size_t i = 0; i < samples.size(); i += chunkSize) {
      chunked.Append(samples.data() + i, std::min(chunkSize, samples.size() - i), rate);
    }
    for (uint32_t frame = 1; frame <= rate; frame += 13) {
      Require(whole.GetLevel(frame) == chunked.GetLevel(frame), "Envelope must be independent of chunk boundaries");
    }
  }

  // Slow network delivery must not cause a burst of synthesized-ahead lights.
  SpeechLightEnvelope streaming, reference;
  reference.Append(samples.data(), samples.size(), rate);
  for (size_t i = 0; i < samples.size(); i += 137) {
    const size_t count = std::min(size_t{137}, samples.size() - i);
    streaming.Append(samples.data() + i, count, rate);
    const auto completeWindow = static_cast<uint32_t>((i + count) / (rate / 100) * (rate / 100));
    if (completeWindow > 0) {
      Require(streaming.GetLevel(completeWindow) == reference.GetLevel(completeWindow),
              "Streaming and prepared speech must agree at the playback cursor");
    }
  }
}

int main()
{
  for (const uint32_t rate : {8000u, 16000u, 16025u, 22050u, 48000u}) {
    CheckSpeech(rate);
  }

  SpeechLightEnvelope silence;
  const std::vector<float> zeros(16000, 0.f);
  silence.Append(nullptr, 0, 0);
  silence.Append(zeros.data(), zeros.size(), 16000);
  Require(silence.GetLevel(16000) == 0.f, "Digital silence never lights up");

  SpeechLightEnvelope partial;
  const std::vector<float> shortChunk(27, 0.15f);
  partial.Append(shortChunk.data(), shortChunk.size(), 16000);
  Require(partial.GetLevel(27) > 0.25f, "A final partial window must remain visible");

  Require(SpeechLightEnvelope::Colors(0.f) == std::array<uint32_t, 3>{}, "No speech turns all LEDs off");
  const auto quiet = SpeechLightEnvelope::Colors(0.2f);
  Require(quiet[0] == 0 && quiet[1] != 0 && quiet[2] == 0, "Center LED responds to quiet speech");
  Require((quiet[1] >> 24) == ((quiet[1] >> 16) & 0xff) &&
          (quiet[1] >> 24) == ((quiet[1] >> 8) & 0xff), "Quiet speech is white");
  const auto medium = SpeechLightEnvelope::Colors(0.5f);
  Require(medium[0] != 0 && medium[1] != 0 && medium[2] == 0, "Medium speech fills two LEDs");
  Require((medium[1] >> 24) > ((medium[1] >> 16) & 0xff) &&
          ((medium[1] >> 16) & 0xff) == ((medium[1] >> 8) & 0xff) &&
          ((medium[1] >> 8) & 0xff) > 0, "Medium speech blends white into red through pink");
  const auto loud = SpeechLightEnvelope::Colors(1.f);
  Require(loud[0] == 0xff000000 && loud[1] == loud[0] && loud[2] == loud[0],
          "Loud speech fills the three red LEDs in robot color format");

  // Feed real PCM through the envelope as well as the color mapping: ordinary
  // syllables used to saturate at 0.18 RMS and should now stay well below red.
  const auto levelAtRms = [](float rms) {
    std::vector<float> tone(1600);
    for (size_t i = 0; i < tone.size(); ++i) {
      tone[i] = static_cast<float>(rms * std::sqrt(2.) * std::sin(6.28318530718 * 200. * i / 16000.));
    }
    SpeechLightEnvelope envelope;
    envelope.Append(tone.data(), tone.size(), 16000);
    return envelope.GetLevel(static_cast<uint32_t>(tone.size()));
  };
  const auto ordinary = SpeechLightEnvelope::Colors(levelAtRms(0.18f))[1];
  Require((ordinary >> 24) > 0 && (ordinary >> 24) == ((ordinary >> 16) & 0xff) &&
          (ordinary >> 24) == ((ordinary >> 8) & 0xff), "Formerly saturated syllables should now stay white");
  const auto strong = SpeechLightEnvelope::Colors(levelAtRms(0.32f))[1];
  Require((strong >> 24) > ((strong >> 16) & 0xff) &&
          ((strong >> 16) & 0xff) > (strong >> 24) / 2,
          "Strong syllables should ease into pink with headroom before red");
  Require(SpeechLightEnvelope::Colors(levelAtRms(0.46f)) == loud, "Strong peaks must still reach red");
  float previousLevel = 0.f;
  for (const float rms : {0.04f, 0.08f, 0.16f, 0.24f, 0.32f, 0.42f, 0.46f}) {
    const float level = levelAtRms(rms);
    Require(level > previousLevel, "Meter must retain dynamics across quiet, ordinary, and strong speech");
    previousLevel = level;
  }

  // The synthesis worker and animation thread share an envelope. Exercise that
  // path while data is still arriving, with playback always behind the producer.
  SpeechLightEnvelope concurrent;
  std::atomic<uint32_t> published{0};
  const auto samples = Speech(16000);
  std::thread producer([&] {
    for (uint32_t i = 0; i < samples.size(); i += 160) {
      concurrent.Append(samples.data() + i, 160, 16000);
      published.store(i + 160);
    }
  });
  uint32_t played = 0;
  while (played < samples.size()) {
    played = published.load();
    const float level = concurrent.GetLevel(played);
    Require(level >= 0.f && level <= 1.f, "Concurrent playback level stays bounded");
  }
  producer.join();
  Require(concurrent.GetLevel(16000) < 0.01f, "Concurrent speech finishes dark");

  SpeechLightPlayback output;
  output.Push(0.25f, 1000, 200);
  output.Push(0.75f, 1033, 200);
  output.Push(0.f, 1066, 200);
  output.Finish(1100, 200);
  output.Update(1199);
  Require(!output.IsPlaying(), "Lights wait for the speaker output buffers");
  output.Update(1200);
  Require(output.IsPlaying() && output.GetLevel() == 0.25f, "First syllable follows speaker latency");
  output.Update(1233);
  Require(output.IsPlaying() && output.GetLevel() == 0.75f, "Louder syllable preserves its timing");
  output.Update(1266);
  Require(output.IsPlaying() && output.GetLevel() == 0.f, "Buffered pauses remain dark");
  Require(!output.IsDone(), "Normal completion keeps the buffered tail alive");
  output.Update(1300);
  Require(output.IsDone() && !output.IsPlaying(), "Lights end when the buffered voice ends");

  SpeechLightPlayback underrun;
  underrun.Push(1.f, 2000, 200);
  underrun.Update(2200);
  Require(underrun.IsPlaying(), "Delayed stream level becomes visible");
  underrun.Update(2300);
  Require(!underrun.IsPlaying(), "An underrun must not leave the last syllable stuck on");
  underrun.Push(0.5f, 2400, 200);
  underrun.Update(2600);
  Require(underrun.IsPlaying() && underrun.GetLevel() == 0.5f, "Playback resumes with the same output delay");

  std::cout << "PASS: playback synchronization, silence, syllable dynamics, pauses, sample rates, chunk boundaries, streaming, partial windows, white-to-red meter, speech headroom, gradual color curve, concurrent generation, speaker latency, buffered completion, underrun/resume\n";
}
