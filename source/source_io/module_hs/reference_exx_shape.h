#ifndef ABACUS_REFERENCE_EXX_SHAPE_H
#define ABACUS_REFERENCE_EXX_SHAPE_H

#include <cstddef>
#include <string>
#include <vector>
#ifdef __MPI
#include <mpi.h>
#endif

namespace ModuleIO
{
// A completed EXX calculation supplies one map per spin channel, even on
// ranks with no local tensor blocks. An empty map is therefore valid, while
// a missing pointer/channel vector is not a computed zero Fock operator.
template<typename HexxContainer>
std::string reference_exx_shape_error(const HexxContainer* const hexx,
                                      const int nspin,
                                      const std::vector<std::size_t>& atom_orbitals)
{
    if (hexx == nullptr)
        return "The selected reference EXX object is unavailable.";
    if (nspin != 1 && nspin != 2 && nspin != 4)
        return "Unsupported reference EXX spin-channel count.";
    if (hexx->size() != static_cast<std::size_t>(nspin))
        return "Expected " + std::to_string(nspin) + " reference EXX spin channels, found "
               + std::to_string(hexx->size()) + ".";
    for (const auto& channel : *hexx)
        for (const auto& first : channel)
        {
            const int i = first.first;
            if (i < 0 || static_cast<std::size_t>(i) >= atom_orbitals.size())
                return "Reference EXX contains an invalid row atom index.";
            for (const auto& second : first.second)
            {
                const int j = second.first.first;
                if (j < 0 || static_cast<std::size_t>(j) >= atom_orbitals.size())
                    return "Reference EXX contains an invalid column atom index.";
                const auto& tensor = second.second;
                if (tensor.shape.size() != 2 || tensor.shape[0] != atom_orbitals[i]
                    || tensor.shape[1] != atom_orbitals[j])
                    return "Reference EXX tensor dimensions do not match its spatial AO atom block.";
                const std::size_t count = atom_orbitals[i] * atom_orbitals[j];
                if ((count != 0 && !tensor.data) || (tensor.data && tensor.data->size() != count))
                    return "Reference EXX tensor data do not match its declared dimensions.";
            }
        }
    return std::string();
}

template<typename HexxContainer>
bool reference_exx_shapes_valid_on_all_ranks(const HexxContainer* const hexx,
                                            const int nspin,
                                            const std::vector<std::size_t>& atom_orbitals,
                                            std::string& local_error
#ifdef __MPI
                                            , MPI_Comm comm
#endif
)
{
    local_error = reference_exx_shape_error(hexx, nspin, atom_orbitals);
    const int local_bad = local_error.empty() ? 0 : 1;
    int any_bad = local_bad;
#ifdef __MPI
    MPI_Allreduce(&local_bad, &any_bad, 1, MPI_INT, MPI_MAX, comm);
#endif
    return any_bad == 0;
}
} // namespace ModuleIO
#endif
