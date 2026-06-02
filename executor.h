#include <unistd.h>
#include <vector>
#include <string>
#include <sys/wait.h>
#include <iostream>
#include <filesystem>
#include <fcntl.h>
#include "models.h"
using namespace std;

int execute_command(const ASTNode *node);
int execute_pipe(const ASTNode *node);
int execute_redirection(const ASTNode *node);
int execute_node(const ASTNode *node);

bool isAmpersand = false;

void executor(vector<ASTNode *> &ast)
{
    // Placeholder for the executor function that will execute the commands represented by the AST
    // This function will need to handle various command types, pipelines, redirections, etc.
    // The implementation will depend on how the AST is structured and how you want to execute commands
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

    // The right child is always the FILE node according to your parser
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

    // Duplicate standard FD to our file descriptor
    int saved_fd = dup(target_fd);
    dup2(fd, target_fd);
    close(fd);

    // Execute the left child command with streams redirected
    int result = execute_node(node->children[0]);

    // Restore the original standard FD stream
    dup2(saved_fd, target_fd);
    close(saved_fd);

    return result;
}

int execute_node(const ASTNode *node)
{
    if (!node)
        return 0;
    if (node->type == TokenType::COMMAND)
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
        return -1; // For other node types (like WORD, FILE, etc.) that don't directly execute commands
    }
    // Handle other node types (redirections, logical operators, etc.) as needed
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
        // Left Child: Writes to pipe
        dup2(pipefds[1], STDOUT_FILENO);
        close(pipefds[0]);
        close(pipefds[1]);
        exit(execute_node(node->children[0]));
    }

    pid_t pid2 = fork();
    if (pid2 == 0)
    {
        // Right Child: Reads from pipe
        dup2(pipefds[0], STDIN_FILENO);
        close(pipefds[0]);
        close(pipefds[1]);
        exit(execute_node(node->children[1]));
    }

    // Parent closes its copies of the pipe ends
    close(pipefds[0]);
    close(pipefds[1]);

    int status1, status2;
    waitpid(pid1, &status1, 0);
    waitpid(pid2, &status2, 0);

    // Return the status of the rightmost command in the pipeline
    return WIFEXITED(status2) ? WEXITSTATUS(status2) : 1;
}

int execute_command(const ASTNode *node)
{
    // Placeholder for a function that executes a single command represented by an ASTNode
    // This function will need to handle the command and its arguments, as well as any redirections or pipelines
    // The implementation will depend on how the ASTNode is structured and how you want to execute commands
    if (node->children.empty())
        return 0;
    vector<char *> args; // Convert ASTNode children to char* array for execvp
    for (const auto &child : node->children)
    {
        if (child->type == TokenType::WORD)
        {
            args.push_back(const_cast<char *>(child->value.c_str()));
        }
    }
    args.push_back(nullptr); // Null-terminate the arguments array
    pid_t pid = fork();
    if (pid == 0)
    {
        // In child process: execute the command
        // Use execvp or similar to execute the command with its arguments
        if (execvp(args[0], args.data()) == -1)
        {
            cerr << args[0] << ": command not found" << endl;
            exit(127); // Exit after execution
        }
        else if (pid > 0)
        {
            if (isAmpersand)
            {
                // Background process requested with '&'
                cout << "[Process running in background with PID " << pid << "]" << endl;
                return 0;
            }
            else
            {
                int status;
                waitpid(pid, &status, 0);
                if (WIFEXITED(status))
                {
                    return WEXITSTATUS(status);
                }
                return 1;
            }
        }
        else
        {
            // Fork failed
            cerr << "Failed to fork process" << endl;
            return -1;
        }
    }
    return 0;
}

/*
if (node->type == TokenType::COMMAND)
        {
            // Handle command execution
            int status = execute_command(node);
        }
        else if (node->type == TokenType::PIPE)
        {
            // Handle pipeline execution
        }
        else if (node->type == TokenType::REDIRECT_IN || node->type == TokenType::REDIRECT_OUT || node->type == TokenType::REDIRECT_APPEND)
        {
            // Handle redirection
        }
        else if (node->type == TokenType::AND)
        {
            // Handle logical operators
            int leftStatus = executor(node->children[0]);
            if (leftStatus == 0)
            {
                int rightStatus = execute_command(node->children[1]);
            }
        }
        else if (node->type == TokenType::OR)
        {
            // Handle logical operators
            int leftStatus = execute_command(node->children[0]);
            if (leftStatus != 0)
            {
                int rightStatus = execute_command(node->children[1]);
            }
        }
        else if (node->type == TokenType::END)
        {
            // Handle command sequence termination
            execute_command(node->children[0]);
            execute_command(node->children[1]);
        }
*/