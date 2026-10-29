# Lower-pitched English speech at the original pace

The English native TTS configuration in `resources/config/engine/tts_config.json`
restores the original speaking pace and lowers the pitch for a deeper,
CASE-inspired delivery. It replaces the earlier slowdown:

| Setting | Original | Previous calm voice | Current |
| --- | --- | --- | --- |
| Base speed | 80 | 76 | 80 |
| Speed for text longer than 10 characters | 83 | 76 | 83 |
| Speed for text up to 10 characters | 68 | 68 | 68 |
| Pitch | 100 | 94 | 85 |
| Voice shaping | 130 | 120 | 120 |

Both the robot (`vicos`) and macOS English entries use these settings. Pitch and
shaping are synthesizer parameters, not percentages of acoustic frequency.
The pitch setting of 85 is a starting point for a noticeably deeper voice;
the warmer shaping from the previous configuration is retained. The providers
pass speed and pitch separately to Acapela, so lowering pitch does not require
slowing speech or changing the playback sample rate.
Volume, equalization, punctuation pauses, and the selected base voice retain
their existing settings. Other languages retain their existing configuration.

The speed traits override the base speed for matching text lengths, so the
longer-text trait is restored along with the base. The existing inclusive
ranges overlap at 10 characters; the first matching trait wins, retaining speed
68 for that boundary. Text outside the traits uses the original base speed, 80.

The synthesizer still divides speed by each request's duration scalar. With a
scalar of 0.95, longer replies use 83 / 0.95, or about 87.4, restored from 80.
With a scalar of 1.0, they use 83, restored from 76. Short acknowledgements
retain their original pace as well.

## On-robot intonation

The robot's English configuration also enables these optional controls:

| Setting | Value |
| --- | --- |
| `pitchRangeMin_percent` | 85 |
| `pitchRangeMax_percent` | 115 |
| `neutralSentenceEndings` | true |

The range limits are passed to Acapela's `BABIL_PARM_MINPITCH` and
`BABIL_PARM_MAXPITCH` after each synthesis reset. They follow the request's
adjusted pitch, including `pitchScalar`. At pitch 85 the requested range is
72 to 97. This is an experimental request for a more restrained contour:
the SDK exposes these controls, but their audible effect on the bundled Ben
Colibri voice has not been verified. A rejected setting logs a warning and
speech continues. Acceptance alone does not prove the voice uses the limits.
Missing or invalid range settings leave the voice defaults in place.

Neutral sentence endings turn unfinished statements and final exclamations
into full stops: `Your timer is ready` and `Your timer is ready!` both become
`Your timer is ready.` for synthesis. Questions, already completed sentences,
ellipses, and explicit speech markup are preserved. Acapela documents that
full stops encourage downward intonation, while commas encourage a rise:
[Acapela voice tuning](https://www.acapela-group.com/voices/voice-tuning/).
This improves sentence endings; it does not rewrite the voice's learned
intonation throughout a sentence. Speed traits still use the original text
length, before this punctuation change.

These additional controls run in the animation process on the robot, without
an external speech service. macOS retains its pitch and speed configuration
but does not apply the on-robot prosody controls. Other languages are unchanged.
Set both range values to 0 and `neutralSentenceEndings` to false to disable
the additional controls.

The intonation changes require rebuilding and deploying the animation binary
along with the updated resources, then restarting the animation process.
Pitch/speed-only changes need a resource deployment and restart. They apply to native
synthesized English speech, including Rocket-Pod's native `SayText` requests.
Externally generated speech streamed by Rocket-Pod and prerecorded character
sounds use separate audio paths and do not inherit these settings.

The source configuration and effective rates can be checked on the desktop,
but the result still needs a listening check on Vector. In particular, verify
that the installed voice accepts the pitch and shaping controls, and listen
for a deeper tone, consonant clarity, consistent loudness, and the restored
pace through the robot processing. This tunes the existing Ben voice rather
than reproducing CASE's voice exactly.
Useful sample phrases are:

- "Your timer is ready."
- "The temperature is seventy-two degrees."
- "You have a reminder at three thirty."
- "Your timer is ready" (should have a completed statement ending).
- "Your timer is ready!" (should have a more restrained ending).
- "Would you like another timer?" (should remain a question).

To restore the original English voice settings, use the Original values in the
table for both English configuration entries and redeploy the resources.
