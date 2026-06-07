#pragma once

#include <vector>
#include <string>

enum class TokenType
{
    WORD,
    COMMAND,
    PIPE,
    REDIRECT_IN,
    REDIRECT_OUT,
    REDIRECT_APPEND,
    FILE,
    OR,
    AND,
    LPAREN,
    RPAREN,
    LRPAREN,
    VARIABLE,
    END,
    SINGLE_QUOTE,
    DOUBLE_QUOTE,
    AMPERSAND,
    COMMAND_SUBSTITUTION,
    SKIP
};

struct ASTNode
{
    std::string value;
    std::vector<ASTNode *> children;
    TokenType type;
};

inline const std::vector<std::string> specifiedTokens = {"|", "<", ">", ">>", "||", "&&", "(", ")", ";", "&", "$", "\"", "\'", "\\", "~", "{", "}", "#"};
inline const std::vector<std::string> binaryOperators = {"|", "||", "&&", ">", ">>", "<"};

inline const std::vector<std::string> builtInCommands = {"cd", "exit", "declare", "script"};