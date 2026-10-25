#include "source_lcao/module_ri/orbital_path.h"

#include "gtest/gtest.h"

TEST(OrbitalPath, ResolvesRelativePathsAgainstOrbitalDirectory)
{
    const std::vector<std::string> filenames = {
        "Bi_4s4p3d2f2g_1e-4.abfs",
        "/absolute/Se_4s4p3d2f2g_1e-4.abfs"};

    const std::vector<std::string> paths =
        ExxAbfsPath::resolve_orbital_paths(filenames, "/data/orbitals/");

    ASSERT_EQ(paths.size(), 2u);
    EXPECT_EQ(paths[0], "/data/orbitals/Bi_4s4p3d2f2g_1e-4.abfs");
    EXPECT_EQ(paths[1], "/absolute/Se_4s4p3d2f2g_1e-4.abfs");
}

TEST(OrbitalPath, PreservesEmptyFilenameWithDirectoryPrefix)
{
    const std::vector<std::string> paths =
        ExxAbfsPath::resolve_orbital_paths({""}, "./orbital/");

    ASSERT_EQ(paths.size(), 1u);
    EXPECT_EQ(paths[0], "./orbital/");
}
