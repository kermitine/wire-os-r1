#include "gtest/gtest.h"

#include "cozmoAnim/textToSpeech/speechProsody.h"

using Anki::Vector::TextToSpeech::WithNeutralSentenceEnding;
using Anki::Vector::TextToSpeech::WithReducedWordEmphasis;

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

TEST(SpeechProsody, ReducesStressOnEveryWordIncludingTheLast)
{
  EXPECT_EQ("\\emph=0\\quarter.", WithReducedWordEmphasis(WithNeutralSentenceEnding("quarter")));
  EXPECT_EQ("\\emph=0\\A \\emph=0\\quarter \\emph=0\\past \\emph=0\\three.",
            WithReducedWordEmphasis(WithNeutralSentenceEnding("A quarter past three!")));
  EXPECT_EQ("\\emph=0\\Your \\emph=0\\timer \\emph=0\\is \\emph=0\\ready.",
            WithReducedWordEmphasis(WithNeutralSentenceEnding("Your timer is ready!")));
  EXPECT_EQ("\\emph=0\\Would \\emph=0\\you \\emph=0\\like \\emph=0\\another \\emph=0\\timer?",
            WithReducedWordEmphasis(WithNeutralSentenceEnding("Would you like another timer?")));
}

TEST(SpeechProsody, KeepsTokensAndWhitespaceIntactWhenReducingStress)
{
  EXPECT_EQ("  \\emph=0\\Don't\t\\emph=0\\change \\emph=0\\$72.50 \\emph=0\\at \\emph=0\\3:30\n\\emph=0\\PM.  ",
            WithReducedWordEmphasis("  Don't\tchange $72.50 at 3:30\nPM.  "));
  EXPECT_EQ(u8"\\emph=0\\\u201cCaf\u00e9 \\emph=0\\is \\emph=0\\open.\u201d",
            WithReducedWordEmphasis(u8"\u201cCaf\u00e9 is open.\u201d"));
  for (const std::string text : {"", " \t\n", "...", "?!", u8"\u2026"}) {
    EXPECT_EQ(text, WithReducedWordEmphasis(text));
  }
}

TEST(SpeechProsody, PreservesCallerMarkupWhenReducingStress)
{
  for (const std::string text : {"\\pit=90\\Ready", "<speak>Ready!</speak>", "x > y",
                                 "\\emph=1\\Ready!", "C:\\robot", "1 < 2"}) {
    EXPECT_EQ(text, WithReducedWordEmphasis(text));
    EXPECT_EQ(text, WithReducedWordEmphasis(WithNeutralSentenceEnding(text)));
  }
}
