#pragma once

/// @file DeclarationBinder.hpp
/// @brief Binds declarations: variables (local and global), functions and their
/// overloads, structs and classes (layout, inheritance, virtual functions),
/// enums and type aliases.
///
/// Also registers what the host exposes.
///
/// @author kevin-mcm <kevincardenasmiranda9@gmail.com>
/// @date 2026-10-08

#include "sema/AnalysisContext.hpp"
#include "sema/BoundTree.hpp"
#include "sema/TypeResolver.hpp"

#include "ast/Stmt.hpp"

#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace cppi::sema {

/// Binds every kind of declaration and owns what must happen once the whole
/// program is known (undefined functions, `main`, destructors of globals,
/// catch tables).
class DeclarationBinder {
public:
    /// @param ctx The shared analysis state.
    explicit DeclarationBinder(AnalysisContext& ctx) noexcept : ctx_(ctx) {}

    /// Makes the host's functions and enums visible to player code.
    void register_host();

    /// `int a = 1, b;` (local or, at the top level, global). Statements go to
    /// `*out`; a variable that needs destruction redirects `out` into a nested
    /// block whose cleanup destroys it.
    void bind_variables(const ast::DeclStmt& decl, SourceRange range, std::vector<BStmt>*& out);
    /// The variable of a range-based for, initialized from `element`.
    bool bind_loop_variable(const ast::DeclStmt& decl, BExprPtr element, SourceRange range, std::vector<BStmt>*& out);

    /// Declares or defines a function (or adds an overload).
    void bind_function(const ast::FunctionDecl& decl, SourceRange range);
    /// `out` receives the initialization of static data members (null when a
    /// class template is instantiated: static data members are not allowed there).
    void bind_record(const ast::RecordDef& def, SourceRange range, std::vector<BStmt>* out = nullptr);
    /// `enum` / `enum class`: declares the type and its enumerators.
    void bind_enum(const ast::EnumDef& def, SourceRange range);
    /// `typedef` / `using` alias.
    void bind_alias(const ast::AliasDecl& alias, SourceRange range);

    /// Marks the names a rejected statement would have declared, so later
    /// uses do not produce a cascade of errors.
    void poison(const ast::Stmt& stmt);

    /// End of the program: functions called but never defined, the call to
    /// `main` if the player wrote one, destructors of globals.
    void finish(std::vector<BStmt>& script);

    /// `Guard g(n);` parses like a function declaration; when some
    /// "parameter" cannot be a type (it names a variable or an enumerator, or
    /// is `true`, `false` or `nullptr`) it is really an object: returns that
    /// declaration (the prototype's object reading), or null.
    [[nodiscard]] const ast::DeclStmt* as_object_declaration(const ast::FunctionDecl& decl);

    /// A function that default-initializes `*this` for a class without
    /// constructors (members with constructors or default initializers, bases):
    /// `new T` and `new T[n]` run it on each object. Written on first use.
    /// `site`: the code that needs it, where errors in it are reported.
    [[nodiscard]] std::uint32_t implicit_default_constructor(std::uint32_t record, SourceRange site);

    /// The entry of BoundProgram::thrown for exceptions of `type`.
    [[nodiscard]] std::uint32_t thrown_index(TypeRef type);

    /// Destructors of globals, in reverse order of construction.
    [[nodiscard]] std::vector<BStmt>& global_cleanup() noexcept { return global_cleanup_; }

    /// Instantiates a function template's declaration (template parameters
    /// are already visible as types); returns its function id.
    std::optional<std::uint32_t> instantiate_function(const ast::FunctionDecl& decl, std::string display,
                                                      const std::function<void(std::uint32_t)>& registered);
    /// What instantiate_method() needs to instantiate a generic lambda's operator().
    struct GenericLambda {
        /// The types of its `auto` parameters.
        const std::vector<TypeRef>* autos = nullptr;  ///< the types of its `auto` parameters
        /// Its operator() is const (not `mutable`).
        bool is_const = true;
        /// The return type is deduced from the body.
        bool deduce_return = false;
    };
    /// The same for a member function template of `record` (or, with
    /// `lambda`, the operator() of a generic lambda's closure class).
    std::optional<std::uint32_t> instantiate_method(std::uint32_t record, const ast::FunctionDecl& decl,
                                                    std::string display,
                                                    const std::function<void(std::uint32_t)>& registered,
                                                    const GenericLambda* lambda = nullptr);

    /// One variable a lambda captures.
    struct Capture {
        /// The captured variable.
        std::string name;
        /// Captured by reference.
        bool by_reference = false;
        TypeRef type = 0;       ///< the captured object's type
        bool is_const = false;  ///< by reference: the variable is const
    };
    /// Creates the closure class of a lambda and binds its operator();
    /// returns the class type.
    std::optional<TypeRef> bind_lambda(const ast::LambdaExpr& lambda, const std::vector<Capture>& captures,
                                       SourceRange range);

    /// Creates a function entry (FunctionInfo + empty BoundFunction).
    std::uint32_t add_function(FunctionInfo info);
    /// Binds a function's body (parameters, statements, implicit parts of
    /// constructors and destructors).
    void define_body(std::uint32_t id, const ast::FunctionDecl* decl);

private:
    /// A resolved function signature.
    struct Signature {
        /// Return type.
        TypeRef return_type = 0;
        /// Parameters.
        std::vector<ParamInfo> params;
    };
    /// Resolves the return and parameter types of `decl`. Constructors and
    /// destructors have no return type (`constructor_like`).
    [[nodiscard]] std::optional<Signature> resolve_signature(const ast::FunctionDecl& decl, bool constructor_like,
                                                             bool deduced_return = false);
    /// True if both parameter lists have the same types (overloads vs.
    /// redeclarations).
    [[nodiscard]] bool same_params(const std::vector<ParamInfo>& a, const std::vector<ParamInfo>& b) const;
    /// `void Point::move() { ... }`: defines a member function declared in its class.
    void define_out_of_class(const ast::FunctionDecl& decl, SourceRange range);
    /// The names of `auto [a, b] = ...` refer into the hidden variable `whole`.
    bool bind_structured(const Symbol& whole, const ast::VarDeclarator& var, SourceRange range,
                         std::vector<BStmt>& out);
    /// Declares one variable and binds its initialization (`loop_element`: the
    /// element of a range-based for, if it is one).
    bool declare_variable(const ast::TypeSpec& spec, const ast::VarDeclarator& var, SourceRange range,
                          std::vector<BStmt>*& out, BExprPtr loop_element);

    // Static data members: globals named through their class.
    /// A static data member.
    struct StaticMember {
        /// The class it belongs to.
        std::uint32_t record = 0;
        std::string display;  ///< "Counter::count"
        /// Where it is declared.
        SourceRange range;
        bool defined = false;             ///< initialized in the class or by a definition outside it
        bool needs_construction = false;  ///< a class with constructors: only its definition builds it
    };
    /// Declares a static data member inside its class.
    bool declare_static_member(std::uint32_t record, const ast::FieldDecl& field, const ast::VarDeclarator& var,
                               std::vector<BStmt>* out);
    /// `int Counter::count = 0;`: defines a static data member outside its class.
    bool define_static_member(const ast::TypeSpec& spec, const ast::VarDeclarator& var, SourceRange range,
                              std::vector<BStmt>*& out);
    /// Binds as if inside the class: its static members are visible and its private parts accessible.
    template <class F>
    auto in_class_scope(std::uint32_t record, F body);

    // Records
    /// Places the fields, bases and object headers of `record`.
    void layout_record(std::uint32_t record);
    /// Decides whether copies are plain cell copies and, if a member needs
    /// its own copy constructor or operator=, writes the implicit ones.
    void synthesize_copies(std::uint32_t record, const ast::RecordDef& def,
                           std::vector<std::pair<std::uint32_t, const ast::FunctionDecl*>>& bodies);
    /// Builds the virtual tables of every polymorphic subobject of `record` and
    /// decides whether it is abstract.
    void build_dispatch(std::uint32_t record);
    /// Appends the polymorphic, non-virtual subobjects of `record` at `offset`
    /// (record, offset), each once.
    void collect_subobjects(std::uint32_t complete, std::uint32_t record, std::uint32_t offset,
                            std::vector<std::pair<std::uint32_t, std::uint32_t>>& out) const;
    /// The function that overrides `function` for the subobject of
    /// `target_record` at `target_offset` inside a complete `record`, and the
    /// offset of its class; nullopt if the subobject is not found.
    [[nodiscard]] std::optional<std::pair<std::uint32_t, std::uint32_t>> final_overrider(std::uint32_t record,
                                                                                         std::uint32_t offset,
                                                                                         std::uint32_t target_record,
                                                                                         std::uint32_t target_offset,
                                                                                         std::uint32_t function) const;
    /// True if `candidate` overrides `function`: same name and parameters, or
    /// both destructors.
    [[nodiscard]] bool overrides(std::uint32_t candidate, std::uint32_t function) const;

    // Exceptions: which catch clause takes which thrown type, once all are known.
    /// Fills CatchClauseInfo::matches.
    void resolve_catches();
    /// Cells from a complete `derived` object to its unique `base` subobject.
    [[nodiscard]] std::optional<std::uint32_t> base_offset(std::uint32_t derived, std::uint32_t base) const;
    /// Cells from a thrown object of type `thrown` to what a clause catching
    /// `caught` sees; nullopt if the clause does not catch it.
    [[nodiscard]] std::optional<std::uint32_t> catch_offset(TypeRef thrown, TypeRef caught) const;

    /// The shared analysis state.
    AnalysisContext& ctx_;
    /// See global_cleanup().
    std::vector<BStmt> global_cleanup_;
    /// Static data members, by symbol.
    std::map<const Symbol*, StaticMember> statics_;
    /// The player's `main`, if declared.
    std::optional<std::uint32_t> main_;
    bool deduce_next_ = false;                             ///< the next define_body deduces its return type
    const std::vector<TypeRef>* generic_autos_ = nullptr;  ///< types for `auto` parameters being resolved
};

}  // namespace cppi::sema
