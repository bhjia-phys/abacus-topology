#include "source_lcao/module_ri/hartree_reference.h"
#include "source_basis/module_ao/ORB_atomic_lm.h"
#include "source_base/constants.h"
#include <gtest/gtest.h>
#include <cmath>
#include <vector>

TEST(HartreeReference, UnitGaussianFullFourierNormalization)
{
    const double beta = 0.8;
    const double dr = 0.01;
    const int nr = 1001;
    std::vector<double> radial(nr), rab(nr, dr), density(nr);
    for (int i = 0; i < nr; ++i)
    {
        radial[i] = i * dr;
        density[i] = std::sqrt(ModuleBase::FOUR_PI) * std::pow(beta / ModuleBase::PI, 1.5)
                     * std::exp(-beta * radial[i] * radial[i]);
    }
    Numerical_Orbital_Lm gaussian;
    gaussian.set_orbital_info("unit Gaussian charge", 0, 0, 0, nr, rab.data(), radial.data(),
                              Numerical_Orbital_Lm::Psi_Type::Psi, density.data(),
                              801, 0.025, dr, false, true, false);
    const double y00 = 1.0 / std::sqrt(ModuleBase::FOUR_PI);
    for (const double g : {0.0, 0.3, 1.0, 3.0})
    {
        const double actual = HartreeReference::radial_fourier(gaussian, g) * y00;
        EXPECT_NEAR(actual, std::exp(-g * g / (4 * beta)), 2e-10);
    }
}

TEST(HartreeReference, GaussianPHasCorrectRadialPower)
{
    const double beta = 0.8;
    const double dr = 0.01;
    const int nr = 1001;
    std::vector<double> radial(nr), rab(nr, dr), density(nr);
    for (int i = 0; i < nr; ++i)
    {
        radial[i] = i * dr;
        density[i] = radial[i] * std::exp(-beta * radial[i] * radial[i]);
    }
    Numerical_Orbital_Lm gaussian;
    gaussian.set_orbital_info("Gaussian p", 0, 1, 0, nr, rab.data(), radial.data(),
                              Numerical_Orbital_Lm::Psi_Type::Psi, density.data(),
                              801, 0.025, dr, false, true, false);
    for (const double g : {0.0, 0.3, 1.0, 3.0})
    {
        const double exact = std::pow(ModuleBase::PI, 1.5) * g / (2 * std::pow(beta, 2.5))
                             * std::exp(-g * g / (4 * beta));
        EXPECT_NEAR(HartreeReference::radial_fourier(gaussian, g), exact, 2e-9);
    }
}
