#include "MisalignedPointerCastCheck.h"

#include "clang/AST/ASTContext.h"
#include "clang/AST/Expr.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/Basic/TargetInfo.h"
#include "llvm/ADT/StringRef.h"

#include <cstdint>
#include <optional>

using namespace clang::ast_matchers;

namespace clang {
namespace tidy {
namespace bugprone {

namespace {

using Alignment = uint64_t;

static Alignment gcdAlignment(Alignment A, Alignment B) {
  while (B) {
    Alignment R = A % B;
    A = B;
    B = R;
  }
  return A;
}

static const Expr *ignoreCasts(const Expr *E) {
  return E ? E->IgnoreParenImpCasts() : nullptr;
}

static bool isAllocationFunction(const FunctionDecl *FD) {
  if (!FD || !FD->getIdentifier() || !FD->isExternC())
    return false;

  if (FD->getBuiltinID() == 0)
    return false;

  llvm::StringRef Name = FD->getName();

  return Name == "malloc" || Name == "calloc" || Name == "realloc";
}

static std::optional<int64_t> getConstantValue(const Expr *E,
                                               ASTContext &Context) {
  if (!E)
    return std::nullopt;

  Expr::EvalResult Result;
  if (!E->EvaluateAsInt(Result, Context))
    return std::nullopt;

  return Result.Val.getInt().getSExtValue();
}

static Alignment applyOffset(Alignment Base, int64_t Offset) {
  if (!Offset)
    return Base;

  uint64_t AbsOffset =
      Offset < 0 ? uint64_t(-(Offset + 1)) + 1 : uint64_t(Offset);

  return gcdAlignment(Base, AbsOffset);
}

static Alignment getGuaranteedAlignment(const Expr *E, ASTContext &Context,
                                        unsigned Depth = 0);

static Alignment analyzePointerArithmetic(const BinaryOperator *BO,
                                          ASTContext &Context, unsigned Depth) {
  const Expr *BaseExpr = BO->getLHS();
  const Expr *OffsetExpr = BO->getRHS();

  if (!BaseExpr->getType()->isPointerType()) {
    if (BO->getOpcode() != BO_Add || !OffsetExpr->getType()->isPointerType())
      return 0;

    std::swap(BaseExpr, OffsetExpr);
  }

  Alignment Base = getGuaranteedAlignment(BaseExpr, Context, Depth + 1);
  if (!Base)
    return 0;

  QualType ElementType = BaseExpr->getType()->getPointeeType();
  if (ElementType->isIncompleteType())
    return 0;

  uint64_t ElementSize = Context.getTypeSizeInChars(ElementType).getQuantity();
  if (!ElementSize)
    return 1;

  auto Offset = getConstantValue(OffsetExpr, Context);
  if (!Offset)
    return gcdAlignment(Base, ElementSize);

  int64_t ByteOffset = *Offset * static_cast<int64_t>(ElementSize);
  if (BO->getOpcode() == BO_Sub)
    ByteOffset = -ByteOffset;

  return applyOffset(Base, ByteOffset);
}

static Alignment analyzeIntegerExpression(const Expr *E, ASTContext &Context,
                                          unsigned Depth) {
  if (Depth > 16 || !(E = ignoreCasts(E)))
    return 0;

  if (const auto *CE = dyn_cast<CastExpr>(E)) {
    const Expr *Sub = CE->getSubExpr();

    if (Sub->getType()->isPointerType())
      return getGuaranteedAlignment(Sub, Context, Depth + 1);

    if (Sub->getType()->isIntegerType())
      return analyzeIntegerExpression(Sub, Context, Depth + 1);
  }

  if (const auto *BO = dyn_cast<BinaryOperator>(E)) {
    if (BO->getOpcode() != BO_Add && BO->getOpcode() != BO_Sub)
      return 0;

    Alignment Base = analyzeIntegerExpression(BO->getLHS(), Context, Depth + 1);
    if (!Base)
      return 0;

    auto Offset = getConstantValue(BO->getRHS(), Context);
    if (!Offset)
      return 1;

    int64_t Value = *Offset;
    if (BO->getOpcode() == BO_Sub)
      Value = -Value;

    return applyOffset(Base, Value);
  }

  if (const auto *DRE = dyn_cast<DeclRefExpr>(E)) {
    if (const auto *VD = dyn_cast<VarDecl>(DRE->getDecl())) {
      if (VD->hasInit())
        return analyzeIntegerExpression(VD->getInit(), Context, Depth + 1);
    }
  }

  return 0;
}

static Alignment getGuaranteedAlignment(const Expr *E, ASTContext &Context,
                                        unsigned Depth) {
  if (Depth > 16 || !(E = ignoreCasts(E)))
    return 0;

  if (const auto *DRE = dyn_cast<DeclRefExpr>(E)) {
    if (const auto *VD = dyn_cast<VarDecl>(DRE->getDecl())) {
      QualType T = VD->getType();

      if (T->isArrayType())
        return Context.getDeclAlign(VD).getQuantity();

      if (T->isPointerType()) {
        if (VD->hasInit())
          return getGuaranteedAlignment(VD->getInit(), Context, Depth + 1);

        QualType Pointee = T->getPointeeType();

        if (Pointee->isIncompleteType() || Pointee->isVoidType())
          return 0;

        return Context.getTypeAlignInChars(Pointee).getQuantity();
      }
    }
  }

  if (const auto *UO = dyn_cast<UnaryOperator>(E)) {
    if (UO->getOpcode() == UO_AddrOf) {
      const Expr *Operand = ignoreCasts(UO->getSubExpr());

      if (const auto *ASE = dyn_cast<ArraySubscriptExpr>(Operand)) {
        Alignment Base =
            getGuaranteedAlignment(ASE->getBase(), Context, Depth + 1);
        if (!Base)
          return 0;

        QualType ElementType = ASE->getBase()->getType()->getPointeeType();
        if (ElementType.isNull() || ElementType->isIncompleteType())
          return 1;

        uint64_t ElementSize =
            Context.getTypeSizeInChars(ElementType).getQuantity();
        auto Index = getConstantValue(ASE->getIdx(), Context);

        return Index ? applyOffset(Base,
                                   *Index * static_cast<int64_t>(ElementSize))
                     : gcdAlignment(Base, ElementSize);
      }

      if (const auto *DRE = dyn_cast<DeclRefExpr>(Operand))
        if (const auto *VD = dyn_cast<VarDecl>(DRE->getDecl()))
          return Context.getDeclAlign(VD).getQuantity();
    }
  }

  if (const auto *Call = dyn_cast<CallExpr>(E)) {
    if (isAllocationFunction(Call->getDirectCallee())) {
      const TargetInfo &Target = Context.getTargetInfo();
      return Target.getNewAlign() / Target.getCharWidth();
    }
  }

  if (E->getType()->isIntegerType())
    return analyzeIntegerExpression(E, Context, Depth + 1);

  if (const auto *BO = dyn_cast<BinaryOperator>(E))
    if (BO->getOpcode() == BO_Add || BO->getOpcode() == BO_Sub)
      return analyzePointerArithmetic(BO, Context, Depth);

  if (const auto *CE = dyn_cast<CastExpr>(E)) {
    const Expr *Sub = CE->getSubExpr();

    if (Sub->getType()->isPointerType())
      return getGuaranteedAlignment(Sub, Context, Depth + 1);

    if (Sub->getType()->isIntegerType())
      return analyzeIntegerExpression(Sub, Context, Depth + 1);
  }

  if (E->getType()->isPointerType()) {
    QualType Pointee = E->getType()->getPointeeType();

    return Pointee->isVoidType() || Pointee->isIncompleteType()
               ? 1
               : Context.getTypeAlignInChars(Pointee).getQuantity();
  }

  return 0;
}

} // namespace

void MisalignedPointerCastCheck::registerMatchers(MatchFinder *Finder) {
  Finder->addMatcher(castExpr(hasSourceExpression(expr().bind("source")),
                              hasType(pointerType()))
                         .bind("target"),
                     this);
}

void MisalignedPointerCastCheck::check(const MatchFinder::MatchResult &Result) {
  const auto *Source = Result.Nodes.getNodeAs<Expr>("source");
  const auto *Target = Result.Nodes.getNodeAs<CastExpr>("target");

  if (!Source || !Target)
    return;

  QualType TargetType = Target->getType();
  if (!TargetType->isPointerType())
    return;

  QualType Pointee = TargetType->getPointeeType();
  if (Pointee->isDependentType() || Pointee->isIncompleteType() ||
      Pointee->isFunctionType() || Pointee->isVoidType())
    return;

  Alignment Required =
      Result.Context->getTypeAlignInChars(Pointee).getQuantity();
  if (Required <= 1)
    return;

  Alignment Guaranteed = getGuaranteedAlignment(Source, *Result.Context);

  if (!Guaranteed || Guaranteed >= Required)
    return;

  QualType SourcePointedType = Source->getType()->isPointerType()
                                   ? Source->getType()->getPointeeType()
                                   : Source->getType();

  diag(Target->getBeginLoc(),
       "casting from type %0 (guaranteed alignment %1) to type %2 "
       "(required alignment %3 bytes) may result in misaligned access")
      << SourcePointedType << Guaranteed << Pointee << Required;
}

} // namespace bugprone
} // namespace tidy
} // namespace clang
