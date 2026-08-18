#include "shine/lexer.h"
#include "test_framework.h"

using namespace shine;

TEST(empty_input_is_just_eof) {
    Lexer l("", "t.shine");
    auto toks = l.tokenize();
    ASSERT_EQ(toks.size(), 1u);
    ASSERT_TRUE(toks[0].kind == TokenKind::EndOfFile);
}

TEST(keyword_and_identifier) {
    Lexer l("fn main", "t.shine");
    auto toks = l.tokenize();
    ASSERT_TRUE(toks[0].kind == TokenKind::KwFn);
    ASSERT_TRUE(toks[1].kind == TokenKind::Identifier);
    ASSERT_EQ(toks[1].text, "main");
}

TEST(int_literal_value) {
    Lexer l("42", "t.shine");
    ASSERT_EQ(l.tokenize()[0].intValue, 42);
}

TEST(string_literal_escapes) {
    Lexer l(R"("a\nb")", "t.shine");
    ASSERT_EQ(l.tokenize()[0].text, "a\nb");
}

TEST(comments_skipped) {
    Lexer l("// x\nfn /* y */ main", "t.shine");
    auto toks = l.tokenize();
    ASSERT_TRUE(toks[0].kind == TokenKind::KwFn);
    ASSERT_TRUE(toks[1].kind == TokenKind::Identifier);
}

TEST(arrow_token) {
    Lexer l("->", "t.shine");
    ASSERT_TRUE(l.tokenize()[0].kind == TokenKind::Arrow);
}

TEST(v0_2_tokens) {
    Lexer l("let var + - * / == != < <= > >=", "t.shine");
    auto toks = l.tokenize();
    ASSERT_TRUE(toks[0].kind == TokenKind::KwLet);
    ASSERT_TRUE(toks[1].kind == TokenKind::KwVar);
    ASSERT_TRUE(toks[2].kind == TokenKind::Plus);
    ASSERT_TRUE(toks[3].kind == TokenKind::Minus);
    ASSERT_TRUE(toks[4].kind == TokenKind::Star);
    ASSERT_TRUE(toks[5].kind == TokenKind::Slash);
    ASSERT_TRUE(toks[6].kind == TokenKind::EqualEqual);
    ASSERT_TRUE(toks[7].kind == TokenKind::BangEqual);
    ASSERT_TRUE(toks[8].kind == TokenKind::Less);
    ASSERT_TRUE(toks[9].kind == TokenKind::LessEqual);
    ASSERT_TRUE(toks[10].kind == TokenKind::Greater);
    ASSERT_TRUE(toks[11].kind == TokenKind::GreaterEqual);
}

TEST(r_slash_is_return_keyword) {
    Lexer l("r/0;", "t.shine");
    auto toks = l.tokenize();
    ASSERT_TRUE(toks[0].kind == TokenKind::KwReturn);
    ASSERT_TRUE(toks[1].kind == TokenKind::IntLiteral);
}

TEST(bare_r_identifier_still_works) {
    Lexer l("r(5)", "t.shine");
    auto toks = l.tokenize();
    ASSERT_TRUE(toks[0].kind == TokenKind::Identifier);
    ASSERT_EQ(toks[0].text, "r");
}

TEST(r_before_comment_is_identifier_not_return) {
    Lexer l("r // comment", "t.shine");
    auto toks = l.tokenize();
    ASSERT_TRUE(toks[0].kind == TokenKind::Identifier);
    ASSERT_EQ(toks[0].text, "r");
}

TEST(bracket_tokens) {
    Lexer l("[5]i32 arr[0]", "t.shine");
    auto toks = l.tokenize();
    ASSERT_TRUE(toks[0].kind == TokenKind::LBracket);
    ASSERT_TRUE(toks[1].kind == TokenKind::IntLiteral);
    ASSERT_TRUE(toks[2].kind == TokenKind::RBracket);
    ASSERT_TRUE(toks[3].kind == TokenKind::Identifier);
    ASSERT_TRUE(toks[4].kind == TokenKind::Identifier);
    ASSERT_EQ(toks[4].text, "arr");
    ASSERT_TRUE(toks[5].kind == TokenKind::LBracket);
    ASSERT_TRUE(toks[6].kind == TokenKind::IntLiteral);
    ASSERT_TRUE(toks[7].kind == TokenKind::RBracket);
}
