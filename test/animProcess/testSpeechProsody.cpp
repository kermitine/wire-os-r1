#include "gtest/gtest.h"

#include "cozmoAnim/textToSpeech/speechProsody.h"

using Anki::Vector::TextToSpeech::WithNeutralSentenceEnding;

TEST(SpeechProsody, CompletesStatementsWithoutChangingWords)
{
  EXPECT_EQ("Your timer is ready.", WithNeutralSentenceEnding("Your timer is ready"));
  EXPECT_EQ("Ready.", WithNeutralSentenceEnding("Ready!"));
  EXPECT_EQ("Ready.", WithNeutralSentenceEnding("Ready!!!"));
  EXPECT_EQ("Ready.", WithNeutralSentenceEnding("Ready,"));
  EXPECT_EQ("Ready.", WithNeutralSentenceEnding("Ready;"));
  EXPECT_EQ("The temperature is 72.5.", WithNeutralSentenceEnding("The temperature is 72.5"));
  EXPECT_EQ("\"Ready.\"  \n", WithNeutralSentenceEnding("\"Ready!\"  \n"));
  EXPECT_EQ("(Ready.)", WithNeutralSentenceEnding("(Ready)"));
  EXPECT_EQ(u8"He said, \u201cReady.\u201d", WithNeutralSentenceEnding(u8"He said, \u201cReady!\u201d"));
  EXPECT_EQ(u8"Caf\u00e9 is open.", WithNeutralSentenceEnding(u8"Caf\u00e9 is open"));
}

TEST(SpeechProsody, PreservesQuestionsAndCompleteSentences)
{
  for (const std::string text : {"Are you ready?", "Really?!", "He said, \"Ready?\"",
                                 "Your timer is ready.", "Thinking...", u8"Thinking\u2026",
                                 u8"\u201cReady?\u201d", "", " \t\n", u8"\u201d"}) {
    EXPECT_EQ(text, WithNeutralSentenceEnding(text));
  }
}

TEST(SpeechProsody, LeavesExplicitSpeechMarkupAlone)
{
  for (const std::string text : {"\\pit=90\\Ready", "<speak>Ready!</speak>", "x > y"}) {
    EXPECT_EQ(text, WithNeutralSentenceEnding(text));
  }
}
