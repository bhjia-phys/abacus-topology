// Regression tests for the RPA singularity-correction resolution used by
// Input_Conv::Convert.
//
// Background: for rpa=1 + LCAO + a non-hybrid functional (e.g. PBE) the
// RPA fallback builds the Fock Coulomb parameter from
// Input_Conv::resolve_rpa_singularity_correction. The INPUT-level value
// "default" is only resolved to spencer/limits for hybrid functionals in
// read_input, so for the RPA fallback it must resolve to "massidda" to route
// the full Coulomb production through the Ewald method (matching producers
// that explicitly set exx_singularity_correction=massidda). Explicit values
// must pass through unchanged; unsupported values are deliberately not
// remapped here and are rejected later by RI_Util::update_coulomb_settings.

#include "source_io/module_parameter/input_conv.h"
#include <gtest/gtest.h>
#include <string>

TEST(InputConvRpaSingularityCorrection, DefaultResolvesToMassidda)
{
    // The INPUT default (also the value left untouched by read_input for
    // non-hybrid functionals) must map to massidda: this is the root fix for
    // the empty-Vs_full / std::out_of_range crash in RPA runs.
    EXPECT_EQ(Input_Conv::resolve_rpa_singularity_correction("default"), "massidda");
}

TEST(InputConvRpaSingularityCorrection, ExplicitValuesPassThrough)
{
    // Explicit exx_singularity_correction values are preserved verbatim.
    EXPECT_EQ(Input_Conv::resolve_rpa_singularity_correction("massidda"), "massidda");
    EXPECT_EQ(Input_Conv::resolve_rpa_singularity_correction("carrier"), "carrier");
    EXPECT_EQ(Input_Conv::resolve_rpa_singularity_correction("spencer"), "spencer");
    EXPECT_EQ(Input_Conv::resolve_rpa_singularity_correction("revised_spencer"), "revised_spencer");
    EXPECT_EQ(Input_Conv::resolve_rpa_singularity_correction("limits"), "limits");
}

TEST(InputConvRpaSingularityCorrection, UnsupportedValuesPassThroughForLaterValidation)
{
    // Unknown values are not silently remapped: the descriptive
    // std::invalid_argument from RI_Util::update_coulomb_settings must keep
    // firing so misconfigurations are reported instead of silently producing
    // an empty Vs_full.
    EXPECT_EQ(Input_Conv::resolve_rpa_singularity_correction("bogus"), "bogus");
    EXPECT_EQ(Input_Conv::resolve_rpa_singularity_correction(""), "");
}
