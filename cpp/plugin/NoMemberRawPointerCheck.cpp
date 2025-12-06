#include "NoMemberRawPointerCheck.h"

#include "clang/ASTMatchers/ASTMatchers.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"

using namespace clang;
using namespace clang::ast_matchers;

void NoMemberRawPointerCheck::registerMatchers(MatchFinder* Finder) {

	// 匹配任何成员字段（包含 static），如果其类型是裸指针，则禁止
	Finder->addMatcher(
		fieldDecl(
			hasType(pointerType())     // 任何裸指针类型
		).bind("ptrField"),
		this
	);
}

void NoMemberRawPointerCheck::check(const clang::ast_matchers::MatchFinder::MatchResult& Result) {
	const FieldDecl* Field = Result.Nodes.getNodeAs<FieldDecl>("ptrField");
	if (!Field) return;

	diag(Field->getLocation(),
		"禁止在类/结构体内声明任何裸指针成员变量 '%0'（包括 static 成员）")
		<< Field->getName();
}
