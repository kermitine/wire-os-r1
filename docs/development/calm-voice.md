# Kalani-inspired native English speech

The robot's native English speech uses a tactical-droid preset aimed at
General Kalani's voice in *The Clone Wars*. The reference character is
[General Kalani](https://www.starwars.com/databank/general-kalani).
This combines a lower register and restrained intonation with metallic audio
coloration. It processes the existing Acapela Ben voice; resemblance to the
character needs a listening check on Vector.

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
| `pitchRangeMax_percent` | unset | 115 | 105 |
| `neutralSentenceEndings` | false | true | true |
| `tacticalDroidEffect` | false | false | true |

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
76 to 84. These controls are experimental: their audible effect on the bundled
voice has not been verified. A rejected setting logs a warning and speech
continues. Acceptance alone does not prove the voice uses the limits. Missing
or invalid ranges leave the voice's default contour in place.

Neutral sentence endings complete unfinished statements and replace final
exclamations with full stops: `Your timer is ready` and `Your timer is ready!`
both become `Your timer is ready.` for synthesis. Questions, completed
sentences, ellipses, and explicit speech markup are preserved. This encourages
completed statement endings without changing the words or rewriting the
entire voice contour. Speed traits use the original text length.

## Metallic audio effect

`tacticalDroidEffect: true` enables `TacticalDroidVoiceEffect` in the robot's
native TTS provider, immediately after each mono PCM chunk is synthesized:

- A 140 Hz high-pass and two 3.6 kHz low-pass stages shape the speech band.
- A 6 ms, 16% feed-forward delay adds short metallic resonance.
- A 65 Hz partial ring modulator adds electronic sidebands while retaining
  the speech carrier.
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
speech-band preservation, and the modulation sidebands. These checks establish
signal behavior; they do not establish an audible character match.

Listen on Vector for the lower tone, metallic texture, clear consonants,
consistent loudness, and absence of clicks or exaggerated warbling. The small
speaker and existing Wwise effects influence the final result. Useful phrases:

- "The probability of success is high."
- "Your timer is ready."
- "The temperature is seventy-two degrees."
- "You have a reminder at three thirty."
- "Would you like another timer?"

To retain the synthesizer tuning but remove the metallic coloration, set
`tacticalDroidEffect` to false and redeploy resources. To restore the previous
lower-pitched robot voice, also restore pitch 85, shaping 120, and pitch range
85 to 115. To restore the original voice, use pitch 100 and shaping 130, set
both range values to 0, and set `neutralSentenceEndings` to false. Restart the
animation process after changing the resources.
