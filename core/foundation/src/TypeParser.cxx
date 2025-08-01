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
   "const", "volatile", "not", "and", "or", "bitand", "bitor", "xor", "class", "struct", "union", "enum", "&&",
   "||",    "&",        "|",   "^",   "~",  "++",     "--",    "->",  "+",     "-",      "*",     "/",    "::",
   "<=",    ">=",       "<",   ">",   "==", "!=",     "!",     ",",   "(",     ")",      "[",     "]"};
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

void TNodeTree::PushNesting()
{
   assert(fNodes.size() > 0);
   fCurNode = &fNodes.back();
}

void TNodeTree::PopNesting()
{
   assert(fCurNode);
   fCurNode = fCurNode->fParent;
}

void TNodeTree::AddNode(TNode::ENodeType type)
{
   fNodes.emplace_back().fNodeType = type;

   auto &newNode = fNodes.back();
   newNode.fParent = fCurNode;

   if (!fCurNode)
      fCurNode = &newNode;
   else if (fCurNode->fFirstChild)
      fCurNode->fFirstChild->fNextSibling = &newNode;
   else
      fCurNode->fFirstChild = &newNode;
}

TType &TNodeTree::GetCurType()
{
   assert(fCurNode->fNodeType == TNode::kType);
   return fCurNode->fType;
}

TExpr &TNodeTree::GetCurExpr()
{
   assert(fCurNode->fNodeType == TNode::kExpr);
   return fCurNode->fExpr;
}

static void ParseCvList(TLexer &lex, TNodeTree &tree)
{
   auto &type = tree.GetCurType();

   TToken tok = lex.Peek();
   while (tok.fType == kKwConst || tok.fType == kKwVolatile) {
      type.fQual |= (tok.fType == kKwConst) ? TType::kConst : TType::kVolatile;
      lex.Consume();
      tok = lex.Peek();
   }
}

static void ParseNamespace(TLexer &lex, TNodeTree &tree)
{
   auto &type = tree.GetCurType();

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
      ParseNamespace(lex, tree);
   }
}

static void ParseTypeSpecifier(TLexer &lex, TNodeTree &tree)
{
   (void)tree;

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

static bool IsBinOp(ETokType type)
{
   return type == kKwAnd || type == kKwOr || type == kKwBitand || type == kKwBitor || type == kAndAnd ||
          type == kOrOr || type == kAnd || type == kOr || type == kXor || type == kArrow || type == kPlus ||
          type == kMinus || type == kStar || type == kSlash || type == kLe || type == kGe || type == kLt ||
          type == kGt || type == kEq || type == kNe;
}

static const char *FixedToStr(ETokType type)
{
   if (type >= kFirstFixed && type <= kLastFixed) {
      return kFixeds[type - kFirstFixed];
   }
   return "";
}

static bool ParseExpr(TLexer &lex, TNodeTree &tree)
{
   auto *expr = &tree.GetCurExpr();
   TToken tok = lex.Peek();
   // expr :: [unary-op] (number | string | ident) | "(" [expr] ")" | expr [binop] expr
   if (IsUnaryOp(tok.fType)) {
      *expr += FixedToStr(tok.fType);
      lex.Consume();
      tok = lex.Peek();
   }
   if (tok.fType == kOpenRound) {
      lex.Consume();
      *expr += "(";
      if (lex.Peek().fType != kCloseRound) {
         tree.AddNode(TNode::kExpr);
         tree.PushNesting();
         if (!ParseExpr(lex, tree))
            return false;
         *expr += tree.GetCurExpr();
         tree.PopNesting();
      }
      *expr += ")";
      tok = lex.Peek();
      if (tok.fType != kCloseRound) {
         tree.fErrors.push_back("unterminated parens expression");
         return false;
      }
      lex.Consume();
   }

   expr = &tree.GetCurExpr();
   if (tok.fType == kNumber || tok.fType == kString || tok.fType == kCharacter) {
      *expr += tok.fStr;
      lex.Consume();
      tok = lex.Peek();
   }

   // Note that we don't care about operator precedence, we just want to parse until the end of
   // the expression.
   if (IsBinOp(tok.fType)) {
      // Kinda workaround for treating '>' as an operator vs a close template.
      // We currently treat it as an operator if we're in the middle of a parentheses operation
      if (tok.fType != kGt || (tree.fCurNode->fParent && tree.fCurNode->fParent->fNodeType == TNode::kExpr)) {
         *expr += FixedToStr(tok.fType);
         lex.Consume();
         if (!ParseExpr(lex, tree))
            return false;
         tok = lex.Peek();
      }
   }

   if (tok.fType == kOpenSquare) {
      *expr += FixedToStr(tok.fType);
      lex.Consume();
      if (!ParseExpr(lex, tree))
         return false;
      tok = lex.Peek();
      if (tok.fType != kCloseSquare) {
         tree.fErrors.push_back("unterminated array expression");
         return false;
      }
      lex.Consume();
   }

   return true;
}

static bool ParseTypeInternal(TLexer &lex, TNodeTree &tree);

static bool ParseTemplate(TLexer &lex, TNodeTree &tree)
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
         tree.AddNode(childType);
         tree.PushNesting();

         if (childType == TNode::kType) {
            if (!ParseTypeInternal(lex, tree)) {
               return false;
            }
         } else {
            if (!ParseExpr(lex, tree))
               return false;
         }

         tree.PopNesting();

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

static void ParseRefsAndPtrs(TLexer &lex, TNodeTree &tree)
{
   TToken tok = lex.Peek();
   // TNode *prevNode = tree.fCurNode;
   while (tok.fType == kAnd || tok.fType == kAndAnd || tok.fType == kStar) {
      // When we find a ptr or ref we wrap the current node in another node that represents the indirection.
      TNode wrapped = *tree.fCurNode; // copy the current node
      tree.fCurNode->fType = {};
      tree.fCurNode->fType.fIndirection = TokTypeToIndirection(tok.fType);
      auto &newNode = tree.fNodes.emplace_back(wrapped);
      // Adjust links
      if (tree.fCurNode->fFirstChild)
         tree.fCurNode->fFirstChild->fParent = &newNode;
      newNode.fParent = tree.fCurNode;
      newNode.fNextSibling = nullptr;
      newNode.fFirstChild = tree.fCurNode->fFirstChild;
      tree.fCurNode->fFirstChild = &newNode;

      lex.Consume();

      // Note that we do NOT push the wrapped node as the new latest node until we're done with parsing ptrs and
      // refs, as each new ptr/ref refers to the outermost node of the new hierarchy.
      ParseCvList(lex, tree);

      tok = lex.Peek();
   }
   // if (prevNode != tree.fCurNode)
   //    tree.PushNesting();
}

static bool ParseTypeInternal(TLexer &lex, TNodeTree &tree)
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
   ParseCvList(lex, tree);
   ParseNamespace(lex, tree);
   ParseTypeSpecifier(lex, tree);

   // parse type name
   auto &type = tree.GetCurType();
   TToken tok = lex.Peek();
   if (tok.fType != kIdent) {
      tree.fErrors.push_back("expected type name, found " + tok.ToString());
      return false;
   }
   type.fName = tok.fStr;
   lex.Consume();

   if (!ParseTemplate(lex, tree))
      return false;

   ParseCvList(lex, tree);
   ParseRefsAndPtrs(lex, tree);

   return true;
}

TNodeTree ParseType(std::string_view src)
{
   TNodeTree res;

   TLexer lex{src};

   // Push root node (assume it's a type, otherwise we'll error out)
   res.AddNode(TNode::kType);

   ParseTypeInternal(lex, res);

   return res;
}

void PrintNode(std::ostream &out, const TNode &node, int flags, int indent)
{
   std::string indentStr;
   indentStr.resize(indent, ' ');

   if (flags & kPrintDebug)
      out << indentStr << ((node.fNodeType == TNode::kType) ? "Type " : "Expr ");

   if (node.fNodeType == TNode::kType && node.fType.fIndirection == TType::EIndirection::kNone) {
      if (!(flags & kStripCV)) {
         if (node.fType.fQual & TType::kConst)
            out << "const ";
         if (node.fType.fQual & TType::kVolatile)
            out << "volatile ";
      }
      if (!(flags & kStripNamespace))
         out << node.fType.fNamespace << node.fType.fName;
   } else if (node.fNodeType == TNode::kExpr) {
      out << node.fExpr;
   }

   for (TNode *child = node.fFirstChild; child; child = child->fNextSibling)
      PrintNode(out, *child, flags, indent + 2);

   if (node.fNodeType == TNode::kType && node.fType.fIndirection != TType::EIndirection::kNone) {
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

void TNodeTree::Print(std::ostream &out, int flags) const
{
   if (fNodes.empty())
      return;

   PrintNode(out, fNodes[0], flags);
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

} // namespace ROOT::Internal::TypeParsing
