#include "parse/NodeFeatureMap.hpp"

#include "parse/TreeSitterUtils.hpp"

#include <algorithm>
#include <iterator>

namespace cppi::parse {

namespace {

struct Entry {
    std::string_view kind;
    Feature feature;
};

// Node kinds from tree-sitter-cpp's grammar. Kinds not listed here are
// neutral (they don't introduce a feature by themselves).
constexpr Entry kTable[] = {
    {"call_expression", Feature::FunctionCalls},

    {"init_declarator", Feature::Variables},
    {"assignment_expression", Feature::Variables},

    {"binary_expression", Feature::Operators},
    {"unary_expression", Feature::Operators},
    {"update_expression", Feature::Operators},
    {"comma_expression", Feature::Operators},
    {"sizeof_expression", Feature::Operators},
    {"cast_expression", Feature::Operators},

    {"switch_statement", Feature::Conditionals},
    {"case_statement", Feature::Conditionals},
    {"conditional_expression", Feature::Conditionals},

    {"while_statement", Feature::Loops},
    {"do_statement", Feature::Loops},
    {"for_statement", Feature::Loops},
    {"break_statement", Feature::Loops},
    {"continue_statement", Feature::Loops},

    {"compound_statement", Feature::Blocks},

    {"function_definition", Feature::UserFunctions},
    {"return_statement", Feature::UserFunctions},

    {"subscript_expression", Feature::Arrays},
    {"array_declarator", Feature::Arrays},
    {"initializer_list", Feature::Arrays},

    {"string_literal", Feature::Strings},
    {"raw_string_literal", Feature::Strings},
    {"concatenated_string", Feature::Strings},
    {"char_literal", Feature::Strings},

    {"struct_specifier", Feature::Structs},
    {"union_specifier", Feature::Structs},
    {"field_expression", Feature::Structs},

    {"class_specifier", Feature::Classes},
    {"access_specifier", Feature::Classes},
    {"this", Feature::Classes},

    {"base_class_clause", Feature::Inheritance},
    {"virtual", Feature::Inheritance},

    {"pointer_declarator", Feature::Pointers},
    {"abstract_pointer_declarator", Feature::Pointers},
    {"pointer_expression", Feature::Pointers},
    {"new_expression", Feature::Pointers},
    {"delete_expression", Feature::Pointers},

    {"reference_declarator", Feature::References},
    {"abstract_reference_declarator", Feature::References},

    {"template_declaration", Feature::Templates},
    {"template_type", Feature::Templates},
    {"template_method", Feature::Templates},

    {"namespace_definition", Feature::Namespaces},
    {"using_declaration", Feature::Namespaces},

    {"try_statement", Feature::Exceptions},
    {"throw_statement", Feature::Exceptions},
    {"catch_clause", Feature::Exceptions},

    {"preproc_include", Feature::Preprocessor},
    {"preproc_def", Feature::Preprocessor},
    {"preproc_function_def", Feature::Preprocessor},
    {"preproc_ifdef", Feature::Preprocessor},
    {"preproc_if", Feature::Preprocessor},
    {"preproc_call", Feature::Preprocessor},

    {"placeholder_type_specifier", Feature::Auto},
    {"for_range_loop", Feature::RangeFor},
    {"lambda_expression", Feature::Lambdas},
    {"nullptr", Feature::Nullptr},
    {"static_assert_declaration", Feature::StaticAssert},
    {"alias_declaration", Feature::TypeAliases},

    {"structured_binding_declarator", Feature::StructuredBindings},

    {"concept_definition", Feature::Concepts},
    {"requires_clause", Feature::Concepts},
    {"requires_expression", Feature::Concepts},

    {"co_await_expression", Feature::Coroutines},
    {"co_return_statement", Feature::Coroutines},
    {"co_yield_statement", Feature::Coroutines},

    {"module_declaration", Feature::Modules},
    {"import_declaration", Feature::Modules},
};

bool has_child_of_kind(TSNode node, std::string_view kind) noexcept {
    const std::uint32_t count = ts_node_child_count(node);
    for (std::uint32_t i = 0; i < count; ++i) {
        if (kind_of(ts_node_child(node, i)) == kind) {
            return true;
        }
    }
    return false;
}

bool is_floating_literal(std::string_view text) noexcept {
    const bool hex = text.size() > 1 && text[0] == '0' && (text[1] == 'x' || text[1] == 'X');
    if (hex) {
        return text.find_first_of(".pP") != std::string_view::npos;
    }
    return text.find_first_of(".eE") != std::string_view::npos;
}

}  // namespace

std::optional<Feature> NodeFeatureMap::number_literal_feature(std::string_view text) noexcept {
    if (is_floating_literal(text)) {
        return Feature::FloatingPoint;
    }
    if (text.find('\'') != std::string_view::npos) {
        return Feature::DigitSeparators;
    }
    if (text.size() > 1 && text[0] == '0' && (text[1] == 'b' || text[1] == 'B')) {
        return Feature::BinaryLiterals;
    }
    return std::nullopt;
}

std::optional<Feature> NodeFeatureMap::feature_of(TSNode node, std::string_view source) noexcept {
    const std::string_view kind = kind_of(node);

    // Kinds whose feature depends on their contents.
    if (kind == "if_statement") {
        return has_child_of_kind(node, "constexpr") ? Feature::IfConstexpr : Feature::Conditionals;
    }
    if (kind == "enum_specifier") {
        return (has_child_of_kind(node, "class") || has_child_of_kind(node, "struct")) ? Feature::EnumClass
                                                                                       : Feature::Enums;
    }
    if (kind == "type_qualifier") {
        const auto text = text_of(node, source);
        if (text == "constexpr" || text == "consteval" || text == "constinit") {
            return Feature::Constexpr;
        }
        return std::nullopt;
    }
    if (kind == "number_literal") {
        return number_literal_feature(text_of(node, source));
    }
    if (kind == "placeholder_type_specifier") {
        // `void f(auto x)`: an abbreviated function template (C++20); `auto x = 1;` is C++11.
        const TSNode param = ts_node_parent(node);
        const TSNode list = ts_node_is_null(param) ? param : ts_node_parent(param);
        const TSNode declarator = ts_node_is_null(list) ? list : ts_node_parent(list);
        if (!ts_node_is_null(declarator) && kind_of(param) == "parameter_declaration" &&
            kind_of(declarator) == "function_declarator") {
            return Feature::Concepts;
        }
        return Feature::Auto;
    }
    if (kind == "parameter_declaration") {
        // `template <Number T>`: a type parameter constrained by a concept.
        const TSNode parent = ts_node_parent(node);
        if (!ts_node_is_null(parent) && kind_of(parent) == "template_parameter_list") {
            return Feature::Concepts;
        }
    }
    if (kind == "null") {  // newer grammars: `nullptr` and `NULL` share one node kind
        return text_of(node, source) == "nullptr" ? std::optional<Feature>(Feature::Nullptr) : std::nullopt;
    }
    if (kind == "declaration") {
        // A prototype declares a function, not a variable.
        const TSNode declarator = ts_node_child_by_field_name(node, "declarator", 10);
        if (!ts_node_is_null(declarator) && kind_of(declarator) == "function_declarator") {
            return Feature::UserFunctions;
        }
        return Feature::Variables;
    }
    if (kind == "primitive_type") {
        const auto text = text_of(node, source);
        if (text == "double" || text == "float") {
            return Feature::FloatingPoint;
        }
        if (text == "char") {
            return Feature::Strings;
        }
        return std::nullopt;
    }
    if (kind == "template_function") {
        // static_cast<int>(x) is a cast, not a template.
        const TSNode name = ts_node_child_by_field_name(node, "name", 4);
        const auto spelling = ts_node_is_null(name) ? std::string_view{} : text_of(name, source);
        if (spelling == "static_cast" || spelling == "dynamic_cast" || spelling == "const_cast" ||
            spelling == "reinterpret_cast") {
            return Feature::Operators;
        }
        return Feature::Templates;
    }

    const auto* it =
        std::find_if(std::begin(kTable), std::end(kTable), [kind](const Entry& e) { return e.kind == kind; });
    if (it == std::end(kTable)) {
        return std::nullopt;
    }
    return it->feature;
}

}  // namespace cppi::parse
