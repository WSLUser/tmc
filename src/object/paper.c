/**
 * @file paper.c
 * @ingroup Objects
 *
 * @brief Paper object
 */

#define NENT_DEPRECATED
#include "functions.h"
#include "global.h"
#include "object.h"

void Paper_Init(Entity*);
void Paper_Action1(Entity*);
void Paper_Type0(Entity*);
void Paper_Type1(Entity*);
void Paper_Type2(Entity*);

void Paper(Entity* this) {
    static void (*const Paper_Actions[])(Entity*) = {
        Paper_Init,
        Paper_Action1,
    };
    Paper_Actions[this->action](this);
}

void Paper_Init(Entity* this) {
    static void (*const Paper_Types[])(Entity*) = {
        Paper_Type0,
        Paper_Type1,
        Paper_Type2,
    };
    Paper_Types[this->type](this);
}

void Paper_Type0(Entity* this) {
    this->action = 1;
    this->spriteRendering.b3 = 1;
    this->z.HALF.HI -= 8;
    if (this->type2 != 1) {
        if (this->type2 != 2) {
            return;
        }
        this->spriteSettings.draw = 0;
    }
    SetTile(0x4051, COORD_TO_TILE(this), 1);
}

void Paper_Type1(Entity* this) {
    this->action = 1;
    this->frameIndex = this->type2;
    switch (this->type2) {
        case 0:
            this->spriteOffsetY = 2;
            break;
        case 2:
            this->spriteOffsetY = -2;
            break;
    }
}

void Paper_Type2(Entity* this) {
    this->action = 1;
    this->y.HALF.HI++;
    this->spriteOffsetY = -1;
    SetTile(0x4051, COORD_TO_TILE(this) - 1, 1);
    SetTile(0x4051, COORD_TO_TILE(this), 1);
}

void Paper_Action1(Entity* this) {
    if (this->type == 0) {
        if ((gPlayerEntity.y.HALF.HI < this->y.HALF.HI) || (gPlayerEntity.y.HALF.HI) > this->y.HALF.HI + 0x18) {
            this->spriteRendering.b3 = 1;
        } else {
            this->spriteRendering.b3 = 2;
        }
    }
}
