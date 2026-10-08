#include "SpanishCatalog.hpp"

#include "MessageTable.hpp"

namespace cppi_run {

namespace {

constexpr MessageEntry kDiagnostics[] = {
    {"syntax-error", "este código no es C++ válido"},
    {"missing-token", "falta '{token}' aquí"},
    {"missing-token-at-line-end", "falta '{token}' al final de la línea {line}"},
    {"nesting-too-deep", "las expresiones están anidadas en más de {limit} niveles"},
    {"unknown-function", "no existe ninguna función llamada '{name}'"},
    {"unknown-identifier", "'{name}' no ha sido declarado"},
    {"argument-count-mismatch", "'{function}' recibe {expected} argumento(s), pero se pasaron {actual}"},
    {"argument-type-mismatch", "el argumento {index} de '{function}' debe ser '{expected}', no '{actual}'"},
    {"not-callable", "'{name}' no es una función y no se puede llamar"},
    {"integer-out-of-range", "el número {literal} es demasiado grande"},
    {"function-not-called", "'{name}' es una función: agrega () para llamarla"},
    {"expression-has-no-effect", "esta instrucción no tiene ningún efecto"},
    {"feature-locked", "aún no desbloqueaste {feature}"},
    {"feature-requires-standard", "para usar {feature} necesitas {required} (estás usando {current})"},
    {"feature-not-implemented", "{feature}: todavía no disponible en esta versión del juego"},
    {"unsupported-syntax", "esta construcción ({construct}) no está soportada"},
    {"budget-exhausted", "sin operaciones: se agotó el presupuesto de {budget}"},
    {"host-error", "'{function}' falló: {detail}"},
    {"redefinition", "'{name}' ya fue declarado"},
    {"not-assignable", "esto no se puede modificar[: '{name}' es constante]"},
    {"invalid-operands", "operandos no válidos para {op}: '{left}'[ y '{right}']"},
    {"cannot-convert", "no se puede convertir '{from}' a '{to}'"},
    {"misplaced-jump", "'{statement}' no está dentro de un bucle o switch"},
    {"return-type-mismatch", "'{function}' debe devolver '{expected}', no '{actual}'"},
    {"unknown-type", "tipo desconocido '{name}'"},
    {"invalid-array-size", "tamaño de arreglo no válido[ ({size})]"},
    {"not-constant", "esto debe ser una constante conocida antes de ejecutar"},
    {"duplicate-case", "el case {value} aparece dos veces"},
    {"no-member", "'{type}' no tiene un miembro llamado '{member}'"},
    {"not-subscriptable", "'{type}' no se puede indexar con []"},
    {"too-many-initializers", "demasiados inicializadores: como máximo {expected}, pero hay {actual}"},
    {"uninitialized-const", "'{name}' debe inicializarse al declararse"},
    {"missing-return", "'{function}' puede terminar sin devolver un valor"},
    {"inaccessible-member", "'{member}' es {access} en '{class}'"},
    {"ambiguous-name", "'{name}' es ambiguo: se hereda más de una vez"},
    {"abstract-class", "'{class}' es abstracta[: '{function}' no tiene implementación]"},
    {"no-matching-function", "ninguna versión de '{function}' acepta ({arguments})"},
    {"static-assertion-failed", "static_assert falló[: {message}]"},
    {"assignment-in-condition", "'=' asigna un valor; para comparar usa '=='"},
    {"reference-needs-lvalue", "una '{type}' debe referirse a una variable"},
    {"unsupported-type", "el tipo '{name}' no está soportado"},
    {"declaration-not-allowed", "{construct}: no está permitido aquí"},
    {"no-default-constructor", "'{class}' no tiene un constructor sin argumentos"},
    {"nothing-to-override", "'{function}' está marcada override pero no sobrescribe nada"},
    {"template-deduction", "no se pueden deducir los argumentos de plantilla de '{function}'"},
    {"ambiguous-call", "la llamada a '{function}' con ({arguments}) es ambigua"},
    {"undefined-function", "'{function}' se declaró pero nunca se definió"},
    {"deleted-function", "'{function}' está eliminada: no se puede usar"},
    {"no-operator", "no hay operator{op} para '{type}'"},
    {"instantiated-from", "en '{function}', usada aquí"},
    {"constraints-not-satisfied", "los argumentos no cumplen las restricciones de '{function}'[: {constraint}]"},
    {"stack-overflow", "desbordamiento de pila: más de {depth} llamadas anidadas"},
    {"out-of-memory", "sin memoria"},
    {"uncaught-exception", "excepción no capturada de tipo '{type}'[: {what}]"},
    {"exception-during-unwind", "se lanzó una excepción de tipo '{type}' mientras se manejaba otra"},
    {"no-active-exception", "'throw;' sin ninguna excepción en curso"},
    {"division-by-zero", "comportamiento indefinido: división entre cero"},
    {"integer-overflow", "comportamiento indefinido: el resultado no cabe en '{type}'"},
    {"uninitialized-read", "comportamiento indefinido: se lee una variable que aún no tiene valor[ ('{name}')]"},
    {"out-of-bounds", "comportamiento indefinido: el índice {index} está fuera de rango (tamaño {size})"},
    {"null-dereference", "comportamiento indefinido: se usa un puntero nulo"},
    {"use-after-free", "comportamiento indefinido: se usa memoria después de delete"},
    {"double-free", "comportamiento indefinido: se libera la misma memoria dos veces"},
    {"invalid-delete", "comportamiento indefinido: delete de memoria que no vino del new correspondiente"},
    {"flow-off-end", "comportamiento indefinido: '{function}' terminó sin devolver un valor"},
    {"invalid-shift", "comportamiento indefinido: desplazamiento de {amount} bits"},
    {"memory-leak", "fuga de memoria: {count} bloque(s) creados con new nunca se liberaron"},
    {"pure-virtual-call", "comportamiento indefinido: se llama a la función virtual pura '{function}'"},
    {"invalid-pointer", "comportamiento indefinido: acceso con un puntero no válido"},
    {"bad-access", "comportamiento indefinido: se lee un {what} vacío"},
};

constexpr MessageEntry kFeatures[] = {
    {"function-calls", "las llamadas a funciones"},
    {"variables", "las variables"},
    {"operators", "los operadores"},
    {"conditionals", "las condicionales"},
    {"loops", "los bucles"},
    {"blocks", "los bloques { }"},
    {"user-functions", "las funciones propias"},
    {"arrays", "los arreglos"},
    {"strings", "las cadenas de texto"},
    {"floating-point", "los números decimales"},
    {"structs", "los structs"},
    {"classes", "las clases"},
    {"inheritance", "la herencia"},
    {"enums", "los enums"},
    {"pointers", "los punteros"},
    {"references", "las referencias"},
    {"templates", "las plantillas"},
    {"namespaces", "los espacios de nombres"},
    {"exceptions", "las excepciones"},
    {"preprocessor", "las directivas del preprocesador"},
    {"auto", "la deducción de tipos con 'auto'"},
    {"range-for", "los for de rango"},
    {"lambdas", "las lambdas"},
    {"nullptr", "nullptr"},
    {"enum-class", "los enum class"},
    {"static-assert", "static_assert"},
    {"constexpr", "constexpr"},
    {"type-aliases", "los alias de tipo (using)"},
    {"binary-literals", "los literales binarios"},
    {"digit-separators", "los separadores de dígitos"},
    {"structured-bindings", "los structured bindings"},
    {"if-constexpr", "if constexpr"},
    {"concepts", "los concepts"},
    {"coroutines", "las corrutinas"},
    {"modules", "los módulos"},
    {"delegating-constructors", "los constructores delegados"},
};

constexpr MessageEntry kUi[] = {
    {"initial-world", "Mundo inicial"}, {"output", "Salida"},
    {"final-world", "Mundo final"},     {"result", "Resultado"},
    {"operations", "Operaciones"},      {"hay", "Heno"},
    {"position", "Posición"},           {"did-you-mean", "¿quisiste decir '{suggestion}'?"},
    {"bytecode", "Bytecode"},           {"cannot-read", "no se puede leer el archivo"},
};

}  // namespace

std::optional<std::string_view> SpanishCatalog::diagnostic(std::string_view key) const {
    return MessageTable(kDiagnostics).find(key);
}

std::optional<std::string_view> SpanishCatalog::feature(std::string_view key) const {
    return MessageTable(kFeatures).find(key);
}

std::optional<std::string_view> SpanishCatalog::ui(std::string_view key) const {
    return MessageTable(kUi).find(key);
}

std::string_view SpanishCatalog::severity(cppi::Severity severity) const {
    switch (severity) {
        case cppi::Severity::Error: return "error";
        case cppi::Severity::Warning: return "advertencia";
        case cppi::Severity::Note: return "nota";
    }
    return {};
}

std::string_view SpanishCatalog::status(cppi::RunStatus status) const {
    switch (status) {
        case cppi::RunStatus::Running: return "en ejecución";
        case cppi::RunStatus::Completed: return "completado";
        case cppi::RunStatus::BudgetExhausted: return "sin operaciones";
        case cppi::RunStatus::HostError: return "detenido por un error";
        case cppi::RunStatus::Paused: return "en pausa";
        case cppi::RunStatus::RuntimeError: return "detenido por un error en ejecución";
    }
    return {};
}

}  // namespace cppi_run
