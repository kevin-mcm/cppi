#include "vm/HostCallInvoker.hpp"

#include <exception>
#include <string>

namespace cppi::detail {

Value HostCallInvoker::invoke(const HostFunctionInfo& fn, HostCall& call) const {
    Value result;
    try {
        result = fn.impl(call);
    } catch (const std::exception& e) {
        call.fail(kHostFault, std::string("host exception: ") + e.what());
    } catch (...) {
        call.fail(kHostFault, "host exception");
    }

    // Guard against host bugs: the declared signature is part of the contract.
    if (!call.failed() && fn.result != types::Void && result.type() != fn.result) {
        call.fail(kHostFault, "host function returned " + host_.type_name(result.type()) + ", expected " +
                                  host_.type_name(fn.result));
    }
    return call.failed() ? Value::void_value() : result;
}

}  // namespace cppi::detail
