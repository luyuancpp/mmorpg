#include "NoMemberRawPointerCheck.h"

#include "clang/ASTMatchers/ASTMatchers.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"

using namespace clang;
using namespace clang::ast_matchers;

void NoMemberRawPointerCheck::registerMatchers(MatchFinder* Finder) {

	// Match any member field (including static) if its type is a raw pointer
	Finder->addMatcher(
		fieldDecl(
			hasType(pointerType())     // any raw pointer type
		).bind("ptrField"),
		this
	);
}

void NoMemberRawPointerCheck::check(const clang::ast_matchers::MatchFinder::MatchResult& Result) {
	const FieldDecl* Field = Result.Nodes.getNodeAs<FieldDecl>("ptrField");
	if (!Field) return;

	diag(Field->getLocation(),
		"Declaring raw pointer member variables in classes/structs is prohibited (including static members): '%0' of type '%1'")
		<< Field->getName() << Field->getType().getAsString();
}
