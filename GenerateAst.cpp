#include <algorithm>       // std::find_if_not
#include <cctype>          // std::tolower, std::isspace
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

std::vector<std::string_view> split(std::string_view str,
                                    std::string_view delim) {
  std::vector<std::string_view> out;

  std::string_view::size_type start;
  std::string_view::size_type end = 0;

  while (true) {
    start = end;
    end = str.find(delim, start);

    out.push_back(str.substr(start, end - start));

    if (end >= str.length()) break;

    end += delim.length();
  }

  return out;
}

std::string_view trim(std::string_view str) {
  auto f = [] (auto c) { return std::isspace(c); };

  auto start = std::find_if_not(str.begin(), str.end(), f);
  auto end = std::find_if_not(str.rbegin(), str.rend(), f).base();

  return {start, std::string_view::size_type(end - start)};
}

std::string toLowerCase(std::string_view str) {
  std::string copy;

  for (char c : str) {
    copy.push_back(std::tolower(c));
  }

  return copy;
}

// In C++ we cannot rely on garbage collection to clean up our pointer
// messes for us. Instead of having to worry about calling delete
// every time we're done with some heap memory, we can dump this job
// onto std::shared_ptr. This function lets us use a '*' in our
// metaprogram to indicate that we actually want a smart pointer.
std::string fix_pointer(std::string_view field) {
  std::ostringstream out;
  std::string_view type = split(field, " ")[0];
  std::string_view name = split(field, " ")[1];
  bool close_bracket = false;

  if (type.substr(0, 12) == "std::vector<") {
    out << "std::vector<";
    type = type.substr(12, type.length() - 13);
    close_bracket = true;
  }

  if (type.back() == '*') {
    type.remove_suffix(1);
    out << "std::shared_ptr<" << type << ">";
  } else {
    out << type;
  }

  if (close_bracket) out << ">";
  out << " " << name;

  return out.str();
}

void defineVisitor(
    std::ofstream& writer, std::string_view baseName,
    const std::vector<std::string_view>& types) {
  writer << "struct " << baseName << "Visitor {\n";

  for (std::string_view type : types) {
      std::string_view typeName = trim(split(type, ":")[0]);
      writer << "  virtual std::any visit" << typeName << baseName <<
                "(std::shared_ptr<" << typeName << "> " <<
                toLowerCase(baseName) << ") = 0;\n";
  }

  writer << "  virtual ~" << baseName << "Visitor() = default;\n";
  writer << "};\n";
}

void defineType(
    std::ofstream& writer, std::string_view baseName,
    std::string_view className, std::string_view fieldList) {
  writer << "struct " << className << ": " << baseName <<
            ", std::enable_shared_from_this<" << className << "> {\n";

  // Constructor.
  writer << "  " << className << "(";

  std::vector<std::string_view> fields = split(fieldList, ", ");
  writer << fix_pointer(fields[0]);

  for (int i = 1; i < fields.size(); ++i) {
    writer << ", " << fix_pointer(fields[i]);
  }

  writer << ")\n"
         << "    : ";

  // Store parameters in fields.
  std::string_view name = split(fields[0], " ")[1];
  writer << name << "{std::move(" << name << ")}";

  for (int i = 1; i < fields.size(); ++i) {
    name = split(fields[i], " ")[1];
    writer << ", " << name << "{std::move(" << name << ")}";
  }

  writer << "\n"
         << "  {}\n";

  // Visitor pattern.
  writer << "\n"
            "  std::any accept(" << baseName <<
            "Visitor& visitor) override {\n"
            "    return visitor.visit" << className << baseName <<
            "(this->shared_from_this());\n"
            "  }\n";

  // Fields.
  writer << "\n";
  for (std::string_view field : fields) {
    writer << "  const " << fix_pointer(field) << ";\n";
  }

  writer << "};\n\n";
}

void defineAst(
    std::string_view outputDir, std::string_view baseName,
    const std::vector<std::string_view>& types) {
  std::string path = std::string{outputDir} + "/" +
                     std::string{baseName} + ".h";
  std::ofstream writer{path};

  writer << "#pragma once\n"
            "\n"
            "#include <any>\n"
            "#include <memory>\n"
            "#include \"Token.h\"\n";

  if (baseName == "Stmt") writer << "#include \"Expr.h\"\n";
  writer << "\n";

  // Forward declare the AST classes.
  for (std::string_view type : types) {
    std::string_view className = trim(split(type, ": ")[0]);
    writer << "struct " << className << ";\n";
  }

  // Visitor.
  writer << "\n";

  defineVisitor(writer, baseName, types);

  // The base class.
  // C++ does not allow virtual methods to be templated. That means
  // multiple accept signatures are out -- at least if we don't want
  // to over complicate things. An alternative is to use std::any,
  // which holds values of any type in a type-safe way. Classes
  // implementing Visitor are then required to cast the return value
  // to the expected type inside their member functions.
  writer << "\n"
            "struct " << baseName << " {\n"
            "  virtual std::any accept(" << baseName <<
            "Visitor& visitor) = 0;\n"
            "};\n\n";

  // The AST classes.
  for (std::string_view type : types) {
    std::string_view className = trim(split(type, ": ")[0]);
    std::string_view fields = trim(split(type, ": ")[1]);
    defineType(writer, baseName, className, fields);
  }
}

int main(int argc, char* argv[]) {
  if (argc != 2) {
    std::cout << "Usage: generate_ast <output directory>";
    std::exit(64);
  }

  std::string_view outputDir = argv[1];

  defineAst(outputDir, "Expr", {
    "Assign   : Token name, Expr* value",
    "Binary   : Expr* left, Token op, Expr* right",

    // Chapter 10 - Functions
    "Call     : Expr* callee, Token paren,"
              " std::vector<Expr*> arguments",

    "Grouping : Expr* expression",
    "Literal  : std::any value",

    // Chapter 9 - Control Flow
    "Logical  : Expr* left, Token op, Expr* right",

    "Unary    : Token op, Expr* right"

    // Chapter 8 - Statements and State
    ,
    "Variable : Token name"
  });

  // Chapter 8 - Statements and State
  defineAst(outputDir, "Stmt", {
    "Block      : std::vector<Stmt*> statements",
    "Expression : Expr* expression",

    // Chapter 10 - Functions
    "Function   : Token name, std::vector<Token> params,"
                " std::vector<Stmt*> body",

    // Chapter 9 - Control Flow
    "If         : Expr* condition, Stmt* thenBranch,"
                " Stmt* elseBranch",

    "Print      : Expr* expression",

    // Chapter 10 - Functions
    "Return     : Token keyword, Expr* value",

    "Var        : Token name, Expr* initializer"

    // Chapter 9 - Control Flow
    ,
    "While      : Expr* condition, Stmt* body"
  });
}

