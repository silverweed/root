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

static constexpr std::size_t kNumFixeds = kLastFixed - kFirstFixed + 1;
static const std::size_t kNumKeywords = kFirstNonKeyword - kFirstFixed;

// NOTE: must be in the same order as ETokType.
// Strings with the same prefixes must come in order from longest to shortest.
static const char *const kFixeds[] = {
   "const", "volatile", "not",    "and", "or", "bitand", "bitor", "xor", "class", "struct",
   "union", "enum",     "sizeof", "&&",  "||", "&",      "|",     "^",   "~",     "++",
   "--",    "->",       "+",      "-",   "*",  "/",      "::",    "<=",  ">=",    "<",
   ">",     "==",       "!=",     "!",   ",",  ".",      "(",     ")",   "[",     "]"};
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

TToken TLexer::PeekInternal()
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
      if (fLatestToken.fType != kIdent && IsStartOfNumber(ch)) {
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

TToken TLexer::Peek()
{
   fLatestToken = PeekInternal();
   return fLatestToken;
}

void TLexer::Consume()
{
   fPrev = fCur;
   fCur = fNext;
}

void TLexer::Rewind()
{
   fNext = fCur;
   fCur = fPrev;
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
   }
}

void TNodeTree::WrapNode(TNode *&node)
{
   TNode wrapped = *node; // copy the node to wrap
   node->fType = {};
   node->fExpr = {};
   auto &newNode = fNodes.emplace_back(wrapped);
   // Adjust links
   for (TNode *child = node->fFirstChild; child; child = child->fNextSibling)
      child->fParent = &newNode;
   newNode.fParent = node;
   newNode.fNextSibling = nullptr;
   newNode.fFirstChild = node->fFirstChild;
   node->fFirstChild = &newNode;
}

static void ParseCvList(TLexer &lex, TType &type)
{
   TToken tok = lex.Peek();
   while (tok.fType == kKwConst || tok.fType == kKwVolatile) {
      type.fQual |= (tok.fType == kKwConst) ? TType::kConst : TType::kVolatile;
      lex.Consume();
      tok = lex.Peek();
   }
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
   if (tok.fType == kKwClass || tok.fType == kKwStruct || tok.fType == kKwEnum) {
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
      return 5;
   case kPlus:
   case kMinus:
      return 6;
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

static TNode *
ParseExprIncreasingPrecedence(TLexer &lex, TNodeTree &tree, TNode *left, const TNode *parent, int minPrecedence)
{
   TToken tok = lex.Peek();
   int precedence = GetBinOpPrecedence(tok.fType);
   // Note: "precedence < highest" means it was not a BinOp
   if (precedence < kHighestPrecedence || precedence > minPrecedence)
      return left;

   // Kinda workaround for treating '>' as an operator vs a close template.
   // We currently treat it as an operator if we're in the middle of a parentheses operation
   const bool isActuallyBinOp = (tok.fType != kGt || (parent && parent->fNodeType == TNode::kExpr));
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
   // (Note that we don't really care about its precedence).
   if (IsUnaryOp(tok.fType)) {
      lex.Consume();
      expr = tree.PushNode(TNode::kExpr);
      expr->fExpr.fStr = FixedToStr(tok.fType);
      expr->fExpr.fType = TExpr::kUnaryOp;
      TNode *inner = ParseExpr(lex, tree, expr, kLowestPrecedence);
      tree.AddChild(expr, inner);
   } else if (tok.fType == kOpenRound) {
      lex.Consume();
      expr = tree.PushNode(TNode::kExpr);
      expr->fExpr.fType = TExpr::kParens;
      if (lex.Peek().fType != kCloseRound) {
         TNode *inner = ParseExpr(lex, tree, expr, kLowestPrecedence);
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

   // expr :: [unary-op] (number | string | ident) | "(" [expr] ")" | expr [binop] expr
   TNode *left = ParseLeaf(lex, tree);
   if (!left) {
      return nullptr;
   }

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
   TToken tok = lex.Peek();
   if (tok.fType == kLt) {
      lex.Consume();

      // Find out if we're pushing a type or an expression
      tok = lex.Peek();

      while (tok.fType != kGt) {
         TNode::ENodeType childType = TNode::kType;
         if (IsUnaryOp(tok.fType) || tok.fType == kOpenRound || tok.fType == kNumber || tok.fType == kString ||
             tok.fType == kCharacter) {
            childType = TNode::kExpr;
         }
         // special case: check if this is an array expression
         if (tok.fType == kIdent) {
            lex.Consume();
            tok = lex.Peek();
            if (tok.fType == kOpenSquare) {
               childType = TNode::kExpr;
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

         tree.AddChild(&parentType, newChild);

         tok = lex.Peek();
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
      ParseCvList(lex, type->fType);

      tok = lex.Peek();
   }
}

static TNode *ParseTypeInternal(TLexer &lex, TNodeTree &tree)
{
   // A type should look something like this:
   //
   // type :: [cv-list] [namespace] [elab-type-spec] ident [template] [cv-list] [refs-and-ptrs]
   // cv-list :: const | volatile
   // namespace :: [ident] "::" [namespace]
   // elab-type-spec :: class | struct | enum
   // refs-and-ptrs :: ("&" | "&&" | "*" [cv-list]) [refs-and-ptrs]
   // template :: "<" [types | exprs] ">"
   // types :: type ["," types]
   // exprs :: expr ["," exprs]
   // expr :: [unary-op] (number | string | ident) | "(" [exprs] ")" | expr [binop] expr
   TNode *type = tree.PushNode(TNode::kType);
   ParseCvList(lex, type->fType);
   ParseNamespace(lex, type->fType);
   ParseTypeSpecifier(lex, *type);

   // parse type name
   TToken tok = lex.Peek();
   if (tok.fType != kIdent) {
      tree.fErrors.push_back("expected type name, found " + tok.ToString());
      return nullptr;
   }
   type->fType.fName = tok.fStr;
   lex.Consume();

   if (!ParseTemplate(lex, tree, *type))
      return nullptr;

   ParseCvList(lex, type->fType);
   ParseRefsAndPtrs(lex, tree, type);

   return type;
}

TNodeTree ParseType(std::string_view src)
{
   // std::cout << src << "\n";

   TNodeTree res;

   TLexer lex{src};

   ParseTypeInternal(lex, res);

   // std::cout << "-------------\n";
   // res.PrintTreeDebug();
   // std::cout << "-------------\n";

   return res;
}

static void PrintNode(std::ostream &out, const TNode &node, int flags);

static void PrintTypeNode(std::ostream &out, const TNode &node, int flags)
{
   assert(node.fNodeType == TNode::kType);

   if (node.fType.fIndirection == TType::EIndirection::kNone) {
      if (!(flags & kStripCV)) {
         if (node.fType.fQual & TType::kConst)
            out << "const ";
         if (node.fType.fQual & TType::kVolatile)
            out << "volatile ";
      }
      if (!(flags & kStripNamespace))
         out << node.fType.fNamespace << node.fType.fName;
   }

   if (node.fFirstChild) {
      if (node.fType.fIndirection == TType::EIndirection::kNone)
         out << '<';

      for (TNode *child = node.fFirstChild; child; child = child->fNextSibling) {
         PrintNode(out, *child, flags);
         if (child->fNextSibling)
            out << ',';
      }

      if (node.fType.fIndirection == TType::EIndirection::kNone)
         out << '>';
   }

   if (node.fType.fIndirection != TType::EIndirection::kNone) {
      if (!(flags & kStripRefs) && node.fType.fIndirection == TType::EIndirection::kRef)
         out << "&";
      if (!(flags & kStripRefs) && node.fType.fIndirection == TType::EIndirection::kRvRef)
         out << "&&";
      if (!(flags & kStripPointers) && node.fType.fIndirection == TType::EIndirection::kPtr)
         out << "*";

      if (!(flags & kStripCV) && node.fType.fQual & TType::kConst)
         out << " const";
      if (!(flags & kStripCV) && node.fType.fQual & TType::kVolatile)
         out << " volatile";
   }
}

static void PrintExprNode(std::ostream &out, const TNode &node, int flags)
{
   assert(node.fNodeType == TNode::kExpr);

   switch (node.fExpr.fType) {
   case TExpr::kLeaf:
      assert(!node.fFirstChild);
      out << node.fExpr.fStr;
      break;

   case TExpr::kUnaryOp:
      assert(node.fFirstChild);
      assert(!node.fFirstChild->fNextSibling);

      out << node.fExpr.fStr;
      PrintExprNode(out, *node.fFirstChild, flags);
      break;

   case TExpr::kBinOp:
      assert(node.fFirstChild);
      assert(node.fFirstChild->fNextSibling);
      assert(!node.fFirstChild->fNextSibling->fNextSibling);

      PrintExprNode(out, *node.fFirstChild, flags);
      out << node.fExpr.fStr;
      PrintExprNode(out, *node.fFirstChild->fNextSibling, flags);
      // Special case: array subscript
      if (node.fExpr.fStr == "[")
         out << ']';
      break;

   case TExpr::kParens:
      out << '(';
      for (TNode *child = node.fFirstChild; child; child = child->fNextSibling) {
         PrintExprNode(out, *child, flags);
      }
      out << ')';
      break;

   default: assert(false);
   }
}

static void PrintNode(std::ostream &out, const TNode &node, int flags)
{
   if (node.fNodeType == TNode::kType) {
      PrintTypeNode(out, node, flags);
   } else {
      PrintExprNode(out, node, flags);
   }
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
         out << "Type " << node.fType.fNamespace << "::" << node.fType.fName;
      else {
         PrintTo(node.fType.fIndirection, &out);
         out << " to:";
      }
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
      return R__FAIL(ROOT::Join("\n", tree.fErrors));
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

} // namespace ROOT::Internal::TypeParsing
