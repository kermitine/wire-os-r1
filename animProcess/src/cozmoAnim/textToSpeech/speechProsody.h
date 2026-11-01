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

// "Minute" has different noun and adjective readings. Keep its immediate
// linguistic context free of inserted tags so the native engine can choose
// the pronunciation from the phrase ("one minute" / "minute details").
inline bool ContainsMinuteHomograph(std::string token)
{
  for (auto& c : token) {
    if (c >= 'A' && c <= 'Z') {
      c = static_cast<char>(c - 'A' + 'a');
    }
  }
  const auto isWordCharacter = [](unsigned char c) {
    return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c >= 0x80;
  };
  for (auto begin = token.find("minute"); begin != std::string::npos;
       begin = token.find("minute", begin + 1)) {
    auto end = begin + 6;
    if (end < token.size() && token[end] == 's') {
      ++end;
    }
    if ((begin == 0 || !isWordCharacter(token[begin - 1])) &&
        (end == token.size() || !isWordCharacter(token[end]))) {
      return true;
    }
  }
  return false;
}

// Acapela's emphasis tag applies to the next word, rather than the whole
// utterance. Request less stress throughout plain English speech. Only insert
// at whitespace boundaries so contractions, dates, numbers, and UTF-8 bytes
// remain intact. Preserve the word after "minute" as well, since it can
// distinguish time phrases from adjective phrases. Caller markup owns its delivery.
inline std::string WithReducedWordEmphasis(const std::string& text)
{
  if (text.find_first_of("\\<>") != std::string::npos) {
    return text;
  }

  const char* whitespace = " \t\r\n\f\v";
  std::string result;
  size_t position = 0;
  bool previousWordNeedsContext = false;
  while (position < text.size()) {
    const auto begin = text.find_first_not_of(whitespace, position);
    if (begin == std::string::npos) {
      result.append(text, position, text.size() - position);
      break;
    }
    result.append(text, position, begin - position);
    const auto nextSpace = text.find_first_of(whitespace, begin);
    const auto end = nextSpace == std::string::npos ? text.size() : nextSpace;

    bool hasWord = false;
    for (size_t i = begin; i < end && !hasWord; ++i) {
      const auto c = text[i];
      hasWord = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9');
    }
    const bool needsContext = hasWord && ContainsMinuteHomograph(text.substr(begin, end - begin));
    if (hasWord && !needsContext && !previousWordNeedsContext) {
      result += "\\emph=0\\";
    }
    result.append(text, begin, end - begin);
    if (hasWord) {
      previousWordNeedsContext = needsContext;
    }
    position = end;
  }
  return result;
}

} // namespace TextToSpeech
} // namespace Vector
} // namespace Anki

#endif
