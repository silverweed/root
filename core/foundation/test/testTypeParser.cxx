#include <ROOT/TypeParser.hxx>

#include "gtest/gtest.h"

using namespace ROOT::Internal::TypeParsing;

#define VT std::vector<TToken>

TEST(TypeParser, Lex)
{
   EXPECT_EQ(TLexer::Tokenize(""), VT{});
   EXPECT_EQ(TLexer::Tokenize("int"), VT{TToken::Ident("int")});
   EXPECT_EQ(TLexer::Tokenize("int&"), VT({TToken::Ident("int"), kAnd}));
   EXPECT_EQ(TLexer::Tokenize("volatile int *const"), VT({kKwVolatile, TToken::Ident("int"), kStar, kKwConst}));
   EXPECT_EQ(TLexer::Tokenize("void**"), VT({TToken::Ident("void"), kStar, kStar}));
   EXPECT_EQ(TLexer::Tokenize("float&&"), VT({TToken::Ident("float"), kAndAnd}));
   EXPECT_EQ(TLexer::Tokenize("const"), VT{TToken::Fixed("const")});
   EXPECT_EQ(TLexer::Tokenize("volatile const float"),
             VT({TToken::Fixed("volatile"), TToken::Fixed("const"), TToken::Ident("float")}));
   EXPECT_EQ(TLexer::Tokenize("std::size_t"), VT({TToken::Ident("std"), TToken{kColonColon}, TToken::Ident("size_t")}));
   EXPECT_EQ(TLexer::Tokenize("std::vector<float>"),
             VT({TToken::Ident("std"), TToken{kColonColon}, TToken::Ident("vector"), TToken{kLt},
                 TToken::Ident("float"), TToken{kGt}}));
   // clang-format off
   EXPECT_EQ(TLexer::Tokenize("std::conditional_t<(T > 32), int, std::list<T, MyAllocator>>"),
             VT({TToken::Ident("std"), kColonColon, TToken::Ident("conditional_t"),
                 kLt,
                   kOpenRound,
                     TToken::Ident("T"), kGt, kNumber,
                   kCloseRound,
                   kComma,
                   TToken::Ident("int"),
                   kComma,
                   TToken::Ident("std"), kColonColon, TToken::Ident("list"),
                   kLt,
                     TToken::Ident("T"),
                     kComma,
                     TToken::Ident("MyAllocator"),
                   kGt,
                 kGt
             }));

   EXPECT_EQ(TLexer::Tokenize("std::conditional_t<T<32, int,std::list<T, MyAllocator>> "),
             VT({TToken::Ident("std"), kColonColon, TToken::Ident("conditional_t"),
                 kLt,
                   TToken::Ident("T"), kLt, kNumber,
                   kComma,
                   TToken::Ident("int"),
                   kComma,
                   TToken::Ident("std"), kColonColon, TToken::Ident("list"),
                   kLt,
                     TToken::Ident("T"),
                     kComma,
                     TToken::Ident("MyAllocator"),
                   kGt,
                 kGt
             }));
   // clang-format on
   EXPECT_EQ(TLexer::Tokenize("My   :: Namespace ::MyType<(2<3)>()"),
             VT({TToken::Ident("My"), kColonColon, TToken::Ident("Namespace"), kColonColon, TToken::Ident("MyType"),
                 kLt, kOpenRound, kNumber, kLt, kNumber, kCloseRound, kGt, kOpenRound, kCloseRound}));
   EXPECT_EQ(TLexer::Tokenize("std::enable_if<true || false, A, B>"),
             VT({TToken::Ident("std"), kColonColon, TToken::Ident("enable_if"), kLt, TToken::Ident("true"), kOrOr,
                 TToken::Ident("false"), kComma, TToken::Ident("A"), kComma, TToken::Ident("B"), kGt}));
   EXPECT_EQ(TLexer::Tokenize("Foo<\"bar\", 'F'>"),
             VT({TToken::Ident("Foo"), kLt, TToken::String("bar"), kComma, TToken::Char('F'), kGt}));
}

TEST(TypeParser, LexInvalidTypes)
{
   EXPECT_EQ(TLexer::Tokenize("std::vector<int"),
             VT({TToken::Ident("std"), kColonColon, TToken::Ident("vector"), kLt, TToken::Ident("int")}));
   EXPECT_EQ(TLexer::Tokenize("&*void*&"), VT({kAnd, kStar, TToken::Ident("void"), kStar, kAnd}));
   EXPECT_EQ(TLexer::Tokenize("!double"), VT({kNot, TToken::Ident("double")}));
}

TEST(TypeParser, LexNumbers)
{
   EXPECT_EQ(TLexer::Tokenize("~3"), VT({kTilde, kNumber}));
   EXPECT_EQ(TLexer::Tokenize("3.0f"), VT({{kNumber}}));
   EXPECT_EQ(TLexer::Tokenize("1e10 - 3.0f + 0x4.5p7"), VT({kNumber, kMinus, kNumber, kPlus, kNumber}));
}

TEST(TypeParser, LexStrings) {
  EXPECT_EQ(TLexer::Tokenize("\"\""), VT({TToken::String("")}));
  EXPECT_EQ(TLexer::Tokenize("\"asd\""), VT({TToken::String("asd")}));
  EXPECT_EQ(TLexer::Tokenize("\" asd  \""), VT({TToken::String(" asd  ")}));
  EXPECT_EQ(TLexer::Tokenize("\"'\""), VT({TToken::String("'")}));
  EXPECT_EQ(TLexer::Tokenize("''"), VT({{kInvalid}}));
  EXPECT_EQ(TLexer::Tokenize("' '"), VT({TToken::Char(' ')}));
}
