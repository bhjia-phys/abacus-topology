#ifndef ABACUS_LIBRI_COMPLETION_H
#define ABACUS_LIBRI_COMPLETION_H

namespace ExxLriDetail
{
// LibRI renamed this completion field. Detect the actual API rather than
// tying source compatibility to an unrelated optional feature/version macro.
template<typename Flags>
auto mark_coefficients_ready(Flags& flags, int) -> decltype(flags.Cs = true, void())
{
    flags.Cs = true;
}

template<typename Flags>
auto mark_coefficients_ready(Flags& flags, long) -> decltype(flags.C = true, void())
{
    flags.C = true;
}
} // namespace ExxLriDetail

#endif
