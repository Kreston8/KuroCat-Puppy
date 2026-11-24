// Offline preview harness for KuroCat's Windows renderer.
// Renders a contact sheet of interactive states straight from CatRenderer,
// so artwork changes can be reviewed without launching the desktop pet.
//
//   preview.exe <output.png> [coatIndex]
//
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <objidl.h>
#include <gdiplus.h>
#include <cstdio>
#include <string>
#include <vector>

#include "../../src/CatRenderer.hpp"

using namespace Gdiplus;

static int getEncoderClsid(const WCHAR* mime, CLSID* clsid) {
    UINT num = 0, size = 0;
    GetImageEncodersSize(&num, &size);
    if (size == 0) return -1;
    std::vector<BYTE> buf(size);
    auto* codecs = reinterpret_cast<ImageCodecInfo*>(buf.data());
    GetImageEncoders(num, size, codecs);
    for (UINT i = 0; i < num; ++i) {
        if (wcscmp(codecs[i].MimeType, mime) == 0) { *clsid = codecs[i].Clsid; return (int)i; }
    }
    return -1;
}

struct Scene {
    const wchar_t* title;
    const char* subtitle;
    void (*setup)(CatPhysics&);
};

static void sceneIdle(CatPhysics& p) {
    p.time = 0.35f;
    p.tailPhase = 0.9f;
    p.earFold.value = 0.12f;
}

static void sceneLookUp(CatPhysics& p) {
    p.time = 1.1f;
    p.tailPhase = 1.9f;
    p.headYaw.value = 0.62f;
    p.headPitch.value = -0.42f;
    p.headTilt.value = -0.06f;
    p.eyeGaze.x.value = 3.2f;
    p.eyeGaze.y.value = -3.0f;
    p.earFold.value = 0.05f;
}

static void sceneLookDown(CatPhysics& p) {
    p.time = 2.0f;
    p.tailPhase = 2.7f;
    p.headYaw.value = -0.58f;
    p.headPitch.value = 0.46f;
    p.headTilt.value = 0.07f;
    p.eyeGaze.x.value = -3.0f;
    p.eyeGaze.y.value = 3.2f;
}

static void sceneHunt(CatPhysics& p) {
    p.time = 3.2f;
    p.tailPhase = 3.6f;
    p.huntIntensity.value = 1.0f;
    p.pupilScale.value = 1.22f;
    p.headYaw.value = -0.30f;
    p.headPitch.value = 0.34f;
    p.headOffsetY.value = 5.0f;
    p.bodyTilt.value = 0.16f;
    p.earFold.value = -0.10f;
}

static void sceneTyping(CatPhysics& p) {
    p.time = 4.4f;
    p.tailPhase = 4.8f;
    p.leftPawTyping = true;
    p.rightPawTyping = true;
    p.leftPaw.x.value = 150.0f;  p.leftPaw.y.value = 252.0f;
    p.rightPaw.x.value = 206.0f; p.rightPaw.y.value = 250.0f;
    p.hindFootActive.value = 1.0f;
    p.hindFoot.x.value = 214.0f; p.hindFoot.y.value = 238.0f;
    p.headPitch.value = 0.22f;
    p.bodySquash.value = 0.97f;
    p.earTwitch.value = 0.35f;
}

static void scenePetting(CatPhysics& p) {
    p.time = 5.6f;
    p.tailPhase = 5.9f;
    p.petHeadIntensity = 0.92f;
    p.headTilt.value = 0.13f;
    p.headOffsetY.value = 2.0f;
    p.earFold.value = 0.55f;
    p.tailCurl.value = 8.0f;
}

static bool renderSheet(const wchar_t* outPath, int coatIndex) {

    const int PW = (int)CatPhysics::CanvasWidth;   // 360
    const int PH = (int)CatPhysics::CanvasHeight;  // 320
    const int SS = 2;                              // supersample for crisp preview
    const int pad = 26, headerH = 108, captionH = 52;

    std::vector<Scene> scenes = {
        { L"待机",       "idle",              sceneIdle },
        { L"抬头张望",   "look up-left",      sceneLookUp },
        { L"低头好奇",   "look down-right",   sceneLookDown },
        { L"捕猎锁定",   "hunting the cursor", sceneHunt },
        { L"打字 + 第三只爪", "typing + hind paw", sceneTyping },
        { L"摸头闭眼",   "being petted",      scenePetting },
    };

    const int cols = 3, rows = 2;
    const int cellW = PW * SS, cellH = PH * SS;
    const int sheetW = pad + cols * (cellW + pad);
    const int sheetH = headerH + rows * (cellH + captionH + pad) + pad;

    Bitmap sheet(sheetW, sheetH, PixelFormat32bppARGB);
    Graphics sg(&sheet);
    sg.SetSmoothingMode(SmoothingModeAntiAlias);
    sg.SetTextRenderingHint(TextRenderingHintAntiAlias);
    {
        LinearGradientBrush bg(Point(0, 0), Point(0, sheetH), Color(255, 250, 246, 238), Color(255, 234, 222, 203));
        sg.FillRectangle(&bg, 0, 0, sheetW, sheetH);
    }

    FontFamily fam(L"Microsoft YaHei");
    FontFamily famLatin(L"Segoe UI");
    Font titleFont(&fam, 30.0f, FontStyleBold, UnitPixel);
    Font subFont(&famLatin, 18.0f, FontStyleRegular, UnitPixel);
    Font capFont(&fam, 20.0f, FontStyleBold, UnitPixel);
    Font capSubFont(&famLatin, 14.0f, FontStyleRegular, UnitPixel);
    SolidBrush ink(Color(255, 74, 58, 44));
    SolidBrush dim(Color(255, 138, 120, 100));

    const wchar_t* coatName = L"小金毛 (Golden Retriever Puppy)";
    switch ((CatCoatTheme)coatIndex) {
        case CatCoatTheme::ClassicBlack: coatName = L"经典小黑猫"; break;
        case CatCoatTheme::Siamese:      coatName = L"暹罗猫"; break;
        case CatCoatTheme::Shorthair:    coatName = L"美短"; break;
        case CatCoatTheme::OrangeWhite:  coatName = L"橘白猫"; break;
        default: break;
    }

    sg.DrawString(L"KuroCat 桌宠 · 皮肤预览", -1, &titleFont, PointF((REAL)pad, 28.0f), &ink);
    std::wstring sub = std::wstring(L"皮肤：") + coatName +
                       L"      眼睛：淡金琥珀      真机渲染 · CatRenderer 输出 (2x)";
    sg.DrawString(sub.c_str(), -1, &subFont, PointF((REAL)pad, 70.0f), &dim);

    CatSettings settings;
    settings.coatTheme = (CatCoatTheme)coatIndex;
    settings.eyeTheme = EyeColorTheme::ReferenceHazel;
    settings.keyboardTheme = KeyboardTheme::ClassicMilk;
    settings.showKeyLegends = true;
    settings.keyboardMarquee = true;

    for (size_t i = 0; i < scenes.size(); ++i) {
        const int col = (int)i % cols, row = (int)i / cols;
        const int ox = pad + col * (cellW + pad);
        const int oy = headerH + row * (cellH + captionH + pad);

        Bitmap frame(cellW, cellH, PixelFormat32bppPARGB);
        Graphics fg(&frame);
        fg.Clear(Color(0, 0, 0, 0));
        fg.SetSmoothingMode(SmoothingModeAntiAlias);
        fg.SetPixelOffsetMode(PixelOffsetModeHighQuality);
        fg.ScaleTransform((REAL)SS, (REAL)SS);

        CatPhysics physics;
        physics.eyeGaze.x.value = 0.0f;
        physics.eyeGaze.y.value = 0.0f;
        scenes[i].setup(physics);
        PuppySkin::get().snap(physics);   // 预览直接跳帧，不做过场淡化
        CatRenderer::draw(fg, physics, settings);

        // soft card behind the sprite
        GraphicsPath card;
        float r = 22.0f;
        RectF box((REAL)ox, (REAL)oy, (REAL)cellW, (REAL)cellH);
        card.AddArc(box.X, box.Y, r * 2, r * 2, 180, 90);
        card.AddArc(box.GetRight() - r * 2, box.Y, r * 2, r * 2, 270, 90);
        card.AddArc(box.GetRight() - r * 2, box.GetBottom() - r * 2, r * 2, r * 2, 0, 90);
        card.AddArc(box.X, box.GetBottom() - r * 2, r * 2, r * 2, 90, 90);
        card.CloseFigure();
        SolidBrush cardFill(Color(255, 255, 252, 246));
        sg.FillPath(&cardFill, &card);
        Pen cardEdge(Color(90, 200, 178, 146), 1.2f);
        sg.DrawPath(&cardEdge, &card);

        sg.DrawImage(&frame, ox, oy, cellW, cellH);

        sg.DrawString(scenes[i].title, -1, &capFont, PointF((REAL)(ox + 6), (REAL)(oy + cellH + 8)), &ink);
        const size_t subLen = strlen(scenes[i].subtitle);
        std::wstring st(subLen, L' ');
        if (subLen) std::mbstowcs(&st[0], scenes[i].subtitle, subLen);
        sg.DrawString(st.c_str(), -1, &capSubFont, PointF((REAL)(ox + 6), (REAL)(oy + cellH + 30)), &dim);
    }

    CLSID pngClsid;
    if (getEncoderClsid(L"image/png", &pngClsid) < 0) return false;
    Status st = sheet.Save(outPath, &pngClsid, nullptr);
    if (st != Ok) {
        std::fwprintf(stderr, L"save failed: %d\n", (int)st);
        return false;
    }
    std::wprintf(L"saved: %s (%dx%d)\n", outPath, sheetW, sheetH);
    return true;
}
// 3x3 朝向矩阵：用来核对 9 张朝向帧的身体位置和大小是否一致
static bool renderGrid(const wchar_t* outPath, int coatIndex) {
    const int PW = (int)CatPhysics::CanvasWidth;
    const int PH = (int)CatPhysics::CanvasHeight;
    const int pad = 10, captionH = 20;
    const int cols = 3, rows = 3;
    const int cellW = PW, cellH = PH;
    const int sheetW = pad + cols * (cellW + pad);
    const int sheetH = pad + rows * (cellH + captionH + pad);

    Bitmap sheet(sheetW, sheetH, PixelFormat32bppARGB);
    Graphics sg(&sheet);
    sg.SetSmoothingMode(SmoothingModeAntiAlias);
    SolidBrush bg(Color(255, 248, 240, 228));
    sg.FillRectangle(&bg, 0, 0, sheetW, sheetH);

    FontFamily famLatin(L"Segoe UI");
    Font capFont(&famLatin, 13.0f, FontStyleBold, UnitPixel);
    SolidBrush ink(Color(255, 74, 58, 44));

    CatSettings settings;
    settings.coatTheme = (CatCoatTheme)coatIndex;
    settings.eyeTheme = EyeColorTheme::ReferenceHazel;
    settings.keyboardTheme = KeyboardTheme::ClassicMilk;
    settings.showKeyLegends = true;
    settings.keyboardMarquee = true;

    const wchar_t* names[3][3] = {
        { L"L.up",   L"L.mid",   L"L.down"   },
        { L"C.up",   L"C.mid",   L"C.down"   },
        { L"R.up",   L"R.mid",   L"R.down"   },
    };

    for (int c = 0; c < 3; ++c) {
        for (int r = 0; r < 3; ++r) {
            const int ox = pad + c * (cellW + pad);
            const int oy = pad + r * (cellH + captionH + pad);
            Bitmap frame(cellW, cellH, PixelFormat32bppPARGB);
            Graphics fg(&frame);
            fg.Clear(Color(0, 0, 0, 0));
            fg.SetSmoothingMode(SmoothingModeAntiAlias);
            fg.SetPixelOffsetMode(PixelOffsetModeHighQuality);

            CatPhysics physics;
            physics.time = 0.5f;
            physics.tailPhase = 0.9f;
            physics.headYaw.value = (c - 1) * 0.9f;
            physics.headPitch.value = (r - 1) * 0.9f;
            // 关掉侧倾干扰，纯看身体对不对得齐
            physics.headTilt.value = 0.0f;
            physics.bodyTilt.value = 0.0f;
            PuppySkin::get().snap(physics);
            CatRenderer::draw(fg, physics, settings);

            sg.DrawImage(&frame, ox, oy, cellW, cellH);
            sg.DrawString(names[c][r], -1, &capFont, PointF((REAL)(ox + 4), (REAL)(oy + cellH + 3)), &ink);
        }
    }

    CLSID pngClsid;
    if (getEncoderClsid(L"image/png", &pngClsid) < 0) return false;
    Status st = sheet.Save(outPath, &pngClsid, nullptr);
    if (st != Ok) { std::fwprintf(stderr, L"save failed: %d\n", (int)st); return false; }
    std::wprintf(L"saved: %s (%dx%d)\n", outPath, sheetW, sheetH);
    return true;
}


int wmain(int argc, wchar_t** argv) {
    if (argc < 2) {
        std::fwprintf(stderr, L"usage: preview.exe <out.png> [coatIndex]\n");
        return 2;
    }
    const bool grid = (argc > 2 && wcscmp(argv[2], L"grid") == 0);
    const int coatIndex = grid ? (int)CatCoatTheme::GoldenPuppy
                               : ((argc > 2) ? _wtoi(argv[2]) : (int)CatCoatTheme::GoldenPuppy);
    GdiplusStartupInput gsi;
    ULONG_PTR token = 0;
    if (GdiplusStartup(&token, &gsi, nullptr) != Ok) return 1;
    const bool ok = grid ? renderGrid(argv[1], coatIndex) : renderSheet(argv[1], coatIndex);
    PuppySkin::get().shutdown();
    GdiplusShutdown(token);
    return ok ? 0 : 1;
}
