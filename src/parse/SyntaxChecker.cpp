#include "parse/SyntaxChecker.hpp"

#include "parse/TreeSitterUtils.hpp"
#include "support/DiagnosticFactory.hpp"

namespace cppi::parse {

using detail::DiagnosticFactory;

void SyntaxChecker::check(TSNode root) {
    bool nesting_reported = false;
    walk(root, [&](TSNode node, std::uint32_t depth) {
        if (diagnostics_.size() >= max_errors_) {
            return false;
        }
        if (depth > kMaxNesting) {
            if (!nesting_reported) {
                nesting_reported = true;
                diagnostics_.push_back(DiagnosticFactory::nesting_too_deep(range_of(node), kMaxNesting));
            }
            return false;
        }
        if (ts_node_is_missing(node)) {
            diagnostics_.push_back(DiagnosticFactory::missing_token(range_of(node), kind_of(node)));
            return false;
        }
        if (ts_node_is_error(node)) {
            diagnostics_.push_back(DiagnosticFactory::syntax_error(range_of(node)));
            return false;  // one report per ERROR region
        }
        return true;
    });
}

}  // namespace cppi::parse
