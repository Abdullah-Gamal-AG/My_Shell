#include <algorithm>
#include <unistd.h>
#include <vector>
#include <string>
#include <sys/wait.h>
#include <iostream>
#include <filesystem>
#include <stack>
#include <map>
using namespace std;

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
    SKIP
};

struct ASTNode
{
    string value;
    vector<ASTNode *> children;
    TokenType type;
};

vector<string> specifiedTokens = {"|", "<", ">", ">>", "||", "&&", "(", ")", ";", "&", "$", "\"", "\'", "\\","~","{","}"};
vector<string> binaryOperators = {"|", "||", "&&", ">", ">>", "<"};

void handle_double_quote(const vector<ASTNode *> &tokens);
void expand_variables(vector<ASTNode *> &tokens);
vector<ASTNode *> parse_tokens(const vector<ASTNode *> &tokens);

size_t find_matching_rparen(const vector<ASTNode *> &tokens, size_t openIndex)
{
    int depth = 0;
    for (size_t index = openIndex; index < tokens.size(); ++index)
    {
        if (tokens[index]->type == TokenType::LPAREN)
        {
            ++depth;
        }
        else if (tokens[index]->type == TokenType::RPAREN)
        {
            --depth;
            if (depth == 0)
                return index;
        }
    }
    return tokens.size();
}

vector<ASTNode *> tokenize(const string &input)
{
    vector<ASTNode *> tokens;
    bool inSingleQuote = false;
    bool inDoubleQuote = false;
    bool isEscaped = false;
    bool escapeNext = false;
    stack<char> parenStack;
    map<char, char> parenPairs = {{')', '('},{'}', '{'}};
    string currentToken;
    TokenType type;
    for (size_t i = 0; i < input.size(); i++)
    {
        char c = input[i];
        if(escapeNext)
            isEscaped = false;
        if(isEscaped)
            escapeNext = true;

        if (isspace(c) && !inSingleQuote && !inDoubleQuote)
        {
            if (!currentToken.empty())
            {
                tokens.push_back(new ASTNode{currentToken, {}, TokenType::WORD});
                currentToken.clear();
            }
        }
        else if (find(specifiedTokens.begin(), specifiedTokens.end(), string(1, c)) != specifiedTokens.end())
        {
            if (!currentToken.empty() && !inSingleQuote && !inDoubleQuote)
            {
                tokens.push_back(new ASTNode{currentToken, {}, TokenType::WORD});
                currentToken.clear();
            }
            string tokenStr(1, c);
            if (c == '>' && i + 1 < input.size() && input[i + 1] == '>')
            {
                tokenStr += '>';
                ++i;
            }
            else if (c == '&' && i + 1 < input.size() && input[i + 1] == '&')
            {
                tokenStr += '&';
                ++i;
            }
            else if (c == '|' && i + 1 < input.size() && input[i + 1] == '|')
            {
                tokenStr += '|';
                ++i;
            }
            if (tokenStr == "|" && !inSingleQuote && !inDoubleQuote)
                type = TokenType::PIPE;
            else if (tokenStr == "<" && !inSingleQuote && !inDoubleQuote)
                type = TokenType::REDIRECT_IN;
            else if (tokenStr == ">" && !inSingleQuote && !inDoubleQuote)
                type = TokenType::REDIRECT_OUT;
            else if (tokenStr == ">>" && !inSingleQuote && !inDoubleQuote)
                type = TokenType::REDIRECT_APPEND;
            else if (tokenStr == "||" && !inSingleQuote && !inDoubleQuote)
                type = TokenType::OR;
            else if (tokenStr == "&&" && !inSingleQuote && !inDoubleQuote)
                type = TokenType::AND;
            else if (tokenStr == "(" && !inSingleQuote && !inDoubleQuote)
            {
                type = TokenType::LPAREN;
                parenStack.push('(');
            }   
            else if (tokenStr == ")" && !inSingleQuote && !inDoubleQuote)
            {
                if (parenStack.empty() || parenStack.top() != '(')
                {
                    cerr << "Error: Unmatched parentheses detected.+1" << endl;
                    return {};
                }
                parenStack.pop();
                type = TokenType::RPAREN;
            }  
            else if (tokenStr == "~" && !inSingleQuote && !inDoubleQuote && !isEscaped)
            {
                const char *homeDir = getenv("HOME");
                if (homeDir)
                {
                    currentToken += string(homeDir);
                }
            }
            else if (tokenStr == "$" && !inSingleQuote && !inDoubleQuote && !isEscaped)
            {
                if (isEscaped)
                    currentToken += '$';
                else
                {
                    // Expand ${VAR} or $VAR inline into currentToken
                    if (i + 1 < input.size() && input[i + 1] == '{')
                    {
                        size_t j = i + 2;
                        string varName;
                        while (j < input.size() && (isalnum((unsigned char)input[j]) || input[j] == '_'))
                        {
                            varName += input[j];
                            j++;
                        }
                        if (j < input.size() && input[j] == '}')
                        {
                            const char *varValue = getenv(varName.c_str());
                            if (varValue)
                                currentToken += string(varValue);
                            i = j; // consume up to closing brace
                        }
                        else
                        {
                            cerr << "Error: Invalid variable syntax detected." << endl;
                        }
                    }
                    else if (i + 1 < input.size() && (isalpha((unsigned char)input[i+1]) || input[i+1] == '_'))
                    {
                        size_t j = i + 1;
                        string varName;
                        while (j < input.size() && (isalnum((unsigned char)input[j]) || input[j] == '_'))
                        {
                            varName += input[j];
                            j++;
                        }
                        const char *varValue = getenv(varName.c_str());
                        if (varValue)
                            currentToken += string(varValue);
                        i = j - 1; // consume variable characters
                    }
                    else
                    {
                        cerr << "Error: Invalid variable syntax detected." << endl;
                    }
                }
            }
            else if (tokenStr == ";" && !inSingleQuote && !inDoubleQuote)
                type = TokenType::END;
            else if (tokenStr == "&" && !inSingleQuote && !inDoubleQuote)
                type = TokenType::AMPERSAND;
            else if (tokenStr == "\\" && !inSingleQuote)
            {
                if (inDoubleQuote)
                    currentToken += '\\';
                if (isEscaped)
                    currentToken += '\\';
                else
                    isEscaped = true;
            }
            else if (tokenStr == "\"" && !inSingleQuote)
            {
                if(isEscaped)
                    currentToken += '\"';
                else
                {
                    if (!inSingleQuote && !currentToken.empty())
                    {
                        tokens.push_back(new ASTNode{currentToken, {}, TokenType::DOUBLE_QUOTE}); // Treat as double quote if inside double quotes
                        currentToken.clear();
                    }
                    inDoubleQuote = !inDoubleQuote; // Toggle double quote state
                }
            }
            else if (tokenStr == "\'" && !isEscaped && !inDoubleQuote)
            {
                if(isEscaped)
                    currentToken += '\'';
                else
                {
                    if (!inDoubleQuote && !currentToken.empty())
                    {
                        tokens.push_back(new ASTNode{currentToken, {}, TokenType::SINGLE_QUOTE}); // Treat as single quote if inside single quotes
                        currentToken.clear();
                    }
                    inSingleQuote = !inSingleQuote; // Toggle single quote state
                }
            }
            else
                type = TokenType::WORD; // Default case
            if((inDoubleQuote || inSingleQuote) && c!='\"' && c!='\'' && c!='\\')
            {
                currentToken += tokenStr;
            }
            else if(c!='\"' && c!='\'' && c!='\\'&&c!='~'&&c!='$')
            {
                tokens.push_back(new ASTNode{tokenStr, {}, type});
            }     
        }
        else
        {
            currentToken += c;
        }
    }
    if (!currentToken.empty())
    {
        tokens.push_back(new ASTNode{currentToken, {}, TokenType::WORD});
    }
    if (inSingleQuote || inDoubleQuote)
    {
        cerr << "Error: Unmatched quote detected." << endl;
        return {};
    }
    if(!parenStack.empty())
    {
        cerr << "Error: Unmatched parentheses detected.+2" << endl;
        return {};
    }
    /*for (auto token : tokens)
    {
        cout << "Token: " << token->value << ", Type: " << static_cast<int>(token->type) << endl;
    }*/
    //expand_variables(tokens);
    handle_double_quote(tokens);
    return tokens;
}









/*void expand_variables(vector<ASTNode *> &tokens)
{
    bool inVariable = false;
    for (int i = 0; i < tokens.size(); ++i)
    {
        ASTNode *token = tokens[i];
        if (token->type == TokenType::VARIABLE)
        {
            inVariable = true;
            continue;
        }
        if (inVariable)
        {
            const char *varValue = getenv(token->value.c_str());
            if (varValue)
            {
                token->value = string(varValue);
            }
            else
            {
                token->value = ""; // If variable is not set, replace with empty string
            }
            inVariable = false;
        }
    }
}*/










void handle_double_quote(const vector<ASTNode *> &tokens)
{
    bool isEscaped = false;
    string result;
    for(auto token : tokens)
    {
        if(token->type == TokenType::DOUBLE_QUOTE)
        {
            for(size_t i = 0; i < token->value.size(); i++)
            {
                if(isEscaped)
                {
                    result += token->value[i];
                    isEscaped = false;
                    continue;
                }
                if(token->value[i] == '\\')
                {
                    isEscaped = true;
                    continue;
                }
                if(token->value[i] == '~')
                {
                    const char *homeDir = getenv("HOME");
                    if (homeDir)
                    {
                        result += string(homeDir);
                    }
                    continue;
                }
                if(token->value[i] == '$')
                {
                    string varName="";
                    if(i +1 < token->value.size() && token->value[i+1] == '{')
                    {
                        i++;
                    }
                    else
                    {
                        cerr << "Error: Invalid variable syntax detected." << endl;
                        continue;
                    }
                    while(i + 1 < token->value.size())
                    {
                        if(token->value[i+1] == '}')
                        {
                            i++;
                            break;
                        }
                        if(isalnum(token->value[i+1]) || token->value[i+1] == '_')
                        {
                            varName += token->value[i+1];
                            i++;
                        }
                        else
                        {
                            break;
                        }
                    }
                    const char *varValue = getenv(varName.c_str());
                    if (varValue)
                    {
                        result += string(varValue);
                    }
                    continue;
                }
                result += token->value[i];
            }
            token->type = TokenType::WORD;
            token->value = result;
            result.clear();
            isEscaped = false;
        }
    }
}













/*vector<ASTNode *> parse_tokens(const vector<ASTNode *> &tokens)
{
    ASTNode currentNode;
    vector<ASTNode *> result;
    for (int i = 0; i < tokens.size(); i++)
    {
        if (tokens[i]->type == TokenType::LPAREN)
        {
            if (!currentNode.children.empty())
            {
                result.push_back(new ASTNode{currentNode.value, currentNode.children, currentNode.type});
                currentNode.children.clear();
            }

            size_t closeIndex = find_matching_rparen(tokens, i);
            if (closeIndex == tokens.size())
            {
                cerr << "Error: Unmatched parentheses detected.+3" << endl;
                return {};
            }

            vector<ASTNode *> nestedTokens(tokens.begin() + i + 1, tokens.begin() + closeIndex);
            vector<ASTNode *> nestedResult = parse_tokens(nestedTokens);
            if (nestedResult.empty())
                return {};

            result.push_back(new ASTNode{"group", nestedResult, TokenType::LRPAREN});
            i = static_cast<int>(closeIndex);
            continue;
        }

        if (tokens[i]->type == TokenType::RPAREN && (currentNode.type == TokenType::LPAREN || currentNode.type == TokenType::LRPAREN))
        {
            cerr << "Error: Unmatched parentheses detected.+4" << endl;
            return {};
        }

        if (tokens[i]->type == TokenType::WORD || tokens[i]->type == TokenType::SINGLE_QUOTE || tokens[i]->type == TokenType::DOUBLE_QUOTE)
        {
            currentNode.children.push_back(tokens[i]);
            currentNode.type = TokenType::COMMAND;
        }
        else
        {
            if (!currentNode.children.empty())
            {
                result.push_back(new ASTNode{currentNode.value, currentNode.children, currentNode.type});
                currentNode.children.clear();
            }
            if (tokens[i]->type == TokenType::PIPE || tokens[i]->type == TokenType::OR || tokens[i]->type == TokenType::AND || tokens[i]->type == TokenType::END)
            {
                ASTNode *temp = new ASTNode{};
                currentNode.type = TokenType::PIPE;
                currentNode.children.push_back(result[result.size() - 1]);
                result.erase(result.end() - 1);
                temp->type = TokenType::COMMAND;
                i++;
                for (int j = i + 1; j < tokens.size(); j++)
                {
                    if (tokens[j]->type == TokenType::WORD)
                    {
                        temp->children.push_back(tokens[j]);
                    }
                    else
                    {
                        break;
                    }
                }
                currentNode.children.push_back(temp);
                result.push_back(new ASTNode{currentNode.value, currentNode.children, currentNode.type});
                currentNode.children.clear();
            }
            else if (tokens[i]->type == TokenType::REDIRECT_IN || tokens[i]->type == TokenType::REDIRECT_OUT || tokens[i]->type == TokenType::REDIRECT_APPEND)
            {
                ASTNode *temp = new ASTNode{};
                currentNode.type = tokens[i]->type;
                currentNode.children.push_back(result.back());
                result.pop_back();
                temp->type = TokenType::FILE;
                if (i + 1 < tokens.size() && tokens[i + 1]->type == TokenType::WORD)
                {
                    temp->children.push_back(tokens[i + 1]);
                    i++;
                }
                currentNode.children.push_back(temp);
                result.push_back(new ASTNode{currentNode.value, currentNode.children, currentNode.type});
                currentNode.children.clear();
            }
            else
            {
                result.push_back(tokens[i]);
            }
        }
    }
    if (!currentNode.children.empty())
    {
        result.push_back(new ASTNode{currentNode.value, currentNode.children, currentNode.type});
    }
    return result;
}*/







// Helper function to safely extract the command payload arguments
void build_command_nodes(const vector<ASTNode *> &tokens, vector<ASTNode *> &output)
{
    ASTNode *currentCmd = nullptr;

    for (size_t i = 0; i < tokens.size(); i++)
    {
        // Handle nested groups
        if (tokens[i]->type == TokenType::LPAREN)
        {
            size_t closeIndex = find_matching_rparen(tokens, i);
            if (closeIndex == tokens.size()) return;

            vector<ASTNode *> nested(tokens.begin() + i + 1, tokens.begin() + closeIndex);
            vector<ASTNode *> nestedResult = parse_tokens(nested);
            
            if (!nestedResult.empty()) {
                output.push_back(new ASTNode{"group", nestedResult, TokenType::LRPAREN});
            }
            i = closeIndex;
            currentCmd = nullptr;
        }
        // Handle Redirections (binds tightly to the current running command)
        else if (tokens[i]->type == TokenType::REDIRECT_IN || 
                 tokens[i]->type == TokenType::REDIRECT_OUT || 
                 tokens[i]->type == TokenType::REDIRECT_APPEND)
        {
            if (i + 1 >= tokens.size()) return; // Syntax error safety

            ASTNode *fileNode = new ASTNode{tokens[i+1]->value, {}, TokenType::FILE};
            ASTNode *prevNode = nullptr;

            if (!output.empty() && currentCmd) {
                prevNode = output.back();
                output.pop_back();
            }

            ASTNode *redirNode = new ASTNode{tokens[i]->value, {prevNode, fileNode}, tokens[i]->type};
            output.push_back(redirNode);
            i++; // skip file token
            currentCmd = nullptr;
        }
        // Handle operators (pass them through cleanly as markers for the next phase)
        else if (tokens[i]->type == TokenType::PIPE || tokens[i]->type == TokenType::AND || 
                 tokens[i]->type == TokenType::OR || tokens[i]->type == TokenType::END)
        {
            output.push_back(tokens[i]);
            currentCmd = nullptr;
        }
        // Handle basic arguments / Command words
        else if (tokens[i]->type == TokenType::WORD || tokens[i]->type == TokenType::SINGLE_QUOTE || tokens[i]->type == TokenType::DOUBLE_QUOTE)
        {
            if (!currentCmd)
            {
                currentCmd = new ASTNode{tokens[i]->value, {}, TokenType::COMMAND};
                output.push_back(currentCmd);
            }
            currentCmd->children.push_back(tokens[i]);
        }
    }
}

// Main parser entry point
vector<ASTNode *> parse_tokens(const vector<ASTNode *> &tokens)
{
    if (tokens.empty()) return {};

    // Phase 1: Build the primary structures (Commands, Redirections, Groups)
    vector<ASTNode *> stage1;
    build_command_nodes(tokens, stage1);

    // Phase 2: Process High Precedence Operators (Pipes '|')
    vector<ASTNode *> stage2;
    for (size_t i = 0; i < stage1.size(); i++)
    {
        if (stage1[i]->type == TokenType::PIPE)
        {
            if (stage2.empty() || i + 1 >= stage1.size()) return {}; // Syntax error
            
            ASTNode *left = stage2.back();
            stage2.pop_back();
            ASTNode *right = stage1[i+1];
            i++; // Consume right-hand node

            stage2.push_back(new ASTNode{"|", {left, right}, TokenType::PIPE});
        }
        else
        {
            stage2.push_back(stage1[i]);
        }
    }

    // Phase 3: Process Lower Precedence Operators (&&, ||, ;)
    vector<ASTNode *> finalResult;
    for (size_t i = 0; i < stage2.size(); i++)
    {
        if (stage2[i]->type == TokenType::AND || stage2[i]->type == TokenType::OR || stage2[i]->type == TokenType::END)
        {
            if (finalResult.empty() || i + 1 >= stage2.size()) return {}; // Syntax error
            
            ASTNode *left = finalResult.back();
            finalResult.pop_back();
            ASTNode *right = stage2[i+1];
            i++; // Consume right-hand node

            finalResult.push_back(new ASTNode{stage2[i-1]->value, {left, right}, stage2[i-1]->type});
        }
        else
        {
            finalResult.push_back(stage2[i]);
        }
    }

    return finalResult;
}