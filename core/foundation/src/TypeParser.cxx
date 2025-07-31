#include <ROOT/TypeParser.hxx>
#include <cstring>
#include <cassert>

namespace ROOT::Internal::TypeParsing {

static constexpr std::size_t kNumFixeds = kLastFixed - kFirstFixed + 1;
static const std::size_t kNumKeywords = kFirstNonKeyword - kFirstFixed;

// NOTE: must be in the same order as ETokType.
// Strings with the same prefixes must come in order from longest to shortest.
static const char *const kFixeds[] = {
   "const", "volatile", "not", "and", "or", "bitand", "bitor", "xor", "&&", "||", "&", "|", "^", "~", "++", "--", "+",
   "-",     "*",        "/",   "::",  "<=", ">=",     "<",     ">",   "==", "!=", "!", ",", "(", ")", "[",  "]"};
static_assert(std::size(kFixeds) == kNumFixeds);

static bool IsStartOfNumber(char ch)
{
   return ch == '.' || (ch >= '0' && ch <= '9');
}

static bool IsPartOfNumber(char ch)
{
   return IsStartOfNumber(ch) || ch == 'f' || ch == 'F' || ch == 'e' || ch == 'E' || ch == 'x' || ch == 'X' ||
          ch == 'p' || ch == 'P' || ch == 'o' || ch == 'O';
}

int TLexer::PeekFixed(std::size_t pos, std::size_t firstToCheck) const
{
   int idx = -1;

   const std::size_t maxChars = fSrc.length() - pos;
   const char *const curWord = fSrc.data() + pos;
   for (std::size_t i = firstToCheck; i < std::size(kFixeds); ++i) {
      std::size_t kwLen = strlen(kFixeds[i]);
      if (strncmp(kFixeds[i], curWord, std::min(kwLen, maxChars)) == 0) {
         idx = i;
         break;
      }
   }
   return idx;
}

bool TLexer::IsWordTerminator(std::size_t pos) const
{
   char ch = fSrc[pos];
   // NOTE: a word is terminated by an operator, but not by a keyword
   // (otherwise stuff like "vector" would be lexed as "vect" + "or")
   return std::isspace(ch) || PeekFixed(pos) >= static_cast<int>(kNumKeywords);
}

TToken TLexer::Peek()
{
   std::size_t srcSize = fSrc.size();

   assert(fNext >= fCur);
   assert(fCur <= srcSize);

   if (fCur >= srcSize)
      return {kEOF};

   auto cur = fCur;
   do {
      char ch = fSrc[cur];
      ++cur;

      if (std::isspace(ch)) {
         fCur = fNext = cur;
         continue;
      }

      std::size_t wordStart = cur - 1;

      // character
      if (ch == '\'') {
         TToken tok = {};
         if (cur < srcSize) {
            char character = fSrc[cur];
            ++cur;
            if (cur < srcSize && fSrc[cur] == '\'') {
               tok.fType = kCharacter;
               tok.fStr = character;
               ++cur;
            }
         }
         fNext = cur;
         return tok;
      }

      // string
      if (ch == '"') {
         TToken tok = {};
         const auto stringStart = cur;
         auto stringLen = 0u;
         while (cur < srcSize) {
            char newCh = fSrc[cur];
            ++cur;
            if (newCh == '"') {
               tok.fType = kString;
               break;
            }
            ++stringLen;
         }
         if (tok.fType == kString)
            tok.fStr = {fSrc.data() + stringStart, stringLen};
         fNext = cur;
         return tok;
      }

      // number
      if (IsStartOfNumber(ch)) {
         // NOTE: we don't really decode or validate the number, we simply skip ahead until the string looks like one.
         TToken token = {kNumber};
         while (cur < srcSize && IsPartOfNumber(fSrc[cur])) {
            ++cur;
         }
         fNext = cur;
         return token;
      }

      if (cur > srcSize)
         break;

      // fixed
      int fixedIdx = PeekFixed(cur - 1);
      while (fixedIdx >= 0) {
         const char *keyword = kFixeds[fixedIdx];
         auto kwLen = strlen(keyword);
         const auto endPos = cur - 1 + kwLen;
         // For keyword tokens, check if it ends properly (e.g. "constf" should be an ident, not keyword "const").
         if (endPos == srcSize ||
             (endPos < srcSize && (fixedIdx >= (int)kFirstNonKeyword || IsWordTerminator(endPos)))) {
            cur += kwLen - 1;
            fNext = cur;
            TToken tok;
            tok.fType = static_cast<ETokType>(kFirstFixed + fixedIdx);
            return tok;
         }
         // Try again: maybe that was a keyword with matching prefix but there is a valid one later.
         fixedIdx = PeekFixed(cur - 1, fixedIdx + 1);
      }

      if (cur >= srcSize)
         break;

      // identifier
      do {
         if (IsWordTerminator(cur))
            break;
         ++cur;
      } while (cur < srcSize);

      std::size_t identSize = cur - wordStart;
      TToken tok = TToken::Ident({fSrc.data() + wordStart, identSize});

      fNext = cur;
      return tok;

   } while (cur < srcSize);

   assert(cur == srcSize);
   fNext = cur;
   return {kEOF};
}

void TLexer::Consume()
{
   fCur = fNext;
}

std::vector<TToken> TLexer::Tokenize(std::string_view src)
{
   std::vector<TToken> outTokens;
   TLexer lex(src);
   auto t = lex.Peek();
   while (t.fType != kEOF) {
      outTokens.push_back(t);
      lex.Consume();
      t = lex.Peek();
   }
   return outTokens;
}

void TLexer::TokenizeAndPrint(std::string_view src, std::ostream &out)
{
   const auto tokens = TLexer::Tokenize(src);

   for (const auto &t : tokens) {
      out << t << "\n";
   }
}

TToken TToken::Fixed(std::string_view fixed)
{
   TToken tok = {};
   for (auto i = 0u; i < kNumFixeds; ++i) {
      if (fixed == kFixeds[i]) {
         tok.fType = static_cast<ETokType>(kFirstFixed + i);
         break;
      }
   }
   assert(tok.fType >= kFirstFixed && tok.fType <= kLastFixed);
   return tok;
}

bool operator==(const TToken &a, const TToken &b)
{
   if (a.fType != b.fType)
      return false;
   if (a.fType == kIdent)
      return a.fStr == b.fStr;

   return true;
}

std::ostream &operator<<(std::ostream &out, const TToken &t)
{
   out << t.fType << " ";
   if (t.fType == kIdent || t.fType == kString || t.fType == kCharacter)
      out << "\"" << t.fStr << "\"";
   else if (t.fType == kNumber)
      out << "(number)";
   else if (t.fType >= kFirstFixed && t.fType <= kLastFixed)
      out << '"' << kFixeds[t.fType - kFirstFixed] << '"';
   return out;
}

void PrintTo(const TToken &t, std::ostream *os)
{
   if (os)
      *os << t;
}

} // namespace ROOT::Internal::TypeParsing
