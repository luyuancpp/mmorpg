#include "NoMemberRawPointerCheck.h"

#include "clang/AST/Decl.h"
#include "clang/Basic/SourceManager.h"
#include "llvm/Support/raw_ostream.h"

NoMemberRawPointerCheck::NoMemberRawPointerCheck(
    std::vector<std::string> skipPatterns)
    : SkipPatterns(std::move(skipPatterns)) {}

void NoMemberRawPointerCheck::run(
    const clang::ast_matchers::MatchFinder::MatchResult& Result) {
    const auto* FD = Result.Nodes.getNodeAs<clang::FieldDecl>("ptrField");
    if (!FD) return;
    if (FD->isImplicit()) return;

    auto& SM = *Result.SourceManager;
    auto Loc = FD->getLocation();

    if (SM.isInSystemHeader(Loc)) return;

    std::string File = SM.getFilename(Loc).str();
    for (const auto& Pat : SkipPatterns) {
        if (File.find(Pat) != std::string::npos) return;
    }

    unsigned Line = SM.getSpellingLineNumber(Loc);
    if (!Seen.insert({File, Line}).second) return; // already reported

    llvm::errs() << File << ":" << Line
                 << ": error: raw pointer member '" << FD->getName()
                 << "' (type '" << FD->getType().getAsString()
                 << "') is prohibited\n";
    ++Count;
}
