#include "Engine/Graphics/Direct3D/Render.h"

#include <d3d.h>
#include <ddraw.h>

#include <algorithm>

#include "Engine/Engine.h"
#include "Engine/LOD.h"
#include "Engine/OurMath.h"
#include "Engine/Party.h"
#include "Engine/SpellFxRenderer.h"
#include "Engine/Time.h"

#include "Engine/Objects/Actor.h"
#include "Engine/Objects/ObjectList.h"
#include "Engine/Objects/SpriteObject.h"

#include "Engine/Graphics/DecalBuilder.h"
#include "Engine/Graphics/DecorationList.h"
#include "Engine/Graphics/Direct3D/TextureD3D.h"
#include "Engine/Graphics/ImageLoader.h"
#include "Engine/Graphics/Level/Decoration.h"
#include "Engine/Graphics/LightmapBuilder.h"
#include "Engine/Graphics/Lights.h"
#include "Engine/Graphics/Outdoor.h"
#include "Engine/Graphics/PCX.h"
#include "Engine/Graphics/PaletteManager.h"
#include "Engine/Graphics/ParticleEngine.h"
#include "Engine/Graphics/Sprites.h"
#include "Engine/Graphics/Viewport.h"
#include "Engine/Graphics/Vis.h"
#include "Engine/Graphics/Weather.h"

#include "Engine/Graphics/Direct3D/RenderD3D.h"

#include "GUI/GUIWindow.h"

#include "IO/Mouse.h"

#include "Arcomage/Arcomage.h"

#include "Platform/Api.h"

#pragma comment(lib, "GdiPlus.lib")

using EngineIoc = Engine_::IocContainer;

struct IDirectDrawClipper *pDDrawClipper;
struct RenderVertexD3D3 pVertices[50];

RenderVertexSoft VertexRenderList[50];  // array_50AC10
RenderVertexSoft array_73D150[20];

RenderVertexD3D3 d3d_vertex_buffer[50];

DDPIXELFORMAT ddpfPrimarySuface;

void ErrHR(HRESULT hr, const char *pAPI, const char *pFunction,
           const char *pFile, int line) {
    if (SUCCEEDED(hr)) {
        return;
    }

    char msg[4096];
    sprintf(msg, "%s error (%08X) in\n\t%s\nin\n\t%s:%u", pAPI, hr, pFunction,
            pFile, line);

    char caption[1024];
    sprintf(caption, "%s error", pAPI);

    Error(msg);
}

unsigned int Render::GetRenderWidth() const { return window->GetWidth(); }
unsigned int Render::GetRenderHeight() const { return window->GetHeight(); }



Texture *Render::CreateTexture_ColorKey(const String &name, uint16_t colorkey) {
    return TextureD3D::Create(new ColorKey_LOD_Loader(pIcons_LOD, name, colorkey));
}

Texture *Render::CreateTexture_Solid(const String &name) {
    return TextureD3D::Create(new Image16bit_LOD_Loader(pIcons_LOD, name));
}

Texture *Render::CreateTexture_Alpha(const String &name) {
    return TextureD3D::Create(new Alpha_LOD_Loader(pIcons_LOD, name));
}

Texture *Render::CreateTexture_PCXFromIconsLOD(const String &name) {
    return TextureD3D::Create(new PCX_LOD_Loader(pIcons_LOD, name));
}

Texture *Render::CreateTexture_PCXFromNewLOD(const String &name) {
    return TextureD3D::Create(new PCX_LOD_Loader(pNew_LOD, name));
}

Texture *Render::CreateTexture_PCXFromFile(const String &name) {
    return TextureD3D::Create(new PCX_File_Loader(pIcons_LOD, name));
}

Texture *Render::CreateTexture_Blank(unsigned int width, unsigned int height,
    IMAGE_FORMAT format, const void *pixels) {

    return TextureD3D::Create(width, height, format, pixels);
}



Texture *Render::CreateTexture(const String &name) {
    return TextureD3D::Create(new Bitmaps_LOD_Loader(pBitmaps_LOD, name));
}

Texture *Render::CreateSprite(const String &name, unsigned int palette_id,
                              unsigned int lod_sprite_id) {
    return TextureD3D::Create(
        new Sprites_LOD_Loader(pSprites_LOD, palette_id, name, lod_sprite_id));
}

void Render::WritePixel16(int x, int y, uint16_t color) {
    // do not use this - slow
    __debugbreak();
    logger->Info(L"Reduce use of WritePixel16");

    unsigned int b = (color & 0x1F) << 3;
    unsigned int g = ((color >> 5) & 0x3F) << 2;
    unsigned int r = ((color >> 11) & 0x1F) << 3;

    Gdiplus::Color c(r, g, b);
    p2DSurface->SetPixel(x, y, c);
}

bool Render::CheckTextureStages() {
    bool bResult = false;

    IDirectDrawSurface4 *pSurface1 = nullptr;
    IDirect3DTexture2 *pTexture1 = nullptr;
    pRenderD3D->CreateTexture(64, 64, &pSurface1, &pTexture1, true, false, 32);
    ErrD3D(pRenderD3D->pDevice->SetTexture(0, pTexture1));
    ErrD3D(pRenderD3D->pDevice->SetTextureStageState(0, D3DTSS_ADDRESS,
                                                     D3DTADDRESS_WRAP));
    ErrD3D(pRenderD3D->pDevice->SetTextureStageState(0, D3DTSS_COLOROP, 2));
    ErrD3D(pRenderD3D->pDevice->SetTextureStageState(0, D3DTSS_COLORARG1, 2));
    ErrD3D(pRenderD3D->pDevice->SetTextureStageState(0, D3DTSS_MAGFILTER, 2));
    ErrD3D(pRenderD3D->pDevice->SetTextureStageState(0, D3DTSS_MINFILTER, 2));
    ErrD3D(pRenderD3D->pDevice->SetTextureStageState(0, D3DTSS_MIPFILTER, 1));

    IDirectDrawSurface4 *pSurface2 = nullptr;
    IDirect3DTexture2 *pTexture2 = nullptr;
    pRenderD3D->CreateTexture(64, 64, &pSurface2, &pTexture2, true, false, 32);
    ErrD3D(pRenderD3D->pDevice->SetTexture(0, pTexture2));
    ErrD3D(pRenderD3D->pDevice->SetTextureStageState(1, D3DTSS_ADDRESS,
                                                     D3DTADDRESS_CLAMP));
    ErrD3D(
        pRenderD3D->pDevice->SetTextureStageState(1, D3DTSS_TEXCOORDINDEX, 1));
    ErrD3D(pRenderD3D->pDevice->SetTextureStageState(1, D3DTSS_COLOROP, 7));
    ErrD3D(pRenderD3D->pDevice->SetTextureStageState(1, D3DTSS_COLORARG1, 2));
    ErrD3D(pRenderD3D->pDevice->SetTextureStageState(1, D3DTSS_COLORARG2, 1));
    ErrD3D(pRenderD3D->pDevice->SetTextureStageState(1, D3DTSS_MAGFILTER, 2));
    ErrD3D(pRenderD3D->pDevice->SetTextureStageState(1, D3DTSS_MINFILTER, 2));
    ErrD3D(pRenderD3D->pDevice->SetTextureStageState(1, D3DTSS_MIPFILTER, 1));

    DWORD v4 = 0;
    if (!pRenderD3D->pDevice->ValidateDevice(&v4) && v4 == 1) {
        bResult = true;
    }
    ErrD3D(pRenderD3D->pDevice->SetTextureStageState(1, D3DTSS_COLOROP, 1));
    pTexture1->Release();
    pTexture2->Release();
    pSurface1->Release();
    pSurface2->Release();

    return bResult;
}

void Render::DrawBillboardList_BLV() {
    SoftwareBillboard soft_billboard = {0};
    soft_billboard.sParentBillboardID = -1;
    //  soft_billboard.pTarget = pBLVRenderParams->pRenderTarget;
    soft_billboard.pTargetZ = pBLVRenderParams->pTargetZBuffer;
    //  soft_billboard.uTargetPitch = uTargetSurfacePitch;
    soft_billboard.uViewportX = pBLVRenderParams->uViewportX;
    soft_billboard.uViewportY = pBLVRenderParams->uViewportY;
    soft_billboard.uViewportZ = pBLVRenderParams->uViewportZ - 1;
    soft_billboard.uViewportW = pBLVRenderParams->uViewportW;

    pODMRenderParams->uNumBillboards = ::uNumBillboardsToDraw;
    for (uint i = 0; i < ::uNumBillboardsToDraw; ++i) {
        RenderBillboard *p = &pBillboardRenderList[i];
        if (p->hwsprite) {
            soft_billboard.screen_space_x = p->screen_space_x;
            soft_billboard.screen_space_y = p->screen_space_y;
            soft_billboard.screen_space_z = p->screen_space_z;
            soft_billboard.sParentBillboardID = i;
            soft_billboard.screenspace_projection_factor_x =
                p->screenspace_projection_factor_x;
            soft_billboard.screenspace_projection_factor_y =
                p->screenspace_projection_factor_y;
            soft_billboard.object_pid = p->object_pid;
            soft_billboard.uFlags = p->field_1E;
            soft_billboard.sTintColor = p->sTintColor;

            DrawBillboard_Indoor(&soft_billboard, p);
        }
    }
}

bool Render::AreRenderSurfacesOk() { return pFrontBuffer4 && pBackBuffer4; }

extern unsigned int BlendColors(unsigned int a1, unsigned int a2);

void Render::RenderTerrainD3D() {  // New function
    // warning: the game uses CW culling by default, ccw is incosistent
    pRenderD3D->pDevice->SetRenderState(D3DRENDERSTATE_CULLMODE, D3DCULL_CW);

    static RenderVertexSoft
        pTerrainVertices[128 * 128];  // vertexCountX and vertexCountZ

    //Генерация местоположения
    //вершин-------------------------------------------------------------------------
    //решётка вершин делится на две части от -64 до 0 и от 0 до 64
    //
    // -64  X  0     64
    //  --------------- 64
    //  |      |      |
    //  |      |      |
    //  |      |      |
    // 0|------+------| Z
    //  |      |      |
    //  |      |      |
    //  |      |      |
    //  ---------------
    //                -64

    int blockScale = 512;
    int heightScale = 32;
    for (unsigned int z = 0; z < 128; ++z) {
        for (unsigned int x = 0; x < 128; ++x) {
            pTerrainVertices[z * 128 + x].vWorldPosition.x =
                (-64 + (signed)x) * blockScale;
            pTerrainVertices[z * 128 + x].vWorldPosition.y =
                (64 - (signed)z) * blockScale;
            pTerrainVertices[z * 128 + x].vWorldPosition.z =
                heightScale * pOutdoor->pTerrain.pHeightmap[z * 128 + x];
            pIndoorCameraD3D->ViewTransform(&pTerrainVertices[z * 128 + x], 1);
            pIndoorCameraD3D->Project(&pTerrainVertices[z * 128 + x], 1, 0);
        }
    }
    //-------(Отсечение невидимой части
    //карты)------------------------------------------------------------------------------------------
    float direction = (float)(pIndoorCameraD3D->sRotationY /
                              256);  // direction of the camera(напрвление
                                     // камеры) 0-East(B) 1-NorthEast(CB)
                                     // 2-North(C)
                                     // 3-WestNorth(CЗ)
                                     // 4-West(З)
                                     // 5-SouthWest(ЮЗ)
                                     // 6-South(Ю)
                                     // 7-SouthEast(ЮВ)
    unsigned int Start_X, End_X, Start_Z, End_Z;
    if (direction >= 0 && direction < 1.0) {  // East(B) - NorthEast(CB)
        Start_X = pODMRenderParams->uMapGridCellX - 2, End_X = 128;
        Start_Z = 0, End_Z = 128;
    } else if (direction >= 1.0 &&
               direction < 3.0) {  // NorthEast(CB) - WestNorth(CЗ)
        Start_X = 0, End_X = 128;
        Start_Z = 0, End_Z = pODMRenderParams->uMapGridCellZ + 2;
    } else if (direction >= 3.0 &&
               direction < 5.0) {  // WestNorth(CЗ) - SouthWest(ЮЗ)
        Start_X = 0, End_X = pODMRenderParams->uMapGridCellX + 2;
        Start_Z = 0, End_Z = 128;
    } else if (direction >= 5.0 &&
               direction < 7.0) {  // SouthWest(ЮЗ) - //SouthEast(ЮВ)
        Start_X = 0, End_X = 128;
        Start_Z = pODMRenderParams->uMapGridCellZ - 2, End_Z = 128;
    } else {  // SouthEast(ЮВ) - East(B)
        Start_X = pODMRenderParams->uMapGridCellX - 2, End_X = 128;
        Start_Z = 0, End_Z = 128;
    }

    int camx = pODMRenderParams->uMapGridCellX;
    int camz = pODMRenderParams->uMapGridCellZ - 1;
    int tilerange = (pIndoorCameraD3D->GetFarClip() / blockScale) + 1;

    float Light_tile_dist;

    for (unsigned int z = Start_Z; z < End_Z; ++z) {
        for (unsigned int x = Start_X; x < End_X; ++x) {
            // tile culling
            int xdist = camx - x;
            int zdist = camz - z;

            if (xdist > tilerange || zdist > tilerange) continue;

            int dist = sqrt((xdist)*(xdist)+(zdist)*(zdist));
            if (dist > tilerange) continue;  // crude distance culling


            struct Polygon *pTilePolygon = &array_77EC08[pODMRenderParams->uNumPolygons];
            pTilePolygon->flags = 0;
            pTilePolygon->field_32 = 0;
            // pTilePolygon->uTileBitmapID = pOutdoor->DoGetTileTexture(x, z);
            // pTilePolygon->pTexture = (Texture_MM7
            // *)&pBitmaps_LOD->pHardwareTextures[pTilePolygon->uTileBitmapID];
            // if (pTilePolygon->uTileBitmapID == 0xFFFF)
            //    continue;
            auto tile = pOutdoor->DoGetTile(x, z);
            if (!tile) continue;

            // pTile->flags = 0x8010 |pOutdoor->GetSomeOtherTileInfo(x, z);
            pTilePolygon->flags = pOutdoor->GetSomeOtherTileInfo(x, z);
            pTilePolygon->field_32 = 0;
            pTilePolygon->field_59 = 1;
            pTilePolygon->sTextureDeltaU = 0;
            pTilePolygon->sTextureDeltaV = 0;
            //  x,z         x+1,z
            //  .____________.
            //  |            |
            //  |            |
            //  |            |
            //  |            |
            //  |            |
            //  .____________.
            //  x,z+1       x+1,z+1

            // verts are CW
            // memcpy(&array_73D150[0], &pTerrainVertices[z * 128 + x],
            //       sizeof(RenderVertexSoft));  // x, z
            // array_73D150[0].u = 0;
            // array_73D150[0].v = 0;
            // memcpy(&array_73D150[1], &pTerrainVertices[z * 128 + x + 1],
            //       sizeof(RenderVertexSoft));  // x + 1, z
            // array_73D150[1].u = 1;
            // array_73D150[1].v = 0;
            // memcpy(&array_73D150[2], &pTerrainVertices[(z + 1) * 128 + x + 1],
            //       sizeof(RenderVertexSoft));  // x + 1, z + 1
            // array_73D150[2].u = 1;
            // array_73D150[2].v = 1;
            // memcpy(&array_73D150[3], &pTerrainVertices[(z + 1) * 128 + x],
            //       sizeof(RenderVertexSoft));  // x, z + 1
            // array_73D150[3].u = 0;
            // array_73D150[3].v = 1;

            // verts CCW - for testing
            memcpy(&array_73D150[0], &pTerrainVertices[z * 128 + x],
                sizeof(RenderVertexSoft));  // x, z
            array_73D150[0].u = 0;
            array_73D150[0].v = 0;
            memcpy(&array_73D150[3], &pTerrainVertices[z * 128 + x + 1],
                sizeof(RenderVertexSoft));  // x + 1, z
            array_73D150[3].u = 1;
            array_73D150[3].v = 0;
            memcpy(&array_73D150[2], &pTerrainVertices[(z + 1) * 128 + x + 1],
                sizeof(RenderVertexSoft));  // x + 1, z + 1
            array_73D150[2].u = 1;
            array_73D150[2].v = 1;
            memcpy(&array_73D150[1], &pTerrainVertices[(z + 1) * 128 + x],
                sizeof(RenderVertexSoft));  // x, z + 1
            array_73D150[1].u = 0;
            array_73D150[1].v = 1;

            // v58 = 0;
            // if (v58 == 4) // if all y == first y;  primitive in xz plane
            // pTile->field_32 |= 0x0001;
            pTilePolygon->pODMFace = nullptr;
            pTilePolygon->uNumVertices = 4;
            pTilePolygon->field_59 = 5;

            if (array_73D150[0].vWorldViewPosition.x < pIndoorCameraD3D->GetNearClip() &&
                array_73D150[1].vWorldViewPosition.x < pIndoorCameraD3D->GetNearClip() &&
                array_73D150[2].vWorldViewPosition.x < pIndoorCameraD3D->GetNearClip() &&
                array_73D150[3].vWorldViewPosition.x < pIndoorCameraD3D->GetNearClip())
                continue;

            if (pIndoorCameraD3D->GetFarClip() < array_73D150[0].vWorldViewPosition.x &&
                pIndoorCameraD3D->GetFarClip() < array_73D150[1].vWorldViewPosition.x &&
                pIndoorCameraD3D->GetFarClip() < array_73D150[2].vWorldViewPosition.x &&
                pIndoorCameraD3D->GetFarClip() < array_73D150[3].vWorldViewPosition.x)
                continue;

            //----------------------------------------------------------------------------

            ++pODMRenderParams->uNumPolygons;
            // ++pODMRenderParams->field_44;
            assert(pODMRenderParams->uNumPolygons < 20000);

            pTilePolygon->uBModelID = 0;
            pTilePolygon->uBModelFaceID = 0;
            pTilePolygon->pid = (8 * (0 | (0 << 6))) | 6;
            for (unsigned int k = 0; k < pTilePolygon->uNumVertices; ++k) {
                memcpy(&VertexRenderList[k], &array_73D150[k], sizeof(struct RenderVertexSoft));
                VertexRenderList[k]._rhw = 1.0 / (array_73D150[k].vWorldViewPosition.x + 0.0000001000000011686097);
            }

            // shading
            // (затенение)----------------------------------------------------------------------------
            // uint norm_idx = pTerrainNormalIndices[2 * (z * 128 + x) + 1];
            uint norm_idx = pTerrainNormalIndices[(2 * x * 128) + (2 * z) + 2 /*+ 1*/ ];  // 2 is top tri // 3 is bottom
            uint bottnormidx = pTerrainNormalIndices[(2 * x * 128) + (2 * z) + 3];

            assert(norm_idx < uNumTerrainNormals);
            assert(bottnormidx < uNumTerrainNormals);

            Vec3_float_ *norm = &pTerrainNormals[norm_idx];
            Vec3_float_ *norm2 = &pTerrainNormals[bottnormidx];

            if (norm_idx < 0 || norm_idx > uNumTerrainNormals - 1)
                norm = 0;
            else
                norm = &pTerrainNormals[norm_idx];

            if (bottnormidx < 0 || bottnormidx > uNumTerrainNormals - 1)
                norm2 = 0;
            else
                norm2 = &pTerrainNormals[bottnormidx];


            if (norm_idx != bottnormidx) {
                // we have a split poly - need to apply lights and decals seperately to each half

                pTilePolygon->uNumVertices = 3;

                ///////////// triangle 1 - 1 2 3

                // verts CCW - for testing
                memcpy(&array_73D150[0], &pTerrainVertices[z * 128 + x],
                    sizeof(RenderVertexSoft));  // x, z
                array_73D150[0].u = 0;
                array_73D150[0].v = 0;
                memcpy(&array_73D150[2], &pTerrainVertices[z * 128 + x + 1],
                    sizeof(RenderVertexSoft));  // x + 1, z
                array_73D150[2].u = 1;
                array_73D150[2].v = 0;
                memcpy(&array_73D150[1], &pTerrainVertices[(z + 1) * 128 + x + 1],
                    sizeof(RenderVertexSoft));  // x + 1, z + 1
                array_73D150[1].u = 1;
                array_73D150[1].v = 1;
                // memcpy(&array_73D150[2], &pTerrainVertices[(z + 1) * 128 + x],
                //    sizeof(RenderVertexSoft));  // x, z + 1
                // array_73D150[2].u = 0;
                // array_73D150[2].v = 1;

                for (unsigned int k = 0; k < pTilePolygon->uNumVertices; ++k) {
                    memcpy(&VertexRenderList[k], &array_73D150[k], sizeof(struct RenderVertexSoft));
                    VertexRenderList[k]._rhw = 1.0 / (array_73D150[k].vWorldViewPosition.x + 0.0000001000000011686097);
                }

                float _f = ((norm->x * (float)pOutdoor->vSunlight.x / 65536.0) -
                    (norm->y * (float)pOutdoor->vSunlight.y / 65536.0) -
                    (norm->z * (float)pOutdoor->vSunlight.z / 65536.0));
                pTilePolygon->dimming_level = 20.0 - floorf(20.0 * _f + 0.5f);

                lightmap_builder->StackLights_TerrainFace(norm, &Light_tile_dist, VertexRenderList, 3, 1);
                decal_builder->ApplyBloodSplatToTerrain(pTilePolygon, norm, &Light_tile_dist, VertexRenderList, 3, 1);

                unsigned int a5 = 4;

                // ---------Draw distance(Дальность отрисовки)-------------------------------
                int far_clip_distance = pIndoorCameraD3D->GetFarClip();
                float near_clip_distance = pIndoorCameraD3D->GetNearClip();


                bool neer_clip = array_73D150[0].vWorldViewPosition.x < near_clip_distance ||
                    array_73D150[1].vWorldViewPosition.x < near_clip_distance ||
                    array_73D150[2].vWorldViewPosition.x < near_clip_distance;
                bool far_clip =
                    (float)far_clip_distance < array_73D150[0].vWorldViewPosition.x ||
                    (float)far_clip_distance < array_73D150[1].vWorldViewPosition.x ||
                    (float)far_clip_distance < array_73D150[2].vWorldViewPosition.x;

                int uClipFlag = 0;
                static stru154 static_sub_0048034E_stru_154;
                lightmap_builder->StationaryLightsCount = 0;
                if (Lights.uNumLightsApplied > 0 || decal_builder->uNumDecals > 0) {
                    if (neer_clip)
                        uClipFlag = 3;
                    else
                        uClipFlag = far_clip != 0 ? 5 : 0;
                    static_sub_0048034E_stru_154.ClassifyPolygon(norm, Light_tile_dist);

                    if (decal_builder->uNumDecals > 0)
                        decal_builder->ApplyDecals(31 - pTilePolygon->dimming_level,
                            4, &static_sub_0048034E_stru_154,
                            3, VertexRenderList, 0,
                            *(float *)&uClipFlag, -1);
                    if (Lights.uNumLightsApplied > 0)
                        lightmap_builder->ApplyLights(
                            &Lights, &static_sub_0048034E_stru_154, 3,
                            VertexRenderList, 0, uClipFlag);
                }







                // pODMRenderParams->shading_dist_mist = temp;

                // check the transparency and texture (tiles) mapping (проверка
                // прозрачности и наложение текстур (тайлов))----------------------
                bool transparent = false;

                auto tile_texture = tile->GetTexture();
                if (!(pTilePolygon->flags & 1)) {
                    // не поддерживается TextureFrameTable
                    if (/*pTile->flags & 2 && */ tile->IsWaterTile()) {
                        tile_texture =
                            this->hd_water_tile_anim[this->hd_water_current_frame];
                    } else if (tile->IsWaterBorderTile()) {
                        // for all shore tiles - draw a tile water under them since
                        // they're half-empty
                        DrawBorderTiles(pTilePolygon);
                        transparent = true;
                    }
                    pTilePolygon->texture = tile_texture;

                    render->DrawTerrainPolygon(pTilePolygon, transparent, true);
                }

                ///////////// triangle 2  0 1 3
                {
                    // verts CCW - for testing
                    memcpy(&array_73D150[0], &pTerrainVertices[z * 128 + x],
                        sizeof(RenderVertexSoft));  // x, z
                    array_73D150[0].u = 0;
                    array_73D150[0].v = 0;
                    // memcpy(&array_73D150[2], &pTerrainVertices[z * 128 + x + 1],
                    //    sizeof(RenderVertexSoft));  // x + 1, z
                    // array_73D150[2].u = 1;
                    // array_73D150[2].v = 0;
                    memcpy(&array_73D150[2], &pTerrainVertices[(z + 1) * 128 + x + 1],
                        sizeof(RenderVertexSoft));  // x + 1, z + 1
                    array_73D150[2].u = 1;
                    array_73D150[2].v = 1;
                    memcpy(&array_73D150[1], &pTerrainVertices[(z + 1) * 128 + x],
                        sizeof(RenderVertexSoft));  // x, z + 1
                    array_73D150[1].u = 0;
                    array_73D150[1].v = 1;

                    for (unsigned int k = 0; k < pTilePolygon->uNumVertices; ++k) {
                        memcpy(&VertexRenderList[k], &array_73D150[k], sizeof(struct RenderVertexSoft));
                        VertexRenderList[k]._rhw = 1.0 / (array_73D150[k].vWorldViewPosition.x + 0.0000001000000011686097);
                    }

                    float _f2 = ((norm2->x * (float)pOutdoor->vSunlight.x / 65536.0) -
                        (norm2->y * (float)pOutdoor->vSunlight.y / 65536.0) -
                        (norm2->z * (float)pOutdoor->vSunlight.z / 65536.0));
                    pTilePolygon->dimming_level = 20.0 - floorf(20.0 * _f2 + 0.5f);


                    lightmap_builder->StackLights_TerrainFace(norm2, &Light_tile_dist, VertexRenderList, 3, 0);

                    decal_builder->ApplyBloodSplatToTerrain(pTilePolygon, norm2,
                        &Light_tile_dist, VertexRenderList, 3, 0);


                    unsigned int a5_2 = 4;

                    // ---------Draw distance(Дальность отрисовки)-------------------------------
                    int far_clip_distance_2 = pIndoorCameraD3D->GetFarClip();
                    float near_clip_distance_2 = pIndoorCameraD3D->GetNearClip();


                    bool neer_clip_2 = array_73D150[0].vWorldViewPosition.x < near_clip_distance_2 ||
                        array_73D150[1].vWorldViewPosition.x < near_clip_distance_2 ||
                        array_73D150[2].vWorldViewPosition.x < near_clip_distance_2;
                    bool far_clip_2 =
                        (float)far_clip_distance_2 < array_73D150[0].vWorldViewPosition.x ||
                        (float)far_clip_distance_2 < array_73D150[1].vWorldViewPosition.x ||
                        (float)far_clip_distance_2 < array_73D150[2].vWorldViewPosition.x;

                    int uClipFlag_2 = 0;
                    static stru154 static_sub_0048034E_stru_154_2;
                    lightmap_builder->StationaryLightsCount = 0;
                    if (Lights.uNumLightsApplied > 0 || decal_builder->uNumDecals > 0) {
                        if (neer_clip_2)
                            uClipFlag_2 = 3;
                        else
                            uClipFlag_2 = far_clip_2 != 0 ? 5 : 0;
                        static_sub_0048034E_stru_154_2.ClassifyPolygon(norm2, Light_tile_dist);

                        if (decal_builder->uNumDecals > 0)
                            decal_builder->ApplyDecals(31 - pTilePolygon->dimming_level,
                                4, &static_sub_0048034E_stru_154_2,
                                3, VertexRenderList, 0,
                                *(float *)&uClipFlag_2, -1);
                        if (Lights.uNumLightsApplied > 0)
                            lightmap_builder->ApplyLights(
                                &Lights, &static_sub_0048034E_stru_154_2, 3,
                                VertexRenderList, 0, uClipFlag_2);
                    }







                    // pODMRenderParams->shading_dist_mist = temp;

                    // check the transparency and texture (tiles) mapping (проверка
                    // прозрачности и наложение текстур (тайлов))----------------------
                    bool transparent = false;

                    auto tile_texture = tile->GetTexture();
                    if (!(pTilePolygon->flags & 1)) {
                        // не поддерживается TextureFrameTable
                        if (/*pTile->flags & 2 && */ tile->IsWaterTile()) {
                            tile_texture =
                                this->hd_water_tile_anim[this->hd_water_current_frame];
                        } else if (tile->IsWaterBorderTile()) {
                            // for all shore tiles - draw a tile water under them since
                            // they're half-empty
                            DrawBorderTiles(pTilePolygon);
                            transparent = true;
                        }
                        pTilePolygon->texture = tile_texture;

                        render->DrawTerrainPolygon(pTilePolygon, transparent, true);
                    }
                }  // end split trinagles
            } else {
                float _f = ((norm->x * (float)pOutdoor->vSunlight.x / 65536.0) -
                    (norm->y * (float)pOutdoor->vSunlight.y / 65536.0) -
                    (norm->z * (float)pOutdoor->vSunlight.z / 65536.0));
                pTilePolygon->dimming_level = 20.0 - floorf(20.0 * _f + 0.5f);

                lightmap_builder->StackLights_TerrainFace(norm, &Light_tile_dist, VertexRenderList, pTilePolygon->uNumVertices, 1);
                decal_builder->ApplyBloodSplatToTerrain(pTilePolygon, norm, &Light_tile_dist, VertexRenderList, pTilePolygon->uNumVertices, 1);

                unsigned int a5 = 4;

                // ---------Draw distance(Дальность отрисовки)-------------------------------
                int far_clip_distance = pIndoorCameraD3D->GetFarClip();
                float near_clip_distance = pIndoorCameraD3D->GetNearClip();

                if (engine->config->extended_draw_distance)
                    far_clip_distance = 0x5000;
                bool neer_clip = array_73D150[0].vWorldViewPosition.x < near_clip_distance ||
                    array_73D150[1].vWorldViewPosition.x < near_clip_distance ||
                    array_73D150[2].vWorldViewPosition.x < near_clip_distance ||
                    array_73D150[3].vWorldViewPosition.x < near_clip_distance;
                bool far_clip =
                    (float)far_clip_distance < array_73D150[0].vWorldViewPosition.x ||
                    (float)far_clip_distance < array_73D150[1].vWorldViewPosition.x ||
                    (float)far_clip_distance < array_73D150[2].vWorldViewPosition.x ||
                    (float)far_clip_distance < array_73D150[3].vWorldViewPosition.x;

                int uClipFlag = 0;
                static stru154 static_sub_0048034E_stru_154;
                lightmap_builder->StationaryLightsCount = 0;
                if (Lights.uNumLightsApplied > 0 || decal_builder->uNumDecals > 0) {
                    if (neer_clip)
                        uClipFlag = 3;
                    else
                        uClipFlag = far_clip != 0 ? 5 : 0;
                    static_sub_0048034E_stru_154.ClassifyPolygon(norm, Light_tile_dist);

                    if (decal_builder->uNumDecals > 0)
                        decal_builder->ApplyDecals(31 - pTilePolygon->dimming_level,
                            4, &static_sub_0048034E_stru_154,
                            a5, VertexRenderList, 0,
                            *(float *)&uClipFlag, -1);

                    if (Lights.uNumLightsApplied > 0)
                        lightmap_builder->ApplyLights(&Lights, &static_sub_0048034E_stru_154, a5, VertexRenderList, 0, uClipFlag);
                }


                // pODMRenderParams->shading_dist_mist = temp;

                // check the transparency and texture (tiles) mapping (проверка
                // прозрачности и наложение текстур (тайлов))----------------------
                bool transparent = false;

                auto tile_texture = tile->GetTexture();
                if (!(pTilePolygon->flags & 1)) {
                    // не поддерживается TextureFrameTable
                    if (/*pTile->flags & 2 && */ tile->IsWaterTile()) {
                        tile_texture =
                            this->hd_water_tile_anim[this->hd_water_current_frame];
                    } else if (tile->IsWaterBorderTile()) {
                        // for all shore tiles - draw a tile water under them since
                        // they're half-empty
                        DrawBorderTiles(pTilePolygon);
                        transparent = true;
                    }
                    pTilePolygon->texture = tile_texture;

                    render->DrawTerrainPolygon(pTilePolygon, transparent, true);
                }
            // end norm split
            }
        }
    }
}

void Render::DrawBorderTiles(struct Polygon *poly) {
    struct Polygon poly_clone;
    memcpy(&poly_clone, poly, sizeof(poly_clone));
    poly_clone.texture = this->hd_water_tile_anim[this->hd_water_current_frame];

    pRenderD3D->pDevice->SetRenderState(D3DRENDERSTATE_ZWRITEENABLE, false);
    DrawTerrainPolygon(&poly_clone, true, true);

    pRenderD3D->pDevice->SetRenderState(D3DRENDERSTATE_ZWRITEENABLE, true);
}


void Render::PrepareDecorationsRenderList_ODM() {
    unsigned int v6;        // edi@9
    int v7;                 // eax@9
    SpriteFrame *frame;     // eax@9
    unsigned __int16 *v10;  // eax@9
    int v13;                // ecx@9
    char r;                 // ecx@20
    char g;                 // dl@20
    char b_;                // eax@20
    Particle_sw local_0;    // [sp+Ch] [bp-98h]@7
    unsigned __int16 *v37;  // [sp+84h] [bp-20h]@9
    int v38;                // [sp+88h] [bp-1Ch]@9

    for (unsigned int i = 0; i < uNumLevelDecorations; ++i) {
        // LevelDecoration* decor = &pLevelDecorations[i];
        if ((!(pLevelDecorations[i].uFlags & LEVEL_DECORATION_OBELISK_CHEST) ||
             pLevelDecorations[i].IsObeliskChestActive()) &&
            !(pLevelDecorations[i].uFlags & LEVEL_DECORATION_INVISIBLE)) {
            DecorationDesc *decor_desc = pDecorationList->GetDecoration(pLevelDecorations[i].uDecorationDescID);
            if (!(decor_desc->uFlags & 0x80)) {
                if (!(decor_desc->uFlags & 0x22)) {
                    v6 = pMiscTimer->uTotalGameTimeElapsed;
                    v7 = abs(pLevelDecorations[i].vPosition.x +
                             pLevelDecorations[i].vPosition.y);

                    frame = pSpriteFrameTable->GetFrame(decor_desc->uSpriteID,
                                                        v6 + v7);

                    if (engine->config->seasons_change) {
                        frame = LevelDecorationChangeSeason(decor_desc, v6 + v7, pParty->uCurrentMonth);
                    }

                    if (!frame) {
                        continue;
                    }

                    // v8 = pSpriteFrameTable->GetFrame(decor_desc->uSpriteID,
                    // v6 + v7);

                    v10 = (unsigned __int16 *)stru_5C6E00->Atan2(
                        pLevelDecorations[i].vPosition.x -
                            pIndoorCameraD3D->vPartyPos.x,
                        pLevelDecorations[i].vPosition.y -
                            pIndoorCameraD3D->vPartyPos.y);
                    v38 = 0;
                    v13 = ((signed int)(stru_5C6E00->uIntegerPi +
                                        ((signed int)stru_5C6E00->uIntegerPi >>
                                         3) +
                                        pLevelDecorations[i].field_10_y_rot -
                                        (signed int)v10) >>
                           8) &
                          7;
                    v37 = (unsigned __int16 *)v13;
                    if (frame->uFlags & 2) v38 = 2;
                    if ((256 << v13) & frame->uFlags) v38 |= 4;
                    if (frame->uFlags & 0x40000) v38 |= 0x40;
                    if (frame->uFlags & 0x20000) v38 |= 0x80;

                    // for light
                    if (frame->uGlowRadius) {
                        r = 255;
                        g = 255;
                        b_ = 255;
                        if (render->config->is_using_colored_lights) {
                            r = decor_desc->uColoredLightRed;
                            g = decor_desc->uColoredLightGreen;
                            b_ = decor_desc->uColoredLightBlue;
                        }
                        pStationaryLightsStack->AddLight(
                            pLevelDecorations[i].vPosition.x,
                            pLevelDecorations[i].vPosition.y,
                            pLevelDecorations[i].vPosition.z +
                                decor_desc->uDecorationHeight / 2,
                            frame->uGlowRadius, r, g, b_, _4E94D0_light_type);
                    }  // for light

                    // v17 = (pLevelDecorations[i].vPosition.x -
                    // pIndoorCameraD3D->vPartyPos.x) << 16; v40 =
                    // (pLevelDecorations[i].vPosition.y -
                    // pIndoorCameraD3D->vPartyPos.y) << 16;
                    int party_to_decor_x = pLevelDecorations[i].vPosition.x -
                                           pIndoorCameraD3D->vPartyPos.x;
                    int party_to_decor_y = pLevelDecorations[i].vPosition.y -
                                           pIndoorCameraD3D->vPartyPos.y;
                    int party_to_decor_z = pLevelDecorations[i].vPosition.z -
                                           pIndoorCameraD3D->vPartyPos.z;

                    int view_x = 0;
                    int view_y = 0;
                    int view_z = 0;
                    bool visible = pIndoorCameraD3D->ViewClip(
                        pLevelDecorations[i].vPosition.x,
                        pLevelDecorations[i].vPosition.y,
                        pLevelDecorations[i].vPosition.z, &view_x, &view_y,
                        &view_z);

                    if (visible) {
                        // if (2 * abs(view_x) >= abs(view_y)) {
                            int projected_x = 0;
                            int projected_y = 0;
                            pIndoorCameraD3D->Project(view_x, view_y, view_z,
                                                      &projected_x,
                                                      &projected_y);

                            auto _v41 =
                                frame->scale *
                                fixed::FromInt(pODMRenderParams->int_fov_rad) /
                                fixed::FromInt(view_x);

                            int screen_space_half_width = 0;
                            screen_space_half_width =
                                _v41.GetFloat() *
                                frame->hw_sprites[(int)v37]->uBufferWidth / 2;


                            if (projected_x + screen_space_half_width >=
                                    (signed int)pViewport->uViewportTL_X &&
                                projected_x - screen_space_half_width <=
                                    (signed int)pViewport->uViewportBR_X) {
                                if (::uNumBillboardsToDraw >= 500) return;
                                ::uNumBillboardsToDraw++;
                                ++uNumDecorationsDrawnThisFrame;

                                pBillboardRenderList[::uNumBillboardsToDraw - 1]
                                    .hwsprite = frame->hw_sprites[(int)v37];

                                // error catching
                                if (frame->hw_sprites[(int)v37]->texture->GetHeight() == 0 || frame->hw_sprites[(int)v37]->texture->GetWidth() == 0)
                                    __debugbreak();

                                pBillboardRenderList[::uNumBillboardsToDraw - 1]
                                    .world_x = pLevelDecorations[i].vPosition.x;
                                pBillboardRenderList[::uNumBillboardsToDraw - 1]
                                    .world_y = pLevelDecorations[i].vPosition.y;
                                pBillboardRenderList[::uNumBillboardsToDraw - 1]
                                    .world_z = pLevelDecorations[i].vPosition.z;
                                pBillboardRenderList[::uNumBillboardsToDraw - 1]
                                    .screen_space_x = projected_x;
                                pBillboardRenderList[::uNumBillboardsToDraw - 1]
                                    .screen_space_y = projected_y;
                                pBillboardRenderList[::uNumBillboardsToDraw - 1]
                                    .screen_space_z = view_x;
                                pBillboardRenderList[::uNumBillboardsToDraw - 1]
                                    .screenspace_projection_factor_x = _v41.GetFloat();
                                pBillboardRenderList[::uNumBillboardsToDraw - 1]
                                    .screenspace_projection_factor_y = _v41.GetFloat();
                                pBillboardRenderList[::uNumBillboardsToDraw - 1]
                                    .uPalette = frame->uPaletteIndex;
                                pBillboardRenderList[::uNumBillboardsToDraw - 1]
                                    .field_1E = v38 | 0x200;
                                pBillboardRenderList[::uNumBillboardsToDraw - 1]
                                    .uIndoorSectorID = 0;
                                pBillboardRenderList[::uNumBillboardsToDraw - 1]
                                    .object_pid = PID(OBJECT_Decoration, i);
                                pBillboardRenderList[::uNumBillboardsToDraw - 1]
                                    .dimming_level = 0;
                                pBillboardRenderList[::uNumBillboardsToDraw - 1]
                                    .pSpriteFrame = frame;
                                pBillboardRenderList[::uNumBillboardsToDraw - 1]
                                    .sTintColor = 0;
                            } else {
                                // temp
                            }
                       // }
                    }
                }
            } else {
                memset(&local_0, 0, 0x68);
                local_0.type = ParticleType_Bitmap | ParticleType_Rotating |
                               ParticleType_8;
                local_0.uDiffuse = 0xFF3C1E;
                local_0.x = (double)pLevelDecorations[i].vPosition.x;
                local_0.y = (double)pLevelDecorations[i].vPosition.y;
                local_0.z = (double)pLevelDecorations[i].vPosition.z;
                local_0.r = 0.0;
                local_0.g = 0.0;
                local_0.b = 0.0;
                local_0.particle_size = 1.0;
                local_0.timeToLive = (rand() & 0x80) + 128;
                local_0.texture = spell_fx_renderer->effpar01;
                particle_engine->AddParticle(&local_0);
            }
        }
    }
}

void Render::DrawPolygon(struct Polygon *pPolygon) {
    if ((uNumD3DSceneBegins == 0) || (pPolygon->uNumVertices < 3)) {
        return;
    }

    unsigned int v41;     // eax@29
    unsigned int sCorrectedColor;  // [sp+64h] [bp-4h]@4

    auto texture = (TextureD3D *)pPolygon->texture;
    ODMFace *pFace = pPolygon->pODMFace;
    auto uNumVertices = pPolygon->uNumVertices;

    if (lightmap_builder->StationaryLightsCount) {
        sCorrectedColor = -1;
    }
    engine->AlterGamma_ODM(pFace, &sCorrectedColor);
    if (_4D864C_force_sw_render_rules && engine->config->Flag1_1()) {
        int v8 = ::GetActorTintColor(
            pPolygon->dimming_level, 0,
            VertexRenderList[0].vWorldViewPosition.x, 0, 0);
        lightmap_builder->DrawLightmaps(v8 /*, 0*/);
    } else {
        if (!lightmap_builder->StationaryLightsCount ||
            _4D864C_force_sw_render_rules && engine->config->Flag1_2()) {
            ErrD3D(pRenderD3D->pDevice->SetTextureStageState(0, D3DTSS_ADDRESS,
                                                             D3DTADDRESS_WRAP));
            ErrD3D(pRenderD3D->pDevice->SetRenderState(D3DRENDERSTATE_CULLMODE,
                                                       D3DCULL_CW));
            if (config->is_using_specular) {
                ErrD3D(pRenderD3D->pDevice->SetRenderState(D3DRENDERSTATE_ALPHABLENDENABLE, TRUE));
                ErrD3D(pRenderD3D->pDevice->SetRenderState(D3DRENDERSTATE_SRCBLEND, D3DBLEND_ONE));
                ErrD3D(pRenderD3D->pDevice->SetRenderState(D3DRENDERSTATE_DESTBLEND, D3DBLEND_ZERO));
            }
            for (uint i = 0; i < uNumVertices; ++i) {
                d3d_vertex_buffer[i].pos.x =
                    VertexRenderList[i].vWorldViewProjX;
                d3d_vertex_buffer[i].pos.y =
                    VertexRenderList[i].vWorldViewProjY;
                d3d_vertex_buffer[i].pos.z =
                    1.0 -
                    1.0 / ((VertexRenderList[i].vWorldViewPosition.x * 1000) /
                           pIndoorCameraD3D->GetFarClip());
                d3d_vertex_buffer[i].rhw =
                    1.0 /
                    (VertexRenderList[i].vWorldViewPosition.x + 0.0000001);
                d3d_vertex_buffer[i].diffuse = ::GetActorTintColor(
                    pPolygon->dimming_level, 0,
                    VertexRenderList[i].vWorldViewPosition.x, 0, 0);
                engine->AlterGamma_ODM(pFace, &d3d_vertex_buffer[i].diffuse);

                if (config->is_using_specular)
                    d3d_vertex_buffer[i].specular = sub_47C3D7_get_fog_specular(
                        0, 0, VertexRenderList[i].vWorldViewPosition.x);
                else
                    d3d_vertex_buffer[i].specular = 0;
                d3d_vertex_buffer[i].texcoord.x = VertexRenderList[i].u;
                d3d_vertex_buffer[i].texcoord.y = VertexRenderList[i].v;
            }

            if (pFace->uAttributes & FACE_OUTLINED) {
                int color;
                if (OS_GetTime() % 300 >= 150)
                    color = 0xFFFF2020;
                else
                    color = 0xFF901010;

                for (uint i = 0; i < uNumVertices; ++i)
                    d3d_vertex_buffer[i].diffuse = color;
            }

            ErrD3D(pRenderD3D->pDevice->SetTexture(
                0, texture->GetDirect3DTexture()));
            ErrD3D(pRenderD3D->pDevice->DrawPrimitive(
                D3DPT_TRIANGLEFAN,
                D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_SPECULAR | D3DFVF_TEX1,
                d3d_vertex_buffer, uNumVertices, D3DDP_DONOTLIGHT));
        } else {
            for (uint i = 0; i < uNumVertices; ++i) {
                d3d_vertex_buffer[i].pos.x =
                    VertexRenderList[i].vWorldViewProjX;
                d3d_vertex_buffer[i].pos.y =
                    VertexRenderList[i].vWorldViewProjY;
                d3d_vertex_buffer[i].pos.z =
                    1.0 -
                    1.0 / ((VertexRenderList[i].vWorldViewPosition.x * 1000) /
                           pIndoorCameraD3D->GetFarClip());
                d3d_vertex_buffer[i].rhw =
                    1.0 /
                    (VertexRenderList[i].vWorldViewPosition.x + 0.0000001);
                d3d_vertex_buffer[i].diffuse = GetActorTintColor(
                    pPolygon->dimming_level, 0,
                    VertexRenderList[i].vWorldViewPosition.x, 0, 0);
                if (config->is_using_specular)
                    d3d_vertex_buffer[i].specular = sub_47C3D7_get_fog_specular(
                        0, 0, VertexRenderList[i].vWorldViewPosition.x);
                else
                    d3d_vertex_buffer[i].specular = 0;
                d3d_vertex_buffer[i].texcoord.x = VertexRenderList[i].u;
                d3d_vertex_buffer[i].texcoord.y = VertexRenderList[i].v;
            }

            ErrD3D(pRenderD3D->pDevice->SetRenderState(D3DRENDERSTATE_ZWRITEENABLE, FALSE));
            ErrD3D(pRenderD3D->pDevice->SetTextureStageState(0, D3DTSS_ADDRESS,
                                                             D3DTADDRESS_WRAP));
            if (config->is_using_specular)
                ErrD3D(pRenderD3D->pDevice->SetRenderState(D3DRENDERSTATE_FOGENABLE, FALSE));

            ErrD3D(pRenderD3D->pDevice->SetTexture(0, nullptr));
            ErrD3D(pRenderD3D->pDevice->DrawPrimitive(
                D3DPT_TRIANGLEFAN,
                D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_SPECULAR | D3DFVF_TEX1,
                d3d_vertex_buffer, uNumVertices, D3DDP_DONOTLIGHT));
            // v50 = (const char *)v5->pRenderD3D->pDevice;
            ErrD3D(pRenderD3D->pDevice->SetRenderState(D3DRENDERSTATE_CULLMODE,
                                                       D3DCULL_NONE));
            // (*(void (**)(void))(*(int *)v50 + 88))();
            lightmap_builder->DrawLightmaps(-1);
            for (uint i = 0; i < uNumVertices; ++i) {
                d3d_vertex_buffer[i].diffuse = sCorrectedColor;
            }
            ErrD3D(pRenderD3D->pDevice->SetTexture(
                0, texture->GetDirect3DTexture()));
            ErrD3D(pRenderD3D->pDevice->SetTextureStageState(0, D3DTSS_ADDRESS,
                                                             D3DTADDRESS_WRAP));
            if (!config->is_using_specular)
                ErrD3D(pRenderD3D->pDevice->SetRenderState(D3DRENDERSTATE_ZWRITEENABLE, TRUE));

            ErrD3D(pRenderD3D->pDevice->SetRenderState(
                D3DRENDERSTATE_ALPHABLENDENABLE, TRUE));
            ErrD3D(pRenderD3D->pDevice->SetRenderState(D3DRENDERSTATE_SRCBLEND,
                                                       D3DBLEND_ZERO));
            ErrD3D(pRenderD3D->pDevice->SetRenderState(D3DRENDERSTATE_DESTBLEND,
                                                       D3DBLEND_SRCCOLOR));
            ErrD3D(pRenderD3D->pDevice->DrawPrimitive(
                D3DPT_TRIANGLEFAN,
                D3DFVF_XYZRHW | D3DFVF_TEX1 | D3DFVF_DIFFUSE | D3DFVF_SPECULAR,
                d3d_vertex_buffer, uNumVertices, D3DDP_DONOTLIGHT));
            if (config->is_using_specular) {
                ErrD3D(pRenderD3D->pDevice->SetRenderState(D3DRENDERSTATE_ZWRITEENABLE, TRUE));

                for (uint i = 0; i < uNumVertices; ++i) {
                    d3d_vertex_buffer[i].diffuse =
                        render->uFogColor |
                        d3d_vertex_buffer[i].specular & 0xFF000000;
                    d3d_vertex_buffer[i].specular = 0;
                }

                ErrD3D(pRenderD3D->pDevice->SetTexture(0, nullptr));
                ErrD3D(pRenderD3D->pDevice->SetRenderState(
                    D3DRENDERSTATE_SRCBLEND, D3DBLEND_INVSRCALPHA));
                ErrD3D(pRenderD3D->pDevice->SetRenderState(
                    D3DRENDERSTATE_DESTBLEND, D3DBLEND_SRCALPHA));
                ErrD3D(pRenderD3D->pDevice->DrawPrimitive(
                    D3DPT_TRIANGLEFAN,
                    D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_SPECULAR |
                        D3DFVF_TEX1,
                    d3d_vertex_buffer, uNumVertices, D3DDP_DONOTLIGHT));
                ErrD3D(pRenderD3D->pDevice->SetRenderState(
                    D3DRENDERSTATE_FOGENABLE, TRUE));
                // v40 = render->pRenderD3D->pDevice->lpVtbl;
                v41 = GetLevelFogColor();
                pRenderD3D->pDevice->SetRenderState(
                    D3DRENDERSTATE_FOGCOLOR, GetLevelFogColor() & 0xFFFFFF);
                pRenderD3D->pDevice->SetRenderState(D3DRENDERSTATE_FOGTABLEMODE,
                                                    0);
            }
            ErrD3D(pRenderD3D->pDevice->SetRenderState(D3DRENDERSTATE_SRCBLEND,
                                                       D3DBLEND_ONE));
            ErrD3D(pRenderD3D->pDevice->SetRenderState(D3DRENDERSTATE_DESTBLEND,
                                                       D3DBLEND_ZERO));
            ErrD3D(pRenderD3D->pDevice->SetRenderState(
                D3DRENDERSTATE_ALPHABLENDENABLE, FALSE));
        }
    }
}

Render::Render()
    : RenderBase() {
    this->pDirectDraw4 = nullptr;
    this->pFrontBuffer4 = nullptr;
    this->pBackBuffer4 = nullptr;
    this->bWindowMode = 1;
    this->pActiveZBuffer = nullptr;
    this->pDefaultZBuffer = nullptr;
    this->pRenderD3D = 0;
    this->uNumD3DSceneBegins = 0;
    this->bRequiredTextureStagesAvailable = 0;

    uNumBillboardsToDraw = 0;

    hd_water_tile_id = -1;
    hd_water_current_frame = 0;

    Gdiplus::GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);
}

Render::~Render() {
    free(pDefaultZBuffer);
    Release();
    Gdiplus::GdiplusShutdown(gdiplusToken);
}

bool Render::Initialize(OSWindow *window) {
    if (!RenderBase::Initialize(window)) {
        return false;
    }

    uDesiredDirect3DDevice = OS_GetAppInt("D3D Device", 0);

    PostInitialization();

    return true;
}

void Render::ClearBlack() { pRenderD3D->ClearTarget(true, 0, false, 0.0); }

void Render::PresentBlackScreen() {
    RECT dest_rect = {0};
    GetWindowRect((HWND)window->GetWinApiHandle(), &dest_rect);

    DDBLTFX lpDDBltFx = {0};
    lpDDBltFx.dwSize = sizeof(DDBLTFX);
    lpDDBltFx.dwFillColor = 0;
    pBackBuffer4->Blt(&dest_rect, NULL, NULL, DDBLT_COLORFILL, &lpDDBltFx);
    render->Present();
}

void Render::SavePCXScreenshot() {
    char file_name[40];
    sprintf(file_name, "screen%0.2i.pcx", ScreenshotFileNumber++ % 100);

    SaveWinnersCertificate(file_name);
}

void Render::SaveWinnersCertificate(const char *file_name) {
    BeginScene();
    //  SavePCXImage32(file_name, (uint16_t*)render->pTargetSurface,
    //  render->GetRenderWidth(), render->GetRenderHeight());
    EndScene();
}

void Render::SavePCXImage32(const String &filename, uint16_t *picture_data,
                            int width, int height) {
    FILE *result = fopen(filename.c_str(), "wb");
    if (result == nullptr) {
        return;
    }

    unsigned int pcx_data_size = width * height * 5;
    uint8_t *pcx_data = new uint8_t[pcx_data_size];
    unsigned int pcx_data_real_size = 0;
    PCX::Encode32(picture_data, width, height, pcx_data, pcx_data_size,
                  &pcx_data_real_size);
    fwrite(pcx_data, pcx_data_real_size, 1, result);
    delete[] pcx_data;
    fclose(result);
}

void Render::SavePCXImage16(const String &filename, uint16_t *picture_data,
                            int width, int height) {
    FILE *result = fopen(filename.c_str(), "wb");
    if (result == nullptr) {
        return;
    }

    unsigned int pcx_data_size = width * height * 5;
    uint8_t *pcx_data = new uint8_t[pcx_data_size];
    unsigned int pcx_data_real_size = 0;
    PCX::Encode16(picture_data, width, height, pcx_data, pcx_data_size,
                  &pcx_data_real_size);
    fwrite(pcx_data, pcx_data_real_size, 1, result);
    delete[] pcx_data;
    fclose(result);
}

void Render::ClearTarget(unsigned int uColor) {
    pRenderD3D->ClearTarget(true, uColor, false, 0.0);
}

void Render::Present() {
    pBeforePresentFunction();
    if (pRenderD3D) {
        pRenderD3D->Present(false);
    } else {
        assert(false);
    }
}

void Render::CreateZBuffer() {
    if (!pDefaultZBuffer) {
        pDefaultZBuffer = pActiveZBuffer = (int *)malloc(0x12C000);
        memset32(pActiveZBuffer, 0xFFFF0000,
                 0x4B000u);  //    // inlined Render::ClearActiveZBuffer
                             //    (mm8::004A085B)
    }
}

void Render::Release() {
    if (pRenderD3D) {
        pRenderD3D->ClearTarget(true, 0, false, 1.0);
        pRenderD3D->Present(0);
        pRenderD3D->ClearTarget(true, 0, false, 1.0);
        this->pBackBuffer4 = nullptr;
        this->pFrontBuffer4 = nullptr;
        this->pDirectDraw4 = nullptr;
        if (pRenderD3D) {
            pRenderD3D->Release();
            delete pRenderD3D;
        }
        pRenderD3D = nullptr;
    }
}

void Present32(uint32_t *src, unsigned int src_pitch, uint32_t *dst,
               unsigned int dst_pitch) {
     // return;

    for (uint y = 0; y < 8; ++y) {
        memcpy(dst + y * dst_pitch, src + y * src_pitch,
               src_pitch * sizeof(uint32_t));
    }

    for (uint y = 8; y < 352; ++y) {
        memcpy(dst + y * dst_pitch, src + y * src_pitch, 8 * sizeof(uint32_t));
        memcpy(dst + 8 + game_viewport_width + y * dst_pitch,
               src + 8 + game_viewport_width + y * src_pitch,
               174 /*172*/ * sizeof(uint32_t));
    }

    for (uint y = 352; y < 480; ++y) {
        memcpy(dst + y * dst_pitch, src + y * src_pitch,
               src_pitch * sizeof(uint32_t));
    }

    for (uint y = pViewport->uViewportTL_Y; y < pViewport->uViewportBR_Y + 1;
         ++y) {
        for (uint x = pViewport->uViewportTL_X; x < pViewport->uViewportBR_X;
             ++x) {
            if (src[x + y * src_pitch] !=
                0xFF00FCF8) {  // FFF8FCF8 =  Color32(Color16(g_mask | b_mask))
                dst[x + y * dst_pitch] = src[x + y * src_pitch];
            }
        }
    }
}

void Present_NoColorKey() {
    Render *r = (Render *)render.get();

    DDSURFACEDESC2 Dst = {0};
    Dst.dwSize = sizeof(Dst);
    if (r->LockSurface_DDraw4(r->pBackBuffer4, &Dst, DDLOCK_WAIT)) {
        Gdiplus::Rect rect(0, 0, Dst.dwWidth, Dst.dwHeight);
        Gdiplus::BitmapData bitmapData;
        r->p2DSurface->LockBits(&rect, Gdiplus::ImageLockModeRead,
                                PixelFormat32bppARGB, &bitmapData);
        Present32((uint32_t *)bitmapData.Scan0, bitmapData.Width,
                  (uint32_t *)Dst.lpSurface, Dst.lPitch / 4);
        r->p2DSurface->UnlockBits(&bitmapData);
        ErrD3D(r->pBackBuffer4->Unlock(NULL));
    }
}

bool Render::InitializeFullscreen() {
    this->pBackBuffer4 = nullptr;
    this->pFrontBuffer4 = nullptr;
    this->pDirectDraw4 = nullptr;
    Release();
    this->window = window;
    CreateZBuffer();

    pRenderD3D = new RenderD3D;

    int v29 = -1;
    RenderD3D__DevInfo *v7 = pRenderD3D->pAvailableDevices;
    bool v8 = false;
    if (pRenderD3D->pAvailableDevices[uDesiredDirect3DDevice]
            .bIsDeviceCompatible) {
        v8 = pRenderD3D->CreateDevice(uDesiredDirect3DDevice, true, window);
    } else {
        if (v7[1].bIsDeviceCompatible) {
            v8 = pRenderD3D->CreateDevice(1, true, window);
        } else {
            if (!v7->bIsDeviceCompatible)
                Error("There aren't any D3D devices to create.");

            v8 = pRenderD3D->CreateDevice(0, true, window);
        }
    }
    if (!v8) {
        Error("D3Drend->Init failed.");
    }

    pBackBuffer4 = pRenderD3D->pBackBuffer;
    pFrontBuffer4 = pRenderD3D->pFrontBuffer;
    pDirectDraw4 = pRenderD3D->pHost;

    unsigned int v10 = pRenderD3D->GetDeviceCaps();
    if (v10 & 1) {
        if (pRenderD3D) {
            pRenderD3D->Release();
            delete pRenderD3D;
        }
        pRenderD3D = nullptr;
        pBackBuffer4 = nullptr;
        pFrontBuffer4 = nullptr;
        pDirectDraw4 = nullptr;
        Error("Direct3D renderer:  The device failed to return capabilities.");
    }
    if (v10 & 0x3E) {
        if (pRenderD3D) {
            pRenderD3D->Release();
            delete pRenderD3D;
        }
        pRenderD3D = nullptr;
        pBackBuffer4 = nullptr;
        pFrontBuffer4 = nullptr;
        pDirectDraw4 = nullptr;
        Error(
            "Direct3D renderer:  The device doesn't support the necessary "
            "alpha blending modes.");
    }
    if ((v10 & 0x80) != 0) {
        if (pRenderD3D) {
            pRenderD3D->Release();
            delete pRenderD3D;
        }
        pRenderD3D = nullptr;
        pBackBuffer4 = nullptr;
        pFrontBuffer4 = nullptr;
        pDirectDraw4 = nullptr;
        Error(
            "Direct3D renderer:  The device doesn't support non-square "
            "textures.");
    }

    bRequiredTextureStagesAvailable = CheckTextureStages();

    D3DDEVICEDESC halCaps = {0};
    halCaps.dwSize = sizeof(halCaps);

    D3DDEVICEDESC refCaps = {0};
    refCaps.dwSize = sizeof(refCaps);

    ErrD3D(pRenderD3D->pDevice->GetCaps(&halCaps, &refCaps));

    uMinDeviceTextureDim = halCaps.dwMinTextureWidth;
    if ((unsigned int)halCaps.dwMinTextureWidth >= halCaps.dwMinTextureHeight)
        uMinDeviceTextureDim = halCaps.dwMinTextureHeight;
    uMinDeviceTextureDim = halCaps.dwMaxTextureWidth;
    if ((unsigned int)halCaps.dwMaxTextureWidth < halCaps.dwMaxTextureHeight)
        uMinDeviceTextureDim = halCaps.dwMaxTextureHeight;
    if ((unsigned int)uMinDeviceTextureDim < 4) uMinDeviceTextureDim = 4;

    ErrD3D(pRenderD3D->pDevice->SetRenderState(D3DRENDERSTATE_ZENABLE, true));
    ErrD3D(
        pRenderD3D->pDevice->SetRenderState(D3DRENDERSTATE_ZWRITEENABLE, true));
    ErrD3D(pRenderD3D->pDevice->SetRenderState(D3DRENDERSTATE_ZFUNC, 2));
    ErrD3D(pRenderD3D->pDevice->SetRenderState(D3DRENDERSTATE_SPECULARENABLE,
                                               false));
    ErrD3D(pRenderD3D->pDevice->SetRenderState(D3DRENDERSTATE_COLORKEYENABLE,
                                               false));
    ErrD3D(pRenderD3D->pDevice->SetRenderState(D3DRENDERSTATE_ALPHATESTENABLE,
                                               false));
    ErrD3D(pRenderD3D->pDevice->SetRenderState(D3DRENDERSTATE_CULLMODE, 1));
    ErrD3D(pRenderD3D->pDevice->SetTextureStageState(0, D3DTSS_MAGFILTER, 2));
    ErrD3D(pRenderD3D->pDevice->SetTextureStageState(0, D3DTSS_MINFILTER, 2));
    ErrD3D(pRenderD3D->pDevice->SetTextureStageState(0, D3DTSS_MIPFILTER, 3));
    ErrD3D(pRenderD3D->pDevice->SetTextureStageState(0, D3DTSS_ALPHAARG1, 2));
    ErrD3D(pRenderD3D->pDevice->SetTextureStageState(0, D3DTSS_ALPHAARG2, 0));
    ErrD3D(pRenderD3D->pDevice->SetTextureStageState(0, D3DTSS_ALPHAOP, 2));
    ErrD3D(pRenderD3D->pDevice->SetTextureStageState(0, D3DTSS_COLORARG1, 2));
    ErrD3D(pRenderD3D->pDevice->SetTextureStageState(0, D3DTSS_COLORARG2, 0));
    ErrD3D(pRenderD3D->pDevice->SetTextureStageState(0, D3DTSS_COLOROP, 4));

    ddpfPrimarySuface.dwSize = sizeof(DDPIXELFORMAT);
    GetTargetPixelFormat(&ddpfPrimarySuface);
    ParseTargetPixelFormat();

    if (!pRenderD3D) {
        __debugbreak();
        pBeforePresentFunction = 0;
    }

    p2DSurface = new Gdiplus::Bitmap(window->GetWidth(), window->GetHeight(),
                                     PixelFormat32bppRGB);
    p2DGraphics = new Gdiplus::Graphics(p2DSurface);
    pBeforePresentFunction = Present_NoColorKey;

    bWindowMode = 0;
    pParty->uFlags |= 2;
    pViewport->SetFOV(_6BE3A0_fov);

    return true;
}

bool Render::DrawLightmap(Lightmap *pLightmap, Vec3_float_ *pColorMult,
                          float z_bias) {
    // For outdoor terrain and indoor light (VII)(VII)
    if (pLightmap->NumVertices < 3) {
        log->Warning(L"Lightmap uNumVertices < 3");
        return false;
    }

    unsigned int uLightmapColorMaskR = (pLightmap->uColorMask >> 16) & 0xFF;
    unsigned int uLightmapColorMaskG = (pLightmap->uColorMask >> 8) & 0xFF;
    unsigned int uLightmapColorMaskB = pLightmap->uColorMask & 0xFF;

    unsigned int uLightmapColorR = floorf(
        uLightmapColorMaskR * pLightmap->fBrightness * pColorMult->x + 0.5f);
    unsigned int uLightmapColorG = floorf(
        uLightmapColorMaskG * pLightmap->fBrightness * pColorMult->y + 0.5f);
    unsigned int uLightmapColorB = floorf(
        uLightmapColorMaskB * pLightmap->fBrightness * pColorMult->z + 0.5f);

    RenderVertexD3D3 pVerticesD3D[64];
    for (uint i = 0; i < pLightmap->NumVertices; ++i) {
        float v18;
        if (fabs(z_bias) < 1e-5) {
            v18 = 1.0 - 1.0 / ((1.0f / 16192.0) * pLightmap->pVertices[i].vWorldViewPosition.x * 1000.0);
        } else {
            v18 = 1.0 - 1.0 / ((1.0f / 16192.0) * pLightmap->pVertices[i].vWorldViewPosition.x * 1000.0) - z_bias;

            if (v18 < 0.000099999997) {
                v18 = 0.000099999997;
            }
        }

        pVerticesD3D[i].pos.x = pLightmap->pVertices[i].vWorldViewProjX;
        pVerticesD3D[i].pos.y = pLightmap->pVertices[i].vWorldViewProjY;
        pVerticesD3D[i].pos.z = v18;

        pVerticesD3D[i].rhw = 1.0 / pLightmap->pVertices[i].vWorldViewPosition.x;
        pVerticesD3D[i].diffuse = (uLightmapColorR << 16) | (uLightmapColorG << 8) | uLightmapColorB;
        pVerticesD3D[i].specular = 0;

        pVerticesD3D[i].texcoord.x = pLightmap->pVertices[i].u;
        pVerticesD3D[i].texcoord.y = pLightmap->pVertices[i].v;
    }

    int dwFlags = D3DDP_DONOTLIGHT;
    if (uCurrentlyLoadedLevelType == LEVEL_Indoor) {
        dwFlags |= D3DDP_DONOTCLIP | D3DDP_DONOTUPDATEEXTENTS;
    }

    ErrD3D(pRenderD3D->pDevice->DrawPrimitive(
        D3DPT_TRIANGLEFAN,
        D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_SPECULAR | D3DFVF_TEX1,
        pVerticesD3D, pLightmap->NumVertices, dwFlags));

    return true;
}

// blue mask
void Render::am_Blt_Chroma(Rect *pSrcRect, Point *pTargetPoint, int a3, int blend_mode) {
    uint16_t *pSrc;          // eax@2
    int uSrcTotalWidth = 0;      // ecx@4
    unsigned int v10;        // esi@9
    int v21;                 // [sp+Ch] [bp-18h]@8
    uint16_t *src_surf_pos;  // [sp+10h] [bp-14h]@9
    int32_t src_width;       // [sp+14h] [bp-10h]@3
    int32_t src_height;      // [sp+18h] [bp-Ch]@3
    int uSrcPitch;           // [sp+1Ch] [bp-8h]@5

    if (!pArcomageGame->pBlit_Copy_pixels) {
        __debugbreak();
        return;
    }

    src_width = pSrcRect->z - pSrcRect->x;
    src_height = pSrcRect->w - pSrcRect->y;

    /*if (pArcomageGame->pBlit_Copy_pixels == pArcomageGame->pBackgroundPixels)
        uSrcTotalWidth = pArcomageGame->pGameBackground->GetWidth();
    else*/ if (pArcomageGame->pBlit_Copy_pixels == pArcomageGame->pSpritesPixels)
        uSrcTotalWidth = pArcomageGame->pSprites->GetWidth();

    pSrc = pArcomageGame->pBlit_Copy_pixels;
    uSrcPitch = uSrcTotalWidth;
    src_surf_pos = &pSrc[pSrcRect->x + uSrcPitch * pSrcRect->y];
    v10 = 0x1F;
    v21 = (uTargetGBits != 6 ? 0x31EF : 0x7BEF);

    Image *temp = Image::Create(src_width, src_height, IMAGE_FORMAT_A8R8G8B8);
    uint32_t *temppix = (uint32_t *)temp->GetPixels(IMAGE_FORMAT_A8R8G8B8);

    if (blend_mode == 2) {
        uSrcPitch = (uSrcPitch - src_width);
        for (int i = 0; i < src_height; ++i) {
            for (int j = 0; j < src_width; ++j) {
                if (*src_surf_pos != v10) {
                    if (pTargetPoint->x + j >= 0 &&
                        pTargetPoint->x + j <= window->GetWidth() - 1 &&
                        pTargetPoint->y + i >= 0 &&
                        pTargetPoint->y + i <= window->GetHeight() - 1)
                        temppix[j + i * src_width] = Color32(*src_surf_pos);
                }
                ++src_surf_pos;
            }
            src_surf_pos += uSrcPitch;
        }
    } else {
        uSrcPitch = (uSrcPitch - src_width);
        for (int i = 0; i < src_height; ++i) {
            for (int j = 0; j < src_width; ++j) {
                if (*src_surf_pos != v10) {
                    if (pTargetPoint->x + j >= 0 &&//
                        pTargetPoint->x + j <= window->GetWidth() - 1 &&//
                        pTargetPoint->y + i >= 0 &&//
                        pTargetPoint->y + i <= window->GetHeight() - 1)//
                        temppix[j + i * src_width] = Color32((0x7BEF & (*src_surf_pos / 2)));//
                }//
                ++src_surf_pos;//
            }//
            src_surf_pos += uSrcPitch;//
        }//
    }//
    render->DrawTextureAlphaNew(pTargetPoint->x / 640., pTargetPoint->y / 480., temp);//
    temp->Release();//
}

bool Render::SwitchToWindow() {
    // pParty->uFlags |= PARTY_FLAGS_1_0002;
    pViewport->SetFOV(_6BE3A0_fov);
    Release();

    pBackBuffer4 = nullptr;
    pFrontBuffer4 = nullptr;
    pDirectDraw4 = nullptr;
    CreateZBuffer();
    pRenderD3D = new RenderD3D;

    bool v7 = false;
    if (pRenderD3D->pAvailableDevices[uDesiredDirect3DDevice]
            .bIsDeviceCompatible &&
        uDesiredDirect3DDevice != 1) {
        v7 = pRenderD3D->CreateDevice(uDesiredDirect3DDevice, true, window);
    } else {
        if (!pRenderD3D->pAvailableDevices[0].bIsDeviceCompatible) {
            Error("There aren't any D3D devices to init.");
        }
        v7 = pRenderD3D->CreateDevice(0, true, window);
    }
    if (!v7) Error("D3Drend->Init failed.");

    pBackBuffer4 = pRenderD3D->pBackBuffer;
    pFrontBuffer4 = pRenderD3D->pFrontBuffer;
    pDirectDraw4 = pRenderD3D->pHost;

    unsigned int device_caps = pRenderD3D->GetDeviceCaps();
    if (device_caps & 1) {
        if (pRenderD3D) {
            pRenderD3D->Release();
            delete pRenderD3D;
        }
        pRenderD3D = nullptr;
        pBackBuffer4 = nullptr;
        pFrontBuffer4 = nullptr;
        pDirectDraw4 = nullptr;
        Error("Direct3D renderer:  The device failed to return capabilities.");
    }
    if (device_caps & 0x3E) {
        if (pRenderD3D) {
            pRenderD3D->Release();
            delete pRenderD3D;
        }
        pRenderD3D = nullptr;
        pBackBuffer4 = nullptr;
        pFrontBuffer4 = nullptr;
        pDirectDraw4 = nullptr;
        Error(
            "Direct3D renderer:  The device doesn't support the necessary "
            "alpha blending modes.");
    }
    if (device_caps & 0x80) {
        if (pRenderD3D) {
            pRenderD3D->Release();
            delete pRenderD3D;
        }
        pRenderD3D = nullptr;
        pBackBuffer4 = nullptr;
        pFrontBuffer4 = nullptr;
        pDirectDraw4 = nullptr;
        Error(
            "Direct3D renderer:  The device doesn't support non-square "
            "textures.");
    }

    bRequiredTextureStagesAvailable = CheckTextureStages();

    D3DDEVICEDESC halCaps = {0};
    halCaps.dwSize = sizeof(halCaps);

    D3DDEVICEDESC refCaps = {0};
    refCaps.dwSize = sizeof(refCaps);

    ErrD3D(pRenderD3D->pDevice->GetCaps(&halCaps, &refCaps));
    int v12 = halCaps.dwMinTextureWidth;
    if ((unsigned int)halCaps.dwMinTextureWidth > halCaps.dwMinTextureHeight)
        v12 = halCaps.dwMinTextureHeight;
    uMinDeviceTextureDim = v12;
    int v13 = halCaps.dwMaxTextureWidth;
    if ((unsigned int)halCaps.dwMaxTextureWidth < halCaps.dwMaxTextureHeight)
        v13 = halCaps.dwMaxTextureHeight;
    uMaxDeviceTextureDim = v13;
    ErrD3D(pRenderD3D->pDevice->SetRenderState(D3DRENDERSTATE_ZENABLE, 1));
    ErrD3D(pRenderD3D->pDevice->SetRenderState(D3DRENDERSTATE_ZWRITEENABLE, 1));
    ErrD3D(pRenderD3D->pDevice->SetRenderState(D3DRENDERSTATE_ZFUNC, 2));
    ErrD3D(
        pRenderD3D->pDevice->SetRenderState(D3DRENDERSTATE_SPECULARENABLE, 0));
    ErrD3D(
        pRenderD3D->pDevice->SetRenderState(D3DRENDERSTATE_COLORKEYENABLE, 0));
    ErrD3D(pRenderD3D->pDevice->SetRenderState(D3DRENDERSTATE_CULLMODE, 1));
    ErrD3D(pRenderD3D->pDevice->SetTextureStageState(0, D3DTSS_MAGFILTER, 2));
    ErrD3D(pRenderD3D->pDevice->SetTextureStageState(0, D3DTSS_MINFILTER, 2));
    ErrD3D(pRenderD3D->pDevice->SetTextureStageState(0, D3DTSS_MIPFILTER, 3));
    ErrD3D(pRenderD3D->pDevice->SetTextureStageState(0, D3DTSS_ALPHAARG1, 2));
    ErrD3D(pRenderD3D->pDevice->SetTextureStageState(0, D3DTSS_ALPHAARG2, 0));
    ErrD3D(pRenderD3D->pDevice->SetTextureStageState(0, D3DTSS_ALPHAOP, 2));
    ErrD3D(pRenderD3D->pDevice->SetTextureStageState(0, D3DTSS_COLORARG1, 2));
    ErrD3D(pRenderD3D->pDevice->SetTextureStageState(0, D3DTSS_COLORARG2, 0));
    ErrD3D(pRenderD3D->pDevice->SetTextureStageState(0, D3DTSS_COLOROP, 4));

    ddpfPrimarySuface.dwSize = sizeof(DDPIXELFORMAT);
    GetTargetPixelFormat(&ddpfPrimarySuface);
    ParseTargetPixelFormat();

    if (!pRenderD3D) {
        __debugbreak();
    }

    p2DSurface = new Gdiplus::Bitmap(window->GetWidth(), window->GetHeight(),
                                     PixelFormat32bppRGB);
    p2DGraphics = new Gdiplus::Graphics(p2DSurface);
    pBeforePresentFunction = Present_NoColorKey;

    return true;
}

void Render::RasterLine2D(int uX, int uY, int uZ, int uW, uint16_t color) {
    // change to 32bit clor input??

    unsigned int b = (color & 0x1F) << 3;
    unsigned int g = ((color >> 5) & 0x3F) << 2;
    unsigned int r = ((color >> 11) & 0x1F) << 3;
    Gdiplus::Pen pen(Gdiplus::Color(r, g, b));
    p2DGraphics->DrawLine(&pen, uX, uY, uZ, uW);
}

void Render::ClearZBuffer(int a2, int a3) {
    memset32(this->pActiveZBuffer, -65536, 0x4B000);
}

void Render::ParseTargetPixelFormat() {
    int v2 = 0;
    unsigned int uRedMask = ddpfPrimarySuface.dwRBitMask;
    this->uTargetBBits = 0;
    do {
        if ((1 << v2) & uRedMask) {
            ++this->uTargetRBits;
        }
        ++v2;
    } while (v2 < 32);

    unsigned int uGreenMask = ddpfPrimarySuface.dwGBitMask;
    int v5 = 0;
    do {
        if ((1 << v5) & uGreenMask) ++this->uTargetGBits;
        ++v5;
    } while (v5 < 32);

    unsigned int uBlueMask = ddpfPrimarySuface.dwBBitMask;
    int v7 = 0;
    do {
        if ((1 << v7) & uBlueMask) ++this->uTargetBBits;
        ++v7;
    } while (v7 < 32);
}

bool Render::LockSurface_DDraw4(IDirectDrawSurface4 *pSurface,
                                DDSURFACEDESC2 *pDesc,
                                unsigned int uLockFlags) {
    HRESULT result = pSurface->Lock(NULL, pDesc, uLockFlags, NULL);
    /*
    Когда объект DirectDrawSurface теряет поверхностную память, методы возвратят
    DDERR_SURFACELOST и не выполнят никакую другую функцию. Метод
    IDirectDrawSurface::Restore перераспределит поверхностную память и повторно
    присоединит ее к объекту DirectDrawSurface.
    */
    if (result == DDERR_SURFACELOST) {
        HRESULT v6 =
            pSurface->Restore();  //Восстанавливает потерянную поверхность. Это
                                  //происходит, когда поверхностная память,
                                  //связанная с объектом DirectDrawSurface была
                                  //освобождена.
        if (v6) {
            if (v6 !=
                DDERR_IMPLICITLYCREATED) {  // DDERR_IMPLICITLYCREATED -
                                            // Поверхность не может быть
                                            // восстановлена, потому что она -
                                            // неявно созданная поверхность.
                result = (bool)memset(pDesc, 0, 4);
                return 0;
            }
            this->pFrontBuffer4->Restore();
            pSurface->Restore();
        }
        result = pSurface->Lock(NULL, pDesc, DDLOCK_WAIT, NULL);
        if (result == DDERR_INVALIDRECT ||
            result ==
                DDERR_SURFACEBUSY) {  // DDERR_SURFACEBUSY - Доступ к этой
                                      // поверхности отказан, потому что
                                      // поверхность блокирована другой нитью.
                                      // DDERR_INVALIDRECT - Обеспечиваемый
                                      // прямоугольник недопустим.
            result = (bool)memset(pDesc, 0, 4);
            return 0;
        }
        ErrD3D(result);
        if (result) {
            result = (bool)memset(pDesc, 0, 4);
            return result;
        }
        if (pRenderD3D) {
            pRenderD3D->HandleLostResources();
        }
        result = this->pDirectDraw4->RestoreAllSurfaces();
    } else {
        if (result) {
            if (result == DDERR_INVALIDRECT || result == DDERR_SURFACEBUSY) {
                result = (bool)memset(pDesc, 0, 4);
                return result;
            }
            ErrD3D(result);
        }
    }
    return true;
}

void Render::CreateClipper(OSWindow *window) {
    ErrD3D(pDirectDraw4->CreateClipper(0, &pDDrawClipper, NULL));
    ErrD3D(pDDrawClipper->SetHWnd(0, (HWND)window->GetWinApiHandle()));
    ErrD3D(pFrontBuffer4->SetClipper(pDDrawClipper));
}

void Render::GetTargetPixelFormat(DDPIXELFORMAT *pOut) {
    pFrontBuffer4->GetPixelFormat(pOut);
}

void Render::RestoreFrontBuffer() {
    if (pFrontBuffer4->IsLost() == DDERR_SURFACELOST) {
        pFrontBuffer4->Restore();
    }
}

void Render::RestoreBackBuffer() {
    if (pBackBuffer4->IsLost() == DDERR_SURFACELOST) {
        pBackBuffer4->Restore();
    }
}

void Render::BltBackToFontFast(int a2, int a3, Rect *pSrcRect) {
    IDirectDrawSurface *pFront;
    IDirectDrawSurface *pBack;
    pFront = (IDirectDrawSurface *)this->pFrontBuffer4;
    pBack = (IDirectDrawSurface *)this->pBackBuffer4;
    pFront->BltFast(NULL, NULL, pBack, (RECT *)pSrcRect, DDBLTFAST_WAIT);
}

unsigned int Render::GetBillboardDrawListSize() {
    return render->uNumBillboardsToDraw;
}

unsigned int Render::GetParentBillboardID(unsigned int uBillboardID) {
    return render->pBillboardRenderListD3D[uBillboardID].sParentBillboardID;
}

void Render::BeginSceneD3D() {
    if (!uNumD3DSceneBegins++) {
        pRenderD3D->ClearTarget(true, 0x00F08020, true, 1.0);
        render->uNumBillboardsToDraw = 0;
        pRenderD3D->pDevice->BeginScene();

        if (uCurrentlyLoadedLevelType == LEVEL_Outdoor)
            uFogColor = GetLevelFogColor();
        else
            uFogColor = 0;

        if (uFogColor & 0xFF000000) {
            pRenderD3D->pDevice->SetRenderState(D3DRENDERSTATE_FOGENABLE, 1);
            pRenderD3D->pDevice->SetRenderState(D3DRENDERSTATE_FOGCOLOR,
                                                uFogColor & 0xFFFFFF);
            pRenderD3D->pDevice->SetRenderState(D3DRENDERSTATE_FOGTABLEMODE, 0);
            SetUsingSpecular(true);
        } else {
            pRenderD3D->pDevice->SetRenderState(D3DRENDERSTATE_FOGENABLE, 0);
            SetUsingSpecular(false);
        }
    }
}

void Render::DrawBillboards_And_MaybeRenderSpecialEffects_And_EndScene() {
    --uNumD3DSceneBegins;
    if (uNumD3DSceneBegins == 0) {
        engine->draw_debug_outlines();
        DoRenderBillboards_D3D();
        spell_fx_renderer->RenderSpecialEffects();
        pRenderD3D->pDevice->EndScene();
    }
}

unsigned int Render::GetActorTintColor(int DimLevel, int tint, float WorldViewX, int a5, RenderBillboard *Billboard) {
    // GetActorTintColor(int max_dimm, int min_dimm, float distance, int a4, RenderBillboard *a5)
    return ::GetActorTintColor(DimLevel, tint, WorldViewX, a5, Billboard);
}

void Render::DrawTerrainPolygon(struct Polygon *a4, bool transparent,
                                bool clampAtTextureBorders) {
    int v11;           // eax@5
    unsigned int v45;  // eax@28

    unsigned int uNumVertices = a4->uNumVertices;

    auto texture = (TextureD3D *)a4->texture;

    if (!this->uNumD3DSceneBegins) return;
    if (uNumVertices < 3) return;

    if (_4D864C_force_sw_render_rules && engine->config->Flag1_1()) {
        v11 =
            ::GetActorTintColor(a4->dimming_level, 0,
                                VertexRenderList[0].vWorldViewPosition.x, 0, 0);
        lightmap_builder->DrawLightmaps(v11 /*, 0*/);
    } else if (transparent || !lightmap_builder->StationaryLightsCount ||
        _4D864C_force_sw_render_rules && engine->config->Flag1_2()) {
        if (clampAtTextureBorders)
            this->pRenderD3D->pDevice->SetTextureStageState(0, D3DTSS_ADDRESS,
                                                            D3DTADDRESS_CLAMP);
        else
            this->pRenderD3D->pDevice->SetTextureStageState(0, D3DTSS_ADDRESS,
                                                            D3DTADDRESS_WRAP);

        if (transparent || config->is_using_specular) {
            this->pRenderD3D->pDevice->SetRenderState(
                D3DRENDERSTATE_ALPHABLENDENABLE, TRUE);
            if (transparent) {
                this->pRenderD3D->pDevice->SetRenderState(
                    D3DRENDERSTATE_SRCBLEND, D3DBLEND_SRCALPHA);
                this->pRenderD3D->pDevice->SetRenderState(
                    D3DRENDERSTATE_DESTBLEND, D3DBLEND_INVSRCALPHA);
                // this->pRenderD3D->pDevice->SetRenderState(D3DRENDERSTATE_SRCBLEND,
                // D3DBLEND_ZERO);
                // this->pRenderD3D->pDevice->SetRenderState(D3DRENDERSTATE_DESTBLEND,
                // D3DBLEND_ONE);
            } else {
                this->pRenderD3D->pDevice->SetRenderState(
                    D3DRENDERSTATE_SRCBLEND, D3DBLEND_ONE);
                this->pRenderD3D->pDevice->SetRenderState(
                    D3DRENDERSTATE_DESTBLEND, D3DBLEND_ZERO);
            }
        }

        for (uint i = 0; i < uNumVertices; ++i) {
            d3d_vertex_buffer[i].pos.x = VertexRenderList[i].vWorldViewProjX;
            d3d_vertex_buffer[i].pos.y = VertexRenderList[i].vWorldViewProjY;
            d3d_vertex_buffer[i].pos.z =
                1.0 - 1.0 / ((VertexRenderList[i].vWorldViewPosition.x * 1000) /
                             pIndoorCameraD3D->GetFarClip());
            d3d_vertex_buffer[i].rhw =
                1.0 / (VertexRenderList[i].vWorldViewPosition.x + 0.0000001);
            d3d_vertex_buffer[i].diffuse = ::GetActorTintColor(
                a4->dimming_level, 0, VertexRenderList[i].vWorldViewPosition.x,
                0, 0);
            d3d_vertex_buffer[i].specular = 0;
            if (config->is_using_specular)
                d3d_vertex_buffer[i].specular = sub_47C3D7_get_fog_specular(
                    0, 0, VertexRenderList[i].vWorldViewPosition.x);

            d3d_vertex_buffer[i].texcoord.x = VertexRenderList[i].u;
            d3d_vertex_buffer[i].texcoord.y = VertexRenderList[i].v;
        }

        this->pRenderD3D->pDevice->SetTexture(0, texture->GetDirect3DTexture());
        this->pRenderD3D->pDevice->DrawPrimitive(
            D3DPT_TRIANGLEFAN,
            D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_SPECULAR | D3DFVF_TEX1,
            d3d_vertex_buffer, uNumVertices, D3DDP_DONOTLIGHT);
        if (transparent) {
            ErrD3D(pRenderD3D->pDevice->SetRenderState(
                D3DRENDERSTATE_ALPHABLENDENABLE, FALSE));
            ErrD3D(pRenderD3D->pDevice->SetRenderState(D3DRENDERSTATE_SRCBLEND,
                                                       D3DBLEND_ONE));
            ErrD3D(pRenderD3D->pDevice->SetRenderState(D3DRENDERSTATE_DESTBLEND,
                                                       D3DBLEND_ZERO));
        }
    } else if (lightmap_builder->StationaryLightsCount) {
        for (uint i = 0; i < uNumVertices; ++i) {
            d3d_vertex_buffer[i].pos.x = VertexRenderList[i].vWorldViewProjX;
            d3d_vertex_buffer[i].pos.y = VertexRenderList[i].vWorldViewProjY;
            d3d_vertex_buffer[i].pos.z =
                1.0 - 1.0 / ((VertexRenderList[i].vWorldViewPosition.x * 1000) /
                             pIndoorCameraD3D->GetFarClip());
            d3d_vertex_buffer[i].rhw =
                1.0 / (VertexRenderList[i].vWorldViewPosition.x + 0.0000001);
            d3d_vertex_buffer[i].diffuse = GetActorTintColor(a4->dimming_level, 0, VertexRenderList[i].vWorldViewPosition.x, 0, 0);
            d3d_vertex_buffer[i].specular = 0;
            if (config->is_using_specular)
                d3d_vertex_buffer[i].specular = sub_47C3D7_get_fog_specular(
                    0, 0, VertexRenderList[i].vWorldViewPosition.x);
            d3d_vertex_buffer[i].texcoord.x = VertexRenderList[i].u;
            d3d_vertex_buffer[i].texcoord.y = VertexRenderList[i].v;
        }
        ErrD3D(pRenderD3D->pDevice->SetRenderState(D3DRENDERSTATE_ZWRITEENABLE,
                                                   FALSE));
        ErrD3D(pRenderD3D->pDevice->SetTextureStageState(0, D3DTSS_ADDRESS,
                                                         D3DTADDRESS_WRAP));
        if (config->is_using_specular)
            ErrD3D(pRenderD3D->pDevice->SetRenderState(D3DRENDERSTATE_FOGENABLE, FALSE));

        ErrD3D(pRenderD3D->pDevice->SetTexture(0, 0));
        ErrD3D(pRenderD3D->pDevice->DrawPrimitive(
            D3DPT_TRIANGLEFAN,  //рисуется текстурка с светом
            D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_SPECULAR | D3DFVF_TEX1,
            d3d_vertex_buffer, uNumVertices, D3DDP_DONOTLIGHT));

        ErrD3D(pRenderD3D->pDevice->SetRenderState(D3DRENDERSTATE_CULLMODE, D3DCULL_NONE));

        lightmap_builder->DrawLightmaps(-1 /*, 0*/);

        for (uint i = 0; i < uNumVertices; ++i) {
            d3d_vertex_buffer[i].diffuse = 0xFFFFFFFF; /*-1;*/
        }
        ErrD3D(pRenderD3D->pDevice->SetTexture(0, texture->GetDirect3DTexture()));  //текстурка
        ErrD3D(pRenderD3D->pDevice->SetTextureStageState(0, D3DTSS_ADDRESS, D3DTADDRESS_WRAP));
        if (!config->is_using_specular) {
            ErrD3D(pRenderD3D->pDevice->SetRenderState(D3DRENDERSTATE_ZWRITEENABLE, TRUE));
        }
        ErrD3D(pRenderD3D->pDevice->SetRenderState(D3DRENDERSTATE_ALPHABLENDENABLE, TRUE));
        ErrD3D(pRenderD3D->pDevice->SetRenderState(D3DRENDERSTATE_SRCBLEND,
                                                   D3DBLEND_ZERO));
        ErrD3D(pRenderD3D->pDevice->SetRenderState(D3DRENDERSTATE_DESTBLEND,
                                                   D3DBLEND_SRCCOLOR));
        ErrD3D(pRenderD3D->pDevice->DrawPrimitive(
            D3DPT_TRIANGLEFAN,
            D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_SPECULAR | D3DFVF_TEX1,
            d3d_vertex_buffer, uNumVertices, D3DDP_DONOTLIGHT));

        if (config->is_using_specular) {
            ErrD3D(pRenderD3D->pDevice->SetRenderState(D3DRENDERSTATE_ZWRITEENABLE, TRUE));
            ErrD3D(pRenderD3D->pDevice->SetTexture(0, 0));
            for (uint i = 0; i < uNumVertices; ++i) {
                d3d_vertex_buffer[i].diffuse =
                    render->uFogColor |
                    d3d_vertex_buffer[i].specular & 0xFF000000;
                d3d_vertex_buffer[i].specular = 0;
            }

            ErrD3D(pRenderD3D->pDevice->SetTexture(0, 0));  // problem
           ErrD3D(pRenderD3D->pDevice->SetRenderState(D3DRENDERSTATE_SRCBLEND,
                                                       D3DBLEND_INVSRCALPHA));
            ErrD3D(pRenderD3D->pDevice->SetRenderState(D3DRENDERSTATE_DESTBLEND,
                                                       D3DBLEND_SRCALPHA));
            ErrD3D(pRenderD3D->pDevice->DrawPrimitive(
                D3DPT_TRIANGLEFAN,
                D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_SPECULAR | D3DFVF_TEX1,
                d3d_vertex_buffer, uNumVertices, D3DDP_DONOTLIGHT));
            ErrD3D(pRenderD3D->pDevice->SetRenderState(D3DRENDERSTATE_FOGENABLE,
                                                       TRUE));
            v45 = GetLevelFogColor();
            ErrD3D(pRenderD3D->pDevice->SetRenderState(D3DRENDERSTATE_FOGCOLOR,
                                                       v45 & 0xFFFFFF));
            ErrD3D(pRenderD3D->pDevice->SetRenderState(D3DRENDERSTATE_FOGTABLEMODE, FALSE));
        }
        ErrD3D(pRenderD3D->pDevice->SetRenderState(D3DRENDERSTATE_SRCBLEND, D3DBLEND_ONE));
        ErrD3D(pRenderD3D->pDevice->SetRenderState(D3DRENDERSTATE_DESTBLEND, D3DBLEND_ZERO));
        ErrD3D(pRenderD3D->pDevice->SetRenderState(D3DRENDERSTATE_ALPHABLENDENABLE, FALSE));
        //}
    }

    // if (pIndoorCamera->flags & INDOOR_CAMERA_DRAW_TERRAIN_OUTLINES ||
    // pBLVRenderParams->uFlags & INDOOR_CAMERA_DRAW_TERRAIN_OUTLINES) if
    // (pIndoorCameraD3D->debug_flags & ODM_RENDER_DRAW_TERRAIN_OUTLINES)
    if (engine->config->debug_terrain)
        pIndoorCameraD3D->debug_outline_d3d(d3d_vertex_buffer, uNumVertices,
                                            0x00FFFFFF, 0.0);
}







void Render::DrawIndoorPolygon(unsigned int uNumVertices, BLVFace *pFace,
                               int uPackedID, unsigned int uColor, int a8) {
    if (!uNumD3DSceneBegins || uNumVertices < 3) {
        return;
    }

    unsigned int sCorrectedColor = uColor;

    TextureD3D *texture = (TextureD3D *)pFace->GetTexture();

    if (lightmap_builder->StationaryLightsCount) {
        sCorrectedColor =  0xFFFFFFFF/*-1*/;
    }


    // perception
    // engine->AlterGamma_BLV(pFace, &sCorrectedColor);

    if (engine->CanSaturateFaces() && (pFace->uAttributes & FACE_CAN_SATURATE_COLOR)) {
        uint eightSeconds = OS_GetTime() % 3000;
        float angle = (eightSeconds / 3000.0f) * 2 * 3.1415f;

        int redstart = (sCorrectedColor & 0x00FF0000) >> 16;

        int col = (redstart - 64) - (64 * cosf(angle));
        // (a << 24) | (r << 16) | (g << 8) | b;
        sCorrectedColor = (0xFF << 24) | (redstart << 16) | (col << 8) | col;
    }

    if (pFace->uAttributes & FACE_OUTLINED) {
        if (OS_GetTime() % 300 >= 150)
            uColor = sCorrectedColor = 0xFF20FF20;
        else
            uColor = sCorrectedColor = 0xFF109010;
    }

    if (_4D864C_force_sw_render_rules && engine->config->Flag1_1()) {
        __debugbreak();
        ErrD3D(pRenderD3D->pDevice->SetRenderState(D3DRENDERSTATE_ZWRITEENABLE, false));
        ErrD3D(pRenderD3D->pDevice->SetTextureStageState(0, D3DTSS_ADDRESS, D3DTADDRESS_WRAP));
        for (uint i = 0; i < uNumVertices; ++i) {
            d3d_vertex_buffer[i].pos.x = array_507D30[i].vWorldViewProjX;
            d3d_vertex_buffer[i].pos.y = array_507D30[i].vWorldViewProjY;
            d3d_vertex_buffer[i].pos.z =
                1.0 -
                1.0 / (array_507D30[i].vWorldViewPosition.x * 0.061758894);
            d3d_vertex_buffer[i].rhw =
                1.0 / array_507D30[i].vWorldViewPosition.x;
            d3d_vertex_buffer[i].diffuse = sCorrectedColor;
            d3d_vertex_buffer[i].specular = 0;
            d3d_vertex_buffer[i].texcoord.x =
                array_507D30[i].u / (double)pFace->GetTexture()->GetWidth();
            d3d_vertex_buffer[i].texcoord.y =
                array_507D30[i].v / (double)pFace->GetTexture()->GetHeight();
        }

        ErrD3D(pRenderD3D->pDevice->SetTextureStageState(0, D3DTSS_ADDRESS,
                                                         D3DTADDRESS_WRAP));
        ErrD3D(pRenderD3D->pDevice->SetTexture(0, nullptr));
        ErrD3D(pRenderD3D->pDevice->DrawPrimitive(
            D3DPT_TRIANGLEFAN,
            D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_SPECULAR | D3DFVF_TEX1,
            d3d_vertex_buffer, uNumVertices, D3DDP_DONOTLIGHT));
        lightmap_builder->DrawLightmaps(-1 /*, 0*/);
    } else {
        if (!lightmap_builder->StationaryLightsCount ||
            _4D864C_force_sw_render_rules && engine->config->Flag1_2()) {
           // return;

            for (uint i = 0; i < uNumVertices; ++i) {
                d3d_vertex_buffer[i].pos.x = array_507D30[i].vWorldViewProjX;
                d3d_vertex_buffer[i].pos.y = array_507D30[i].vWorldViewProjY;
                d3d_vertex_buffer[i].pos.z =
                    1.0 -
                    1.0 / (array_507D30[i].vWorldViewPosition.x * 0.061758894);
                d3d_vertex_buffer[i].rhw =
                    1.0 / array_507D30[i].vWorldViewPosition.x;
                d3d_vertex_buffer[i].diffuse = sCorrectedColor;
                d3d_vertex_buffer[i].specular = 0;
                d3d_vertex_buffer[i].texcoord.x =
                    array_507D30[i].u / (double)pFace->GetTexture()->GetWidth();
                d3d_vertex_buffer[i].texcoord.y =
                    array_507D30[i].v /
                    (double)pFace->GetTexture()->GetHeight();
            }

            ErrD3D(pRenderD3D->pDevice->SetTextureStageState(0, D3DTSS_ADDRESS,
                                                             D3DTADDRESS_WRAP));
            ErrD3D(pRenderD3D->pDevice->SetTexture(
                0, texture->GetDirect3DTexture()));
            ErrD3D(pRenderD3D->pDevice->DrawPrimitive(
                D3DPT_TRIANGLEFAN,
                D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_SPECULAR | D3DFVF_TEX1,
                d3d_vertex_buffer, uNumVertices, D3DDP_DONOTLIGHT));
        } else {
            for (uint i = 0; i < uNumVertices; ++i) {
                d3d_vertex_buffer[i].pos.x = array_507D30[i].vWorldViewProjX;
                d3d_vertex_buffer[i].pos.y = array_507D30[i].vWorldViewProjY;
                d3d_vertex_buffer[i].pos.z =
                    1.0 -
                    1.0 / (array_507D30[i].vWorldViewPosition.x * 0.061758894);
                d3d_vertex_buffer[i].rhw = 1.0 / array_507D30[i].vWorldViewPosition.x;
                d3d_vertex_buffer[i].diffuse = uColor;
                d3d_vertex_buffer[i].specular = 0;
                d3d_vertex_buffer[i].texcoord.x =
                    array_507D30[i].u / (double)pFace->GetTexture()->GetWidth();
                d3d_vertex_buffer[i].texcoord.y =
                    array_507D30[i].v /
                    (double)pFace->GetTexture()->GetHeight();
            }

            ErrD3D(pRenderD3D->pDevice->SetRenderState(D3DRENDERSTATE_ZWRITEENABLE, FALSE));
            ErrD3D(pRenderD3D->pDevice->SetTextureStageState(0, D3DTSS_ADDRESS, D3DTADDRESS_WRAP));
            ErrD3D(pRenderD3D->pDevice->SetTexture(0, nullptr));

            ErrD3D(pRenderD3D->pDevice->DrawPrimitive(
                D3DPT_TRIANGLEFAN,
                D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_SPECULAR | D3DFVF_TEX1,
                d3d_vertex_buffer, uNumVertices, D3DDP_DONOTLIGHT));

            ErrD3D(pRenderD3D->pDevice->SetRenderState(D3DRENDERSTATE_CULLMODE, D3DCULL_NONE));

            lightmap_builder->DrawLightmaps(-1 /*, 0*/);

            for (uint i = 0; i < uNumVertices; ++i) {
                d3d_vertex_buffer[i].diffuse = sCorrectedColor;
            }

            ErrD3D(pRenderD3D->pDevice->SetTexture(0, texture->GetDirect3DTexture()));
            ErrD3D(pRenderD3D->pDevice->SetTextureStageState(0, D3DTSS_ADDRESS, D3DTADDRESS_WRAP));

            ErrD3D(pRenderD3D->pDevice->SetRenderState(D3DRENDERSTATE_ZWRITEENABLE, TRUE));
            ErrD3D(pRenderD3D->pDevice->SetRenderState(D3DRENDERSTATE_ALPHABLENDENABLE, TRUE));
            ErrD3D(pRenderD3D->pDevice->SetRenderState(D3DRENDERSTATE_SRCBLEND, D3DBLEND_ZERO));
            ErrD3D(pRenderD3D->pDevice->SetRenderState(D3DRENDERSTATE_DESTBLEND, D3DBLEND_SRCCOLOR));

            ErrD3D(pRenderD3D->pDevice->DrawPrimitive(
                D3DPT_TRIANGLEFAN,
                D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_SPECULAR | D3DFVF_TEX1,
                d3d_vertex_buffer, uNumVertices, D3DDP_DONOTLIGHT));

            ErrD3D(pRenderD3D->pDevice->SetRenderState(D3DRENDERSTATE_SRCBLEND, D3DBLEND_ONE));
            ErrD3D(pRenderD3D->pDevice->SetRenderState(D3DRENDERSTATE_DESTBLEND, D3DBLEND_ZERO));
            ErrD3D(pRenderD3D->pDevice->SetRenderState(D3DRENDERSTATE_ALPHABLENDENABLE, FALSE));
        }
    }
}

void Render::DrawBillboard_Indoor(SoftwareBillboard *pSoftBillboard,
                                  RenderBillboard *billboard) {
    int v11;     // eax@9
    int v12;     // eax@9
    double v15;  // st5@12
    double v16;  // st4@12
    double v17;  // st3@12
    double v18;  // st2@12
    int v19;     // ecx@14
    double v20;  // st3@14
    int v21;     // ecx@16
    double v22;  // st3@16
    float v27;   // [sp+24h] [bp-Ch]@5
    float v29;   // [sp+2Ch] [bp-4h]@5
    float v31;   // [sp+3Ch] [bp+Ch]@5
    float a1;    // [sp+40h] [bp+10h]@5

    if (this->uNumD3DSceneBegins == 0) {
        return;
    }

    Sprite *pSprite = billboard->hwsprite;
    int dimming_level = billboard->dimming_level;

    // v4 = pSoftBillboard;
    // v5 = (double)pSoftBillboard->zbuffer_depth;
    // pSoftBillboarda = pSoftBillboard->zbuffer_depth;
    // v6 = pSoftBillboard->zbuffer_depth;
    unsigned int v7 = Billboard_ProbablyAddToListAndSortByZOrder(
        pSoftBillboard->screen_space_z);
    // v8 = dimming_level;
    // device_caps = v7;
    int v28 = dimming_level & 0xFF000000;
    if (dimming_level & 0xFF000000) {
        pBillboardRenderListD3D[v7].opacity = RenderBillboardD3D::Opaque_3;
    } else {
        pBillboardRenderListD3D[v7].opacity = RenderBillboardD3D::Transparent;
    }
    // v10 = a3;
    pBillboardRenderListD3D[v7].field_90 = pSoftBillboard->field_44;
    pBillboardRenderListD3D[v7].screen_space_z = pSoftBillboard->screen_space_z;
    pBillboardRenderListD3D[v7].object_pid = pSoftBillboard->object_pid;
    pBillboardRenderListD3D[v7].sParentBillboardID =
        pSoftBillboard->sParentBillboardID;
    // v25 = pSoftBillboard->uScreenSpaceX;
    // v24 = pSoftBillboard->uScreenSpaceY;
    a1 = pSoftBillboard->screenspace_projection_factor_x;
    v29 = pSoftBillboard->screenspace_projection_factor_y;
    v31 = (double)((pSprite->uBufferWidth >> 1) - pSprite->uAreaX);
    v27 = (double)(pSprite->uBufferHeight - pSprite->uAreaY);
    if (pSoftBillboard->uFlags & 4) {
        v31 = v31 * -1.0;
    }
    if (config->is_tinting && pSoftBillboard->sTintColor) {
        v11 = ::GetActorTintColor(dimming_level, 0,
                                  pSoftBillboard->screen_space_z, 0, 0);
        v12 = BlendColors(pSoftBillboard->sTintColor, v11);
        if (v28)
            v12 =
                (unsigned int)((char *)&array_77EC08[1852].pEdgeList1[17] + 3) &
                ((unsigned int)v12 >> 1);
    } else {
        v12 = ::GetActorTintColor(dimming_level, 0,
                                  pSoftBillboard->screen_space_z, 0, 0);
    }
    // v13 = (double)v25;
    pBillboardRenderListD3D[v7].pQuads[0].specular = 0;
    pBillboardRenderListD3D[v7].pQuads[0].diffuse = v12;
    pBillboardRenderListD3D[v7].pQuads[0].pos.x =
        pSoftBillboard->screen_space_x - v31 * a1;
    // v14 = (double)v24;
    // v32 = v14;
    pBillboardRenderListD3D[v7].pQuads[0].pos.y =
        pSoftBillboard->screen_space_y - v27 * v29;
    v15 = 1.0 - 1.0 / (pSoftBillboard->screen_space_z * 0.061758894);
    pBillboardRenderListD3D[v7].pQuads[0].pos.z = v15;
    v16 = 1.0 / pSoftBillboard->screen_space_z;
    pBillboardRenderListD3D[v7].pQuads[0].rhw =
        1.0 / pSoftBillboard->screen_space_z;
    pBillboardRenderListD3D[v7].pQuads[0].texcoord.x = 0.0;
    pBillboardRenderListD3D[v7].pQuads[0].texcoord.y = 0.0;
    v17 = (double)((pSprite->uBufferWidth >> 1) - pSprite->uAreaX);
    v18 = (double)(pSprite->uBufferHeight - pSprite->uAreaY -
                   pSprite->uAreaHeight);
    if (pSoftBillboard->uFlags & 4) {
        v17 = v17 * -1.0;
    }
    pBillboardRenderListD3D[v7].pQuads[1].specular = 0;
    pBillboardRenderListD3D[v7].pQuads[1].diffuse = v12;
    pBillboardRenderListD3D[v7].pQuads[1].pos.x =
        pSoftBillboard->screen_space_x - v17 * a1;
    pBillboardRenderListD3D[v7].pQuads[1].pos.y =
        pSoftBillboard->screen_space_y - v18 * v29;
    pBillboardRenderListD3D[v7].pQuads[1].pos.z = v15;
    pBillboardRenderListD3D[v7].pQuads[1].rhw = v16;
    pBillboardRenderListD3D[v7].pQuads[1].texcoord.x = 0.0;
    pBillboardRenderListD3D[v7].pQuads[1].texcoord.y = 1.0;
    v19 = pSprite->uBufferHeight - pSprite->uAreaY - pSprite->uAreaHeight;
    v20 = (double)(pSprite->uAreaX + pSprite->uAreaWidth +
                   (pSprite->uBufferWidth >> 1) - pSprite->uBufferWidth);
    if (pSoftBillboard->uFlags & 4) {
        v20 = v20 * -1.0;
    }
    pBillboardRenderListD3D[v7].pQuads[2].specular = 0;
    pBillboardRenderListD3D[v7].pQuads[2].diffuse = v12;
    pBillboardRenderListD3D[v7].pQuads[2].pos.x =
        v20 * a1 + pSoftBillboard->screen_space_x;
    pBillboardRenderListD3D[v7].pQuads[2].pos.y =
        pSoftBillboard->screen_space_y - (double)v19 * v29;
    pBillboardRenderListD3D[v7].pQuads[2].pos.z = v15;
    pBillboardRenderListD3D[v7].pQuads[2].rhw = v16;
    pBillboardRenderListD3D[v7].pQuads[2].texcoord.x = 1.0;
    pBillboardRenderListD3D[v7].pQuads[2].texcoord.y = 1.0;
    v21 = pSprite->uBufferHeight - pSprite->uAreaY;
    v22 = (double)(pSprite->uAreaX + pSprite->uAreaWidth +
                   (pSprite->uBufferWidth >> 1) - pSprite->uBufferWidth);
    if (pSoftBillboard->uFlags & 4) {
        v22 = v22 * -1.0;
    }
    pBillboardRenderListD3D[v7].pQuads[3].specular = 0;
    pBillboardRenderListD3D[v7].pQuads[3].diffuse = v12;
    pBillboardRenderListD3D[v7].pQuads[3].pos.x =
        v22 * a1 + pSoftBillboard->screen_space_x;
    pBillboardRenderListD3D[v7].pQuads[3].pos.y =
        pSoftBillboard->screen_space_y - (double)v21 * v29;
    pBillboardRenderListD3D[v7].pQuads[3].pos.z = v15;
    pBillboardRenderListD3D[v7].pQuads[3].rhw = v16;
    pBillboardRenderListD3D[v7].pQuads[3].texcoord.x = 1.0;
    pBillboardRenderListD3D[v7].pQuads[3].texcoord.y = 0.0;
    // v23 = pSprite->pTexture;
    pBillboardRenderListD3D[v7].uNumVertices = 4;
    pBillboardRenderListD3D[v7].z_order = pSoftBillboard->screen_space_z;
    pBillboardRenderListD3D[v7].texture = pSprite->texture;

    if (pSprite->texture->GetHeight() == 0 || pSprite->texture->GetWidth() == 0) __debugbreak();
}

void Render::DrawProjectile(float srcX, float srcY, float a3, float a4,
                            float dstX, float dstY, float a7, float a8,
                            Texture *texture) {
    double v20;  // st4@8
    double v21;  // st4@10
    double v22;  // st4@10
    double v23;  // st4@10
    double v25;  // st4@11
    double v26;  // st4@13
    double v28;  // st4@13

    TextureD3D *textured3d = (TextureD3D *)texture;

    int xDifference = bankersRounding(dstX - srcX);
    int yDifference = bankersRounding(dstY - srcY);
    int absYDifference = abs(yDifference);
    int absXDifference = abs(xDifference);
    unsigned int smallerabsdiff = min(absXDifference, absYDifference);
    unsigned int largerabsdiff = max(absXDifference, absYDifference);
    int v32 = (11 * smallerabsdiff >> 5) + largerabsdiff;
    double v16 = 1.0 / (double)v32;
    double v17 = (double)yDifference * v16 * a4;
    double v18 = (double)xDifference * v16 * a4;
    if (uCurrentlyLoadedLevelType == LEVEL_Outdoor) {
        v20 = a3 * 1000.0 / pIndoorCameraD3D->GetFarClip();
        v25 = a7 * 1000.0 / pIndoorCameraD3D->GetFarClip();
    } else {
        v20 = a3 * 0.061758894;
        v25 = a7 * 0.061758894;
    }
    v21 = 1.0 / a3;
    v22 = (double)yDifference * v16 * a8;
    v23 = (double)xDifference * v16 * a8;
    v26 = 1.0 - 1.0 / v25;
    v28 = 1.0 / a7;

    RenderVertexD3D3 v29[4];
    v29[0].pos.x = srcX + v17;
    v29[0].pos.y = srcY - v18;
    v29[0].pos.z = 1.0 - 1.0 / v20;
    v29[0].rhw = v21;
    v29[0].diffuse = -1;
    v29[0].specular = 0;
    v29[0].texcoord.x = 1.0;
    v29[0].texcoord.y = 0.0;

    v29[1].pos.x = v22 + dstX;
    v29[1].pos.y = dstY - v23;
    v29[1].pos.z = v26;
    v29[1].rhw = v28;
    v29[1].diffuse = -16711936;
    v29[1].specular = 0;
    v29[1].texcoord.x = 1.0;
    v29[1].texcoord.y = 1.0;

    v29[2].pos.x = dstX - v22;
    v29[2].pos.y = v23 + dstY;
    v29[2].pos.z = v26;
    v29[2].rhw = v28;
    v29[2].diffuse = -1;
    v29[2].specular = 0;
    v29[2].texcoord.x = 0.0;
    v29[2].texcoord.y = 1.0;

    v29[3].pos.x = srcX - v17;
    v29[3].pos.y = v18 + srcY;
    v29[3].pos.z = v29[0].pos.z;
    v29[3].rhw = v21;
    v29[3].diffuse = -1;
    v29[3].specular = 0;
    v29[3].texcoord.x = 0.0;
    v29[3].texcoord.y = 0.0;

    ErrD3D(pRenderD3D->pDevice->SetRenderState(D3DRENDERSTATE_ALPHABLENDENABLE,
                                               TRUE));
    ErrD3D(pRenderD3D->pDevice->SetRenderState(D3DRENDERSTATE_SRCBLEND,
                                               D3DBLEND_ONE));
    ErrD3D(pRenderD3D->pDevice->SetRenderState(D3DRENDERSTATE_DESTBLEND,
                                               D3DBLEND_ONE));
    ErrD3D(pRenderD3D->pDevice->SetRenderState(D3DRENDERSTATE_DITHERENABLE,
                                               FALSE));
    ErrD3D(pRenderD3D->pDevice->SetRenderState(D3DRENDERSTATE_ZWRITEENABLE,
                                               FALSE));
    ErrD3D(pRenderD3D->pDevice->SetRenderState(D3DRENDERSTATE_CULLMODE,
                                               D3DCULL_NONE));
    ErrD3D(
        pRenderD3D->pDevice->SetTexture(0, textured3d->GetDirect3DTexture()));
    ErrD3D(pRenderD3D->pDevice->DrawPrimitive(
        D3DPT_TRIANGLEFAN,
        D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_SPECULAR | D3DFVF_TEX1, v29, 4,
        24));
    ErrD3D(pRenderD3D->pDevice->SetRenderState(D3DRENDERSTATE_ALPHABLENDENABLE,
                                               FALSE));
    ErrD3D(pRenderD3D->pDevice->SetRenderState(D3DRENDERSTATE_SRCBLEND,
                                               D3DBLEND_ONE));
    ErrD3D(pRenderD3D->pDevice->SetRenderState(D3DRENDERSTATE_DESTBLEND,
                                               D3DBLEND_ZERO));
    ErrD3D(
        pRenderD3D->pDevice->SetRenderState(D3DRENDERSTATE_DITHERENABLE, TRUE));
    ErrD3D(
        pRenderD3D->pDevice->SetRenderState(D3DRENDERSTATE_ZWRITEENABLE, TRUE));
    ErrD3D(pRenderD3D->pDevice->SetRenderState(D3DRENDERSTATE_CULLMODE,
                                               D3DCULL_CW));
}

void Render::_4A4CC9_AddSomeBillboard(SpellFX_Billboard *a1,
                                      int diffuse) {
    if (a1->uNumVertices < 3) {
        return;
    }

    float depth = 1000000.0;
    for (uint i = 0; i < (unsigned int)a1->uNumVertices; ++i) {
        if (a1->field_104[i].z < depth) {
            depth = a1->field_104[i].z;
        }
    }

    unsigned int v5 = Billboard_ProbablyAddToListAndSortByZOrder(depth);
    pBillboardRenderListD3D[v5].field_90 = 0;
    pBillboardRenderListD3D[v5].sParentBillboardID = -1;
    pBillboardRenderListD3D[v5].opacity = RenderBillboardD3D::Opaque_2;
    pBillboardRenderListD3D[v5].texture = 0;
    pBillboardRenderListD3D[v5].uNumVertices = a1->uNumVertices;
    pBillboardRenderListD3D[v5].z_order = depth;

    for (unsigned int i = 0; i < (unsigned int)a1->uNumVertices; ++i) {
        pBillboardRenderListD3D[v5].pQuads[i].pos.x = a1->field_104[i].x;
        pBillboardRenderListD3D[v5].pQuads[i].pos.y = a1->field_104[i].y;



        // if (a1->field_104[i].z < 17) a1->field_104[i].z = 17;
        float rhw = 1.f / a1->field_104[i].z;
        float z = 1.f - 1.f / (a1->field_104[i].z * 1000.f / pIndoorCameraD3D->GetFarClip());



        double v10 = a1->field_104[i].z;
        if (uCurrentlyLoadedLevelType == LEVEL_Indoor) {
            v10 *= 1000.f / 16192.f;
        } else {
            v10 *= 1000.f / pIndoorCameraD3D->GetFarClip();
        }


        pBillboardRenderListD3D[v5].pQuads[i].pos.z = z;  // 1.0 - 1.0 / v10;
        pBillboardRenderListD3D[v5].pQuads[i].rhw = rhw;  // 1.0 / a1->field_104[i].z;

        int v12;
        if (diffuse & 0xFF000000) {
            v12 = a1->field_104[i].diffuse;
        } else {
            v12 = diffuse;
        }
        pBillboardRenderListD3D[v5].pQuads[i].diffuse = v12;
        pBillboardRenderListD3D[v5].pQuads[i].specular = 0;

        pBillboardRenderListD3D[v5].pQuads[i].texcoord.x = 0.0;
        pBillboardRenderListD3D[v5].pQuads[i].texcoord.y = 0.0;
    }
}

HWLTexture *Render::LoadHwlBitmap(const char *name) {
    return pD3DBitmaps.LoadTexture(name);
}

HWLTexture *Render::LoadHwlSprite(const char *name) {
    return pD3DSprites.LoadTexture(name);
}

void Render::Update_Texture(Texture *texture) {
    // nothing
}
void Render::DeleteTexture(Texture *texture) {
    // nothing
}

void Render::RemoveTextureFromDevice(Texture* texture) {
    auto t = (TextureD3D*)texture;
    if (t->initialized) {
        IDirectDrawSurface* dds_get = t->GetDirectDrawSurface();
        IDirect3DTexture2* d3dt_get = t->GetDirect3DTexture();
        if (dds_get) dds_get->Release();
        if (d3dt_get) d3dt_get->Release();
    }
}

bool Render::MoveTextureToDevice(Texture *texture) {
    auto t = (TextureD3D *)texture;
    if (t) {
        auto pixels = (uint16_t *)t->GetPixels(IMAGE_FORMAT_A1R5G5B5);

        IDirectDrawSurface4 *dds;
        IDirect3DTexture2 *d3dt;

        if (!pRenderD3D->CreateTexture(t->GetWidth(), t->GetHeight(), &dds,
                                       &d3dt, true, false,
                                       uMinDeviceTextureDim))
            Error(
                "HiScreen16::LoadTexture - D3Drend->CreateTexture() failed: %x",
                0);

        DDSCAPS2 v19;
        memset(&v19, 0, sizeof(DDSCAPS2));
        v19.dwCaps = DDSCAPS_TEXTURE | DDSCAPS_MIPMAP;

        DDSURFACEDESC2 desc;
        memset(&desc, 0, sizeof(DDSURFACEDESC2));
        desc.dwSize = sizeof(DDSURFACEDESC2);

        if (LockSurface_DDraw4(dds, &desc, DDLOCK_WAIT | DDLOCK_WRITEONLY)) {
            auto v13 = pixels;
            auto v14 = (unsigned __int16 *)desc.lpSurface;
            for (uint bMipMaps = 0; bMipMaps < desc.dwHeight; bMipMaps++) {
                for (auto v15 = 0; v15 < desc.dwWidth; v15++) {
                    *v14 = *v13;
                    ++v14;
                    ++v13;
                }
                v14 += (desc.lPitch >> 1) - desc.dwWidth;
            }
            ErrD3D(dds->Unlock(NULL));
        }

        t->SetDirect3DTexture2(d3dt);
        t->SetDirectDrawSurface((IDirectDrawSurface *)dds);

        return true;
    }
    return false;
}

void Render::BeginScene() {}

void Render::EndScene() {}

void Render::ScreenFade(unsigned int color, float t) {
    unsigned int v3 = 0;

    //{
    if (t > 1.0f)
        t = 1.0f;
    else if (t < 0.0f)
        t = 0.0f;

    int v40 = (char)floorf(t * 255.0f + 0.5f);

    unsigned int v7 = color | (v40 << 24);

    RenderVertexD3D3 v36[4];
    v36[0].specular = 0;
    v36[0].pos.x = pViewport->uViewportTL_X;
    v36[0].pos.y = (double)pViewport->uViewportTL_Y;
    v36[0].pos.z = 0.0;
    v36[0].diffuse = v7;
    v36[0].rhw = 1.0;
    v36[0].texcoord.x = 0.0;
    v36[0].texcoord.y = 0.0;

    v36[1].specular = 0;
    v36[1].pos.x = pViewport->uViewportTL_X;
    v36[1].pos.y = (double)(pViewport->uViewportBR_Y + 1);
    v36[1].pos.z = 0.0;
    v36[1].diffuse = v7;
    v36[1].rhw = 1.0;
    v36[1].texcoord.x = 0.0;
    v36[1].texcoord.y = 0.0;

    v36[2].specular = 0;
    v36[2].pos.x = (double)pViewport->uViewportBR_X;
    v36[2].pos.y = (double)(pViewport->uViewportBR_Y + 1);
    v36[2].pos.z = 0.0;
    v36[2].diffuse = v7;
    v36[2].rhw = 1.0;
    v36[2].texcoord.x = 0.0;
    v36[2].texcoord.y = 0.0;

    v36[3].specular = 0;
    v36[3].pos.x = (double)pViewport->uViewportBR_X;
    v36[3].pos.y = (double)pViewport->uViewportTL_Y;
    v36[3].pos.z = 0.0;
    v36[3].diffuse = v7;
    v36[3].rhw = 1.0;
    v36[3].texcoord.x = 0.0;
    v36[3].texcoord.y = 0.0;

    ErrD3D(pRenderD3D->pDevice->SetTexture(0, 0));
    ErrD3D(
        pRenderD3D->pDevice->SetRenderState(D3DRENDERSTATE_FOGENABLE, FALSE));
    ErrD3D(pRenderD3D->pDevice->SetRenderState(D3DRENDERSTATE_ALPHABLENDENABLE,
                                               TRUE));
    ErrD3D(pRenderD3D->pDevice->SetRenderState(D3DRENDERSTATE_ZWRITEENABLE,
                                               FALSE));
    ErrD3D(pRenderD3D->pDevice->SetRenderState(D3DRENDERSTATE_SRCBLEND,
                                               D3DBLEND_SRCALPHA));
    ErrD3D(pRenderD3D->pDevice->SetRenderState(D3DRENDERSTATE_DESTBLEND,
                                               D3DBLEND_INVSRCALPHA));
    ErrD3D(pRenderD3D->pDevice->SetRenderState(D3DRENDERSTATE_DITHERENABLE,
                                               FALSE));
    ErrD3D(pRenderD3D->pDevice->SetRenderState(D3DRENDERSTATE_ZFUNC,
                                               D3DCMP_ALWAYS));
    ErrD3D(pRenderD3D->pDevice->DrawPrimitive(
        D3DPT_TRIANGLEFAN,
        D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_SPECULAR | D3DFVF_TEX1, v36, 4,
        28));
    ErrD3D(pRenderD3D->pDevice->SetRenderState(D3DRENDERSTATE_SRCBLEND,
                                               D3DBLEND_ONE));
    ErrD3D(pRenderD3D->pDevice->SetRenderState(D3DRENDERSTATE_DESTBLEND,
                                               D3DBLEND_ZERO));
    ErrD3D(pRenderD3D->pDevice->SetRenderState(D3DRENDERSTATE_ALPHABLENDENABLE,
                                               FALSE));
    ErrD3D(
        pRenderD3D->pDevice->SetRenderState(D3DRENDERSTATE_ZWRITEENABLE, TRUE));
    ErrD3D(
        pRenderD3D->pDevice->SetRenderState(D3DRENDERSTATE_DITHERENABLE, TRUE));
    ErrD3D(
        pRenderD3D->pDevice->SetRenderState(D3DRENDERSTATE_ZFUNC, D3DCMP_LESS));
    /*}
    else
    {
    v40 = (1.0 - a3) * 65536.0;
    v39 = v40 + 6.7553994e15;
    LODWORD(a3) = LODWORD(v39);
    v38 = (signed int)(pViewport->uViewportBR_X - pViewport->uViewportTL_X) >>
    1; HIDWORD(v39) = pViewport->uViewportBR_Y - pViewport->uViewportTL_Y + 1;
    v13 = pViewport->uViewportTL_X + ecx0->uTargetSurfacePitch -
    pViewport->uViewportBR_X; v14 =
    &ecx0->pTargetSurface[pViewport->uViewportTL_X + pViewport->uViewportTL_Y *
    ecx0->uTargetSurfacePitch]; v37 = 2 * v13; LODWORD(v40) = (int)v14;

    int __i = 0;
    v15 = dword_F1B430.data();
    do
    {
    v16 = v3;
    v3 += LODWORD(a3);
    dword_F1B430[__i++] = v16 >> 16;
    }
    //while ( (signed int)v15 < (signed int)&Aureal3D_SplashScreen );
    while (__i < 32);

    if ( render->uTargetGBits == 6 )
    {
    v17 = sr_42690D_colors_cvt(this_);
    v18 = (65536 - LODWORD(a3)) * (v17 & 0x1F);
    this_ = (((65536 - LODWORD(a3)) * (unsigned __int16)(v17 & 0xF800) &
    0xF800FFFF | v18 & 0x1F0000 | (65536 - LODWORD(a3)) * (v17 & 0x7E0) &
    0x7E00000u) >> 16 << 16) | (((65536 - LODWORD(a3)) * (unsigned __int16)(v17
    & 0xF800) & 0xF800FFFF | v18 & 0x1F0000 | (65536 - LODWORD(a3)) * (v17 &
    0x7E0) & 0x7E00000u) >> 16); v19 = v40; v20 = off_4EFDB0; v21 =
    HIDWORD(v39); do
    {
    v22 = v38;
    v31 = v21;
    do
    {
    v23 = (*(int *)((char *)v20
    + ((((unsigned __int16)(*(short *)((char *)v20 + ((*(unsigned int
    *)LODWORD(v19) & 0xF800u) >> 9)) << 11) | *(unsigned int *)LODWORD(v19) &
    0x7FF) & 0x7C0u) >> 4)) << 6) | (*(int *)((char *)v20 + ((((*(int *)((char
    *)v20 + ((*(unsigned int *)LODWORD(v19) & 0xF800u) >> 9)) << 11) | (*(int
    *)((char *)v20 + ((*(unsigned int *)LODWORD(v19) & 0xF8000000u) >> 25)) <<
    27) | *(unsigned int *)LODWORD(v19) & 0x7FF07FF) & 0x7C00000u) >> 20)) <<
    22) | ((*(int *)((char *)v20 + ((*(unsigned int *)LODWORD(v19) & 0xF800u) >>
    9)) << 11) | (*(int *)((char *)v20 + ((*(unsigned int *)LODWORD(v19) &
    0xF8000000u) >> 25)) << 27) | *(unsigned int *)LODWORD(v19) & 0x7FF07FF) &
    0xF81FF81F; result = this_
    + (*((int *)v20
    + (((unsigned __int8)(*((char *)v20
    + ((((unsigned __int16)(*(short *)((char *)v20
    + ((*(unsigned int *)LODWORD(v19) & 0xF800u) >> 9)) << 11) | *(unsigned int
    *)LODWORD(v19) & 0x7FF) & 0x7C0u) >> 4)) << 6) | *(unsigned int
    *)LODWORD(v19) & 0x1F) & 0x1F)) | (*(int *)((char *)v20 + ((v23 & 0x1F0000u)
    >> 14)) << 16) | ((*(int *)((char *)v20 + ((((unsigned __int16)(*(short
    *)((char *)v20 + ((*(unsigned int *)LODWORD(v19) & 0xF800u) >> 9)) << 11) |
    *(unsigned int *)LODWORD(v19) & 0x7FF) & 0x7C0u) >> 4)) << 6) | (*(int
    *)((char *)v20 + ((((*(int *)((char *)v20 + ((*(unsigned int *)LODWORD(v19)
    & 0xF800u) >> 9)) << 11) | (*(int *)((char *)v20 + ((*(unsigned int
    *)LODWORD(v19) & 0xF8000000u) >> 25)) << 27) | *(unsigned int *)LODWORD(v19)
    & 0x7FF07FF) & 0x7C00000u) >> 20)) << 22) | ((*(int *)((char *)v20 +
    ((*(unsigned int *)LODWORD(v19) & 0xF800u) >> 9)) << 11) | (*(int *)((char
    *)v20 + ((*(unsigned int *)LODWORD(v19) & 0xF8000000u) >> 25)) << 27) |
    *(unsigned int *)LODWORD(v19) & 0x7FF07FF) & 0xF81FF81F) & 0xFFE0FFE0);
    *(unsigned int *)LODWORD(v19) = result;
    LODWORD(v19) += 4;
    --v22;
    }
    while ( v22 );
    LODWORD(v19) += v37;
    v21 = v31 - 1;
    }
    while ( v31 != 1 );
    }
    else
    {
    v24 = sr_4268E3_smthn_to_a1r5g5b5(this_);
    v25 = (65536 - LODWORD(a3)) * (v24 & 0x1F);
    this_ = (((65536 - LODWORD(a3)) * (v24 & 0x7C00) & 0x7C000000 | v25 &
    0x1F0000 | (65536 - LODWORD(a3))
    * (v24 & 0x3E0) & 0x3E00000u) >> 16 << 16) | (((65536 - LODWORD(a3)) * (v24
    & 0x7C00) & 0x7C000000 | v25 & 0x1F0000 | (65536 - LODWORD(a3)) * (v24 &
    0x3E0) & 0x3E00000u) >> 16); v26 = v40; v27 = (char *)off_4EFDB0; v28 =
    HIDWORD(v39); do
    {
    v29 = v38;
    v32 = v28;
    do
    {
    v30 = 32
    * *(int *)&v27[(((unsigned __int16)(*(short *)&v27[(*(unsigned int
    *)LODWORD(v26) & 0x7C00u) >> 8] << 10) | *(unsigned int *)LODWORD(v26) &
    0x3FF) & 0x3E0u) >> 3] | (*(int *)&v27[(((*(int *)&v27[(*(unsigned int
    *)LODWORD(v26) & 0x7C00u) >> 8] << 10) | (*(int *)&v27[(*(unsigned int
    *)LODWORD(v26) & 0x7C000000u) >> 24] << 26) | *(unsigned int *)LODWORD(v26)
    & 0x3FF03FF) & 0x3E00000u) >> 19] << 21) | ((*(int *)&v27[(*(unsigned int
    *)LODWORD(v26) & 0x7C00u) >> 8] << 10) | (*(int *)&v27[(*(unsigned int
    *)LODWORD(v26) & 0x7C000000u) >> 24] << 26) | *(unsigned int *)LODWORD(v26)
    & 0x3FF03FF) & 0x7C1F7C1F; result = this_
    + (*(int *)&v27[4
    * (((unsigned __int8)(32
    * v27[(((unsigned __int16)(*(short *)&v27[(*(unsigned int *)LODWORD(v26) &
    0x7C00u) >> 8] << 10) | *(unsigned int *)LODWORD(v26) & 0x3FF) & 0x3E0u) >>
    3]) | *(unsigned int *)LODWORD(v26) & 0x1F) & 0x1F)] | (*(int *)&v27[(v30 &
    0x1F0000u) >> 14] << 16) | (32 * *(int *)&v27[(((unsigned __int16)(*(short
    *)&v27[(*(unsigned int *)LODWORD(v26) & 0x7C00u) >> 8] << 10) | *(unsigned
    int *)LODWORD(v26) & 0x3FF) & 0x3E0u) >> 3] | (*(int *)&v27[(((*(int
    *)&v27[(*(unsigned int *)LODWORD(v26) & 0x7C00u) >> 8] << 10) | (*(int
    *)&v27[(*(unsigned int *)LODWORD(v26) & 0x7C000000u) >> 24] << 26) |
    *(unsigned int *)LODWORD(v26) & 0x3FF03FF) & 0x3E00000u) >> 19] << 21) |
    ((*(int *)&v27[(*(unsigned int *)LODWORD(v26) & 0x7C00u) >> 8] << 10) |
    (*(int *)&v27[(*(unsigned int *)LODWORD(v26) & 0x7C000000u) >> 24] << 26) |
    *(unsigned int *)LODWORD(v26) & 0x3FF03FF) & 0x7C1F7C1F) & 0xFFE0FFE0);
    *(unsigned int *)LODWORD(v26) = result;
    LODWORD(v26) += 4;
    --v29;
    }
    while ( v29 );
    LODWORD(v26) += v37;
    v28 = v32 - 1;
    }
    while ( v32 != 1 );
    }
    }*/
}

void Render::SetUIClipRect(unsigned int uX, unsigned int uY, unsigned int uZ,
                           unsigned int uW) {
    p2DGraphics->SetClip(Gdiplus::Rect(uX, uY, uZ - uX, uW - uY));
}

void Render::ResetUIClipRect() {
    p2DGraphics->SetClip(
        Gdiplus::Rect(0, 0, window->GetWidth(), window->GetHeight()));
}

uint32_t Color32_SwapRedBlue(uint16_t color16) {
    uint32_t c = color16;
    unsigned int b = (c & 31) * 8;
    unsigned int g = ((c >> 5) & 63) * 4;
    unsigned int r = ((c >> 11) & 31) * 8;

    return (b << 16) | (g << 8) | r;
}

Gdiplus::Bitmap *Render::BitmapWithImage(Image *image) {
    if (image == nullptr) {
        return nullptr;
    }

    Gdiplus::PixelFormat format = 0;
    uint8_t *data = nullptr;
    int stride = image->GetWidth();

    switch (image->GetFormat()) {
        case IMAGE_FORMAT_R5G6B5: {
            format = PixelFormat16bppRGB565;
            data = (uint8_t *)image->GetPixels(IMAGE_FORMAT_R5G6B5);
            stride *= 2;
            break;
        }
        case IMAGE_FORMAT_A1R5G5B5: {
            format = PixelFormat16bppARGB1555;
            data = (uint8_t *)image->GetPixels(IMAGE_FORMAT_A1R5G5B5);
            stride *= 2;
            break;
        }
        case IMAGE_FORMAT_A8R8G8B8: {
            format = PixelFormat32bppARGB;
            data = (uint8_t *)image->GetPixels(IMAGE_FORMAT_A8R8G8B8);
            stride *= 4;
            break;
        }
        case IMAGE_FORMAT_R8G8B8: {
            format = PixelFormat32bppRGB;
            data = (uint8_t *)image->GetPixels(IMAGE_FORMAT_R8G8B8);
            stride *= 4;
            break;
        }
        case IMAGE_FORMAT_R8G8B8A8:
        default:
            return nullptr;
    }

    if (data == nullptr) {
        return nullptr;
    }

    Gdiplus::Bitmap *bitmap = new Gdiplus::Bitmap(
        image->GetWidth(), image->GetHeight(), stride, format, data);
    if (bitmap->GetLastStatus() != Gdiplus::Ok) {
        delete bitmap;
        bitmap = nullptr;
    }

    return bitmap;
}

void Render::DrawTextureCustomHeight(float u, float v, class Image *image,
                                     int custom_height) {
    Gdiplus::Bitmap *bitmap = BitmapWithImage(image);
    if (bitmap == nullptr) {
        return;
    }

    int x = 640 * u;
    int y = 480 * v;

    p2DGraphics->DrawImage(bitmap, x, y, 0, 0, image->GetWidth(), custom_height,
                           Gdiplus::UnitPixel);

    delete bitmap;
}

void Render::DrawTextureNew(float u, float v, Image *bmp) {
    DrawTextureCustomHeight(u, v, bmp, bmp->GetHeight());
}

void Render::DrawTextureOffset(int x, int y, int offset_x, int offset_y,
                               Image *image) {
    Gdiplus::Bitmap *bitmap = BitmapWithImage(image);
    if (bitmap == nullptr) {
        return;
    }

    p2DGraphics->DrawImage(bitmap, x, y, offset_x, offset_y,
                           image->GetWidth() - offset_x,
                           image->GetHeight() - offset_y, Gdiplus::UnitPixel);

    delete bitmap;
}

void Render::DrawImage(Image *image, const Rect &rect) {
    Gdiplus::Bitmap *bitmap = BitmapWithImage(image);
    if (bitmap == nullptr) {
        return;
    }

    Gdiplus::Rect r(rect.x, rect.y, rect.z - rect.x, rect.w - rect.y);
    p2DGraphics->DrawImage(bitmap, r);

    delete bitmap;
}

void Render::DrawTextureGrayShade(float u, float v, Image *img) {
    DrawMasked(u, v, img, 1, 0x7BEF);
}

void Render::FillRectFast(unsigned int uX, unsigned int uY, unsigned int uWidth,
                          unsigned int uHeight, unsigned int color) {
    unsigned int b = (color & 0x1F) << 3;
    unsigned int g = ((color >> 5) & 0x3F) << 2;
    unsigned int r = ((color >> 11) & 0x1F) << 3;

    Gdiplus::Color c(r, g, b);
    Gdiplus::SolidBrush brush(c);
    p2DGraphics->FillRectangle(&brush, (INT)uX, (INT)uY, (INT)uWidth,
                               (INT)uHeight);
}

void Render::DrawText(int uOutX, int uOutY, uint8_t *pFontPixels,
                      unsigned int uCharWidth, unsigned int uCharHeight,
                      uint8_t *pFontPalette, uint16_t uFaceColor,
                      uint16_t uShadowColor) {
    Image *fonttemp = Image::Create(uCharWidth, uCharHeight, IMAGE_FORMAT_A8R8G8B8);
    uint32_t *fontpix = (uint32_t*)fonttemp->GetPixels(IMAGE_FORMAT_A8R8G8B8);

    for (uint y = 0; y < uCharHeight; ++y) {
        for (uint x = 0; x < uCharWidth; ++x) {
            if (*pFontPixels) {
                uint16_t color = uShadowColor;
                if (*pFontPixels != 1) {
                    color = uFaceColor;
                }
                fontpix[x + y * uCharWidth] = Color32(color);
            }
            ++pFontPixels;
        }
    }
    render->DrawTextureAlphaNew(uOutX / 640., uOutY / 480., fonttemp);
    fonttemp->Release();
}

void Render::DrawTextAlpha(int x, int y, uint8_t *font_pixels, int uCharWidth,
                           unsigned int uFontHeight, uint8_t *pPalette,
                           bool present_time_transparency) {
    Image *fonttemp = Image::Create(uCharWidth, uFontHeight, IMAGE_FORMAT_A8R8G8B8);
    uint32_t *fontpix = (uint32_t *)fonttemp->GetPixels(IMAGE_FORMAT_A8R8G8B8);

    if (present_time_transparency) {
        for (unsigned int dy = 0; dy < uFontHeight; ++dy) {
            for (unsigned int dx = 0; dx < uCharWidth; ++dx) {
                uint16_t color = (*font_pixels)
                                     ? pPalette[*font_pixels]
                                     : 0x7FF;  // transparent color 16bit
                                               // render->uTargetGMask |
                                               // render->uTargetBMask;
                fontpix[dx + dy * uCharWidth] = Color32(color);
                ++font_pixels;
            }
        }
    } else {
        for (unsigned int dy = 0; dy < uFontHeight; ++dy) {
            for (unsigned int dx = 0; dx < uCharWidth; ++dx) {
                if (*font_pixels) {
                    uint8_t index = *font_pixels;
                    uint8_t r = pPalette[index * 3 + 0];
                    uint8_t g = pPalette[index * 3 + 1];
                    uint8_t b = pPalette[index * 3 + 2];
                    fontpix[dx + dy * uCharWidth] = Color32(r, g, b);
                }
                ++font_pixels;
            }
        }
    }
    render->DrawTextureAlphaNew(x / 640., y / 480., fonttemp);
    fonttemp->Release();
}

void Render::DrawTransparentGreenShade(float u, float v, Image *pTexture) {
    DrawMasked(u, v, pTexture, 0, 0x07E0);
}

void Render::DrawTransparentRedShade(float u, float v, Image *a4) {
    DrawMasked(u, v, a4, 0, 0xF800);
}

void Render::DrawMasked(float u, float v, Image *pTexture,
                        unsigned int color_dimming_level, uint16_t mask) {
    if (!pTexture) {
        return;
    }
    uint32_t width = pTexture->GetWidth();
    uint32_t *pixels = (uint32_t *)pTexture->GetPixels(IMAGE_FORMAT_A8R8G8B8);
    Image *temp = Image::Create(width, pTexture->GetHeight(), IMAGE_FORMAT_A8R8G8B8);
    uint32_t *temppix = (uint32_t *)temp->GetPixels(IMAGE_FORMAT_A8R8G8B8);

    for (unsigned int dy = 0; dy < pTexture->GetHeight(); ++dy) {
        for (unsigned int dx = 0; dx < width; ++dx) {
            if (*pixels & 0xFF000000)
                temppix[dx + dy * width] = Color32((((*pixels >> 16) & 0xFF) >> color_dimming_level),
                    (((*pixels >> 8) & 0xFF) >> color_dimming_level), ((*pixels & 0xFF) >> color_dimming_level))
                    &  Color32(mask);
            ++pixels;
        }
    }
    render->DrawTextureAlphaNew(u, v, temp);
    temp->Release();
}

void Render::TexturePixelRotateDraw(float u, float v, Image *img, int time) {
    if (img) {
        auto pixelpoint = (const uint32_t *)img->GetPixels(IMAGE_FORMAT_A8R8G8B8);
        int width = img->GetWidth();
        int height = img->GetHeight();
        Image *temp = Image::Create(width, height, IMAGE_FORMAT_A8R8G8B8);
        uint32_t *temppix = (uint32_t *)temp->GetPixels(IMAGE_FORMAT_A8R8G8B8);

        int brightloc = -1;
        int brightval = 0;
        int darkloc = -1;
        int darkval = 765;

        for (int x = 0; x < width; x++) {
            for (int y = 0; y < height; y++) {
                int nudge = x + y * width;
                // Test the brightness against the threshold
                int bright = (*(pixelpoint + nudge) & 0xFF) + ((*(pixelpoint + nudge) >> 8) & 0xFF) + ((*(pixelpoint + nudge) >> 16) & 0xFF);
                if (bright == 0) continue;

                if (bright > brightval) {
                    brightval = bright;
                    brightloc = nudge;
                }
                if (bright < darkval) {
                    darkval = bright;
                    darkloc = nudge;
                }
            }
        }

       // find brightest
        unsigned int bmax = (*(pixelpoint + brightloc) & 0xFF);
        unsigned int gmax = ((*(pixelpoint + brightloc) >> 8) & 0xFF);
        unsigned int rmax = ((*(pixelpoint + brightloc) >> 16) & 0xFF);

        // find darkest not black
        unsigned int bmin = (*(pixelpoint + darkloc) & 0xFF);
        unsigned int gmin = ((*(pixelpoint + darkloc) >> 8) & 0xFF);
        unsigned int rmin = ((*(pixelpoint + darkloc) >> 16) & 0xFF);

        // steps pixels
        float bstep = (bmax - bmin) / 128.;
        float gstep = (gmax - gmin) / 128.;
        float rstep = (rmax - rmin) / 128.;

        int timestep = time % 256;

        // loop through
        for (int ydraw = 0; ydraw < height; ++ydraw) {
            for (int xdraw = 0; xdraw < width; ++xdraw) {
                if (*pixelpoint) {  // check orig item not got blakc pixel
                    unsigned int bcur = (*(pixelpoint) & 0xFF);
                    unsigned int gcur = ((*(pixelpoint) >> 8) & 0xFF);
                    unsigned int rcur = ((*(pixelpoint) >> 16) & 0xFF);
                    int pixstepb = (bcur - bmin) / bstep + timestep;
                    if (pixstepb > 255) pixstepb = pixstepb - 256;
                    if (pixstepb >= 0 && pixstepb < 128)  // 0-127
                        bcur = bmin + pixstepb * bstep;
                    if (pixstepb >= 128 && pixstepb < 256) {  // 128-255
                        pixstepb = pixstepb - 128;
                        bcur = bmax - pixstepb * bstep;
                    }
                    int pixstepr = (rcur - rmin) / rstep + timestep;
                    if (pixstepr > 255) pixstepr = pixstepr - 256;
                    if (pixstepr >= 0 && pixstepr < 128)  // 0-127
                        rcur = rmin + pixstepr * rstep;
                    if (pixstepr >= 128 && pixstepr < 256) {  // 128-255
                        pixstepr = pixstepr - 128;
                        rcur = rmax - pixstepr * rstep;
                    }
                    int pixstepg = (gcur - gmin) / gstep + timestep;
                    if (pixstepg > 255) pixstepg = pixstepg - 256;
                    if (pixstepg >= 0 && pixstepg < 128)  // 0-127
                        gcur = gmin + pixstepg * gstep;
                    if (pixstepg >= 128 && pixstepg < 256) {  // 128-255
                        pixstepg = pixstepg - 128;
                        gcur = gmax - pixstepg * gstep;
                    }
                    // out pixel
                    temppix[xdraw + ydraw * width] = Color32(rcur, gcur, bcur);
                }
                pixelpoint++;
            }
        }
        // draw image
        render->DrawTextureAlphaNew(u, v, temp);
        temp->Release();
    }
}

void Render::BlendTextures(
    int x, int y, Image *imgin, Image *imgblend, int time, int start_opacity,
    int end_opacity) {  // thrown together as a crude estimate of the enchaintg
                        // effects

    // leaves gap where it shouldnt on dark pixels currently
    // doesnt use opacity params

    const uint32_t *pixelpoint;
    const uint32_t *pixelpointblend;

    if (imgin && imgblend) {  // 2 images to blend
        pixelpoint = (const uint32_t *)imgin->GetPixels(IMAGE_FORMAT_A8R8G8B8);
        pixelpointblend =
            (const uint32_t *)imgblend->GetPixels(IMAGE_FORMAT_A8R8G8B8);

        int Width = imgin->GetWidth();
        int Height = imgin->GetHeight();
        Image *temp = Image::Create(Width, Height, IMAGE_FORMAT_A8R8G8B8);
        uint32_t *temppix = (uint32_t *)temp->GetPixels(IMAGE_FORMAT_A8R8G8B8);

        uint32_t c = *(pixelpointblend + 2700);  // guess at brightest pixel
        unsigned int bmax = (c & 0xFF);
        unsigned int gmax = ((c >> 8) & 0xFF);
        unsigned int rmax = ((c >> 16) & 0xFF);

        unsigned int bmin = bmax / 10;
        unsigned int gmin = gmax / 10;
        unsigned int rmin = rmax / 10;

        unsigned int bstep = (bmax - bmin) / 128;
        unsigned int gstep = (gmax - gmin) / 128;
        unsigned int rstep = (rmax - rmin) / 128;

        for (int ydraw = 0; ydraw < Height; ++ydraw) {
            for (int xdraw = 0; xdraw < Width; ++xdraw) {
                // should go blue -> black -> blue reverse
                // patchy -> solid -> patchy

                if (*pixelpoint) {  // check orig item not got blakc pixel
                    uint32_t nudge =
                        (xdraw % imgblend->GetWidth()) +
                        (ydraw % imgblend->GetHeight()) * imgblend->GetWidth();
                    uint32_t pixcol = *(pixelpointblend + nudge);

                    unsigned int bcur = (pixcol & 0xFF);
                    unsigned int gcur = ((pixcol >> 8) & 0xFF);
                    unsigned int rcur = ((pixcol >> 16) & 0xFF);

                    int steps = (time) % 128;

                    if ((time) % 256 >= 128) {  // step down
                        bcur += bstep * (128 - steps);
                        gcur += gstep * (128 - steps);
                        rcur += rstep * (128 - steps);
                    } else {  // step up
                        bcur += bstep * steps;
                        gcur += gstep * steps;
                        rcur += rstep * steps;
                    }

                    if (bcur > bmax) bcur = bmax;  // limit check
                    if (gcur > gmax) gcur = gmax;
                    if (rcur > rmax) rcur = rmax;
                    if (bcur < bmin) bcur = bmin;
                    if (gcur < gmin) gcur = gmin;
                    if (rcur < rmin) rcur = rmin;

                    temppix[xdraw + ydraw * Width] = Color32(rcur, gcur, bcur);
                }

                pixelpoint++;
            }

            pixelpoint += imgin->GetWidth() - Width;
        }
        // draw image
        render->DrawTextureAlphaNew(x/640., y/480., temp);
        temp->Release();
    }
}

void Render::DrawMonsterPortrait(Rect rc, SpriteFrame *Portrait, int Y_Offset) {
    int dst_x = rc.x + 64 + Portrait->hw_sprites[0]->uAreaX - Portrait->hw_sprites[0]->uBufferWidth / 2;
    int dst_y = rc.y + Y_Offset + Portrait->hw_sprites[0]->uAreaY;
    uint dst_z = dst_x + Portrait->hw_sprites[0]->uAreaWidth;
    uint dst_w = dst_y + Portrait->hw_sprites[0]->uAreaHeight;

    uint Clipped_X = 0;
    uint Clipped_Y = 0;

    if (dst_x < rc.x) {
        Clipped_X = rc.x - dst_x;
        dst_x = rc.x;
    }

    if (dst_y < rc.y) {
        Clipped_Y = rc.y - dst_y;
        dst_y = rc.y;
    }

    if (dst_z > rc.z)
        dst_z = rc.z;
    if (dst_w > rc.w)
        dst_w = rc.w;

    Image *temp = Image::Create(128, 128, IMAGE_FORMAT_R5G6B5);
    uint16_t *temppix = (uint16_t *)temp->GetPixels(IMAGE_FORMAT_R5G6B5);

    int width = Portrait->hw_sprites[0]->texture->GetWidth();
    int height = Portrait->hw_sprites[0]->texture->GetHeight();

    ushort* src = (unsigned __int16 *)Portrait->hw_sprites[0]->texture->GetPixels(IMAGE_FORMAT_A1R5G5B5);
    int num_top_scanlines_above_frame_y = Clipped_Y - dst_y;

    for (uint y = dst_y; y < dst_w; ++y) {
        uint src_y = num_top_scanlines_above_frame_y + y;

        for (uint x = dst_x; x < dst_z; ++x) {
            uint src_x = Clipped_X - dst_x + x;  // num scanlines left to frame_x  + current x

            uint idx =
                height * src_y / Portrait->hw_sprites[0]->uAreaHeight * width +
                width * src_x / Portrait->hw_sprites[0]->uAreaWidth;

            uint a = 2 * (src[idx] & 0xFFE0);
            uint b = src[idx] & 0x1F;

            temppix[(x-dst_x) + 128*(y-dst_y)] = (b | a);
        }
    }

    render->SetUIClipRect(rc.x, rc.y, rc.z, rc.w);
    render->DrawTextureAlphaNew(dst_x / 640., dst_y / 480., temp);
    temp->Release();
    render->ResetUIClipRect();
}

void Render::DrawTextureAlphaNew(float u, float v, Image *image) {
    Gdiplus::Bitmap *bitmap = BitmapWithImage(image);
    if (!bitmap) {
        return;
    }

    int uX = u * 640.0f;
    int uY = v * 480.0f;
    p2DGraphics->DrawImage(bitmap, uX, uY);

    delete bitmap;
}

void Render::ZDrawTextureAlpha(float u, float v, Image *img, int zVal) {
    if (!img) return;

    int uOutX = u * this->window->GetWidth();
    int uOutY = v * this->window->GetHeight();
    unsigned int imgheight = img->GetHeight();
    unsigned int imgwidth = img->GetWidth();
    auto pixels = (uint32_t *)img->GetPixels(IMAGE_FORMAT_A8R8G8B8);

    for (int xs = 0; xs < imgwidth; xs++) {
        for (int ys = 0; ys < imgheight; ys++) {
            if (pixels[xs + imgwidth * ys] & 0xFF000000) {
                this->pActiveZBuffer[uOutX + xs + window->GetWidth() * (uOutY + ys)] = zVal;
            }
        }
    }
}

void Render::ZBuffer_Fill_2(signed int a2, signed int a3, Image *pTexture,
                            int a5) {}

void Render::DoRenderBillboards_D3D() {
    ErrD3D(pRenderD3D->pDevice->SetTextureStageState(0, D3DTSS_ADDRESS, D3DTADDRESS_CLAMP));
    ErrD3D(pRenderD3D->pDevice->SetRenderState(D3DRENDERSTATE_ALPHABLENDENABLE, TRUE));
    ErrD3D(pRenderD3D->pDevice->SetRenderState(D3DRENDERSTATE_ZWRITEENABLE, FALSE));
    ErrD3D(pRenderD3D->pDevice->SetRenderState(D3DRENDERSTATE_CULLMODE, D3DCULL_NONE));

    for (int i = uNumBillboardsToDraw - 1; i >= 0; --i) {
        if (pBillboardRenderListD3D[i].opacity != RenderBillboardD3D::NoBlend) {
            SetBillboardBlendOptions(pBillboardRenderListD3D[i].opacity);
        }

        if (pBillboardRenderListD3D[i].texture) {
            pRenderD3D->pDevice->SetTexture(
                0, ((TextureD3D *)pBillboardRenderListD3D[i].texture)
                ->GetDirect3DTexture());
        } else {
            // auto hwsplat04 = assets->GetBitmap("hwsplat04");
            // ErrD3D(pRenderD3D->pDevice->SetTexture(0, ((TextureD3D *)hwsplat04)->GetDirect3DTexture()));
            // testing
            ErrD3D(pRenderD3D->pDevice->SetTexture(0, 0));
        }

        ErrD3D(pRenderD3D->pDevice->DrawPrimitive(
            D3DPT_TRIANGLEFAN,
            D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_SPECULAR | D3DFVF_TEX1,
            pBillboardRenderListD3D[i].pQuads,
            pBillboardRenderListD3D[i].uNumVertices,
            D3DDP_DONOTLIGHT | D3DDP_DONOTUPDATEEXTENTS));
    }

    if (config->is_using_fog) {
        ErrD3D(pRenderD3D->pDevice->SetRenderState(D3DRENDERSTATE_FOGENABLE, TRUE));
        ErrD3D(pRenderD3D->pDevice->SetRenderState(D3DRENDERSTATE_FOGCOLOR, GetLevelFogColor() & 0xFFFFFF));
        ErrD3D(pRenderD3D->pDevice->SetRenderState(D3DRENDERSTATE_FOGTABLEMODE, 0));
    }
    ErrD3D(pRenderD3D->pDevice->SetRenderState(D3DRENDERSTATE_CULLMODE, D3DCULL_CW));
    ErrD3D(pRenderD3D->pDevice->SetRenderState(D3DRENDERSTATE_ZWRITEENABLE, TRUE));
    ErrD3D(pRenderD3D->pDevice->SetRenderState(D3DRENDERSTATE_ALPHABLENDENABLE, FALSE));
    ErrD3D(pRenderD3D->pDevice->SetRenderState(D3DRENDERSTATE_SRCBLEND, D3DBLEND_ONE));
    ErrD3D(pRenderD3D->pDevice->SetRenderState(D3DRENDERSTATE_DESTBLEND, D3DBLEND_ZERO));
    ErrD3D(pRenderD3D->pDevice->SetRenderState(D3DRENDERSTATE_DITHERENABLE, TRUE));
}

void Render::SetBillboardBlendOptions(RenderBillboardD3D::OpacityType a1) {
    switch (a1) {
        case RenderBillboardD3D::Transparent: {
            if (config->is_using_fog) {
                SetUsingFog(false);
                ErrD3D(pRenderD3D->pDevice->SetRenderState(D3DRENDERSTATE_FOGENABLE, TRUE));
                ErrD3D(pRenderD3D->pDevice->SetRenderState(D3DRENDERSTATE_FOGCOLOR, GetLevelFogColor() & 0xFFFFFF));
                ErrD3D(pRenderD3D->pDevice->SetRenderState(D3DRENDERSTATE_FOGTABLEMODE, 0));
            }

            ErrD3D(pRenderD3D->pDevice->SetRenderState(D3DRENDERSTATE_SRCBLEND, D3DBLEND_SRCALPHA));
            ErrD3D(pRenderD3D->pDevice->SetRenderState(D3DRENDERSTATE_DESTBLEND, D3DBLEND_INVSRCALPHA));
            ErrD3D(pRenderD3D->pDevice->SetRenderState(D3DRENDERSTATE_DITHERENABLE, TRUE));

            break;
        }

        case RenderBillboardD3D::Opaque_1:
        case RenderBillboardD3D::Opaque_2:
        case RenderBillboardD3D::Opaque_3: {
            if (config->is_using_specular) {
                if (!config->is_using_fog) {
                    SetUsingFog(true);
                    ErrD3D(pRenderD3D->pDevice->SetRenderState(D3DRENDERSTATE_FOGENABLE, FALSE));
                }
            }

            ErrD3D(pRenderD3D->pDevice->SetRenderState(D3DRENDERSTATE_SRCBLEND, D3DBLEND_ONE));
            ErrD3D(pRenderD3D->pDevice->SetRenderState(D3DRENDERSTATE_DESTBLEND, D3DBLEND_ONE));
            ErrD3D(pRenderD3D->pDevice->SetRenderState(D3DRENDERSTATE_DITHERENABLE, FALSE));

            break;
        }

        default: {
            log->Warning(
                L"SetBillboardBlendOptions: invalid opacity type (%u)", a1);
            assert(false);  // fireball and impolision can crash here - maybe clipping related
            break;
        }
    }
}

int ODM_NearClip(unsigned int num_vertices) {
    float nearclip = pIndoorCameraD3D->GetNearClip();
    bool current_vertices_flag;  // edi@1
    bool next_vertices_flag;     // [sp+Ch] [bp-24h]@6
    double t;                    // st6@10
    bool bFound = false;

    if (!num_vertices) return 0;
    for (uint i = 0; i < num_vertices; ++i) {  // есть ли пограничные вершины
        if (VertexRenderList[i].vWorldViewPosition.x > nearclip) {
            bFound = true;
            break;
        }
    }
    if (!bFound) return 0;

    memcpy(&VertexRenderList[num_vertices], &VertexRenderList[0],
           sizeof(VertexRenderList[0]));
    next_vertices_flag = false;
    current_vertices_flag = false;
    if (VertexRenderList[0].vWorldViewPosition.x <= nearclip)
        current_vertices_flag = true;
    // check for near clip plane(проверка по ближней границе)
    //
    // v3.__________________. v0
    //   |                  |
    //   |                  |
    //   |                  |
    //  ----------------------- 8.0(near_clip - 8.0)
    //   |                  |
    //   .__________________.
    //  v2                     v1

    int out_num_vertices = 0;
    for (uint i = 0; i < num_vertices; ++i) {
        next_vertices_flag =
            VertexRenderList[i + 1].vWorldViewPosition.x <= nearclip;  //
        if (current_vertices_flag ^ next_vertices_flag) {
            if (next_vertices_flag) {  // следующая вершина за ближней границей
                // t = near_clip - v0.x / v1.x - v0.x    (формула получения
                // точки пересечения отрезка с плоскостью)
                t = (nearclip - VertexRenderList[i].vWorldViewPosition.x) /
                    (VertexRenderList[i + 1].vWorldViewPosition.x -
                     VertexRenderList[i].vWorldViewPosition.x);
                array_507D30[out_num_vertices].vWorldViewPosition.x = nearclip;
                array_507D30[out_num_vertices].vWorldViewPosition.y =
                    VertexRenderList[i].vWorldViewPosition.y +
                    (VertexRenderList[i + 1].vWorldViewPosition.y -
                     VertexRenderList[i].vWorldViewPosition.y) *
                        t;
                array_507D30[out_num_vertices].vWorldViewPosition.z =
                    VertexRenderList[i].vWorldViewPosition.z +
                    (VertexRenderList[i + 1].vWorldViewPosition.z -
                     VertexRenderList[i].vWorldViewPosition.z) *
                        t;
                array_507D30[out_num_vertices].u =
                    VertexRenderList[i].u +
                    (VertexRenderList[i + 1].u - VertexRenderList[i].u) * t;
                array_507D30[out_num_vertices].v =
                    VertexRenderList[i].v +
                    (VertexRenderList[i + 1].v - VertexRenderList[i].v) * t;
                array_507D30[out_num_vertices]._rhw = 1.0 / nearclip;
            } else {  // текущая вершина за ближней границей
                t = (nearclip - VertexRenderList[i].vWorldViewPosition.x) /
                    (VertexRenderList[i].vWorldViewPosition.x -
                     VertexRenderList[i + 1].vWorldViewPosition.x);
                array_507D30[out_num_vertices].vWorldViewPosition.x = nearclip;
                array_507D30[out_num_vertices].vWorldViewPosition.y =
                    VertexRenderList[i].vWorldViewPosition.y +
                    (VertexRenderList[i].vWorldViewPosition.y -
                     VertexRenderList[i + 1].vWorldViewPosition.y) *
                        t;
                array_507D30[out_num_vertices].vWorldViewPosition.z =
                    VertexRenderList[i].vWorldViewPosition.z +
                    (VertexRenderList[i].vWorldViewPosition.z -
                     VertexRenderList[i + 1].vWorldViewPosition.z) *
                        t;
                array_507D30[out_num_vertices].u =
                    VertexRenderList[i].u +
                    (VertexRenderList[i].u - VertexRenderList[i + 1].u) * t;
                array_507D30[out_num_vertices].v =
                    VertexRenderList[i].v +
                    (VertexRenderList[i].v - VertexRenderList[i + 1].v) * t;
                array_507D30[out_num_vertices]._rhw = 1.0 / nearclip;
            }
            // array_507D30[out_num_vertices]._rhw = 0x3E000000u;
            ++out_num_vertices;
        }
        if (!next_vertices_flag) {
            memcpy(&array_507D30[out_num_vertices], &VertexRenderList[i + 1],
                   sizeof(VertexRenderList[i + 1]));
            out_num_vertices++;
        }
        current_vertices_flag = next_vertices_flag;
    }
    return out_num_vertices >= 3 ? out_num_vertices : 0;
}

int ODM_FarClip(unsigned int uNumVertices) {
    bool current_vertices_flag;     // [sp+Ch] [bp-28h]@6
    bool next_vertices_flag;        // edi@1
    double t;                       // st6@10
    signed int depth_num_vertices;  // [sp+18h] [bp-1Ch]@1
    bool bFound;
    //Доп инфо "Программирование трёхмерных игр для windows" Ламот стр 910

    bFound = false;

    memcpy(&VertexRenderList[uNumVertices], &VertexRenderList[0],
           sizeof(VertexRenderList[uNumVertices]));
    depth_num_vertices = 0;
    current_vertices_flag = false;
    if (VertexRenderList[0].vWorldViewPosition.x >=
        pIndoorCameraD3D->GetFarClip())
        current_vertices_flag =
            true;  //настоящая вершина больше границы видимости
    if ((signed int)uNumVertices <= 0) return 0;
    for (uint i = 0; i < uNumVertices; ++i) {  // есть ли пограничные вершины
        if (VertexRenderList[i].vWorldViewPosition.x <
            pIndoorCameraD3D->GetFarClip()) {
            bFound = true;
            break;
        }
    }
    if (!bFound) return 0;
    // check for far clip plane(проверка по дальней границе)
    //
    // v3.__________________. v0
    //   |                  |
    //   |                  |
    //   |                  |
    //  ----------------------- 8192.0(far_clip - 0x2000)
    //   |                  |
    //   .__________________.
    //  v2                     v1

    for (uint i = 0; i < uNumVertices; ++i) {
        next_vertices_flag = VertexRenderList[i + 1].vWorldViewPosition.x >=
                             pIndoorCameraD3D->GetFarClip();
        if (current_vertices_flag ^
            next_vertices_flag) {  // одна из граней за границей видимости
            if (next_vertices_flag) {  // следующая вершина больше границы
                                       // видимости(настоящая вершина меньше
                                       // границы видимости) - v3
                // t = far_clip - v2.x / v3.x - v2.x (формула получения точки
                // пересечения отрезка с плоскостью)
                t = (pIndoorCameraD3D->GetFarClip() -
                     VertexRenderList[i].vWorldViewPosition.x) /
                    (VertexRenderList[i].vWorldViewPosition.x -
                     VertexRenderList[i + 1].vWorldViewPosition.x);
                array_507D30[depth_num_vertices].vWorldViewPosition.x =
                    pIndoorCameraD3D->GetFarClip();
                // New_y = v2.y + (v3.y - v2.y)*t
                array_507D30[depth_num_vertices].vWorldViewPosition.y =
                    VertexRenderList[i].vWorldViewPosition.y +
                    (VertexRenderList[i].vWorldViewPosition.y -
                     VertexRenderList[i + 1].vWorldViewPosition.y) *
                        t;
                // New_z = v2.z + (v3.z - v2.z)*t
                array_507D30[depth_num_vertices].vWorldViewPosition.z =
                    VertexRenderList[i].vWorldViewPosition.z +
                    (VertexRenderList[i].vWorldViewPosition.z -
                     VertexRenderList[i + 1].vWorldViewPosition.z) *
                        t;
                array_507D30[depth_num_vertices].u =
                    VertexRenderList[i].u +
                    (VertexRenderList[i].u - VertexRenderList[i + 1].u) * t;
                array_507D30[depth_num_vertices].v =
                    VertexRenderList[i].v +
                    (VertexRenderList[i].v - VertexRenderList[i + 1].v) * t;
                array_507D30[depth_num_vertices]._rhw =
                    1.0 / pIndoorCameraD3D->GetFarClip();
            } else {  // настоящая вершина больше границы видимости(следующая
                      // вершина меньше границы видимости) - v0
                // t = far_clip - v1.x / v0.x - v1.x
                t = (pIndoorCameraD3D->GetFarClip() -
                     VertexRenderList[i].vWorldViewPosition.x) /
                    (VertexRenderList[i + 1].vWorldViewPosition.x -
                     VertexRenderList[i].vWorldViewPosition.x);
                array_507D30[depth_num_vertices].vWorldViewPosition.x =
                    pIndoorCameraD3D->GetFarClip();
                // New_y = (v0.y - v1.y)*t + v1.y
                array_507D30[depth_num_vertices].vWorldViewPosition.y =
                    VertexRenderList[i].vWorldViewPosition.y +
                    (VertexRenderList[i + 1].vWorldViewPosition.y -
                     VertexRenderList[i].vWorldViewPosition.y) *
                        t;
                // New_z = (v0.z - v1.z)*t + v1.z
                array_507D30[depth_num_vertices].vWorldViewPosition.z =
                    VertexRenderList[i].vWorldViewPosition.z +
                    (VertexRenderList[i + 1].vWorldViewPosition.z -
                     VertexRenderList[i].vWorldViewPosition.z) *
                        t;
                array_507D30[depth_num_vertices].u =
                    VertexRenderList[i].u +
                    (VertexRenderList[i + 1].u - VertexRenderList[i].u) * t;
                array_507D30[depth_num_vertices].v =
                    VertexRenderList[i].v +
                    (VertexRenderList[i + 1].v - VertexRenderList[i].v) * t;
                array_507D30[depth_num_vertices]._rhw =
                    1.0 / pIndoorCameraD3D->GetFarClip();
            }
            ++depth_num_vertices;
        }
        if (!next_vertices_flag) {  //оба в границе видимости
            memcpy(&array_507D30[depth_num_vertices], &VertexRenderList[i + 1],
                   sizeof(array_507D30[depth_num_vertices]));
            depth_num_vertices++;
        }
        current_vertices_flag = next_vertices_flag;
    }
    return depth_num_vertices >= 3 ? depth_num_vertices : 0;
}

void Render::DrawBuildingsD3D() {
    // int v27;  // eax@57
    int farclip;  // [sp+2Ch] [bp-2Ch]@10
    int nearclip;  // [sp+30h] [bp-28h]@34
    int v51;  // [sp+34h] [bp-24h]@35
    int v52;  // [sp+38h] [bp-20h]@36
    int v53;  // [sp+3Ch] [bp-1Ch]@8

    for (BSPModel &model : pOutdoor->pBModels) {
        int reachable;
        if (!IsBModelVisible(&model, &reachable)) {
            continue;
        }
        model.field_40 |= 1;
        if (model.pFaces.empty()) {
            continue;
        }

        for (ODMFace &face : model.pFaces) {
            if (face.Invisible()) {
                continue;
            }

            v53 = 0;
            auto poly = &array_77EC08[pODMRenderParams->uNumPolygons];

            poly->flags = 0;
            poly->field_32 = 0;
            poly->texture = face.GetTexture();

            if (face.uAttributes & FACE_FLUID) poly->flags |= 2;
            if (face.uAttributes & FACE_INDOOR_SKY) poly->flags |= 0x400;

            if (face.uAttributes & FACE_FLOW_DIAGONAL)
                poly->flags |= 0x400;
            else if (face.uAttributes & FACE_FLOW_VERTICAL)
                poly->flags |= 0x800;

            if (face.uAttributes & FACE_FLOW_HORIZONTAL)
                poly->flags |= 0x2000;
            else if (face.uAttributes & FACE_DONT_CACHE_TEXTURE)
                poly->flags |= 0x1000;

            poly->sTextureDeltaU = face.sTextureDeltaU;
            poly->sTextureDeltaV = face.sTextureDeltaV;

            unsigned int flow_anim_timer = OS_GetTime() >> 4;
            unsigned int flow_u_mod = poly->texture->GetWidth() - 1;
            unsigned int flow_v_mod = poly->texture->GetHeight() - 1;

            if (face.pFacePlane.vNormal.z &&
                abs(face.pFacePlane.vNormal.z) >= 59082) {
                if (poly->flags & 0x400)
                    poly->sTextureDeltaV += flow_anim_timer & flow_v_mod;
                if (poly->flags & 0x800)
                    poly->sTextureDeltaV -= flow_anim_timer & flow_v_mod;
            } else {
                if (poly->flags & 0x400)
                    poly->sTextureDeltaV -= flow_anim_timer & flow_v_mod;
                if (poly->flags & 0x800)
                    poly->sTextureDeltaV += flow_anim_timer & flow_v_mod;
            }

            if (poly->flags & 0x1000)
                poly->sTextureDeltaU -= flow_anim_timer & flow_u_mod;
            else if (poly->flags & 0x2000)
                poly->sTextureDeltaU += flow_anim_timer & flow_u_mod;

            nearclip = 0;
            farclip = 0;

            for (uint vertex_id = 1; vertex_id <= face.uNumVertices;
                 vertex_id++) {
                array_73D150[vertex_id - 1].vWorldPosition.x =
                    model.pVertices.pVertices[face.pVertexIDs[vertex_id - 1]].x;
                array_73D150[vertex_id - 1].vWorldPosition.y =
                    model.pVertices.pVertices[face.pVertexIDs[vertex_id - 1]].y;
                array_73D150[vertex_id - 1].vWorldPosition.z =
                    model.pVertices.pVertices[face.pVertexIDs[vertex_id - 1]].z;
                array_73D150[vertex_id - 1].u =
                    (poly->sTextureDeltaU +
                     (__int16)face.pTextureUIDs[vertex_id - 1]) *
                    (1.0 / (double)poly->texture->GetWidth());
                array_73D150[vertex_id - 1].v =
                    (poly->sTextureDeltaV +
                     (__int16)face.pTextureVIDs[vertex_id - 1]) *
                    (1.0 / (double)poly->texture->GetHeight());
            }
            for (uint i = 1; i <= face.uNumVertices; i++) {
                if (model.pVertices.pVertices[face.pVertexIDs[0]].z ==
                    array_73D150[i - 1].vWorldPosition.z)
                    ++v53;
                pIndoorCameraD3D->ViewTransform(&array_73D150[i - 1], 1);
                if (array_73D150[i - 1].vWorldViewPosition.x <
                        pIndoorCameraD3D->GetNearClip() ||
                    array_73D150[i - 1].vWorldViewPosition.x >
                        pIndoorCameraD3D->GetFarClip()) {
                    if (array_73D150[i - 1].vWorldViewPosition.x >=
                        pIndoorCameraD3D->GetNearClip())
                        farclip = 1;
                    else
                        nearclip = 1;
                } else {
                    pIndoorCameraD3D->Project(&array_73D150[i - 1], 1, 0);
                }
            }

            if (v53 == face.uNumVertices) poly->field_32 |= 1;
            poly->pODMFace = &face;
            poly->uNumVertices = face.uNumVertices;
            poly->field_59 = 5;
            v51 =
                fixpoint_mul(-pOutdoor->vSunlight.x, face.pFacePlane.vNormal.x);
            v53 =
                fixpoint_mul(-pOutdoor->vSunlight.y, face.pFacePlane.vNormal.y);
            v52 =
                fixpoint_mul(-pOutdoor->vSunlight.z, face.pFacePlane.vNormal.z);
            poly->dimming_level = 20 - fixpoint_mul(20, v51 + v53 + v52);
            if (poly->dimming_level < 0) poly->dimming_level = 0;
            if (poly->dimming_level > 31) poly->dimming_level = 31;
            if (pODMRenderParams->uNumPolygons >= 1999 + 5000) return;
            if (ODMFace::IsBackfaceNotCulled(array_73D150, poly)) {
                face.bVisible = 1;
                poly->uBModelFaceID = face.index;
                poly->uBModelID = model.index;
                poly->pid =
                    PID(OBJECT_BModel, (face.index | (model.index << 6)));
                for (int vertex_id = 0; vertex_id < face.uNumVertices;
                     ++vertex_id) {
                    memcpy(&VertexRenderList[vertex_id],
                           &array_73D150[vertex_id],
                           sizeof(VertexRenderList[vertex_id]));
                    VertexRenderList[vertex_id]._rhw =
                        1.0 / (array_73D150[vertex_id].vWorldViewPosition.x +
                               0.0000001);
                }
                static stru154 static_RenderBuildingsD3D_stru_73C834;

                lightmap_builder->ApplyLights_OutdoorFace(&face);
                decal_builder->ApplyDecals_OutdoorFace(&face);
                lightmap_builder->StationaryLightsCount = 0;
                int v31 = 0;
                if (Lights.uNumLightsApplied > 0 || decal_builder->uNumDecals > 0) {
                    v31 = nearclip ? 3 : farclip != 0 ? 5 : 0;

                   // if (face.uAttributes & FACE_OUTLINED) __debugbreak();

                    static_RenderBuildingsD3D_stru_73C834.GetFacePlaneAndClassify(&face, &model.pVertices);
                    if (decal_builder->uNumDecals > 0) {
                        decal_builder->ApplyDecals(
                            31 - poly->dimming_level, 2,
                            &static_RenderBuildingsD3D_stru_73C834,
                            face.uNumVertices, VertexRenderList, 0, (char)v31,
                            -1);
                    }
                }
                if (Lights.uNumLightsApplied > 0)
                    // if (face.uAttributes & FACE_OUTLINED)
                    lightmap_builder->ApplyLights(
                        &Lights, &static_RenderBuildingsD3D_stru_73C834,
                        poly->uNumVertices, VertexRenderList, 0, (char)v31);

                if (nearclip) {
                    poly->uNumVertices = ODM_NearClip(face.uNumVertices);
                    ODM_Project(poly->uNumVertices);
                }
                if (farclip) {
                    poly->uNumVertices = ODM_FarClip(face.uNumVertices);
                    ODM_Project(poly->uNumVertices);
                }

                if (poly->uNumVertices) {
                    if (poly->IsWater()) {
                        if (poly->IsWaterAnimDisabled())
                            poly->texture = render->hd_water_tile_anim[0];
                        else
                            poly->texture =
                                render->hd_water_tile_anim
                                    [render->hd_water_current_frame];
                    }

                    render->DrawPolygon(poly);
                }
            }
        }
    }
}

unsigned short *Render::MakeScreenshot(int width, int height) {
    uint16_t *for_pixels;  // ebx@1
    DDSURFACEDESC2 Dst;    // [sp+4h] [bp-A0h]@6

    float interval_x = game_viewport_width / (double)width;
    float interval_y = game_viewport_height / (double)height;

    uint16_t *pPixels = (uint16_t *)malloc(sizeof(uint16_t) * height * width);
    memset(pPixels, 0, sizeof(uint16_t) * height * width);

    for_pixels = pPixels;

    BeginSceneD3D();

    if (uCurrentlyLoadedLevelType == LEVEL_Indoor) {
        pIndoor->Draw();
    } else if (uCurrentlyLoadedLevelType == LEVEL_Outdoor) {
        pOutdoor->Draw();
    }
    DrawBillboards_And_MaybeRenderSpecialEffects_And_EndScene();
    memset(&Dst, 0, sizeof(Dst));
    Dst.dwSize = sizeof(Dst);

    if (LockSurface_DDraw4(pBackBuffer4, &Dst, DDLOCK_WAIT)) {
        if (uCurrentlyLoadedLevelType == LEVEL_null) {
            memset(&for_pixels, 0, sizeof(for_pixels));
        } else {
            for (uint y = 0; y < (unsigned int)height; ++y) {
                for (uint x = 0; x < (unsigned int)width; ++x) {
                    if (Dst.ddpfPixelFormat.dwRGBBitCount == 32) {
                        unsigned __int32 *p =
                            (unsigned __int32 *)Dst.lpSurface +
                            (int)(x * interval_x + 8.0) +
                            (int)(y * interval_y + 8.0) * (Dst.lPitch >> 2);
                        *for_pixels = Color16((*p >> 16) & 255, (*p >> 8) & 255,
                                              *p & 255);
                    } else if (Dst.ddpfPixelFormat.dwRGBBitCount == 16) {
                        uint16_t *p =
                            (uint16_t*)Dst.lpSurface +
                            (int)(x * interval_x + 8.0) + y * Dst.lPitch;
                        *for_pixels = *p;
                    } else {
                        assert(false);
                    }
                    ++for_pixels;
                }
            }
        }
        ErrD3D(pBackBuffer4->Unlock(NULL));
    }
    return pPixels;
}

Image *Render::TakeScreenshot(unsigned int width, unsigned int height) {
    auto pixels = MakeScreenshot(width, height);
    Image *image = Image::Create(width, height, IMAGE_FORMAT_R5G6B5, pixels);
    return image;
}

void Render::SaveScreenshot(const String &filename, unsigned int width,
                            unsigned int height) {
    auto pixels = MakeScreenshot(width, height);
    SavePCXImage16(filename, pixels, width, height);
    free(pixels);
}

void Render::PackScreenshot(unsigned int width, unsigned int height, void *data,
                            unsigned int data_size,
                            unsigned int *out_screenshot_size) {
    auto pixels = MakeScreenshot(150, 112);
    PCX::Encode16(pixels, width, height, data, data_size, out_screenshot_size);
    free(pixels);
}

int Render::GetActorsInViewport(int pDepth) {
    unsigned int
        v3;  // eax@2 применяется в закле Жар печи для подсчёта кол-ва монстров
             // видимых группе и заполнения массива id видимых монстров
    unsigned int v5;   // eax@2
    unsigned int v6;   // eax@4
    unsigned int v12;  // [sp+10h] [bp-14h]@1
    int mon_num;       // [sp+1Ch] [bp-8h]@1
    unsigned int a1a;  // [sp+20h] [bp-4h]@1

    mon_num = 0;
    v12 = GetBillboardDrawListSize();
    if ((signed int)GetBillboardDrawListSize() > 0) {
        for (a1a = 0; (signed int)a1a < (signed int)v12; ++a1a) {
            v3 = GetParentBillboardID(a1a);
            v5 = (unsigned __int16)pBillboardRenderList[v3].object_pid;
            if (PID_TYPE(v5) == OBJECT_Actor) {
                if (pBillboardRenderList[v3].screen_space_z <= pDepth) {
                    v6 = PID_ID(v5);
                    if (pActors[v6].uAIState != Dead &&
                        pActors[v6].uAIState != Dying &&
                        pActors[v6].uAIState != Removed &&
                        pActors[v6].uAIState != Disabled &&
                        pActors[v6].uAIState != Summoned) {
                        if (vis->DoesRayIntersectBillboard(
                                (double)pDepth, a1a)) {
                            if (mon_num < 100) {
                                _50BF30_actors_in_viewport_ids[mon_num] = v6;
                                mon_num++;
                            }
                        }
                    }
                }
            }
        }
    }
    return mon_num;
}

void Render::BeginLightmaps() {
    ErrD3D(pRenderD3D->pDevice->SetTextureStageState(0, D3DTSS_ADDRESS, D3DTADDRESS_CLAMP));

    if (config->is_using_specular)
        pRenderD3D->pDevice->SetRenderState(D3DRENDERSTATE_FOGENABLE, FALSE);

    ErrD3D(pRenderD3D->pDevice->SetRenderState(D3DRENDERSTATE_ALPHABLENDENABLE, TRUE));
    ErrD3D(pRenderD3D->pDevice->SetRenderState(D3DRENDERSTATE_DITHERENABLE, FALSE));

    // ErrD3D(pRenderD3D->pDevice->SetTexture(0,
    // pIndoorCameraD3D->LoadTextureAndGetHardwarePtr("effpar03")));
    auto effpar03 = assets->GetBitmap("effpar03");
    ErrD3D(pRenderD3D->pDevice->SetTexture(
        0, ((TextureD3D *)effpar03)->GetDirect3DTexture()));

    ErrD3D(pRenderD3D->pDevice->SetRenderState(D3DRENDERSTATE_SRCBLEND,
                                               D3DBLEND_ONE));
    ErrD3D(pRenderD3D->pDevice->SetRenderState(D3DRENDERSTATE_DESTBLEND,
                                               D3DBLEND_ONE));
}

void Render::EndLightmaps() {
    ErrD3D(pRenderD3D->pDevice->SetRenderState(D3DRENDERSTATE_SRCBLEND, D3DBLEND_ONE));
    ErrD3D(pRenderD3D->pDevice->SetRenderState(D3DRENDERSTATE_DESTBLEND, D3DBLEND_ZERO));
    ErrD3D(pRenderD3D->pDevice->SetRenderState(D3DRENDERSTATE_ALPHABLENDENABLE, FALSE));
    ErrD3D(pRenderD3D->pDevice->SetRenderState(D3DRENDERSTATE_DITHERENABLE, TRUE));

    if (config->is_using_specular) {
        ErrD3D(pRenderD3D->pDevice->SetRenderState(D3DRENDERSTATE_FOGENABLE, TRUE));
        ErrD3D(pRenderD3D->pDevice->SetRenderState(D3DRENDERSTATE_FOGCOLOR, uFogColor));
        ErrD3D(pRenderD3D->pDevice->SetRenderState(D3DRENDERSTATE_FOGTABLEMODE, 0));
    }
}

void Render::BeginLightmaps2() {
    if (config->is_using_specular)
        ErrD3D(pRenderD3D->pDevice->SetRenderState(D3DRENDERSTATE_FOGENABLE, FALSE));

    ErrD3D(pRenderD3D->pDevice->SetRenderState(D3DRENDERSTATE_CULLMODE, D3DCULL_NONE));
    ErrD3D(pRenderD3D->pDevice->SetRenderState(D3DRENDERSTATE_ZWRITEENABLE, FALSE));
    ErrD3D(pRenderD3D->pDevice->SetRenderState(D3DRENDERSTATE_ALPHABLENDENABLE, TRUE));
    ErrD3D(pRenderD3D->pDevice->SetRenderState(D3DRENDERSTATE_DITHERENABLE, FALSE));

    // ErrD3D(pRenderD3D->pDevice->SetTexture(0,
    // pIndoorCameraD3D->LoadTextureAndGetHardwarePtr("effpar03")));
    auto effpar03 = assets->GetBitmap("effpar03");
    ErrD3D(pRenderD3D->pDevice->SetTexture(
        0, ((TextureD3D *)effpar03)->GetDirect3DTexture()));

    ErrD3D(pRenderD3D->pDevice->SetRenderState(D3DRENDERSTATE_SRCBLEND, D3DBLEND_ONE));
    ErrD3D(pRenderD3D->pDevice->SetRenderState(D3DRENDERSTATE_DESTBLEND, D3DBLEND_ONE));
}

void Render::EndLightmaps2() {
    ErrD3D(pRenderD3D->pDevice->SetRenderState(D3DRENDERSTATE_SRCBLEND, D3DBLEND_ONE));
    ErrD3D(pRenderD3D->pDevice->SetRenderState(D3DRENDERSTATE_DESTBLEND, D3DBLEND_ZERO));
    ErrD3D(pRenderD3D->pDevice->SetRenderState(D3DRENDERSTATE_ALPHABLENDENABLE, FALSE));
    ErrD3D(pRenderD3D->pDevice->SetRenderState(D3DRENDERSTATE_DITHERENABLE, TRUE));
    ErrD3D(pRenderD3D->pDevice->SetRenderState(D3DRENDERSTATE_ZWRITEENABLE, TRUE));
    ErrD3D(pRenderD3D->pDevice->SetRenderState(D3DRENDERSTATE_CULLMODE, D3DCULL_CW));

    if (config->is_using_specular)
        ErrD3D(pRenderD3D->pDevice->SetRenderState(D3DRENDERSTATE_FOGENABLE, TRUE));
}

void Render::do_draw_debug_line_d3d(const RenderVertexD3D3 *pLineBegin,
                                    signed int sDiffuseBegin,
                                    const RenderVertexD3D3 *pLineEnd,
                                    signed int sDiffuseEnd, float z_stuff) {
    RenderVertexD3D3 vertices[2];  // [sp+8h] [bp-40h]@2

    memcpy(&vertices[0], pLineBegin, sizeof(vertices[0]));
    memcpy(&vertices[1], pLineEnd, sizeof(vertices[1]));

    vertices[0].pos.z = 0.001 - z_stuff;
    vertices[1].pos.z = 0.001 - z_stuff;

    vertices[0].diffuse = sDiffuseBegin;
    vertices[1].diffuse = sDiffuseEnd;

    ErrD3D(pRenderD3D->pDevice->SetTexture(0, nullptr));
    ErrD3D(pRenderD3D->pDevice->DrawPrimitive(D3DPT_LINELIST, 452, vertices, 2,
                                              D3DDP_DONOTLIGHT));
}

void Render::DrawLines(const RenderVertexD3D3 *vertices,
                       unsigned int num_vertices) {
    ErrD3D(pRenderD3D->pDevice->SetTexture(0, nullptr));
    ErrD3D(pRenderD3D->pDevice->DrawPrimitive(
        D3DPT_LINELIST,
        D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_SPECULAR | D3DFVF_TEX1,
        (void *)vertices, num_vertices, D3DDP_DONOTLIGHT));
}

void Render::DrawFansTransparent(const RenderVertexD3D3 *vertices,
                                 unsigned int num_vertices) {
    // ErrD3D(render->pRenderD3D->pDevice->SetRenderState(D3DRENDERSTATE_ZWRITEENABLE,
    // false));
    // ErrD3D(render->pRenderD3D->pDevice->SetRenderState(D3DRENDERSTATE_ZENABLE,
    // false));
    ErrD3D(pRenderD3D->pDevice->SetRenderState(D3DRENDERSTATE_ALPHABLENDENABLE,
                                               TRUE));
    ErrD3D(pRenderD3D->pDevice->SetRenderState(D3DRENDERSTATE_SRCBLEND,
                                               D3DBLEND_SRCALPHA));
    ErrD3D(pRenderD3D->pDevice->SetRenderState(D3DRENDERSTATE_DESTBLEND,
                                               D3DBLEND_INVSRCALPHA));

    ErrD3D(pRenderD3D->pDevice->SetTexture(0, nullptr));
    ErrD3D(pRenderD3D->pDevice->DrawPrimitive(
        D3DPT_TRIANGLEFAN,
        D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_SPECULAR | D3DFVF_TEX1,
        (void *)vertices, num_vertices, 28));

    ErrD3D(pRenderD3D->pDevice->SetRenderState(D3DRENDERSTATE_SRCBLEND,
                                               D3DBLEND_ONE));
    ErrD3D(pRenderD3D->pDevice->SetRenderState(D3DRENDERSTATE_DESTBLEND,
                                               D3DBLEND_ZERO));
    ErrD3D(pRenderD3D->pDevice->SetRenderState(D3DRENDERSTATE_ALPHABLENDENABLE,
                                               FALSE));
    // ErrD3D(render->pRenderD3D->pDevice->SetRenderState(D3DRENDERSTATE_ZENABLE,
    // TRUE));
    // ErrD3D(render->pRenderD3D->pDevice->SetRenderState(D3DRENDERSTATE_ZWRITEENABLE,
    // TRUE));
}

void Render::BeginDecals() {
    // code chunk from 0049C304
    if (config->is_using_specular)
        ErrD3D(pRenderD3D->pDevice->SetRenderState(D3DRENDERSTATE_FOGENABLE, FALSE));
    ErrD3D(pRenderD3D->pDevice->SetTextureStageState(0, D3DTSS_ADDRESS, D3DTADDRESS_CLAMP));

    ErrD3D(pRenderD3D->pDevice->SetRenderState(D3DRENDERSTATE_ALPHABLENDENABLE, TRUE));
    ErrD3D(pRenderD3D->pDevice->SetRenderState(D3DRENDERSTATE_ZWRITEENABLE, FALSE));
    ErrD3D(pRenderD3D->pDevice->SetRenderState(D3DRENDERSTATE_CULLMODE, D3DCULL_NONE));
    ErrD3D(pRenderD3D->pDevice->SetRenderState(D3DRENDERSTATE_SRCBLEND, D3DBLEND_ONE));
    ErrD3D(pRenderD3D->pDevice->SetRenderState(D3DRENDERSTATE_DESTBLEND, D3DBLEND_ONE));
    ErrD3D(pRenderD3D->pDevice->SetRenderState(D3DRENDERSTATE_DITHERENABLE, FALSE));

    // ErrD3D(pRenderD3D->pDevice->SetTexture(0,
    // pIndoorCameraD3D->LoadTextureAndGetHardwarePtr("hwsplat04")));
    auto hwsplat04 = assets->GetBitmap("hwsplat04");
    ErrD3D(pRenderD3D->pDevice->SetTexture(0, ((TextureD3D *)hwsplat04)->GetDirect3DTexture()));
}

void Render::EndDecals() {
    // code chunk from 0049C304
    ErrD3D(pRenderD3D->pDevice->SetRenderState(D3DRENDERSTATE_CULLMODE, D3DCULL_CW));
    ErrD3D(
        pRenderD3D->pDevice->SetRenderState(D3DRENDERSTATE_ZWRITEENABLE, TRUE));
    ErrD3D(pRenderD3D->pDevice->SetRenderState(D3DRENDERSTATE_ALPHABLENDENABLE, FALSE));
    ErrD3D(pRenderD3D->pDevice->SetRenderState(D3DRENDERSTATE_SRCBLEND, D3DBLEND_ONE));
    ErrD3D(pRenderD3D->pDevice->SetRenderState(D3DRENDERSTATE_DESTBLEND, D3DBLEND_ZERO));

    if (config->is_using_specular)
        ErrD3D(pRenderD3D->pDevice->SetRenderState(D3DRENDERSTATE_FOGENABLE, TRUE));
}

void Render::DrawDecal(Decal *pDecal, float z_bias) {
    int dwFlags;                        // [sp+Ch] [bp-864h]@15
    RenderVertexD3D3 pVerticesD3D[64];  // [sp+20h] [bp-850h]@6

    if (pDecal->uNumVertices < 3) {
        log->Warning(L"Decal has < 3 vertices");
        return;
    }

    float color_mult;
    if (pDecal->field_C1C & 1)
        color_mult = 1.0;
    else
        color_mult = pDecal->field_C18->_43B570_get_color_mult_by_time();

    // temp - bloodsplat persistance
    color_mult = 1;

    for (uint i = 0; i < (unsigned int)pDecal->uNumVertices; ++i) {
        uint uTint =
            Render::GetActorTintColor(pDecal->field_C14, 0, pDecal->pVertices[i].vWorldViewPosition.x, 0, nullptr);

        uint uTintR = (uTint >> 16) & 0xFF, uTintG = (uTint >> 8) & 0xFF,
             uTintB = uTint & 0xFF;

        uint uDecalColorMultR = (pDecal->uColorMultiplier >> 16) & 0xFF,
             uDecalColorMultG = (pDecal->uColorMultiplier >> 8) & 0xFF,
             uDecalColorMultB = pDecal->uColorMultiplier & 0xFF;

        uint uFinalR =
                 floorf(uTintR / 255.0 * color_mult * uDecalColorMultR + 0.0f),
             uFinalG =
                 floorf(uTintG / 255.0 * color_mult * uDecalColorMultG + 0.0f),
             uFinalB =
                 floorf(uTintB / 255.0 * color_mult * uDecalColorMultB + 0.0f);

        // temp - make yellow for easier spotting
        // uFinalR = 255;
        // uFinalG = 255;

        float v15;
        if (fabs(z_bias) < 1e-5) {
            v15 = 1.0 -
                1.0 / ((1.0f / pIndoorCameraD3D->GetFarClip()) *
                    pDecal->pVertices[i].vWorldViewPosition.x * 1000.0);
        } else {
            v15 = 1.0 -
                  1.0 / ((1.0f / pIndoorCameraD3D->GetFarClip()) *
                         pDecal->pVertices[i].vWorldViewPosition.x * 1000.0) -
                  z_bias;
            if (v15 < 0.000099999997) v15 = 0.000099999997;
        }

        pVerticesD3D[i].pos.x = pDecal->pVertices[i].vWorldViewProjX;
        pVerticesD3D[i].pos.y = pDecal->pVertices[i].vWorldViewProjY;
        pVerticesD3D[i].pos.z = v15;

        pVerticesD3D[i].rhw = 1.0 / pDecal->pVertices[i].vWorldViewPosition.x;
        pVerticesD3D[i].diffuse = (uFinalR << 16) | (uFinalG << 8) | uFinalB;
        pVerticesD3D[i].specular = 0;

        pVerticesD3D[i].texcoord.x = pDecal->pVertices[i].u;
        pVerticesD3D[i].texcoord.y = pDecal->pVertices[i].v;
    }

    if (uCurrentlyLoadedLevelType == LEVEL_Indoor)
        dwFlags = D3DDP_DONOTLIGHT | D3DDP_DONOTCLIP | D3DDP_DONOTUPDATEEXTENTS;
    else
        dwFlags = D3DDP_DONOTLIGHT;

    ErrD3D(pRenderD3D->pDevice->DrawPrimitive(
        D3DPT_TRIANGLEFAN,
        D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_SPECULAR | D3DFVF_TEX1,
        pVerticesD3D, pDecal->uNumVertices, dwFlags));
}

void Render::DrawSpecialEffectsQuad(const RenderVertexD3D3 *vertices,
                                    Texture *texture) {
    auto gapi_texture = ((TextureD3D *)texture)->GetDirect3DTexture();
    ErrD3D(
        pRenderD3D->pDevice->SetTexture(0, (IDirect3DTexture2 *)gapi_texture));
    ErrD3D(
        pRenderD3D->pDevice->SetRenderState(D3DRENDERSTATE_FOGENABLE, FALSE));
    ErrD3D(pRenderD3D->pDevice->SetRenderState(D3DRENDERSTATE_ALPHABLENDENABLE,
                                               TRUE));
    ErrD3D(pRenderD3D->pDevice->SetRenderState(D3DRENDERSTATE_ZWRITEENABLE,
                                               FALSE));
    ErrD3D(pRenderD3D->pDevice->SetRenderState(D3DRENDERSTATE_SRCBLEND,
                                               D3DBLEND_ONE));
    ErrD3D(pRenderD3D->pDevice->SetRenderState(D3DRENDERSTATE_DESTBLEND,
                                               D3DBLEND_ONE));
    ErrD3D(pRenderD3D->pDevice->SetRenderState(D3DRENDERSTATE_DITHERENABLE,
                                               FALSE));
    ErrD3D(pRenderD3D->pDevice->SetRenderState(D3DRENDERSTATE_ZFUNC,
                                               D3DCMP_ALWAYS));
    ErrD3D(pRenderD3D->pDevice->DrawPrimitive(
        D3DPT_TRIANGLEFAN,
        D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_SPECULAR | D3DFVF_TEX1,
        (void *)vertices, 4, 28));
    ErrD3D(pRenderD3D->pDevice->SetRenderState(D3DRENDERSTATE_SRCBLEND,
                                               D3DBLEND_ONE));
    ErrD3D(pRenderD3D->pDevice->SetRenderState(D3DRENDERSTATE_DESTBLEND,
                                               D3DBLEND_ZERO));
    ErrD3D(pRenderD3D->pDevice->SetRenderState(D3DRENDERSTATE_ALPHABLENDENABLE,
                                               FALSE));
    ErrD3D(
        pRenderD3D->pDevice->SetRenderState(D3DRENDERSTATE_ZWRITEENABLE, TRUE));
    ErrD3D(
        pRenderD3D->pDevice->SetRenderState(D3DRENDERSTATE_DITHERENABLE, TRUE));
    ErrD3D(
        pRenderD3D->pDevice->SetRenderState(D3DRENDERSTATE_ZFUNC, D3DCMP_LESS));
}

unsigned int _452442_color_cvt(unsigned __int16 a1, unsigned __int16 a2, int a3,
                               int a4) {
    int v4;                // ebx@0
    __int16 v5;            // ST14_2@1
    __int16 v6;            // dx@1
    int v7;                // ecx@1
    __int16 v8;            // ST10_2@1
    int v9;                // edi@1
    unsigned __int16 v10 = 0;  // dh@1@1
    int v11;               // ebx@1
    int v12;               // ebx@1
    __int16 a3a;           // [sp+1Ch] [bp+8h]@1

    v5 = a2 >> 2;
    v6 = (unsigned __int16)a4 >> 2;
    v8 = a1 >> 2;
    a3a = (unsigned __int16)a3 >> 2;
    HEXRAYS_LOWORD(v7) = a3a;
    v9 = v7;
    HEXRAYS_LOWORD(v4) = ((unsigned __int16)a4 >> 2) & 0xE0;
    HEXRAYS_LOWORD(v7) = a3a & 0xE0;
    HEXRAYS_LOWORD(v9) = v9 & 0x1C00;
    v11 = v7 + v4;
    HEXRAYS_LOWORD(v7) = v5 & 0xE0;
    v12 = v7 + v11;
    HEXRAYS_LOWORD(v7) = v8 & 0xE0;
    __debugbreak();  // warning C4700: uninitialized local variable 'v10' used
    return (PID_TYPE(v8) + PID_TYPE(v5) + PID_TYPE(a3a) + PID_TYPE(v6)) |
           (v7 + v12) |
           ((v8 & 0x1C00) + (v5 & 0x1C00) + v9 +
            (__PAIR__(v10, (unsigned __int16)a4 >> 2) & 0x1C00));
}

int GetActorTintColor(int max_dimm, int min_dimm, float distance, int a4, RenderBillboard *a5) {
    signed int v6;   // edx@1
    int v8;          // eax@3
    double v9;       // st7@12
    int v11;         // ecx@28
    double v15;      // st7@44
    int v18;         // ST14_4@44
    signed int v20;  // [sp+10h] [bp-4h]@10
    float a3c;       // [sp+1Ch] [bp+8h]@44
    int a5a;         // [sp+24h] [bp+10h]@44

    // v5 = a2;
    v6 = 0;

    if (uCurrentlyLoadedLevelType == LEVEL_Indoor)
        return 8 * (31 - max_dimm) | ((8 * (31 - max_dimm) | ((31 - max_dimm) << 11)) << 8);

    if (pParty->armageddon_timer) return 0xFFFF0000;

    v8 = pWeather->bNight;
    if (engine->IsUnderwater())
        v8 = 0;
    if (v8) {
        v20 = 1;
        if (pParty->pPartyBuffs[PARTY_BUFF_TORCHLIGHT].Active())
            v20 = pParty->pPartyBuffs[PARTY_BUFF_TORCHLIGHT].uPower;
        v9 = (double)v20 * 1024.0;
        if (a4) {
            v6 = 216;
            goto LABEL_20;
        }
        if (distance <= v9) {
            if (distance > 0.0) {
                // a4b = distance * 216.0 / device_caps;
                // v10 = a4b + 6.7553994e15;
                // v6 = LODWORD(v10);
                v6 = floorf(0.5f + distance * 216.0 / v9);
                if (v6 > 216) {
                    v6 = 216;
                    goto LABEL_20;
                }
            }
        } else {
            v6 = 216;
        }
        if (distance != 0.0) {
        LABEL_20:
            if (a5) v6 = 8 * _43F55F_get_billboard_light_level(a5, v6 >> 3);
            if (v6 > 216) v6 = 216;
            return (255 - v6) | ((255 - v6) << 16) | ((255 - v6) << 8);
        }
        // LABEL_19:
        v6 = 216;
        goto LABEL_20;
    }

    if (fabsf(distance) < 1.0e-6f) return 0xFFF8F8F8;

    // dim in measured in 8-steps
    v11 = 8 * (max_dimm - min_dimm);
    // v12 = v11;
    if (v11 >= 0) {
        if (v11 > 216) v11 = 216;
    } else {
        v11 = 0;
    }

    float fog_density_mult = 216.0f;
    if (a4)
        fog_density_mult +=
            distance / (double)pODMRenderParams->shading_dist_shade * 32.0;

    v6 = v11 + floorf(pOutdoor->fFogDensity * fog_density_mult + 0.5f);

    if (a5) v6 = 8 * _43F55F_get_billboard_light_level(a5, v6 >> 3);
    if (v6 > 216) v6 = 216;
    if (v6 < v11) v6 = v11;
    if (v6 > 8 * pOutdoor->max_terrain_dimming_level)
        v6 = 8 * pOutdoor->max_terrain_dimming_level;
    if (!engine->IsUnderwater()) {
        return (255 - v6) | ((255 - v6) << 16) | ((255 - v6) << 8);
    } else {
        v15 = (double)(255 - v6) * 0.0039215689;
        a3c = v15;
        // a4c = v15 * 16.0;
        // v16 = a4c + 6.7553994e15;
        a5a = floorf(v15 * 16.0 + 0.5f);  // LODWORD(v16);
                                          // a4d = a3c * 194.0;
                                          // v17 = a4d + 6.7553994e15;
        v18 = floorf(a3c * 194.0 + 0.5f);  // LODWORD(v17);
                                           // a3d = a3c * 153.0;
                                           // v19 = a3d + 6.7553994e15;
        return (int)floorf(a3c * 153.0 + 0.5f) /*LODWORD(v19)*/ |
               ((v18 | (a5a << 8)) << 8);
    }
}

int _43F55F_get_billboard_light_level(RenderBillboard *a1,
                                      int uBaseLightLevel) {
    int v3 = 0;

    if (uCurrentlyLoadedLevelType == LEVEL_Indoor) {
        v3 = pIndoor->pSectors[a1->uIndoorSectorID].uMinAmbientLightLevel;
    } else {
        if (uBaseLightLevel == -1) {
            v3 = a1->dimming_level;
        } else {
            v3 = uBaseLightLevel;
        }
    }

    return _43F5C8_get_point_light_level_with_respect_to_lights(
        v3, a1->uIndoorSectorID, a1->world_x, a1->world_y, a1->world_z);
}

int _43F5C8_get_point_light_level_with_respect_to_lights(
    unsigned int uBaseLightLevel, int uSectorID, float x, float y, float z) {
    signed int v6;     // edi@1
    int v8;            // eax@6
    int v9;            // ebx@6
    unsigned int v10;  // ecx@6
    unsigned int v11;  // edx@9
    unsigned int v12;  // edx@11
    signed int v13;    // ecx@12
    BLVLightMM7 *v16;  // esi@20
    int v17;           // ebx@21
    signed int v24;    // ecx@30
    int v26;           // ebx@35
    int v37;           // [sp+Ch] [bp-18h]@37
    int v39;           // [sp+10h] [bp-14h]@23
    int v40;           // [sp+10h] [bp-14h]@36
    int v42;           // [sp+14h] [bp-10h]@22
    unsigned int v43;  // [sp+18h] [bp-Ch]@12
    unsigned int v44;  // [sp+18h] [bp-Ch]@30
    unsigned int v45;  // [sp+18h] [bp-Ch]@44

    v6 = uBaseLightLevel;
    for (uint i = 0; i < pMobileLightsStack->uNumLightsActive; ++i) {
        MobileLight *p = &pMobileLightsStack->pLights[i];

        float distX = abs(p->vPosition.x - x);
        if (distX <= p->uRadius) {
            float distY = abs(p->vPosition.y - y);
            if (distY <= p->uRadius) {
                float distZ = abs(p->vPosition.z - z);
                if (distZ <= p->uRadius) {
                    v8 = distX;
                    v9 = distY;
                    v10 = distZ;
                    if (distX < distY) {
                        v8 = distY;
                        v9 = distX;
                    }
                    if (v8 < distZ) {
                        v11 = v8;
                        v8 = distZ;
                        v10 = v11;
                    }
                    if (v9 < (signed int)v10) {
                        v12 = v10;
                        v10 = v9;
                        v9 = v12;
                    }
                    v43 = ((unsigned int)(11 * v9) / 32) + (v10 / 4) + v8;
                    v13 = p->uRadius;
                    if ((signed int)v43 < v13)
                        v6 += ((unsigned __int64)(30i64 *
                                                  (signed int)(v43 << 16) /
                                                  v13) >>
                               16) -
                              30;
                }
            }
        }
    }

    if (uCurrentlyLoadedLevelType == LEVEL_Indoor) {
        BLVSector *pSector = &pIndoor->pSectors[uSectorID];

        for (uint i = 0; i < pSector->uNumLights; ++i) {
            v16 = pIndoor->pLights + pSector->pLights[i];
            if (~v16->uAtributes & 8) {
                v17 = abs(v16->vPosition.x - x);
                if (v17 <= v16->uRadius) {
                    v42 = abs(v16->vPosition.y - y);
                    if (v42 <= v16->uRadius) {
                        v39 = abs(v16->vPosition.z - z);
                        if (v39 <= v16->uRadius) {
                            v44 = int_get_vector_length(v17, v42, v39);
                            v24 = v16->uRadius;
                            if ((signed int)v44 < v24)
                                v6 += ((unsigned __int64)(30i64 *
                                                          (signed int)(v44
                                                                       << 16) /
                                                          v24) >>
                                       16) -
                                      30;
                        }
                    }
                }
            }
        }
    }

    for (uint i = 0; i < pStationaryLightsStack->uNumLightsActive; ++i) {
        // StationaryLight* p = &pStationaryLightsStack->pLights[i];
        v26 = abs(pStationaryLightsStack->pLights[i].vPosition.x - x);
        if (v26 <= pStationaryLightsStack->pLights[i].uRadius) {
            v40 = abs(pStationaryLightsStack->pLights[i].vPosition.y - y);
            if (v40 <= pStationaryLightsStack->pLights[i].uRadius) {
                v37 = abs(pStationaryLightsStack->pLights[i].vPosition.z - z);
                if (v37 <= pStationaryLightsStack->pLights[i].uRadius) {
                    v45 = int_get_vector_length(v26, v40, v37);
                    // v33 = pStationaryLightsStack->pLights[i].uRadius;
                    if ((signed int)v45 <
                        pStationaryLightsStack->pLights[i].uRadius)
                        v6 += ((unsigned __int64)(30i64 *
                                                  (signed int)(v45 << 16) /
                                                  pStationaryLightsStack
                                                      ->pLights[i]
                                                      .uRadius) >>
                               16) -
                              30;
                }
            }
        }
    }

    if (v6 <= 31) {
        if (v6 < 0) v6 = 0;
    } else {
        v6 = 31;
    }
    return v6;
}

int _46E44E_collide_against_faces_and_portals(unsigned int b1) {
    BLVSector *pSector;   // edi@1
    int v2;        // ebx@1
    BLVFace *pFace;       // esi@2
    __int16 pNextSector;  // si@10
    int pArrayNum;        // ecx@12
    unsigned __int8 v6;   // sf@12
    unsigned __int8 v7;   // of@12
    int result;           // eax@14
    // int v10; // ecx@15
    int pFloor;             // eax@16
    int v15;                // eax@24
    int v16;                // edx@25
    int v17;                // eax@29
    unsigned int v18;       // eax@33
    int v21;                // eax@35
    int v22;                // ecx@36
    int v23;                // eax@40
    unsigned int v24;       // eax@44
    int a3;                 // [sp+10h] [bp-48h]@28
    int v26;                // [sp+14h] [bp-44h]@15
    int i;                  // [sp+18h] [bp-40h]@1
    int a10;                // [sp+1Ch] [bp-3Ch]@1
    int v29;                // [sp+20h] [bp-38h]@14
    int v32;                // [sp+2Ch] [bp-2Ch]@15
    int pSectorsArray[10];  // [sp+30h] [bp-28h]@1

    pSector = &pIndoor->pSectors[stru_721530.uSectorID];
    i = 1;
    a10 = b1;
    pSectorsArray[0] = stru_721530.uSectorID;
    for (v2 = 0; v2 < pSector->uNumPortals; ++v2) {
        pFace = &pIndoor->pFaces[pSector->pPortals[v2]];
        if (stru_721530.sMaxX <= pFace->pBounding.x2 &&
            stru_721530.sMinX >= pFace->pBounding.x1 &&
            stru_721530.sMaxY <= pFace->pBounding.y2 &&
            stru_721530.sMinY >= pFace->pBounding.y1 &&
            stru_721530.sMaxZ <= pFace->pBounding.z2 &&
            stru_721530.sMinZ >= pFace->pBounding.z1 &&
            abs((pFace->pFacePlane_old.dist +
                 stru_721530.normal.x * pFace->pFacePlane_old.vNormal.x +
                 stru_721530.normal.y * pFace->pFacePlane_old.vNormal.y +
                 stru_721530.normal.z * pFace->pFacePlane_old.vNormal.z) >>
                16) <= stru_721530.field_6C + 16) {
            pNextSector = pFace->uSectorID == stru_721530.uSectorID
                              ? pFace->uBackSectorID
                              : pFace->uSectorID;  // FrontSectorID
            pArrayNum = i++;
            v7 = i < 10;
            v6 = i - 10 < 0;
            pSectorsArray[pArrayNum] = pNextSector;
            if (!(v6 ^ v7)) break;
        }
    }
    result = 0;
    for (v29 = 0; v29 < i; v29++) {
        pSector = &pIndoor->pSectors[pSectorsArray[v29]];
        v32 = pSector->uNumFloors + pSector->uNumWalls + pSector->uNumCeilings;
        for (v26 = 0; v26 < v32; v26++) {
            pFloor = pSector->pFloors[v26];
            pFace = &pIndoor->pFaces[pSector->pFloors[v26]];
            if (!pFace->Portal() && stru_721530.sMaxX <= pFace->pBounding.x2 &&
                stru_721530.sMinX >= pFace->pBounding.x1 &&
                stru_721530.sMaxY <= pFace->pBounding.y2 &&
                stru_721530.sMinY >= pFace->pBounding.y1 &&
                stru_721530.sMaxZ <= pFace->pBounding.z2 &&
                stru_721530.sMinZ >= pFace->pBounding.z1 &&
                pFloor != stru_721530.field_84) {
                v15 =
                    (pFace->pFacePlane_old.dist +
                     stru_721530.normal.x * pFace->pFacePlane_old.vNormal.x +
                     stru_721530.normal.y * pFace->pFacePlane_old.vNormal.y +
                     stru_721530.normal.z * pFace->pFacePlane_old.vNormal.z) >>
                    16;
                if (v15 > 0) {
                    v16 = (pFace->pFacePlane_old.dist +
                           stru_721530.normal2.x *
                               pFace->pFacePlane_old.vNormal.x +
                           stru_721530.normal2.y *
                               pFace->pFacePlane_old.vNormal.y +
                           stru_721530.normal2.z *
                               pFace->pFacePlane_old.vNormal.z) >>
                          16;
                    if (v15 <= stru_721530.prolly_normal_d ||
                        v16 <= stru_721530.prolly_normal_d) {
                        if (v16 <= v15) {
                            a3 = stru_721530.field_6C;
                            if (sub_47531C(
                                    stru_721530.prolly_normal_d, &a3,
                                    stru_721530.normal.x, stru_721530.normal.y,
                                    stru_721530.normal.z,
                                    stru_721530.direction.x,
                                    stru_721530.direction.y,
                                    stru_721530.direction.z, pFace, a10)) {
                                v17 = a3;
                            } else {
                                a3 = stru_721530.field_6C +
                                     stru_721530.prolly_normal_d;
                                if (!sub_475D85(&stru_721530.normal,
                                                &stru_721530.direction, &a3,
                                                pFace))
                                    goto LABEL_34;
                                v17 = a3 - stru_721530.prolly_normal_d;
                                a3 -= stru_721530.prolly_normal_d;
                            }
                            if (v17 < stru_721530.field_7C) {
                                stru_721530.field_7C = v17;
                                v18 = 8 * pSector->pFloors[v26];
                                v18 |= 6;
                                stru_721530.pid = v18;
                            }
                        }
                    }
                }
            LABEL_34:
                if (!(stru_721530.field_0 & 1) ||
                    (v21 = (pFace->pFacePlane_old.dist +
                            stru_721530.position.x *
                                pFace->pFacePlane_old.vNormal.x +
                            stru_721530.position.y *
                                pFace->pFacePlane_old.vNormal.y +
                            stru_721530.position.z *
                                pFace->pFacePlane_old.vNormal.z) >>
                           16,
                     v21 <= 0) ||
                    (v22 = (pFace->pFacePlane_old.dist +
                            stru_721530.field_4C *
                                pFace->pFacePlane_old.vNormal.x +
                            stru_721530.field_50 *
                                pFace->pFacePlane_old.vNormal.y +
                            stru_721530.field_54 *
                                pFace->pFacePlane_old.vNormal.z) >>
                           16,
                     v21 > stru_721530.prolly_normal_d) &&
                        v22 > stru_721530.prolly_normal_d ||
                    v22 > v21)
                    continue;
                a3 = stru_721530.field_6C;
                if (sub_47531C(stru_721530.field_8_radius, &a3,
                               stru_721530.position.x, stru_721530.position.y,
                               stru_721530.position.z, stru_721530.direction.x,
                               stru_721530.direction.y, stru_721530.direction.z,
                               pFace, a10)) {
                    v23 = a3;
                    goto LABEL_43;
                }
                a3 = stru_721530.field_6C + stru_721530.field_8_radius;
                if (sub_475D85(&stru_721530.position, &stru_721530.direction,
                               &a3, pFace)) {
                    v23 = a3 - stru_721530.prolly_normal_d;
                    a3 -= stru_721530.prolly_normal_d;
                LABEL_43:
                    if (v23 < stru_721530.field_7C) {
                        stru_721530.field_7C = v23;
                        v24 = 8 * pSector->pFloors[v26];
                        v24 |= 6;
                        stru_721530.pid = v24;
                    }
                }
            }
        }
        result = v29 + 1;
    }
    return result;
}

void _46E889_collide_against_bmodels(unsigned int ecx0) {
    int v8;            // eax@19
    int v9;            // ecx@20
    int v10;           // eax@24
    // unsigned int v14;  // eax@28
    int v15;           // eax@30
    int v16;           // ecx@31
    // unsigned int v17;  // eax@36
    int v21;           // eax@42
    // unsigned int v22;  // eax@43
    int a2;            // [sp+84h] [bp-4h]@23
    BLVFace face;      // [sp+Ch] [bp-7Ch]@1

    for (BSPModel &model : pOutdoor->pBModels) {
        if (stru_721530.sMaxX <= model.sMaxX &&
            stru_721530.sMinX >= model.sMinX &&
            stru_721530.sMaxY <= model.sMaxY &&
            stru_721530.sMinY >= model.sMinY &&
            stru_721530.sMaxZ <= model.sMaxZ &&
            stru_721530.sMinZ >= model.sMinZ) {
            for (ODMFace &mface : model.pFaces) {
                if (stru_721530.sMaxX <= mface.pBoundingBox.x2 &&
                    stru_721530.sMinX >= mface.pBoundingBox.x1 &&
                    stru_721530.sMaxY <= mface.pBoundingBox.y2 &&
                    stru_721530.sMinY >= mface.pBoundingBox.y1 &&
                    stru_721530.sMaxZ <= mface.pBoundingBox.z2 &&
                    stru_721530.sMinZ >= mface.pBoundingBox.z1) {
                    face.pFacePlane_old.vNormal.x = mface.pFacePlane.vNormal.x;
                    face.pFacePlane_old.vNormal.y = mface.pFacePlane.vNormal.y;
                    face.pFacePlane_old.vNormal.z = mface.pFacePlane.vNormal.z;

                    face.pFacePlane_old.dist =
                        mface.pFacePlane.dist;  // incorrect

                    face.uAttributes = mface.uAttributes;

                    face.pBounding.x1 = mface.pBoundingBox.x1;
                    face.pBounding.y1 = mface.pBoundingBox.y1;
                    face.pBounding.z1 = mface.pBoundingBox.z1;

                    face.pBounding.x2 = mface.pBoundingBox.x2;
                    face.pBounding.y2 = mface.pBoundingBox.y2;
                    face.pBounding.z2 = mface.pBoundingBox.z2;

                    face.zCalc1 = mface.zCalc1;
                    face.zCalc2 = mface.zCalc2;
                    face.zCalc3 = mface.zCalc3;

                    face.pXInterceptDisplacements =
                        mface.pXInterceptDisplacements;
                    face.pYInterceptDisplacements =
                        mface.pYInterceptDisplacements;
                    face.pZInterceptDisplacements =
                        mface.pZInterceptDisplacements;

                    face.uPolygonType = (PolygonType)mface.uPolygonType;

                    face.uNumVertices = mface.uNumVertices;

                    // face.uBitmapID = model.pFaces[j].uTextureID;
                    face.resource = mface.resource;

                    face.pVertexIDs = mface.pVertexIDs;

                    if (!face.Ethereal() && !face.Portal()) {
                        v8 = (face.pFacePlane_old.dist +
                              face.pFacePlane_old.vNormal.x *
                                  stru_721530.normal.x +
                              face.pFacePlane_old.vNormal.y *
                                  stru_721530.normal.y +
                              face.pFacePlane_old.vNormal.z *
                                  stru_721530.normal.z) >>
                             16;
                        if (v8 > 0) {
                            v9 = (face.pFacePlane_old.dist +
                                  face.pFacePlane_old.vNormal.x *
                                      stru_721530.normal2.x +
                                  face.pFacePlane_old.vNormal.y *
                                      stru_721530.normal2.y +
                                  face.pFacePlane_old.vNormal.z *
                                      stru_721530.normal2.z) >>
                                 16;
                            if (v8 <= stru_721530.prolly_normal_d ||
                                v9 <= stru_721530.prolly_normal_d) {
                                if (v9 <= v8) {
                                    a2 = stru_721530.field_6C;
                                    if (sub_4754BF(stru_721530.prolly_normal_d,
                                                   &a2, stru_721530.normal.x,
                                                   stru_721530.normal.y,
                                                   stru_721530.normal.z,
                                                   stru_721530.direction.x,
                                                   stru_721530.direction.y,
                                                   stru_721530.direction.z,
                                                   &face, model.index, ecx0)) {
                                        v10 = a2;
                                    } else {
                                        a2 = stru_721530.prolly_normal_d +
                                             stru_721530.field_6C;
                                        if (!sub_475F30(&a2, &face,
                                                        stru_721530.normal.x,
                                                        stru_721530.normal.y,
                                                        stru_721530.normal.z,
                                                        stru_721530.direction.x,
                                                        stru_721530.direction.y,
                                                        stru_721530.direction.z,
                                                        model.index))
                                            goto LABEL_29;
                                        v10 = a2 - stru_721530.prolly_normal_d;
                                        a2 -= stru_721530.prolly_normal_d;
                                    }
                                    if (v10 < stru_721530.field_7C) {
                                        stru_721530.field_7C = v10;
                                        stru_721530.pid = PID(
                                            OBJECT_BModel,
                                            (mface.index | (model.index << 6)));
                                    }
                                }
                            }
                        }
                    LABEL_29:
                        if (stru_721530.field_0 & 1) {
                            v15 = (face.pFacePlane_old.dist +
                                   face.pFacePlane_old.vNormal.x *
                                       stru_721530.position.x +
                                   face.pFacePlane_old.vNormal.y *
                                       stru_721530.position.y +
                                   face.pFacePlane_old.vNormal.z *
                                       stru_721530.position.z) >>
                                  16;
                            if (v15 > 0) {
                                v16 = (face.pFacePlane_old.dist +
                                       face.pFacePlane_old.vNormal.x *
                                           stru_721530.field_4C +
                                       face.pFacePlane_old.vNormal.y *
                                           stru_721530.field_50 +
                                       face.pFacePlane_old.vNormal.z *
                                           stru_721530.field_54) >>
                                      16;
                                if (v15 <= stru_721530.prolly_normal_d ||
                                    v16 <= stru_721530.prolly_normal_d) {
                                    if (v16 <= v15) {
                                        a2 = stru_721530.field_6C;
                                        if (sub_4754BF(
                                                stru_721530.field_8_radius, &a2,
                                                stru_721530.position.x,
                                                stru_721530.position.y,
                                                stru_721530.position.z,
                                                stru_721530.direction.x,
                                                stru_721530.direction.y,
                                                stru_721530.direction.z, &face,
                                                model.index, ecx0)) {
                                            if (a2 < stru_721530.field_7C) {
                                                stru_721530.field_7C = a2;
                                                stru_721530.pid =
                                                    PID(OBJECT_BModel,
                                                        (mface.index |
                                                         (model.index << 6)));
                                            }
                                        } else {
                                            a2 = stru_721530.field_6C +
                                                 stru_721530.field_8_radius;
                                            if (sub_475F30(
                                                    &a2, &face,
                                                    stru_721530.position.x,
                                                    stru_721530.position.y,
                                                    stru_721530.position.z,
                                                    stru_721530.direction.x,
                                                    stru_721530.direction.y,
                                                    stru_721530.direction.z,
                                                    model.index)) {
                                                v21 =
                                                    a2 -
                                                    stru_721530.prolly_normal_d;
                                                a2 -=
                                                    stru_721530.prolly_normal_d;
                                                if (a2 < stru_721530.field_7C) {
                                                    stru_721530.field_7C = v21;
                                                    stru_721530.pid = PID(
                                                        OBJECT_BModel,
                                                        (mface.index |
                                                         (model.index << 6)));
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}

int collide_against_floor(int x, int y, int z, unsigned int *pSectorID,
                          unsigned int *pFaceID) {
    uint uFaceID = -1;
    int floor_level = BLV_GetFloorLevel(x, y, z, *pSectorID, &uFaceID);

    if (floor_level != -30000 && floor_level <= z + 50) {
        *pFaceID = uFaceID;
        return floor_level;
    }

    uint uSectorID = pIndoor->GetSector(x, y, z);
    *pSectorID = uSectorID;

    floor_level = BLV_GetFloorLevel(x, y, z, uSectorID, &uFaceID);
    if (uSectorID && floor_level != -30000)
        *pFaceID = uFaceID;
    else
        return -30000;
    return floor_level;
}

void _46ED8A_collide_against_sprite_objects(unsigned int _this) {
    ObjectDesc *object;  // edx@4
    int v10;             // ecx@12
    int v11;             // esi@13

    for (uint i = 0; i < uNumSpriteObjects; ++i) {
        if (pSpriteObjects[i].uObjectDescID) {
            object = &pObjectList->pObjects[pSpriteObjects[i].uObjectDescID];
            if (!(object->uFlags & OBJECT_DESC_NO_COLLISION)) {
                if (stru_721530.sMaxX <=
                        pSpriteObjects[i].vPosition.x + object->uRadius &&
                    stru_721530.sMinX >=
                        pSpriteObjects[i].vPosition.x - object->uRadius &&
                    stru_721530.sMaxY <=
                        pSpriteObjects[i].vPosition.y + object->uRadius &&
                    stru_721530.sMinY >=
                        pSpriteObjects[i].vPosition.y - object->uRadius &&
                    stru_721530.sMaxZ <=
                        pSpriteObjects[i].vPosition.z + object->uHeight &&
                    stru_721530.sMinZ >= pSpriteObjects[i].vPosition.z) {
                    if (abs(((pSpriteObjects[i].vPosition.x -
                              stru_721530.normal.x) *
                                 stru_721530.direction.y -
                             (pSpriteObjects[i].vPosition.y -
                              stru_721530.normal.y) *
                                 stru_721530.direction.x) >>
                            16) <=
                        object->uHeight + stru_721530.prolly_normal_d) {
                        v10 = ((pSpriteObjects[i].vPosition.x -
                                stru_721530.normal.x) *
                                   stru_721530.direction.x +
                               (pSpriteObjects[i].vPosition.y -
                                stru_721530.normal.y) *
                                   stru_721530.direction.y) >>
                              16;
                        if (v10 > 0) {
                            v11 = stru_721530.normal.z +
                                  ((unsigned __int64)(stru_721530.direction.z *
                                                      (signed __int64)v10) >>
                                   16);
                            if (v11 >= pSpriteObjects[i].vPosition.z -
                                           stru_721530.prolly_normal_d) {
                                if (v11 <= object->uHeight +
                                               stru_721530.prolly_normal_d +
                                               pSpriteObjects[i].vPosition.z) {
                                    if (v10 < stru_721530.field_7C) {
                                        sub_46DEF2(_this, i);
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}

int _46EF01_collision_chech_player(int a1) {
    int result;  // eax@1
    int v3;      // ebx@7
    int v4;      // esi@7
    int v5;      // edi@8
    int v6;      // ecx@9
    int v7;      // edi@12
    int v10;     // [sp+14h] [bp-8h]@7
    int v11;     // [sp+18h] [bp-4h]@7

    result = pParty->vPosition.x;
    // device_caps = pParty->uPartyHeight;
    if (stru_721530.sMaxX <=
            pParty->vPosition.x + (2 * pParty->field_14_radius) &&
        stru_721530.sMinX >=
            pParty->vPosition.x - (2 * pParty->field_14_radius) &&
        stru_721530.sMaxY <=
            pParty->vPosition.y + (2 * pParty->field_14_radius) &&
        stru_721530.sMinY >=
            pParty->vPosition.y - (2 * pParty->field_14_radius) &&
        stru_721530.sMinZ/*sMaxZ*/ <= (pParty->vPosition.z + (int)pParty->uPartyHeight) &&
        stru_721530.sMaxZ/*sMinZ*/ >= pParty->vPosition.z) {
        v3 = stru_721530.prolly_normal_d + (2 * pParty->field_14_radius);
        v11 = pParty->vPosition.x - stru_721530.normal.x;
        v4 = ((pParty->vPosition.x - stru_721530.normal.x) *
                  stru_721530.direction.y -
              (pParty->vPosition.y - stru_721530.normal.y) *
                  stru_721530.direction.x) >>
             16;
        v10 = pParty->vPosition.y - stru_721530.normal.y;
        result = abs(((pParty->vPosition.x - stru_721530.normal.x) *
                          stru_721530.direction.y -
                      (pParty->vPosition.y - stru_721530.normal.y) *
                          stru_721530.direction.x) >>
                     16);
        if (result <=
            stru_721530.prolly_normal_d + (2 * pParty->field_14_radius)) {
            result = v10 * stru_721530.direction.y;
            v5 = (v10 * stru_721530.direction.y +
                  v11 * stru_721530.direction.x) >>
                 16;
            if (v5 > 0) {
                v6 = fixpoint_mul(stru_721530.direction.z, v5) +
                     stru_721530.normal.z;
                result = pParty->vPosition.z;
                if (v6 >= pParty->vPosition.z) {
                    result = pParty->uPartyHeight + pParty->vPosition.z;
                    if (v6 <= (signed int)(pParty->uPartyHeight +
                                           pParty->vPosition.z) ||
                        a1) {
                        result = integer_sqrt(v3 * v3 - v4 * v4);
                        v7 = v5 - integer_sqrt(v3 * v3 - v4 * v4);
                        if (v7 < 0) v7 = 0;
                        if (v7 < stru_721530.field_7C) {
                            stru_721530.field_7C = v7;
                            stru_721530.pid = 4;
                        }
                    }
                }
            }
        }
    }
    return result;
}

void _46E0B2_collide_against_decorations() {
    BLVSector *sector = &pIndoor->pSectors[stru_721530.uSectorID];
    for (unsigned int i = 0; i < sector->uNumDecorations; ++i) {
        LevelDecoration *decor = &pLevelDecorations[sector->pDecorationIDs[i]];
        if (!(decor->uFlags & LEVEL_DECORATION_INVISIBLE)) {
            DecorationDesc *decor_desc = pDecorationList->GetDecoration(decor->uDecorationDescID);
            if (!decor_desc->CanMoveThrough()) {
                if (stru_721530.sMaxX <= decor->vPosition.x + decor_desc->uRadius &&
                    stru_721530.sMinX >= decor->vPosition.x - decor_desc->uRadius &&
                    stru_721530.sMaxY <= decor->vPosition.y + decor_desc->uRadius &&
                    stru_721530.sMinY >= decor->vPosition.y - decor_desc->uRadius &&
                    stru_721530.sMaxZ <= decor->vPosition.z + decor_desc->uDecorationHeight &&
                    stru_721530.sMinZ >= decor->vPosition.z) {
                    int v16 = decor->vPosition.x - stru_721530.normal.x;
                    int v15 = decor->vPosition.y - stru_721530.normal.y;
                    int v8 = stru_721530.prolly_normal_d + decor_desc->uRadius;
                    int v17 = ((decor->vPosition.x - stru_721530.normal.x) * stru_721530.direction.y -
                               (decor->vPosition.y - stru_721530.normal.y) * stru_721530.direction.x) >> 16;
                    if (abs(v17) <= stru_721530.prolly_normal_d + decor_desc->uRadius) {
                        int v9 = (v16 * stru_721530.direction.x + v15 * stru_721530.direction.y) >> 16;
                        if (v9 > 0) {
                            int v11 = stru_721530.normal.z + fixpoint_mul(stru_721530.direction.z, v9);
                            if (v11 >= decor->vPosition.z) {
                                if (v11 <= decor_desc->uDecorationHeight + decor->vPosition.z) {
                                    int v12 = v9 - integer_sqrt(v8 * v8 - v17 * v17);
                                    if (v12 < 0) v12 = 0;
                                    if (v12 < stru_721530.field_7C) {
                                        stru_721530.field_7C = v12;
                                        stru_721530.pid = PID(OBJECT_Decoration, sector->pDecorationIDs[i]);
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}

int _46F04E_collide_against_portals() {
    int a3;             // [sp+Ch] [bp-8h]@13
    int v12 = 0;            // [sp+10h] [bp-4h]@15

    unsigned int v1 = 0xFFFFFF;
    unsigned int v10 = 0xFFFFFF;
    for (unsigned int i = 0; i < pIndoor->pSectors[stru_721530.uSectorID].uNumPortals; ++i) {
        if (pIndoor->pSectors[stru_721530.uSectorID].pPortals[i] !=
            stru_721530.field_80) {
            BLVFace *face = &pIndoor->pFaces[pIndoor->pSectors[stru_721530.uSectorID].pPortals[i]];
            if (stru_721530.sMaxX <= face->pBounding.x2 &&
                stru_721530.sMinX >= face->pBounding.x1 &&
                stru_721530.sMaxY <= face->pBounding.y2 &&
                stru_721530.sMinY >= face->pBounding.y1 &&
                stru_721530.sMaxZ <= face->pBounding.z2 &&
                stru_721530.sMinZ >= face->pBounding.z1) {
                int v4 = (stru_721530.normal.x * face->pFacePlane_old.vNormal.x +
                          face->pFacePlane_old.dist +
                          stru_721530.normal.y * face->pFacePlane_old.vNormal.y +
                          stru_721530.normal.z * face->pFacePlane_old.vNormal.z) >> 16;
                int v5 = (stru_721530.normal2.z * face->pFacePlane_old.vNormal.z +
                          face->pFacePlane_old.dist +
                          stru_721530.normal2.x * face->pFacePlane_old.vNormal.x +
                          stru_721530.normal2.y * face->pFacePlane_old.vNormal.y) >> 16;
                if ((v4 < stru_721530.prolly_normal_d || v5 < stru_721530.prolly_normal_d) &&
                    (v4 > -stru_721530.prolly_normal_d || v5 > -stru_721530.prolly_normal_d) &&
                    (a3 = stru_721530.field_6C, sub_475D85(&stru_721530.normal, &stru_721530.direction, &a3, face)) && a3 < (int)v10) {
                    v10 = a3;
                    v12 = pIndoor->pSectors[stru_721530.uSectorID].pPortals[i];
                }
            }
        }
    }

    v1 = v10;

    int result = 1;

    if (stru_721530.field_7C >= (int)v1 && (int)v1 <= stru_721530.field_6C) {
        stru_721530.field_80 = v12;
        if (pIndoor->pFaces[v12].uSectorID == stru_721530.uSectorID) {
            stru_721530.uSectorID = pIndoor->pFaces[v12].uBackSectorID;
        } else {
            stru_721530.uSectorID = pIndoor->pFaces[v12].uSectorID;
        }
        stru_721530.field_7C = 268435455;  // 0xFFFFFFF
        result = 0;
    }

    return result;
}

unsigned int sub_46DEF2(signed int a2, unsigned int uLayingItemID) {
    unsigned int result = uLayingItemID;
    if (pObjectList->pObjects[pSpriteObjects[uLayingItemID].uObjectDescID].uFlags & 0x10) {
        result = _46BFFA_update_spell_fx(uLayingItemID, a2);
    }
    return result;
}

void UpdateObjects() {
    int v5;   // ecx@6
    int v7;   // eax@9
    int v11;  // eax@17
    int v12;  // edi@27
    int v18;  // [sp+4h] [bp-10h]@27
    int v19;  // [sp+8h] [bp-Ch]@27

    for (uint i = 0; i < uNumSpriteObjects; ++i) {
        if (pSpriteObjects[i].uAttributes & OBJECT_40) {
            pSpriteObjects[i].uAttributes &= ~OBJECT_40;
        } else {
            ObjectDesc *object =
                &pObjectList->pObjects[pSpriteObjects[i].uObjectDescID];
            if (pSpriteObjects[i].AttachedToActor()) {
                v5 = PID_ID(pSpriteObjects[i].spell_target_pid);
                pSpriteObjects[i].vPosition.x = pActors[v5].vPosition.x;
                pSpriteObjects[i].vPosition.y = pActors[v5].vPosition.y;
                pSpriteObjects[i].vPosition.z =
                    pActors[v5].vPosition.z + pActors[v5].uActorHeight;
                if (!pSpriteObjects[i].uObjectDescID) continue;
                pSpriteObjects[i].uSpriteFrameID += pEventTimer->uTimeElapsed;
                if (!(object->uFlags & OBJECT_DESC_TEMPORARY)) continue;
                if (pSpriteObjects[i].uSpriteFrameID >= 0) {
                    v7 = object->uLifetime;
                    if (pSpriteObjects[i].uAttributes & ITEM_BROKEN)
                        v7 = pSpriteObjects[i].field_20;
                    if (pSpriteObjects[i].uSpriteFrameID < v7) continue;
                }
                SpriteObject::OnInteraction(i);
                continue;
            }
            if (pSpriteObjects[i].uObjectDescID) {
                pSpriteObjects[i].uSpriteFrameID += pEventTimer->uTimeElapsed;
                if (object->uFlags & OBJECT_DESC_TEMPORARY) {
                    if (pSpriteObjects[i].uSpriteFrameID < 0) {
                        SpriteObject::OnInteraction(i);
                        continue;
                    }
                    v11 = object->uLifetime;
                    if (pSpriteObjects[i].uAttributes & ITEM_BROKEN)
                        v11 = pSpriteObjects[i].field_20;
                }
                if (!(object->uFlags & OBJECT_DESC_TEMPORARY) ||
                    pSpriteObjects[i].uSpriteFrameID < v11) {
                    if (uCurrentlyLoadedLevelType == LEVEL_Indoor)
                        SpriteObject::UpdateObject_fn0_BLV(i);
                    else
                        SpriteObject::UpdateObject_fn0_ODM(i);
                    if (!pParty->bTurnBasedModeOn || !(pSpriteObjects[i].uSectorID & 4)) {
                        continue;
                    }
                    v12 = abs(pParty->vPosition.x -
                              pSpriteObjects[i].vPosition.x);
                    v18 = abs(pParty->vPosition.y -
                              pSpriteObjects[i].vPosition.y);
                    v19 = abs(pParty->vPosition.z -
                              pSpriteObjects[i].vPosition.z);
                    if (int_get_vector_length(v12, v18, v19) <= 5120) continue;
                    SpriteObject::OnInteraction(i);
                    continue;
                }
                if (!(object->uFlags & OBJECT_DESC_INTERACTABLE)) {
                    SpriteObject::OnInteraction(i);
                    continue;
                }
                _46BFFA_update_spell_fx(i, PID(OBJECT_Item, i));
            }
        }
    }
}

bool sub_47531C(int a1, int *a2, int pos_x, int pos_y, int pos_z, int dir_x,
                int dir_y, int dir_z, BLVFace *face, int a10) {
    int v11;          // ST1C_4@3
    int v12;          // edi@3
    int v13;          // esi@3
    int v14;          // edi@4
    __int64 v15;      // qtt@6
                      // __int16 v16; // si@7
    int a7a;          // [sp+30h] [bp+18h]@7
    int a9b;          // [sp+38h] [bp+20h]@3
    int a9a;          // [sp+38h] [bp+20h]@3
    int a10b;         // [sp+3Ch] [bp+24h]@3
    signed int a10a;  // [sp+3Ch] [bp+24h]@4
    int a10c;         // [sp+3Ch] [bp+24h]@5

    if (a10 && face->Ethereal()) return 0;
    v11 = fixpoint_mul(dir_x, face->pFacePlane_old.vNormal.x);
    a10b = fixpoint_mul(dir_y, face->pFacePlane_old.vNormal.y);
    a9b = fixpoint_mul(dir_z, face->pFacePlane_old.vNormal.z);
    v12 = v11 + a9b + a10b;
    a9a = v11 + a9b + a10b;
    v13 = (a1 << 16) - pos_x * face->pFacePlane_old.vNormal.x -
          pos_y * face->pFacePlane_old.vNormal.y -
          pos_z * face->pFacePlane_old.vNormal.z - face->pFacePlane_old.dist;
    if (abs((a1 << 16) - pos_x * face->pFacePlane_old.vNormal.x -
            pos_y * face->pFacePlane_old.vNormal.y -
            pos_z * face->pFacePlane_old.vNormal.z -
            face->pFacePlane_old.dist) >= a1 << 16) {
        a10c = abs(v13) >> 14;
        if (a10c > abs(v12)) return 0;
        HEXRAYS_LODWORD(v15) = v13 << 16;
        HEXRAYS_HIDWORD(v15) = v13 >> 16;
        v14 = a1;
        a10a = v15 / a9a;
    } else {
        a10a = 0;
        v14 = abs(v13) >> 16;
    }
    // v16 = pos_y + ((unsigned int)fixpoint_mul(a10a, dir_y) >> 16);
    HEXRAYS_LOWORD(a7a) = (short)pos_x +
                          ((unsigned int)fixpoint_mul(a10a, dir_x) >> 16) -
                          fixpoint_mul(v14, face->pFacePlane_old.vNormal.x);
    HEXRAYS_HIWORD(a7a) = pos_y +
                          ((unsigned int)fixpoint_mul(a10a, dir_y) >> 16) -
                          fixpoint_mul(v14, face->pFacePlane_old.vNormal.y);
    if (!sub_475665(face, a7a,
                    (short)pos_z +
                        ((unsigned int)fixpoint_mul(a10a, dir_z) >> 16) -
                        fixpoint_mul(v14, face->pFacePlane_old.vNormal.z)))
        return 0;
    *a2 = a10a >> 16;
    if (a10a >> 16 < 0) *a2 = 0;
    return 1;
}

bool sub_4754BF(int a1, int *a2, int X, int Y, int Z, int dir_x, int dir_y,
                int dir_z, BLVFace *face, int a10, int a11) {
    int v12;      // ST1C_4@3
    int v13;      // edi@3
    int v14;      // esi@3
    int v15;      // edi@4
    int64_t v16;  // qtt@6
                  // __int16 v17; // si@7
    int a7a;      // [sp+30h] [bp+18h]@7
    int a1b;      // [sp+38h] [bp+20h]@3
    int a1a;      // [sp+38h] [bp+20h]@3
    int a11b;     // [sp+40h] [bp+28h]@3
    int a11a;     // [sp+40h] [bp+28h]@4
    int a11c;     // [sp+40h] [bp+28h]@5

    if (a11 && face->Ethereal()) return false;
    v12 = fixpoint_mul(dir_x, face->pFacePlane_old.vNormal.x);
    a11b = fixpoint_mul(dir_y, face->pFacePlane_old.vNormal.y);
    a1b = fixpoint_mul(dir_z, face->pFacePlane_old.vNormal.z);
    v13 = v12 + a1b + a11b;
    a1a = v12 + a1b + a11b;
    v14 = (a1 << 16) - X * face->pFacePlane_old.vNormal.x -
          Y * face->pFacePlane_old.vNormal.y -
          Z * face->pFacePlane_old.vNormal.z - face->pFacePlane_old.dist;
    if (abs((a1 << 16) - X * face->pFacePlane_old.vNormal.x -
            Y * face->pFacePlane_old.vNormal.y -
            Z * face->pFacePlane_old.vNormal.z - face->pFacePlane_old.dist) >=
        a1 << 16) {
        a11c = abs(v14) >> 14;
        if (a11c > abs(v13)) return false;
        HEXRAYS_LODWORD(v16) = v14 << 16;
        HEXRAYS_HIDWORD(v16) = v14 >> 16;
        v15 = a1;
        a11a = v16 / a1a;
    } else {
        a11a = 0;
        v15 = abs(v14) >> 16;
    }
    // v17 = Y + ((unsigned int)fixpoint_mul(a11a, dir_y) >> 16);
    HEXRAYS_LOWORD(a7a) = (short)X +
                          ((unsigned int)fixpoint_mul(a11a, dir_x) >> 16) -
                          fixpoint_mul(v15, face->pFacePlane_old.vNormal.x);
    HEXRAYS_HIWORD(a7a) = Y + ((unsigned int)fixpoint_mul(a11a, dir_y) >> 16) -
                          fixpoint_mul(v15, face->pFacePlane_old.vNormal.y);
    if (!sub_4759C9(face, a10, a7a,
                    (short)Z + ((unsigned int)fixpoint_mul(a11a, dir_z) >> 16) -
                        fixpoint_mul(v15, face->pFacePlane_old.vNormal.z)))
        return false;
    *a2 = a11a >> 16;
    if (a11a >> 16 < 0) *a2 = 0;
    return true;
}

int sub_475665(BLVFace *face, int a2, __int16 a3) {
    bool v16;     // edi@14
    int v20;      // ebx@18
    int v21;      // edi@20
    int v22;      // ST14_4@22
    __int64 v23;  // qtt@22
    int result;   // eax@25
    int v25;      // [sp+14h] [bp-10h]@14
    int v26;      // [sp+1Ch] [bp-8h]@2
    int v27;      // [sp+20h] [bp-4h]@2
    int v28;      // [sp+30h] [bp+Ch]@2
    int v29;      // [sp+30h] [bp+Ch]@7
    int v30;      // [sp+30h] [bp+Ch]@11
    int v31;      // [sp+30h] [bp+Ch]@14

    if (face->uAttributes & FACE_XY_PLANE) {
        v26 = (signed __int16)a2;
        v27 = HEXRAYS_SHIWORD(a2);
        if (face->uNumVertices) {
            for (v28 = 0; v28 < face->uNumVertices; v28++) {
                word_720C10_intercepts_xs[2 * v28] =
                    face->pXInterceptDisplacements[v28] +
                    pIndoor->pVertices[face->pVertexIDs[v28]].x;
                word_720B40_intercepts_zs[2 * v28] =
                    face->pYInterceptDisplacements[v28] +
                    pIndoor->pVertices[face->pVertexIDs[v28]].y;
                word_720C10_intercepts_xs[2 * v28 + 1] =
                    face->pXInterceptDisplacements[v28 + 1] +
                    pIndoor->pVertices[face->pVertexIDs[v28 + 1]].x;
                word_720B40_intercepts_zs[2 * v28 + 1] =
                    face->pYInterceptDisplacements[v28 + 1] +
                    pIndoor->pVertices[face->pVertexIDs[v28 + 1]].y;
            }
        }
    } else {
        if (face->uAttributes & FACE_XZ_PLANE) {
            v26 = (signed __int16)a2;
            v27 = a3;
            if (face->uNumVertices) {
                for (v29 = 0; v29 < face->uNumVertices; v29++) {
                    word_720C10_intercepts_xs[2 * v29] =
                        face->pXInterceptDisplacements[v29] +
                        pIndoor->pVertices[face->pVertexIDs[v29]].x;
                    word_720B40_intercepts_zs[2 * v29] =
                        face->pZInterceptDisplacements[v29] +
                        pIndoor->pVertices[face->pVertexIDs[v29]].z;
                    word_720C10_intercepts_xs[2 * v29 + 1] =
                        face->pXInterceptDisplacements[v29 + 1] +
                        pIndoor->pVertices[face->pVertexIDs[v29 + 1]].x;
                    word_720B40_intercepts_zs[2 * v29 + 1] =
                        face->pZInterceptDisplacements[v29 + 1] +
                        pIndoor->pVertices[face->pVertexIDs[v29 + 1]].z;
                }
            }
        } else {
            v26 = HEXRAYS_SHIWORD(a2);
            v27 = a3;
            if (face->uNumVertices) {
                for (v30 = 0; v30 < face->uNumVertices; v30++) {
                    word_720C10_intercepts_xs[2 * v30] =
                        face->pYInterceptDisplacements[v30] +
                        pIndoor->pVertices[face->pVertexIDs[v30]].y;
                    word_720B40_intercepts_zs[2 * v30] =
                        face->pZInterceptDisplacements[v30] +
                        pIndoor->pVertices[face->pVertexIDs[v30]].z;
                    word_720C10_intercepts_xs[2 * v30 + 1] =
                        face->pYInterceptDisplacements[v30 + 1] +
                        pIndoor->pVertices[face->pVertexIDs[v30 + 1]].y;
                    word_720B40_intercepts_zs[2 * v30 + 1] =
                        face->pZInterceptDisplacements[v30 + 1] +
                        pIndoor->pVertices[face->pVertexIDs[v30 + 1]].z;
                }
            }
        }
    }
    v31 = 0;
    word_720C10_intercepts_xs[2 * face->uNumVertices] =
        word_720C10_intercepts_xs[0];
    word_720B40_intercepts_zs[2 * face->uNumVertices] =
        word_720B40_intercepts_zs[0];
    v16 = word_720B40_intercepts_zs[0] >= v27;
    if (2 * face->uNumVertices <= 0) return 0;
    for (v25 = 0; v25 < 2 * face->uNumVertices; ++v25) {
        if (v31 >= 2) break;
        if (v16 ^ (word_720B40_intercepts_zs[v25 + 1] >= v27)) {
            if (word_720C10_intercepts_xs[v25 + 1] >= v26)
                v20 = 0;
            else
                v20 = 2;
            v21 = v20 | (word_720C10_intercepts_xs[v25] < v26);
            if (v21 != 3) {
                v22 = word_720C10_intercepts_xs[v25 + 1] -
                      word_720C10_intercepts_xs[v25];
                HEXRAYS_LODWORD(v23) = v22 << 16;
                HEXRAYS_HIDWORD(v23) = v22 >> 16;
                if (!v21 ||
                    (word_720C10_intercepts_xs[v25] +
                         ((signed int)(((unsigned __int64)(v23 /
                                                           (word_720B40_intercepts_zs
                                                                [v25 + 1] -
                                                            word_720B40_intercepts_zs
                                                                [v25]) *
                                                           ((v27 -
                                                             (signed int)
                                                                 word_720B40_intercepts_zs
                                                                     [v25])
                                                            << 16)) >>
                                        16) +
                                       32768) >>
                          16) >=
                     v26))
                    ++v31;
            }
        }
        v16 = word_720B40_intercepts_zs[v25 + 1] >= v27;
    }
    result = 1;
    if (v31 != 1) result = 0;
    return result;
}

bool sub_4759C9(BLVFace *face, int a2, int a3, __int16 a4) {
    bool v12;            // edi@14
    signed int v16;      // ebx@18
    int v17;             // edi@20
    signed int v18;      // ST14_4@22
    signed __int64 v19;  // qtt@22
    bool result;         // eax@25
    int v21;             // [sp+14h] [bp-10h]@14
    signed int v22;      // [sp+18h] [bp-Ch]@1
    int v23;             // [sp+1Ch] [bp-8h]@2
    signed int v24;      // [sp+20h] [bp-4h]@2
    signed int a4d;      // [sp+30h] [bp+Ch]@14

    if (face->uAttributes & FACE_XY_PLANE) {
        v23 = (signed __int16)a3;
        v24 = HEXRAYS_SHIWORD(a3);
        if (face->uNumVertices) {
            for (v22 = 0; v22 < face->uNumVertices; ++v22) {
                word_720A70_intercepts_xs_plus_xs[2 * v22] =
                    face->pXInterceptDisplacements[v22] +
                    LOWORD(pOutdoor->pBModels[a2]
                               .pVertices.pVertices[face->pVertexIDs[v22]]
                               .x);
                word_7209A0_intercepts_ys_plus_ys[2 * v22] =
                    face->pYInterceptDisplacements[v22] +
                    LOWORD(pOutdoor->pBModels[a2]
                               .pVertices.pVertices[face->pVertexIDs[v22]]
                               .y);
                word_720A70_intercepts_xs_plus_xs[2 * v22 + 1] =
                    face->pXInterceptDisplacements[v22 + 1] +
                    LOWORD(pOutdoor->pBModels[a2]
                               .pVertices.pVertices[face->pVertexIDs[v22 + 1]]
                               .x);
                word_7209A0_intercepts_ys_plus_ys[2 * v22 + 1] =
                    face->pYInterceptDisplacements[v22 + 1] +
                    LOWORD(pOutdoor->pBModels[a2]
                               .pVertices.pVertices[face->pVertexIDs[v22 + 1]]
                               .y);
            }
        }
    } else {
        if (face->uAttributes & FACE_XZ_PLANE) {
            v23 = (signed __int16)a3;
            v24 = a4;
            if (face->uNumVertices) {
                for (v22 = 0; v22 < face->uNumVertices; ++v22) {
                    word_720A70_intercepts_xs_plus_xs[2 * v22] =
                        face->pXInterceptDisplacements[v22] +
                        LOWORD(pOutdoor->pBModels[a2]
                                   .pVertices.pVertices[face->pVertexIDs[v22]]
                                   .x);
                    word_7209A0_intercepts_ys_plus_ys[2 * v22] =
                        face->pZInterceptDisplacements[v22] +
                        LOWORD(pOutdoor->pBModels[a2]
                                   .pVertices.pVertices[face->pVertexIDs[v22]]
                                   .z);
                    word_720A70_intercepts_xs_plus_xs[2 * v22 + 1] =
                        face->pXInterceptDisplacements[v22 + 1] +
                        LOWORD(
                            pOutdoor->pBModels[a2]
                                .pVertices.pVertices[face->pVertexIDs[v22 + 1]]
                                .x);
                    word_7209A0_intercepts_ys_plus_ys[2 * v22 + 1] =
                        face->pZInterceptDisplacements[v22 + 1] +
                        LOWORD(
                            pOutdoor->pBModels[a2]
                                .pVertices.pVertices[face->pVertexIDs[v22 + 1]]
                                .z);
                }
            }
        } else {
            v23 = HEXRAYS_SHIWORD(a3);
            v24 = a4;
            if (face->uNumVertices) {
                for (v22 = 0; v22 < face->uNumVertices; ++v22) {
                    word_720A70_intercepts_xs_plus_xs[2 * v22] =
                        face->pYInterceptDisplacements[v22] +
                        LOWORD(pOutdoor->pBModels[a2]
                                   .pVertices.pVertices[face->pVertexIDs[v22]]
                                   .y);
                    word_7209A0_intercepts_ys_plus_ys[2 * v22] =
                        face->pZInterceptDisplacements[v22] +
                        LOWORD(pOutdoor->pBModels[a2]
                                   .pVertices.pVertices[face->pVertexIDs[v22]]
                                   .z);
                    word_720A70_intercepts_xs_plus_xs[2 * v22 + 1] =
                        face->pYInterceptDisplacements[v22 + 1] +
                        LOWORD(
                            pOutdoor->pBModels[a2]
                                .pVertices.pVertices[face->pVertexIDs[v22 + 1]]
                                .y);
                    word_7209A0_intercepts_ys_plus_ys[2 * v22 + 1] =
                        face->pZInterceptDisplacements[v22 + 1] +
                        LOWORD(
                            pOutdoor->pBModels[a2]
                                .pVertices.pVertices[face->pVertexIDs[v22 + 1]]
                                .z);
                }
            }
        }
    }
    a4d = 0;
    word_720A70_intercepts_xs_plus_xs[2 * face->uNumVertices] =
        word_720A70_intercepts_xs_plus_xs[0];
    word_7209A0_intercepts_ys_plus_ys[2 * face->uNumVertices] =
        word_7209A0_intercepts_ys_plus_ys[0];
    v12 = word_7209A0_intercepts_ys_plus_ys[0] >= v24;
    if (2 * face->uNumVertices <= 0) return 0;
    for (v21 = 0; v21 < 2 * face->uNumVertices; ++v21) {
        if (a4d >= 2) break;
        if (v12 ^ (word_7209A0_intercepts_ys_plus_ys[v21 + 1] >= v24)) {
            if (word_720A70_intercepts_xs_plus_xs[v21 + 1] >= v23)
                v16 = 0;
            else
                v16 = 2;
            v17 = v16 | (word_720A70_intercepts_xs_plus_xs[v21] < v23);
            if (v17 != 3) {
                v18 = word_720A70_intercepts_xs_plus_xs[v21 + 1] -
                      word_720A70_intercepts_xs_plus_xs[v21];
                HEXRAYS_LODWORD(v19) = v18 << 16;
                HEXRAYS_HIDWORD(v19) = v18 >> 16;
                if (!v17 ||
                    (word_720A70_intercepts_xs_plus_xs[v21] +
                         ((signed int)(((unsigned __int64)(v19 /
                                                           (word_7209A0_intercepts_ys_plus_ys
                                                                [v21 + 1] -
                                                            word_7209A0_intercepts_ys_plus_ys
                                                                [v21]) *
                                                           ((v24 -
                                                             (signed int)
                                                                 word_7209A0_intercepts_ys_plus_ys
                                                                     [v21])
                                                            << 16)) >>
                                        16) +
                                       0x8000) >>
                          16) >=
                     v23))
                    ++a4d;
            }
        }
        v12 = word_7209A0_intercepts_ys_plus_ys[v21 + 1] >= v24;
    }
    result = 1;
    if (a4d != 1) result = 0;
    return result;
}

bool sub_475D85(Vec3_int_ *a1, Vec3_int_ *a2, int *a3, BLVFace *a4) {
    BLVFace *v4;         // ebx@1
    int v5;              // ST24_4@2
    int v6;              // ST28_4@2
    int v7;              // edi@2
    int v8;              // eax@5
    signed int v9;       // esi@5
    signed __int64 v10;  // qtt@10
    Vec3_int_ *v11;      // esi@11
    int v12;             // ST14_4@11
    Vec3_int_ *v14;      // [sp+Ch] [bp-18h]@1
    Vec3_int_ *v15;      // [sp+14h] [bp-10h]@1
    int v17;             // [sp+20h] [bp-4h]@10
    int a4b;             // [sp+30h] [bp+Ch]@2
    int a4c;             // [sp+30h] [bp+Ch]@9
    int a4a;             // [sp+30h] [bp+Ch]@10

    v4 = a4;
    v15 = a2;
    v14 = a1;
    v5 = fixpoint_mul(a2->x, a4->pFacePlane_old.vNormal.x);
    a4b = fixpoint_mul(a2->y, a4->pFacePlane_old.vNormal.y);
    v6 = fixpoint_mul(a2->z, v4->pFacePlane_old.vNormal.z);
    v7 = v5 + v6 + a4b;
    // (v16 = v5 + v6 + a4b) == 0;
    if (a4->uAttributes & FACE_ETHEREAL || !v7 || v7 > 0 && !v4->Portal())
        return 0;
    v8 = v4->pFacePlane_old.vNormal.z * a1->z;
    v9 = -(v4->pFacePlane_old.dist + v8 + a1->y * v4->pFacePlane_old.vNormal.y +
           a1->x * v4->pFacePlane_old.vNormal.x);
    if (v7 <= 0) {
        if (v4->pFacePlane_old.dist + v8 +
                a1->y * v4->pFacePlane_old.vNormal.y +
                a1->x * v4->pFacePlane_old.vNormal.x <
            0)
            return 0;
    } else {
        if (v9 < 0) return 0;
    }
    a4c = abs(-(v4->pFacePlane_old.dist + v8 +
                a1->y * v4->pFacePlane_old.vNormal.y +
                a1->x * v4->pFacePlane_old.vNormal.x)) >>
          14;
    v11 = v14;
    HEXRAYS_LODWORD(v10) = v9 << 16;
    HEXRAYS_HIDWORD(v10) = v9 >> 16;
    a4a = v10 / v7;
    v17 = v10 / v7;
    HEXRAYS_LOWORD(v12) =
        HEXRAYS_LOWORD(v14->x) +
        (((unsigned int)fixpoint_mul(v17, v15->x) + 0x8000) >> 16);
    HEXRAYS_HIWORD(v12) =
        HEXRAYS_LOWORD(v11->y) +
        (((unsigned int)fixpoint_mul(v17, v15->y) + 0x8000) >> 16);
    if (a4c > abs(v7) || (v17 > *a3 << 16) ||
        !sub_475665(
            v4, v12,
            LOWORD(v11->z) +
                (((unsigned int)fixpoint_mul(v17, v15->z) + 0x8000) >> 16)))
        return 0;
    *a3 = a4a >> 16;
    return 1;
}

bool sub_475F30(int *a1, BLVFace *a2, int a3, int a4, int a5, int a6, int a7,
                int a8, int a9) {
    int v10 = fixpoint_mul(a6, a2->pFacePlane_old.vNormal.x);
    int v11 = fixpoint_mul(a7, a2->pFacePlane_old.vNormal.y);
    int v12 = fixpoint_mul(a8, a2->pFacePlane_old.vNormal.z);
    int v13 = v10 + v12 + v11;
    int v14 = v10 + v12 + v11;
    int v22 = v10 + v12 + v11;
    if (a2->Ethereal() || !v13 || v14 > 0 && !a2->Portal()) {
        return false;
    }
    int v16 = -(a2->pFacePlane_old.dist + a4 * a2->pFacePlane_old.vNormal.y +
                a3 * a2->pFacePlane_old.vNormal.x +
                a5 * a2->pFacePlane_old.vNormal.z);
    if (v14 <= 0) {
        if (a2->pFacePlane_old.dist + a4 * a2->pFacePlane_old.vNormal.y +
                a3 * a2->pFacePlane_old.vNormal.x +
                a5 * a2->pFacePlane_old.vNormal.z <
            0)
            return 0;
    } else {
        if (v16 < 0) {
            return 0;
        }
    }
    int v17 =
        abs(-(a2->pFacePlane_old.dist + a4 * a2->pFacePlane_old.vNormal.y +
              a3 * a2->pFacePlane_old.vNormal.x +
              a5 * a2->pFacePlane_old.vNormal.z)) >>
        14;
    int64_t v18;
    HEXRAYS_LODWORD(v18) = v16 << 16;
    HEXRAYS_HIDWORD(v18) = v16 >> 16;
    int v24 = v18 / v22;
    int v23 = v18 / v22;
    int v19;
    HEXRAYS_LOWORD(v19) =
        a3 + (((unsigned int)fixpoint_mul(v23, a6) + 0x8000) >> 16);
    HEXRAYS_HIWORD(v19) =
        a4 + (((unsigned int)fixpoint_mul(v23, a7) + 0x8000) >> 16);
    if (v17 > abs(v14) || v23 > *a1 << 16 ||
        !sub_4759C9(
            a2, a9, v19,
            a5 + (((unsigned int)fixpoint_mul(v23, a8) + 0x8000) >> 16)))
        return 0;
    *a1 = v24 >> 16;
    return 1;
}

bool IsBModelVisible(BSPModel *model, int *reachable) {
    int v11;      // esi@6
    int v12;      // esi@8
    bool result;  // eax@9

    int angle = (int)(pODMRenderParams->uCameraFovInDegrees << 11) / 360 / 2;
    int v3 = model->vBoundingCenter.x - pIndoorCameraD3D->vPartyPos.x;
    int v4 = model->vBoundingCenter.y - pIndoorCameraD3D->vPartyPos.y;
    stru_5C6E00->Sin(pIndoorCameraD3D->sRotationX);
    int v17 = v3 * stru_5C6E00->Cos(pIndoorCameraD3D->sRotationY) +
              v4 * stru_5C6E00->Sin(pIndoorCameraD3D->sRotationY);
    if (pIndoorCameraD3D->sRotationX) {
        v17 = fixpoint_mul(v17, stru_5C6E00->Cos(pIndoorCameraD3D->sRotationX));
    }
    int v19 = v4 * stru_5C6E00->Cos(pIndoorCameraD3D->sRotationY) -
              v3 * stru_5C6E00->Sin(pIndoorCameraD3D->sRotationY);
    int v9 = int_get_vector_length(abs(v3), abs(v4), 0);
    // v10 = v14 * 188;
    // v22 = device_caps;
    *reachable = false;
    if (v9 < model->sBoundingRadius + 256) *reachable = true;
    if (v19 >= 0)
        v11 = fixpoint_mul(stru_5C6E00->Sin(angle), v17) -
              fixpoint_mul(stru_5C6E00->Cos(angle), v19);
    else
        v11 = fixpoint_mul(stru_5C6E00->Cos(angle), v19) +
              fixpoint_mul(stru_5C6E00->Sin(angle), v17);
    v12 = v11 >> 16;
    if (v9 <= pIndoorCameraD3D->GetFarClip() + 2048) {
        // if ( abs(v12) > *(int *)((char *)&pOutdoor->pBModels->sBoundingRadius
        // + v10) + 512 )
        if (abs(v12) > model->sBoundingRadius + 512) {
            result = v12 < 0;
            HEXRAYS_LOBYTE(result) = v12 >= 0;
            return result;
        } else {
            return true;
        }
    }
    return false;
}

// int Polygon::_479295() {
//    int v3;           // ecx@4
//    int v4;           // eax@4
//    int v5;           // edx@4
//    Vec3_int_ thisa;  // [sp+Ch] [bp-10h]@8
//    int v11;          // [sp+18h] [bp-4h]@4
//
//    if (!this->pODMFace->pFacePlane.vNormal.z) {
//        v3 = this->pODMFace->pFacePlane.vNormal.x;
//        v4 = -this->pODMFace->pFacePlane.vNormal.y;
//        v5 = 0;
//        v11 = 65536;
//    } else if ((this->pODMFace->pFacePlane.vNormal.x ||
//                this->pODMFace->pFacePlane.vNormal.y) &&
//               abs(this->pODMFace->pFacePlane.vNormal.z) < 59082) {
//        thisa.x = -this->pODMFace->pFacePlane.vNormal.y;
//        thisa.y = this->pODMFace->pFacePlane.vNormal.x;
//        thisa.z = 0;
//        thisa.Normalize_float();
//        v4 = thisa.x;
//        v3 = thisa.y;
//        v5 = 0;
//        v11 = 65536;
//    } else {
//        v3 = 0;
//        v4 = 65536;
//        v11 = 0;
//        v5 = -65536;
//    }
//    sTextureDeltaU = this->pODMFace->sTextureDeltaU;
//    sTextureDeltaV = this->pODMFace->sTextureDeltaV;
//    ptr_38->CalcSkyFrustumVec(v4, v3, 0, 0, v5, v11);
//    return 1;
//}

void sr_485F53(Vec2_int_ *v) {
    ++v->y;
    if (v->y > 1000) v->y = 0;
}

void Polygon::_normalize_v_18() {
    float len = sqrt((double)this->v_18.z * (double)this->v_18.z +
                     (double)this->v_18.y * (double)this->v_18.y +
                     (double)this->v_18.x * (double)this->v_18.x);
    if (fabsf(len) < 1e-6f) {
        v_18.x = 0;
        v_18.y = 0;
        v_18.z = 65536;
    } else {
        v_18.x = round_to_int((double)this->v_18.x / len * 65536.0);
        v_18.y = round_to_int((double)this->v_18.y / len * 65536.0);
        v_18.z = round_to_int((double)this->v_18.z / len * 65536.0);
    }
}

void Render::DrawOutdoorSkyD3D() {
    int v13;                     // edi@6
    int v14;                     // ecx@6
    int v15;                     // eax@8
    int v16;                     // eax@12
    int v32;                     // [sp+13Ch] [bp-28h]@6
    int v35;                     // [sp+148h] [bp-1Ch]@4
    int v36;                     // [sp+14Ch] [bp-18h]@2
    int v37;                     // [sp+154h] [bp-10h]@8
    int v39;                     // [sp+15Ch] [bp-8h]@4


    double rot_to_rads = ((2 * pi_double) / 2048);

    // lowers clouds as party goes up
    int  horizon_height_offset =
        (signed __int64)((double)(pODMRenderParams->int_fov_rad * pIndoorCameraD3D->vPartyPos.z)
            / ((double)pODMRenderParams->int_fov_rad + 8192.0)
            + (double)(pViewport->uScreenCenterY));

    // magnitude in up direction
    signed __int64 cam_vec_up = cos((double)pIndoorCameraD3D->sRotationX * rot_to_rads) *
        pIndoorCameraD3D->GetFarClip();

    int bot_y_proj = (signed __int64)((double)(pViewport->uScreenCenterY) -
        (double)pODMRenderParams->int_fov_rad /
        (cam_vec_up + 0.0000001) *
        (sin((double)pIndoorCameraD3D->sRotationX * rot_to_rads)
            *
            pIndoorCameraD3D->GetFarClip() -
            (double)pIndoorCameraD3D->vPartyPos.z));

    struct Polygon pSkyPolygon;
    pSkyPolygon.texture = nullptr;
    pSkyPolygon.ptr_38 = &SkyBillboard;


    // if ( pParty->uCurrentHour > 20 || pParty->uCurrentHour < 5 )
    // pSkyPolygon.uTileBitmapID = pOutdoor->New_SKY_NIGHT_ID;
    // else
    // pSkyPolygon.uTileBitmapID = pOutdoor->sSky_TextureID;//179(original 166)
    // pSkyPolygon.pTexture = (Texture_MM7 *)(pSkyPolygon.uTileBitmapID != -1 ?
    // (int)&pBitmaps_LOD->pTextures[pSkyPolygon.uTileBitmapID] : 0);

    pSkyPolygon.texture = pOutdoor->sky_texture;
    if (pSkyPolygon.texture) {
        pSkyPolygon.dimming_level = 0;
        pSkyPolygon.uNumVertices = 4;

        // centering(центруем)-----------------------------------------------------------------
        // plane of sky polygon rotation vector
        pSkyPolygon.v_18.x =
            -stru_5C6E00->Sin(-pIndoorCameraD3D->sRotationX + 16);
        pSkyPolygon.v_18.y = 0;
        pSkyPolygon.v_18.z =
            -stru_5C6E00->Cos(pIndoorCameraD3D->sRotationX + 16);

        // sky wiew position(положение неба на
        // экране)------------------------------------------
        //                X
        // 0._____________________________.3
        //  |8,8                    468,8 |
        //  |                             |
        //  |                             |
        // Y|                             |
        //  |                             |
        //  |8,351                468,351 |
        // 1._____________________________.2
        //
        VertexRenderList[0].vWorldViewProjX =
            (double)(signed int)pViewport->uViewportTL_X;  // 8
        VertexRenderList[0].vWorldViewProjY =
            (double)(signed int)pViewport->uViewportTL_Y;  // 8

        VertexRenderList[1].vWorldViewProjX =
            (double)(signed int)pViewport->uViewportTL_X;   // 8
        VertexRenderList[1].vWorldViewProjY = (double)bot_y_proj + 1;  // 247

        VertexRenderList[2].vWorldViewProjX =
            (double)(signed int)pViewport->uViewportBR_X;   // 468
        VertexRenderList[2].vWorldViewProjY = (double)bot_y_proj + 1;  // 247

        VertexRenderList[3].vWorldViewProjX =
            (double)(signed int)pViewport->uViewportBR_X;  // 468
        VertexRenderList[3].vWorldViewProjY =
            (double)(signed int)pViewport->uViewportTL_Y;  // 8

        double half_fov_angle_rads = ((pODMRenderParams->uCameraFovInDegrees - 1) * pi_double) / 360;

        // far width per pixel??
        int widthperpixel = 65536 /
            (signed int)(signed __int64)(((double)(pViewport->uViewportBR_X - pViewport->uViewportTL_X) / 2)
                / tan(half_fov_angle_rads) +
                0.5);

        for (uint i = 0; i < pSkyPolygon.uNumVertices; ++i) {
            // rotate skydome(вращение купола
            // неба)--------------------------------------
            // В игре принята своя система измерения углов. Полный угол (180).
            // Значению угла 0 соответствует направление на север и/или юг (либо
            // на восток и/или запад), значению 65536 еденицам(0х10000)
            // соответствует угол 90. две переменные хранят данные по углу
            // обзора. field_14 по западу и востоку. field_20 по югу и северу от
            // -25080 до 25080

            v13 = widthperpixel * (pViewport->uScreenCenterX -
                (signed __int64)VertexRenderList[i].vWorldViewProjX);


            v39 = fixpoint_mul(
                pSkyPolygon.ptr_38->CamVecLeft_Y,
                widthperpixel * (horizon_height_offset - floor(VertexRenderList[i].vWorldViewProjY + 0.5)));
            v35 = v39 + pSkyPolygon.ptr_38->CamVecLeft_Z;

            int skyfinalleft = v35 + fixpoint_mul(pSkyPolygon.ptr_38->CamVecLeft_X, v13);

            v39 = fixpoint_mul(
                pSkyPolygon.ptr_38->CamVecFront_Y,
                widthperpixel * (horizon_height_offset - floor(VertexRenderList[i].vWorldViewProjY + 0.f)));
            v36 = v39 + pSkyPolygon.ptr_38->CamVecFront_Z;

            int finalskyfront = v36 + fixpoint_mul(pSkyPolygon.ptr_38->CamVecFront_X, v13);


            int v9 = fixpoint_mul(
                pSkyPolygon.v_18.z,
                widthperpixel * (horizon_height_offset - floor(VertexRenderList[i].vWorldViewProjY + 0.5)));




            int top_y_proj = pSkyPolygon.v_18.x + v9;
            if (top_y_proj > 0) top_y_proj = 0;

            v32 = (signed __int64)VertexRenderList[i].vWorldViewProjY - 1.0;
            v14 = widthperpixel * (horizon_height_offset - v32);
            while (1) {
                if (top_y_proj) {
                    v37 = 2048;  // abs((int)cam_vec_up >> 14);
                    v15 = abs(top_y_proj);
                    if (v37 <= v15 ||
                        v32 <= (signed int)pViewport->uViewportTL_Y) {
                        if (top_y_proj <= 0) break;
                    }
                }
                v16 = fixpoint_mul(pSkyPolygon.v_18.z, v14);  // does this bit ever get called?
                --v32;
                v14 += widthperpixel;
                top_y_proj = pSkyPolygon.v_18.x + v16;
            }

            signed int worldviewdepth = (__int64(-512) * 65536 * 65536) / top_y_proj;
            if (worldviewdepth < 0) worldviewdepth = pIndoorCameraD3D->GetFarClip();

            int texoffset_U = 224 * pMiscTimer->uTotalGameTimeElapsed +
                ((signed int)fixpoint_mul(skyfinalleft, worldviewdepth) >> 3);
            VertexRenderList[i].u = (double)texoffset_U /
                ((double)pSkyPolygon.texture->GetWidth() * 65536.0);

            int texoffset_V = 224 * pMiscTimer->uTotalGameTimeElapsed +
                ((signed int)fixpoint_mul(finalskyfront, worldviewdepth) >> 3);
            VertexRenderList[i].v = (double)texoffset_V /
                ((double)pSkyPolygon.texture->GetHeight() * 65536.0);

            VertexRenderList[i].vWorldViewPosition.x = pIndoorCameraD3D->GetFarClip();
            VertexRenderList[i]._rhw = 1.0 / (double)(worldviewdepth >> 16);
        }

        DrawOutdoorSkyPolygon(&pSkyPolygon);

        // adjust and draw again to fill gap below horizon
        // could mirror over??

        VertexRenderList[0].vWorldViewProjY += 60;
        VertexRenderList[1].vWorldViewProjY += 60;
        VertexRenderList[2].vWorldViewProjY += 60;
        VertexRenderList[3].vWorldViewProjY += 60;

        DrawOutdoorSkyPolygon(&pSkyPolygon);
    }
}

void Render::DrawIndoorSky(unsigned int uNumVertices, unsigned int uFaceID) {  // can this be combined with outdoor or removed?
    BLVFace *pFace;              // esi@1
    double v5;                   // st7@3
    signed __int64 v6;           // qax@3
    int v12;                     // edx@7
    int v13;                     // eax@7
    int v17;                     // edi@9
    double v18;                  // st7@9
    signed int v19;              // ebx@9
    void *v20;                   // ecx@9
    int v21;                     // ebx@11
    int v22;                     // eax@14
    signed __int64 v23;          // qtt@16
    double v28;                  // st7@20
    double v33;                  // st6@23
    const void *v35;             // ecx@31
    int v36;                     // eax@31
    const void *v37;             // edi@31
    signed __int64 v38;          // qax@31
    int v39;                     // ecx@31
    int v40;                     // ebx@33
    int v41;                     // eax@36
    signed __int64 v42;          // qtt@39
    int v43;                     // eax@39
    double v48;                  // st7@41
    double v51;                  // st7@46
    struct Polygon pSkyPolygon;  // [sp+14h] [bp-160h]@6
    unsigned int v63;            // [sp+120h] [bp-54h]@7
    unsigned int v65;            // [sp+128h] [bp-4Ch]@1
    unsigned int v66;            // [sp+12Ch] [bp-48h]@7
    __int64 v69;                 // [sp+13Ch] [bp-38h]@3
    int v70;                     // [sp+144h] [bp-30h]@3
    int X;                       // [sp+148h] [bp-2Ch]@9
    int v72;                     // [sp+14Ch] [bp-28h]@7
    float v73;                   // [sp+150h] [bp-24h]@16
    unsigned int v74;            // [sp+154h] [bp-20h]@3
    unsigned int v74_;           // [sp+154h] [bp-20h]@3
    RenderVertexSoft *v75;       // [sp+158h] [bp-1Ch]@3
    float v76;                   // [sp+15Ch] [bp-18h]@9
    int v77;                     // [sp+160h] [bp-14h]@9
    int v78;                     // [sp+164h] [bp-10h]@7
    void *v79;                   // [sp+168h] [bp-Ch]@9
    float v80;                   // [sp+16Ch] [bp-8h]@3
    const void *v81;             // [sp+170h] [bp-4h]@7

    pFace = &pIndoor->pFaces[uFaceID];
    // for floor and wall(for example Selesta)-------------------
    if (pFace->uPolygonType == POLYGON_InBetweenFloorAndWall ||
        pFace->uPolygonType == POLYGON_Floor) {
        int v69 = (OS_GetTime() / 32) - pIndoorCameraD3D->vPartyPos.x;
        int v55 = (OS_GetTime() / 32) + pIndoorCameraD3D->vPartyPos.y;
        for (uint i = 0; i < uNumVertices; ++i) {
            array_507D30[i].u = (v69 + array_507D30[i].u) * 0.25f;
            array_507D30[i].v = (v55 + array_507D30[i].v) * 0.25f;
        }
        render->DrawIndoorPolygon(uNumVertices, pFace,
            PID(OBJECT_BModel, uFaceID), -1, 0);
        return;
    }
    //---------------------------------------
    v70 = (signed __int64)((double)(pBLVRenderParams->bsp_fov_rad *
        pIndoorCameraD3D->vPartyPos.z)  // 179
        / (((double)pBLVRenderParams->bsp_fov_rad +
            16192.0) *
            65536.0) +
            (double)pBLVRenderParams->uViewportCenterY);
    v5 = (double)pIndoorCameraD3D->sRotationX * 0.0030664064;         // 0
    v6 = (signed __int64)((double)pBLVRenderParams->uViewportCenterY  // 183
        - (double)pBLVRenderParams->bsp_fov_rad /
        ((cos(v5) * 16192.0 + 0.0000001) * 65535.0) *
        (sin(v5) * -16192.0 -
        (double)pIndoorCameraD3D->vPartyPos.z));

    SkyBillboard.CalcSkyFrustumVec(65536, 0, 0, 0, 65536, 0);
    // CalcSkyFrustumVec use as same

    pSkyPolygon.texture = nullptr;
    pSkyPolygon.ptr_38 = &SkyBillboard;

    // pSkyPolygon.uTileBitmapID = pFace->uBitmapID;
    pSkyPolygon.texture = pFace->GetTexture();

    // pSkyPolygon.pTexture =
    // pBitmaps_LOD->GetTexture(pSkyPolygon.uTileBitmapID);
    if (!pSkyPolygon.texture) return;

    pSkyPolygon.dimming_level = 0;
    pSkyPolygon.uNumVertices = uNumVertices;

    pSkyPolygon.v_18.x = -stru_5C6E00->Sin(pIndoorCameraD3D->sRotationX + 16);
    pSkyPolygon.v_18.y = 0;
    pSkyPolygon.v_18.z = -stru_5C6E00->Cos(pIndoorCameraD3D->sRotationX + 16);

    memcpy(&array_507D30[uNumVertices], array_507D30,
        sizeof(array_507D30[uNumVertices]));
    pSkyPolygon.field_24 = 0x2000000;

    extern float _calc_fov(int viewport_width, int angle_degree);
    // v64 = (double)(signed int)(pBLVRenderParams->uViewportZ -
    // pBLVRenderParams->uViewportX) * 0.5; v72 = 65536 / (signed int)(signed
    // __int64)(v64 / tan(0.6457717418670654) + 0.5);
    v72 = 65536.0f /
        _calc_fov(pBLVRenderParams->uViewportZ - pBLVRenderParams->uViewportX,
            74);
    v12 = pSkyPolygon.texture->GetWidth() - 1;
    v13 = pSkyPolygon.texture->GetHeight() - 1;
    // v67 = 1.0 / (double)pSkyPolygon.pTexture->uTextureWidth;
    v63 = 224 * pMiscTimer->uTotalGameTimeElapsed & v13;
    v66 = 224 * pMiscTimer->uTotalGameTimeElapsed & v12;
    v78 = 0;
    // v81 = 0;
    float v68 = 1.0 / (double)pSkyPolygon.texture->GetHeight();
    if ((signed int)pSkyPolygon.uNumVertices <= 0) return;

    int _507D30_idx = 0;
    for (_507D30_idx; _507D30_idx < pSkyPolygon.uNumVertices; _507D30_idx++) {
        // v15 = (void *)(v72 * (v70 -
        // (int)array_507D30[_507D30_idx].vWorldViewProjY));
        v77 = fixpoint_mul(
            pSkyPolygon.ptr_38->CamVecLeft_Y,
            v72 * (v70 - array_507D30[_507D30_idx].vWorldViewProjY));
        v74 = v77 + pSkyPolygon.ptr_38->CamVecLeft_Z;

        v77 = fixpoint_mul(
            pSkyPolygon.ptr_38->CamVecFront_Y,
            v72 * (v70 - array_507D30[_507D30_idx].vWorldViewProjY));
        v74_ = v77 + pSkyPolygon.ptr_38->CamVecFront_Z;

        v79 = (void *)(fixpoint_mul(
            pSkyPolygon.v_18.z,
            v72 * (v70 - (int)array_507D30[_507D30_idx].vWorldViewProjY)));
        v17 = v72 * (pBLVRenderParams->uViewportCenterX -
            (int)array_507D30[_507D30_idx].vWorldViewProjX);
        v18 = array_507D30[_507D30_idx].vWorldViewProjY - 1.0;
        v19 = -pSkyPolygon.field_24;
        v77 = -pSkyPolygon.field_24;
        X = (int)((char *)v79 + pSkyPolygon.v_18.x);
        HEXRAYS_LODWORD(v76) = (signed __int64)v18;
        v20 = (void *)(v72 * (v70 - HEXRAYS_LODWORD(v76)));
        while (1) {
            v79 = v20;
            if (!X) goto LABEL_14;
            v21 = abs(v19 >> 14);
            if (v21 <= abs(X))  // 0x800 <= 0x28652
                break;
            if (HEXRAYS_SLODWORD(v76) <= (signed int)pViewport->uViewportTL_Y)
                break;
            v19 = v77;
            v20 = v79;
        LABEL_14:
            v79 = (void *)fixpoint_mul(pSkyPolygon.v_18.z, (int)v20);
            v22 = fixpoint_mul(pSkyPolygon.v_18.z, (int)v20);
            --HEXRAYS_LODWORD(v76);
            v20 = (char *)v20 + v72;
            X = v22 + pSkyPolygon.v_18.x;
            v78 = 1;
        }
        if (!v78) {
            HEXRAYS_LODWORD(v23) = v77 << 16;
            HEXRAYS_HIDWORD(v23) = v77 >> 16;  // v23 = 0xfffffe0000000000
            v79 = (void *)(v23 / X);           // X = FFFF9014(-28652)
            v77 = v17;
            __int64 s = v74 + fixpoint_mul(pSkyPolygon.ptr_38->CamVecLeft_X,
                v17);  // s = 0xFFFFFFFF FFFF3EE6
            HEXRAYS_LODWORD(v80) =
                v66 +
                ((signed int)fixpoint_mul(HEXRAYS_SLODWORD(s), v23 / X) >> 4);
            array_507D30[_507D30_idx].u =
                ((double)HEXRAYS_SLODWORD(v80) * 0.000015259022) *
                (1.0 / (double)pSkyPolygon.texture->GetWidth());

            __int64 s2 =
                v74_ + fixpoint_mul(pSkyPolygon.ptr_38->CamVecFront_X, v17);
            HEXRAYS_LODWORD(v80) =
                v63 +
                ((signed int)fixpoint_mul(HEXRAYS_SLODWORD(s2), v23 / X) >> 4);
            array_507D30[_507D30_idx].v =
                ((double)HEXRAYS_SLODWORD(v80) * 0.000015259022) * v68;

            v77 = fixpoint_mul(HEXRAYS_SLODWORD(s), v23 / X);
            HEXRAYS_LODWORD(v73) = fixpoint_mul(HEXRAYS_SLODWORD(s2), v23 / X);
            array_507D30[_507D30_idx]._rhw = 65536.0 / (double)(signed int)v79;

            // if ( (int)v81 >= pSkyPolygon.uNumVertices )
            //{
            //  render->DrawIndoorSkyPolygon(pSkyPolygon.uNumVertices,
            //  &pSkyPolygon,
            //     pBitmaps_LOD->pHardwareTextures[(signed
            //     __int16)pSkyPolygon.uTileBitmapID]);
            //  return;
            //}
            continue;
        }
        break;
    }
    if (_507D30_idx >= pSkyPolygon.uNumVertices) {
        DrawIndoorSkyPolygon(pSkyPolygon.uNumVertices, &pSkyPolygon);
        return;
    }
    HEXRAYS_LODWORD(v73) = 0;
    v80 = v76;
    if ((signed int)pSkyPolygon.uNumVertices > 0) {
        v28 = (double)HEXRAYS_SLODWORD(v76);
        HEXRAYS_LODWORD(v76) = (int)(char *)VertexRenderList + 28;
        uint i = 0;
        for (v78 = pSkyPolygon.uNumVertices; v78; --v78) {
            ++HEXRAYS_LODWORD(v73);
            memcpy(&VertexRenderList[i], &array_507D30[i], 0x30u);
            HEXRAYS_LODWORD(v76) += 48;
            if (v28 < array_507D30[i].vWorldViewProjY |
                v28 == array_507D30[i].vWorldViewProjY ||
                v28 >= array_507D30[i + 1].vWorldViewProjY) {
                if (v28 >= array_507D30[i].vWorldViewProjY ||
                    v28 <= array_507D30[i + 1].vWorldViewProjY) {
                    i++;
                    continue;
                }
                v33 = (array_507D30[i + 1].vWorldViewProjX -
                    array_507D30[i].vWorldViewProjX) *
                    v28 /
                    (array_507D30[i + 1].vWorldViewProjY -
                        array_507D30[i].vWorldViewProjY) +
                    array_507D30[i + 1].vWorldViewProjX;
            } else {
                v33 = (array_507D30[i].vWorldViewProjX -
                    array_507D30[i + 1].vWorldViewProjX) *
                    v28 /
                    (array_507D30[i].vWorldViewProjY -
                        array_507D30[i + 1].vWorldViewProjY) +
                    array_507D30[i].vWorldViewProjX;
            }
            VertexRenderList[i + 1].vWorldViewProjX = v33;
            ++HEXRAYS_LODWORD(v73);
            *(unsigned int *)HEXRAYS_LODWORD(v76) = v28;
            HEXRAYS_LODWORD(v76) += 48;
            i++;
        }
    }
    if (HEXRAYS_SLODWORD(v73) <= 0) goto LABEL_40;
    // v34 = (char *)&VertexRenderList[0].vWorldViewProjY;
    uint j = 0;
    v65 = v77 >> 14;
    // HIDWORD(v69) = LODWORD(v73);
    for (int t = (int)HEXRAYS_LODWORD(v73); t > 1; t--) {
        v35 = (const void *)(v72 * (v70 - (unsigned __int64)(signed __int64)
            VertexRenderList[j]
            .vWorldViewProjY));

        // v78 = pSkyPolygon.ptr_38->viewing_angle_from_west_east;
        // v81 = (const void
        // *)fixpoint_mul(pSkyPolygon.ptr_38->viewing_angle_from_west_east, v35);
        v36 =
            (int)(fixpoint_mul(pSkyPolygon.ptr_38->CamVecLeft_Y,
            (int)v35) +
                pSkyPolygon.ptr_38->CamVecLeft_Z);

        v81 = v35;
        v74 = v36;
        // v78 = pSkyPolygon.ptr_38->viewing_angle_from_north_south;
        v81 = (const void *)fixpoint_mul(
            pSkyPolygon.ptr_38->CamVecFront_Y, (int)v35);
        v78 = (int)v35;
        v75 = (RenderVertexSoft *)((char *)v81 +
            pSkyPolygon.ptr_38->CamVecFront_Z);
        // v81 = (const void *)pSkyPolygon.v_18.z;
        v78 = fixpoint_mul(pSkyPolygon.v_18.z, (int)v35);
        v37 = (const void *)(v72 * (pBLVRenderParams->uViewportCenterX -
            (unsigned __int64)(signed __int64)
            VertexRenderList[j]
            .vWorldViewProjX));
        v38 = (signed __int64)(VertexRenderList[j].vWorldViewProjY - 1.0);
        v81 = 0;
        HEXRAYS_LODWORD(v76) = v38;
        v39 = v72 * (v70 - v38);
        while (1) {
            v78 = v39;
            if (!X) goto LABEL_36;
            v40 = abs(X);
            if (abs((signed __int64)v65) <= v40) break;
            if (HEXRAYS_SLODWORD(v76) <= (signed int)pViewport->uViewportTL_Y)
                break;
            v39 = v78;
        LABEL_36:
            v78 = pSkyPolygon.v_18.z;
            v41 = fixpoint_mul(pSkyPolygon.v_18.z, v39);
            --HEXRAYS_LODWORD(v76);
            v39 += v72;
            X = v41 + pSkyPolygon.v_18.x;
            v81 = (const void *)1;
        }
        if (v81) {
            v79 = (void *)pSkyPolygon.v_18.z;
            v78 = 2 * HEXRAYS_LODWORD(v76);
            v81 = (const void *)fixpoint_mul(
                pSkyPolygon.v_18.z,
                (((double)v70 - ((double)(2 * HEXRAYS_LODWORD(v76)) -
                    VertexRenderList[j].vWorldViewProjY)) *
                    (double)v72));
            X = (int)((char *)v81 + pSkyPolygon.v_18.x);
        }
        HEXRAYS_LODWORD(v42) = v77 << 16;
        HEXRAYS_HIDWORD(v42) = v77 >> 16;
        v79 = (void *)(v42 / X);
        v81 = v37;

        // v78 = pSkyPolygon.ptr_38->angle_from_west;
        v81 = (const void *)fixpoint_mul(pSkyPolygon.ptr_38->CamVecLeft_X,
            (int)v37);
        v43 = v74 + fixpoint_mul(pSkyPolygon.ptr_38->CamVecLeft_X, (int)v37);
        v74 = (unsigned int)v37;
        HEXRAYS_LODWORD(v76) = v43;

        // v78 = pSkyPolygon.ptr_38->angle_from_south;
        v75 = (RenderVertexSoft *)((char *)v75 +
            fixpoint_mul(
                pSkyPolygon.ptr_38->CamVecFront_X,
                (int)v37));
        // v74 = fixpoint_mul(v43, v42 / X);
        v81 = (const void *)fixpoint_mul((int)v75, v42 / X);

        // v34 += 48;
        // v78 = v66 + ((signed int)fixpoint_mul(v43, v42 / X) >> 4);
        // v44 = HIDWORD(v69)-- == 1;
        // v45 = (double)(v66 + ((signed int)fixpoint_mul(v43, v42 / X) >> 4)) *
        // 0.000015259022; v78 = v63 + ((signed int)fixpoint_mul((int)v75, v42 /
        // X) >> 4);
        VertexRenderList[j].u =
            ((double)(v66 + ((signed int)fixpoint_mul(v43, v42 / X) >> 4)) *
                0.000015259022) *
                (1.0 / (double)pSkyPolygon.texture->GetWidth());
        VertexRenderList[j].v =
            ((double)(v66 + ((signed int)fixpoint_mul(v43, v42 / X) >> 4)) *
                0.000015259022) *
            v68;
        // v46 = (double)(signed int)v79;
        VertexRenderList[j].vWorldViewPosition.x =
            0.000015258789 * (double)(signed int)v79;
        VertexRenderList[j]._rhw = 65536.0 / (double)(signed int)v79;
        ++j;
    }
    // while ( !v44 );
LABEL_40:
    uint i = 0;
    if (HEXRAYS_SLODWORD(v73) > 0) {
        v48 = (double)HEXRAYS_SLODWORD(v80);
        for (HEXRAYS_HIDWORD(v69) = HEXRAYS_LODWORD(v73); HEXRAYS_HIDWORD(v69);
            --HEXRAYS_HIDWORD(v69)) {
            if (v48 >= VertexRenderList[i].vWorldViewProjY) {
                ++i;
                memcpy(&array_507D30[i], &VertexRenderList[i], 0x30u);
            }
        }
    }
    pSkyPolygon.uNumVertices = i;
    DrawIndoorSkyPolygon(pSkyPolygon.uNumVertices, &pSkyPolygon);
    int pNumVertices = 0;
    if (HEXRAYS_SLODWORD(v73) > 0) {
        v51 = (double)HEXRAYS_SLODWORD(v80);
        for (v80 = v73; v80 != 0.0; --HEXRAYS_LODWORD(v80)) {
            if (v51 <= VertexRenderList[pNumVertices].vWorldViewProjY) {
                ++pNumVertices;
                memcpy(&array_507D30[pNumVertices],
                    &VertexRenderList[pNumVertices], 0x30u);
            }
        }
    }
    DrawIndoorSkyPolygon(pNumVertices, &pSkyPolygon);
}

void Render::DrawOutdoorSkyPolygon(struct Polygon *pSkyPolygon) {
    if (uNumD3DSceneBegins == 0) {
        return;
    }

    unsigned int uNumVertices = pSkyPolygon->uNumVertices;
    TextureD3D *texture = (TextureD3D *)pSkyPolygon->texture;

    if (uNumVertices >= 3) {
        this->pRenderD3D->pDevice->SetTextureStageState(0, D3DTSS_ADDRESS,
            D3DTADDRESS_WRAP);
        if (config->is_using_specular) {
            this->pRenderD3D->pDevice->SetRenderState(D3DRENDERSTATE_ALPHABLENDENABLE, TRUE);
            this->pRenderD3D->pDevice->SetRenderState(D3DRENDERSTATE_SRCBLEND, D3DBLEND_ONE);
            this->pRenderD3D->pDevice->SetRenderState(D3DRENDERSTATE_DESTBLEND, D3DBLEND_ZERO);
        }
        for (unsigned int i = 0; i < uNumVertices; ++i) {
            pVertices[i].pos.x = VertexRenderList[i].vWorldViewProjX;
            pVertices[i].pos.y = VertexRenderList[i].vWorldViewProjY;
            pVertices[i].pos.z = 0.99989998;
            pVertices[i].rhw = VertexRenderList[i]._rhw;

            pVertices[i].diffuse = ::GetActorTintColor(31, 0, VertexRenderList[i].vWorldViewPosition.x, 1, 0);
            int v7 = 0;
            if (config->is_using_specular)
                v7 = sub_47C3D7_get_fog_specular(0, 1, VertexRenderList[i].vWorldViewPosition.x);
            pVertices[i].specular = v7;
            pVertices[i].texcoord.x = VertexRenderList[i].u;
            pVertices[i].texcoord.y = VertexRenderList[i].v;
        }
        pRenderD3D->pDevice->SetTexture(0, texture->GetDirect3DTexture());
        pRenderD3D->pDevice->DrawPrimitive(
            D3DPT_TRIANGLEFAN,
            D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_SPECULAR | D3DFVF_TEX1,
            pVertices, uNumVertices,
            D3DDP_DONOTUPDATEEXTENTS | D3DDP_DONOTLIGHT);
    }
}

void Render::DrawIndoorSkyPolygon(int uNumVertices, struct Polygon *pSkyPolygon) {
    TextureD3D *texture = (TextureD3D *)pSkyPolygon->texture;

    if (uNumD3DSceneBegins == 0) {
        return;
    }

    if (uNumVertices >= 3) {
        ErrD3D(pRenderD3D->pDevice->SetTextureStageState(0, D3DTSS_ADDRESS,
            D3DTADDRESS_WRAP));
        int v5 = 31 - (pSkyPolygon->dimming_level & 0x1F);
        if (v5 < pOutdoor->max_terrain_dimming_level) {
            v5 = pOutdoor->max_terrain_dimming_level;
        }
        for (uint i = 0; i < (unsigned int)uNumVertices; ++i) {
            d3d_vertex_buffer[i].pos.x = array_507D30[i].vWorldViewProjX;
            d3d_vertex_buffer[i].pos.y = array_507D30[i].vWorldViewProjY;
            d3d_vertex_buffer[i].pos.z =
                1.0 -
                1.0 / (array_507D30[i].vWorldViewPosition.x * 0.061758894);
            d3d_vertex_buffer[i].rhw = array_507D30[i]._rhw;
            d3d_vertex_buffer[i].diffuse =
                8 * v5 | ((8 * v5 | (v5 << 11)) << 8);
            d3d_vertex_buffer[i].specular = 0;
            d3d_vertex_buffer[i].texcoord.x = array_507D30[i].u;
            d3d_vertex_buffer[i].texcoord.y = array_507D30[i].v;
        }
        ErrD3D(
            pRenderD3D->pDevice->SetTexture(0, texture->GetDirect3DTexture()));
        ErrD3D(pRenderD3D->pDevice->DrawPrimitive(
            D3DPT_TRIANGLEFAN,
            D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_SPECULAR | D3DFVF_TEX1,
            d3d_vertex_buffer, uNumVertices, 28));
    }
}
