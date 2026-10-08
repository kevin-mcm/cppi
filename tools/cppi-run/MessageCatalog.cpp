#include "MessageCatalog.hpp"

#include "EnglishCatalog.hpp"
#include "SpanishCatalog.hpp"

namespace cppi_run {

MessageCatalog::~MessageCatalog() = default;

std::unique_ptr<MessageCatalog> MessageCatalog::create(Language language) {
    switch (language) {
        case Language::Spanish: return std::make_unique<SpanishCatalog>();
        case Language::English: break;
    }
    return std::make_unique<EnglishCatalog>();
}

}  // namespace cppi_run
