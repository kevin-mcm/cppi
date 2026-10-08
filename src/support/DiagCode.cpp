#include <cppi/DiagCode.hpp>

#include <string>

namespace cppi {

std::string_view diag_key(DiagCode code) noexcept {
    switch (code) {
        case DiagCode::SyntaxError: return "syntax-error";
        case DiagCode::MissingToken: return "missing-token";
        case DiagCode::NestingTooDeep: return "nesting-too-deep";
        case DiagCode::UnknownFunction: return "unknown-function";
        case DiagCode::UnknownIdentifier: return "unknown-identifier";
        case DiagCode::ArgumentCountMismatch: return "argument-count-mismatch";
        case DiagCode::ArgumentTypeMismatch: return "argument-type-mismatch";
        case DiagCode::NotCallable: return "not-callable";
        case DiagCode::IntegerOutOfRange: return "integer-out-of-range";
        case DiagCode::FunctionNotCalled: return "function-not-called";
        case DiagCode::ExpressionHasNoEffect: return "expression-has-no-effect";
        case DiagCode::Redefinition: return "redefinition";
        case DiagCode::NotAssignable: return "not-assignable";
        case DiagCode::InvalidOperands: return "invalid-operands";
        case DiagCode::CannotConvert: return "cannot-convert";
        case DiagCode::MisplacedJump: return "misplaced-jump";
        case DiagCode::ReturnTypeMismatch: return "return-type-mismatch";
        case DiagCode::UnknownType: return "unknown-type";
        case DiagCode::InvalidArraySize: return "invalid-array-size";
        case DiagCode::NotConstant: return "not-constant";
        case DiagCode::DuplicateCase: return "duplicate-case";
        case DiagCode::NoMember: return "no-member";
        case DiagCode::NotSubscriptable: return "not-subscriptable";
        case DiagCode::TooManyInitializers: return "too-many-initializers";
        case DiagCode::UninitializedConst: return "uninitialized-const";
        case DiagCode::MissingReturn: return "missing-return";
        case DiagCode::InaccessibleMember: return "inaccessible-member";
        case DiagCode::AmbiguousName: return "ambiguous-name";
        case DiagCode::AbstractClass: return "abstract-class";
        case DiagCode::NoMatchingFunction: return "no-matching-function";
        case DiagCode::StaticAssertionFailed: return "static-assertion-failed";
        case DiagCode::AssignmentInCondition: return "assignment-in-condition";
        case DiagCode::ReferenceNeedsLvalue: return "reference-needs-lvalue";
        case DiagCode::UnsupportedType: return "unsupported-type";
        case DiagCode::DeclarationNotAllowed: return "declaration-not-allowed";
        case DiagCode::NoDefaultConstructor: return "no-default-constructor";
        case DiagCode::NothingToOverride: return "nothing-to-override";
        case DiagCode::TemplateDeduction: return "template-deduction";
        case DiagCode::AmbiguousCall: return "ambiguous-call";
        case DiagCode::UndefinedFunction: return "undefined-function";
        case DiagCode::DeletedFunction: return "deleted-function";
        case DiagCode::ConstraintsNotSatisfied: return "constraints-not-satisfied";
        case DiagCode::NoOperator: return "no-operator";
        case DiagCode::InstantiatedFrom: return "instantiated-from";
        case DiagCode::FeatureLocked: return "feature-locked";
        case DiagCode::FeatureRequiresStandard: return "feature-requires-standard";
        case DiagCode::FeatureNotImplemented: return "feature-not-implemented";
        case DiagCode::UnsupportedSyntax: return "unsupported-syntax";
        case DiagCode::BudgetExhausted: return "budget-exhausted";
        case DiagCode::HostError: return "host-error";
        case DiagCode::StackOverflow: return "stack-overflow";
        case DiagCode::OutOfMemory: return "out-of-memory";
        case DiagCode::UncaughtException: return "uncaught-exception";
        case DiagCode::ExceptionDuringUnwind: return "exception-during-unwind";
        case DiagCode::NoActiveException: return "no-active-exception";
        case DiagCode::DivisionByZero: return "division-by-zero";
        case DiagCode::IntegerOverflow: return "integer-overflow";
        case DiagCode::UninitializedRead: return "uninitialized-read";
        case DiagCode::OutOfBounds: return "out-of-bounds";
        case DiagCode::NullDereference: return "null-dereference";
        case DiagCode::UseAfterFree: return "use-after-free";
        case DiagCode::DoubleFree: return "double-free";
        case DiagCode::InvalidDelete: return "invalid-delete";
        case DiagCode::FlowOffEnd: return "flow-off-end";
        case DiagCode::InvalidShift: return "invalid-shift";
        case DiagCode::MemoryLeak: return "memory-leak";
        case DiagCode::PureVirtualCall: return "pure-virtual-call";
        case DiagCode::InvalidPointer: return "invalid-pointer";
        case DiagCode::BadAccess: return "bad-access";
    }
    return "unknown";
}

std::string diag_id(DiagCode code) {
    std::string digits = std::to_string(static_cast<unsigned>(code));
    if (digits.size() < 4) {
        digits.insert(0, 4 - digits.size(), '0');
    }
    return "E" + digits;
}

}  // namespace cppi
