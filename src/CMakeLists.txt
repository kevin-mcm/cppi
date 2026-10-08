# The cppi library target.
#
# Author: kevin-mcm <kevincardenasmiranda9@gmail.com>
# Date:   2026-10-08

find_package(tree-sitter REQUIRED CONFIG)
find_package(tree-sitter-cpp REQUIRED CONFIG)

add_library(cppi)
add_library(cppi::cppi ALIAS cppi)

configure_file(${PROJECT_SOURCE_DIR}/include/cppi/Version.hpp.in ${PROJECT_BINARY_DIR}/include/cppi/Version.hpp
               @ONLY)

target_sources(
  cppi
  PRIVATE api/Execution.cpp
          api/ExecutionObserver.cpp
          api/Interpreter.cpp
          api/Program.cpp
          api/ProgramCache.cpp
          codegen/CodeGenerator.cpp
          codegen/ConstantPool.cpp
          codegen/Disassembler.cpp
          codegen/OpCode.cpp
          codegen/Reachability.cpp
          library/Prelude.cpp
          parse/FeatureScanner.cpp
          parse/IntegerLiteralParser.cpp
          parse/NodeFeatureMap.cpp
          parse/Parser.cpp
          parse/SyntaxChecker.cpp
          parse/TreeConverter.cpp
          pipeline/CompilerPipeline.cpp
          sema/AnalysisResult.cpp
          sema/Analyzer.cpp
          sema/CaptureAnalysis.cpp
          sema/ClassHierarchy.cpp
          sema/ConstEvaluator.cpp
          sema/ConstexprInterpreter.cpp
          sema/DeclarationBinder.cpp
          sema/ExpressionBinder.cpp
          sema/FeatureGate.cpp
          sema/ImplicitConversions.cpp
          sema/InitializerBinder.cpp
          sema/LvalueCloner.cpp
          sema/MemberLookup.cpp
          sema/NameSuggester.cpp
          sema/OverloadResolver.cpp
          sema/StatementBinder.cpp
          sema/SymbolTable.cpp
          sema/TemplateEngine.cpp
          sema/TypeResolver.cpp
          sema/TypeTable.cpp
          support/CppIdentifier.cpp
          support/DiagCode.cpp
          support/Diagnostic.cpp
          support/DiagnosticFactory.cpp
          support/Feature.cpp
          support/FeatureSet.cpp
          support/FunctionBuilder.cpp
          support/HostRegistry.cpp
          support/NumberFormat.cpp
          support/Standard.cpp
          vm/HostCallInvoker.cpp
          vm/Memory.cpp
          vm/VariableInspector.cpp
          vm/Vm.cpp
  PUBLIC FILE_SET
         public_headers
         TYPE
         HEADERS
         BASE_DIRS
         ${PROJECT_SOURCE_DIR}/include
         ${PROJECT_BINARY_DIR}/include
         FILES
         ${PROJECT_SOURCE_DIR}/include/cppi/CompileResult.hpp
         ${PROJECT_SOURCE_DIR}/include/cppi/CostModel.hpp
         ${PROJECT_SOURCE_DIR}/include/cppi/Cppi.hpp
         ${PROJECT_SOURCE_DIR}/include/cppi/DiagArg.hpp
         ${PROJECT_SOURCE_DIR}/include/cppi/DiagCode.hpp
         ${PROJECT_SOURCE_DIR}/include/cppi/Diagnostic.hpp
         ${PROJECT_SOURCE_DIR}/include/cppi/Execution.hpp
         ${PROJECT_SOURCE_DIR}/include/cppi/ExecutionObserver.hpp
         ${PROJECT_SOURCE_DIR}/include/cppi/Feature.hpp
         ${PROJECT_SOURCE_DIR}/include/cppi/FeatureSet.hpp
         ${PROJECT_SOURCE_DIR}/include/cppi/FunctionBuilder.hpp
         ${PROJECT_SOURCE_DIR}/include/cppi/FunctionId.hpp
         ${PROJECT_SOURCE_DIR}/include/cppi/HostCall.hpp
         ${PROJECT_SOURCE_DIR}/include/cppi/HostConstant.hpp
         ${PROJECT_SOURCE_DIR}/include/cppi/HostEnumInfo.hpp
         ${PROJECT_SOURCE_DIR}/include/cppi/HostFunction.hpp
         ${PROJECT_SOURCE_DIR}/include/cppi/HostFunctionInfo.hpp
         ${PROJECT_SOURCE_DIR}/include/cppi/HostRegistry.hpp
         ${PROJECT_SOURCE_DIR}/include/cppi/Interpreter.hpp
         ${PROJECT_SOURCE_DIR}/include/cppi/LineOperations.hpp
         ${PROJECT_SOURCE_DIR}/include/cppi/Options.hpp
         ${PROJECT_SOURCE_DIR}/include/cppi/Program.hpp
         ${PROJECT_SOURCE_DIR}/include/cppi/ProgramCache.hpp
         ${PROJECT_SOURCE_DIR}/include/cppi/RunOptions.hpp
         ${PROJECT_SOURCE_DIR}/include/cppi/RunResult.hpp
         ${PROJECT_SOURCE_DIR}/include/cppi/RunStatus.hpp
         ${PROJECT_SOURCE_DIR}/include/cppi/Severity.hpp
         ${PROJECT_SOURCE_DIR}/include/cppi/SourceLocation.hpp
         ${PROJECT_SOURCE_DIR}/include/cppi/SourceRange.hpp
         ${PROJECT_SOURCE_DIR}/include/cppi/StackFrame.hpp
         ${PROJECT_SOURCE_DIR}/include/cppi/Standard.hpp
         ${PROJECT_SOURCE_DIR}/include/cppi/TypeId.hpp
         ${PROJECT_SOURCE_DIR}/include/cppi/Value.hpp
         ${PROJECT_SOURCE_DIR}/include/cppi/Variable.hpp
         ${PROJECT_BINARY_DIR}/include/cppi/Version.hpp)

target_include_directories(cppi PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# std::from_chars / std::to_chars for double (libstdc++ 11, libc++ 20, MSVC
# 2019): numbers are read and written without the C++ locale. Older libraries
# get a fallback (see support/NumberFormat.cpp).
include(CheckCXXSourceCompiles)
check_cxx_source_compiles(
  "#include <charconv>
   int main() {
     char text[32] = \"2.5\";
     double value = 0;
     auto read = std::from_chars(text, text + 3, value, std::chars_format::general);
     auto written = std::to_chars(text, text + 32, value, std::chars_format::general, 6);
     return read.ec == std::errc{} && written.ec == std::errc{} ? 0 : 1;
   }"
  CPPI_HAS_FLOAT_CHARCONV)
target_compile_definitions(cppi PRIVATE CPPI_HAS_FLOAT_CHARCONV=$<BOOL:${CPPI_HAS_FLOAT_CHARCONV}>)
target_compile_features(cppi PUBLIC cxx_std_20)
target_link_libraries(cppi PRIVATE tree-sitter-cpp::tree-sitter-cpp tree-sitter::tree-sitter)
set_target_properties(cppi PROPERTIES VERSION ${PROJECT_VERSION} SOVERSION ${PROJECT_VERSION_MAJOR})

# Shared builds export only the public API (CMAKE_CXX_VISIBILITY_PRESET=hidden).
if(BUILD_SHARED_LIBS)
  set_target_properties(cppi PROPERTIES CXX_VISIBILITY_PRESET default)
endif()

cppi_set_warnings(cppi)
