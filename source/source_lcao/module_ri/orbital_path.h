#ifndef ORBITAL_PATH_H
#define ORBITAL_PATH_H

#include <string>
#include <vector>

namespace ExxAbfsPath
{
inline std::vector<std::string> resolve_orbital_paths(
    const std::vector<std::string>& filenames,
    const std::string& orbital_dir)
{
    std::vector<std::string> paths;
    paths.reserve(filenames.size());
    for (const auto& filename : filenames)
    {
        if (!filename.empty() && filename.front() == '/')
        {
            paths.push_back(filename);
        }
        else
        {
            paths.push_back(orbital_dir + filename);
        }
    }
    return paths;
}
}

#endif
