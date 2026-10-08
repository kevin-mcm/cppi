#include "ValueFormatter.hpp"

namespace cppi_run {

std::string ValueFormatter::format(const cppi::HostRegistry& host, const cppi::Value& v) {
    if (v.type() == cppi::types::Int) return std::to_string(v.as_int());
    if (v.type() == cppi::types::Bool) return v.as_bool() ? "true" : "false";
    return std::string(host.enumerator_name(v.type(), v.raw()));
}

}  // namespace cppi_run
