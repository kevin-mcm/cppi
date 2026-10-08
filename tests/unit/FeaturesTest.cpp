/// @file FeaturesTest.cpp
/// @brief Unit tests of language features, standards and FeatureSet.
///
/// @author kevin-mcm <kevincardenasmiranda9@gmail.com>
/// @date 2026-10-08

#include <gtest/gtest.h>

#include <cppi/FeatureSet.hpp>

using cppi::Feature;
using cppi::FeatureSet;
using cppi::Standard;

TEST(Features, FeaturesKnowTheStandardThatIntroducedThem) {
    EXPECT_EQ(cppi::introduced_in(Feature::Loops), Standard::Cpp98);
    EXPECT_EQ(cppi::introduced_in(Feature::Inheritance), Standard::Cpp98);
    EXPECT_EQ(cppi::introduced_in(Feature::Lambdas), Standard::Cpp11);
    EXPECT_EQ(cppi::introduced_in(Feature::BinaryLiterals), Standard::Cpp14);
    EXPECT_EQ(cppi::introduced_in(Feature::StructuredBindings), Standard::Cpp17);
    EXPECT_EQ(cppi::introduced_in(Feature::Coroutines), Standard::Cpp20);
}

TEST(Features, FeatureKeysRoundTrip) {
    for (std::size_t i = 0; i < cppi::kFeatureCount; ++i) {
        const auto feature = static_cast<Feature>(i);
        const auto key = cppi::feature_key(feature);
        SCOPED_TRACE(::testing::Message() << "feature key: " << key);
        EXPECT_FALSE(key.empty());
        ASSERT_TRUE(cppi::parse_feature(key).has_value());
        EXPECT_EQ(*cppi::parse_feature(key), feature);
    }
    EXPECT_FALSE(cppi::parse_feature("teleportation").has_value());
}

TEST(Features, StandardsParseFromCommonSpellings) {
    EXPECT_EQ(cppi::parse_standard("c++98"), Standard::Cpp98);
    EXPECT_EQ(cppi::parse_standard("C++03"), Standard::Cpp98);
    EXPECT_EQ(cppi::parse_standard("cpp17"), Standard::Cpp17);
    EXPECT_EQ(cppi::parse_standard("20"), Standard::Cpp20);
    EXPECT_FALSE(cppi::parse_standard("c++42").has_value());
    EXPECT_EQ(cppi::to_string(Standard::Cpp23), "C++23");
}

TEST(Features, FeatureSetBehavesLikeASet) {
    FeatureSet set;
    EXPECT_TRUE(set.empty());
    set.add(Feature::Loops).add(Feature::Variables);
    EXPECT_TRUE(set.contains(Feature::Loops));
    EXPECT_EQ(set.size(), 2);
    set.remove(Feature::Loops);
    EXPECT_FALSE(set.contains(Feature::Loops));

    const auto cpp98 = FeatureSet::all_in(Standard::Cpp98);
    EXPECT_TRUE(cpp98.contains(Feature::Templates));
    EXPECT_FALSE(cpp98.contains(Feature::Lambdas));
    EXPECT_EQ(FeatureSet::all_in(Standard::Cpp26).size(), cppi::kFeatureCount);
}
