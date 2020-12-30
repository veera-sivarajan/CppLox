#pragma once

#include <any>
#include <iostream>
#include <memory>
#include <stdexcept>
#include "Error.h"
#include "Expr.h"

// Chapter 7 - Evaluating Expressions
#include "RuntimeError.h"

// Chapter 8 - Statements and State
#include <vector>
#include "Environment.h"
#include "Stmt.h"

// Chapter 10 - Functions
#include <chrono>
#include <string>
#include "LoxCallable.h"
#include "LoxFunction.h"
#include "Return.h"

// Chapter 11 - Resolving and Binding
#include <map>

// Chapter 12 - Classes
#include "LoxClass.h"
#include "LoxInstance.h"

class NativeClock: public LoxCallable {
public:
  int arity() override { return 0; }

  std::any call(Interpreter& interpreter,
                std::vector<std::any> arguments) override {
    auto ticks = std::chrono::system_clock::now().time_since_epoch();
    return std::chrono::duration<double>{ticks}.count() / 1000.0;
  }

  std::string toString() override { return "<native fn>"; }
};

// class Interpreter: public ExprVisitor {

// Chapter 8 - Statements and State
class Interpreter: public ExprVisitor,
                   public StmtVisitor {
  // Chapter 8 - Statements and State
  // std::shared_ptr<Environment> environment {new Environment};

  // Chapter 10 - Functions
public:
  std::shared_ptr<Environment> globals{new Environment};
private:
  std::shared_ptr<Environment> environment = globals;

  // Chapter 11 - Resolving and Binding
  std::map<std::shared_ptr<Expr>, int> locals;

public:
  // Chapter 10 - Functions
  Interpreter() {
    // See the comment in visitFunctionStmt for why this should be
    // stored through a base pointer.
    globals->define("clock",
        std::shared_ptr<LoxCallable>{new NativeClock});
  }

  // void interpret(std::shared_ptr<Expr> expression) const {
  //   try {
  //     std::any value = evaluate(expression);
  //     std::cout << stringify(value) << "\n";
  //   } catch (RuntimeError error) {
  //     runtimeError(error);
  //   }
  // }

  // Chapter 8 - Statements and State
  void interpret(
      const std::vector<std::shared_ptr<Stmt>>& statements) {
    try {
      for (const std::shared_ptr<Stmt>& statement : statements) {
        execute(statement);
      }
    } catch (RuntimeError error) {
      runtimeError(error);
    }
  }

private:
  std::any evaluate(std::shared_ptr<Expr> expr) {
    return expr->accept(*this);
  }

  // Chapter 8 - Statements and State
  void execute(std::shared_ptr<Stmt> stmt) {
    stmt->accept(*this);
  }

public:
  // Chapter 11 - Resolving and Binding
  void resolve(std::shared_ptr<Expr> expr, int depth) {
    locals[expr] = depth;
  }

  // Chapter 8 - Statements and State
  void executeBlock(
      const std::vector<std::shared_ptr<Stmt>>& statements,
      std::shared_ptr<Environment> environment) {
    auto previous = this->environment;
    try {
      this->environment = environment;

      for (const std::shared_ptr<Stmt>& statement : statements) {
        execute(statement);
      }
    } catch (...) {
      this->environment = previous;
      throw;
    }

    this->environment = previous;
  }

  std::any visitBlockStmt(std::shared_ptr<Block> stmt) override {
    executeBlock(stmt->statements,
                 std::make_unique<Environment>(environment));
    return {};
  }

  // Chapter 12 - Classes
  std::any visitClassStmt(std::shared_ptr<Class> stmt) override {
    environment->define(stmt->name.lexeme, nullptr);

    // auto klass = std::make_shared<LoxClass>(stmt->name.lexeme);

    std::map<std::string, std::shared_ptr<LoxFunction>> methods;
    for (std::shared_ptr<Function> method : stmt->methods) {
      auto function = std::make_shared<LoxFunction>(method,
          // environment);
          environment, method->name.lexeme == "init");
      methods[method->name.lexeme] = function;
    }

    auto klass = std::make_shared<LoxClass>(stmt->name.lexeme,
                                            methods);
    environment->assign(stmt->name, std::move(klass));
    return {};
  }

  std::any visitExpressionStmt(
      std::shared_ptr<Expression> stmt) override {
    evaluate(stmt->expression);
    return {};
  }

  // Chapter 10 - Functions
  std::any visitFunctionStmt(
      std::shared_ptr<Function> stmt) override {
    // auto function = std::make_shared<LoxFunction>(stmt);
    // auto function = std::make_shared<LoxFunction>(stmt, environment);

    // Chapter 12 - Classes
    auto function = std::make_shared<LoxFunction>(stmt, environment,
                                                  false);
    environment->define(stmt->name.lexeme, function);
    return {};
  }

  // Chapter 9 - Control Flow
  std::any visitIfStmt(std::shared_ptr<If> stmt) override {
    if (isTruthy(evaluate(stmt->condition))) {
      execute(stmt->thenBranch);
    } else if (stmt->elseBranch != nullptr) {
      execute(stmt->elseBranch);
    }
    return {};
  }

  std::any visitPrintStmt(std::shared_ptr<Print> stmt) override {
    std::any value = evaluate(stmt->expression);
    std::cout << stringify(value) << "\n";
    return {};
  }

  // Chapter 10 - Functions
  std::any visitReturnStmt(std::shared_ptr<Return> stmt) override {
    std::any value = nullptr;
    if (stmt->value != nullptr) value = evaluate(stmt->value);

    throw LoxReturn{value};
  }

  // Chapter 8 - Statements and State
  std::any visitVarStmt(std::shared_ptr<Var> stmt) override {
    std::any value = nullptr;
    if (stmt->initializer != nullptr) {
      value = evaluate(stmt->initializer);
    }

    environment->define(stmt->name.lexeme, std::move(value));
    return {};
  }

  // Chapter 9 - Control Flow
  std::any visitWhileStmt(std::shared_ptr<While> stmt) override {
    while (isTruthy(evaluate(stmt->condition))) {
      execute(stmt->body);
    }
    return {};
  }

  std::any visitAssignExpr(std::shared_ptr<Assign> expr) override {
    std::any value = evaluate(expr->value);
    // environment->assign(expr->name, value);

    // Chapter 11 - Resolving and Binding
    auto elem = locals.find(expr);
    if (elem != locals.end()) {
      int distance = elem->second;
      environment->assignAt(distance, expr->name, value);
    } else {
      globals->assign(expr->name, value);
    }

    return value;
  }

  std::any visitBinaryExpr(std::shared_ptr<Binary> expr) override {
    std::any left = evaluate(expr->left);
    std::any right = evaluate(expr->right);

    switch (expr->op.type) {
      case GREATER:
        checkNumberOperands(expr->op, left, right);
        return std::any_cast<double>(left) >
               std::any_cast<double>(right);
      case GREATER_EQUAL:
        checkNumberOperands(expr->op, left, right);
        return std::any_cast<double>(left) >=
               std::any_cast<double>(right);
      case LESS:
        checkNumberOperands(expr->op, left, right);
        return std::any_cast<double>(left) <
               std::any_cast<double>(right);
      case LESS_EQUAL:
        checkNumberOperands(expr->op, left, right);
        return std::any_cast<double>(left) <=
               std::any_cast<double>(right);
      case MINUS:
        checkNumberOperands(expr->op, left, right);
        return std::any_cast<double>(left) -
               std::any_cast<double>(right);
      case PLUS:
        if (left.type() == typeid(double) &&
            right.type() == typeid(double)) {
          return std::any_cast<double>(left) +
                 std::any_cast<double>(right);
        }

        if (left.type() == typeid(std::string) &&
            right.type() == typeid(std::string)) {
          return std::any_cast<std::string>(left) +
                 std::any_cast<std::string>(right);
        }

        // break;

        throw RuntimeError{expr->op,
          "Operands must be two numbers or two strings."};
      case SLASH:
        checkNumberOperands(expr->op, left, right);
        return std::any_cast<double>(left) /
               std::any_cast<double>(right);
      case STAR:
        checkNumberOperands(expr->op, left, right);
        return std::any_cast<double>(left) *
               std::any_cast<double>(right);
      case BANG_EQUAL: return !isEqual(left, right);
      case EQUAL_EQUAL: return isEqual(left, right);
    }

    // Unreachable.
    return nullptr;
  }

  // Chapter 10 - Functions
  std::any visitCallExpr(std::shared_ptr<Call> expr) override {
    std::any callee = evaluate(expr->callee);

    std::vector<std::any> arguments;
    for (const std::shared_ptr<Expr>& argument : expr->arguments) {
      arguments.push_back(evaluate(argument));
    }

    // Values in a std::any wrapper lose the ability to be cast; the
    // original object must first be extracted before being converted.
    std::shared_ptr<LoxCallable> function;

    if (callee.type() == typeid(std::shared_ptr<LoxFunction>)) {
      function = std::any_cast<std::shared_ptr<LoxFunction>>(callee);
    } else if (callee.type() == typeid(std::shared_ptr<LoxClass>)) {
      function = std::any_cast<std::shared_ptr<LoxClass>>(callee);
    } else if (callee.type() ==
        typeid(std::shared_ptr<NativeClock>)) {
      function = std::any_cast<std::shared_ptr<NativeClock>>(callee);
    } else {
      throw RuntimeError{expr->paren,
          "Can only call functions and classes."};
    }

    if (arguments.size() != function->arity()) {
      throw RuntimeError{expr->paren, "Expected " +
          std::to_string(function->arity()) + " arguments but got " +
          std::to_string(arguments.size()) + "."};
    }

    return function->call(*this, std::move(arguments));
  }

  std::any visitGetExpr(std::shared_ptr<Get> expr) override {
    std::any object = evaluate(expr->object);
    if (object.type() == typeid(std::shared_ptr<LoxInstance>)) {
      return std::any_cast<
          std::shared_ptr<LoxInstance>>(object)->get(expr->name);
    }

    throw RuntimeError(expr->name,
        "Only instances have properties.");
  }

  std::any visitGroupingExpr(
      std::shared_ptr<Grouping> expr) override {
    return evaluate(expr->expression);
  }

  std::any visitLiteralExpr(std::shared_ptr<Literal> expr) override {
    return expr->value;
  }

  // Chapter 9 - Control Flow
  std::any visitLogicalExpr(std::shared_ptr<Logical> expr) override {
    std::any left = evaluate(expr->left);

    if (expr->op.type == OR) {
      if (isTruthy(left)) return left;
    } else {
      if (!isTruthy(left)) return left;
    }

    return evaluate(expr->right);
  }

  // Chapter 12 - Classes
  std::any visitSetExpr(std::shared_ptr<Set> expr) override {
    std::any object = evaluate(expr->object);

    if (object.type() != typeid(std::shared_ptr<LoxInstance>)) {
      throw RuntimeError(expr->name,
                         "Only instances have fields.");
    }

    std::any value = evaluate(expr->value);
    std::any_cast<
        std::shared_ptr<LoxInstance>>(object)->set(expr->name, value);
    return value;
  }

  std::any visitThisExpr(std::shared_ptr<This> expr) override {
    return lookUpVariable(expr->keyword, expr);
  }

  std::any visitUnaryExpr(std::shared_ptr<Unary> expr) override {
    std::any right = evaluate(expr->right);

    switch (expr->op.type) {
      case BANG:
        return !isTruthy(right);
      case MINUS:
        checkNumberOperand(expr->op, right);
        return -std::any_cast<double>(right);
    }

    // Unreachable.
    return nullptr;
  }

  // Chapter 8 - Statements and State
  std::any visitVariableExpr(
      std::shared_ptr<Variable> expr) override {
    // return environment->get(expr->name);

    // Chapter 11 - Resolving and Binding
    return lookUpVariable(expr->name, expr);
  }

private:
  // Chapter 11 - Resolving and Binding
  std::any lookUpVariable(const Token& name,
                          std::shared_ptr<Expr> expr) {
    auto elem = locals.find(expr);
    if (elem != locals.end()) {
      int distance = elem->second;
      return environment->getAt(distance, name.lexeme);
    } else {
      return globals->get(name);
    }
  }

  void checkNumberOperand(const Token& op,
                          const std::any& operand) const {
    if (operand.type() == typeid(double)) return;
    throw RuntimeError{op, "Operand must be a number."};
  }

  void checkNumberOperands(const Token& op,
                           const std::any& left,
                           const std::any& right) const {
    if (left.type() == typeid(double) &&
        right.type() == typeid(double)) {
      return;
    }

    throw RuntimeError{op, "Operands must be numbers."};
  }

  bool isTruthy(const std::any& object) {
    if (object.type() == typeid(nullptr)) return false;
    if (object.type() == typeid(bool)) {
      return std::any_cast<bool>(object);
    }
    return true;
  }

  bool isEqual(const std::any& a, const std::any& b) {
    if (a.type() == typeid(nullptr) && b.type() == typeid(nullptr)) {
      return true;
    }
    if (a.type() == typeid(nullptr)) return false;

    if (a.type() == typeid(std::string) &&
        b.type() == typeid(std::string)) {
      return std::any_cast<std::string>(a) ==
             std::any_cast<std::string>(b);
    }
    if (a.type() == typeid(double) && b.type() == typeid(double)) {
      return std::any_cast<double>(a) == std::any_cast<double>(b);
    }
    if (a.type() == typeid(bool) && b.type() == typeid(bool)) {
      return std::any_cast<bool>(a) == std::any_cast<bool>(b);
    }

    return false;
  }

  std::string stringify(const std::any& object) {
    if (object.type() == typeid(nullptr)) return "nil";

    if (object.type() == typeid(double)) {
      std::string text = std::to_string(
          std::any_cast<double>(object));
      if (text[text.length() - 2] == '.' &&
          text[text.length() - 1] == '0') {
        text = text.substr(0, text.length() - 2);
      }
      return text;
    }

    if (object.type() == typeid(std::string)) {
      return std::any_cast<std::string>(object);
    }

    if (object.type() == typeid(bool)) {
      return std::any_cast<bool>(object) ? "true" : "false";
    }

    if (object.type() == typeid(std::shared_ptr<LoxFunction>)) {
      return std::any_cast<
          std::shared_ptr<LoxFunction>>(object)->toString();
    }

    if (object.type() == typeid(std::shared_ptr<NativeClock>)) {
      return std::any_cast<
          std::shared_ptr<NativeClock>>(object)->toString();
    }

    if (object.type() == typeid(std::shared_ptr<LoxClass>)) {
      return std::any_cast<
          std::shared_ptr<LoxClass>>(object)->toString();
    }

    if (object.type() == typeid(std::shared_ptr<LoxInstance>)) {
      return std::any_cast<
          std::shared_ptr<LoxInstance>>(object)->toString();
    }

    return "Error in stringify: object type not recognized.";
  }
};
