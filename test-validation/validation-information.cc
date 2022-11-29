#include "validation-information.hh"


unsigned int coot::validation_information_t::get_index_for_chain(const std::string &chain_id) {
    for (unsigned int i=0; i<cviv.size(); i++) {
    if (chain_id == cviv[i].chain_id)
        return i;
    }
    chain_validation_information_t cvi(chain_id);
    cviv.push_back(cvi);
    return cviv.size() -1;
}

// void coot::validation_information_t::add_residue_valiation_informtion(
//     const residue_validation_information_t &rvi, 
//     const std::string &chain_id) {

//     unsigned int idx = get_index_for_chain(chain_id);
//     cviv[idx].add_residue_validation_information(rvi);
// }
