/// \file TypeParser.cxx
/// \ingroup Core
/// \author Giacomo Parolini <giacomo.parolini@cern.ch>
/// \date 2025-08-04

#ifndef ROOT_CORE_TYPE_PARSER
#define ROOT_CORE_TYPE_PARSER

#include <string_view>
#include <string>
#include <cstdint>
#include <iostream>
#include <vector>
#include <deque>

#include <ROOT/RError.hxx>

namespace ROOT::Internal::TypeParsing {

/// Lexer token types
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
   // Contains:
   // - the identifier name, for kIdent
   // - the string or character, for kString and kCharacter
   // - the number, for kNumber
   // - the error, for kInvalid
   std::string fStr;

   static TToken Ident(std::string_view str);
   static TToken Char(char ch);
   static TToken String(std::string_view str);
   static TToken Number(std::string_view str);
   static TToken Fixed(std::string_view fixed);

   TToken() = default;
   TToken(ETokType type) : fType(type) {}

   // Used for debugging
   std::string ToString() const;
};

bool operator==(const TToken &a, const TToken &b);
std::ostream &operator<<(std::ostream &out, const TToken &t);
// Required by GoogleTest
void PrintTo(const TToken &t, std::ostream *os);

/// Turns a string into a stream of tokens.
/// This lexer is designed to work with "mostly well-formed" input, so it does only basic validation.
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

   /// Finds the next token and returns it. Does not advance the internal position (except by skipping whitespaces).
   /// This function is idempotent and will always return the same result if Consume() or Rewind() are not called
   /// in between.
   TToken Peek();
   /// Advances to the next token.
   void Consume();
   /// Goes back to the previous token. The lexer only has 1 step of backtracking available, so this function
   /// is idempotent and can't be used to rewind the stream multiple times.
   void Rewind();
};

/// Data relative to a parsed type.
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

/// Data relative to a parsed expression.
struct TExpr {
   enum EType {
      // has no children; string, ident or number is in fStr.
      kLeaf,
      // has 1 child, operator is in fStr.
      kUnaryOp,
      // has 2 children
      kBinOp,
      // has 0+ children
      kParens,
   };
   EType fType;
   std::string fStr;
};

std::ostream &operator<<(std::ostream &out, TExpr::EType type);

/// A parsed node. May be a Type or an Expression.
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

   kStripPointersAndRefs = kStripPointers | kStripRefs,
};

/// A tree constructed from parsing a type.
/// Nodes are either types or expressions.
/// Types have children in case of templates (e.g A<B, C> has root A with children B and C) and expressions have
/// children depending on their nesting (see description in TExpr).
/// Even though the tree is logically an N-ary tree, internally it is stored as a binary tree where each node points
/// to its first children and its next sibling.
struct TNodeTree {
   // deque to keep pointers valid
   std::deque<TNode> fNodes;
   std::vector<std::string> fErrors;

   TNode *PushNode(TNode::ENodeType type);
   void AddChild(TNode *parent, TNode *child);

   // Makes `node` a child of a new node wrapping it, then resets all data of the new wrapper node.
   // This doesn't invalidate the pointers to `node` (but makes them point to the "new" wrapper).
   void WrapNode(TNode *&node);

   void Print(std::ostream &out = std::cout, int flags = kNone) const;
   void PrintTreeDebug(std::ostream &out = std::cout) const;
};

/// Parses a type string into a tree. This is not a full AST but rather a tree storing enough info to be able to
/// manipulate and reconstruct the given type.
TNodeTree ParseType(std::string_view src);

ROOT::RResult<std::string> ShortType(std::string_view typeDesc);

} // namespace ROOT::Internal::TypeParsing

#endif
