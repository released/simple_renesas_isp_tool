/* C90 host fixture: no driver, Flash API or transport implementation. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "image_metadata.h"
#include "image_metadata_config.h"

#ifndef TEST_TRANSPORT
#define TEST_TRANSPORT 1UL
#endif
const unsigned long g_image_metadata_contract[32] = {
    0x004E4143UL, 0x00038006UL, 0x01050000UL | TEST_TRANSPORT, 0x0001F13BUL,
    0UL, 0UL, 0UL, 0x2000UL, IMAGE_APPLICATION_CAPACITY, 0x20000UL,
    0x2000UL, 1024UL, 4UL, 0UL, 0x700UL, 0x701UL,
    0x31463552UL, 0x474D4230UL, 0UL, 0UL, 0x38374C52UL, 0x3331462FUL, 0UL,
    IMAGE_MINIMUM_IMAGE_SIZE, 0x31445055UL, 0x00012003UL | (TEST_TRANSPORT << 24),
    0x31563146UL, IMAGE_BOOT_VERSION, 0UL, 0UL, 0UL, 0UL
};

int main(void)
{
    unsigned long words[64];
    unsigned long broken[64];
    unsigned long value;
    unsigned long expected;
    unsigned short i;
    unsigned char status;
    union { unsigned long align; unsigned char bytes[258]; } input;
    unsigned char before[258];

    memcpy(words, g_image_metadata_contract, 128U);
    memset((unsigned char *)words + 128U, 255, 128U);
    words[4] = 0x010203UL; words[5] = 20261017UL; words[6] = 1024UL;
    assert(ImageMetadataValidate(words) == IMAGE_METADATA_OK);
    words[29] = 1UL;
    assert(ImageMetadataValidate(words) == IMAGE_METADATA_OK);
    words[29] = 0UL;
    for (i = 0U; i < 64U; i++)
    {
        if ((i >= 4U && i <= 6U) || i == 27U || i == 29U) continue;
        memcpy(broken, words, sizeof(words));
        broken[i] ^= 0x01000000UL;
        status = ImageMetadataValidate(broken);
        assert(status != IMAGE_METADATA_OK);
#if IMAGE_METADATA_DETAILED_STATUS
        expected = (i == 25U) ? IMAGE_METADATA_BAD_POLICY :
            (i == 26U) ? IMAGE_METADATA_BAD_IRQ_ABI :
            (i >= 7U && i <= 11U) ? IMAGE_METADATA_BAD_LAYOUT : IMAGE_METADATA_BAD_FIELD;
        assert(status == expected);
#else
        assert(status == IMAGE_METADATA_BAD_IMAGE);
#endif
    }
    memcpy(broken, words, sizeof(words)); broken[4] = 0x01000000UL;
    assert(ImageMetadataValidate(broken) != IMAGE_METADATA_OK);
    memcpy(broken, words, sizeof(words)); broken[5] = 0xFFFFFFFFUL;
    assert(ImageMetadataValidate(broken) != IMAGE_METADATA_OK);
    memcpy(broken, words, sizeof(words)); broken[27] = IMAGE_BOOT_VERSION + 1UL;
    assert(ImageMetadataValidate(broken) != IMAGE_METADATA_OK);
    for (i = 1U; i < 32U; i++)
    {
        memcpy(broken, words, sizeof(words)); broken[29] = 1UL << i;
        assert(ImageMetadataValidate(broken) != IMAGE_METADATA_OK);
    }
    memcpy(broken, words, sizeof(words)); broken[6] = IMAGE_MINIMUM_IMAGE_SIZE;
    assert(ImageMetadataValidate(broken) == IMAGE_METADATA_OK);
    broken[6] = IMAGE_APPLICATION_CAPACITY;
    assert(ImageMetadataValidate(broken) == IMAGE_METADATA_OK);
    broken[6] += 4UL; assert(ImageMetadataValidate(broken) != IMAGE_METADATA_OK);
    broken[6] = 0UL; assert(ImageMetadataValidate(broken) != IMAGE_METADATA_OK);
    broken[6] = 517UL; assert(ImageMetadataValidate(broken) != IMAGE_METADATA_OK);
    assert(ImageMetadataValidate(words) == IMAGE_METADATA_OK);

    for (i = 0U; i < 258U; i++) input.bytes[i] = (unsigned char)i;
    memcpy(before, input.bytes, sizeof(before));
    for (i = 0U; i <= 252U; i++)
    {
        expected = (unsigned long)input.bytes[i + 1U] |
            ((unsigned long)input.bytes[i + 2U] << 8) |
            ((unsigned long)input.bytes[i + 3U] << 16) |
            ((unsigned long)input.bytes[i + 4U] << 24);
        assert(ImageMetadataRead32(input.bytes + 1U, 256U, i, &value));
        assert(value == expected);
    }
    assert(!ImageMetadataRead32(input.bytes, 255U, 0U, &value) && value == 0UL);
    assert(!ImageMetadataRead32(input.bytes, 256U, 253U, &value) && value == 0UL);
    assert(!ImageMetadataRead32(input.bytes, 256U, 65535U, &value) && value == 0UL);
    assert(!ImageMetadataRead32(0, 256U, 0U, &value) && value == 0UL);
    assert(!ImageMetadataRead32(input.bytes, 256U, 0U, 0));
    assert(memcmp(before, input.bytes, sizeof(before)) == 0);
    printf("PASS: C90 metadata parser/profile/bounds; transport=%lu detail=%u\n",
        TEST_TRANSPORT, (unsigned int)IMAGE_METADATA_DETAILED_STATUS);
    return 0;
}
