#ifndef LLVM_CLANG_TOOLS_EXTRA_CLANG_TIDY_BUGPRONE_MISALIGNEDPOINTERCASTCHECK_H
#define LLVM_CLANG_TOOLS_EXTRA_CLANG_TIDY_BUGPRONE_MISALIGNEDPOINTERCASTCHECK_H

#include "../ClangTidyCheck.h"

namespace clang {
namespace tidy {
namespace bugprone {

class MisalignedPointerCastCheck : public ClangTidyCheck {
public:
  MisalignedPointerCastCheck(StringRef Name, ClangTidyContext *Context)
      : ClangTidyCheck(Name, Context) {}

  void registerMatchers(ast_matchers::MatchFinder *Finder) override;

  void check(const ast_matchers::MatchFinder::MatchResult &Result) override;
};

} // namespace bugprone
} // namespace tidy
} // namespace clang

#endif
