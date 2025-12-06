#pragma once
#include "clang-tidy/ClangTidyCheck.h"

class NoMemberRawPointerCheck : public clang::tidy::ClangTidyCheck {
public:
    NoMemberRawPointerCheck(llvm::StringRef Name,
                            clang::tidy::ClangTidyContext* Context)
        : ClangTidyCheck(Name, Context) {}

    void registerMatchers(clang::ast_matchers::MatchFinder* Finder) override;
    void check(const clang::ast_matchers::MatchFinder& Result) override;
};