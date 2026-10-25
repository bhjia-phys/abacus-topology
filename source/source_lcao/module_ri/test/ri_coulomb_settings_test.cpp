// Focused unit tests for RI_Util::update_coulomb_settings routing.
//
// Background: for rpa=1 + LCAO + a non-hybrid functional (e.g. PBE) with the
// INPUT default exx_singularity_correction=default, input_conv resolves
// "default" to "massidda", which must route the Fock Coulomb parameters to the
// Ewald method so that the full (singularity-corrected) Coulomb kernel is
// produced. This test locks the routing for every supported scheme and the
// clear error raised for unsupported values (which used to be silently dropped,
// leaving an empty Vs_full and a cryptic std::out_of_range in write_Vs_abf).

#include <gtest/gtest.h>

#include "source_cell/klist.h"
#include "source_cell/unitcell.h"
#include "source_lcao/module_ri/RI_Util.h"

#include <map>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{

using CoulombParam = std::map<Conv_Coulomb_Pot_K::Coulomb_Type,
                              std::vector<std::map<std::string, std::string>>>;
using CoulombSettings = std::map<Conv_Coulomb_Pot_K::Coulomb_Method,
                                 std::pair<bool, CoulombParam>>;

CoulombParam make_fock_param(const std::string& singularity_correction)
{
    CoulombParam param;
    param[Conv_Coulomb_Pot_K::Coulomb_Type::Fock] = {
        {{"alpha", "1"}, {"singularity_correction", singularity_correction}}};
    return param;
}

const std::vector<std::map<std::string, std::string>>* get_fock_params(
    const CoulombSettings& settings, const Conv_Coulomb_Pot_K::Coulomb_Method method)
{
    const auto it = settings.find(method);
    if (it == settings.end())
    {
        return nullptr;
    }
    const auto it_fock = it->second.second.find(Conv_Coulomb_Pot_K::Coulomb_Type::Fock);
    if (it_fock == it->second.second.end())
    {
        return nullptr;
    }
    return &it_fock->second;
}

} // namespace

TEST(RIUtilUpdateCoulombSettings, MassiddaRoutesToEwald)
{
    UnitCell ucell;
    K_Vectors kv;
    const CoulombSettings settings
        = RI_Util::update_coulomb_settings(make_fock_param("massidda"), ucell, &kv);

    ASSERT_EQ(settings.size(), 1);
    ASSERT_EQ(settings.count(Conv_Coulomb_Pot_K::Coulomb_Method::Ewald), 1);
    EXPECT_TRUE(settings.at(Conv_Coulomb_Pot_K::Coulomb_Method::Ewald).first);
    const auto* fock = get_fock_params(settings, Conv_Coulomb_Pot_K::Coulomb_Method::Ewald);
    ASSERT_NE(fock, nullptr);
    ASSERT_EQ(fock->size(), 1);
    EXPECT_EQ(fock->at(0).at("singularity_correction"), "massidda");
    EXPECT_EQ(fock->at(0).at("alpha"), "1");
}

TEST(RIUtilUpdateCoulombSettings, CarrierRoutesToEwald)
{
    UnitCell ucell;
    K_Vectors kv;
    const CoulombSettings settings
        = RI_Util::update_coulomb_settings(make_fock_param("carrier"), ucell, &kv);

    ASSERT_EQ(settings.size(), 1);
    ASSERT_EQ(settings.count(Conv_Coulomb_Pot_K::Coulomb_Method::Ewald), 1);
    EXPECT_TRUE(settings.at(Conv_Coulomb_Pot_K::Coulomb_Method::Ewald).first);
    const auto* fock = get_fock_params(settings, Conv_Coulomb_Pot_K::Coulomb_Method::Ewald);
    ASSERT_NE(fock, nullptr);
    ASSERT_EQ(fock->size(), 1);
    EXPECT_EQ(fock->at(0).at("singularity_correction"), "carrier");
}

TEST(RIUtilUpdateCoulombSettings, SpencerRoutesToCenter2)
{
    UnitCell ucell;
    ucell.omega = 100.0;
    K_Vectors kv;
    kv.set_nkstot_full(8);
    const CoulombSettings settings
        = RI_Util::update_coulomb_settings(make_fock_param("spencer"), ucell, &kv);

    ASSERT_EQ(settings.size(), 1);
    ASSERT_EQ(settings.count(Conv_Coulomb_Pot_K::Coulomb_Method::Center2), 1);
    EXPECT_TRUE(settings.at(Conv_Coulomb_Pot_K::Coulomb_Method::Center2).first);
    const auto* fock = get_fock_params(settings, Conv_Coulomb_Pot_K::Coulomb_Method::Center2);
    ASSERT_NE(fock, nullptr);
    ASSERT_EQ(fock->size(), 1);
    EXPECT_EQ(fock->at(0).at("singularity_correction"), "spencer");
    EXPECT_EQ(fock->at(0).count("Rcut"), 1); // update_coulomb_param filled Rcut for spencer
}

TEST(RIUtilUpdateCoulombSettings, RevisedSpencerAndLimitsRouteToCenter2)
{
    UnitCell ucell;
    ucell.omega = 100.0;
    ucell.a1 = ModuleBase::Vector3<double>(10.0, 0.0, 0.0);
    ucell.a2 = ModuleBase::Vector3<double>(0.0, 10.0, 0.0);
    ucell.a3 = ModuleBase::Vector3<double>(0.0, 0.0, 10.0);
    K_Vectors kv;
    kv.set_nkstot_full(8);
    kv.nmp[0] = 2;
    kv.nmp[1] = 2;
    kv.nmp[2] = 2;

    for (const std::string& scheme : {"revised_spencer", "limits"})
    {
        const CoulombSettings settings
            = RI_Util::update_coulomb_settings(make_fock_param(scheme), ucell, &kv);
        ASSERT_EQ(settings.size(), 1) << "scheme=" << scheme;
        ASSERT_EQ(settings.count(Conv_Coulomb_Pot_K::Coulomb_Method::Center2), 1)
            << "scheme=" << scheme;
        EXPECT_TRUE(settings.at(Conv_Coulomb_Pot_K::Coulomb_Method::Center2).first)
            << "scheme=" << scheme;
        const auto* fock = get_fock_params(settings, Conv_Coulomb_Pot_K::Coulomb_Method::Center2);
        ASSERT_NE(fock, nullptr) << "scheme=" << scheme;
        ASSERT_EQ(fock->size(), 1) << "scheme=" << scheme;
        EXPECT_EQ(fock->at(0).at("singularity_correction"), scheme) << "scheme=" << scheme;
    }
}

TEST(RIUtilUpdateCoulombSettings, UnsupportedValueThrows)
{
    UnitCell ucell;
    K_Vectors kv;
    try
    {
        RI_Util::update_coulomb_settings(make_fock_param("bogus"), ucell, &kv);
        FAIL() << "expected std::invalid_argument for unsupported singularity_correction";
    }
    catch (const std::invalid_argument& e)
    {
        const std::string what = e.what();
        EXPECT_NE(what.find("bogus"), std::string::npos);
        EXPECT_NE(what.find("massidda"), std::string::npos);
        EXPECT_NE(what.find("spencer"), std::string::npos);
    }
}

TEST(RIUtilUpdateCoulombSettings, UnresolvedDefaultThrows)
{
    // The INPUT-level value "default" must be resolved by the caller before
    // building RI Coulomb settings: input_conv maps it to "massidda" for
    // rpa=1 non-hybrid runs. Reaching this layer unresolved is a bug and must
    // fail loudly instead of silently producing empty settings.
    UnitCell ucell;
    K_Vectors kv;
    EXPECT_THROW(RI_Util::update_coulomb_settings(make_fock_param("default"), ucell, &kv),
                 std::invalid_argument);
}
