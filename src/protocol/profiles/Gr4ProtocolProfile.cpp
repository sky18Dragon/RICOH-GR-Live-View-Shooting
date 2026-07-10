#include "Gr4ProtocolProfile.h"

namespace rvf {
namespace {

constexpr CameraProtocolProfile kGr4ProtocolProfile = {
    CameraModel::RicohGr4Hdf,
    "RICOH_GR_IV_HDF",
    {
        0x0135,
        0x0138,
        0x013A,
        0x013C,
        0x013E,
        0x0000,
        0x0140,
        0x00EB,
        0x00EC,
    },
    {
        "9A5ED1C5-74CC-4C50-B5B6-66A48E7CCFF1",
        "4B445988-CAA0-4DD3-941D-37B4F52ACA86",
        "1452335A-EC7F-4877-B8AB-0F72E18BB295",
        "9F00F387-8345-4BBC-8B92-B87B52E3091A",
        "B29E6DE3-1AEC-48C1-9D05-02CEA57CE664",
        "559644B8-E0BC-4011-929B-5CF9199851E7",
        "0F291746-0C80-4726-87A7-3C501FD3B4B6",
    },
    {
        true,
        true,
        false,
        true,
        true,
        true,
        true,
    },
    BlePairingMode::ExistingDefault,
    0x01,
    0x01,
    0x00,
};

}  // namespace

const CameraProtocolProfile& gr4ProtocolProfile() {
    return kGr4ProtocolProfile;
}

}  // namespace rvf
