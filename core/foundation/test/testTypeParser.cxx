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
                   // NOTE: '>>' is parsed as ShiftRight unless the proper flag is given to TLexer::Peek().
                   // This happens when properly calling ParseType(), but Tokenize doesn't know about it so we get
                   // the "wrong" token in this case.
                   kShiftRight,
             }));

   EXPECT_EQ(TLexer::Tokenize("std::conditional_t<T<32, int,std::list<T, MyAllocator> > "),
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

   EXPECT_EQ(TLexer::Tokenize("T<\"a.b\">"), VT({TToken::Ident("T"), kLt, TToken::String("a.b"), kGt}));
   EXPECT_EQ(TLexer::Tokenize("T<\">\">"), VT({TToken::Ident("T"), kLt, TToken::String(">"), kGt}));
   EXPECT_EQ(TLexer::Tokenize("T<(a.b->b + c.d)>"),
             VT({TToken::Ident("T"), kLt, kOpenRound, TToken::Ident("a"), kPeriod, TToken::Ident("b"), kArrow,
                 TToken::Ident("b"), kPlus, TToken::Ident("c"), kPeriod, TToken::Ident("d"), kCloseRound, kGt}));
}

TEST(TypeParser, LexTypeParam)
{
   EXPECT_EQ(TLexer::Tokenize("Foo<bar, type-parameter-0-1, Baz<type-parameter-1-1> >"),
             VT({TToken::Ident("Foo"), kLt, TToken::Ident("bar"), kComma, TToken::TypeParam("type-parameter-0-1"),
                 kComma, TToken::Ident("Baz"), kLt, TToken::TypeParam("type-parameter-1-1"), kGt, kGt}));
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
   EXPECT_EQ(TLexer::Tokenize("42ull"), VT({TToken::Number("42ull")}));
   EXPECT_EQ(TLexer::Tokenize("0x42uL"), VT({TToken::Number("0x42uL")}));
}

TEST(TypeParser, LexStrings)
{
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
   EXPECT_EQ(tree.fNodes[0].fType.fFlags, 0);
   EXPECT_EQ(tree.fNodes[0].fType.fIndirection, TType::EIndirection::kNone);

   tree = ParseType("const int volatile");
   ASSERT_EQ(tree.fNodes.size(), 1);
   ASSERT_EQ(tree.fNodes[0].fNodeType, TNode::kType);
   EXPECT_EQ(tree.fNodes[0].fType.fName, "int");
   EXPECT_EQ(tree.fNodes[0].fType.fNamespace, "");
   EXPECT_EQ(tree.fNodes[0].fType.fFlags, TType::kConst | TType::kVolatile);
   EXPECT_EQ(tree.fNodes[0].fType.fIndirection, TType::EIndirection::kNone);

   tree = ParseType("const std::size_t");
   ASSERT_EQ(tree.fNodes.size(), 1);
   ASSERT_EQ(tree.fNodes[0].fNodeType, TNode::kType);
   EXPECT_EQ(tree.fNodes[0].fType.fName, "size_t");
   EXPECT_EQ(tree.fNodes[0].fType.fNamespace, "std::");
   EXPECT_EQ(tree.fNodes[0].fType.fFlags, TType::kConst);
   EXPECT_EQ(tree.fNodes[0].fType.fIndirection, TType::EIndirection::kNone);
}

TEST(TypeParser, ParseTypeNested)
{
   auto tree = ParseType("std::list<int>");
   ASSERT_EQ(tree.fNodes.size(), 2);
   ASSERT_EQ(tree.fNodes[0].fNodeType, TNode::kType);
   EXPECT_EQ(tree.fNodes[0].fType.fName, "list");
   EXPECT_EQ(tree.fNodes[0].fType.fNamespace, "std::");
   EXPECT_EQ(tree.fNodes[0].fType.fFlags, TType::kTemplated);
   EXPECT_EQ(tree.fNodes[0].fType.fIndirection, TType::EIndirection::kNone);
   ASSERT_EQ(tree.fNodes[1].fNodeType, TNode::kType);
   EXPECT_EQ(tree.fNodes[1].fType.fName, "int");
   EXPECT_EQ(tree.fNodes[1].fType.fNamespace, "");
   EXPECT_EQ(tree.fNodes[1].fType.fFlags, 0);
   EXPECT_EQ(tree.fNodes[1].fType.fIndirection, TType::EIndirection::kNone);

   tree = ParseType("::my::ns::Type<5> volatile");
   ASSERT_EQ(tree.fNodes.size(), 2);
   ASSERT_EQ(tree.fNodes[0].fNodeType, TNode::kType);
   EXPECT_EQ(tree.fNodes[0].fType.fName, "Type");
   EXPECT_EQ(tree.fNodes[0].fType.fNamespace, "::my::ns::");
   EXPECT_EQ(tree.fNodes[0].fType.fFlags, TType::kVolatile | TType::kTemplated);
   EXPECT_EQ(tree.fNodes[0].fType.fIndirection, TType::EIndirection::kNone);
   ASSERT_EQ(tree.fNodes[1].fNodeType, TNode::kExpr);
   EXPECT_EQ(tree.fNodes[1].fExpr.fType, TExpr::kLeaf);
   EXPECT_EQ(tree.fNodes[1].fExpr.fStr, "5");

   tree = ParseType("T<(a > (1 == 3))>");
   ASSERT_EQ(tree.fNodes.size(), 8);
   ASSERT_EQ(tree.fNodes[0].fNodeType, TNode::kType);
   EXPECT_EQ(tree.fNodes[0].fType.fName, "T");
   EXPECT_EQ(tree.fNodes[0].fType.fNamespace, "");
   EXPECT_EQ(tree.fNodes[0].fType.fFlags, TType::kTemplated);
   EXPECT_EQ(tree.fNodes[0].fType.fIndirection, TType::EIndirection::kNone);
   ASSERT_EQ(tree.fNodes[1].fNodeType, TNode::kExpr);

   tree = ParseType("::Type<(5 > 3), bar::Foo<(2 < 3)>>");
   ASSERT_EQ(tree.fNodes.size(), 10);
   ASSERT_EQ(tree.fNodes[0].fNodeType, TNode::kType);
   EXPECT_EQ(tree.fNodes[0].fType.fName, "Type");
   EXPECT_EQ(tree.fNodes[0].fType.fNamespace, "::");
   EXPECT_EQ(tree.fNodes[0].fType.fFlags, TType::kTemplated);
   EXPECT_EQ(tree.fNodes[0].fType.fIndirection, TType::EIndirection::kNone);
   ASSERT_EQ(tree.fNodes[1].fNodeType, TNode::kExpr);

   tree = ParseType("short *const *");
   ASSERT_EQ(tree.fNodes.size(), 3);
   const auto *root = &tree.fNodes[0];
   ASSERT_EQ(root->fNodeType, TNode::kType);
   EXPECT_EQ(root->fType.fName, "");
   EXPECT_EQ(root->fType.fNamespace, "");
   EXPECT_EQ(root->fType.fFlags, 0);
   EXPECT_EQ(root->fType.fIndirection, TType::EIndirection::kPtr);
   const auto *firstChild = root->fFirstChild;
   ASSERT_EQ(firstChild->fNodeType, TNode::kType);
   EXPECT_EQ(firstChild->fType.fName, "");
   EXPECT_EQ(firstChild->fType.fNamespace, "");
   EXPECT_EQ(firstChild->fType.fFlags, TType::kConst);
   EXPECT_EQ(firstChild->fType.fIndirection, TType::EIndirection::kPtr);
   const auto *secondChild = firstChild->fFirstChild;
   ASSERT_EQ(secondChild->fNodeType, TNode::kType);
   EXPECT_EQ(secondChild->fType.fName, "");
   EXPECT_EQ(secondChild->fType.fNamespace, "");
   EXPECT_EQ(secondChild->fType.fFlags, TType::kShort);
   EXPECT_EQ(secondChild->fType.fIndirection, TType::EIndirection::kNone);

   tree = ParseType("int *const");
   ASSERT_EQ(tree.fNodes.size(), 2);
   ASSERT_EQ(tree.fNodes[0].fNodeType, TNode::kType);
   EXPECT_EQ(tree.fNodes[0].fType.fName, "");
   EXPECT_EQ(tree.fNodes[0].fType.fNamespace, "");
   EXPECT_EQ(tree.fNodes[0].fType.fFlags, TType::kConst);
   EXPECT_EQ(tree.fNodes[0].fType.fIndirection, TType::EIndirection::kPtr);
   ASSERT_EQ(tree.fNodes[1].fNodeType, TNode::kType);
   EXPECT_EQ(tree.fNodes[1].fType.fName, "int");
   EXPECT_EQ(tree.fNodes[1].fType.fNamespace, "");
   EXPECT_EQ(tree.fNodes[1].fType.fFlags, 0);
   EXPECT_EQ(tree.fNodes[1].fType.fIndirection, TType::EIndirection::kNone);

   tree = ParseType("int const*");
   ASSERT_EQ(tree.fNodes.size(), 2);
   ASSERT_EQ(tree.fNodes[0].fNodeType, TNode::kType);
   EXPECT_EQ(tree.fNodes[0].fType.fName, "");
   EXPECT_EQ(tree.fNodes[0].fType.fNamespace, "");
   EXPECT_EQ(tree.fNodes[0].fType.fFlags, 0);
   EXPECT_EQ(tree.fNodes[0].fType.fIndirection, TType::EIndirection::kPtr);
   ASSERT_EQ(tree.fNodes[1].fNodeType, TNode::kType);
   EXPECT_EQ(tree.fNodes[1].fType.fName, "int");
   EXPECT_EQ(tree.fNodes[1].fType.fNamespace, "");
   EXPECT_EQ(tree.fNodes[1].fType.fFlags, TType::kConst);
   EXPECT_EQ(tree.fNodes[1].fType.fIndirection, TType::EIndirection::kNone);

   tree = ParseType("int const&");
   ASSERT_EQ(tree.fNodes.size(), 2);
   ASSERT_EQ(tree.fNodes[0].fNodeType, TNode::kType);
   EXPECT_EQ(tree.fNodes[0].fType.fName, "");
   EXPECT_EQ(tree.fNodes[0].fType.fNamespace, "");
   EXPECT_EQ(tree.fNodes[0].fType.fFlags, 0);
   EXPECT_EQ(tree.fNodes[0].fType.fIndirection, TType::EIndirection::kRef);
   ASSERT_EQ(tree.fNodes[1].fNodeType, TNode::kType);
   EXPECT_EQ(tree.fNodes[1].fType.fName, "int");
   EXPECT_EQ(tree.fNodes[1].fType.fNamespace, "");
   EXPECT_EQ(tree.fNodes[1].fType.fFlags, TType::kConst);
   EXPECT_EQ(tree.fNodes[1].fType.fIndirection, TType::EIndirection::kNone);

   tree = ParseType("double&&");
   ASSERT_EQ(tree.fNodes.size(), 2);
   ASSERT_EQ(tree.fNodes[0].fNodeType, TNode::kType);
   EXPECT_EQ(tree.fNodes[0].fType.fName, "");
   EXPECT_EQ(tree.fNodes[0].fType.fNamespace, "");
   EXPECT_EQ(tree.fNodes[0].fType.fFlags, 0);
   EXPECT_EQ(tree.fNodes[0].fType.fIndirection, TType::EIndirection::kRvRef);
   ASSERT_EQ(tree.fNodes[1].fNodeType, TNode::kType);
   EXPECT_EQ(tree.fNodes[1].fType.fName, "double");
   EXPECT_EQ(tree.fNodes[1].fType.fNamespace, "");
   EXPECT_EQ(tree.fNodes[1].fType.fFlags, 0);
   EXPECT_EQ(tree.fNodes[1].fType.fIndirection, TType::EIndirection::kNone);

   tree = ParseType("volatile int **const");
   ASSERT_EQ(tree.fNodes.size(), 3);
   root = &tree.fNodes[0];
   ASSERT_EQ(root->fNodeType, TNode::kType);
   EXPECT_EQ(root->fType.fName, "");
   EXPECT_EQ(root->fType.fNamespace, "");
   EXPECT_EQ(root->fType.fFlags, TType::kConst);
   EXPECT_EQ(root->fType.fIndirection, TType::EIndirection::kPtr);
   firstChild = root->fFirstChild;
   ASSERT_EQ(firstChild->fNodeType, TNode::kType);
   EXPECT_EQ(firstChild->fType.fName, "");
   EXPECT_EQ(firstChild->fType.fNamespace, "");
   EXPECT_EQ(firstChild->fType.fFlags, 0);
   EXPECT_EQ(firstChild->fType.fIndirection, TType::EIndirection::kPtr);
   secondChild = firstChild->fFirstChild;
   ASSERT_EQ(secondChild->fNodeType, TNode::kType);
   EXPECT_EQ(secondChild->fType.fName, "int");
   EXPECT_EQ(secondChild->fType.fNamespace, "");
   EXPECT_EQ(secondChild->fType.fFlags, TType::kVolatile);
   EXPECT_EQ(secondChild->fType.fIndirection, TType::EIndirection::kNone);

   tree = ParseType("int volatile *const *const volatile");
   ASSERT_EQ(tree.fNodes.size(), 3);
   root = &tree.fNodes[0];
   ASSERT_EQ(root->fNodeType, TNode::kType);
   EXPECT_EQ(root->fType.fName, "");
   EXPECT_EQ(root->fType.fNamespace, "");
   EXPECT_EQ(root->fType.fFlags, TType::kVolatile | TType::kConst);
   EXPECT_EQ(root->fType.fIndirection, TType::EIndirection::kPtr);
   firstChild = root->fFirstChild;
   ASSERT_EQ(firstChild->fNodeType, TNode::kType);
   EXPECT_EQ(firstChild->fType.fName, "");
   EXPECT_EQ(firstChild->fType.fNamespace, "");
   EXPECT_EQ(firstChild->fType.fFlags, TType::kConst);
   EXPECT_EQ(firstChild->fType.fIndirection, TType::EIndirection::kPtr);
   secondChild = firstChild->fFirstChild;
   ASSERT_EQ(secondChild->fNodeType, TNode::kType);
   EXPECT_EQ(secondChild->fType.fName, "int");
   EXPECT_EQ(secondChild->fType.fNamespace, "");
   EXPECT_EQ(secondChild->fType.fFlags, TType::kVolatile);
   EXPECT_EQ(secondChild->fType.fIndirection, TType::EIndirection::kNone);

   tree = ParseType("T<v[2]>");
   ASSERT_EQ(tree.fNodes.size(), 4);
   root = &tree.fNodes[0];
   ASSERT_EQ(root->fNodeType, TNode::kType);
   EXPECT_EQ(root->fType.fName, "T");
   firstChild = root->fFirstChild;
   ASSERT_EQ(firstChild->fNodeType, TNode::kExpr);
   EXPECT_EQ(firstChild->fExpr.fStr, "[");
   EXPECT_EQ(firstChild->fExpr.fType, TExpr::kBinOp);
   secondChild = firstChild->fFirstChild;
   ASSERT_EQ(secondChild->fNodeType, TNode::kExpr);
   EXPECT_EQ(secondChild->fExpr.fStr, "v");
   EXPECT_EQ(secondChild->fExpr.fType, TExpr::kLeaf);
   ASSERT_NE(secondChild->fNextSibling, nullptr);
   ASSERT_EQ(secondChild->fNextSibling->fNodeType, TNode::kExpr);
   EXPECT_EQ(secondChild->fNextSibling->fExpr.fStr, "2");
   EXPECT_EQ(secondChild->fNextSibling->fExpr.fType, TExpr::kLeaf);

   tree = ParseType("T<(a.b->b + c.d)>");
   ASSERT_EQ(tree.fNodes.size(), 11);
}

TEST(TypeParser, ParseFuncPtr)
{
   auto tree = ParseType("const int(*)(void)");
   ASSERT_EQ(tree.fNodes.size(), 3);
   auto root = &tree.fNodes[0];
   ASSERT_EQ(root->fNodeType, TNode::kType);
   EXPECT_EQ(root->fType.fName, "");
   EXPECT_EQ(root->fType.fIndirection, TType::EIndirection::kFuncPtr);
   EXPECT_EQ(root->fNumChildren, 2);
   auto ret = root->fFirstChild;
   ASSERT_EQ(ret->fNodeType, TNode::kType);
   EXPECT_EQ(ret->fType.fName, "int");
   EXPECT_EQ(ret->fType.fFlags, TType::kConst);
   EXPECT_EQ(ret->fType.fIndirection, TType::EIndirection::kNone);
   auto arg = ret->fNextSibling;
   ASSERT_EQ(arg->fNodeType, TNode::kType);
   EXPECT_EQ(arg->fType.fName, "void");
   EXPECT_EQ(arg->fType.fFlags, 0);
   EXPECT_EQ(arg->fType.fIndirection, TType::EIndirection::kNone);
}

TEST(TypeParser, ParseScoped)
{
   auto tree = ParseType("const Foo<>::Bar<V>::baz &");
   ASSERT_EQ(tree.fNodes.size(), 5);
   auto root = &tree.fNodes[0];
   ASSERT_EQ(root->fNodeType, TNode::kType);
   EXPECT_EQ(root->fFlags, 0);
   EXPECT_EQ(root->fType.fName, "");
   EXPECT_EQ(root->fType.fIndirection, TType::EIndirection::kRef);
   EXPECT_EQ(root->fNumChildren, 1);
   auto scoped = root->fFirstChild;
   ASSERT_EQ(scoped->fNodeType, TNode::kType);
   ASSERT_EQ(scoped->fNumChildren, 1);
   EXPECT_EQ(scoped->fFlags, TNode::kScoped);
   EXPECT_EQ(scoped->fType.fName, "baz");
   EXPECT_EQ(scoped->fType.fFlags, TType::kConst);
   EXPECT_EQ(scoped->fType.fIndirection, TType::EIndirection::kNone);
   auto middle = scoped->fFirstChild;
   ASSERT_EQ(middle->fNodeType, TNode::kType);
   EXPECT_EQ(middle->fType.fName, "Bar");
   EXPECT_EQ(middle->fType.fFlags, TType::kTemplated);
   EXPECT_EQ(middle->fType.fIndirection, TType::EIndirection::kNone);
   EXPECT_EQ(middle->fNumChildren, 2);
   auto outer = middle->fFirstChild;
   auto tmpArg = outer->fNextSibling;
   ASSERT_EQ(tmpArg->fNodeType, TNode::kType);
   EXPECT_EQ(tmpArg->fType.fName, "V");
   EXPECT_EQ(tmpArg->fType.fFlags, 0);
   EXPECT_EQ(tmpArg->fType.fIndirection, TType::EIndirection::kNone);
   EXPECT_EQ(tmpArg->fNumChildren, 0);
   ASSERT_EQ(outer->fNodeType, TNode::kType);
   EXPECT_EQ(outer->fType.fName, "Foo");
   EXPECT_EQ(outer->fType.fFlags, TType::kTemplated);
   EXPECT_EQ(outer->fType.fIndirection, TType::EIndirection::kNone);
   EXPECT_EQ(outer->fNumChildren, 0);
}

TEST(TypeParser, ShortType)
{
   EXPECT_EQ(ShortType("const int").Unwrap(), "int");
   EXPECT_EQ(ShortType("short*").Unwrap(), "short*");
   EXPECT_EQ(ShortType("const volatile class TNamed**").Unwrap(), "TNamed**");
   EXPECT_EQ(ShortType("unsigned long int").Unwrap(), "unsigned long int");
   EXPECT_EQ(ShortType("T<C, Foo ...>").Unwrap(), "T<C,Foo...>");

   EXPECT_EQ(
      ShortType("std::pmr::polymorphic_allocator<std::sub_match<__gnu_cxx::__normal_iterator<const "
                "char*,std::basic_string<char,std::char_traits<char>,std::pmr::polymorphic_allocator<char> > > > >")
         .Unwrap(),
      "std::pmr::polymorphic_allocator<std::sub_match<__gnu_cxx::__normal_iterator<"
      "char*,std::basic_string<char,std::char_traits<char>,std::pmr::polymorphic_allocator<char>>>>>");
}

TEST(TypeParser, ShortTypeExpr)
{
   EXPECT_EQ(ShortType("std::conditional_t<(T > 32), int, float>").Unwrap(), "std::conditional_t<(T>32),int,float>");
   EXPECT_EQ(ShortType("std::conditional_t<(T < 32), int, float>").Unwrap(), "std::conditional_t<(T<32),int,float>");
   EXPECT_EQ(ShortType("std::conditional_t<(T < (A >= (32))), int, float>").Unwrap(),
             "std::conditional_t<(T<(A>=(32))),int,float>");
   EXPECT_EQ(ShortType("std::function<bool(std::vector<int>)>").Unwrap(), "std::function<bool(std::vector<int>)>");
   EXPECT_EQ(ShortType("T<2, *x>").Unwrap(), "T<2,*x>");
   EXPECT_EQ(ShortType("T<2, (x + 1 > 2)>").Unwrap(), "T<2,(x+1>2)>");
   EXPECT_EQ(ShortType("T<(a->b)>").Unwrap(), "T<(a->b)>");
   EXPECT_EQ(ShortType("T<(a.b->b + c.d)>").Unwrap(), "T<(a.b->b+c.d)>");
}

TEST(TypeParser, ShortTypeAmbiguous)
{
   // We treat any '>' outside of a parens expression as template delimiters.
   EXPECT_FALSE(bool(ShortType("T<2>3>")));
   EXPECT_EQ(ShortType("T<(2>3)>").Unwrap(), "T<(2>3)>");
   EXPECT_FALSE(bool(ShortType("T<2>>3>")));
   EXPECT_EQ(ShortType("T<(2>>3)>").Unwrap(), "T<(2>>3)>");
}

TEST(TypeParser, ShortTypeArray)
{
   EXPECT_EQ(ShortType("T<v[2]+1>").Unwrap(), "T<v[2]+1>");
   EXPECT_EQ(ShortType("T<v[2 + a[1]]>").Unwrap(), "T<v[2+a[1]]>");
   EXPECT_EQ(ShortType("T<a[b[c+(d>>2)]] - a[1]>").Unwrap(), "T<a[b[c+(d>>2)]]-a[1]>");
   EXPECT_EQ(ShortType("T<a[0][1] - b[(a[1]+2)][5]>").Unwrap(), "T<a[0][1]-b[(a[1]+2)][5]>");
   EXPECT_EQ(ShortType("T<x[i]...>").Unwrap(), "T<x[i]...>");
   EXPECT_EQ(ShortType("unsigned char[]").Unwrap(), "unsigned char[]");
   EXPECT_EQ(ShortType("unsigned char[][2][]").Unwrap(), "unsigned char[][2][]");
   EXPECT_EQ(ShortType("T<Foo<2>[3 + 4]>").Unwrap(), "T<Foo<2>[3+4]>");
}

TEST(TypeParser, ShortTypeParam)
{
   EXPECT_EQ(ShortType("T<type-parameter-0-0, C<int, type-parameter-1-1>>").Unwrap(),
             "T<type-parameter-0-0,C<int,type-parameter-1-1>>");
}

TEST(TypeParser, ShortTypeScoped)
{
   EXPECT_EQ(ShortType("Foo::Bar::baz").Unwrap(), "Foo::Bar::baz");
   EXPECT_EQ(ShortType("const Foo::Bar::baz*").Unwrap(), "Foo::Bar::baz*");
   EXPECT_EQ(ShortType("typename A<B[]>::B<int(*)(void)>:: C").Unwrap(), "typename A<B[]>::B<int(*)(void)>::C");

   auto tree = ParseType("const Foo::Bar::baz*");
   EXPECT_EQ(StringifyNode(tree.fNodes[0]), "const Foo::Bar::baz*");
}

TEST(TypeParser, PrintNodeFlags)
{
   auto tree = ParseType("A<B<C>>");
   ASSERT_EQ(tree.fNodes.size(), 3);
   EXPECT_EQ(StringifyNode(tree.fNodes[0], kSpaceAfterClosingTemplate), "A<B<C> >");

   tree = ParseType("A<B<C>, D<>>");
   ASSERT_EQ(tree.fNodes.size(), 4);
   EXPECT_EQ(StringifyNode(tree.fNodes[0], kSpaceAfterClosingTemplate), "A<B<C>,D<> >");

   tree = ParseType("std::is_assignable<std::__future_base::_Result<void>*&,std::__future_base::_Result<void>*>");
   EXPECT_EQ(StringifyNode(tree.fNodes[0], kSpaceAfterClosingTemplate),
             "std::is_assignable<std::__future_base::_Result<void>*&,std::__future_base::_Result<void>*>");

   tree = ParseType("A<B<C>>::D");
   EXPECT_EQ(StringifyNode(tree.fNodes[0], kSpaceAfterClosingTemplate), "A<B<C> >::D");

   tree = ParseType("typename enable_if<(N>1&&N%2==0),void>::type");
   EXPECT_EQ(StringifyNode(tree.fNodes[0], kSpaceAfterClosingTemplate), "typename enable_if<(N>1&&N%2==0),void>::type");
}

TEST(TypeParser, ShortTypeFnPtr)
{
   EXPECT_EQ(ShortType("const int(*)(void)").Unwrap(), "int(*)(void)");
   EXPECT_EQ(ShortType("void*(*)()").Unwrap(), "void*(*)()");
   EXPECT_EQ(ShortType("void*(**[])()").Unwrap(), "void*(**[])()");
   EXPECT_EQ(ShortType("Foo<void>*(*&&)(Bar*[],Baz<>)").Unwrap(), "Foo<void>*(*&&)(Bar*[],Baz<>)");
   EXPECT_EQ(ShortType("A<B>(*)(Foo, Bar<2>)").Unwrap(), "A<B>(*)(Foo,Bar<2>)");
   EXPECT_EQ(ShortType("::__untag_result<__deduce_visit_result<type-parameter-1-0>(*)(type-parameter-1-1)>").Unwrap(),
             "::__untag_result<__deduce_visit_result<type-parameter-1-0>(*)(type-parameter-1-1)>");
   EXPECT_EQ(ShortType("_Multi_array<type-parameter-0-0(*)(type-parameter-0-1,type-parameter-0-3...),__dimensions...>")
                .Unwrap(),
             "_Multi_array<type-parameter-0-0(*)(type-parameter-0-1,type-parameter-0-3...),__dimensions...>");
}
