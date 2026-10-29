/* Project/API reading notes: Device-engine contract and board callbacks.
 * Flash/ROM operations are injected, keeping transport parsing independent of MCU registers.
 * Installed identity and a version floor are trusted only after full installed-image validation.
 */
#ifndef ISP_DEVICE_H
#define ISP_DEVICE_H

#include "isp_protocol.h"

#ifdef __cplusplus
extern "C"
{
#endif

    /* Board callbacks return nonzero only on success. The updater has no device
     * register, serial-port, Flash-library or reset dependency. Metadata parsing
     * remains in the independent image_metadata module used by the golden sample.
     * The board must provide integer-address-safe far-ROM reads across 64 KiB.
     */
    typedef struct
    {
        unsigned char (*metadata)(const unsigned long *words);
        unsigned char (*erase)(unsigned long address);
        unsigned char (*write)(unsigned long address, const unsigned char *data);
        unsigned char (*read)(unsigned long address, unsigned char *data, unsigned short size);
        /* Require expected size AND stored trailer==crc AND actual image CRC,
         * metadata contract and every App jump slot. Success permits handoff.
         */
        unsigned char (*verify)(unsigned long size, unsigned long crc);
    } IspDeviceOps;

    typedef struct
    {
        unsigned char contract, mode, transport, max_write;
        unsigned short target;
        unsigned char valid, active;
        unsigned long app_start, capacity, flash_size, boot_size, erase_size;
        unsigned long boot_version, boot_build, baud;
        unsigned long installed_version, installed_build, installed_size;
        unsigned char version_control, ready, reserved[2];
        unsigned char part[16], family[12];
        unsigned long irq_abi;
    } IspDeviceConfig;
    typedef char IspInfoLayout[(sizeof(IspDeviceConfig) == 88U) ? 1 : -1];

    typedef struct
    {
        const IspDeviceConfig *config;
        const IspDeviceOps *ops;
        unsigned long image_size, image_crc, next_offset;
        unsigned long installed_version, installed_build, installed_size;
        unsigned long requests, replays, errors, written;
        unsigned short staged;
        unsigned char mode, active, dirty, valid, version_control, ready, action;
        unsigned char cached;
        IspPacket last_request, last_reply;
        /* Keep frequently accessed state in the first256 bytes for RL78 indexed
         * addressing. This RAM-only layout is not a serialized protocol format.
         */
        unsigned long metadata[64];
#if !defined(ISP_DEVICE_BOOT)
        /* Optional App/Transfer-only read-only board snapshot. Never infer its
         * availability from MCU identity. It does not alter the serialized INFO.
         */
        void (*diagnostics)(unsigned char *bytes);
#endif
    } IspDevice;
#if defined(ISP_DEVICE_BOOT) && defined(ISP_XMODEM) &&                                             \
    (defined(__CCRL__) || defined(ISP_TEST_DEVICE_CONTEXT))
    /* One updater per single-interface MCU Boot image; native tests exercise the
     * same fixed-context binding as well as the ordinary injected-state path.
     */
    extern IspDevice g_isp_device;
    extern IspPacket g_isp_request, g_isp_reply;
#endif

    void IspDeviceInit(IspDevice ISP_RAM *device, const IspDeviceConfig *config,
                       const IspDeviceOps *ops, unsigned char mode);
    /* The board supplies installed identity only after complete metadata/vector/
     * image-CRC validation. No version floor is persisted outside that valid App.
     */
    void IspDeviceInstalled(IspDevice ISP_RAM *device, unsigned long version, unsigned long build,
                            unsigned long size, unsigned char version_control);
#if !defined(ISP_DEVICE_BOOT)
    void IspDeviceSetDiagnostics(IspDevice ISP_RAM *device, void (*read)(unsigned char *bytes));
#endif
    /* Dispatch only complete CRC-valid requests. UART/I2C adaptation and idle
     * deadlines are separate. Perform action only after the full reply leaves TX.
     */
    void IspDeviceProcess(IspDevice ISP_RAM *device, const IspPacket ISP_RAM *request,
                          IspPacket ISP_RAM *reply);
#if !defined(ISP_DEVICE_APP) && !defined(ISP_DEVICE_TRANSFER)
    /* Internal main-loop data path, after START. XMODEM owns exact block replay;
     * these functions preserve the same range/metadata/Flash/final CRC gates as
     * framed WRITE/END, without constructing an artificial wire transaction.
     */
    unsigned char IspDeviceWrite(IspDevice ISP_RAM *device, unsigned long offset,
                                 const unsigned char ISP_RAM *data, unsigned short count);
    unsigned char IspDeviceFinish(IspDevice ISP_RAM *device);
#endif

#ifdef __cplusplus
}
#endif

#endif
