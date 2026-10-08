/// @file TraceObserver.cpp
/// @brief Implementation of TraceObserver.hpp.
///
/// @author kevin-mcm <kevincardenasmiranda9@gmail.com>
/// @date 2026-10-08

#include "TraceObserver.hpp"

#include "ValueFormatter.hpp"

namespace cppi_run {

void TraceObserver::on_host_call(const cppi::HostCall& call) {
    *out_ << "  " << call.location().begin.line << ':' << call.location().begin.column << "  " << call.function_name()
          << '(';
    for (std::size_t i = 0; i < call.args().size(); ++i) {
        *out_ << (i > 0 ? ", " : "") << ValueFormatter::format(*host_, call.arg(i));
    }
    *out_ << ")\n";
}

}  // namespace cppi_run
