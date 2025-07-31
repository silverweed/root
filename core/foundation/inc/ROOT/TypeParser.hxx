#pragma once

#include <string_view>
#include <string>
#include <cstdint>
#include <iostream>
#include <vector>

namespace ROOT::Internal::TypeParsing {

enum ETokType {
   kInvalid,
   kIdent,
   kNumber,
   kString,
   kCharacter,
   kFirstFixed,
   kKwConst = kFirstFixed,
   kKwVolatile,
   kKwNot,
   kKwAnd,
   kKwOr,
   kKwBitand,
   kKwBitor,
   kKwXor,
   kFirstNonKeyword,
   kAndAnd = kFirstNonKeyword,
   kOrOr,
   kAnd,
   kOr,
   kXor,
   kTilde,
   kPlusPlus,
   kMinusMinus,
   kPlus,
   kMinus,
   kStar,
   kSlash,
   kColonColon,
   kLe,
   kGe,
   kLt,
   kGt,
   kEq,
   kNe,
   kNot,
   kComma,
   kOpenRound,
   kCloseRound,
   kOpenSquare,
   kCloseSquare,
   kLastFixed = kCloseSquare,
   kEOF,
};

struct TToken {
   ETokType fType = kInvalid;
   // Either an ident name (for kIdent) or an error string (for kInvalid)
   std::string fStr;

   TToken() = default;
   TToken(ETokType type) : fType(type) {}

   static TToken Ident(std::string_view str)
   {
      TToken tok = {kIdent};
      tok.fStr = str;
      return tok;
   }

   static TToken Char(char ch)
   {
      TToken tok = {kCharacter};
      tok.fStr = ch;
      return tok;
   }

   static TToken String(std::string_view str)
   {
      TToken tok = {kString};
      tok.fStr = str;
      return tok;
   }

   static TToken Fixed(std::string_view fixed);
};

bool operator==(const TToken &a, const TToken &b);
std::ostream &operator<<(std::ostream &out, const TToken &t);
// Required by GoogleTest
void PrintTo(const TToken &t, std::ostream *os);

class TLexer final {
   std::string_view fSrc;
   std::size_t fCur = 0;
   std::size_t fNext = 0;

   // Returns the index inside kFixeds or -1 if not found
   int PeekFixed(std::size_t pos, std::size_t firstToCheck = 0) const;
   bool IsWordTerminator(std::size_t pos) const;

public:
   // These static methods are mostly for debugging/testing. In a real case one should use Peek()/Consume().
   static std::vector<TToken> Tokenize(std::string_view src);
   static void TokenizeAndPrint(std::string_view src, std::ostream &out = std::cout);

   explicit TLexer(std::string_view src) : fSrc(src) {}

   TToken Peek();
   void Consume();
};

} // namespace ROOT::Internal::TypeParsing
