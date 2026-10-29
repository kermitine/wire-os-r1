#ifndef __cozmo_textToSpeech_speechProsody_h
#define __cozmo_textToSpeech_speechProsody_h

#include <string>

namespace Anki {
namespace Vector {
namespace TextToSpeech {

// Give plain-text replies a completed statement contour. Keep real questions
// and explicit Acapela/SSML markup under the caller's control.
inline std::string WithNeutralSentenceEnding(const std::string& text)
{
  if (text.find_first_of("\\<>") != std::string::npos) {
    return text;
  }

  // Find the last spoken character, inside any closing quotes or brackets.
  auto limit = text.size();
  while (limit > 0) {
    if (std::string(" \t\r\n\"')]}").find(text[limit - 1]) != std::string::npos) {
      --limit;
    } else if (limit >= 3 &&
               (text.compare(limit - 3, 3, "\xE2\x80\x9D") == 0 ||
                text.compare(limit - 3, 3, "\xE2\x80\x99") == 0)) {
      limit -= 3; // UTF-8 right double/single quotation mark
    } else {
      break;
    }
  }
  if (limit == 0) {
    return text;
  }
  const auto end = limit - 1;

  // Find a trailing punctuation group, including combinations such as ?!.
  const auto beforePunctuation = text.find_last_not_of(".!?,;:", end);
  const auto begin = (beforePunctuation == std::string::npos) ? 0 : beforePunctuation + 1;
  const auto punctuation = text.substr(begin, end - begin + 1);
  if (punctuation.find('?') != std::string::npos || text[end] == '.' ||
      (limit >= 3 && text.compare(limit - 3, 3, "\xE2\x80\xA6") == 0)) {
    return text;
  }

  std::string result = text;
  if (begin <= end) {
    result.replace(begin, end - begin + 1, ".");
  } else {
    result.insert(end + 1, ".");
  }
  return result;
}

} // namespace TextToSpeech
} // namespace Vector
} // namespace Anki

#endif
