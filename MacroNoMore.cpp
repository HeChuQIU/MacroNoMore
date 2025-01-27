#include <sys/types.h>
#include <mutex>
#include "clang/AST/ASTConsumer.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendAction.h"
#include "clang/Tooling/Tooling.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include <random>
#include <unordered_map>
#include <unordered_set>
#include <fstream>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/ADT/SmallString.h>

using namespace clang;

std::string generateRandomAlias() {
    static const char letters[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
    static const char alphanum[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
    int len = 8;
    std::string alias;
    alias.reserve(len);
    alias += letters[rand() % (sizeof(letters) - 1)]; // Ensure the first character is a letter
    for (int i = 1; i < len; ++i) {
        alias += alphanum[rand() % (sizeof(alphanum) - 1)];
    }
    return alias;
}

class RenameVariableVisitor : public RecursiveASTVisitor<RenameVariableVisitor> {
public:
    explicit RenameVariableVisitor(ASTContext *Context, Rewriter &R)
      : Context(Context), TheRewriter(R) {}

    bool VisitVarDecl(VarDecl *Declaration) {
        if (Context->getSourceManager().isInMainFile(Declaration->getLocation())) {
            std::string name = Declaration->getNameAsString();
            if (aliases.find(name) == aliases.end()) {
                std::string alias = generateRandomAlias();
                aliases[name] = alias;
            }
            nameLocations[name].push_back(Declaration->getLocation());
        }
        return true;
    }

    bool VisitDeclRefExpr(DeclRefExpr *DRE) {
        std::string name = DRE->getNameInfo().getAsString();
        if (Context->getSourceManager().isInMainFile(DRE->getLocation()) || isPredefinedIdentifier(name)) {
            if (isOperator(name)) {
                name = name.substr(8); // Remove "operator" prefix
            }
            if (aliases.find(name) == aliases.end()) {
                std::string alias = generateRandomAlias();
                aliases[name] = alias;
            }
            nameLocations[name].push_back(DRE->getLocation());
        }
        return true;
    }

    bool VisitIntegerLiteral(IntegerLiteral *Literal) {
        if (Context->getSourceManager().isInMainFile(Literal->getLocation())) {
            SmallString<16> value;
            Literal->getValue().toString(value, 10, true, false);
            std::string valueStr = value.str().str();
            if (aliases.find(valueStr) == aliases.end()) {
                std::string alias = generateRandomAlias();
                aliases[valueStr] = alias;
            }
            nameLocations[valueStr].push_back(Literal->getLocation());
        }
        return true;
    }

    bool VisitStringLiteral(StringLiteral *Literal) {
        if (Context->getSourceManager().isInMainFile(Literal->getBeginLoc())) {
            std::string value = "\"" + Literal->getString().str() + "\"";
            if (aliases.find(value) == aliases.end()) {
                std::string alias = generateRandomAlias();
                aliases[value] = alias;
            }
            nameLocations[value].push_back(Literal->getBeginLoc());
        }
        return true;
    }

    bool VisitFunctionDecl(FunctionDecl *Declaration) {
        if (Context->getSourceManager().isInMainFile(Declaration->getLocation())) {
            std::string name = Declaration->getNameAsString();
            if (aliases.find(name) == aliases.end()) {
                std::string alias = generateRandomAlias();
                aliases[name] = alias;
            }
            nameLocations[name].push_back(Declaration->getLocation());
        }
        return true;
    }

    void InsertAliases(raw_ostream &outputFile) {
        for (const auto &entry : aliases) {
            outputFile << "#define " << entry.second << " " << entry.first << "\n";
        }
    }

    void ReplaceReferences() {
        for (const auto &entry : aliases) {
            for (const auto &loc : nameLocations[entry.first]) {
                TheRewriter.ReplaceText(loc, entry.first.length(), entry.second);
            }
        }
    }

private:
    ASTContext *Context;
    Rewriter &TheRewriter;
    std::unordered_map<std::string, std::string> aliases;
    std::unordered_map<std::string, std::vector<SourceLocation>> nameLocations;

    bool isPredefinedIdentifier(const std::string &name) {
        static const std::unordered_set<std::string> predefinedIdentifiers = {"cout", "cin", "cerr", "clog"};
        return predefinedIdentifiers.find(name) != predefinedIdentifiers.end();
    }

    bool isOperator(const std::string &name) {
        return name.find("operator") == 0;
    }
};

class RenameVariableConsumer : public ASTConsumer {
public:
    explicit RenameVariableConsumer(ASTContext* Context, Rewriter& R, raw_ostream& outputFile)
        : Visitor(Context, R), OutputFile(outputFile) {}

    virtual void HandleTranslationUnit(ASTContext& Context) {
        Visitor.TraverseDecl(Context.getTranslationUnitDecl());
        Visitor.ReplaceReferences();
        Visitor.InsertAliases(OutputFile);
    }

private:
    RenameVariableVisitor Visitor;
    raw_ostream& OutputFile;
};

class RenameVariableAction : public ASTFrontendAction {
public:
    RenameVariableAction(raw_ostream& outputFile)
        : OutputFile(outputFile) {}

    virtual std::unique_ptr<ASTConsumer> CreateASTConsumer(
        CompilerInstance& Compiler, StringRef InFile) {
        TheRewriter.setSourceMgr(Compiler.getSourceManager(), Compiler.getLangOpts());
        return std::make_unique<RenameVariableConsumer>(&Compiler.getASTContext(), TheRewriter, OutputFile);
    }

    void EndSourceFileAction() override {
        TheRewriter.getEditBuffer(TheRewriter.getSourceMgr().getMainFileID()).write(OutputFile);
    }

private:
    Rewriter TheRewriter;
    raw_ostream& OutputFile;
};

int main(int argc, char** argv) {
    if (argc != 3) {
        llvm::errs() << "Usage: " << argv[0] << " <source code path> <output file path>\n";
        return 1;
    }

    std::ifstream sourceFile(argv[1]);
    if (!sourceFile.is_open()) {
        llvm::errs() << "Error opening source file: " << argv[1] << "\n";
        return 1;
    }

    std::error_code EC;
    llvm::raw_fd_ostream outputFile(argv[2], EC, llvm::sys::fs::OF_None);
    if (EC) {
        llvm::errs() << "Error opening output file: " << EC.message() << "\n";
        return 1;
    }

    std::string code((std::istreambuf_iterator<char>(sourceFile)), std::istreambuf_iterator<char>());
    tooling::runToolOnCode(std::make_unique<RenameVariableAction>(outputFile), code);

    return 0;
}