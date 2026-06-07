#include <iostream>
#include <vector>
#include <string>
#include <filesystem>
#include "tokenizer.h"
#include "executor.h"
#include <signal.h>
namespace fs = std::filesystem;
using namespace std;

void freeAST(ASTNode *node)
{
    if (!node)
        return;
    for (auto &child : node->children)
    {
        freeAST(child);
    }
    delete node;
}
int main()
{
    signal(SIGINT, SIG_IGN);
    signal(SIGQUIT, SIG_IGN);
    string input;
    while (true)
    {
        string currentPath = fs::current_path().string();
        cout << currentPath << "> " << flush;
        if (!getline(cin, input))
            break;
        vector<ASTNode *> tokens = tokenize(input);
        vector<ASTNode *> parsedTokens = parse_tokens(tokens);
        if (parsedTokens.empty())
            continue;
        executor(parsedTokens);

        freeAST(parsedTokens[0]);
    }
}