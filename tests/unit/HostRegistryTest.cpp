#include <cppi/HostRegistry.hpp>

#include <gtest/gtest.h>

#include <stdexcept>

using cppi::HostCall;
using cppi::HostRegistry;
using cppi::Value;

namespace {
Value nothing(HostCall&) {
    return Value::void_value();
}
}  // namespace

TEST(HostRegistry, FunctionsAreRegisteredWithAFluentBuilder) {
    HostRegistry host;
    const auto dir = host.add_enum("Direction", {"North", "East"});
    const auto id = host.function("move").param(dir).cost(5).bind(nothing);

    ASSERT_EQ(host.find_function("move"), id);
    const auto& fn = host.function(id);
    EXPECT_EQ(fn.name, "move");
    EXPECT_EQ(fn.params.size(), 1);
    EXPECT_EQ(fn.params[0], dir);
    EXPECT_EQ(fn.result, cppi::types::Void);
    EXPECT_EQ(fn.cost, 5);
    EXPECT_FALSE(host.find_function("teleport").has_value());
}

TEST(HostRegistry, EnumeratorsBecomeConstants) {
    HostRegistry host;
    const auto dir = host.add_enum("Direction", {"North", "East", "South"});
    auto south = host.find_constant("South");
    ASSERT_TRUE(south.has_value());
    EXPECT_EQ(south->type, dir);
    EXPECT_EQ(south->value, 2);
    EXPECT_EQ(host.type_name(dir), "Direction");
    EXPECT_EQ(host.enumerator_name(dir, 1), "East");
    EXPECT_TRUE(host.enumerator_name(dir, 9).empty());
    EXPECT_NE(host.find_enum("Direction"), nullptr);
}

TEST(HostRegistry, RegistrationRejectsInvalidOrDuplicateNames) {
    HostRegistry host;
    host.add_enum("Direction", {"North"});
    host.function("move").bind(nothing);

    EXPECT_THROW((void)host.function("move"), std::invalid_argument);
    EXPECT_THROW((void)host.function("North"), std::invalid_argument);
    EXPECT_THROW((void)host.function("while"), std::invalid_argument);
    EXPECT_THROW((void)host.function("2fast"), std::invalid_argument);
    EXPECT_THROW((void)host.function("__secret"), std::invalid_argument);
    EXPECT_THROW((void)host.add_enum("Crop", {"Grass", "Grass"}), std::invalid_argument);
    EXPECT_THROW((void)host.add_enum("Empty", std::vector<std::string>{}), std::invalid_argument);
    EXPECT_THROW((void)host.function("noop").bind(nullptr), std::invalid_argument);
    EXPECT_THROW((void)host.function("bad").param(cppi::types::Void), std::invalid_argument);
    EXPECT_THROW((void)host.function("bad2").param(cppi::TypeId{999}), std::invalid_argument);
}

TEST(HostRegistry, BuiltinTypeNames) {
    HostRegistry host;
    EXPECT_EQ(host.type_name(cppi::types::Int), "int");
    EXPECT_EQ(host.type_name(cppi::types::Bool), "bool");
    EXPECT_EQ(host.type_name(cppi::types::Void), "void");
    EXPECT_TRUE(host.is_known_type(cppi::types::Int));
    EXPECT_FALSE(host.is_known_type(cppi::TypeId{999}));
}
