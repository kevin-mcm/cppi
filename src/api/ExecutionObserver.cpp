#include <cppi/ExecutionObserver.hpp>

namespace cppi {

ExecutionObserver::~ExecutionObserver() = default;
void ExecutionObserver::on_host_call(const HostCall& /*call*/) {}
void ExecutionObserver::on_step(SourceRange /*location*/, std::uint64_t /*operations_used*/) {}
void ExecutionObserver::on_output(std::string_view /*text*/) {}

}  // namespace cppi
