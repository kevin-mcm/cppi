#include "vm/VariableInspector.hpp"

#include <algorithm>
#include <bit>

#include "support/NumberFormat.hpp"

namespace cppi::detail {

namespace {

bool before_or_at(SourceLocation a, SourceLocation b) {
    return a.line < b.line || (a.line == b.line && a.column <= b.column);
}

bool in_scope(const VariableMeta& v, SourceLocation at) {
    return before_or_at(v.scope.begin, at) && (v.scope.end.line == 0 || before_or_at(at, v.scope.end));
}

constexpr int kMaxDepth = 4;
constexpr std::uint32_t kMaxElements = 64;

}  // namespace

std::string VariableInspector::scalar_text(const TypeMeta& type, std::int64_t raw) const {
    switch (type.kind) {
        case ValueKind::Bool: return raw != 0 ? "true" : "false";
        case ValueKind::Char: {
            if (raw >= 32 && raw < 127) {
                return std::string("'") + static_cast<char>(raw) + "' (" + std::to_string(raw) + ")";
            }
            return std::to_string(raw);
        }
        case ValueKind::Double: return NumberFormat::shortest(std::bit_cast<double>(raw));
        case ValueKind::ULong: return std::to_string(static_cast<std::uint64_t>(raw));
        case ValueKind::Enum:
            for (std::size_t i = 0; i < type.enum_values.size(); ++i) {
                if (type.enum_values[i] == raw) {
                    return type.enumerators[i];
                }
            }
            return std::to_string(raw);
        case ValueKind::Pointer:
        case ValueKind::Nullptr: {
            if (raw == 0) {
                return "nullptr";
            }
            const std::uint32_t address = PackedPointer::address(raw);
            if (address >= Memory::kHeapBase) {
                return "heap object";
            }
            if (auto name = describe(address)) {
                return "&" + *name;
            }
            return "dangling pointer";
        }
        default: return std::to_string(raw);
    }
}

Variable VariableInspector::render(std::string name, std::uint32_t address, std::uint32_t type, int depth) const {
    const TypeMeta& t = program_.types[type];
    Variable v;
    v.name = std::move(name);
    v.type = t.name;
    if (t.kind == ValueKind::Array) {
        const std::uint32_t elem_cells = std::max<std::uint32_t>(1, program_.types[t.element].cells);
        std::string summary;
        bool simple = true;
        for (std::uint32_t i = 0; i < t.count && i < kMaxElements && depth < kMaxDepth; ++i) {
            v.children.push_back(render("[" + std::to_string(i) + "]", address + i * elem_cells, t.element, depth + 1));
            simple = simple && v.children.back().children.empty();
            summary += (i > 0 ? ", " : "") + v.children.back().value;
        }
        v.value = simple && t.count <= 8 ? "{" + summary + "}" : "{...}";
        return v;
    }
    if (t.kind == ValueKind::Record && render_container(v, t, address, depth)) {
        return v;
    }
    if (t.kind == ValueKind::Record) {
        std::string summary;
        bool simple = true;
        for (const auto& f : t.fields) {
            if (depth >= kMaxDepth) {
                break;
            }
            v.children.push_back(render(f.name, address + f.offset, f.type, depth + 1));
            simple = simple && v.children.back().children.empty();
            summary += (summary.empty() ? "" : ", ") + f.name + "=" + v.children.back().value;
        }
        v.value = simple && t.fields.size() <= 6 ? "{" + summary + "}" : "{...}";
        return v;
    }
    if (!memory_.initialized(address)) {
        v.initialized = false;
        v.value = "?";
        return v;
    }
    v.value = scalar_text(t, memory_.value(address));
    return v;
}

bool VariableInspector::render_container(Variable& v, const TypeMeta& t, std::uint32_t address, int depth) const {
    // The prelude's containers keep their elements in a heap block: `data_`
    // points to it and `size_` counts the ones in use.
    const bool vector = t.name.starts_with("vector<");
    if (!vector && t.name != "string") {
        return false;
    }
    const TypeMeta::Field* data = nullptr;
    const TypeMeta::Field* size = nullptr;
    for (const auto& f : t.fields) {
        if (f.name == "data_" && program_.types[f.type].kind == ValueKind::Pointer) {
            data = &f;
        } else if (f.name == "size_") {
            size = &f;
        }
    }
    if (data == nullptr || size == nullptr || !memory_.initialized(address + size->offset) ||
        !memory_.initialized(address + data->offset)) {
        return false;
    }
    const auto count = static_cast<std::uint32_t>(std::max<std::int64_t>(0, memory_.value(address + size->offset)));
    const std::int64_t raw = memory_.value(address + data->offset);
    const std::uint32_t element = program_.types[data->type].element;
    const std::uint32_t cells = std::max<std::uint32_t>(1, program_.types[element].cells);
    const std::uint32_t block = raw == 0 ? 0 : PackedPointer::address(raw);
    if (!vector) {
        std::string text;
        for (std::uint32_t i = 0; i < count && block != 0 && i < 256; ++i) {
            text += static_cast<char>(memory_.value(block + i));
        }
        v.value = "\"" + text + "\"";
        return true;
    }
    std::string summary;
    bool simple = true;
    for (std::uint32_t i = 0; i < count && block != 0 && i < kMaxElements && depth < kMaxDepth; ++i) {
        v.children.push_back(render("[" + std::to_string(i) + "]", block + i * cells, element, depth + 1));
        simple = simple && v.children.back().children.empty();
        summary += (i > 0 ? ", " : "") + v.children.back().value;
    }
    if (count == 0) {
        v.value = "{}";
    } else if (!simple) {
        v.value = "{...}";
    } else if (count <= 8) {
        v.value = "{" + summary + "}";
    } else {
        v.value = "{size=" + std::to_string(count) + "}";
    }
    return true;
}

std::optional<std::string> VariableInspector::path_in(std::uint32_t address, std::uint32_t start, std::uint32_t type,
                                                      const std::string& name) const {
    const TypeMeta& t = program_.types[type];
    const std::uint32_t cells = std::max<std::uint32_t>(1, t.cells);
    if (address < start || address >= start + cells) {
        return std::nullopt;
    }
    if (address == start && t.kind != ValueKind::Array && t.kind != ValueKind::Record) {
        return name;
    }
    if (t.kind == ValueKind::Array) {
        const std::uint32_t elem = std::max<std::uint32_t>(1, program_.types[t.element].cells);
        const std::uint32_t i = (address - start) / elem;
        return path_in(address, start + i * elem, t.element, name + "[" + std::to_string(i) + "]");
    }
    if (t.kind == ValueKind::Record) {
        for (const auto& f : t.fields) {
            if (auto p = path_in(address, start + f.offset, f.type, name + "." + f.name)) {
                return p;
            }
        }
    }
    return name;
}

std::optional<std::string> VariableInspector::describe(std::uint32_t address) const {
    for (const auto& frame : frames_) {
        for (const auto& v : program_.functions[frame.function].locals) {
            if (v.reference) {
                continue;
            }
            if (auto p = path_in(address, frame.base + v.offset, v.type, v.name)) {
                return p;
            }
        }
    }
    for (const auto& v : program_.globals) {
        if (auto p = path_in(address, Memory::kStackBase + v.offset, v.type, v.name)) {
            return p;
        }
    }
    return std::nullopt;
}

std::vector<StackFrame> VariableInspector::call_stack() const {
    std::vector<StackFrame> out;
    for (const auto& frame : frames_) {
        const FunctionMeta& fn = program_.functions[frame.function];
        StackFrame sf;
        sf.function = fn.name;
        sf.location = frame.location;
        for (const auto& v : fn.locals) {
            if (!in_scope(v, frame.location.begin)) {
                continue;
            }
            std::uint32_t address = frame.base + v.offset;
            if (v.reference) {
                if (!memory_.initialized(address)) {
                    continue;  // not bound yet
                }
                address = PackedPointer::address(memory_.value(address));
            }
            sf.locals.push_back(render(v.name, address, v.type, 0));
        }
        out.push_back(std::move(sf));
    }
    return out;
}

std::vector<Variable> VariableInspector::globals() const {
    std::vector<Variable> out;
    out.reserve(program_.globals.size());
    for (const auto& v : program_.globals) {
        out.push_back(render(v.name, Memory::kStackBase + v.offset, v.type, 0));
    }
    return out;
}

}  // namespace cppi::detail
