/// \file TypeParser.cxx
/// \ingroup Core
/// \author Giacomo Parolini <giacomo.parolini@cern.ch>
/// \date 2025-08-04

#include <ROOT/TypeParser.hxx>

#include <ROOT/StringUtils.hxx>

#include <cstring>
#include <cassert>
#include <sstream>

namespace ROOT::Internal::TypeParsing {

// This lexer+parser is designed to parse an "extended" version of C++ types that may show up in (and must be handled
// by) TClassEdit::ShortType(). This flag is used to keep track of where that happens so that, if in the future this
// code needs to be used elsewhere, we may toggle it off.
static constexpr bool kAcceptExtendedSyntax = true;

static constexpr std::size_t kNumFixeds = kLastFixed - kFirstFixed + 1;
static const std::size_t kNumKeywords = kFirstNonKeyword - kFirstFixed;

// NOTE: must be in the same order as ETokType.
// Strings with the same prefixes must come in order from longest to shortest.
static const char *const kFixeds[] = {"const",  "volatile", "not",    "and",   "or",   "bitand",   "bitor",
                                      "xor",    "class",    "struct", "union", "enum", "typename", "unsigned",
                                      "signed", "long",     "short",  "&&",    "||",   "&",        "|",
                                      "^",      "~",        "++",     "--",    "->",   "+",        "-",
                                      "*",      "/",        "%",      "::",    "<<",   ">>",       "<=>",
                                      "<=",     ">=",       "<",      ">",     "==",   "!=",       "!",
                                      ",",      "...",      ".",      "(",     ")",    "[",        "]"};
static_assert(std::size(kFixeds) == kNumFixeds);

static bool IsDigit(char ch)
{
   return ch >= '0' && ch <= '9';
}

bool TLexer::IsStartOfNumber(std::size_t pos) const
{
   char ch = fSrc[pos];
   if (IsDigit(ch))
      return true;

   if (ch == '.' && pos < fSrc.size() - 1) {
      char nxt = fSrc[pos + 1];
      return IsDigit(nxt);
   }
   return false;
}

static bool IsPartOfNumber(char ch)
{
   if (IsDigit(ch))
      return true;

   // make uppercase
   ch -= (ch >= 'a' && ch <= 'z') * ('a' - 'A');
   return ch == '.' || ch == 'F' || ch == 'E' || ch == 'X' || ch == 'P' || ch == 'O' || ch == 'U' || ch == 'L';
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

TToken TLexer::PeekInternal(int flags)
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
      // NOTE: we check the latest token so we avoid returning "number" in cases like `x.y`
      if (fLatestToken.fType != kIdent && IsStartOfNumber(fCur)) {
         // NOTE: we don't really decode or validate the number, we simply skip ahead until the string looks like one.
         TToken token = {kNumber};
         token.fStr = ch;
         while (cur < srcSize && IsPartOfNumber(fSrc[cur])) {
            token.fStr += fSrc[cur];
            ++cur;
         }
         fNext = cur;
         return token;
      }

      if (cur > srcSize)
         break;

      // type-parameter
      if (kAcceptExtendedSyntax) {
         constexpr auto typeParLen = std::char_traits<char>::length("type-parameter-");
         if (strncmp("type-parameter-", fSrc.data() + fCur, typeParLen) == 0) {
            auto start = fCur;
            cur += typeParLen;
            while (cur < srcSize && (IsDigit(fSrc[cur]) || fSrc[cur] == '-'))
               ++cur;
            TToken tok;
            tok.fType = kTypeParam;
            tok.fStr = fSrc.substr(start, cur - start);
            fNext = cur;
            return tok;
         }
      }

      // fixed
      int fixedIdx = PeekFixed(cur - 1);
      while (fixedIdx >= 0) {
         const ETokType tokType = static_cast<ETokType>(kFirstFixed + fixedIdx);
         const bool mustSkipShiftRight = ((flags & kPeekForceSplitGt) && tokType == kShiftRight);
         const char *keyword = kFixeds[fixedIdx];
         auto kwLen = strlen(keyword);
         const auto endPos = cur - 1 + kwLen;
         // For keyword tokens, check if it ends properly (e.g. "constf" should be an ident, not keyword "const").
         const auto terminatesProperly =
            (endPos == srcSize ||
             (endPos < srcSize && (fixedIdx >= (int)kFirstNonKeyword || IsWordTerminator(endPos))));
         if (!mustSkipShiftRight && terminatesProperly) {
            cur += kwLen - 1;
            fNext = cur;
            TToken tok;
            tok.fType = tokType;
            return tok;
         }
         // Try again: maybe that was a keyword with matching prefix but there is a valid one later.
         fixedIdx = PeekFixed(cur - 1, fixedIdx + 1);
      }

      // identifier
      while (cur < srcSize) {
         if (IsWordTerminator(cur))
            break;
         ++cur;
      }

      std::size_t identSize = cur - wordStart;
      TToken tok = TToken::Ident({fSrc.data() + wordStart, identSize});

      fNext = cur;
      return tok;

   } while (cur < srcSize);

   assert(cur == srcSize);
   fNext = cur;
   return {kEOF};
}

TToken TLexer::Peek(int flags)
{
   fLatestToken = PeekInternal(flags);
   return fLatestToken;
}

void TLexer::Consume()
{
   fPrev[1] = fPrev[0];
   fPrev[0] = fCur;
   fCur = fNext;
}

void TLexer::Rewind()
{
   fNext = fCur;
   fCur = fPrev[0];
   fPrev[0] = fPrev[1];
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

TToken TToken::Ident(std::string_view str)
{
   TToken tok = {kIdent};
   tok.fStr = str;
   return tok;
}

TToken TToken::Char(char ch)
{
   TToken tok = {kCharacter};
   tok.fStr = ch;
   return tok;
}

TToken TToken::String(std::string_view str)
{
   TToken tok = {kString};
   tok.fStr = str;
   return tok;
}

TToken TToken::Number(std::string_view str)
{
   TToken tok = {kNumber};
   tok.fStr = str;
   return tok;
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

TToken TToken::TypeParam(std::string_view str)
{
   TToken tok = {kTypeParam};
   tok.fStr = str;
   return tok;
}

bool operator==(const TToken &a, const TToken &b)
{
   if (a.fType != b.fType)
      return false;
   if (a.fType == kIdent || a.fType == kNumber || a.fType == kString || a.fType == kCharacter)
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
   else if (t.fType == kEOF)
      out << "(EOF)";
   return out;
}

void PrintTo(const TToken &t, std::ostream *os)
{
   if (os)
      *os << t;
}

std::string TToken::ToString() const
{
   std::stringstream ss;
   ss << *this;
   return ss.str();
}

TNode *TNodeTree::PushNode(TNode::ENodeType type)
{
   auto &newNode = fNodes.emplace_back();
   newNode.fNodeType = type;
   return &newNode;
}

void TNodeTree::AddChild(TNode *parent, TNode *newChild)
{
   newChild->fParent = parent;
   if (parent) {
      TNode *child = parent->fFirstChild;
      if (!child)
         parent->fFirstChild = newChild;
      else {
         while (child->fNextSibling)
            child = child->fNextSibling;
         child->fNextSibling = newChild;
      }
      ++parent->fNumChildren;
   }
}

void TNodeTree::WrapNode(TNode *const node)
{
   TNode wrapped = *node; // copy the node to wrap
   node->fType = {};
   node->fExpr = {};
   node->fFlags = 0;
   auto &newNode = fNodes.emplace_back(wrapped);

   // Adjust links
   for (TNode *child = node->fFirstChild; child; child = child->fNextSibling)
      child->fParent = &newNode;
   newNode.fNumChildren = node->fNumChildren;
   newNode.fParent = node;
   newNode.fNextSibling = nullptr;
   newNode.fFirstChild = node->fFirstChild;
   node->fFirstChild = &newNode;
   node->fNumChildren = 1;
}

static TType::ETypeFlags CvOrModifierKeywordToTypeFlag(ETokType type)
{
   switch (type) {
   case kKwUnsigned: return TType::kUnsigned;
   case kKwSigned: return TType::kSigned;
   case kKwLong: return TType::kLong;
   case kKwShort: return TType::kShort;
   case kKwConst: return TType::kConst;
   case kKwVolatile: return TType::kVolatile;
   default: return TType::kNone;
   }
}

static void ParseCvAndModifiers(TLexer &lex, TType &type, bool onlyCv = false)
{
   TToken tok = lex.Peek();
   auto flag = CvOrModifierKeywordToTypeFlag(tok.fType);
   while (flag) {
      if (onlyCv && !(flag & (TType::kConst | TType::kVolatile)))
         return;

      if ((type.fFlags & TType::kLong) && flag == TType::kLong)
         type.fFlags |= TType::kLongLong;
      type.fFlags |= flag;
      lex.Consume();
      tok = lex.Peek();
      flag = CvOrModifierKeywordToTypeFlag(tok.fType);
   }
}

static void ParseCvList(TLexer &lex, TType &type)
{
   ParseCvAndModifiers(lex, type, true);
}

static std::string TypeFlagsToKeywords(int flags)
{
   std::string out;

   if (flags & TType::kConst)
      out += "const ";
   if (flags & TType::kVolatile)
      out += "volatile ";
   if (flags & TType::kSigned)
      out += "signed ";
   if (flags & TType::kUnsigned)
      out += "unsigned ";
   if (flags & TType::kLong)
      out += "long ";
   if (flags & TType::kLongLong) // LongLong implies Long, so we only print another "long"
      out += "long ";
   if (flags & TType::kShort)
      out += "short ";

   return out;
}

static void ParseNamespace(TLexer &lex, TType &type)
{
   TToken tok = lex.Peek();
   if (tok.fType == kIdent) {
      lex.Consume();
      if (lex.Peek().fType == kColonColon) {
         type.fNamespace += tok.fStr;
         assert(type.fNamespace == ROOT::Trim(type.fNamespace)); // we should not have leading or trailing whitespaces
         tok = lex.Peek();
      } else {
         lex.Rewind();
      }
   }

   if (tok.fType == kColonColon) {
      type.fNamespace += "::";
      lex.Consume();
      ParseNamespace(lex, type);
   }
}

static void ParseTypeSpecifier(TLexer &lex, TNode &type)
{
   (void)type;

   TToken tok = lex.Peek();
   if (tok.fType == kKwClass || tok.fType == kKwStruct || tok.fType == kKwEnum || tok.fType == kKwTypename) {
      // We don't really care about the class/struct/enum specifier, so just eat it and go on.
      lex.Consume();
   }
}

static bool IsUnaryOp(ETokType type)
{
   return type == kKwNot || type == kAnd || type == kTilde || type == kPlusPlus || type == kMinusMinus ||
          type == kPlus || type == kMinus || type == kStar || type == kNot;
}

static const char *FixedToStr(ETokType type)
{
   if (type >= kFirstFixed && type <= kLastFixed) {
      return kFixeds[type - kFirstFixed];
   }
   return "";
}

static int GetBinOpPrecedence(ETokType op)
{
   // From https://en.cppreference.com/w/cpp/language/operator_precedence.html
   // clang-format off
   switch (op) {
   case kArrow:
   case kPeriod:
   case kOpenSquare:
      return 2;
   case kStar:
   case kSlash:
   case kPercent:
      return 5;
   case kPlus:
   case kMinus:
      return 6;
   case kShiftLeft:
   case kShiftRight:
      return 7;
   case kSpaceship:
      return 8;
   case kLe:
   case kGe:
   case kLt:
   case kGt:
      return 9;
   case kEq:
   case kNe:
      return 10;
   case kAnd:
   case kKwBitand:
      return 11;
   case kXor:
   case kKwXor:
      return 12;
   case kOr:
   case kKwBitor:
      return 13;
   case kAndAnd:
   case kKwAnd:
      return 14;
   case kOrOr:
   case kKwOr:
      return 15;
   default:
      return 0;
   }
   // clang-format on
}

constexpr int kHighestPrecedence = 1;
constexpr int kLowestPrecedence = 100;

static TNode *ParseExpr(TLexer &lex, TNodeTree &tree, const TNode *parent, int minPrecedence);
static TNode *ParseLeaf(TLexer &lex, TNodeTree &tree);

static bool IsInsideParensExpr(const TNode *parent)
{
   while (parent) {
      if (parent->fNodeType != TNode::kExpr)
         break;
      if (parent->fExpr.fType == TExpr::kParens)
         return true;
      parent = parent->fParent;
   }
   return false;
}

static TNode *
ParseExprIncreasingPrecedence(TLexer &lex, TNodeTree &tree, TNode *left, const TNode *parent, int minPrecedence)
{
   TToken tok = lex.Peek();
   int precedence = GetBinOpPrecedence(tok.fType);
   // Note: "precedence < highest" means it was not a BinOp
   if (precedence < kHighestPrecedence || precedence > minPrecedence)
      return left;

   // Kinda workaround for treating '>' and '>>' as an operator vs a close template.
   // We currently treat it as an operator if we're inside a parentheses operation.
   const bool isActuallyBinOp = ((tok.fType != kGt && tok.fType != kShiftRight) || IsInsideParensExpr(parent));
   if (!isActuallyBinOp)
      return left;

   lex.Consume();

   // Found a binary operator: turn the current expression into its left-hand side.
   TNode *binopExpr = tree.PushNode(TNode::kExpr);
   binopExpr->fExpr.fType = TExpr::kBinOp;
   binopExpr->fExpr.fStr = FixedToStr(tok.fType);

   // parse right-hand side
   const bool isArraySub = tok.fType == kOpenSquare;
   TNode *right = ParseExpr(lex, tree, binopExpr, isArraySub ? kLowestPrecedence : precedence);
   if (!right) {
      tree.fErrors.push_back("failed to parse right-hand side of binary op '" + binopExpr->fExpr.fStr + "'");
      return nullptr;
   }

   if (isArraySub) {
      tok = lex.Peek();
      if (tok.fType != kCloseSquare) {
         tree.fErrors.push_back("unterminated array expression");
         return nullptr;
      }
      lex.Consume();
      tok = lex.Peek();
   }

   tree.AddChild(binopExpr, left);
   tree.AddChild(binopExpr, right);

   return binopExpr;
}

static TNode *ParseLeaf(TLexer &lex, TNodeTree &tree)
{
   TNode *expr = nullptr;
   TToken tok = lex.Peek();

   // We consider the unary operator as having the lowest possible precedence.
   if (IsUnaryOp(tok.fType)) {
      lex.Consume();
      expr = tree.PushNode(TNode::kExpr);
      expr->fExpr.fStr = FixedToStr(tok.fType);
      expr->fExpr.fType = TExpr::kUnaryOp;
      TNode *inner = ParseExpr(lex, tree, expr, kLowestPrecedence);
      if (!inner) {
         tree.fErrors.push_back("failed to parse inner expression of unary op '" + expr->fExpr.fStr + "'");
         return nullptr;
      }
      tree.AddChild(expr, inner);
   } else if (tok.fType == kOpenRound) {
      lex.Consume();
      expr = tree.PushNode(TNode::kExpr);
      expr->fExpr.fType = TExpr::kParens;
      if (lex.Peek().fType != kCloseRound) {
         TNode *inner = ParseExpr(lex, tree, expr, kLowestPrecedence);
         if (!inner) {
            tree.fErrors.push_back("failed to parse inner expression of parens expression");
            return nullptr;
         }
         tree.AddChild(expr, inner);
      }
      tok = lex.Peek();
      if (tok.fType != kCloseRound) {
         tree.fErrors.push_back("unterminated parens expression");
         return nullptr;
      }
      lex.Consume();
   } else if (tok.fType == kNumber || tok.fType == kString || tok.fType == kCharacter || tok.fType == kIdent) {
      lex.Consume();
      expr = tree.PushNode(TNode::kExpr);
      expr->fExpr.fType = TExpr::kLeaf;
      expr->fExpr.fStr = tok.fStr;
   }

   return expr;
}

static TNode *ParseExpr(TLexer &lex, TNodeTree &tree, const TNode *parent, int minPrecedence)
{
   TToken tok = lex.Peek();

   TNode *left = ParseLeaf(lex, tree);
   if (!left)
      return nullptr;

   while (left) {
      TNode *node = ParseExprIncreasingPrecedence(lex, tree, left, parent, minPrecedence);
      if (node == left)
         break;
      left = node;
   }

   return left;
}

static TNode *ParseTypeInternal(TLexer &lex, TNodeTree &tree);

static bool ParseTemplate(TLexer &lex, TNodeTree &tree, TNode &parentType)
{
   assert(parentType.fNodeType == TNode::kType);

   TToken tok = lex.Peek();
   if (tok.fType == kLt) {
      lex.Consume();

      // Mark this class as templated so we know it even if we end up pushing no children (which can happen if
      // it's something like Foo<>)
      parentType.fType.fFlags |= TType::kTemplated;

      // Find out if we're pushing a type or an expression
      tok = lex.Peek(TLexer::kPeekForceSplitGt);

      while (tok.fType != kGt) {
         TNode::ENodeType childType = TNode::kType;
         if (IsUnaryOp(tok.fType) || tok.fType == kOpenRound || tok.fType == kNumber || tok.fType == kString ||
             tok.fType == kCharacter) {
            childType = TNode::kExpr;
         }
         // special case: check if this is an array expression.
         // We consider it an expression rather than an array type depending on whether the bracket closes immediately
         // (type) or not (expr). So in `T<v[1]>` v[1] is an expression but in `T<v[]>` v[] is a type.
         if (tok.fType == kIdent) {
            lex.Consume();
            tok = lex.Peek();
            if (tok.fType == kOpenSquare) {
               lex.Consume();
               tok = lex.Peek();
               if (tok.fType != kCloseSquare)
                  childType = TNode::kExpr;
               lex.Rewind();
            }
            lex.Rewind();
            tok = lex.Peek();
         }

         TNode *newChild = nullptr;
         if (childType == TNode::kType) {
            newChild = ParseTypeInternal(lex, tree);
         } else {
            newChild = ParseExpr(lex, tree, &parentType, kLowestPrecedence);
         }
         if (!newChild)
            return false;

         tok = lex.Peek(TLexer::kPeekForceSplitGt);
         if (tok.fType == kEllipsis) {
            newChild->fFlags |= TNode::kEllipsis;
            lex.Consume();
            tok = lex.Peek(TLexer::kPeekForceSplitGt);
         }

         tree.AddChild(&parentType, newChild);

         lex.Consume();
         if (tok.fType == kComma) {
            tok = lex.Peek();
         }
      }

      lex.Consume();
   }

   return true;
}

static TType::EIndirection TokTypeToIndirection(ETokType type)
{
   switch (type) {
   case kStar: return TType::EIndirection::kPtr;
   case kAnd: return TType::EIndirection::kRef;
   case kAndAnd: return TType::EIndirection::kRvRef;
   default: assert(false); return TType::EIndirection::kNone;
   }
}

static void ParseRefsAndPtrs(TLexer &lex, TNodeTree &tree, TNode *type)
{
   assert(type->fNodeType == TNode::kType);

   TToken tok = lex.Peek();
   while (tok.fType == kAnd || tok.fType == kAndAnd || tok.fType == kStar) {
      // When we find a ptr or ref we wrap the current node in another node that represents the indirection.
      tree.WrapNode(type);
      type->fType.fIndirection = TokTypeToIndirection(tok.fType);

      lex.Consume();

      // Note that we do NOT push the wrapped node as the new latest node until we're done with parsing ptrs and
      // refs, as each new ptr/ref refers to the outermost node of the new hierarchy.
      ParseCvAndModifiers(lex, type->fType);

      tok = lex.Peek();
   }
}

static bool ParseTypeName(TLexer &lex, TNodeTree &tree, TNode *type)
{
   assert(type->fNodeType == TNode::kType);

   TToken tok = lex.Peek();
   if (tok.fType != kIdent && tok.fType != kTypeParam) {
      // type name might be omitted if we found some modifiers (e.g. "short").
      // In that case, transform the modifiers into the actual type name.
      if (!(type->fType.fFlags & TType::kModifiersMask)) {
         tree.fErrors.push_back("expected type name, found " + tok.ToString());
         return false;
      }
   } else {
      type->fType.fName = tok.fStr;
      lex.Consume();
   }
   return true;
}

static bool ParseTypeArray(TLexer &lex, TNodeTree &tree, TNode *type)
{
   TToken tok = lex.Peek();
   while (tok.fType == kOpenSquare) {
      lex.Consume();
      tok = lex.Peek();

      tree.WrapNode(type);
      type->fType.fIndirection = TType::EIndirection::kArray;

      if (tok.fType != kCloseSquare) {
         // Parse whatever is inside the brackets
         TNode *expr = ParseExpr(lex, tree, type, kLowestPrecedence);
         if (!expr) {
            tree.fErrors.push_back("failed to parse expression inside array");
            return false;
         }
         tree.AddChild(type, expr);
         tok = lex.Peek();
      }

      if (tok.fType != kCloseSquare) {
         tree.fErrors.push_back("unterminated array after type `" + type->fType.fName + "`");
         return false;
      }
      lex.Consume();
      tok = lex.Peek();
   }

   return true;
}

static bool ParseFunctionPtr(TLexer &lex, TNodeTree &tree, TNode *&type)
{
   TToken tok = lex.Peek();
   if (tok.fType != kOpenRound)
      return true;

   lex.Consume();
   tok = lex.Peek();

   if (tok.fType != kStar) {
      tree.fErrors.push_back("expected '*' in function pointer");
      return false;
   }

   // The current type becomes the return type of the function pointer
   tree.WrapNode(type);
   type->fType.fIndirection = TType::EIndirection::kFuncPtr;

   lex.Consume();
   tok = lex.Peek();
   // handle pointers, refs and arrays of function pointers.
   // We don't do much semantic validation here, so we will happily parse illegal ref-to-ref, ptr-to-ref and so on.
   while (tok.fType == kStar || tok.fType == kAnd || tok.fType == kAndAnd) {
      lex.Consume();
      tree.WrapNode(type);
      type->fType.fIndirection = TokTypeToIndirection(tok.fType);
      tok = lex.Peek();
   }
   ParseTypeArray(lex, tree, type);
   // NOTE: if we find any indirection, we still need to finish parsing the function pointer, so pop back to it.
   while (type->fType.fIndirection != TType::EIndirection::kFuncPtr)
      type = type->fFirstChild;

   tok = lex.Peek();
   if (tok.fType != kCloseRound) {
      tree.fErrors.push_back("expected ')' in function pointer");
      return false;
   }

   lex.Consume();
   tok = lex.Peek();
   if (tok.fType != kOpenRound) {
      tree.fErrors.push_back("expected '(' in function pointer");
      return false;
   }

   lex.Consume();
   tok = lex.Peek();
   while (tok.fType != kCloseRound) {
      TNode *arg = ParseTypeInternal(lex, tree);
      if (!arg) {
         tree.fErrors.push_back("failed to parse argument of function pointer");
         return false;
      }

      tok = lex.Peek();
      if (kAcceptExtendedSyntax && tok.fType == kEllipsis) {
         arg->fFlags |= TNode::kEllipsis;
         lex.Consume();
         tok = lex.Peek();
      }

      tree.AddChild(type, arg);

      if (tok.fType == kComma) {
         lex.Consume();
         tok = lex.Peek();
      }
   }

   lex.Consume();

   return true;
}

static bool ParseUnqualifiedType(TLexer &lex, TNodeTree &tree, TNode *type);

static bool ParseScopedType(TLexer &lex, TNodeTree &tree, TNode *type)
{
   TToken tok = lex.Peek();
   while (tok.fType == kColonColon) {
      lex.Consume();

      // If we have an inner type, the outer type gets "frozen" and all further modifications apply to the inner type.
      // To do this we wrap the outer type into the inner type and go on.
      // Note that in our node tree the outer type is the *child* of the inner type rather than the other way around.
      const auto specifiers = type->fType.fFlags & (TType::kCvMask | TType::kModifiersMask);
      tree.WrapNode(type);
      type->fFlags |= TNode::kScoped;
      // All parsed type specifiers actually belong to the inner type, so move them over.
      type->fType.fFlags = specifiers;
      type->fFirstChild->fType.fFlags &= ~(TType::kCvMask | TType::kModifiersMask);

      if (!ParseUnqualifiedType(lex, tree, type))
         return false;

      tok = lex.Peek();
   }
   return true;
}

static bool ParseUnqualifiedType(TLexer &lex, TNodeTree &tree, TNode *type)
{
   if (!ParseTypeName(lex, tree, type))
      return false;

   if (!ParseTemplate(lex, tree, *type))
      return false;

   // Parse SuperType::SubType construct (also works for static variables etc)
   if (!ParseScopedType(lex, tree, type))
      return false;

   return true;
}

static TNode *ParseTypeInternal(TLexer &lex, TNodeTree &tree)
{
   TNode *type = tree.PushNode(TNode::kType);

   // Parse const/volatile/unsigned/short/long/etc (they can be in any order)
   ParseCvAndModifiers(lex, type->fType);
   ParseNamespace(lex, type->fType);
   // Parse "class", "struct" and "enum" keywords
   ParseTypeSpecifier(lex, *type);
   // Parse cv list again to handle weird spellings like "class const Foo"
   ParseCvList(lex, type->fType);

   if (!ParseUnqualifiedType(lex, tree, type))
      return nullptr;

   // cv/modifiers may come before or after the type, so parse them again
   ParseCvAndModifiers(lex, type->fType);
   ParseRefsAndPtrs(lex, tree, type);

   ParseFunctionPtr(lex, tree, type);
   ParseTypeArray(lex, tree, type);

   return type;
}

TNodeTree ParseType(std::string_view src)
{
   TNodeTree res;

   TLexer lex{src};

   ParseTypeInternal(lex, res);

   // We should have parsed all tokens
   if (lex.Peek().fType != kEOF)
      res.fErrors.push_back("trailing tokens after type: `" + std::string(lex.fSrc.substr(lex.fCur)) + "`");

   return res;
}

// flags is a bitmask of EPrintFlags
static void PrintTypeNode(std::ostream &out, const TNode &node, int flags)
{
   assert(node.fNodeType == TNode::kType);

   if (node.fType.fIndirection == TType::EIndirection::kNone) {
      int nodeFlags = node.fType.fFlags;
      if (flags & kStripCV)
         nodeFlags &= ~(TType::kConst | TType::kVolatile);

      out << TypeFlagsToKeywords(nodeFlags);
      if (node.fType.fName.empty() && (node.fType.fFlags & TType::kModifiersMask)) {
         // Remove extra space if we have no explicit type name (so we print "short" and not "short ")
         out.seekp(-1, out.cur);
      }

      if (!(flags & kStripNamespace))
         out << node.fType.fNamespace;

      out << node.fType.fName;
   }

   // Note that a templated type might have no children
   if (node.fType.fFlags & TType::kTemplated)
      out << '<';

   // If this is a Scoped node we already printed its first child, so skip it.
   TNode *const firstChild = (node.fFlags & TNode::kScoped) ? node.fFirstChild->fNextSibling : node.fFirstChild;

   for (TNode *child = firstChild; child; child = child->fNextSibling) {
      PrintNode(out, *child, flags);
      // In case of Array indirection nodes, the second node (if present) is the expression
      // inside the brackets, so we want to print it later.
      if (node.fType.fIndirection == TType::EIndirection::kArray)
         break;
      // In case of FuncPtr indirection nodes, all children after the first are the
      // arguments of the function.
      if (node.fType.fIndirection == TType::EIndirection::kFuncPtr)
         break;
      if (child->fNextSibling)
         out << ',';
   }

   if (node.fType.fFlags & TType::kTemplated) {
      out << '>';
      // Put a space after the '>' only if we're about to close another template
      if ((flags & kSpaceAfterClosingTemplate) && !node.fNextSibling && node.fParent &&
          node.fParent->fNodeType == TNode::kType && node.fParent->fType.fIndirection == TType::EIndirection::kNone &&
          !(node.fParent->fFlags & TNode::kScoped))
         out << ' ';
   }

   if (node.fType.fIndirection == TType::EIndirection::kFuncPtr) {
      out << "(*";
   }

   if (node.fType.fIndirection != TType::EIndirection::kNone) {
      if (!(flags & kStripRefs) && node.fType.fIndirection == TType::EIndirection::kRef)
         out << "&";
      if (!(flags & kStripRefs) && node.fType.fIndirection == TType::EIndirection::kRvRef)
         out << "&&";
      if (!(flags & kStripPointers) && node.fType.fIndirection == TType::EIndirection::kPtr)
         out << "*";
      if (node.fType.fIndirection == TType::EIndirection::kArray) {
         out << "[";
         if (node.fNumChildren > 1) {
            PrintNode(out, *firstChild->fNextSibling, flags);
         }
         out << "]";
      }

      // If we are the outermost indirection of a function pointer, terminate the type by appending the arguments.
      const TNode *fnPtr = &node;
      while (fnPtr &&
             !(fnPtr->fNodeType == TNode::kType && fnPtr->fType.fIndirection == TType::EIndirection::kFuncPtr)) {
         bool isScoped = fnPtr->fFlags & TNode::kScoped;
         fnPtr = fnPtr->fFirstChild;
         if (isScoped)
            fnPtr = fnPtr->fNextSibling;
      }
      if (fnPtr) {
         assert(!(fnPtr->fFlags & TNode::kScoped));

         // found the function pointer descendant, now check if we're the outermost indirection.
         bool isOutermost = !node.fParent || node.fParent->fNodeType != TNode::kType;
         if (!isOutermost) {
            auto parInd = node.fParent->fType.fIndirection;
            isOutermost = parInd != TType::EIndirection::kArray && parInd != TType::EIndirection::kRef &&
                          parInd != TType::EIndirection::kPtr && parInd != TType::EIndirection::kRvRef;
         }
         if (isOutermost) {
            out << ")(";
            for (TNode *arg = fnPtr->fFirstChild->fNextSibling; arg; arg = arg->fNextSibling) {
               PrintNode(out, *arg, flags);
               if (arg->fNextSibling)
                  out << ",";
            }
            out << ")";
         }
      }

      if (!(flags & kStripCV) && node.fType.fFlags & TType::kConst)
         out << "const";
      if (!(flags & kStripCV) && node.fType.fFlags & TType::kVolatile)
         out << "volatile";
   }
}

static void PrintExprNode(std::ostream &out, const TNode &node, int flags)
{
   assert(node.fNodeType == TNode::kExpr);

   switch (node.fExpr.fType) {
   case TExpr::kLeaf:
      assert(node.fNumChildren == 0);
      out << node.fExpr.fStr;
      break;

   case TExpr::kUnaryOp:
      assert(node.fNumChildren == 1);

      out << node.fExpr.fStr;
      PrintNode(out, *node.fFirstChild, flags);
      break;

   case TExpr::kBinOp:
      assert(node.fNumChildren == 2);

      PrintNode(out, *node.fFirstChild, flags);
      out << node.fExpr.fStr;
      PrintNode(out, *node.fFirstChild->fNextSibling, flags);
      // Special case: array subscript
      if (node.fExpr.fStr == "[")
         out << ']';
      break;

   case TExpr::kParens:
      out << '(';
      for (TNode *child = node.fFirstChild; child; child = child->fNextSibling) {
         PrintNode(out, *child, flags);
      }
      out << ')';
      break;

   default: assert(false);
   }
}

void PrintNode(std::ostream &out, const TNode &node, int flags)
{
   if (node.fFlags & TNode::kScoped) {
      assert(node.fFirstChild);
      PrintNode(out, *node.fFirstChild, flags);
      out << "::";
   }

   if (node.fNodeType == TNode::kType) {
      PrintTypeNode(out, node, flags);
   } else {
      PrintExprNode(out, node, flags);
   }
   if (node.fFlags & TNode::kEllipsis)
      out << "...";
}

std::string StringifyNode(const TNode &node, int flags)
{
   std::stringstream ss;
   PrintNode(ss, node, flags);
   return ss.str();
}

void TNodeTree::Print(std::ostream &out, int flags) const
{
   if (fNodes.empty())
      return;

   PrintNode(out, fNodes[0], flags);
}

static void PrintNodeDebug(std::ostream &out, const TNode &node, int indent)
{
   for (int i = 0; i < indent; ++i)
      out << ' ';

   if (node.fNodeType == TNode::kType) {
      if (node.fType.fIndirection == TType::EIndirection::kNone)
         out << "Type " << node.fType.fNamespace << node.fType.fName;
      else {
         PrintTo(node.fType.fIndirection, &out);
         out << " to:";
      }
      if (node.fType.fFlags & TType::kTemplated)
         out << "<>";
   } else {
      out << node.fExpr.fType << " Expr: ";
      out << node.fExpr.fStr;
   }
   out << "\n";

   for (TNode *child = node.fFirstChild; child; child = child->fNextSibling)
      PrintNodeDebug(out, *child, indent + 2);
}

void TNodeTree::PrintTreeDebug(std::ostream &out) const
{
   if (fNodes.empty())
      return;

   PrintNodeDebug(out, fNodes[0], 0);
}

ROOT::RResult<std::string> ShortType(std::string_view typeDesc)
{
   auto tree = ParseType(typeDesc);
   if (!tree.fErrors.empty()) {
      return R__FAIL("Failed to parse type `" + std::string(typeDesc) + "`: " + ROOT::Join("\n", tree.fErrors));
   }

   std::stringstream ss;
   tree.Print(ss, kStripCV);
   return ss.str();
}

void PrintTo(const TType::EIndirection &t, std::ostream *os)
{
   switch (t) {
   case TType::EIndirection::kNone: *os << "None"; return;
   case TType::EIndirection::kRef: *os << "Ref"; return;
   case TType::EIndirection::kPtr: *os << "Ptr"; return;
   case TType::EIndirection::kRvRef: *os << "RvRef"; return;
   case TType::EIndirection::kFuncPtr: *os << "FuncPtr"; return;
   case TType::EIndirection::kArray: *os << "Array"; return;
   }
}

std::ostream &operator<<(std::ostream &out, TExpr::EType type)
{
   switch (type) {
   case TExpr::kLeaf: out << "Leaf"; return out;
   case TExpr::kUnaryOp: out << "UnaryOp"; return out;
   case TExpr::kBinOp: out << "BinOp"; return out;
   case TExpr::kParens: out << "Parens"; return out;
   default: assert(false); return out;
   }
}

TNode *TNode::LastChild() const
{
   TNode *child = fFirstChild;
   while (child && child->fNextSibling) {
      child = child->fNextSibling;
   }
   return child;
}

void TNode::DropLastChild()
{
   if (fNumChildren < 2) {
      fFirstChild = nullptr;
      fNumChildren = 0;
      return;
   }

   TNode *child = fFirstChild;
   assert(child && child->fNextSibling);
   while (child->fNextSibling->fNextSibling) {
      child = child->fNextSibling;
   }
   child->fNextSibling = nullptr;
   --fNumChildren;
}

} // namespace ROOT::Internal::TypeParsing
