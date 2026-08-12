#include "shine/lexer.h"
#include <cctype>
#include <unordered_map>
#include "shine/error.h"

namespace shine {

static const std::unordered_map<std::string, TokenKind> kKeywords = {
    {"fn", TokenKind::KwFn},
    {"let", TokenKind::KwLet}, {"var", TokenKind::KwVar},
    {"int", TokenKind::KwInt}, {"void", TokenKind::KwVoid},
    {"if", TokenKind::KwIf}, {"else", TokenKind::KwElse},
    {"loop", TokenKind::KwLoop}, {"stop", TokenKind::KwStop}, {"cont", TokenKind::KwCont},
};

const char* tokenName(TokenKind k) {
    switch (k) {
        case TokenKind::IntLiteral: return "int literal";
        case TokenKind::StringLiteral: return "string literal";
        case TokenKind::Identifier: return "identifier";
        case TokenKind::KwFn: return "'fn'";
        case TokenKind::KwReturn: return "'r/'";
        case TokenKind::KwLet: return "'let'";
        case TokenKind::KwVar: return "'var'";
        case TokenKind::KwInt: return "'int'";
        case TokenKind::KwVoid: return "'void'";
        case TokenKind::KwIf: return "'if'";
        case TokenKind::KwElse: return "'else'";
        case TokenKind::KwLoop: return "'loop'";
        case TokenKind::KwStop: return "'stop'";
        case TokenKind::KwCont: return "'cont'";
        case TokenKind::LParen: return "'('";
        case TokenKind::RParen: return "')'";
        case TokenKind::LBrace: return "'{'";
        case TokenKind::RBrace: return "'}'";
        case TokenKind::Semicolon: return "';'";
        case TokenKind::Comma: return "','";
        case TokenKind::Colon: return "':'";
        case TokenKind::Dot: return "'.'";
        case TokenKind::Arrow: return "'->'";
        case TokenKind::Equal: return "'='";
        case TokenKind::Plus: return "'+'";
        case TokenKind::Minus: return "'-'";
        case TokenKind::Star: return "'*'";
        case TokenKind::Slash: return "'/'";
        case TokenKind::Amp: return "'&'";
        case TokenKind::EqualEqual: return "'=='";
        case TokenKind::BangEqual: return "'!='";
        case TokenKind::Less: return "'<'";
        case TokenKind::LessEqual: return "'<='";
        case TokenKind::Greater: return "'>'";
        case TokenKind::GreaterEqual: return "'>='";
        case TokenKind::EndOfFile: return "end of file";
        default: return "invalid token";
    }
}

Lexer::Lexer(std::string src, std::string file) : src_(std::move(src)), file_(std::move(file)) {}

bool Lexer::atEnd() const { return pos_ >= src_.size(); }

char Lexer::peek(int off) const {
    size_t i = pos_ + (size_t)off;
    return i < src_.size() ? src_[i] : '\0';
}

char Lexer::advance() {
    char c = src_[pos_++];
    if (c == '\n') { line_++; col_ = 1; } else { col_++; }
    return c;
}

bool Lexer::match(char c) {
    if (atEnd() || peek() != c) return false;
    advance();
    return true;
}

void Lexer::err(const std::string& msg) const {
    throw CompileError({file_, line_, col_}, msg);
}

void Lexer::err(Err code, const std::vector<std::string>& args) const {
    throw CompileError({file_, line_, col_}, code, args);
}

void Lexer::skipTrivia() {
    for (;;) {
        char c = peek();
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') { advance(); continue; }
        if (c == '/' && peek(1) == '/') { while (!atEnd() && peek() != '\n') advance(); continue; }
        if (c == '/' && peek(1) == '*') {
            advance(); advance();
            while (!atEnd() && !(peek() == '*' && peek(1) == '/')) advance();
            if (atEnd()) err(Err::UnterminatedComment);
            advance(); advance();
            continue;
        }
        break;
    }
}

Token Lexer::ident() {
    int l = line_, c = col_;
    std::string s;
    while (!atEnd() && (std::isalnum((unsigned char)peek()) || peek() == '_')) s += advance();

    if (s == "r" && peek() == '/' && peek(1) != '/' && peek(1) != '*') {
        advance();
        return {TokenKind::KwReturn, "r/", 0, {file_, l, c}};
    }

    auto it = kKeywords.find(s);
    return {it != kKeywords.end() ? it->second : TokenKind::Identifier, s, 0, {file_, l, c}};
}

Token Lexer::number() {
    int l = line_, c = col_;
    std::string s;
    while (!atEnd() && std::isdigit((unsigned char)peek())) s += advance();
    return {TokenKind::IntLiteral, s, std::stoll(s), {file_, l, c}};
}

Token Lexer::str() {
    int l = line_, c = col_;
    advance();
    std::string v;
    while (!atEnd() && peek() != '"') {
        char ch = advance();
        if (ch != '\\') { v += ch; continue; }
        if (atEnd()) err(Err::UnterminatedString);
        switch (advance()) {
            case 'n': v += '\n'; break;
            case 't': v += '\t'; break;
            case '"': v += '"'; break;
            case '\\': v += '\\'; break;
            default: err(Err::BadEscapeSequence);
        }
    }
    if (atEnd()) err(Err::UnterminatedString);
    advance();
    return {TokenKind::StringLiteral, v, 0, {file_, l, c}};
}

std::vector<Token> Lexer::tokenize() {
    std::vector<Token> out;
    for (;;) {
        skipTrivia();
        if (atEnd()) break;
        char c = peek();
        if (std::isalpha((unsigned char)c) || c == '_') { out.push_back(ident()); continue; }
        if (std::isdigit((unsigned char)c)) { out.push_back(number()); continue; }
        if (c == '"') { out.push_back(str()); continue; }

        int l = line_, col = col_;
        auto tok = [&](TokenKind k, const char* t) { out.push_back({k, t, 0, {file_, l, col}}); };
        switch (c) {
            case '(': advance(); tok(TokenKind::LParen, "("); break;
            case ')': advance(); tok(TokenKind::RParen, ")"); break;
            case '{': advance(); tok(TokenKind::LBrace, "{"); break;
            case '}': advance(); tok(TokenKind::RBrace, "}"); break;
            case ';': advance(); tok(TokenKind::Semicolon, ";"); break;
            case ',': advance(); tok(TokenKind::Comma, ","); break;
            case ':': advance(); tok(TokenKind::Colon, ":"); break;
            case '.': advance(); tok(TokenKind::Dot, "."); break;
            case '=':
                advance();
                if (match('=')) tok(TokenKind::EqualEqual, "==");
                else tok(TokenKind::Equal, "=");
                break;
            case '+': advance(); tok(TokenKind::Plus, "+"); break;
            case '*': advance(); tok(TokenKind::Star, "*"); break;
            case '&': advance(); tok(TokenKind::Amp, "&"); break;
            case '/': advance(); tok(TokenKind::Slash, "/"); break;
            case '!':
                advance();
                if (match('=')) tok(TokenKind::BangEqual, "!=");
                else err(Err::UnexpectedChar, {"!"});
                break;
            case '<':
                advance();
                if (match('=')) tok(TokenKind::LessEqual, "<=");
                else tok(TokenKind::Less, "<");
                break;
            case '>':
                advance();
                if (match('=')) tok(TokenKind::GreaterEqual, ">=");
                else tok(TokenKind::Greater, ">");
                break;
            case '-':
                advance();
                if (match('>')) tok(TokenKind::Arrow, "->");
                else tok(TokenKind::Minus, "-");
                break;
            default: err(Err::UnexpectedChar, {std::string(1, c)});
        }
    }
    out.push_back({TokenKind::EndOfFile, "", 0, {file_, line_, col_}});
    return out;
}

}