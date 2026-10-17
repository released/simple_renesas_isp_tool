#ifndef IMAGE_METADATA_PROFILE_H
#define IMAGE_METADATA_PROFILE_H
#include "bridge_image_metadata.h"
/* Supply the expected local board/Flash/UART/IRQ contract, without reading
 * hardware or installed Flash. Runtime evidence is filled by the engine.
 */
void F24Bridge_Info(bu_info_t *info, uint8_t mode);
#endif
