#include <unistd.h>
#include <vector>
#include <string>
#include <sys/wait.h>
#include <iostream>
#include <filesystem>
#include <fcntl.h>
#include "models.h"
#include <algorithm>
#include <signal.h>
using namespace std;

int execute_command(const ASTNode *node);
int execute_pipe(const ASTNode *node);
int execute_redirection(const ASTNode *node);
int execute_node(const ASTNode *node);

bool isAmpersand = false;

void executor(vector<ASTNode *> &ast)
{
    if (ast.back()->type == TokenType::AMPERSAND)
    {
        isAmpersand = true;
        ast.pop_back();
    }
    for (const auto &node : ast)
    {
        execute_node(node);
    }
}

int execute_redirection(const ASTNode *node)
{
    if (node->children.size() < 2)
        return 1;
    string filename = node->children[1]->value;
    int fd = -1;
    int target_fd = -1;

    if (node->type == TokenType::REDIRECT_OUT)
    {
        fd = open(filename.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
        target_fd = STDOUT_FILENO;
    }
    else if (node->type == TokenType::REDIRECT_APPEND)
    {
        fd = open(filename.c_str(), O_WRONLY | O_CREAT | O_APPEND, 0644);
        target_fd = STDOUT_FILENO;
    }
    else if (node->type == TokenType::REDIRECT_IN)
    {
        fd = open(filename.c_str(), O_RDONLY);
        target_fd = STDIN_FILENO;
    }

    if (fd < 0)
    {
        perror(("failed to open file: " + filename).c_str());
        return 1;
    }

    int saved_fd = dup(target_fd);
    dup2(fd, target_fd);
    close(fd);

    int result = execute_node(node->children[0]);

    dup2(saved_fd, target_fd);
    close(saved_fd);

    return result;
}

int execute_node(const ASTNode *node)
{
    if (!node)
        return 0;
    if (find(builtInCommands.begin(), builtInCommands.end(), node->value) != builtInCommands.end())
    {
        return execute_buildin_command(node);
    }
    else if (node->type == TokenType::COMMAND)
    {
        return execute_command(node);
    }
    else if (node->type == TokenType::PIPE)
    {
        return execute_pipe(node);
    }
    else if (node->type == TokenType::REDIRECT_IN || node->type == TokenType::REDIRECT_OUT || node->type == TokenType::REDIRECT_APPEND)
    {
        return execute_redirection(node);
    }
    else if (node->type == TokenType::AND)
    {
        int leftStatus = execute_node(node->children[0]);
        if (leftStatus == 0)
        {
            return execute_node(node->children[1]);
        }
        return leftStatus;
    }
    else if (node->type == TokenType::OR)
    {
        int leftStatus = execute_node(node->children[0]);
        if (leftStatus != 0)
        {
            return execute_node(node->children[1]);
        }
        return leftStatus;
    }
    else if (node->type == TokenType::END)
    {
        execute_node(node->children[0]);
        execute_node(node->children[1]);
        return 0;
    }
    else if (node->type == TokenType::LRPAREN)
    {
        return execute_node(node->children[0]);
    }
    else
    {
        return -1;
    }
    return 0;
}

int execute_pipe(const ASTNode *node)
{
    if (node->children.size() < 2)
        return 1;

    int pipefds[2];
    if (pipe(pipefds) == -1)
    {
        perror("pipe creation failed");
        return 1;
    }

    pid_t pid1 = fork();
    if (pid1 == 0)
    {
        dup2(pipefds[1], STDOUT_FILENO);
        close(pipefds[0]);
        close(pipefds[1]);
        exit(execute_node(node->children[0]));
    }

    pid_t pid2 = fork();
    if (pid2 == 0)
    {
        dup2(pipefds[0], STDIN_FILENO);
        close(pipefds[0]);
        close(pipefds[1]);
        exit(execute_node(node->children[1]));
    }

    close(pipefds[0]);
    close(pipefds[1]);

    int status1, status2;
    waitpid(pid1, &status1, 0);
    waitpid(pid2, &status2, 0);

    return WIFEXITED(status2) ? WEXITSTATUS(status2) : 1;
}

int execute_command(const ASTNode *node)
{
    if (node->children.empty())
        return 0;
    vector<char *> args;
    for (const auto &child : node->children)
    {
        if (child->type == TokenType::WORD)
        {
            args.push_back(const_cast<char *>(child->value.c_str()));
        }
    }
    args.push_back(nullptr);
    pid_t pid = fork();
    if (pid == 0)
    {
        signal(SIGINT, SIG_DFL);
        signal(SIGQUIT, SIG_DFL);
        if (execvp(args[0], args.data()) == -1)
        {
            cerr << args[0] << ": command not found" << endl;
            exit(127);
        }
    }
    else if (pid > 0)
    {
        if (isAmpersand)
        {
            cout << "[Process running in background with PID " << pid << "]" << endl;
            isAmpersand = false;
            return 0;
        }

        int status;
        waitpid(pid, &status, 0);
        if (WIFEXITED(status))
        {
            return WEXITSTATUS(status);
        }
        return 1;
    }
    else
    {
        // Fork failed
        cerr << "Failed to fork process" << endl;
        return -1;
    }
    return 0;
}

int execute_buildin_command(const ASTNode *node)
{
    signal(SIGINT, SIG_DFL);
    signal(SIGQUIT, SIG_DFL);
    if (node->value == "exit")
    {
        exit(0);
    }
    else if (node->value == "cd")
    {
        if (node->children.empty())
        {
            cerr << "cd: missing argument" << endl;
            return 1;
        }
        const char *path = node->children[0]->value.c_str();
        if (chdir(path) == -1)
        {
            perror(("cd: " + node->children[0]->value).c_str());
            return 1;
        }
        return 0;
    }
    else if (node->value == "declare")
    {
        string varName, varValue;
        bool foundEquals = false;
        for (size_t i = 1; i < node->children.size(); i++)
        {
            if (node->children[i]->value == "=")
            {
                foundEquals = true;
                varName = node->children[i - 1]->value;
                if (i + 1 < node->children.size())
                    varValue = node->children[i + 1]->value;
                break;
            }
        }
        if (!foundEquals || varName.empty())
        {
            cerr << "declare: invalid syntax" << endl;
            return 1;
        }
        setenv(varName.c_str(), varValue.c_str(), 1);
        return 0;
    }
    return 0;
}