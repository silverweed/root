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
                     TToken::Ident("T"), kGt, TToken::Number("32"),
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
                   TToken::Ident("T"), kLt, TToken::Number("32"),
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
   EXPECT_EQ(
      TLexer::Tokenize("My   :: Namespace ::MyType<(2<3)>()"),
      VT({TToken::Ident("My"), kColonColon, TToken::Ident("Namespace"), kColonColon, TToken::Ident("MyType"), kLt,
          kOpenRound, TToken::Number("2"), kLt, TToken::Number("3"), kCloseRound, kGt, kOpenRound, kCloseRound}));
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
   EXPECT_EQ(TLexer::Tokenize("~3"), VT({kTilde, TToken::Number("3")}));
   EXPECT_EQ(TLexer::Tokenize("3.0f"), VT({{TToken::Number("3.0f")}}));
   EXPECT_EQ(TLexer::Tokenize("1e10 - 3.0f + 0x4.5p7"),
             VT({TToken::Number("1e10"), kMinus, TToken::Number("3.0f"), kPlus, TToken::Number("0x4.5p7")}));
}

TEST(TypeParser, LexStrings) {
  EXPECT_EQ(TLexer::Tokenize("\"\""), VT({TToken::String("")}));
  EXPECT_EQ(TLexer::Tokenize("\"asd\""), VT({TToken::String("asd")}));
  EXPECT_EQ(TLexer::Tokenize("\" asd  \""), VT({TToken::String(" asd  ")}));
  EXPECT_EQ(TLexer::Tokenize("\"'\""), VT({TToken::String("'")}));
  EXPECT_EQ(TLexer::Tokenize("''"), VT({{kInvalid}}));
  EXPECT_EQ(TLexer::Tokenize("' '"), VT({TToken::Char(' ')}));
}

TEST(TypeParser, ParseTypeSimple)
{
   auto tree = ParseType("int");
   ASSERT_EQ(tree.fNodes.size(), 1);
   ASSERT_EQ(tree.fNodes[0].fNodeType, TNode::kType);
   EXPECT_EQ(tree.fNodes[0].fType.fName, "int");
   EXPECT_EQ(tree.fNodes[0].fType.fNamespace, "");
   EXPECT_EQ(tree.fNodes[0].fType.fQual, 0);
   EXPECT_EQ(tree.fNodes[0].fType.fIndirection, TType::EIndirection::kNone);

   tree = ParseType("const int volatile");
   ASSERT_EQ(tree.fNodes.size(), 1);
   ASSERT_EQ(tree.fNodes[0].fNodeType, TNode::kType);
   EXPECT_EQ(tree.fNodes[0].fType.fName, "int");
   EXPECT_EQ(tree.fNodes[0].fType.fNamespace, "");
   EXPECT_EQ(tree.fNodes[0].fType.fQual, TType::kConst | TType::kVolatile);
   EXPECT_EQ(tree.fNodes[0].fType.fIndirection, TType::EIndirection::kNone);

   tree = ParseType("const std::size_t");
   ASSERT_EQ(tree.fNodes.size(), 1);
   ASSERT_EQ(tree.fNodes[0].fNodeType, TNode::kType);
   EXPECT_EQ(tree.fNodes[0].fType.fName, "size_t");
   EXPECT_EQ(tree.fNodes[0].fType.fNamespace, "std::");
   EXPECT_EQ(tree.fNodes[0].fType.fQual, TType::kConst);
   EXPECT_EQ(tree.fNodes[0].fType.fIndirection, TType::EIndirection::kNone);
}

TEST(TypeParser, ParseTypeNested)
{
   auto tree = ParseType("std::list<int>");
   ASSERT_EQ(tree.fNodes.size(), 2);
   ASSERT_EQ(tree.fNodes[0].fNodeType, TNode::kType);
   EXPECT_EQ(tree.fNodes[0].fType.fName, "list");
   EXPECT_EQ(tree.fNodes[0].fType.fNamespace, "std::");
   EXPECT_EQ(tree.fNodes[0].fType.fQual, 0);
   EXPECT_EQ(tree.fNodes[0].fType.fIndirection, TType::EIndirection::kNone);
   ASSERT_EQ(tree.fNodes[1].fNodeType, TNode::kType);
   EXPECT_EQ(tree.fNodes[1].fType.fName, "int");
   EXPECT_EQ(tree.fNodes[1].fType.fNamespace, "");
   EXPECT_EQ(tree.fNodes[1].fType.fQual, 0);
   EXPECT_EQ(tree.fNodes[1].fType.fIndirection, TType::EIndirection::kNone);

   tree = ParseType("::my::ns::Type<5> volatile");
   ASSERT_EQ(tree.fNodes.size(), 2);
   ASSERT_EQ(tree.fNodes[0].fNodeType, TNode::kType);
   EXPECT_EQ(tree.fNodes[0].fType.fName, "Type");
   EXPECT_EQ(tree.fNodes[0].fType.fNamespace, "::my::ns::");
   EXPECT_EQ(tree.fNodes[0].fType.fQual, TType::kVolatile);
   EXPECT_EQ(tree.fNodes[0].fType.fIndirection, TType::EIndirection::kNone);
   ASSERT_EQ(tree.fNodes[1].fNodeType, TNode::kExpr);
   EXPECT_EQ(tree.fNodes[1].fExpr, "5");

   tree = ParseType("::Type<(5 > 3), bar::Foo<(2 < 3)>>");
   ASSERT_EQ(tree.fNodes.size(), 6);
   ASSERT_EQ(tree.fNodes[0].fNodeType, TNode::kType);
   EXPECT_EQ(tree.fNodes[0].fType.fName, "Type");
   EXPECT_EQ(tree.fNodes[0].fType.fNamespace, "::");
   EXPECT_EQ(tree.fNodes[0].fType.fQual, 0);
   EXPECT_EQ(tree.fNodes[0].fType.fIndirection, TType::EIndirection::kNone);
   ASSERT_EQ(tree.fNodes[1].fNodeType, TNode::kExpr);
   EXPECT_EQ(tree.fNodes[1].fExpr, "(5>3)");
   ASSERT_EQ(tree.fNodes[2].fNodeType, TNode::kExpr);
   EXPECT_EQ(tree.fNodes[2].fExpr, "5>3");
   ASSERT_EQ(tree.fNodes[3].fNodeType, TNode::kType);
   EXPECT_EQ(tree.fNodes[3].fType.fName, "Foo");
   EXPECT_EQ(tree.fNodes[3].fType.fNamespace, "bar::");
   EXPECT_EQ(tree.fNodes[3].fType.fQual, 0);
   EXPECT_EQ(tree.fNodes[3].fType.fIndirection, TType::EIndirection::kNone);
   ASSERT_EQ(tree.fNodes[4].fNodeType, TNode::kExpr);
   EXPECT_EQ(tree.fNodes[4].fExpr, "(2<3)");
   ASSERT_EQ(tree.fNodes[5].fNodeType, TNode::kExpr);
   EXPECT_EQ(tree.fNodes[5].fExpr, "2<3");

   tree = ParseType("short *const *");
   ASSERT_EQ(tree.fNodes.size(), 3);
   const auto *root = &tree.fNodes[0];
   ASSERT_EQ(root->fNodeType, TNode::kType);
   EXPECT_EQ(root->fType.fName, "");
   EXPECT_EQ(root->fType.fNamespace, "");
   EXPECT_EQ(root->fType.fQual, 0);
   EXPECT_EQ(root->fType.fIndirection, TType::EIndirection::kPtr);
   const auto *firstChild = root->fFirstChild;
   ASSERT_EQ(firstChild->fNodeType, TNode::kType);
   EXPECT_EQ(firstChild->fType.fName, "");
   EXPECT_EQ(firstChild->fType.fNamespace, "");
   EXPECT_EQ(firstChild->fType.fQual, TType::kConst);
   EXPECT_EQ(firstChild->fType.fIndirection, TType::EIndirection::kPtr);
   const auto *secondChild = firstChild->fFirstChild;
   ASSERT_EQ(secondChild->fNodeType, TNode::kType);
   EXPECT_EQ(secondChild->fType.fName, "short");
   EXPECT_EQ(secondChild->fType.fNamespace, "");
   EXPECT_EQ(secondChild->fType.fQual, 0);
   EXPECT_EQ(secondChild->fType.fIndirection, TType::EIndirection::kNone);

   tree = ParseType("int *const");
   ASSERT_EQ(tree.fNodes.size(), 2);
   ASSERT_EQ(tree.fNodes[0].fNodeType, TNode::kType);
   EXPECT_EQ(tree.fNodes[0].fType.fName, "");
   EXPECT_EQ(tree.fNodes[0].fType.fNamespace, "");
   EXPECT_EQ(tree.fNodes[0].fType.fQual, TType::kConst);
   EXPECT_EQ(tree.fNodes[0].fType.fIndirection, TType::EIndirection::kPtr);
   ASSERT_EQ(tree.fNodes[1].fNodeType, TNode::kType);
   EXPECT_EQ(tree.fNodes[1].fType.fName, "int");
   EXPECT_EQ(tree.fNodes[1].fType.fNamespace, "");
   EXPECT_EQ(tree.fNodes[1].fType.fQual, 0);
   EXPECT_EQ(tree.fNodes[1].fType.fIndirection, TType::EIndirection::kNone);

   tree = ParseType("int const*");
   ASSERT_EQ(tree.fNodes.size(), 2);
   ASSERT_EQ(tree.fNodes[0].fNodeType, TNode::kType);
   EXPECT_EQ(tree.fNodes[0].fType.fName, "");
   EXPECT_EQ(tree.fNodes[0].fType.fNamespace, "");
   EXPECT_EQ(tree.fNodes[0].fType.fQual, 0);
   EXPECT_EQ(tree.fNodes[0].fType.fIndirection, TType::EIndirection::kPtr);
   ASSERT_EQ(tree.fNodes[1].fNodeType, TNode::kType);
   EXPECT_EQ(tree.fNodes[1].fType.fName, "int");
   EXPECT_EQ(tree.fNodes[1].fType.fNamespace, "");
   EXPECT_EQ(tree.fNodes[1].fType.fQual, TType::kConst);
   EXPECT_EQ(tree.fNodes[1].fType.fIndirection, TType::EIndirection::kNone);

   tree = ParseType("int const&");
   ASSERT_EQ(tree.fNodes.size(), 2);
   ASSERT_EQ(tree.fNodes[0].fNodeType, TNode::kType);
   EXPECT_EQ(tree.fNodes[0].fType.fName, "");
   EXPECT_EQ(tree.fNodes[0].fType.fNamespace, "");
   EXPECT_EQ(tree.fNodes[0].fType.fQual, 0);
   EXPECT_EQ(tree.fNodes[0].fType.fIndirection, TType::EIndirection::kRef);
   ASSERT_EQ(tree.fNodes[1].fNodeType, TNode::kType);
   EXPECT_EQ(tree.fNodes[1].fType.fName, "int");
   EXPECT_EQ(tree.fNodes[1].fType.fNamespace, "");
   EXPECT_EQ(tree.fNodes[1].fType.fQual, TType::kConst);
   EXPECT_EQ(tree.fNodes[1].fType.fIndirection, TType::EIndirection::kNone);

   tree = ParseType("double&&");
   ASSERT_EQ(tree.fNodes.size(), 2);
   ASSERT_EQ(tree.fNodes[0].fNodeType, TNode::kType);
   EXPECT_EQ(tree.fNodes[0].fType.fName, "");
   EXPECT_EQ(tree.fNodes[0].fType.fNamespace, "");
   EXPECT_EQ(tree.fNodes[0].fType.fQual, 0);
   EXPECT_EQ(tree.fNodes[0].fType.fIndirection, TType::EIndirection::kRvRef);
   ASSERT_EQ(tree.fNodes[1].fNodeType, TNode::kType);
   EXPECT_EQ(tree.fNodes[1].fType.fName, "double");
   EXPECT_EQ(tree.fNodes[1].fType.fNamespace, "");
   EXPECT_EQ(tree.fNodes[1].fType.fQual, 0);
   EXPECT_EQ(tree.fNodes[1].fType.fIndirection, TType::EIndirection::kNone);

   tree = ParseType("volatile int **const");
   ASSERT_EQ(tree.fNodes.size(), 3);
   root = &tree.fNodes[0];
   ASSERT_EQ(root->fNodeType, TNode::kType);
   EXPECT_EQ(root->fType.fName, "");
   EXPECT_EQ(root->fType.fNamespace, "");
   EXPECT_EQ(root->fType.fQual, TType::kConst);
   EXPECT_EQ(root->fType.fIndirection, TType::EIndirection::kPtr);
   firstChild = root->fFirstChild;
   ASSERT_EQ(firstChild->fNodeType, TNode::kType);
   EXPECT_EQ(firstChild->fType.fName, "");
   EXPECT_EQ(firstChild->fType.fNamespace, "");
   EXPECT_EQ(firstChild->fType.fQual, 0);
   EXPECT_EQ(firstChild->fType.fIndirection, TType::EIndirection::kPtr);
   secondChild = firstChild->fFirstChild;
   ASSERT_EQ(secondChild->fNodeType, TNode::kType);
   EXPECT_EQ(secondChild->fType.fName, "int");
   EXPECT_EQ(secondChild->fType.fNamespace, "");
   EXPECT_EQ(secondChild->fType.fQual, TType::kVolatile);
   EXPECT_EQ(secondChild->fType.fIndirection, TType::EIndirection::kNone);

   tree = ParseType("int volatile *const *const volatile");
   ASSERT_EQ(tree.fNodes.size(), 3);
   root = &tree.fNodes[0];
   ASSERT_EQ(root->fNodeType, TNode::kType);
   EXPECT_EQ(root->fType.fName, "");
   EXPECT_EQ(root->fType.fNamespace, "");
   EXPECT_EQ(root->fType.fQual, TType::kVolatile | TType::kConst);
   EXPECT_EQ(root->fType.fIndirection, TType::EIndirection::kPtr);
   firstChild = root->fFirstChild;
   ASSERT_EQ(firstChild->fNodeType, TNode::kType);
   EXPECT_EQ(firstChild->fType.fName, "");
   EXPECT_EQ(firstChild->fType.fNamespace, "");
   EXPECT_EQ(firstChild->fType.fQual, TType::kConst);
   EXPECT_EQ(firstChild->fType.fIndirection, TType::EIndirection::kPtr);
   secondChild = firstChild->fFirstChild;
   ASSERT_EQ(secondChild->fNodeType, TNode::kType);
   EXPECT_EQ(secondChild->fType.fName, "int");
   EXPECT_EQ(secondChild->fType.fNamespace, "");
   EXPECT_EQ(secondChild->fType.fQual, TType::kVolatile);
   EXPECT_EQ(secondChild->fType.fIndirection, TType::EIndirection::kNone);
}

TEST(TypeParser, ShortType)
{
   EXPECT_EQ(ShortType("const int").Unwrap(), "int");
   EXPECT_EQ(ShortType("short*").Unwrap(), "short*");
   EXPECT_EQ(ShortType("const volatile class TNamed**").Unwrap(), "TNamed**");
}
