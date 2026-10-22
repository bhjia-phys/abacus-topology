#include "source_lcao/module_ri/conv_coulomb_pot_k.h"
#include "source_lcao/center2_orb-orb11.h"
#include "source_base/constants.h"

#include <gtest/gtest.h>
#include <cmath>
#include <set>

TEST(LocalHseNativeKernel, GaussianChargeSelfInteractionHasHartreeUnits)
{
    // rho=(beta/pi)^(3/2) exp(-beta*r^2) has unit total charge. Orb11
    // is the same native overlap operation used by LRI_CV::DPcal_V.
    const double beta = 0.8;
    const double omega = 0.11;
    const double dr = 0.01;
    const double dk = 0.025;
    const int nr = 1001;
    const int nk = 801;
    std::vector<double> radial(nr), rab(nr, dr), density(nr);
    for (int i = 0; i < nr; ++i)
    {
        radial[i] = i * dr;
        density[i] = std::sqrt(ModuleBase::FOUR_PI) * std::pow(beta / ModuleBase::PI, 1.5)
                     * std::exp(-beta * radial[i] * radial[i]);
    }
    Numerical_Orbital_Lm charge;
    charge.set_orbital_info("unit Gaussian charge", 0, 0, 0, nr, rab.data(), radial.data(),
                            Numerical_Orbital_Lm::Psi_Type::Psi, density.data(),
                            nk, dk, dr, false, true, false);
    const CoulombParam kernel{{Conv_Coulomb_Pot_K::Coulomb_Type::Erfc,
                              {{{"alpha", "1"}, {"omega", "0.11"},
                                 {"singularity_correction", "limits"}}}}};
    const auto potential = Conv_Coulomb_Pot_K::cal_orbs_ccp(charge, kernel, 1.0);
    ORB_gaunt_table gaunt;
    gaunt.init_Gaunt_CH(0);
    gaunt.init_Gaunt(0);
    ModuleBase::Sph_Bessel_Recursive::D2* bessel = nullptr;
    Center2_Orb::init_Table_Spherical_Bessel(0, dr, dk, nk,
        Center2_Orb::get_rmesh(charge.getRcut(), potential.getRcut(), dr), bessel);
    Center2_Orb::Orb11 metric(potential, charge, bessel, gaunt);
    metric.init_radial_table(std::set<std::size_t>{0});
    const double actual = metric.cal_overlap({0, 0, 0}, {0, 0, 0}, 0, 0);
    const double exact_hartree = std::sqrt(2 * beta / ModuleBase::PI)
        * (1 - 1 / std::sqrt(1 + beta / (2 * omega * omega)));
    EXPECT_NEAR(actual, exact_hartree, 2e-7);
    // A missing Hartree/Ry conversion would give exactly twice this value.
    EXPECT_GT(std::abs(actual - 2 * exact_hartree), 0.1);
}
