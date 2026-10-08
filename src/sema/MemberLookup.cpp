/// @file MemberLookup.cpp
/// @brief Implementation of MemberLookup.hpp.
///
/// @author kevin-mcm <kevincardenasmiranda9@gmail.com>
/// @date 2026-10-08

#include "sema/MemberLookup.hpp"

#include <algorithm>
#include <string>

namespace cppi::sema {

namespace {

ast::Access weaker(ast::Access a, ast::Access b) {
    return static_cast<std::uint8_t>(a) > static_cast<std::uint8_t>(b) ? a : b;
}

}  // namespace

void MemberLookup::search(std::uint32_t record, std::string_view name, ast::Access path_access,
                          std::vector<MemberResult>& found) const {
    const RecordInfo& info = types_.record_at(record);
    for (const FieldInfo& f : info.fields) {
        if (f.name == name) {
            found.push_back(
                MemberResult{MemberResult::Kind::Field, record, &f, nullptr, weaker(f.access, path_access)});
            return;
        }
    }
    if (auto it = info.methods.find(name); it != info.methods.end()) {
        found.push_back(MemberResult{MemberResult::Kind::Methods, record, nullptr, &it->second, path_access});
        return;
    }
    for (const BaseInfo& base : info.bases) {
        // Private members of a base stay private to it; its public members
        // become as accessible as the inheritance allows.
        search(base.record, name, weaker(path_access, base.access), found);
    }
}

MemberResult MemberLookup::find(std::uint32_t record, std::string_view name) const {
    std::vector<MemberResult> found;
    search(record, name, ast::Access::Public, found);
    if (found.empty()) {
        return {};
    }
    MemberResult first = found.front();
    for (const MemberResult& other : found) {
        if (other.owner == first.owner) {
            // Reached through two paths: one subobject if every path is virtual.
            if (other.owner != record) {
                auto path = hierarchy_.find_base(record, other.owner);
                if (path && path->ambiguous) {
                    return MemberResult{MemberResult::Kind::Ambiguous};
                }
            }
            continue;
        }
        return MemberResult{MemberResult::Kind::Ambiguous};
    }
    if (first.owner != record) {
        auto path = hierarchy_.find_base(record, first.owner);
        if (path && path->ambiguous) {
            return MemberResult{MemberResult::Kind::Ambiguous};
        }
    }
    return first;
}

std::vector<std::string> MemberLookup::names(std::uint32_t record) const {
    std::vector<std::string> out;
    const RecordInfo& info = types_.record_at(record);
    out.reserve(info.fields.size());
    for (const FieldInfo& f : info.fields) {
        out.push_back(f.name);
    }
    for (const auto& [name, ids] : info.methods) {
        out.push_back(name);
    }
    for (const BaseInfo& base : info.bases) {
        auto more = names(base.record);
        out.insert(out.end(), more.begin(), more.end());
    }
    return out;
}

bool MemberLookup::accessible(ast::Access access, std::uint32_t owner, std::optional<std::uint32_t> context) const {
    if (access == ast::Access::Public) {
        return true;
    }
    if (!context) {
        return false;
    }
    if (*context == owner) {
        return true;
    }
    if (access == ast::Access::Protected) {
        return hierarchy_.find_base(*context, owner).has_value();
    }
    return false;
}

}  // namespace cppi::sema
