#include "entity.h"
#include "sound.h"
#include "functions.h"
#include "effects.h"
#include "asm.h"

extern u8 gUnk_08003E44;

void sub_0805FBE8(Entity*);
void sub_0805FC74(Entity*);

void PlayerItemSpiralBeam(Entity* this) {
    static void (*const actionFuncs[])(Entity*) = {
        sub_0805FBE8,
        sub_0805FC74,
    };
    actionFuncs[this->action](this);
}

void sub_0805FBE8(Entity* this) {
    static const Hitbox gUnk_08109AD0 = { 0, 0, { 4, 0, 0, 0 }, 6, 6 };
    CopyPosition(&gPlayerEntity, this);
    this->action++;
    this->spriteSettings.draw = TRUE;
    this->collisionFlags = gPlayerEntity.collisionFlags + 1;
    this->hitbox = (Hitbox*)&gUnk_08109AD0;
    this->speed = 0x380;
    this->animationState = this->animationState & 0x7f;
    if (this->collisionLayer == 2) {
        this->type2 = 1;
    }
    this->direction = this->animationState << 2;
    *(u32*)&this->field_0x6c = 0x3c;
    InitializeAnimation(this, (this->animationState >> 1) + 0xc);
    sub_0801766C(this);
    LinearMoveUpdate(this);
    sub_0805FC74(this);
    SoundReq(SFX_ITEM_SWORD_BEAM);
}

void sub_0805FC74(Entity* this) {
    int iVar1;

    if (--*(int*)&this->field_0x6c != -1) {
        GetNextFrame(this);
        LinearMoveUpdate(this);
        this->timer++;
        if (this->type2 == 0) {
            sub_0800451C(this);
        }
        if (!sub_080B1BA4(COORD_TO_TILE(this), gPlayerEntity.collisionLayer, 0x80) &&
            sub_080040D8(this, &gUnk_08003E44, this->x.HALF.HI, this->y.HALF.HI)) {
            CreateFx(this, FX_SWORD_MAGIC, 0);
            DeleteThisEntity();
        }
        if (this->contactFlags != 0) {
            CreateFx(this, FX_SWORD_MAGIC, 0);
            DeleteThisEntity();
        }
    } else {
        DeleteThisEntity();
    }
}
