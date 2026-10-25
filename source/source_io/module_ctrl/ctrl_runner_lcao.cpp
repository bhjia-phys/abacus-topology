#include "ctrl_runner_lcao.h" // use ctrl_runner_lcao() 

#include "source_estate/elecstate_lcao.h" // use elecstate::ElecState
#include "source_lcao/hamilt_lcao.h" // use hamilt::HamiltLCAO<TK, TR>
#include "source_hamilt/module_xc/exx_info.h"

#include "../module_energy/write_proj_band_lcao.h" // projcted band structure
#include "../module_dos/cal_ldos.h" // cal LDOS
#include "../module_energy/write_eband_terms.hpp"
#include "../module_hs/write_vxc.hpp"
#include "../module_hs/write_vxc_r.hpp"
#include "../module_hs/reference_exx_shape.h"
#include "source_base/tool_quit.h"
#ifdef __EXX
#include "source_lcao/module_ri/Exx_LRI_interface.h"
#endif

namespace ModuleIO
{

template <typename TK, typename TR>
void ctrl_runner_lcao(UnitCell& ucell,      // unitcell
        const Input_para &inp,              // input
		K_Vectors &kv,                      // k-point
		elecstate::ElecState* pelec,// electronic info
        const LCAO_domain::Setup_DM<TK> &dmat, // mohan add 2025-11-02
		Parallel_Orbitals &pv,              // orbital info
        Parallel_Grid &pgrid,               // grid info
		Grid_Driver &gd,                    // search for adjacent atoms
		psi::Psi<TK>* psi,                  // wave function
        Charge &chr,                  // charge density
		hamilt::HamiltLCAO<TK, TR>* p_hamilt, // hamiltonian
		TwoCenterBundle &two_center_bundle,   // use two-center integration
		LCAO_Orbitals &orb,                 // LCAO orbitals
		ModulePW::PW_Basis* pw_rho,   // charge density
		ModulePW::PW_Basis* pw_rhod,  // dense charge density 
		Structure_Factor &sf,         // structure factor
        ModuleBase::matrix &vloc,     // local pseudopotential 
		Exx_NAO<TK> &exx_nao,
        surchem &solvent)             // solvent model
{
    ModuleBase::TITLE("ModuleIO", "ctrl_runner_lcao");
    ModuleBase::timer::start("ModuleIO", "ctrl_runner_lcao");

    // 1) write projected band structure
    if (inp.out_proj_band)
    {
        ModuleIO::write_proj_band_lcao(psi, pv, pelec, kv, ucell, p_hamilt);
    }

	// 2) out ldos
	if (inp.out_ldos[0])
    {
        ModuleIO::Cal_ldos<TK>::cal_ldos_lcao(pelec->eferm, chr, dmat, kv, 
          pelec->ekb, pelec->wg, psi[0], pgrid, ucell);
    }

    // Check the requested hybrid reference before either writer can emit a
    // partial local-only Vxc or index an unavailable EXX channel vector.
    const bool write_xc = inp.out_mat_xc || inp.out_mat_xc2[0] || inp.out_eband_terms;
    const bool cal_exx = write_xc && GlobalC::exx_info.info_global.cal_exx;
    const bool real_number = write_xc ? GlobalC::exx_info.info_ri.real_number : false;
    const double hybrid_alpha = write_xc ? GlobalC::exx_info.info_global.hybrid_alpha : 0.0;
#ifdef __EXX
    if (cal_exx)
    {
        std::vector<std::size_t> atom_orbitals(ucell.nat);
        for (int iat = 0; iat < ucell.nat; ++iat)
            atom_orbitals[iat] = ucell.atoms[ucell.iat2it[iat]].nw;
        std::string local_error;
        const bool ready = real_number
            ? reference_exx_shapes_valid_on_all_ranks(
                  exx_nao.exd ? &exx_nao.exd->get_Hexxs() : nullptr,
                  inp.nspin, atom_orbitals, local_error
#ifdef __MPI
                  , MPI_COMM_WORLD
#endif
              )
            : reference_exx_shapes_valid_on_all_ranks(
                  exx_nao.exc ? &exx_nao.exc->get_Hexxs() : nullptr,
                  inp.nspin, atom_orbitals, local_error
#ifdef __MPI
                  , MPI_COMM_WORLD
#endif
              );
        if (!ready)
        {
            const std::string reason = local_error.empty()
                ? "Another MPI rank reported an unavailable or invalid reference EXX."
                : local_error;
            ModuleBase::WARNING_QUIT("ctrl_runner_lcao",
                "Cannot write complete hybrid Vxc: " + reason
                + " Complete the hybrid SCF/reference EXX calculation before requesting Vxc output.");
        }
    }
#endif

    // 3) print out exchange-correlation potential
    if (inp.out_mat_xc)
    {
        ModuleIO::write_Vxc<TK, TR>(inp.nspin,
                                    PARAM.globalv.nlocal,
                                    GlobalV::DRANK,
                                    &pv,
                                    *psi,
                                    ucell,
                                    sf,
                                    solvent,
                                    *pw_rho,
                                    *pw_rhod,
                                    vloc,
                                    chr,
                                    kv,
                                    orb.cutoffs(),
                                    pelec->wg,
                                    gd,
                                    cal_exx
#ifdef __EXX
                                    ,
                                    exx_nao.exd ? &exx_nao.exd->get_Hexxs() : nullptr,
                                    exx_nao.exc ? &exx_nao.exc->get_Hexxs() : nullptr
#endif
        );
    }

    if (inp.out_mat_xc2[0])
    {
        ModuleIO::write_Vxc_R<TK, TR>(inp.nspin,
                                      &pv,
                                      ucell,
                                      sf,
                                      solvent,
                                      *pw_rho,
                                      *pw_rhod,
                                      vloc,
                                      chr,
                                      kv,
                                      orb.cutoffs(),
                                      gd,
                                      cal_exx,
                                      hybrid_alpha,
                                      real_number
#ifdef __EXX
                                      ,
                                      exx_nao.exd ? &exx_nao.exd->get_Hexxs() : nullptr,
                                      exx_nao.exc ? &exx_nao.exc->get_Hexxs() : nullptr
#endif
        );
    }


    // write eband terms
    if (inp.out_eband_terms)
    {
        ModuleIO::write_eband_terms<TK, TR>(inp.nspin,
                                            PARAM.globalv.nlocal,
                                            GlobalV::DRANK,
                                            &pv,
                                            *psi,
                                            ucell,
                                            sf,
                                            solvent,
                                            *pw_rho,
                                            *pw_rhod,
                                            vloc,
                                            chr,
                                            kv,
                                            pelec->wg,
                                            gd,
                                            orb.cutoffs(),
                                            two_center_bundle
#ifdef __EXX
                                            ,
                                            exx_nao.exd ? &exx_nao.exd->get_Hexxs() : nullptr,
                                            exx_nao.exc ? &exx_nao.exc->get_Hexxs() : nullptr
#endif
       );
    }

    ModuleBase::timer::end("ModuleIO", "ctrl_runner_lcao");
}




// TK: double  TR: double 
template void ctrl_runner_lcao<double, double>(UnitCell& ucell,      // unitcell
        const Input_para &inp,              // input
		K_Vectors &kv,                      // k-point
		elecstate::ElecState* pelec,// electronic info
        const LCAO_domain::Setup_DM<double> &dmat, // mohan add 2025-11-02
		Parallel_Orbitals &pv,              // orbital info
        Parallel_Grid &pgrid,               // grid info
		Grid_Driver &gd,                    // search for adjacent atoms
		psi::Psi<double>* psi,                  // wave function
        Charge &chr,                  // charge density
		hamilt::HamiltLCAO<double, double>* p_hamilt, // hamiltonian
		TwoCenterBundle &two_center_bundle,   // use two-center integration
		LCAO_Orbitals &orb,                 // LCAO orbitals
		ModulePW::PW_Basis* pw_rho,   // charge density
		ModulePW::PW_Basis* pw_rhod,  // dense charge density 
		Structure_Factor &sf,         // structure factor
        ModuleBase::matrix &vloc,     // local pseudopotential 
        Exx_NAO<double> &exx_nao,
        surchem &solvent);             // solvent model

// TK: complex<double>  TR: double 
template void ctrl_runner_lcao<std::complex<double>, double>(UnitCell& ucell,      // unitcell
        const Input_para &inp,              // input
		K_Vectors &kv,                      // k-point
		elecstate::ElecState* pelec,// electronic info
        const LCAO_domain::Setup_DM<std::complex<double>> &dmat, // mohan add 2025-11-02
		Parallel_Orbitals &pv,              // orbital info
        Parallel_Grid &pgrid,               // grid info
		Grid_Driver &gd,                    // search for adjacent atoms
		psi::Psi<std::complex<double>>* psi,                  // wave function
        Charge &chr,                  // charge density
		hamilt::HamiltLCAO<std::complex<double>, double>* p_hamilt, // hamiltonian
		TwoCenterBundle &two_center_bundle,   // use two-center integration
		LCAO_Orbitals &orb,                 // LCAO orbitals
		ModulePW::PW_Basis* pw_rho,   // charge density
		ModulePW::PW_Basis* pw_rhod,  // dense charge density 
		Structure_Factor &sf,         // structure factor
        ModuleBase::matrix &vloc,     // local pseudopotential 
        Exx_NAO<std::complex<double>> &exx_nao,
        surchem &solvent);             // solvent model

// TK: complex<double>  TR: complex<double>
template void ctrl_runner_lcao<std::complex<double>, std::complex<double>>(UnitCell& ucell,      // unitcell
        const Input_para &inp,              // input
		K_Vectors &kv,                      // k-point
		elecstate::ElecState* pelec,// electronic info
        const LCAO_domain::Setup_DM<std::complex<double>> &dmat, // mohan add 2025-11-02
		Parallel_Orbitals &pv,              // orbital info
        Parallel_Grid &pgrid,               // grid info
		Grid_Driver &gd,                    // search for adjacent atoms
		psi::Psi<std::complex<double>>* psi,                  // wave function
        Charge &chr,                  // charge density
		hamilt::HamiltLCAO<std::complex<double>, std::complex<double>>* p_hamilt, // hamiltonian
		TwoCenterBundle &two_center_bundle,   // use two-center integration
		LCAO_Orbitals &orb,                 // LCAO orbitals
		ModulePW::PW_Basis* pw_rho,   // charge density
		ModulePW::PW_Basis* pw_rhod,  // dense charge density 
		Structure_Factor &sf,         // structure factor
        ModuleBase::matrix &vloc,     // local pseudopotential 
        Exx_NAO<std::complex<double>> &exx_nao,
        surchem &solvent);             // solvent model

} // end namespace
