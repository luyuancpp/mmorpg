#pragma once

#include "clang/ASTMatchers/ASTMatchFinder.h"

#include <set>
#include <string>
#include <utility>
#include <vector>

/// MatchFinder callback that reports raw-pointer FieldDecl members.
class NoMemberRawPointerCheck
    : public clang::ast_matchers::MatchFinder::MatchCallback {
public:
    explicit NoMemberRawPointerCheck(std::vector<std::string> skipPatterns);

    void run(const clang::ast_matchers::MatchFinder::MatchResult& Result) override;

    int violationCount() const { return Count; }

private:
    std::vector<std::string> SkipPatterns;
    std::set<std::pair<std::string, unsigned>> Seen; // dedup across TUs
    int Count = 0;
};
