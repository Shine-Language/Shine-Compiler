#pragma once
#include <string>
#include <vector>
#include "shine/error.h"
#include "shine/token.h"

namespace shine {

class Lexer {
public:
    Lexer(std::string src, std::string file);
    std::vector<Token> tokenize();

private:
    char peek(int off = 0) const;
    char advance();
    bool match(char c);
    bool atEnd() const;
    void skipTrivia();
    Token ident();
    Token number();
    Token str();
    [[noreturn]] void err(const std::string& msg) const;
    [[noreturn]] void err(Err code, const std::vector<std::string>& args = {}) const;

    std::string src_, file_;
    size_t pos_ = 0;
    int line_ = 1, col_ = 1;
};

}