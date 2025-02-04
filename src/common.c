#include "area.h"
#include "asm.h"
#include "common.h"
#include "flags.h"
#include "functions.h"
#include "game.h"
#include "global.h"
#include "kinstone.h"
#include "main.h"
#include "message.h"
#include "room.h"
#include "save.h"
#include "screen.h"
#include "sound.h"
#include "structures.h"

typedef struct {
    u8 area;
    u8 room;
    u8 unk_2;
    u8 unk_3;
    u32 mapDataOffset;
} DungeonLayout;

extern u8 gUnk_03003DE0;
extern u8 gzHeap[0x1000];
extern u32 gUnk_0201AEE0[0x800];
extern s16 gUnk_02018EE0[];

extern void (*const gUnk_080C9CAC[])(void);

static void StoreKeyInput(Input* input, u32 keyInput);
void ClearOAM(void);
void ResetScreenRegs(void);
void MessageFromFusionTarget(u32);
void sub_0801E24C(s32, s32);
void sub_0801E290(u32, u32, u32);
s32 sub_0801E8B0(u32);

extern u32 sub_0807CB24(u32, u32);

typedef struct {
    u16 paletteId;
    u8 destPaletteNum;
    u8 numPalettes;
} PaletteGroup;

typedef struct {
    union {
        int raw;
        struct {
            u8 filler0[0x3];
            u8 unk3;
        } bytes;
    } unk0;
    u32 dest;
    u32 unk8;
} GfxItem;

typedef struct {
    u8 filler[0xA00];
} struct_02017AA0;
extern struct_02017AA0 gUnk_02017AA0[];

extern const PaletteGroup* gPaletteGroups[];
extern const u8 gGlobalGfxAndPalettes[];
extern u32 gUsedPalettes;
extern u16 gPaletteBuffer[];
extern const GfxItem* gGfxGroups[];

extern const u32 gUnk_080C9460[];

void sub_0801E82C(void);

extern void* GetRoomProperty(u32, u32, u32);

extern u8 gMapData;
extern const DungeonLayout** const gUnk_080C9C50[];
extern u8 gMapDataBottomSpecial[];

u32 sub_0801DF10(const DungeonLayout* lyt);
bool32 sub_0801DF90(TileEntity* tileEntity, u32 bank);
u32 sub_0801DF60(u32 a1, u8* p);
u32 sub_0801DF78(u32 a1, u32 a2);
void sub_0801DF28(u32 x, u32 y, s32 color);

u32 DecToHex(u32 value) {
    u32 result;
    register u32 r1 asm("r1");

    if (value >= 100000000) {
        return 0x99999999;
    }

    result = Div(value, 10000000) * 0x10000000;
    result += Div(r1, 1000000) * 0x1000000;
    result += Div(r1, 100000) * 0x100000;
    result += Div(r1, 10000) * 0x10000;
    result += Div(r1, 1000) * 0x1000;
    result += Div(r1, 100) * 0x100;
    result += Div(r1, 10) * 0x10;
    return result + r1;
}

u32 ReadBit(void* src, u32 bit) {
    return (*((u8*)src + bit / 8) >> (bit % 8)) & 1;
}

NONMATCH("asm/non_matching/common/WriteBit.inc", u32 WriteBit(void* src, u32 bit)) {
    u8* b;
    u32 mask;
    u32 orig;

    b = (u8*)(bit / 8 + (u32)src);
    mask = 1 << (bit % 8);
    orig = *b;
    *b |= mask;
    orig &= mask;
    return orig;
}
END_NONMATCH

NONMATCH("asm/non_matching/common/ClearBit.inc", u32 ClearBit(void* src, u32 bit)) {
    u8* b;
    u32 mask;
    u32 orig;

    b = (u8*)(bit / 8 + (u32)src);
    mask = 1 << (bit % 8);
    orig = *b;
    *b &= ~mask;
    orig &= mask;
    return orig;
}
END_NONMATCH

void MemFill16(u32 value, void* dest, u32 size) {
    DmaFill16(3, value, dest, size);
}

void MemFill32(u32 value, void* dest, u32 size) {
    DmaFill32(3, value, dest, size);
}

void MemClear(void* dest, u32 size) {
    u32 zero = 0;

    // alignment check
    switch (((uintptr_t)dest | size) % 4) {
        case 0:
            MemFill32(0, dest, size);
            break;
        case 2:
            MemFill16(0, dest, size);
            break;
        default:
            do {
                *(u8*)dest = zero;
                dest++;
                size--;
            } while (size != 0);
    }
}

void MemCopy(const void* src, void* dest, u32 size) {
    switch (((uintptr_t)src | (uintptr_t)dest | size) % 4) {
        case 0:
            DmaCopy32(3, src, dest, size);
            break;
        case 2:
            DmaCopy16(3, src, dest, size);
            break;
        default:
            do {
                *(u8*)dest = *(u8*)src;
                src++;
                dest++;
            } while (--size);
    }
}

void ReadKeyInput(void) {
    u32 keyInput = ~REG_KEYINPUT & KEYS_MASK;
    StoreKeyInput(&gInput, keyInput);
}

static void StoreKeyInput(Input* input, u32 keyInput) {
    u32 heldKeys = input->heldKeys;
    u32 difference = keyInput & ~heldKeys;
    input->newKeys = difference;
    if (keyInput == heldKeys) {
        if (--input->unk7 == 0) {
            input->unk7 = 4;
            input->unk4 = keyInput;
        } else {
            input->unk4 = 0;
        }
    } else {
        input->unk7 = 20;
        input->unk4 = difference;
    }
    input->heldKeys = keyInput;
}

void LoadPaletteGroup(u32 group) {
    const PaletteGroup* paletteGroup = gPaletteGroups[group];
    while (1) {
        u32 destPaletteNum = paletteGroup->destPaletteNum;
        u32 numPalettes = paletteGroup->numPalettes & 0xF;
        if (numPalettes == 0) {
            numPalettes = 16;
        }
        LoadPalettes(&gGlobalGfxAndPalettes[paletteGroup->paletteId * 32], destPaletteNum, numPalettes);
        if ((paletteGroup->numPalettes & 0x80) == 0) {
            break;
        }
        paletteGroup++;
    }
}

void LoadPalettes(const u8* src, s32 destPaletteNum, s32 numPalettes) {
    u16* dest;
    u32 size = numPalettes * 32;
    u32 usedPalettesMask = 1 << destPaletteNum;
    while (--numPalettes > 0) {
        usedPalettesMask |= (usedPalettesMask << 1);
    }
    gUsedPalettes |= usedPalettesMask;
    dest = &gPaletteBuffer[destPaletteNum * 16];
    DmaCopy32(3, src, dest, size);
}

void SetColor(u32 colorIndex, u32 color) {
    gPaletteBuffer[colorIndex] = color;
    gUsedPalettes |= 1 << (colorIndex / 16);
}

void SetFillColor(u32 color, u32 disable_layers) {
    if (disable_layers) {
        gScreen.lcd.displayControlMask = ~(DISPCNT_OBJ_ON | DISPCNT_BG_ALL_ON);
    } else {
        gScreen.lcd.displayControlMask = ~0;
    }
    SetColor(0, color);
}

void LoadGfxGroup(u32 group) {
    u32 terminator;
    u32 dmaCtrl;
    int gfxOffset;
    const u8* src;
    u32 dest;
    int size;
    const GfxItem* gfxItem = gGfxGroups[group];
    while (1) {
        u32 loadGfx = FALSE;
        u32 ctrl = gfxItem->unk0.bytes.unk3;
        ctrl &= 0xF;
        switch (ctrl) {
            case 0x7:
                loadGfx = TRUE;
                break;
            case 0xD:
                return;
            case 0xE:
                if (gSaveHeader->language != 0 && gSaveHeader->language != 1) {
                    loadGfx = TRUE;
                }
                break;
            case 0xF:
                if (gSaveHeader->language != 0) {
                    loadGfx = TRUE;
                }
                break;
            default:
                if (ctrl == gSaveHeader->language) {
                    loadGfx = TRUE;
                }
                break;
        }

        if (loadGfx) {
            gfxOffset = gfxItem->unk0.raw & 0xFFFFFF;
            src = &gGlobalGfxAndPalettes[gfxOffset];
            dest = gfxItem->dest;
            size = gfxItem->unk8;
            dmaCtrl = 0x80000000;
            if (size < 0) {
                if (dest >= VRAM) {
                    LZ77UnCompVram(src, (void*)dest);
                } else {
                    LZ77UnCompWram(src, (void*)dest);
                }
            } else {
                DmaSet(3, src, dest, dmaCtrl | ((u32)size >> 1));
            }
        }

        terminator = gfxItem->unk0.bytes.unk3;
        terminator &= 0x80;
        gfxItem++;
        if (!terminator) {
            break;
        }
    }
}

void sub_0801D898(void* dest, void* src, u32 word, u32 size) {
    u32 v6;
    u32 i;

    if (size & 0x8000)
        v6 = 0x40;
    else
        v6 = 0x20;

    size &= (short)~0x8000;
    do {
        DmaCopy16(3, src, dest, word * 2);
        src += word * 2;
        dest += v6 * 2;
    } while (--size);
}

ASM_FUNC("asm/non_matching/common/zMalloc.inc", void* zMalloc(u32 size));

void zFree(void* ptr) {
    u32 uVar1;
    u32 i;
    u16* puVar3;
    s32 uVar5;
    u16* ptr2;

    uVar1 = (int)ptr - (int)gzHeap;
    if (uVar1 < 0x1000) {
        puVar3 = (u16*)gzHeap;
        uVar5 = *puVar3++;

        for (i = 0; i < uVar5; puVar3 += 2, i++) {
            if (*puVar3 == uVar1) {
                ptr2 = &((u16*)(gzHeap - 2))[uVar5 * 2];
                *puVar3 = *ptr2;
                *ptr2++ = 0;
                *(puVar3 + 1) = *ptr2;
                *ptr2 = 0;
                *(u16*)(gzHeap) = uVar5 - 1;
                break;
            }
        }
    }
}

void zMallocInit(void) {
    MemClear(gzHeap, sizeof(gzHeap));
}

void DispReset(bool32 refresh) {
    gMain.interruptFlag = 1;
    gUnk_03003DE0 = 0;
    gFadeControl.active = 0;
    gScreen.vBlankDMA.readyBackup = FALSE;
    gScreen.vBlankDMA.ready = FALSE;
    DmaStop(0);
    REG_DISPCNT = 0;
    ClearOAM();
    ResetScreenRegs();
    MemClear((void*)0x600C000, 0x20);
    MemClear(gBG0Buffer, sizeof(gBG0Buffer));
    gScreen.bg0.updated = refresh;
}

void ClearOAM(void) {
    u8* d = (u8*)gOAMControls.oam;
    u8* mem = (u8*)0x07000000;
    u32 i;
    for (i = 128; i != 0; --i) {
        *(u16*)d = 0x2A0;
        d += 8;
        *(u16*)mem = 0x2A0;
        mem += 8;
    }
}

void ResetScreenRegs(void) {
    MemClear(&gScreen, sizeof(gScreen));
    gScreen.bg0.tilemap = &gBG0Buffer;
    gScreen.bg0.control = 0x1F0C;
    gScreen.bg1.tilemap = &gBG1Buffer;
    gScreen.bg1.control = 0x1C01;
    gScreen.bg2.tilemap = &gBG2Buffer;
    gScreen.bg2.control = 0x1D02;
    gScreen.bg3.tilemap = &gBG3Buffer;
    gScreen.bg3.control = 0x1E03;
    gScreen.lcd.displayControl = 0x140;
    gScreen.lcd.displayControlMask = 0xffff;
}

u32 sub_0801DB94(void) {
    return gRoomTransition.player_status.dungeon_map_y >> 11;
}

ASM_FUNC("asm/non_matching/common/DrawDungeonMap.inc", void DrawDungeonMap(u32 floor, void* data, u32 size));

void sub_0801DD58(u32 area, u32 room) {
    RoomHeader* hdr = gAreaRoomHeaders[area] + room;
    gArea.pCurrentRoomInfo->map_x = hdr->map_x;
    gArea.pCurrentRoomInfo->map_y = hdr->map_y;
}

void LoadDungeonMap(void) {
    LoadResourceAsync(gUnk_0201AEE0, 0x6006000, sizeof(gUnk_0201AEE0));
}

void DrawDungeonFeatures(u32 floor, void* data, u32 size) {
    u32 bankOffset;
    u32 width;
    u32 height;
    u32 x;
    u32 y;
    u16 mapX;
    u16 mapY;
    u32 tmp;
    u32 tmp2;
    u32 color;
    u32 features;
    TileEntity* tileEntity;
    RoomHeader* roomHeader;
    const DungeonLayout* layout;
    const DungeonLayout* nextLayout;
    u8* ptr;
    u32 tmp3;
    u32 tmp4;

    if (!AreaHasMap()) {
        return;
    }
    layout = gUnk_080C9C50[gArea.dungeon_idx][floor];
    MemClear(gMapDataBottomSpecial, 0x8000);
    while (layout->area != 0) {
        tileEntity = (TileEntity*)GetRoomProperty(layout->area, layout->room, 3);
        bankOffset = sub_0801DF10(layout);
        features = 0;
        if (layout->area == gUI.roomControls.area && layout->room == gUI.roomControls.room) {
            features = 8;
        } else {
            if (HasDungeonSmallKey()) {
                features = 2;
            }
            if (sub_0801DF90(tileEntity, bankOffset)) {
                features = 3;
            }
        }
        if ((layout->unk_2 & 1) != 0) {
            features = 0;
        }
        nextLayout = layout + 1;
        if (features != 0) {
            DmaCopy32(3, &gMapData + layout->mapDataOffset, &gMapDataBottomSpecial, 0x400);

            roomHeader = gAreaRoomHeaders[layout->area] + layout->room;
            mapX = roomHeader->map_x / 0x10;
            tmp3 = roomHeader->map_y;
            tmp4 = 0x7ff;
            mapY = (tmp3 & tmp4) / 0x10;
            width = roomHeader->pixel_width / 0x10;
            height = roomHeader->pixel_height / 0x10;
            tmp = (width + 3) / 4;

            for (y = 0; y < height; y++) {
                ptr = gMapDataBottomSpecial + y * tmp;
                for (x = 0; x < width; x++) {
                    tmp2 = mapX + x;
                    color = sub_0801DF78(sub_0801DF60(x, ptr), features);
                    sub_0801DF28(tmp2, mapY + y, color);
                }
            }
        }
        layout = nextLayout;
    }
}

u32 sub_0801DF10(const DungeonLayout* layout) {
    u32 offset;

    if (layout->unk_3 == 1)
        offset = 0x300;
    else
        offset = GetFlagBankOffset(layout->area);
    return offset;
}

void sub_0801DF28(u32 x, u32 y, s32 color) {
    u32* ptr;
    u32 tmp;
    ptr = &gUnk_0201AEE0[(((y >> 3) * 0x10 + (x >> 3)) * 8)];
    ptr = &ptr[(y & 7)];
    tmp = (color << ((x & 7) * 4));
    ptr[0] = (ptr[0] & gUnk_080C9460[x & 7]) | tmp;
}
u32 sub_0801DF60(u32 a1, u8* p) {
    return (p[a1 >> 2] >> (2 * (~a1 & 3))) & 3;
}

u32 sub_0801DF78(u32 a1, u32 a2) {
    switch (a1) {
        case 0:
        case 1:
        default:
            return a1;
        case 2:
            return a2;
        case 3:
            return 7;
    }
}

bool32 sub_0801DF90(TileEntity* tileEntity, u32 bank) {
    if (tileEntity == NULL)
        return FALSE;

    for (; tileEntity->type != 0; tileEntity++) {
        if (tileEntity->type == 1)
            return CheckLocalFlagByBank(bank, tileEntity->localFlag);
    }
    return FALSE;
}

void sub_0801DFB4(Entity* entity, u32 textIndex, u32 a3, u32 a4) {
    MemClear(&gFuseInfo, sizeof(gFuseInfo));
    gFuseInfo.textIndex = textIndex;
    gFuseInfo._8 = a3;
    gFuseInfo._a = a4;
    gFuseInfo.ent = entity;
    gFuseInfo._3 = gUnk_03003DF0.unk_2;
    if (entity != NULL) {
        gFuseInfo.prevUpdatePriority = entity->updatePriority;
        entity->updatePriority = 2;
    }
    gFuseInfo._0 = 0;
}

u32 sub_0801E00C(void) {
    gUnk_080C9CAC[gFuseInfo.action]();
    return gFuseInfo._0;
}

void sub_0801E02C(void) {
    MessageFromFusionTarget(gFuseInfo.textIndex);
    gFuseInfo._0 = 3;
    gFuseInfo.action = 1;
}

void sub_0801E044(void) {
    if ((gMessage.doTextBox & 0x7F) == 0) {
        MenuFadeIn(4, 0);
        gFuseInfo._0 = 4;
        gFuseInfo.action = 2;
        SoundReq(SFX_6B);
    }
}

void sub_0801E074(void) {
    u32 tmp;
    switch (gFuseInfo._0) {
        case 5:
            tmp = gFuseInfo._8;
            break;
        case 6:
            tmp = gFuseInfo._a;
            break;
        default:
            return;
    }
    MessageFromFusionTarget(tmp);
    gFuseInfo.action = 3;
}

void sub_0801E0A0(void) {
    if ((gMessage.doTextBox & 0x7f) == 0) {
        if (gFuseInfo.ent != NULL) {
            gFuseInfo.ent->updatePriority = gFuseInfo.prevUpdatePriority;
        }
        gFuseInfo._0 = gFuseInfo._0 == 6 ? 2 : 1;
    }
}

void MessageFromFusionTarget(u32 textIndex) {
    if (textIndex != 0) {
        if (gFuseInfo.ent != NULL) {
            MessageNoOverlap(textIndex, gFuseInfo.ent);
        } else {
            MessageFromTarget(textIndex);
        }
    }
}

void sub_0801E104(void) {
    gScreen.lcd.displayControl &= ~0x6000;
    gScreen.vBlankDMA.ready = FALSE;
}

void sub_0801E120(void) {
    gScreen.lcd.displayControl |= 0x2000;
    gScreen.controls.windowInsideControl = 0x3F37;
    gScreen.controls.windowOutsideControl = 0x3F;
    gScreen.controls.window0HorizontalDimensions = 0;
    gScreen.controls.window0VerticalDimensions = 160;
}

void sub_0801E154(u32 a1) {
    sub_0801E24C(a1, 0);
}

void sub_0801E160(u32 a1, u32 a2, u32 a3) {
    MemClear(&gUnk_02017AA0[gUnk_03003DE4[0]], sizeof(gUnk_02017AA0[gUnk_03003DE4[0]]));
    sub_0801E290(a1, a2, a3);
    SetVBlankDMA((u16*)&gUnk_02017AA0[gUnk_03003DE4[0]], (u16*)REG_ADDR_WIN0H,
                 ((DMA_ENABLE | DMA_START_HBLANK | DMA_16BIT | DMA_REPEAT | DMA_SRC_INC | DMA_DEST_RELOAD) << 16) +
                     0x1);
}

void sub_0801E1B8(u32 a1, u32 a2) {
    gScreen.lcd.displayControl |= 0x2000;
    gScreen.controls.windowInsideControl = a1;
    gScreen.controls.windowOutsideControl = a2;
    gScreen.controls.window0HorizontalDimensions = 0;
    gScreen.controls.window0VerticalDimensions = 160;
}

void sub_0801E1EC(u32 a1, u32 a2, u32 a3) {
    MemClear(&gUnk_02017AA0[gUnk_03003DE4[0]], sizeof(gUnk_02017AA0[gUnk_03003DE4[0]]));
    sub_0801E24C(a3, 0);
    sub_0801E290(a1, a2, a3);
    SetVBlankDMA((u16*)&gUnk_02017AA0[gUnk_03003DE4[0]], (u16*)REG_ADDR_WIN0H,
                 ((DMA_ENABLE | DMA_START_HBLANK | DMA_16BIT | DMA_REPEAT | DMA_SRC_INC | DMA_DEST_RELOAD) << 16) +
                     0x1);
}

void sub_0801E24C(s32 param_1, s32 param_2) {
    s32 r1;
    s32 r2, i;
    u16* p5;
    p5 = &gUnk_02018EE0[param_2];
    i = 0;
    r2 = param_1;
    r1 = 3 - (r2 * 2);
    while (i <= r2) {
        p5[i] = r2;
        p5[r2] = i;
        if (r1 < 0) {
            r1 += 6 + i++ * 4;
        } else {
            r1 += 10 + (i++ - (r2--)) * 4;
        }
    }
}

void sub_0801E290(u32 param_1, u32 param_2, u32 count) {
    s32 uVar1;
    s32 iVar2;
    s32 iVar4;
    u8* forwardAccess;
    u8* backwardAccess;
    s16* puVar6;
    u32 uVar5;
    u32 uVar7;
    u32 index;
    u32 x;
    forwardAccess = &gUnk_02017AA0[gUnk_03003DE4[0]].filler[param_2 * 2];
    backwardAccess = forwardAccess;
    uVar5 = uVar7 = param_2;
    puVar6 = gUnk_02018EE0;

    while (count-- > 0) {
        uVar1 = *puVar6++;
        iVar2 = param_1 - uVar1;
        iVar4 = param_1 + uVar1;
        if (iVar2 < 0) {
            iVar2 = 0;
        }
        if (iVar4 > 0xef) {
            iVar4 = 0xf0;
        }
        if (((u16)uVar5 & 0xffff) < 0xa0) {
            backwardAccess[0] = iVar4;
            backwardAccess[1] = iVar2;
        }
        if (((u16)uVar7 & 0xffff) < 0xa0) {
            forwardAccess[0] = iVar4;
            forwardAccess[1] = iVar2;
        }
        backwardAccess -= 2;
        forwardAccess += 2;
        uVar5--;
        uVar7++;
    }
}

ASM_FUNC("asm/non_matching/common/sub_0801E31C.inc", void sub_0801E31C(u32 a1, u32 a2, u32 a3, u32 a4));

ASM_FUNC("asm/non_matching/common/sub_0801E49C.inc", void sub_0801E49C(u32 a1, u32 a2, u32 a3, u32 a4));

void sub_0801E64C(s32 param_1, s32 param_2, s32 param_3, s32 param_4, s32 param_5) {
    s32 sVar1;
    s32* ptr = (s32*)gUnk_02018EE0;
    register s32 tmp asm("r1");

    if ((0 <= param_2 || 0 <= param_4) && (param_2 < 0xa0 || (param_4 < 0xa0))) {
        if (param_2 > param_4) {
            SWAP(param_2, param_4, tmp);
            SWAP(param_1, param_3, tmp);
        }
        if (param_2 != param_4) {
            sVar1 = Div((param_3 - param_1) * 0x10000, param_4 - param_2);
            if (param_2 < 0) {
                param_1 += (sVar1 * -param_2) >> 0x10;
                param_2 = 0;
            }
            if (0x9f < param_4) {
                param_4 = 0x9f;
            }
            param_3 = param_1 << 0x10;
            ptr += param_2 * 3 + param_5;
            do {
                if (param_1 < 0) {
                    param_1 = 0;
                }
                if (0xf0 < param_1) {
                    param_1 = 0xf0;
                }
                *ptr = param_1;
                param_3 += sVar1;
                param_1 = param_3 >> 0x10;
                param_2++;
                ptr += 3;
            } while (param_2 <= param_4);
        }
    }
}

void sub_0801E6C8(u32 param_1) {
    u32 tmp;
    u32 index;
    if (param_1 - 1 < 100) {
        for (index = 0; index < 0x80; index++) {
            if (param_1 == gSave.unk1C1[index]) {
                gSave.unk1C1[index] = 0xf1;
            }
        }
        tmp = sub_08002632(gFuseInfo.ent);
        if ((tmp - 1 < 0x7f) && (gSave.unk1C1[tmp] == 0xf1)) {
            gSave.unk1C1[tmp] = 0xf2;
        }
        for (index = 0; index < 0x20; index++) {
            if (param_1 == gUnk_03003DF0.array[index].unk_3) {
                gUnk_03003DF0.array[index].unk_3 = 0xf1;
            }
        }
    }
}

void sub_0801E738(u32 param_1) {
    s32 index;
    s32 tmp;

    sub_0801E82C();
    if (param_1 - 0x65 < 0x11) {
        index = sub_0801E8B0(param_1);
        if (index < 0) {
            index = 0;
            while (gSave.unk118[index] != 0) {
                index++;
            }
        }
        if ((u32)index < 0x12) {
            gSave.unk118[index] = param_1;
            tmp = gSave.unk12B[index] + 1;
            if (tmp > 99) {
                tmp = 99;
            }
            gSave.unk12B[index] = tmp;
        }
    }
}

void sub_0801E798(u32 a1) {
    s32 idx = sub_0801E8B0(a1);
    if (idx >= 0) {
        s32 next = gSave.unk12B[idx] - 1;
        if (next <= 0) {
            gSave.unk118[idx] = 0;
            next = 0;
        }
        gSave.unk12B[idx] = next;
    }
}

u32 sub_0801E7D0(u32 a1) {
    s32 tmp = sub_0801E8B0(a1);
    if (tmp < 0) {
        return 0;
    }
    return gSave.unk12B[tmp];
}

u32 CheckKinstoneFused(u32 idx) {
    if (idx > 100 || idx < 1) {
        return 0;
    }
    return ReadBit(&gSave.unk241, idx);
}

bool32 sub_0801E810(u32 idx) {
    if (idx > 100 || idx < 1) {
        return FALSE;
    }
    return ReadBit(&gSave.unk24E, idx);
}

ASM_FUNC("asm/non_matching/common/sub_0801E82C.inc", void sub_0801E82C(void));

s32 sub_0801E8B0(u32 idx) {
    u32 i;

    for (i = 0; i < 18; ++i) {
        if (idx == gSave.unk118[i])
            return i;
    }
    return -1;
}

void sub_0801E8D4(void) {
    u32 i;
    for (i = 10; i <= 100; ++i) {
        if (CheckKinstoneFused(i) && !sub_0801E810(i)) {
            u32 evt_type = gUnk_080C9CBC[i].evt_type;
            struct_080FE320* s = &gUnk_080FE320[evt_type];
#if !defined EU && !defined JP
            u32 flag = s->flag;
#endif
            u32 tmp;
            switch (s->_10) {
                case 0:
                    tmp = 0;
                    break;
                case 1:
                    tmp = s->_11;
                    break;
                case 2:
                    tmp = 0xf;
                    break;
                case 3:
                    tmp = 0x10;
                    break;
                case 4:
                    tmp = 0x11;
                    break;
#if !defined EU && !defined JP
                case 5:
                    tmp = 4;
                    flag = 0x83;
                    break;
                case 6:
                    tmp = 4;
                    flag = 0x84;
                    break;
                case 7:
                    tmp = 4;
                    flag = 0x87;
                    break;
                case 8:
                    tmp = 4;
                    flag = 0x88;
                    break;
                case 9:
                    tmp = 4;
                    flag = 0x8b;
                    break;
#ifndef DEMO_JP
                case 10:
                    tmp = 5;
                    flag = 0x88;
                    break;
#endif
#endif
            }
#if !defined EU && !defined JP
            if (sub_0807CB24(tmp, flag)) {
#else
            if (sub_0807CB24(tmp, s->flag)) {
#endif
                WriteBit(&gSave.unk24E, i);
            }
        }
    }
}

ASM_FUNC("asm/non_matching/common/sub_0801E99C.inc", u32 sub_0801E99C(u32 a1));

const struct_080C9C6C gUnk_080C9C6C[] = {
    { 1, 2, 2 }, { 3, 3, 3 }, { 4, 3, 0 }, { 3, 5, 5 }, { 3, 2, 2 }, { 5, 7, 7 }, { 5, 5, 5 }, { 1, 3, 3 },
    { 1, 3, 3 }, { 1, 3, 3 }, { 1, 3, 3 }, { 1, 3, 3 }, { 1, 3, 3 }, { 1, 3, 3 }, { 1, 3, 3 }, { 1, 3, 3 },
};

void (*const gUnk_080C9CAC[])(void) = {
    sub_0801E02C,
    sub_0801E044,
    sub_0801E074,
    sub_0801E0A0,
};

// TODO merge
#ifdef JP
const struct_080C9CBC gUnk_080C9CBC[] = {
    { 15, 44, 45, 8, 0, 0, 0, 0 }, { 4, 8, 1, 0, 0, 1, 2, 0 },    { 4, 9, 1, 0, 0, 2, 2, 0 },
    { 4, 10, 1, 0, 0, 3, 2, 0 },   { 4, 10, 1, 0, 0, 3, 2, 0 },   { 4, 9, 1, 0, 0, 2, 2, 0 },
    { 4, 13, 2, 0, 0, 6, 2, 0 },   { 4, 14, 2, 0, 0, 7, 2, 0 },   { 4, 15, 2, 0, 0, 8, 2, 0 },
    { 4, 16, 3, 0, 0, 9, 2, 0 },   { 0, 18, 5, 8, 20, 11, 3, 2 }, { 0, 18, 5, 8, 77, 11, 0, 4 },
    { 0, 18, 5, 8, 65, 11, 2, 5 }, { 0, 18, 5, 8, 7, 11, 1, 3 },  { 0, 18, 5, 8, 87, 11, 2, 0 },
    { 0, 18, 5, 8, 92, 11, 2, 8 }, { 0, 18, 5, 8, 37, 11, 0, 3 }, { 0, 18, 5, 8, 55, 11, 0, 5 },
    { 0, 18, 5, 8, 62, 11, 2, 5 }, { 0, 19, 5, 8, 63, 12, 0, 5 }, { 0, 19, 5, 8, 88, 12, 3, 4 },
    { 0, 19, 5, 8, 66, 12, 2, 5 }, { 0, 19, 5, 8, 10, 12, 3, 2 }, { 0, 20, 5, 8, 70, 13, 2, 6 },
    { 0, 19, 5, 8, 42, 12, 0, 5 }, { 0, 19, 5, 8, 38, 12, 0, 3 }, { 0, 19, 5, 8, 68, 12, 2, 6 },
    { 0, 20, 5, 8, 76, 13, 0, 4 }, { 0, 20, 5, 8, 91, 13, 3, 4 }, { 0, 20, 5, 8, 67, 13, 2, 5 },
    { 0, 20, 5, 8, 43, 13, 3, 5 }, { 0, 20, 5, 8, 41, 13, 0, 7 }, { 0, 20, 5, 8, 36, 13, 0, 3 },
    { 0, 20, 5, 8, 50, 13, 2, 7 }, { 1, 21, 6, 8, 39, 14, 0, 7 }, { 1, 21, 6, 8, 69, 14, 2, 6 },
    { 1, 21, 6, 8, 72, 14, 2, 6 }, { 1, 22, 6, 8, 82, 15, 2, 4 }, { 1, 22, 6, 8, 84, 15, 2, 4 },
    { 1, 21, 6, 8, 56, 14, 2, 5 }, { 1, 21, 6, 8, 78, 14, 2, 5 }, { 1, 21, 6, 8, 81, 14, 2, 4 },
    { 1, 21, 6, 8, 83, 14, 2, 4 }, { 1, 21, 6, 8, 85, 14, 2, 4 }, { 1, 21, 6, 8, 90, 14, 2, 4 },
    { 1, 22, 6, 8, 57, 15, 0, 5 }, { 1, 22, 6, 8, 71, 15, 2, 6 }, { 1, 22, 6, 8, 86, 15, 2, 4 },
    { 1, 22, 6, 8, 79, 15, 2, 5 }, { 1, 22, 6, 8, 89, 15, 2, 4 }, { 1, 22, 6, 8, 58, 15, 0, 5 },
    { 1, 22, 6, 8, 80, 15, 2, 4 }, { 2, 23, 7, 8, 40, 16, 1, 7 }, { 2, 23, 7, 8, 46, 16, 0, 5 },
    { 2, 23, 7, 8, 13, 16, 3, 2 }, { 2, 23, 7, 8, 16, 16, 1, 2 }, { 2, 23, 7, 8, 19, 16, 3, 2 },
    { 2, 23, 7, 8, 23, 16, 3, 2 }, { 2, 23, 7, 8, 47, 16, 3, 7 }, { 2, 23, 7, 8, 2, 16, 1, 3 },
    { 2, 23, 7, 8, 5, 16, 1, 3 },  { 2, 23, 7, 8, 9, 16, 1, 3 },  { 2, 23, 7, 8, 75, 16, 3, 7 },
    { 2, 23, 7, 8, 45, 16, 1, 5 }, { 2, 23, 7, 8, 51, 16, 2, 5 }, { 2, 23, 7, 8, 59, 16, 3, 5 },
    { 2, 23, 7, 8, 64, 16, 3, 5 }, { 2, 24, 7, 8, 11, 17, 3, 2 }, { 2, 24, 7, 8, 14, 17, 3, 2 },
    { 2, 24, 7, 8, 17, 17, 3, 2 }, { 2, 24, 7, 8, 21, 17, 3, 2 }, { 2, 24, 7, 8, 24, 17, 1, 2 },
    { 2, 24, 7, 8, 48, 17, 1, 7 }, { 2, 24, 7, 8, 3, 17, 1, 3 },  { 2, 24, 7, 8, 6, 17, 1, 3 },
    { 2, 24, 7, 8, 73, 17, 3, 7 }, { 2, 24, 7, 8, 49, 17, 1, 7 }, { 2, 24, 7, 8, 52, 17, 2, 5 },
    { 2, 24, 7, 8, 60, 17, 3, 5 }, { 2, 25, 7, 8, 12, 18, 3, 2 }, { 2, 25, 7, 8, 15, 18, 3, 2 },
    { 2, 25, 7, 8, 18, 18, 3, 2 }, { 2, 25, 7, 8, 22, 18, 3, 2 }, { 2, 25, 7, 8, 25, 18, 3, 2 },
    { 2, 25, 7, 8, 1, 18, 1, 3 },  { 2, 25, 7, 8, 4, 18, 1, 3 },  { 2, 25, 7, 8, 8, 18, 1, 3 },
    { 2, 25, 7, 8, 74, 18, 3, 7 }, { 2, 25, 7, 8, 44, 18, 1, 5 }, { 2, 25, 7, 8, 53, 18, 2, 5 },
    { 2, 25, 7, 8, 54, 18, 2, 5 }, { 2, 25, 7, 8, 61, 18, 3, 5 }, { 2, 23, 7, 8, 26, 16, 3, 2 },
    { 2, 24, 7, 8, 27, 17, 3, 2 }, { 2, 25, 7, 8, 28, 18, 3, 2 }, { 2, 23, 7, 8, 29, 16, 3, 2 },
    { 2, 24, 7, 8, 30, 17, 1, 2 }, { 2, 25, 7, 8, 31, 18, 3, 2 }, { 2, 24, 7, 8, 32, 17, 3, 2 },
    { 2, 24, 7, 8, 33, 17, 1, 2 }, { 2, 25, 7, 8, 34, 18, 3, 2 }, { 4, 26, 1, 8, 0, 1, 0, 0 },
    { 4, 27, 1, 8, 0, 2, 0, 0 },   { 4, 28, 1, 8, 0, 3, 0, 0 },   { 4, 28, 1, 8, 0, 3, 0, 0 },
    { 4, 27, 1, 8, 0, 2, 0, 0 },   { 4, 31, 2, 8, 0, 6, 0, 0 },   { 4, 32, 2, 8, 0, 7, 0, 0 },
    { 4, 33, 2, 8, 0, 8, 0, 0 },   { 4, 34, 3, 8, 0, 9, 0, 0 },   { 0, 36, 5, 8, 0, 11, 0, 0 },
    { 0, 37, 5, 8, 0, 12, 0, 0 },  { 0, 38, 5, 8, 0, 13, 0, 0 },  { 1, 39, 6, 8, 0, 14, 0, 0 },
    { 1, 40, 6, 8, 0, 15, 0, 0 },  { 2, 41, 7, 8, 0, 16, 0, 0 },  { 2, 42, 7, 8, 0, 17, 0, 0 },
    { 2, 43, 7, 8, 0, 18, 0, 0 },
};

#else
#ifdef EU
const struct_080C9CBC gUnk_080C9CBC[] = {
    { 15, 44, 45, 8, 0, 0, 0, 0 }, { 4, 8, 1, 0, 0, 1, 2, 0 },    { 4, 9, 1, 0, 0, 2, 2, 0 },
    { 4, 10, 1, 0, 0, 3, 2, 0 },   { 4, 10, 1, 0, 0, 3, 2, 0 },   { 4, 9, 1, 0, 0, 2, 2, 0 },
    { 4, 13, 2, 0, 0, 6, 2, 0 },   { 4, 14, 2, 0, 0, 7, 2, 0 },   { 4, 15, 2, 0, 0, 8, 2, 0 },
    { 4, 16, 3, 0, 0, 9, 2, 0 },   { 0, 18, 5, 8, 20, 11, 3, 2 }, { 0, 18, 5, 8, 77, 11, 0, 4 },
    { 0, 18, 5, 8, 65, 11, 2, 5 }, { 0, 18, 5, 8, 7, 11, 1, 3 },  { 0, 18, 5, 8, 87, 11, 2, 0 },
    { 0, 18, 5, 8, 92, 11, 2, 8 }, { 0, 18, 5, 8, 37, 11, 0, 3 }, { 0, 18, 5, 8, 55, 11, 0, 5 },
    { 0, 18, 5, 8, 62, 11, 2, 5 }, { 0, 19, 5, 8, 63, 12, 0, 5 }, { 0, 19, 5, 8, 88, 12, 3, 4 },
    { 0, 19, 5, 8, 66, 12, 2, 5 }, { 0, 19, 5, 8, 10, 12, 3, 2 }, { 0, 20, 5, 8, 70, 13, 2, 6 },
    { 0, 19, 5, 8, 42, 12, 0, 5 }, { 0, 19, 5, 8, 38, 12, 0, 3 }, { 0, 19, 5, 8, 68, 12, 2, 6 },
    { 0, 20, 5, 8, 76, 13, 0, 4 }, { 0, 20, 5, 8, 91, 13, 3, 4 }, { 0, 20, 5, 8, 67, 13, 2, 5 },
    { 0, 20, 5, 8, 43, 13, 3, 5 }, { 0, 20, 5, 8, 41, 13, 1, 7 }, { 0, 20, 5, 8, 36, 13, 0, 3 },
    { 0, 20, 5, 8, 50, 13, 2, 7 }, { 1, 21, 6, 8, 39, 14, 0, 7 }, { 1, 21, 6, 8, 69, 14, 2, 6 },
    { 1, 21, 6, 8, 72, 14, 2, 6 }, { 1, 22, 6, 8, 82, 15, 2, 4 }, { 1, 22, 6, 8, 84, 15, 2, 4 },
    { 1, 21, 6, 8, 56, 14, 2, 5 }, { 1, 21, 6, 8, 78, 14, 2, 5 }, { 1, 21, 6, 8, 81, 14, 2, 4 },
    { 1, 21, 6, 8, 83, 14, 2, 4 }, { 1, 21, 6, 8, 85, 14, 2, 4 }, { 1, 21, 6, 8, 90, 14, 2, 4 },
    { 1, 22, 6, 8, 57, 15, 0, 5 }, { 1, 22, 6, 8, 71, 15, 2, 6 }, { 1, 22, 6, 8, 86, 15, 2, 4 },
    { 1, 22, 6, 8, 79, 15, 2, 5 }, { 1, 22, 6, 8, 89, 15, 3, 4 }, { 1, 22, 6, 8, 58, 15, 0, 5 },
    { 1, 22, 6, 8, 80, 15, 2, 4 }, { 2, 23, 7, 8, 40, 16, 1, 7 }, { 2, 23, 7, 8, 46, 16, 0, 5 },
    { 2, 23, 7, 8, 13, 16, 3, 2 }, { 2, 23, 7, 8, 16, 16, 1, 2 }, { 2, 23, 7, 8, 19, 16, 3, 2 },
    { 2, 23, 7, 8, 23, 16, 3, 2 }, { 2, 23, 7, 8, 47, 16, 3, 7 }, { 2, 23, 7, 8, 2, 16, 1, 3 },
    { 2, 23, 7, 8, 5, 16, 1, 3 },  { 2, 23, 7, 8, 9, 16, 1, 3 },  { 2, 23, 7, 8, 75, 16, 3, 7 },
    { 2, 23, 7, 8, 45, 16, 1, 5 }, { 2, 23, 7, 8, 51, 16, 2, 5 }, { 2, 23, 7, 8, 59, 16, 1, 5 },
    { 2, 23, 7, 8, 64, 16, 1, 5 }, { 2, 24, 7, 8, 11, 17, 3, 2 }, { 2, 24, 7, 8, 14, 17, 3, 2 },
    { 2, 24, 7, 8, 17, 17, 3, 2 }, { 2, 24, 7, 8, 21, 17, 3, 2 }, { 2, 24, 7, 8, 24, 17, 1, 2 },
    { 2, 24, 7, 8, 48, 17, 3, 7 }, { 2, 24, 7, 8, 3, 17, 1, 3 },  { 2, 24, 7, 8, 6, 17, 1, 3 },
    { 2, 24, 7, 8, 73, 17, 3, 7 }, { 2, 24, 7, 8, 49, 17, 1, 7 }, { 2, 24, 7, 8, 52, 17, 2, 5 },
    { 2, 24, 7, 8, 60, 17, 3, 5 }, { 2, 25, 7, 8, 12, 18, 3, 2 }, { 2, 25, 7, 8, 15, 18, 3, 2 },
    { 2, 25, 7, 8, 18, 18, 3, 2 }, { 2, 25, 7, 8, 22, 18, 3, 2 }, { 2, 25, 7, 8, 25, 18, 3, 2 },
    { 2, 25, 7, 8, 1, 18, 1, 3 },  { 2, 25, 7, 8, 4, 18, 1, 3 },  { 2, 25, 7, 8, 8, 18, 1, 3 },
    { 2, 25, 7, 8, 74, 18, 3, 7 }, { 2, 25, 7, 8, 44, 18, 1, 5 }, { 2, 25, 7, 8, 53, 18, 2, 5 },
    { 2, 25, 7, 8, 54, 18, 2, 5 }, { 2, 25, 7, 8, 61, 18, 1, 5 }, { 2, 23, 7, 8, 26, 16, 3, 2 },
    { 2, 24, 7, 8, 27, 17, 3, 2 }, { 2, 25, 7, 8, 28, 18, 3, 2 }, { 2, 23, 7, 8, 29, 16, 3, 2 },
    { 2, 24, 7, 8, 30, 17, 1, 2 }, { 2, 25, 7, 8, 31, 18, 3, 2 }, { 2, 24, 7, 8, 32, 17, 3, 2 },
    { 2, 24, 7, 8, 33, 17, 1, 2 }, { 2, 25, 7, 8, 34, 18, 3, 2 }, { 4, 26, 1, 8, 0, 1, 0, 0 },
    { 4, 27, 1, 8, 0, 2, 0, 0 },   { 4, 28, 1, 8, 0, 3, 0, 0 },   { 4, 28, 1, 8, 0, 3, 0, 0 },
    { 4, 27, 1, 8, 0, 2, 0, 0 },   { 4, 31, 2, 8, 0, 6, 0, 0 },   { 4, 32, 2, 8, 0, 7, 0, 0 },
    { 4, 33, 2, 8, 0, 8, 0, 0 },   { 4, 34, 3, 8, 0, 9, 0, 0 },   { 0, 36, 5, 8, 0, 11, 0, 0 },
    { 0, 37, 5, 8, 0, 12, 0, 0 },  { 0, 38, 5, 8, 0, 13, 0, 0 },  { 1, 39, 6, 8, 0, 14, 0, 0 },
    { 1, 40, 6, 8, 0, 15, 0, 0 },  { 2, 41, 7, 8, 0, 16, 0, 0 },  { 2, 42, 7, 8, 0, 17, 0, 0 },
    { 2, 43, 7, 8, 0, 18, 0, 0 },
};
#else
const struct_080C9CBC gUnk_080C9CBC[] = {
    { 15, 44, 45, 8, 0, 0, 0, 0 }, { 4, 8, 1, 0, 0, 1, 2, 0 },    { 4, 9, 1, 0, 0, 2, 2, 0 },
    { 4, 10, 1, 0, 0, 3, 2, 0 },   { 4, 10, 1, 0, 0, 3, 2, 0 },   { 4, 9, 1, 0, 0, 2, 2, 0 },
    { 4, 13, 2, 0, 0, 6, 2, 0 },   { 4, 14, 2, 0, 0, 7, 2, 0 },   { 4, 15, 2, 0, 0, 8, 2, 0 },
    { 4, 16, 3, 0, 0, 9, 2, 0 },   { 0, 18, 5, 8, 20, 11, 2, 2 }, { 0, 18, 5, 8, 77, 11, 0, 4 },
    { 0, 18, 5, 8, 65, 11, 2, 5 }, { 0, 18, 5, 8, 7, 11, 1, 3 },  { 0, 18, 5, 8, 87, 11, 2, 0 },
    { 0, 18, 5, 8, 92, 11, 2, 8 }, { 0, 18, 5, 8, 37, 11, 0, 3 }, { 0, 18, 5, 8, 55, 11, 0, 5 },
    { 0, 18, 5, 8, 62, 11, 2, 5 }, { 0, 19, 5, 8, 63, 12, 0, 5 }, { 0, 19, 5, 8, 88, 12, 2, 4 },
    { 0, 19, 5, 8, 66, 12, 2, 5 }, { 0, 19, 5, 8, 10, 12, 2, 2 }, { 0, 20, 5, 8, 70, 13, 2, 6 },
    { 0, 19, 5, 8, 42, 12, 0, 5 }, { 0, 19, 5, 8, 38, 12, 0, 3 }, { 0, 19, 5, 8, 68, 12, 2, 6 },
    { 0, 20, 5, 8, 76, 13, 0, 4 }, { 0, 20, 5, 8, 91, 13, 2, 4 }, { 0, 20, 5, 8, 67, 13, 2, 5 },
    { 0, 20, 5, 8, 43, 13, 2, 5 }, { 0, 20, 5, 8, 41, 13, 0, 7 }, { 0, 20, 5, 8, 36, 13, 0, 3 },
    { 0, 20, 5, 8, 50, 13, 2, 7 }, { 1, 21, 6, 8, 39, 14, 0, 7 }, { 1, 21, 6, 8, 69, 14, 2, 6 },
    { 1, 21, 6, 8, 72, 14, 2, 6 }, { 1, 22, 6, 8, 82, 15, 2, 4 }, { 1, 22, 6, 8, 84, 15, 2, 4 },
    { 1, 21, 6, 8, 56, 14, 2, 5 }, { 1, 21, 6, 8, 78, 14, 2, 5 }, { 1, 21, 6, 8, 81, 14, 2, 4 },
    { 1, 21, 6, 8, 83, 14, 2, 4 }, { 1, 21, 6, 8, 85, 14, 2, 4 }, { 1, 21, 6, 8, 90, 14, 2, 4 },
    { 1, 22, 6, 8, 57, 15, 0, 5 }, { 1, 22, 6, 8, 71, 15, 2, 6 }, { 1, 22, 6, 8, 86, 15, 2, 4 },
    { 1, 22, 6, 8, 79, 15, 2, 5 }, { 1, 22, 6, 8, 89, 15, 2, 4 }, { 1, 22, 6, 8, 58, 15, 0, 5 },
    { 1, 22, 6, 8, 80, 15, 2, 4 }, { 2, 23, 7, 8, 40, 16, 1, 7 }, { 2, 23, 7, 8, 46, 16, 0, 5 },
    { 2, 23, 7, 8, 13, 16, 2, 2 }, { 2, 23, 7, 8, 16, 16, 1, 2 }, { 2, 23, 7, 8, 19, 16, 2, 2 },
    { 2, 23, 7, 8, 23, 16, 2, 2 }, { 2, 23, 7, 8, 47, 16, 2, 7 }, { 2, 23, 7, 8, 2, 16, 1, 3 },
    { 2, 23, 7, 8, 5, 16, 1, 3 },  { 2, 23, 7, 8, 9, 16, 1, 3 },  { 2, 23, 7, 8, 75, 16, 2, 7 },
    { 2, 23, 7, 8, 45, 16, 1, 5 }, { 2, 23, 7, 8, 51, 16, 2, 5 }, { 2, 23, 7, 8, 59, 16, 2, 5 },
    { 2, 23, 7, 8, 64, 16, 2, 5 }, { 2, 24, 7, 8, 11, 17, 2, 2 }, { 2, 24, 7, 8, 14, 17, 2, 2 },
    { 2, 24, 7, 8, 17, 17, 2, 2 }, { 2, 24, 7, 8, 21, 17, 2, 2 }, { 2, 24, 7, 8, 24, 17, 1, 2 },
    { 2, 24, 7, 8, 48, 17, 1, 7 }, { 2, 24, 7, 8, 3, 17, 1, 3 },  { 2, 24, 7, 8, 6, 17, 1, 3 },
    { 2, 24, 7, 8, 73, 17, 2, 7 }, { 2, 24, 7, 8, 49, 17, 1, 7 }, { 2, 24, 7, 8, 52, 17, 2, 5 },
    { 2, 24, 7, 8, 60, 17, 2, 5 }, { 2, 25, 7, 8, 12, 18, 2, 2 }, { 2, 25, 7, 8, 15, 18, 2, 2 },
    { 2, 25, 7, 8, 18, 18, 2, 2 }, { 2, 25, 7, 8, 22, 18, 2, 2 }, { 2, 25, 7, 8, 25, 18, 2, 2 },
    { 2, 25, 7, 8, 1, 18, 1, 3 },  { 2, 25, 7, 8, 4, 18, 1, 3 },  { 2, 25, 7, 8, 8, 18, 1, 3 },
    { 2, 25, 7, 8, 74, 18, 2, 7 }, { 2, 25, 7, 8, 44, 18, 1, 5 }, { 2, 25, 7, 8, 53, 18, 2, 5 },
    { 2, 25, 7, 8, 54, 18, 2, 5 }, { 2, 25, 7, 8, 61, 18, 2, 5 }, { 2, 23, 7, 8, 26, 16, 2, 2 },
    { 2, 24, 7, 8, 27, 17, 2, 2 }, { 2, 25, 7, 8, 28, 18, 2, 2 }, { 2, 23, 7, 8, 29, 16, 2, 2 },
    { 2, 24, 7, 8, 30, 17, 1, 2 }, { 2, 25, 7, 8, 31, 18, 2, 2 }, { 2, 24, 7, 8, 32, 17, 2, 2 },
    { 2, 24, 7, 8, 33, 17, 1, 2 }, { 2, 25, 7, 8, 34, 18, 2, 2 }, { 4, 26, 1, 8, 0, 1, 0, 0 },
    { 4, 27, 1, 8, 0, 2, 0, 0 },   { 4, 28, 1, 8, 0, 3, 0, 0 },   { 4, 28, 1, 8, 0, 3, 0, 0 },
    { 4, 27, 1, 8, 0, 2, 0, 0 },   { 4, 31, 2, 8, 0, 6, 0, 0 },   { 4, 32, 2, 8, 0, 7, 0, 0 },
    { 4, 33, 2, 8, 0, 8, 0, 0 },   { 4, 34, 3, 8, 0, 9, 0, 0 },   { 0, 36, 5, 8, 0, 11, 0, 0 },
    { 0, 37, 5, 8, 0, 12, 0, 0 },  { 0, 38, 5, 8, 0, 13, 0, 0 },  { 1, 39, 6, 8, 0, 14, 0, 0 },
    { 1, 40, 6, 8, 0, 15, 0, 0 },  { 2, 41, 7, 8, 0, 16, 0, 0 },  { 2, 42, 7, 8, 0, 17, 0, 0 },
    { 2, 43, 7, 8, 0, 18, 0, 0 },
};
#endif
#endif

// For sub_080A4418
// TODO these are gGlobalGfxAndPalettes offsets with the size of 0x80
#ifdef EU
const u32 gUnk_080CA06C[] = { 139744, 139744, 140256, 140768, 141280, 141792, 142304, 142816, 143840, 144864, 145888,
                              146912, 147936, 148960, 149984, 151008, 152032, 153056, 154080, 155104, 156128, 157152,
                              158176, 159200, 160224, 161248, 143328, 144352, 145376, 146400, 147424, 148448, 149472,
                              150496, 151520, 152544, 153568, 154592, 155616, 156640, 157664, 158688, 159712, 160736 };
#else
const u32 gUnk_080CA06C[] = { 139808, 139808, 140320, 140832, 141344, 141856, 142368, 142880, 143904, 144928, 145952,
                              146976, 148000, 149024, 150048, 151072, 152096, 153120, 154144, 155168, 156192, 157216,
                              158240, 159264, 160288, 161312, 143392, 144416, 145440, 146464, 147488, 148512, 149536,
                              150560, 151584, 152608, 153632, 154656, 155680, 156704, 157728, 158752, 159776, 160800 };
#endif

// TODO maybe KinstoneFlag?
const u8 gUnk_080CA11C[] = {
    24, 45, 53, 54, 55, 57, 60, 68, 70, 71, 78, 80, 83, 85, 86, 88, 95, 96, 0, 0,
};

u32 sub_0801EA74(void) {
    s32 r = (s32)Random() % 18;
    u32 i;
    for (i = 0; i < 18; ++i) {
        u32 n = gUnk_080CA11C[r];
        if (!CheckKinstoneFused(n))
            return n;
        r = (r + 1) % 18;
    }
    return 0xF2;
}
