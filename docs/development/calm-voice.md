# Kalani-inspired native English speech

The robot's native English speech uses a tactical-droid preset aimed at
General Kalani's voice in *The Clone Wars*. The reference character is
[General Kalani](https://www.starwars.com/databank/general-kalani).
This combines a fixed low pitch with metallic audio coloration. It processes
the existing Acapela Ben voice; resemblance to the character needs a listening
check on Vector.

## Synthesizer settings

`resources/config/engine/tts_config.json` contains these English settings:

| Setting | Original | Previous lower-pitch baseline | Current robot |
| --- | --- | --- | --- |
| Base speed | 80 | 80 | 80 |
| Speed for text longer than 10 characters | 83 | 83 | 83 |
| Speed for text up to 10 characters | 68 | 68 | 68 |
| Pitch | 100 | 85 | 80 |
| Voice shaping | 130 | 120 | 110 |
| `pitchRangeMin_percent` | unset | 85 | 95 |
| `pitchRangeMax_percent` | unset | 115 | 100 |
| `neutralSentenceEndings` | false | true | true |
| `reduceWordEmphasis` | false | false | false |
| `tacticalDroidEffect` | false | false | true |
| `roboticPitch_hz` | unset | unset | 85 |

Speaking speed is preserved. Pitch and shaping are synthesizer parameters,
not percentages of acoustic frequency. macOS retains the previous baseline
(pitch 85, shaping 120, original speed), without the robot-only prosody and
audio effects. Other languages retain their configuration.

Speed traits override the base speed. Their existing inclusive ranges overlap
at 10 characters; the first match retains speed 68 at that boundary. The
synthesizer divides speed by each request's duration scalar, as before. For
example, longer replies at duration scalar 0.95 use 83 / 0.95, about 87.4.

The pitch limits are passed to Acapela's `BABIL_PARM_MINPITCH` and
`BABIL_PARM_MAXPITCH` after each synthesis reset. They follow the request's
adjusted pitch, including `pitchScalar`. At pitch 80 the requested range is
76 to 80. The upper limit no longer requests any rise above the base pitch.
These synthesizer controls are experimental: their audible effect on the bundled
voice has not been verified. A rejected setting logs a warning and speech
continues. Acceptance alone does not prove the voice uses the limits. Missing
or invalid ranges leave the voice's default contour in place. Lowering the
base pitch or limiting its requested range does not reliably remove the Ben
voice's upward word inflections. The fixed-pitch vocoder below addresses the
audio itself, independently of whether Acapela honors these requests.

Neutral sentence endings complete unfinished statements and replace final
exclamations with full stops: `Your timer is ready` and `Your timer is ready!`
both become `Your timer is ready.` for synthesis. Questions, completed
sentences, ellipses, and explicit speech markup are preserved. This encourages
completed statement endings without changing the words or rewriting the
entire voice contour. Speed traits use the original text length.

The optional legacy setting `reduceWordEmphasis: true` requests less stress on
every word, targeting the enthusiastic word endings that remain after lowering the voice and
normalizing final punctuation. Acapela exposes a decrease-emphasis control
in its [voice editor](https://www.acapela-cloud.com/editor/). The native text
preparation inserts `\emph=0\` before each whitespace-delimited token containing
letters or digits, with a pronunciation-context exception described below,
so words inside the sentence receive the instruction too.
For example, `A quarter past three!` is prepared as:

```text
\emph=0\A \emph=0\quarter \emph=0\past \emph=0\three.
```

The tags are inserted after sentence-ending normalization. Existing whitespace,
punctuation, contractions, numeric tokens, and UTF-8 bytes stay intact. Caller
markup bypasses both automatic text changes; questions keep their question
marks. Speed traits still use the original text length, before the tags are
added. Missing or false `reduceWordEmphasis` keeps the previous text behavior.
The current preset leaves this setting false and passes complete phrases to
Acapela without adding emphasis tags. The vocoder handles pitch without
interrupting pronunciation context or number expansion.

When enabled, emphasis is a request to the speech engine, rather than a measured
correction of the waveform's pitch: its audible effect depends on the bundled voice, and
does not guarantee a downward pitch sweep on every syllable. Verify the
response on Vector before judging whether it removes the reported inflection.

With that legacy setting enabled, "minute" (including "minutes" and hyphenated compounds) and its next
word retain native stress without an intervening emphasis tag. This preserves
the quantity/unit context in "one minute" and the adjective context in
"minute details", following a report of the time unit being spoken as
"mine-oot" after the word-level changes. The fix removes the potentially
disruptive tags rather than respelling every occurrence of "minute". Other
words retain reduced emphasis. Test the actual time-unit pronunciation on
Vector; desktop tests verify intact context, not the synthesized phonemes.

## Fixed low pitch and metallic audio effect

`roboticPitch_hz: 85`, together with `tacticalDroidEffect: true`, enables a
20-band channel vocoder before the metallic effect. Speech supplies the changing
frequency-band envelopes, preserving syllable timing and the changing shapes
of vowels and consonants. A harmonic carrier supplies a steady 85 Hz pitch
(harmonics at 170 Hz, 255 Hz, and so on), replacing the source's rising pitch.
This produces a more electronic, monotone delivery; it is not a recording or
model of Kalani's voice. The texture and consonant clarity need a listening
check on Vector.

The setting accepts integer frequencies from 60 to 140 Hz. Try 75 for a deeper
carrier, 85 for the current preset, or 100 for a higher carrier. This frequency
is separate from Acapela's `pitch` parameter. A missing, zero, or invalid value
disables the vocoder and retains the previous metallic effect. Invalid nonzero
values also log a configuration warning. Setting `tacticalDroidEffect` false
bypasses both audio processors.

While vocoding is enabled, the carrier stays fixed even if a request supplies
`pitchScalar` or explicit pitch markup. Those requests still reach Acapela but
do not override the final carrier. Set `roboticPitch_hz` to 0 to restore their
usual influence on the output pitch.

The vocoder tracks rectified band amplitudes through two 25 Hz envelope poles
to suppress the source's vocal vibration. It uses continuous harmonic
oscillators, with allocation and coefficient calculation only at utterance
reset. There is no frame buffer or lookahead; sample count and playback duration
stay unchanged. Envelope smoothing softens fast transitions. Filter, envelope,
and carrier state persist across chunks and reset for each utterance. Silence
does not excite the carrier, and envelopes decay during pauses.

`tacticalDroidEffect: true` enables `TacticalDroidVoiceEffect` in the robot's
native TTS provider, immediately after synthesis and optional vocoding of each
mono PCM chunk:

- A 140 Hz high-pass and two 3.6 kHz low-pass stages shape the speech band.
- A 6 ms, 16% feed-forward delay adds short metallic resonance.
- A partial ring modulator adds electronic sidebands. With the vocoder on, its
  frequency follows `roboticPitch_hz` to keep sidebands on the same harmonic
  grid; otherwise it uses the previous 65 Hz modulation.
- Gentle saturation adds texture with output headroom and bounded 16-bit PCM.

The processor retains filter, delay, and modulation state across synthesis
chunks and resets at the start of each utterance. Processing is in place,
with no changes to sample rate, sample count, or speaking duration. There is
no feedback delay or added reverb tail. Memory allocation happens on reset,
outside the sample-processing loop. Missing or false settings give exact PCM
passthrough. Unsupported sample rates also bypass processing.

This effect is part of the native provider, before the existing Wwise
processing. It therefore applies to native speech even if a caller selects
the Wwise `Unprocessed` style; that style only bypasses Wwise's processing.
Externally generated speech streamed by Rocket-Pod and prerecorded character
sounds use separate paths and do not inherit this preset. Native Rocket-Pod
`SayText` requests do inherit it.

## Build, verify, and tune

Rebuild and deploy the animation binary and updated resources, then restart
the animation process. See the repository README for the supported Linux and
macOS build/deployment workflow. Resource-only deployment is insufficient for
the new PCM effect.

A standalone regression test exercises actual DSP output without the robot
SDK. From the repository root on Linux or macOS:

```sh
mkdir -p _build/tactical-droid-check
c++ -std=c++14 -O2 -Wall -Wextra -pedantic -IanimProcess/src \
  tools/voice/test_tactical_droid_voice.cpp \
  -o _build/tactical-droid-check/test_tactical_droid_voice
_build/tactical-droid-check/test_tactical_droid_voice
```

Or from a Visual Studio Developer PowerShell prompt:

```powershell
New-Item -ItemType Directory -Force _build/tactical-droid-check | Out-Null
cl /nologo /std:c++14 /EHsc /W4 /IanimProcess/src `
  tools/voice/test_tactical_droid_voice.cpp `
  /Fo_build/tactical-droid-check/ /Fe_build/tactical-droid-check/test_tactical_droid_voice.exe
& _build/tactical-droid-check/test_tactical_droid_voice.exe
```

Checks cover exact bypass, silence, headroom at several sample rates,
sample-identical output across different chunk sizes, utterance reset and sample-rate changes,
speech-band preservation, the modulation sidebands, replacement of different
source pitches by the configured carrier (including after metallic processing),
vocoder bypass, silence and decay, and word-level text
preparation (including "quarter", questions, numbers, caller markup, and
intact "minute" pronunciation context).
These checks establish
signal behavior; they do not establish an audible character match.

Listen on Vector for the lower tone, metallic texture, clear consonants,
consistent loudness, and absence of clicks or exaggerated warbling. The small
speaker and existing Wwise effects influence the final result. Useful phrases:

- "The probability of success is high."
- "Update."
- "Update complete."
- "One. Two. Three. Four. Five."
- "The temperature is 72 degrees."
- "Your timer has 1 minute and 30 seconds remaining."
- "Quarter."
- "A quarter past three."
- "The quarter is on the table."
- "One minute."
- "Your one minute timer is ready."
- "Two minutes remaining."
- "A minute detail matters." (should retain the adjective reading).
- "Your timer is ready."
- "The temperature is seventy-two degrees."
- "You have a reminder at three thirty."
- "Would you like another timer?"

To keep the previous metallic effect with its natural speech inflection, set
`roboticPitch_hz` to 0. To retain the synthesizer tuning but remove both
the vocoder and metallic coloration, set
`tacticalDroidEffect` to false and redeploy resources. To restore the previous
lower-pitched robot voice, also restore pitch 85, shaping 120, and pitch range
85 to 115 and set `reduceWordEmphasis` to false. To keep the first metallic
preset's delivery, retain pitch 80 and shaping 110, restore the range to
95 to 105, set `roboticPitch_hz` to 0, and set `reduceWordEmphasis` to false.
To restore the original voice,
use pitch 100 and shaping 130, set both range values to 0, and set
`neutralSentenceEndings` and `reduceWordEmphasis` to false. Restart the animation
process after changing the resources.
