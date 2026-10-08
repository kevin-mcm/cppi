#include "sema/OverloadResolver.hpp"

#include "sema/ExpressionBinder.hpp"
#include "sema/Intrinsics.hpp"
#include "support/DiagnosticFactory.hpp"

#include <algorithm>
#include <utility>

namespace cppi::sema {

using detail::DiagnosticFactory;

std::vector<TypeRef> OverloadResolver::param_types(const Callee& callee) const {
    if (callee.intrinsic) {
        return Intrinsics::info(static_cast<Intrinsic>(callee.id), ctx_.types).params;
    }
    std::vector<TypeRef> out;
    if (callee.host) {
        const auto& fn = ctx_.host.function(FunctionId{callee.id});
        for (const TypeId p : fn.params) {
            out.push_back(ctx_.types.from_host(p).value_or(TypeTable::kError));
        }
        return out;
    }
    for (const auto& p : ctx_.functions[callee.id].params) {
        out.push_back(p.type);
    }
    return out;
}

TypeRef OverloadResolver::return_type(const Callee& callee) const {
    if (callee.intrinsic) {
        return Intrinsics::info(static_cast<Intrinsic>(callee.id), ctx_.types).result;
    }
    if (callee.host) {
        return ctx_.types.from_host(ctx_.host.function(FunctionId{callee.id}).result).value_or(TypeTable::kError);
    }
    return ctx_.functions[callee.id].return_type;
}

std::string OverloadResolver::display_name(const Callee& callee) const {
    if (callee.intrinsic) {
        return std::string(Intrinsics::info(static_cast<Intrinsic>(callee.id), ctx_.types).name);
    }
    if (callee.host) {
        return ctx_.host.function(FunctionId{callee.id}).name;
    }
    return ctx_.functions[callee.id].display;
}

std::size_t OverloadResolver::defaults(const Callee& callee) const {
    if (callee.host || callee.intrinsic) {
        return 0;
    }
    std::size_t n = 0;
    const auto& params = ctx_.functions[callee.id].params;
    for (auto it = params.rbegin(); it != params.rend() && it->default_value != nullptr; ++it) {
        ++n;
    }
    return n;
}

std::optional<std::uint32_t> OverloadResolver::conversion_operator(TypeRef record, TypeRef target,
                                                                   bool allow_explicit) const {
    const TypeTable& types = ctx_.types;
    if (!types.is_record(record)) {
        return std::nullopt;
    }
    std::optional<std::uint32_t> found;
    const RecordInfo& info = types.record(record);
    for (const auto& [name, ids] : info.methods) {
        if (!name.starts_with("operator ")) {
            continue;
        }
        for (const std::uint32_t id : ids) {
            const FunctionInfo& fn = ctx_.functions[id];
            if (fn.is_explicit && !allow_explicit) {
                continue;
            }
            BExpr probe;
            probe.type = fn.return_type;
            probe.node = BLoad{};
            if (fn.return_type == target ||
                (!types.is_record(fn.return_type) && ctx_.conversions.rank(probe, target) != ConversionRank::None)) {
                if (!found || fn.return_type == target) {
                    found = id;
                }
            }
        }
    }
    return found;
}

bool OverloadResolver::converts_by_operator(TypeRef record, TypeRef target, bool allow_explicit) const {
    return conversion_operator(record, target, allow_explicit).has_value();
}

bool OverloadResolver::converts_by_constructor(const BExpr& arg, TypeRef record) const {
    const TypeTable& types = ctx_.types;
    if (!types.is_record(record) || arg.type == record || types.is_void(arg.type)) {
        return false;
    }
    return std::ranges::any_of(types.record(record).constructors, [&](std::uint32_t id) {
        const FunctionInfo& fn = ctx_.functions[id];
        if (fn.params.empty() || fn.access != ast::Access::Public) {
            return false;
        }
        const std::size_t optional = defaults(Callee{false, id});
        if (fn.params.size() - optional > 1) {
            return false;
        }
        const TypeRef p = fn.params.front().type;
        // Only standard conversions on the way into the constructor.
        const TypeRef target = types.is_reference(p) ? types.info(p).target : p;
        if (types.is_record(target) && target != arg.type) {
            return false;
        }
        return rank(arg, p) != ConversionRank::None;
    });
}

ConversionRank OverloadResolver::rank(const BExpr& arg, TypeRef param) const {
    const TypeTable& types = ctx_.types;
    if (types.is_error(arg.type) || types.is_error(param)) {
        return ConversionRank::Exact;
    }
    if (types.is_reference(param)) {
        const TypeInfo& ref = types.info(param);
        const bool same = arg.type == ref.target;
        const bool derived = types.is_record(arg.type) && types.is_record(ref.target) &&
                             ctx_.hierarchy.find_base(types.info(arg.type).decl, types.info(ref.target).decl);
        const bool rvalue_arg = arg.expiring || !arg.lvalue;
        if (types.is_rvalue_reference(param)) {
            // T&& binds expiring objects and plain values, never named lvalues.
            if (!rvalue_arg) {
                return ConversionRank::None;
            }
            if (arg.lvalue && (same || derived) && !arg.is_const) {
                return same ? ConversionRank::Exact : ConversionRank::Conversion;
            }
            if (types.is_record(ref.target)) {
                return converts_by_constructor(arg, ref.target) ? ConversionRank::UserDefined : ConversionRank::None;
            }
            return rank(arg, ref.target);
        }
        if (arg.lvalue && (same || derived) && (ref.target_const || !arg.is_const)) {
            if (arg.expiring) {
                // An expiring object may bind const T&, but T&& is a better match.
                return ref.target_const ? ConversionRank::Promotion : ConversionRank::None;
            }
            return same ? ConversionRank::Exact : ConversionRank::Conversion;
        }
        if (!ref.target_const) {
            return ConversionRank::None;  // a non-const reference needs a modifiable lvalue
        }
        if (types.is_record(ref.target)) {
            if (derived) {
                return ConversionRank::Conversion;
            }
            if (arg.type == ref.target) {
                return ConversionRank::Promotion;  // a temporary of the same class (T&& would be exact)
            }
            return converts_by_constructor(arg, ref.target) ? ConversionRank::UserDefined : ConversionRank::None;
        }
        return rank(arg, ref.target);
    }
    if (types.is_record(param)) {
        if (arg.type == param) {
            return ConversionRank::Exact;
        }
        if (types.is_record(arg.type)) {
            auto path = ctx_.hierarchy.find_base(types.info(arg.type).decl, types.info(param).decl);
            return path && !path->ambiguous ? ConversionRank::Conversion : ConversionRank::None;
        }
        return converts_by_constructor(arg, param) ? ConversionRank::UserDefined : ConversionRank::None;
    }
    if (types.is_array(arg.type) && arg.lvalue) {
        // Array-to-pointer decay is an exact match.
        BExpr decayed;
        decayed.type = ctx_.types.pointer_to(types.info(arg.type).target, arg.is_const);
        decayed.node = BConst{1};
        return ctx_.conversions.rank(decayed, param);
    }
    if (types.is_record(arg.type)) {
        return converts_by_operator(arg.type, param, false) ? ConversionRank::UserDefined : ConversionRank::None;
    }
    if (types.is_void(arg.type)) {
        return ConversionRank::None;
    }
    return ctx_.conversions.rank(arg, param);
}

std::string OverloadResolver::describe(const std::vector<BExprPtr>& args) const {
    std::string out;
    for (const auto& a : args) {
        if (!out.empty()) {
            out += ", ";
        }
        out += ctx_.types.name(a->type);
    }
    return out;
}

void OverloadResolver::report_single(const std::string& name, const Callee& callee, const std::vector<BExprPtr>& args,
                                     SourceRange call_range) {
    const auto params = param_types(callee);
    const std::size_t optional = defaults(callee);
    if (args.size() > params.size() || args.size() + optional < params.size()) {
        ctx_.report(DiagnosticFactory::argument_count_mismatch(call_range, name, params.size(), args.size()));
        return;
    }
    for (std::size_t i = 0; i < args.size(); ++i) {
        if (rank(*args[i], params[i]) != ConversionRank::None) {
            continue;
        }
        const TypeRef p = params[i];
        if (ctx_.types.is_reference(p) && !ctx_.types.info(p).target_const &&
            (args[i]->type == ctx_.types.info(p).target)) {
            ctx_.report(DiagnosticFactory::reference_needs_lvalue(args[i]->range, ctx_.types.name(p)));
            return;
        }
        const TypeRef expected = ctx_.types.is_reference(p) ? ctx_.types.info(p).target : p;
        ctx_.report(DiagnosticFactory::argument_type_mismatch(args[i]->range, name, i + 1, ctx_.types.name(expected),
                                                              ctx_.types.name(args[i]->type)));
        return;
    }
}

std::optional<Callee> OverloadResolver::resolve(const std::string& name, const std::vector<Callee>& candidates,
                                                const std::vector<BExprPtr>& args, SourceRange call_range) {
    auto chosen = choose(name, candidates, args, call_range);
    if (chosen && !chosen->host && !chosen->intrinsic && ctx_.functions[chosen->id].deleted) {
        ctx_.report(DiagnosticFactory::deleted_function(call_range, ctx_.functions[chosen->id].display));
        return std::nullopt;
    }
    return chosen;
}

std::optional<Callee> OverloadResolver::choose(const std::string& name, const std::vector<Callee>& candidates,
                                               const std::vector<BExprPtr>& args, SourceRange call_range) {
    if (candidates.size() == 1) {
        const auto params = param_types(candidates.front());
        const std::size_t optional = defaults(candidates.front());
        bool ok = args.size() <= params.size() && args.size() + optional >= params.size();
        for (std::size_t i = 0; ok && i < args.size(); ++i) {
            ok = rank(*args[i], params[i]) != ConversionRank::None;
        }
        if (!ok) {
            report_single(name, candidates.front(), args, call_range);
            return std::nullopt;
        }
        return candidates.front();
    }

    struct Viable {
        Callee callee;
        std::vector<ConversionRank> ranks;
    };
    std::vector<Viable> viable;
    for (const Callee& c : candidates) {
        const auto params = param_types(c);
        const std::size_t optional = defaults(c);
        if (args.size() > params.size() || args.size() + optional < params.size()) {
            continue;
        }
        Viable v{c, {}};
        bool ok = true;
        for (std::size_t i = 0; i < args.size() && ok; ++i) {
            const ConversionRank r = rank(*args[i], params[i]);
            ok = r != ConversionRank::None;
            v.ranks.push_back(r);
        }
        if (ok) {
            viable.push_back(std::move(v));
        }
    }
    if (viable.empty()) {
        ctx_.report(DiagnosticFactory::no_matching_function(call_range, name, describe(args)));
        return std::nullopt;
    }
    auto better = [](const Viable& a, const Viable& b) {
        bool strictly = false;
        for (std::size_t i = 0; i < a.ranks.size(); ++i) {
            if (a.ranks[i] > b.ranks[i]) {
                return false;
            }
            strictly = strictly || a.ranks[i] < b.ranks[i];
        }
        return strictly;
    };
    for (const Viable& v : viable) {
        const bool best = std::all_of(viable.begin(), viable.end(),
                                      [&](const Viable& other) { return &other == &v || better(v, other); });
        if (best) {
            return v.callee;
        }
    }
    ctx_.report(DiagnosticFactory::ambiguous_call(call_range, name, describe(args)));
    return std::nullopt;
}

std::optional<std::vector<BExprPtr>> OverloadResolver::prepare(const Callee& callee, std::vector<BExprPtr> args,
                                                               SourceRange call_range) {
    const auto params = param_types(callee);
    // Default arguments are bound at the call site.
    if (!callee.host && !callee.intrinsic) {
        const auto& info = ctx_.functions[callee.id];
        for (std::size_t i = args.size(); i < params.size(); ++i) {
            if (info.params[i].default_value == nullptr) {
                return std::nullopt;
            }
            auto value = ctx_.expressions->bind(*info.params[i].default_value);
            if (!value) {
                return std::nullopt;
            }
            value->range = call_range;
            args.push_back(std::move(value));
        }
    }
    std::vector<BExprPtr> out;
    out.reserve(args.size());
    for (std::size_t i = 0; i < args.size(); ++i) {
        auto prepared = ctx_.expressions->as_argument(std::move(args[i]), params[i]);
        if (!prepared) {
            return std::nullopt;
        }
        out.push_back(std::move(prepared));
    }
    return out;
}

}  // namespace cppi::sema
