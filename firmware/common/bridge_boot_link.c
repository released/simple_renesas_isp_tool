#include "bridge_boot_link.h"
#include <string.h>
static uint16_t crc16(const uint8_t *data, uint16_t length) {
    uint16_t crc, i;
    uint8_t bit;
    crc=0xFFFFU;
    for (i=0U; i<length; ++i) {
        crc ^= (uint16_t)data[i]<<8;
        for (bit=0U; bit<8U; ++bit) crc=(uint16_t)((crc&0x8000U) ? (crc<<1)^0x1021U : crc<<1);
    }
    return crc;
}
static void response(bu_link_t *l, uint8_t status, uint16_t length) {
    uint8_t *r;
    r=l->reply; r[0]=0x55U; r[1]=0xAAU; r[2]=2U; r[3]=0x20U;
    r[4]=l->input[4]; r[5]=l->input[5]; r[6]=l->input[6]; r[7]=status;
    Bu_Write16(r+8,length); Bu_Write16(r+10,crc16(r+2U,8U));
    Bu_Write16(r+12U+length,crc16(r+12U,length)); l->reply_length=(uint16_t)(14U+length);
}
static void dispatch(bu_link_t *l) {
    uint8_t status, command, *out;
    uint16_t n, size;
    const uint8_t *p;
    bu_info_t *d;
    command=l->input[6]; n=Bu_Read16(l->input+8); p=l->input+12; out=l->reply+12;
    d=&l->engine->info; status=BU_OK; size=0U;
    if (l->last_length && Bu_Read16(l->input+4)==Bu_Read16(l->last_request+4)) {
        if (l->count==l->last_length && !memcmp(l->input,l->last_request,l->count)) {
            memcpy(l->reply,l->last_reply,l->last_reply_length); l->reply_length=l->last_reply_length; return;
        }
        response(l,BU_SEQUENCE_ERROR,0U); return;
    }
    switch (command) {
    case 0x00U:
        if (n>HOST_WIRE_MAX_PAYLOAD) status=BU_BAD_ARGUMENT;
        else { memcpy(out,p,n); size=n; }
        break;
    case 0x01U:
        if (n) { status=BU_BAD_ARGUMENT; break; }
        memset(out,0,16U); Bu_Write16(out,d->board_id);
        out[2]=(uint8_t)(d->boot_version>>16); out[3]=(uint8_t)(d->boot_version>>8);
        out[4]=(uint8_t)d->boot_version; out[5]=2U; out[6]=2U;
        Bu_Write32(out+8,d->boot_build); Bu_Write32(out+12,BU_FEATURE); size=16U; break;
    case 0x02U:
        if (n) { status=BU_BAD_ARGUMENT; break; }
        memset(out,0,24U); Bu_Write32(out,BU_FEATURE); Bu_Write16(out+4,HOST_WIRE_MAX_PAYLOAD);
        out[7]=1U; out[8]=1U; size=24U; break;
    case 0x03U:
        if (n) status=BU_BAD_ARGUMENT;
        else l->action=1U;
        break;
    case 0x30U:
        if (n) status=BU_BAD_ARGUMENT;
        else { memset(out,0,56U); size=56U; }
        break;
    case BU_INFO_COMMAND:
        if (n) status=BU_BAD_ARGUMENT;
        else { Bu_EncodeInfo(d,out); size=BU_INFO_SIZE; }
        break;
    case BU_DIAGNOSTICS_COMMAND:
        if (n) status=BU_BAD_ARGUMENT;
        else { out[0]=1U; out[1]=l->engine->hold_reason; out[2]=0U; out[3]=d->flags;
            Bu_Write32(out+4U,l->engine->next); size=8U; }
        break;
    case BU_READ_METADATA_COMMAND:
        status=Bu_ReadMetadata(l->engine,p,n,out,&size); break;
    case BU_STAGE_METADATA_COMMAND:
        status=Bu_StageMetadata(l->engine,p,n);
        if (!status) { Bu_Write16(out,l->engine->staged); Bu_Write16(out+2U,BU_METADATA_REGION_SIZE); size=4U; }
        break;
    case BU_BEGIN_COMMAND:
        status=Bu_Begin(l->engine,p,n);
        if (!status) { Bu_Write32(out,l->engine->next); size=4U; } break;
    case BU_DATA_COMMAND:
        status=Bu_Data(l->engine,p,n);
        if (!status) { Bu_Write32(out,l->engine->next); size=4U; } break;
    case BU_END_COMMAND:
        status=Bu_End(l->engine,p,n);
        if (!status) { memcpy(out,p,4U); size=4U; } break;
    case BU_ABORT_COMMAND:
        if (n) status=BU_BAD_ARGUMENT;
        else Bu_Abort(l->engine);
        break;
    case BU_RUN_COMMAND:
        if (n!=4U || Bu_Read16(p)!=d->board_id || p[2]!=BU_PROTOCOL || p[3]!=0xA0U) status=BU_BAD_ARGUMENT;
        else if (!Bu_ValidateInstalled(l->engine)) status=BU_BAD_STATE;
        else l->action=2U;
        break;
    default: status=2U; break;
    }
    response(l,status,size);
    memcpy(l->last_request,l->input,l->count); l->last_length=l->count;
    memcpy(l->last_reply,l->reply,l->reply_length); l->last_reply_length=l->reply_length;
}
void Bu_LinkInit(bu_link_t *l, bu_engine_t *engine) { memset(l,0,sizeof(*l)); l->engine=engine; }
void Bu_LinkTimeout(bu_link_t *l) { l->count=0U; }
void Bu_LinkFeed(bu_link_t *l, uint8_t byte) {
    uint16_t n;
    if (l->reply_length || l->action) return;
    if (l->count>=BU_LINK_FRAME_SIZE) l->count=0U;
    l->input[l->count++]=byte;
    while (l->count && l->input[0]!=0x55U) { --l->count; memmove(l->input,l->input+1,l->count); }
    if (l->count>=2U && l->input[1]!=0xAAU) { l->count=byte==0x55U ? 1U : 0U; return; }
    if (l->count<12U) return;
    n=Bu_Read16(l->input+8);
    if (l->input[2]!=2U || l->input[3]!=0x10U || l->input[7] || !Bu_Read16(l->input+4) ||
        n>HOST_WIRE_MAX_PAYLOAD || Bu_Read16(l->input+10)!=crc16(l->input+2U,8U)) { l->count=0U; return; }
    if (l->count!=n+14U) return;
    if (Bu_Read16(l->input+12U+n)==crc16(l->input+12U,n)) dispatch(l);
    l->count=0U;
}
