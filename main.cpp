#include <iostream>
#include <vector>
#include <string>
#include <filesystem>
#include "tokenizer.h"

namespace fs = std::filesystem;
using namespace std;

void printAST(const ASTNode *node, int depth = 0)
{
    if (!node)
        return;
    cout << string(depth * 2, ' ') << "Value: " << node->value << ", Type: " << static_cast<int>(node->type) << endl;
    for (const auto &child : node->children)
    {
        printAST(child, depth + 1);
    }
}

int main()
{
    string input;
    while (true)
    {
        string currentPath = fs::current_path().string();
        cout << currentPath << "> ";
        if (!getline(cin, input))
            break;
        vector<ASTNode *> tokens = tokenize(input);
        vector<ASTNode *> parsedTokens = parse_tokens(tokens);
        if (parsedTokens.empty())
            continue;
        printAST(parsedTokens[0]);
    }
}