#pragma once

#include <string_view>
#include <string>
#include <cstdint>
#include <iostream>
#include <vector>
#include <deque>
#include <ROOT/RError.hxx>

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
   kKwClass,
   kKwStruct,
   kKwUnion,
   kKwEnum,
   kFirstNonKeyword,
   kAndAnd = kFirstNonKeyword,
   kOrOr,
   kAnd,
   kOr,
   kXor,
   kTilde,
   kPlusPlus,
   kMinusMinus,
   kArrow,
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

   static TToken Number(std::string_view str)
   {
      TToken tok = {kNumber};
      tok.fStr = str;
      return tok;
   }

   static TToken Fixed(std::string_view fixed);

   std::string ToString() const;
};

bool operator==(const TToken &a, const TToken &b);
std::ostream &operator<<(std::ostream &out, const TToken &t);
// Required by GoogleTest
void PrintTo(const TToken &t, std::ostream *os);

class TLexer final {
   std::string_view fSrc;
   std::size_t fCur = 0;
   std::size_t fNext = 0;
   std::size_t fPrev = 0;

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
   void Rewind();
};

using TNodeIdx = std::int32_t;

struct TType {
   enum ETypeQual {
      kNone = 0,
      kConst = 0x1,
      kVolatile = 0x2,
   };
   enum class EIndirection {
      kNone,
      kRef,
      kPtr,
      kRvRef,
   };

   std::string fName;
   std::string fNamespace;
   int fQual = 0;
   EIndirection fIndirection = EIndirection::kNone;
};

// Required by GoogleTest
void PrintTo(const TType::EIndirection &t, std::ostream *os);

using TExpr = std::string;

struct TNode {
   enum ENodeType {
      kInvalid,
      kType,
      kExpr,
   };
   ENodeType fNodeType = kInvalid;
   TNode *fFirstChild = nullptr;
   TNode *fNextSibling = nullptr;
   TNode *fParent = nullptr;
   // Note: fType and fExpr are mutually exclusive, but using a union makes this class non default constructible
   TType fType;
   TExpr fExpr;
};

enum EPrintFlags {
   kNone = 0x0,
   kStripCV = 0x1,
   kStripPointers = 0x2,
   kStripRefs = 0x4,
   kStripNamespace = 0x8,
   kPrintDebug = 0x10,

   kStripPointersAndRefs = kStripPointers | kStripRefs,
};

void PrintNode(std::ostream &out, const TNode &node, int flags = kNone, int indent = 0);

struct TNodeTree {
   // deque to keep pointers valid
   std::deque<TNode> fNodes;
   std::vector<std::string> fErrors;

   // The node that children are appended to
   TNode *fCurNode = nullptr;

   TType &GetCurType();
   TExpr &GetCurExpr();

   void AddNode(TNode::ENodeType type);
   void PushNesting();
   void PopNesting();

   void Print(std::ostream &out = std::cout, int flags = kNone) const;
};

TNodeTree ParseType(std::string_view src);
ROOT::RResult<std::string> ShortType(std::string_view typeDesc);

} // namespace ROOT::Internal::TypeParsing
