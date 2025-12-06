#include "NoMemberRawPointerCheck.h"
#include "clang/ASTMatchers/ASTMatchers.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"

using namespace clang::ast_matchers;

void NoMemberRawPointerCheck::registerMatchers(MatchFinder *Finder) {
    // 匹配类或结构体成员为裸指针
    Finder->addMatcher(
        fieldDecl(hasType(pointerType())).bind("ptrField"),
        this
    );
}

void NoMemberRawPointerCheck::check(const MatchFinder &Result) {
    const auto *Field = Result.Nodes.getNodeAs<clang::FieldDecl>("ptrField");
    if (!Field) return;

    diag(Field->getLocation(),
         "class/struct 成员变量不允许是裸指针类型 '%0'")
        << Field->getType();
}