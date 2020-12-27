#include <fstream>      // readFile
#include <iostream>     // std::getline
#include <memory>
#include <string>
#include <vector>
#include "Error.h"
// #include "AstPrinter.h"
#include "Scanner.h"

// Chapter 7 - Evaluating Expressions
#include "Interpreter.h"

// Chapter 6 - Parsing Expressions
#include "Parser.h"

// Chapter 11 - Resolving and Binding
#include "Resolver.h"

// Chapter 7 - Evaluating Expressions
Interpreter interpreter{};

// void run(std::string_view source) {
//   Scanner scanner {source};
//   std::vector<Token> tokens = scanner.scanTokens();

//   // For now, just print the tokens.
//   for (const Token& token : tokens) {
//     std::cout << token.toString() << "\n";
//   }

//   // Stop if there was a syntax error.
//   if (hadError) return;
//   }
// }

// Chapter 6 - Parsing Expressions
// void run(std::string_view source) {
//   Scanner scanner {source};
//   std::vector<Token> tokens = scanner.scanTokens();

//   Parser parser {tokens};
//   std::shared_ptr<Expr> expression = parser.parse();

//   // Stop if there was a syntax error.
//   if (hadError) return;

//   // std::cout << AstPrinter{}.print(expression) << "\n";

//   // Chapter 7 - Evaluating Expressions
//   interpreter.interpret(expression);
// }

// Chapter 8 - Statements and State
void run(std::string_view source) {
  Scanner scanner {source};
  std::vector<Token> tokens = scanner.scanTokens();

  Parser parser {tokens};
  std::vector<std::shared_ptr<Stmt>> statements = parser.parse();

  // Stop if there was a syntax error.
  if (hadError) return;

  // Chapter 11 - Resolving and Binding
  Resolver resolver{interpreter};
  resolver.resolve(statements);

  // Stop if there was a resolution error.
  if (hadError) return;

  interpreter.interpret(statements);
}

// source: http://insanecoding.blogspot.com/2011/11/how-to-read-in-file-in-c.html
std::string readFile(const char* path) {
  // Open file
  std::ifstream file (path, std::ios::in | std::ios::binary |
                            std::ios::ate);
  if (!file) throw (errno);

  // Create string of sufficient size
  std::string contents;
  contents.resize(file.tellg());

  // Read complete file into string
  file.seekg(0, std::ios::beg);
  file.read(&contents[0], contents.size());

  return contents;
}

void runFile(const char* path) {
  std::string contents = readFile(path);
  run(contents);

  // Indicate an error in the exit code.
  if (hadError) std::exit(65);

  // Chapter 7 - Evaluating Expressions
  if (hadRuntimeError) std::exit(70);
}

void runPrompt() {
  for (;;) {
    std::cout << "> ";
    std::string line;
    if (!std::getline(std::cin, line)) break;

    run(line);
    hadError = false;
  }
}

int main(int argc, char* argv[]) {
  if (argc > 2) {
    std::cout << "Usage: jlox [script]";
    std::exit(64);
  } else if (argc == 2) {
    runFile(argv[1]);
  } else {
    runPrompt();
  }
}