/* Compile the actual F13 Flash wrapper, mocking only the vendor/hardware API. */
#include <assert.h>
#include <stdio.h>
#include <r_cg_macrodriver.h>
#include "fsl_flash_interface.h"

static unsigned int blank_calls;
static unsigned int erase_calls;
static unsigned int write_calls;
static unsigned long last_address;
static fsl_u16 last_block;

unsigned char F13_BoardClockReady(void) { return 1U; }
fsl_u08 FSL_Init(const fsl_descriptor_t *descriptor)
{
    assert(descriptor->fsl_frequency_u08 == 32U);
    return FSL_OK;
}
void FSL_Open(void) {}
void FSL_PrepareFunctions(void) {}
void FSL_PrepareExtFunctions(void) {}
fsl_u08 FSL_BlankCheck(fsl_u16 block)
{
    ++blank_calls;
    last_block = block;
    return FSL_ERR_BLANKCHECK;
}
fsl_u08 FSL_Erase(fsl_u16 block)
{
    ++erase_calls;
    assert(block == last_block);
    return FSL_OK;
}
fsl_u08 FSL_Write(fsl_write_t *request)
{
    ++write_calls;
    assert(request->fsl_word_count_u08 == 1U);
    last_address = request->fsl_destination_address_u32;
    return FSL_OK;
}

int main(void)
{
    static const unsigned long rejected[] = {
        0UL, 0x1FFFUL, 0x2001UL, 0x1FBFFUL, 0x1FC00UL,
        0x1FE00UL, 0x1FFFFUL, 0x20000UL, 0xFFFFFFFFUL
    };
    unsigned int index;
    unsigned int before;
    unsigned char data[4] = {0U, 1U, 2U, 3U};

    assert(sizeof(unsigned long) == 4U);
    assert(IMAGE_APPLICATION_CAPACITY == 119UL * 1024UL);
    assert(FLASH_END_ADDRESS == 0x1FBFFUL);
    assert(BLOCK_APP_END == 126UL);
    assert(init_fcl() == FSL_OK);
    assert(flash_erase_app_area(0x2000UL) == FSL_OK);
    assert(last_block == 8U);
    assert(flash_erase_app_area(0x1F800UL) == FSL_OK);
    assert(last_block == 126U);
    assert(flash_write_packet_data(0x2000UL, data, 0U) == FSL_OK);
    assert(last_address == 0x2000UL);
    assert(flash_write_packet_data(0x1FBFCUL, data, 0U) == FSL_OK);
    assert(last_address == 0x1FBFCUL);
    before = blank_calls + erase_calls + write_calls;
    for (index = 0U; index < sizeof(rejected) / sizeof(rejected[0]); ++index)
    {
        assert(flash_erase_app_area(rejected[index]) == FSL_ERR_PARAMETER);
        assert(flash_write_packet_data(rejected[index], data, 0U) == FSL_ERR_PARAMETER);
    }
    assert(flash_write_packet_data(0x2000UL, 0, 0U) == FSL_ERR_PARAMETER);
    assert(blank_calls + erase_calls + write_calls == before);
    puts("PASS F13 actual Flash wrapper: first/last erase/write; 19 invalid calls blocked before FSL.");
    return 0;
}
