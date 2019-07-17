//-------------------------------------------------------------------------
/*
Copyright (C) 2010-2019 EDuke32 developers and contributors
Copyright (C) 2019 Nuke.YKT

This file is part of NBlood.

NBlood is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License version 2
as published by the Free Software Foundation.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/
//-------------------------------------------------------------------------
#include <string.h>
#include "build.h"
#include "common_game.h"

#include "blood.h"
#include "db.h"
#include "eventq.h"
#include "levels.h"
#include "loadsave.h"
#include "sfx.h"
#include "sound.h"
#include "seq.h"
#include "gameutil.h"
#include "actor.h"
#include "tile.h"

#define kMaxClients 256
#define kMaxSequences 1024

static ACTIVE activeList[kMaxSequences];
static int activeCount = 0;
static int nClients = 0;
static void(*clientCallback[kMaxClients])(int, int);

int seqRegisterClient(void(*pClient)(int, int))
{
    dassert(nClients < kMaxClients);
    clientCallback[nClients] = pClient;
    return nClients++;
}

void Seq::Preload(void)
{
    if (memcmp(signature, "SEQ\x1a", 4) != 0)
        ThrowError("Invalid sequence");
    if ((version & 0xff00) != 0x300)
        ThrowError("Obsolete sequence version");
    for (int i = 0; i < nFrames; i++)
        tilePreloadTile(seqGetTile(&frames[i]));
}

void Seq::Precache(void)
{
    if (memcmp(signature, "SEQ\x1a", 4) != 0)
        ThrowError("Invalid sequence");
    if ((version & 0xff00) != 0x300)
        ThrowError("Obsolete sequence version");
    for (int i = 0; i < nFrames; i++)
        tilePrecacheTile(seqGetTile(&frames[i]));
}

void seqPrecacheId(int id)
{
    DICTNODE *hSeq = gSysRes.Lookup(id, "SEQ");
    if (!hSeq)
        return;
    Seq *pSeq = (Seq*)gSysRes.Lock(hSeq);
    pSeq->Precache();
    gSysRes.Unlock(hSeq);
}

SEQINST siWall[kMaxXWalls];
SEQINST siCeiling[kMaxXSectors];
SEQINST siFloor[kMaxXSectors];
SEQINST siSprite[kMaxXSprites];
SEQINST siMasked[kMaxXWalls];

void UpdateSprite(int nXSprite, SEQFRAME *pFrame)
{
    dassert(nXSprite > 0 && nXSprite < kMaxXSprites);
    int nSprite = xsprite[nXSprite].reference;
    dassert(nSprite >= 0 && nSprite < kMaxSprites);
    spritetype *pSprite = &sprite[nSprite];
    dassert(pSprite->extra == nXSprite);
    if (pSprite->hitag & 2)
    {
        if (tilesiz[pSprite->picnum].y != tilesiz[seqGetTile(pFrame)].y || picanm[pSprite->picnum].yofs != picanm[seqGetTile(pFrame)].yofs
            || (pFrame->at3_0 && pFrame->at3_0 != pSprite->yrepeat))
            pSprite->hitag |= 4;
    }
    pSprite->picnum = seqGetTile(pFrame);
    if (pFrame->at5_0)
        pSprite->pal = pFrame->at5_0;
    pSprite->shade = pFrame->at4_0;
    
    int scale = xsprite[nXSprite].scale; // SEQ size scaling
    if (pFrame->at2_0) {
        if (scale < 0) pSprite->xrepeat = pFrame->at2_0 / abs(scale);
        else if (scale > 0) pSprite->xrepeat = pFrame->at2_0 * scale;
        else pSprite->xrepeat = pFrame->at2_0;
    }

    if (pFrame->at3_0) {
        if (scale < 0) pSprite->yrepeat = pFrame->at3_0 / abs(scale);
        else if (scale > 0) pSprite->yrepeat = pFrame->at3_0 * scale;
        else pSprite->yrepeat = pFrame->at3_0;
    }

    if (pFrame->at1_4)
        pSprite->cstat |= 2;
    else
        pSprite->cstat &= ~2;
    if (pFrame->at1_5)
        pSprite->cstat |= 512;
    else
        pSprite->cstat &= ~512;
    if (pFrame->at1_6)
        pSprite->cstat |= 1;
    else
        pSprite->cstat &= ~1;
    if (pFrame->at1_7)
        pSprite->cstat |= 256;
    else
        pSprite->cstat &= ~256;
    if (pFrame->at6_2)
        pSprite->cstat |= 32768;
    else
        pSprite->cstat &= (unsigned short)~32768;
    if (pFrame->at6_0)
        pSprite->cstat |= 4096;
    else
        pSprite->cstat &= ~4096;
    if (pFrame->at5_6)
        pSprite->hitag |= 256;
    else
        pSprite->hitag &= ~256;
    if (pFrame->at5_7)
        pSprite->hitag |= 8;
    else
        pSprite->hitag &= ~8;
    if (pFrame->at6_3)
        pSprite->hitag |= 1024;
    else
        pSprite->hitag &= ~1024;
    if (pFrame->at6_4)
        pSprite->hitag |= 2048;
    else
        pSprite->hitag &= ~2048;
}

void UpdateWall(int nXWall, SEQFRAME *pFrame)
{
    dassert(nXWall > 0 && nXWall < kMaxXWalls);
    int nWall = xwall[nXWall].reference;
    dassert(nWall >= 0 && nWall < kMaxWalls);
    walltype *pWall = &wall[nWall];
    dassert(pWall->extra == nXWall);
    pWall->picnum = seqGetTile(pFrame);
    if (pFrame->at5_0)
        pWall->pal = pFrame->at5_0;
    if (pFrame->at1_4)
        pWall->cstat |= 128;
    else
        pWall->cstat &= ~128;
    if (pFrame->at1_5)
        pWall->cstat |= 512;
    else
        pWall->cstat &= ~512;
    if (pFrame->at1_6)
        pWall->cstat |= 1;
    else
        pWall->cstat &= ~1;
    if (pFrame->at1_7)
        pWall->cstat |= 64;
    else
        pWall->cstat &= ~64;
}

void UpdateMasked(int nXWall, SEQFRAME *pFrame)
{
    dassert(nXWall > 0 && nXWall < kMaxXWalls);
    int nWall = xwall[nXWall].reference;
    dassert(nWall >= 0 && nWall < kMaxWalls);
    walltype *pWall = &wall[nWall];
    dassert(pWall->extra == nXWall);
    dassert(pWall->nextwall >= 0);
    walltype *pWallNext = &wall[pWall->nextwall];
    pWall->overpicnum = pWallNext->overpicnum = seqGetTile(pFrame);
    if (pFrame->at5_0)
        pWall->pal = pWallNext->pal = pFrame->at5_0;
    if (pFrame->at1_4)
    {
        pWall->cstat |= 128;
        pWallNext->cstat |= 128;
    }
    else
    {
        pWall->cstat &= ~128;
        pWallNext->cstat &= ~128;
    }
    if (pFrame->at1_5)
    {
        pWall->cstat |= 512;
        pWallNext->cstat |= 512;
    }
    else
    {
        pWall->cstat &= ~512;
        pWallNext->cstat &= ~512;
    }
    if (pFrame->at1_6)
    {
        pWall->cstat |= 1;
        pWallNext->cstat |= 1;
    }
    else
    {
        pWall->cstat &= ~1;
        pWallNext->cstat &= ~1;
    }
    if (pFrame->at1_7)
    {
        pWall->cstat |= 64;
        pWallNext->cstat |= 64;
    }
    else
    {
        pWall->cstat &= ~64;
        pWallNext->cstat &= ~64;
    }
}

void UpdateFloor(int nXSector, SEQFRAME *pFrame)
{
    dassert(nXSector > 0 && nXSector < kMaxXSectors);
    int nSector = xsector[nXSector].reference;
    dassert(nSector >= 0 && nSector < kMaxSectors);
    sectortype *pSector = &sector[nSector];
    dassert(pSector->extra == nXSector);
    pSector->floorpicnum = seqGetTile(pFrame);
    pSector->floorshade = pFrame->at4_0;
    if (pFrame->at5_0)
        pSector->floorpal = pFrame->at5_0;
}

void UpdateCeiling(int nXSector, SEQFRAME *pFrame)
{
    dassert(nXSector > 0 && nXSector < kMaxXSectors);
    int nSector = xsector[nXSector].reference;
    dassert(nSector >= 0 && nSector < kMaxSectors);
    sectortype *pSector = &sector[nSector];
    dassert(pSector->extra == nXSector);
    pSector->ceilingpicnum = seqGetTile(pFrame);
    pSector->ceilingshade = pFrame->at4_0;
    if (pFrame->at5_0)
        pSector->ceilingpal = pFrame->at5_0;
}

void SEQINST::Update(ACTIVE *pActive)
{
    dassert(frameIndex < pSequence->nFrames);
    switch (pActive->type)
    {
    case 0:
        UpdateWall(pActive->xindex, &pSequence->frames[frameIndex]);
        break;
    case 1:
        UpdateCeiling(pActive->xindex , &pSequence->frames[frameIndex]);
        break;
    case 2:
        UpdateFloor(pActive->xindex, &pSequence->frames[frameIndex]);
        break;
    case 3: 
    {

        UpdateSprite(pActive->xindex, &pSequence->frames[frameIndex]);
        if (pSequence->frames[frameIndex].at6_1)
            sfxPlay3DSound(&sprite[xsprite[pActive->xindex].reference], pSequence->ata + Random(pSequence->frames[frameIndex].soundRange), -1, 0);

        spritetype* pSprite = &sprite[xsprite[pActive->xindex].reference];
        if (pSequence->frames[frameIndex].surfaceSound && zvel[pSprite->xvel] == 0 && xvel[pSprite->xvel] != 0) {

            // by NoOne: add surfaceSound trigger feature
            if (gUpperLink[pSprite->sectnum] >= 0) break; // don't play surface sound for stacked sectors
            int surf = tileGetSurfType(pSprite->sectnum + 0x4000); if (!surf) break;
            static int surfSfxMove[15][4] = {
                /* {snd1, snd2, gameVolume, myVolume} */
                {800,801,80,25},
                {802,803,80,25},
                {804,805,80,25},
                {806,807,80,25},
                {808,809,80,25},
                {810,811,80,25},
                {812,813,80,25},
                {814,815,80,25},
                {816,817,80,25},
                {818,819,80,25},
                {820,821,80,25},
                {822,823,80,25},
                {824,825,80,25},
                {826,827,80,25},
                {828,829,80,25},
            };

            int sndId = surfSfxMove[surf][Random(2)];
            DICTNODE * hRes = gSoundRes.Lookup(sndId, "SFX"); SFX * pEffect = (SFX*)gSoundRes.Load(hRes);
            sfxPlay3DSoundCP(pSprite, sndId, -1, 0, 0, (surfSfxMove[surf][2] != pEffect->relVol) ? pEffect->relVol : surfSfxMove[surf][3]);
        }
        

        break;
    }
    case 4:
        UpdateMasked(pActive->xindex, &pSequence->frames[frameIndex]);
        break;
    }
    if (pSequence->frames[frameIndex].at5_5 && atc != -1)
        clientCallback[atc](pActive->type, pActive->xindex);
}

SEQINST * GetInstance(int a1, int a2)
{
    switch (a1)
    {
    case 0:
        if (a2 > 0 && a2 < kMaxXWalls) return &siWall[a2];
        break;
    case 1:
        if (a2 > 0 && a2 < kMaxXSectors) return &siCeiling[a2];
        break;
    case 2:
        if (a2 > 0 && a2 < kMaxXSectors) return &siFloor[a2];
        break;
    case 3:
        if (a2 > 0 && a2 < kMaxXSprites) return &siSprite[a2];
        break;
    case 4:
        if (a2 > 0 && a2 < kMaxWalls) return &siMasked[a2];
        break;
    }
    return NULL;
}

void UnlockInstance(SEQINST *pInst)
{
    dassert(pInst != NULL);
    dassert(pInst->hSeq != NULL);
    dassert(pInst->pSequence != NULL);
    gSysRes.Unlock(pInst->hSeq);
    pInst->hSeq = NULL;
    pInst->pSequence = NULL;
    pInst->at13 = 0;
}

void seqSpawn(int a1, int a2, int a3, int a4)
{
    SEQINST *pInst = GetInstance(a2, a3);
    if (!pInst)
        return;
    DICTNODE *hSeq = gSysRes.Lookup(a1, "SEQ");
    if (!hSeq)
        ThrowError("Missing sequence #%d", a1);
    int i = activeCount;
    if (pInst->at13)
    {
        if (hSeq == pInst->hSeq)
            return;
        UnlockInstance(pInst);
        for (i = 0; i < activeCount; i++)
        {
            if (activeList[i].type == a2 && activeList[i].xindex == a3)
                break;
        }
        dassert(i < activeCount);
    }
    Seq *pSeq = (Seq*)gSysRes.Lock(hSeq);
    if (memcmp(pSeq->signature, "SEQ\x1a", 4) != 0)
        ThrowError("Invalid sequence %d", a1);
    if ((pSeq->version & 0xff00) != 0x300)
        ThrowError("Sequence %d is obsolete version", a1);
    if ((pSeq->version & 0xff) == 0x00)
    {
        for (int i = 0; i < pSeq->nFrames; i++)
            pSeq->frames[i].tile2 = 0;
    }
    pInst->at13 = 1;
    pInst->hSeq = hSeq;
    pInst->pSequence = pSeq;
    pInst->at8 = a1;
    pInst->atc = a4;
    pInst->at10 = pSeq->at8;
    pInst->frameIndex = 0;
    if (i == activeCount)
    {
        dassert(activeCount < kMaxSequences);
        activeList[activeCount].type = a2;
        activeList[activeCount].xindex = a3;
        activeCount++;
    }
    pInst->Update(&activeList[i]);
}

void seqKill(int a1, int a2)
{
    SEQINST *pInst = GetInstance(a1, a2);
    if (!pInst || !pInst->at13)
        return;
    int i;
    for (i = 0; i < activeCount; i++)
    {
        if (activeList[i].type == a1 && activeList[i].xindex == a2)
            break;
    }
    dassert(i < activeCount);
    activeCount--;
    activeList[i] = activeList[activeCount];
    pInst->at13 = 0;
    UnlockInstance(pInst);
}

void seqKillAll(void)
{
    for (int i = 0; i < kMaxXWalls; i++)
    {
        if (siWall[i].at13)
            UnlockInstance(&siWall[i]);
        if (siMasked[i].at13)
            UnlockInstance(&siMasked[i]);
    }
    for (int i = 0; i < kMaxXSectors; i++)
    {
        if (siCeiling[i].at13)
            UnlockInstance(&siCeiling[i]);
        if (siFloor[i].at13)
            UnlockInstance(&siFloor[i]);
    }
    for (int i = 0; i < kMaxXSprites; i++)
    {
        if (siSprite[i].at13)
            UnlockInstance(&siSprite[i]);
    }
    activeCount = 0;
}

int seqGetStatus(int a1, int a2)
{
    SEQINST *pInst = GetInstance(a1, a2);
    if (pInst && pInst->at13)
        return pInst->frameIndex;
    return -1;
}

int seqGetID(int a1, int a2)
{
    SEQINST *pInst = GetInstance(a1, a2);
    if (pInst)
        return pInst->at8;
    return -1;
}

void seqProcess(int a1)
{
    for (int i = 0; i < activeCount; i++)
    {
        SEQINST *pInst = GetInstance(activeList[i].type, activeList[i].xindex);
        Seq *pSeq = pInst->pSequence;
        dassert(pInst->frameIndex < pSeq->nFrames);
        pInst->at10 -= a1;
        while (pInst->at10 < 0)
        {
            pInst->at10 += pSeq->at8;
            pInst->frameIndex++;
            if (pInst->frameIndex == pSeq->nFrames)
            {
                if (pSeq->atc & 1)
                    pInst->frameIndex = 0;
                else
                {
                    UnlockInstance(pInst);
                    if (pSeq->atc & 2)
                    {
                        switch (activeList[i].type)
                        {
                        case 3:
                        {
                            int nXSprite = activeList[i].xindex;
                            int nSprite = xsprite[nXSprite].reference;
                            dassert(nSprite >= 0 && nSprite < kMaxSprites);
                            evKill(nSprite, 3);
                            if ((sprite[nSprite].hitag & 16) && sprite[nSprite].zvel >= 200 && sprite[nSprite].zvel < 254)
                                evPost(nSprite, 3, gGameOptions.nMonsterSettings, (COMMAND_ID)9);
                            else
                                DeleteSprite(nSprite);
                            break;
                        }
                        case 4:
                        {
                            int nXWall = activeList[i].xindex;
                            int nWall = xwall[nXWall].reference;
                            dassert(nWall >= 0 && nWall < kMaxWalls);
                            wall[nWall].cstat &= ~(8 + 16 + 32);
                            if (wall[nWall].nextwall != -1)
                                wall[wall[nWall].nextwall].cstat &= ~(8 + 16 + 32);
                            break;
                        }
                        }
                    }
                    activeList[i--] = activeList[--activeCount];
                    break;
                }
            }
            pInst->Update(&activeList[i]);
        }
    }
}

class SeqLoadSave : public LoadSave {
    virtual void Load(void);
    virtual void Save(void);
};

void SeqLoadSave::Load(void)
{
    Read(&siWall, sizeof(siWall));
    Read(&siMasked, sizeof(siMasked));
    Read(&siCeiling, sizeof(siCeiling));
    Read(&siFloor, sizeof(siFloor));
    Read(&siSprite, sizeof(siSprite));
    Read(&activeList, sizeof(activeList));
    Read(&activeCount, sizeof(activeCount));
    for (int i = 0; i < kMaxXWalls; i++)
    {
        siWall[i].hSeq = NULL;
        siMasked[i].hSeq = NULL;
        siWall[i].pSequence = NULL;
        siMasked[i].pSequence = NULL;
    }
    for (int i = 0; i < kMaxXSectors; i++)
    {
        siCeiling[i].hSeq = NULL;
        siFloor[i].hSeq = NULL;
        siCeiling[i].pSequence = NULL;
        siFloor[i].pSequence = NULL;
    }
    for (int i = 0; i < kMaxXSprites; i++)
    {
        siSprite[i].hSeq = NULL;
        siSprite[i].pSequence = NULL;
    }
    for (int i = 0; i < activeCount; i++)
    {
        SEQINST *pInst = GetInstance(activeList[i].type, activeList[i].xindex);
        if (pInst->at13)
        {
            int nSeq = pInst->at8;
            DICTNODE *hSeq = gSysRes.Lookup(nSeq, "SEQ");
            if (!hSeq)
                ThrowError("Missing sequence #%d", nSeq);
            Seq *pSeq = (Seq*)gSysRes.Lock(hSeq);
            if (memcmp(pSeq->signature, "SEQ\x1a", 4) != 0)
                ThrowError("Invalid sequence %d", nSeq);
            if ((pSeq->version & 0xff00) != 0x300)
                ThrowError("Sequence %d is obsolete version", nSeq);
            pInst->hSeq = hSeq;
            pInst->pSequence = pSeq;
        }
    }
}

void SeqLoadSave::Save(void)
{
    Write(&siWall, sizeof(siWall));
    Write(&siMasked, sizeof(siMasked));
    Write(&siCeiling, sizeof(siCeiling));
    Write(&siFloor, sizeof(siFloor));
    Write(&siSprite, sizeof(siSprite));
    Write(&activeList, sizeof(activeList));
    Write(&activeCount, sizeof(activeCount));
}

static SeqLoadSave *myLoadSave;

void SeqLoadSaveConstruct(void)
{
    myLoadSave = new SeqLoadSave();
}
