/// @file HostRegistry.cpp
/// @brief Implementation of HostRegistry.hpp.
///
/// @author kevin-mcm <kevincardenasmiranda9@gmail.com>
/// @date 2026-10-08

#include <cppi/HostRegistry.hpp>

#include "support/CppIdentifier.hpp"

#include <algorithm>
#include <cstddef>
#include <limits>
#include <stdexcept>
#include <utility>

namespace cppi {

void HostRegistry::check_new_name(std::string_view name) const {
    if (!detail::CppIdentifier::is_valid(name)) {
        throw std::invalid_argument("cppi: '" + std::string(name) + "' is not a valid identifier");
    }
    if (function_index_.contains(name) || constants_.contains(name) || enum_index_.contains(name)) {
        throw std::invalid_argument("cppi: name '" + std::string(name) + "' is already registered");
    }
}

TypeId HostRegistry::add_enum(std::string name, std::initializer_list<std::string_view> enumerators) {
    std::vector<std::string> names;
    names.reserve(enumerators.size());
    for (auto e : enumerators) {
        names.emplace_back(e);
    }
    return add_enum(std::move(name), names);
}

TypeId HostRegistry::add_enum(std::string name, const std::vector<std::string>& enumerators) {
    check_new_name(name);
    if (enumerators.empty()) {
        throw std::invalid_argument("cppi: enum '" + name + "' has no enumerators");
    }
    for (std::size_t i = 0; i < enumerators.size(); ++i) {
        check_new_name(enumerators[i]);
        if (enumerators[i] == name ||
            std::find(enumerators.begin(), enumerators.begin() + static_cast<std::ptrdiff_t>(i), enumerators[i]) !=
                enumerators.begin() + static_cast<std::ptrdiff_t>(i)) {
            throw std::invalid_argument("cppi: duplicate enumerator '" + enumerators[i] + "'");
        }
    }
    const auto next = types::kFirstUserType + enums_.size();
    if (next > std::numeric_limits<std::uint16_t>::max()) {
        throw std::invalid_argument("cppi: too many enums");
    }
    const TypeId id{static_cast<std::uint16_t>(next)};

    for (std::size_t i = 0; i < enumerators.size(); ++i) {
        constants_.emplace(enumerators[i], Constant{id, static_cast<std::int64_t>(i)});
    }
    enum_index_.emplace(name, id);
    enums_.push_back(EnumInfo{id, std::move(name), enumerators});
    return id;
}

FunctionBuilder HostRegistry::function(std::string name) {
    check_new_name(name);
    return {*this, std::move(name)};
}

FunctionId HostRegistry::register_function(FunctionInfo info) {
    check_new_name(info.name);  // the name may have been taken since function() was called
    const FunctionId id{static_cast<std::uint32_t>(functions_.size())};
    function_index_.emplace(info.name, id);
    functions_.push_back(std::move(info));
    return id;
}

std::optional<FunctionId> HostRegistry::find_function(std::string_view name) const noexcept {
    if (auto it = function_index_.find(name); it != function_index_.end()) {
        return it->second;
    }
    return std::nullopt;
}

std::optional<HostRegistry::Constant> HostRegistry::find_constant(std::string_view name) const noexcept {
    if (auto it = constants_.find(name); it != constants_.end()) {
        return it->second;
    }
    return std::nullopt;
}

const HostRegistry::EnumInfo* HostRegistry::find_enum(TypeId id) const noexcept {
    if (id.value < types::kFirstUserType) {
        return nullptr;
    }
    const std::size_t index = id.value - types::kFirstUserType;
    return index < enums_.size() ? &enums_[index] : nullptr;
}

const HostRegistry::EnumInfo* HostRegistry::find_enum(std::string_view name) const noexcept {
    if (auto it = enum_index_.find(name); it != enum_index_.end()) {
        return find_enum(it->second);
    }
    return nullptr;
}

bool HostRegistry::is_known_type(TypeId id) const noexcept {
    return id == types::Void || id == types::Bool || id == types::Int || id == types::Char || id == types::Long ||
           id == types::Double || find_enum(id) != nullptr;
}

std::string HostRegistry::type_name(TypeId id) const {
    if (id == types::Void) return "void";
    if (id == types::Bool) return "bool";
    if (id == types::Int) return "int";
    if (id == types::Char) return "char";
    if (id == types::Long) return "long";
    if (id == types::Double) return "double";
    if (const auto* e = find_enum(id)) return e->name;
    return "<unknown>";
}

std::string_view HostRegistry::enumerator_name(TypeId id, std::int64_t value) const noexcept {
    const auto* e = find_enum(id);
    if (e == nullptr || value < 0 || static_cast<std::size_t>(value) >= e->enumerators.size()) {
        return {};
    }
    return e->enumerators[static_cast<std::size_t>(value)];
}

}  // namespace cppi
