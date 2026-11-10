#include "MisalignedPointerCastCheck.h"

#include "clang/AST/ASTContext.h"
#include "clang/AST/ASTTypeTraits.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"

using namespace clang::ast_matchers;

namespace clang {
namespace tidy {
namespace bugprone {

void MisalignedPointerCastCheck::registerMatchers(MatchFinder *Finder) {
  Finder->addMatcher(
      castExpr(hasSourceExpression(expr().bind("source"))).bind("target"),
      this);
}

void MisalignedPointerCastCheck::check(const MatchFinder::MatchResult &Result) {
  const auto *Source = Result.Nodes.getNodeAs<Expr>("source");

  const auto *Target = Result.Nodes.getNodeAs<CastExpr>("target");

  if (!Source || !Target)
    return;

  QualType SourceType = Source->getType();
  QualType TargetType = Target->getType();

  if (!SourceType->isPointerType() || !TargetType->isPointerType())
    return;

  if (SourceType->isDependentType() || TargetType->isDependentType())
    return;

  QualType SourcePointedType = SourceType->getPointeeType();

  QualType TargetPointedType = TargetType->getPointeeType();

  if (SourcePointedType->isIncompleteType() ||
      TargetPointedType->isIncompleteType())
    return;

  CharUnits SourceAlign =
      Result.Context->getTypeAlignInChars(SourcePointedType);

  CharUnits TargetAlign =
      Result.Context->getTypeAlignInChars(TargetPointedType);

  if (SourceAlign >= TargetAlign)
    return;

  if (Target->getCastKind() != CK_BitCast)
    return;

  diag(Target->getBeginLoc(), "casting from type %0 (alignment %1) to type %2 "
                              "(alignment %3) increases required alignment")
      << SourcePointedType << SourceAlign.getQuantity() << TargetPointedType
      << TargetAlign.getQuantity();
}

} // namespace bugprone
} // namespace tidy
} // namespace clang
