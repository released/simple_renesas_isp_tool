#pragma once
#include "image_vectors.h"

namespace firmware_upgrade::firmware_update {
// Retain the serial contract's strict known-ABI acceptance.
inline bool ValidateSerialVectors(const std::vector<std::uint8_t>& image,
    const ImageInfo& info,std::wstring* error) {
    return ValidateImageVectors(image,info,error,true);
}
}
