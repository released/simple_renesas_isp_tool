/* Included by the real dispatcher and portable fault tests. C90 only. */
static uint16_t cb_le16(const uint8_t *p)
{
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}
static uint8_t cb_decode(cb_cursor_t *cursor, const uint8_t *p, uint16_t size)
{
    uint16_t pos;
    uint8_t i;
    uint8_t dlc;
    if (!cursor || !p || size < CB_HEADER || size > 128U ||
        p[0] != CB_VERSION || !p[1] || p[1] > CB_MAX_FRAMES ||
        !cb_le16(p+2) || cb_le16(p+2) > 30000U ||
        cb_le16(p+4) > 0x7FFU || !cb_le16(p+6) || cb_le16(p+6) > 0x7FFU ||
        p[8] > 8U || p[9] || cb_le16(p+10) > 10U) return 0U;
    pos = CB_HEADER;
    for (i = 0U; i < p[1]; i++)
    {
        if (pos + 3U > size || cb_le16(p+pos) > 0x7FFU) return 0U;
        dlc = p[pos+2U];
        pos += 3U;
        if (dlc > 8U || pos + dlc > size) return 0U;
        pos += dlc;
    }
    if (pos != size) return 0U;
    memcpy(cursor->payload, p, size);
    cursor->cursor = CB_HEADER;
    cursor->remaining = p[1];
    return 1U;
}
static uint8_t cb_next(cb_cursor_t *cursor, uint16_t *id,
                       uint8_t *dlc, uint8_t *data)
{
    const uint8_t *p;
    if (!cursor->remaining) return 0U;
    p = cursor->payload + cursor->cursor;
    *id = cb_le16(p);
    *dlc = p[2];
    memcpy(data, p+3, *dlc);
    cursor->cursor += (uint8_t)(3U + *dlc);
    cursor->remaining--;
    return 1U;
}
