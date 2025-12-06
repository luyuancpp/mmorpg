#pragma once

#include "clang-tidy/ClangTidyCheck.h"

class NoMemberRawPointerCheck : public clang::tidy::ClangTidyCheck {
public:
	using ClangTidyCheck::ClangTidyCheck;

	void registerMatchers(clang::ast_matchers::MatchFinder* Finder) override;
	void check(const clang::ast_matchers::MatchFinder::MatchResult& Result) override;
};
