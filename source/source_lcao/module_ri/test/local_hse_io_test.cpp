#include "source_lcao/module_ri/local_hse.h"
#include "source_lcao/module_ri/libri_completion.h"
#include "source_hamilt/module_xc/exx_info_ri.h"

#include <gtest/gtest.h>
#include <cmath>
#include <sstream>
#include <stdexcept>

namespace
{
RI::Tensor<std::complex<double>> two_orbital_tensor()
{
    RI::Tensor<std::complex<double>> tensor({2, 2, 2, 2});
    tensor(0, 0, 0, 0) = 8;
    tensor(1, 1, 1, 1) = 6;
    tensor(0, 0, 1, 1) = 2;
    tensor(1, 1, 0, 0) = 2;
    tensor(0, 1, 1, 0) = 0.5;
    tensor(1, 0, 0, 1) = 0.5;
    return tensor;
}
} // namespace

TEST(LocalHseDependencyAdapter, SupportsBothLibriCoefficientFlagNames)
{
    struct OldFlags { bool C = false; } old_flags;
    struct NewFlags { bool Cs = false; } new_flags;
    ExxLriDetail::mark_coefficients_ready(old_flags, 0);
    ExxLriDetail::mark_coefficients_ready(new_flags, 0);
    EXPECT_TRUE(old_flags.C);
    EXPECT_TRUE(new_flags.Cs);
}

TEST(LocalHseProjectors, ReadsComplexFiniteSupportAndComments)
{
    std::istringstream input(
        "# Final orbitals\nABACUS_LOCAL_HSE_PROJECTORS 1\n"
        "n_orbitals 2\nn_entries 3\n"
        "0 1 -2 0 1 3 0.5 0.25 # tail\n"
        "1 0 0 0 0 0 0 1\n"
        "0 2 1 0 0 1 0.4 -0.3\n");
    const auto frame = LocalHse::read_projectors(input);
    ASSERT_EQ(frame.n_orbitals, 2);
    ASSERT_EQ(frame.entries.size(), 3);
    EXPECT_EQ(frame.entries[0].cell[0], -2);
    EXPECT_EQ(frame.entries[0].ao, 3);
    EXPECT_EQ(frame.entries[0].coefficient, std::complex<double>(0.5, 0.25));
    EXPECT_EQ(frame.entries[1].coefficient, std::complex<double>(0, 1));
}

TEST(LocalHseProjectors, RejectsDuplicatesMissingSupportAndTrailingData)
{
    for (const auto& text: {
        "ABACUS_LOCAL_HSE_PROJECTORS 1 n_orbitals 1 n_entries 2 "
        "0 0 0 0 0 0 1 0 0 0 0 0 0 0 1 0",
        "ABACUS_LOCAL_HSE_PROJECTORS 1 n_orbitals 2 n_entries 1 0 0 0 0 0 0 1 0",
        "ABACUS_LOCAL_HSE_PROJECTORS 1 n_orbitals 1 n_entries 1 0 0 0 0 0 0 1 0 extra",
        "ABACUS_LOCAL_HSE_PROJECTORS 2 n_orbitals 1 n_entries 1 0 0 0 0 0 0 1 0",
        "ABACUS_LOCAL_HSE_PROJECTORS 1 n_orbitals 1 n_entries 1 1 0 0 0 0 0 1 0",
        "ABACUS_LOCAL_HSE_PROJECTORS 1 n_orbitals 1 n_entries 1 0 0 0 0 0 0 0 0"})
    {
        std::istringstream input(text);
        EXPECT_THROW(LocalHse::read_projectors(input), std::invalid_argument);
    }
}

TEST(LocalHseProjectors, ChecksPhysicalOverlapWithoutChangingTheFrame)
{
    // Two Euclidean-unit coefficient columns on a nonorthogonal NAO pair
    // have this Gram matrix, and must not pass as orthonormal final orbitals.
    RI::Tensor<std::complex<double>> gram({2, 2});
    gram(0, 0) = gram(1, 1) = 1;
    gram(0, 1) = gram(1, 0) = 0.2;
    EXPECT_THROW(LocalHse::validate_overlap(gram, 1e-6), std::invalid_argument);
    EXPECT_DOUBLE_EQ(gram(0, 1).real(), 0.2);
    gram(0, 1) = gram(1, 0) = 0;
    EXPECT_DOUBLE_EQ(LocalHse::validate_overlap(gram, 1e-6), 0);
}

TEST(LocalHseOutput, UsesAllDirectAndOnlyOffDiagonalExchangeEntries)
{
    const auto tensor = two_orbital_tensor();
    const auto quarter = LocalHse::summarize(tensor, 0.25, 0);
    const auto half = LocalHse::summarize(tensor, 0.5, 0);
    EXPECT_TRUE(quarter.has_averages);
    EXPECT_DOUBLE_EQ(quarter.ubar, 4.5);
    EXPECT_DOUBLE_EQ(quarter.jbar, 0.5);
    EXPECT_DOUBLE_EQ(quarter.c_hse, 1);
    EXPECT_DOUBLE_EQ(half.c_hse, 2);
    EXPECT_EQ(quarter.direct(0, 0), half.direct(0, 0));
    EXPECT_EQ(quarter.exchange(0, 1), half.exchange(0, 1));
}

TEST(LocalHseOutput, OmitsUndefinedOneOrbitalScalars)
{
    RI::Tensor<std::complex<double>> tensor({1, 1, 1, 1});
    tensor(0, 0, 0, 0) = 3;
    const auto result = LocalHse::summarize(tensor, 0.25, 0);
    EXPECT_FALSE(result.has_averages);
    EXPECT_DOUBLE_EQ(result.direct(0, 0).real(), 3);
    EXPECT_DOUBLE_EQ(result.exchange(0, 0).real(), 3);
    std::istringstream input("ABACUS_LOCAL_HSE_PROJECTORS 1 n_orbitals 1 n_entries 1 "
                             "0 0 0 0 0 0 1 0");
    const auto projectors = LocalHse::read_projectors(input);
    std::ostringstream output;
    Exx_Info_RI info;
    LocalHse::write_result(output, "final projectors.dat", projectors, result, 0.11, 0.25, info);
    EXPECT_NE(output.str().find("energy_unit Hartree"), std::string::npos);
    EXPECT_NE(output.str().find("averages undefined_for_one_orbital"), std::string::npos);
    EXPECT_EQ(output.str().find("\nC_HSE "), std::string::npos);
    EXPECT_EQ(output.str().find("\nJbar_SR "), std::string::npos);
    EXPECT_NE(output.str().find("\nPROJECTORS\n"), std::string::npos);
}

TEST(LocalHseOutput, RejectsNonRealPhysicalDirectExchange)
{
    auto tensor = two_orbital_tensor();
    tensor(0, 0, 1, 1) = {2, 0.01};
    EXPECT_THROW(LocalHse::summarize(tensor, 0.25, 0), std::invalid_argument);
}
