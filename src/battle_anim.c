#include "global.h"
#include "battle.h"
#include "battle_anim.h"
#include "sprite.h"
#include "contest.h"
#include "m4a.h"
#include "pokemon.h"
#include "battle_interface.h"
#include "task.h"
#include "decompress.h"
#include "sound.h"
#include "dma3.h"
#include "bg.h"
#include "gpu_regs.h"
#include "palette.h"

// sprites start at 10000 and thus must be subtracted of 10000 to account for the true index.
#define GET_TRUE_SPRITE_INDEX(i) ((i - 10000))
#define SCRIPT_READ_32(ptr) ((ptr)[0] | (ptr)[1] << 8 | (ptr)[2] << 16 | (ptr)[3] << 24)
#define SCRIPT_READ_32_(ptr) (((ptr)[0]) + ((ptr)[1] << 8) + ((ptr)[2] << 16) + ((ptr)[3] << 24))
#define SCRIPT_READ_16(ptr) ((ptr)[0] | ((ptr)[1] << 8))
#define SCRIPT_READ_16_(ptr) ((ptr)[0] + ((ptr)[1] << 8))

#define ANIM_SPRITE_INDEX_COUNT 8
#define ANIM_ARGS_COUNT 8

extern u8 gBankAttacker;
extern u8 gBankTarget;
extern u16 gBattle_WIN0H;
extern u16 gBattle_WIN0V;
extern u16 gBattle_WIN1H;
extern u16 gBattle_WIN1V;
extern u16 gBattle_BG1_X;
extern u16 gBattle_BG1_Y;
extern u16 gBattle_BG2_X;
extern u16 gBattle_BG2_Y;
extern u16 gBattlePartyID[BATTLE_BANKS_COUNT];
extern u8 gBankSpriteIds[BATTLE_BANKS_COUNT];
extern struct MusicPlayerInfo gMPlay_BGM;
extern struct MusicPlayerInfo gMPlay_SE1;
extern struct MusicPlayerInfo gMPlay_SE2;

EWRAM_DATA const u8 *gBattleAnimScriptPtr = NULL;
EWRAM_DATA const u8 *gBattleAnimScriptRetAddr = NULL;
EWRAM_DATA void (*gAnimScriptCallback)(void) = NULL;
EWRAM_DATA s8 gAnimFramesToWait = 0;
EWRAM_DATA bool8 gAnimScriptActive = FALSE;
EWRAM_DATA u8 gAnimVisualTaskCount = 0;
EWRAM_DATA u8 gAnimSoundTaskCount = 0;
EWRAM_DATA struct DisableStruct *gAnimDisableStructPtr = NULL;
EWRAM_DATA u32 gAnimMoveDmg = 0;
EWRAM_DATA u16 gAnimMovePower = 0;
EWRAM_DATA u16 gAnimSpriteIndexArray[ANIM_SPRITE_INDEX_COUNT] = {0};
EWRAM_DATA u8 gAnimFriendship = 0;
EWRAM_DATA u16 gWeatherMoveAnim = 0;
EWRAM_DATA s16 gBattleAnimArgs[ANIM_ARGS_COUNT] = {0};
EWRAM_DATA u16 gSoundAnimFramesToWait = 0;
EWRAM_DATA u8 gMonAnimTaskIdArray[2] = {0};
EWRAM_DATA u8 gUnknown_02038432 = 0;
EWRAM_DATA u8 gUnknown_02038433 = 0;
EWRAM_DATA u16 gAnimMoveIndex = 0; // set but unused.
EWRAM_DATA u8 gAnimBankAttacker = 0;
EWRAM_DATA u8 gAnimBankTarget = 0;
EWRAM_DATA u16 gAnimSpeciesByBanks[BATTLE_BANKS_COUNT] = {0};
EWRAM_DATA u8 gUnknown_02038440 = 0;

extern void (* const sScriptCmdTable[])(void);
extern const u16 gUnknown_082C8D64[];
extern const u8 * const gBattleAnims_Moves[];
extern const struct CompressedSpriteSheet gBattleAnimPicTable[];
extern const struct CompressedSpritePalette gBattleAnimPaletteTable[];

extern void sub_80A8278(void); // rom_80A5C6C.s
extern void sub_80A6B30(struct UnknownAnimStruct2*); // rom_80A5C6C.s
extern void sub_80A6B90(struct UnknownAnimStruct2*, u32 arg1); // rom_80A5C6C.s
extern u8 sub_80A82E4(u8 bank); // rom_80A5C6C.s
extern u8 sub_80A5C6C(u8 bank, u8 attributeId); // rom_80A5C6C.s
extern bool8 AnimBankSpriteExists(u8 bank); // rom_80A5C6C.s

// this file's functions
void RunAnimScriptCommand(void);
void task_pA_ma0A_obj_to_bg_pal(u8 taskId);
void sub_80A46A0(void);

void ClearBattleAnimationVars(void)
{
    s32 i;

    gAnimFramesToWait = 0;
    gAnimScriptActive = FALSE;
    gAnimVisualTaskCount = 0;
    gAnimSoundTaskCount = 0;
    gAnimDisableStructPtr = NULL;
    gAnimMoveDmg = 0;
    gAnimMovePower = 0;
    gAnimFriendship = 0;

    // clear index array.
    for (i = 0; i < ANIM_SPRITE_INDEX_COUNT; i++)
        gAnimSpriteIndexArray[i] |= 0xFFFF;

    // clear anim args.
    for (i = 0; i < ANIM_ARGS_COUNT; i++)
        gBattleAnimArgs[i] = 0;

    gMonAnimTaskIdArray[0] = 0xFF;
    gMonAnimTaskIdArray[1] = 0xFF;
    gUnknown_02038432 = 0;
    gUnknown_02038433 = 0;
    gAnimMoveIndex = 0;
    gAnimBankAttacker = 0;
    gAnimBankTarget = 0;
    gUnknown_02038440 = 0;
}

void DoMoveAnim(u16 move)
{
    gAnimBankAttacker = gBankAttacker;
    gAnimBankTarget = gBankTarget;
    DoBattleAnim(gBattleAnims_Moves, move, TRUE);
}

void DoBattleAnim(const u8 *const animsTable[], u16 tableId, bool8 isMoveAnim)
{
    s32 i;

    if (!IsContest())
    {
        sub_80A8278();
        sub_8072A88(0);
        for (i = 0; i < BATTLE_BANKS_COUNT; i++)
        {
            if (GetBankSide(i) != 0)
                gAnimSpeciesByBanks[i] = GetMonData(&gEnemyParty[gBattlePartyID[i]], MON_DATA_SPECIES);
            else
                gAnimSpeciesByBanks[i] = GetMonData(&gPlayerParty[gBattlePartyID[i]], MON_DATA_SPECIES);
        }
    }
    else
    {
        for (i = 0; i < 4; i++)
            gAnimSpeciesByBanks[i] = gContestResources->field_18->field_0;
    }

    if (!isMoveAnim)
        gAnimMoveIndex = 0;
    else
        gAnimMoveIndex = tableId;

    for (i = 0; i < ANIM_ARGS_COUNT; i++)
        gBattleAnimArgs[i] = 0;

    gMonAnimTaskIdArray[0] = 0xFF;
    gMonAnimTaskIdArray[1] = 0xFF;
    gBattleAnimScriptPtr = animsTable[tableId];
    gAnimScriptActive = TRUE;
    gAnimFramesToWait = 0;
    gAnimScriptCallback = RunAnimScriptCommand;

    for (i = 0; i < ANIM_SPRITE_INDEX_COUNT; i++)
        gAnimSpriteIndexArray[i] |= 0xFFFF;

    if (isMoveAnim)
    {
        for (i = 0; gUnknown_082C8D64[i] != 0xFFFF; i++)
        {
            if (tableId == gUnknown_082C8D64[i])
            {
                m4aMPlayVolumeControl(&gMPlay_BGM, 0xFFFF, 128);
                break;
            }
        }
    }

    gBattle_WIN0H = 0;
    gBattle_WIN0V = 0;
    gBattle_WIN1H = 0;
    gBattle_WIN1V = 0;
}

void DestroyAnimSprite(struct Sprite *sprite)
{
    FreeSpriteOamMatrix(sprite);
    DestroySprite(sprite);
    gAnimVisualTaskCount--;
}

void DestroyAnimVisualTask(u8 taskId)
{
    DestroyTask(taskId);
    gAnimVisualTaskCount--;
}

void DestroyAnimSoundTask(u8 taskId)
{
    DestroyTask(taskId);
    gAnimSoundTaskCount--;
}

/*static*/ void AddSpriteIndex(u16 index)
{
    s32 i;

    for (i = 0; i < ANIM_SPRITE_INDEX_COUNT; i++)
    {
        if (gAnimSpriteIndexArray[i] == 0xFFFF)
        {
            gAnimSpriteIndexArray[i] = index;
            return;
        }
    }
}

/*static*/ void ClearSpriteIndex(u16 index)
{
    s32 i;

    for (i = 0; i < ANIM_SPRITE_INDEX_COUNT; i++)
    {
        if (gAnimSpriteIndexArray[i] == index)
        {
            gAnimSpriteIndexArray[i] |= 0xFFFF;
            return;
        }
    }
}

/*static*/ void WaitAnimFrameCount(void)
{
    if (gAnimFramesToWait <= 0)
    {
        gAnimScriptCallback = RunAnimScriptCommand;
        gAnimFramesToWait = 0;
    }
    else
    {
        gAnimFramesToWait--;
    }
}

/*static*/ void RunAnimScriptCommand(void)
{
    do
    {
        sScriptCmdTable[gBattleAnimScriptPtr[0]]();
    } while (gAnimFramesToWait == 0 && gAnimScriptActive);
}

/*static*/ void ScriptCmd_loadspritegfx(void)
{
    u16 index;

    gBattleAnimScriptPtr++;
    index = SCRIPT_READ_16(gBattleAnimScriptPtr);
    LoadCompressedObjectPicUsingHeap(&gBattleAnimPicTable[GET_TRUE_SPRITE_INDEX(index)]);
    LoadCompressedObjectPaletteUsingHeap(&gBattleAnimPaletteTable[GET_TRUE_SPRITE_INDEX(index)]);
    gBattleAnimScriptPtr += 2;
    AddSpriteIndex(GET_TRUE_SPRITE_INDEX(index));
    gAnimFramesToWait = 1;
    gAnimScriptCallback = WaitAnimFrameCount;
}

/*static*/ void ScriptCmd_unloadspritegfx(void)
{
    u16 index;

    gBattleAnimScriptPtr++;
    index = SCRIPT_READ_16(gBattleAnimScriptPtr);
    FreeSpriteTilesByTag(gBattleAnimPicTable[GET_TRUE_SPRITE_INDEX(index)].tag);
    FreeSpritePaletteByTag(gBattleAnimPicTable[GET_TRUE_SPRITE_INDEX(index)].tag);
    gBattleAnimScriptPtr += 2;
    ClearSpriteIndex(GET_TRUE_SPRITE_INDEX(index));
}

/*static*/ void ScriptCmd_createsprite(void)
{
    s32 i;
    const struct SpriteTemplate *template;
    u8 argVar;
    u8 argsCount;
    s16 subpriority;

    gBattleAnimScriptPtr++;
    template = (const struct SpriteTemplate *)(SCRIPT_READ_32_(gBattleAnimScriptPtr));
    gBattleAnimScriptPtr += 4;

    argVar = gBattleAnimScriptPtr[0];
    gBattleAnimScriptPtr++;

    argsCount = gBattleAnimScriptPtr[0];
    gBattleAnimScriptPtr++;
    for (i = 0; i < argsCount; i++)
    {
        gBattleAnimArgs[i] = SCRIPT_READ_16(gBattleAnimScriptPtr);
        gBattleAnimScriptPtr += 2;
    }

    if (argVar & 0x80)
    {
        argVar ^= 0x80;
        if (argVar >= 0x40)
            argVar -= 0x40;
        else
            argVar *= -1;

        subpriority = sub_80A82E4(gAnimBankTarget) + (s8)(argVar);
    }
    else
    {
        if (argVar >= 0x40)
            argVar -= 0x40;
        else
            argVar *= -1;

        subpriority = sub_80A82E4(gAnimBankAttacker) + (s8)(argVar);
    }

    if (subpriority < 3)
        subpriority = 3;

    CreateSpriteAndAnimate(template, sub_80A5C6C(gAnimBankTarget, 2), sub_80A5C6C(gAnimBankTarget, 3), subpriority);
    gAnimVisualTaskCount++;
}

/*static*/ void ScriptCmd_createvisualtask(void)
{
    TaskFunc taskFunc;
    u8 taskPriority;
    u8 taskId;
    u8 numArgs;
    s32 i;

    gBattleAnimScriptPtr++;

    taskFunc = (TaskFunc)SCRIPT_READ_32_(gBattleAnimScriptPtr);
    gBattleAnimScriptPtr += 4;

    taskPriority = gBattleAnimScriptPtr[0];
    gBattleAnimScriptPtr++;

    numArgs = gBattleAnimScriptPtr[0];
    gBattleAnimScriptPtr++;

    for (i = 0; i < numArgs; i++)
    {
        gBattleAnimArgs[i] = SCRIPT_READ_16(gBattleAnimScriptPtr);
        gBattleAnimScriptPtr += 2;
    }

    taskId = CreateTask(taskFunc, taskPriority);
    taskFunc(taskId);
    gAnimVisualTaskCount++;
}

/*static*/ void ScriptCmd_delay(void)
{
    gBattleAnimScriptPtr++;
    gAnimFramesToWait = gBattleAnimScriptPtr[0];
    if (gAnimFramesToWait == 0)
        gAnimFramesToWait = -1;
    gBattleAnimScriptPtr++;
    gAnimScriptCallback = WaitAnimFrameCount;
}

// wait for visual tasks to finish.
/*static*/ void ScriptCmd_waitforvisualfinish(void)
{
    if (gAnimVisualTaskCount == 0)
    {
        gBattleAnimScriptPtr++;
        gAnimFramesToWait = 0;
    }
    else
    {
        gAnimFramesToWait = 1;
    }
}

/*static*/ void ScriptCmd_hang1(void)
{
}

/*static*/ void ScriptCmd_hang2(void)
{
}

/*static*/ void ScriptCmd_end(void)
{
    s32 i;
    bool32 continuousAnim = FALSE;

    // keep waiting as long as there is animations to be done.
    if (gAnimVisualTaskCount != 0 || gAnimSoundTaskCount != 0
     || gMonAnimTaskIdArray[0] != 0xFF || gMonAnimTaskIdArray[1] != 0xFF)
    {
        gSoundAnimFramesToWait = 0;
        gAnimFramesToWait = 1;
        return;
    }

    // finish the sound effects.
    if (IsSEPlaying())
    {
        if (++gSoundAnimFramesToWait <= 90) // wait 90 frames, then halt the sound effect.
        {
            gAnimFramesToWait = 1;
            return;
        }
        else
        {
            m4aMPlayStop(&gMPlay_SE1);
            m4aMPlayStop(&gMPlay_SE2);
        }
    }

    // the SE has halted, so set the SE Frame Counter to 0 and continue.
    gSoundAnimFramesToWait = 0;

    for (i = 0; i < ANIM_SPRITE_INDEX_COUNT; i++)
    {
        if (gAnimSpriteIndexArray[i] != 0xFFFF)
        {
            FreeSpriteTilesByTag(gBattleAnimPicTable[gAnimSpriteIndexArray[i]].tag);
            FreeSpritePaletteByTag(gBattleAnimPicTable[gAnimSpriteIndexArray[i]].tag);
            gAnimSpriteIndexArray[i] |= 0xFFFF; // set terminator.
        }
    }

    if (!continuousAnim) // may have been used for debug?
    {
        m4aMPlayVolumeControl(&gMPlay_BGM, 0xFFFF, 256);
        if (!IsContest())
        {
            sub_80A8278();
            sub_8072A88(1);
        }
        gAnimScriptActive = FALSE;
    }
}

/*static*/ void ScriptCmd_playse(void)
{
    gBattleAnimScriptPtr++;
    PlaySE(SCRIPT_READ_16(gBattleAnimScriptPtr));
    gBattleAnimScriptPtr += 2;
}

#define t1_MONBG_BANK           0
#define t1_MON_IN_BG2           1
#define t1_CREATE_ANOTHER_TASK  2
#define t1_IS_SECONDMON_BG      3

#define t2_BANK_SPRITE_ID       0
#define t2_MON_IN_BG2           5
#define t2_MONBG_BANK           6

/*static*/ void sub_80A40F4(u8 taskId)
{
    u8 newTaskId;

    s16 *selfData = gTasks[taskId].data;
    u8 bankSpriteId = gBankSpriteIds[selfData[t1_MONBG_BANK]];
    gSprites[bankSpriteId].invisible = 1;

    if (!selfData[t1_CREATE_ANOTHER_TASK])
    {
        DestroyAnimVisualTask(taskId);
        return;
    }

    newTaskId = CreateTask(task_pA_ma0A_obj_to_bg_pal, 10);
    gTasks[newTaskId].data[t2_BANK_SPRITE_ID] = bankSpriteId;
    gTasks[newTaskId].data[1] = gSprites[bankSpriteId].pos1.x + gSprites[bankSpriteId].pos2.x;
    gTasks[newTaskId].data[2] = gSprites[bankSpriteId].pos1.y + gSprites[bankSpriteId].pos2.y;

    if (!selfData[t1_MON_IN_BG2])
    {
        gTasks[newTaskId].data[3] = gBattle_BG1_X;
        gTasks[newTaskId].data[4] = gBattle_BG1_Y;
    }
    else
    {
        gTasks[newTaskId].data[3] = gBattle_BG2_X;
        gTasks[newTaskId].data[4] = gBattle_BG2_Y;
    }

    gTasks[newTaskId].data[t2_MON_IN_BG2] = selfData[t1_MON_IN_BG2];
    gTasks[newTaskId].data[t2_MONBG_BANK] = selfData[t1_MONBG_BANK];
    gMonAnimTaskIdArray[selfData[t1_IS_SECONDMON_BG]] = newTaskId;
    DestroyAnimVisualTask(taskId);
}

/*static*/ void ScriptCmd_monbg(void)
{
    bool8 toBG_2;
    u8 taskId;
    u8 bank;
    u8 animBank;

    gBattleAnimScriptPtr++;

    animBank = gBattleAnimScriptPtr[0];
    if (animBank & ANIM_BANK_TARGET)
        bank = gAnimBankTarget;
    else
        bank = gAnimBankAttacker;

    if (IsAnimBankSpriteVisible(bank))
    {
        u8 identity = GetBankIdentity(bank);
        if (identity == IDENTITY_OPPONENT_MON1 || identity == IDENTITY_PLAYER_MON2 || IsContest())
            toBG_2 = FALSE;
        else
            toBG_2 = TRUE;

        sub_80A438C(bank, toBG_2, FALSE);
        taskId = CreateTask(sub_80A40F4, 10);
        gAnimVisualTaskCount++;
        gTasks[taskId].data[t1_MONBG_BANK] = bank;
        gTasks[taskId].data[t1_MON_IN_BG2] = toBG_2;
        gTasks[taskId].data[t1_CREATE_ANOTHER_TASK] = TRUE;
        gTasks[taskId].data[t1_IS_SECONDMON_BG] = 0;

    }

    bank ^= BIT_MON;
    if (IsAnimBankSpriteVisible(bank))
    {
        u8 identity = GetBankIdentity(bank);
        if (identity == IDENTITY_OPPONENT_MON1 || identity == IDENTITY_PLAYER_MON2 || IsContest())
            toBG_2 = FALSE;
        else
            toBG_2 = TRUE;

        sub_80A438C(bank, toBG_2, FALSE);
        taskId = CreateTask(sub_80A40F4, 10);
        gAnimVisualTaskCount++;
        gTasks[taskId].data[0] = bank;
        gTasks[taskId].data[1] = toBG_2;
        gTasks[taskId].data[t1_CREATE_ANOTHER_TASK] = TRUE;
        gTasks[taskId].data[t1_IS_SECONDMON_BG] = 1;
    }

    gBattleAnimScriptPtr++;
    gAnimFramesToWait = 1;
    gAnimScriptCallback = WaitAnimFrameCount;
}

bool8 IsAnimBankSpriteVisible(u8 bank)
{
    if (IsContest())
    {
        if (bank == gAnimBankAttacker)
            return TRUE;
        else
            return FALSE;
    }
    if (!AnimBankSpriteExists(bank))
        return FALSE;
    if (IsContest())
        return TRUE; // this line wont ever be reached.
    if (!gBattleSpritesDataPtr->bankData[bank].invisible || !gSprites[gBankSpriteIds[bank]].invisible)
        return TRUE;

    return FALSE;
}

void sub_80A438C(u8 bank, bool8 toBG_2, bool8 setSpriteInvisible)
{
    struct UnknownAnimStruct2 unknownStruct;
    u8 bankSpriteId;

    if (!toBG_2)
    {
        u8 bankIdentity;

        if (IsContest() == TRUE)
        {
            RequestDma3Fill(0, (void*)(VRAM + 0x8000), 0x2000, 1);
            RequestDma3Fill(0xFF, (void*)(VRAM + 0xF000), 0x1000, 0);
        }
        else
        {
            RequestDma3Fill(0, (void*)(VRAM + 0x4000), 0x2000, 1);
            RequestDma3Fill(0xFF, (void*)(VRAM + 0xe000), 0x1000, 0);
        }

        sub_80A6B30(&unknownStruct);
        CpuFill16(0, unknownStruct.unk0, 0x1000);
        CpuFill16(0xFF, unknownStruct.unk4, 0x800);

        SetAnimBgAttribute(1, BG_ANIM_PRIORITY, 2);
        SetAnimBgAttribute(1, BG_ANIM_SCREEN_SIZE, 1);
        SetAnimBgAttribute(1, BG_ANIM_AREA_OVERFLOW_MODE, 0);

        bankSpriteId = gBankSpriteIds[bank];

        gBattle_BG1_X =  -(gSprites[bankSpriteId].pos1.x + gSprites[bankSpriteId].pos2.x) + 0x20;
        if (IsContest() && IsSpeciesNotUnown(gContestResources->field_18->field_0))
            gBattle_BG1_X--;

        gBattle_BG1_Y =  -(gSprites[bankSpriteId].pos1.y + gSprites[bankSpriteId].pos2.y) + 0x20;
        if (setSpriteInvisible)
            gSprites[gBankSpriteIds[bank]].invisible = 1;

        SetGpuReg(REG_OFFSET_BG1HOFS, gBattle_BG1_X);
        SetGpuReg(REG_OFFSET_BG1VOFS, gBattle_BG1_Y);

        LoadPalette(&gPlttBufferUnfaded[0x100 + bank * 16], unknownStruct.unk8 * 16, 0x20);
        CpuCopy32(&gPlttBufferUnfaded[0x100 + bank * 16], (void*)(BG_PLTT + unknownStruct.unk8 * 32), 0x20);

        if (IsContest())
            bankIdentity = 0;
        else
            bankIdentity = GetBankIdentity(bank);

        sub_8118FBC(1, 0, 0, bankIdentity, unknownStruct.unk8, unknownStruct.unk0, unknownStruct.unk4, unknownStruct.unkA);

        if (IsContest())
            sub_80A46A0();
    }
    else
    {
        RequestDma3Fill(0, (void*)(VRAM + 0x6000), 0x2000, 1);
        RequestDma3Fill(0, (void*)(VRAM + 0xF000), 0x1000, 1);
        sub_80A6B90(&unknownStruct, 2);
        CpuFill16(0, unknownStruct.unk0 + 0x1000, 0x1000);
        CpuFill16(0, unknownStruct.unk4 + 0x400, 0x800);
        SetAnimBgAttribute(2, BG_ANIM_PRIORITY, 2);
        SetAnimBgAttribute(2, BG_ANIM_SCREEN_SIZE, 1);
        SetAnimBgAttribute(2, BG_ANIM_AREA_OVERFLOW_MODE, 0);

        bankSpriteId = gBankSpriteIds[bank];

        gBattle_BG2_X =  -(gSprites[bankSpriteId].pos1.x + gSprites[bankSpriteId].pos2.x) + 0x20;
        gBattle_BG2_Y =  -(gSprites[bankSpriteId].pos1.y + gSprites[bankSpriteId].pos2.y) + 0x20;

        if (setSpriteInvisible)
            gSprites[gBankSpriteIds[bank]].invisible = 1;

        SetGpuReg(REG_OFFSET_BG2HOFS, gBattle_BG2_X);
        SetGpuReg(REG_OFFSET_BG2VOFS, gBattle_BG2_Y);

        LoadPalette(&gPlttBufferUnfaded[0x100 + bank * 16], 0x90, 0x20);
        CpuCopy32(&gPlttBufferUnfaded[0x100 + bank * 16], (void*)(BG_PLTT + 0x120), 0x20);

        sub_8118FBC(2, 0, 0, GetBankIdentity(bank), unknownStruct.unk8, unknownStruct.unk0 + 0x1000, unknownStruct.unk4 + 0x400, unknownStruct.unkA);
    }
}

