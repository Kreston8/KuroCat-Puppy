#pragma once
#ifndef PUPPY_SKIN_HPP
#define PUPPY_SKIN_HPP
// 小金毛贴图渲染
//  - 头部朝向用 3x3 网格（左/中/右 x 抬/平/低），按 headYaw / headPitch 做双线性混合，
//    所以头会连续地朝鼠标方向转，而不是硬跳帧。
//  - 打字 / 摸头 / 捕猎 / 犯困 用 4 个特殊帧。
//  - 前爪用程序化画法：目标点直接取 CatPhysics 的 leftPaw / rightPaw（已经指向鼠标或按下的键），
//    只在离位时淡入，所以静止时看到的就是画里的爪子。
// 优先读 exe 目录下 skin\*.png（可换图不重编译），否则回退 exe 内嵌 RCDATA。

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <objidl.h>
#include <gdiplus.h>
#include <shlwapi.h>
#include <algorithm>
#include <cmath>
#include <string>

#include "CatPhysics.hpp"
#include "PuppySkinRes.h"

class PuppySkin {
public:
    // 待机帧内容高度（画布像素），其余帧同比例缩放
    static constexpr float kRefHeight = 170.0f;
    // 贴图底边落点：压在键盘后沿
    static constexpr float kFootY = 241.0f;
    static constexpr float kFootXOffset = 6.0f;
    static constexpr float kFadeSeconds = 0.17f;
    // 程序化前爪：贴图里本来就画好了两只前爪，再叠手臂会变成"四条腿"，
    // 所以默认关闭；只有在有专门的"伸爪"帧时才打开。
    static constexpr bool kDrawPaws = false;
    // 打字帧额外下压量（画布像素）：原图前爪刚够到键盘后沿，压一点才是"趴在键盘上"
    static constexpr float kTypeDrop = 21.0f;

    // 网格：列 = 头朝左/中/右，行 = 抬头/平视/低头
    enum { G_L = 0, G_C = 1, G_R = 2 };
    enum { R_UP = 0, R_MID = 1, R_DN = 2 };
    // 特殊姿态
    enum { S_HAPPY = 0, S_HUNT = 1, S_TYPE = 2, S_BLINK = 3, S_COUNT = 4 };

    static PuppySkin& get() {
        static PuppySkin s;
        return s;
    }

    bool ready() {
        return ensureLoaded();
    }

    void update(const CatPhysics& p) {
        if (!ensureLoaded()) return;
        float dt = (lastTime_ > 0.0f) ? (p.time - lastTime_) : 0.016f;
        lastTime_ = p.time;
        dt = std::min(std::max(dt, 0.0f), 0.1f);

        State want = targetState(p);
        if (!sameState(want, cur_)) {
            prev_ = cur_;
            cur_ = want;
            fade_ = 0.0f;
        }
        fade_ = std::min(1.0f, fade_ + dt / kFadeSeconds);
    }

    void draw(Gdiplus::Graphics& g, const CatPhysics& p, float seatX) {
        if (!ensureLoaded()) return;

        const float footX = seatX + kFootXOffset;
        const float breath = std::sin(p.time * 2.1f) * 0.9f;
        const float footY = kFootY + breath * 0.5f;

        // 朝向：身体侧倾 + 头部倾斜，再叠一点"朝鼠标那侧歪身"的量。
        // 没有左右转头素材，左右方向只能用轻微倾身来体现（不会挡住鼠标）。
        float rot = (p.bodyTilt.value * 5.0f + p.headTilt.value * 2.0f + p.headYaw.value * 1.7f);
        rot = std::max(-3.4f, std::min(3.4f, rot));

        // 打字姿势整只狗趴在键盘上，地面阴影会变成按键上的一块脏印，这时不画
        if (cur_.special != S_TYPE) drawShadow(g, footX, footY, (float)grid_[G_C][R_MID].w);
        if (kDrawPaws) drawFrontPaws(g, p, footX, footY);

        if (fade_ < 1.0f && !sameState(prev_, cur_)) {
            drawState(g, prev_, 1.0f - fade_, footX, footY, rot);
        }
        drawState(g, cur_, 1.0f, footX, footY, rot);
    }

    void shutdown() {
        for (int c = 0; c < 3; ++c)
            for (int r = 0; r < 3; ++r) releaseFrame(grid_[c][r]);
        for (int i = 0; i < S_COUNT; ++i) releaseFrame(spec_[i]);
        loaded_ = false;
        ok_ = false;
        cur_ = prev_ = State();
        fade_ = 1.0f;
    }

    // 预览/截图用：直接跳到目标状态，不做过渡
    void snap(const CatPhysics& p) {
        if (!ensureLoaded()) return;
        cur_ = prev_ = targetState(p);
        fade_ = 1.0f;
        lastTime_ = p.time;
    }

    void setDirOverride(const std::wstring& dir) { dirOverride_ = dir; loaded_ = false; }

private:
    struct Frame {
        Gdiplus::Bitmap* bmp = nullptr;
        IStream* stream = nullptr;   // 资源内存流，Bitmap 活着期间必须保留
        int w = 0, h = 0;            // 裁切缩放后的成品尺寸
        bool own = true;             // 共享帧不重复释放
        float offX = 0.0f, offY = 0.0f;   // 相对落点(脚底中心)的绘制偏移
        float yBias = 0.0f;          // 额外的竖直微调（画布像素，正 = 往下）
    };
    struct State {
        int special = -1;            // >=0 用特殊帧，-1 用头部朝向网格
        int c = 1;                   // 网格列：0 朝左 / 1 朝前 / 2 朝右
        int r = 1;                   // 网格行：0 抬头 / 1 平视 / 2 低头
    };

    Frame grid_[3][3];
    Frame spec_[S_COUNT];
    State cur_, prev_;
    bool loaded_ = false;
    bool ok_ = false;
    float fade_ = 1.0f;
    float lastTime_ = 0.0f;
    float scale_ = 1.0f;
    std::wstring dirOverride_;

    // MARK: - 状态选择
    static int pickSpecial(const CatPhysics& p) {
        if (p.huntIntensity.value > 0.30f) return S_HUNT;
        if (p.isHappyClosedEyes() || p.isBlissfulSquint()) return S_HAPPY;
        if (p.leftPawTyping || p.rightPawTyping || p.tailTyping.value > 0.5f) return S_TYPE;
        if (p.sleepiness > 0.35f) return S_BLINK;
        return -1;
    }

    // MARK: - 文件 / 资源表
    static const wchar_t* gridFile(int c, int r) {
        static const wchar_t* names[3][3] = {
            { L"gaze_l_up.png", L"gaze_l_mid.png", L"gaze_l_down.png" },
            { L"look_up.png",   L"idle.png",       L"look_down.png"   },
            { L"gaze_r_up.png", L"gaze_r_mid.png", L"gaze_r_down.png" }
        };
        return names[c][r];
    }
    static const wchar_t* specFile(int s) {
        static const wchar_t* names[S_COUNT] = {
            L"happy.png", L"hunt.png", L"type.png", L"blink.png"
        };
        return names[s];
    }
    static int gridResId(int c, int r) {
        static const int ids[3][3] = {
            { IDR_PUPPY_GAZE_L_UP, IDR_PUPPY_GAZE_L_MID, IDR_PUPPY_GAZE_L_DOWN },
            { IDR_PUPPY_LOOKUP,    IDR_PUPPY_IDLE,        IDR_PUPPY_LOOKDOWN      },
            { IDR_PUPPY_GAZE_R_UP, IDR_PUPPY_GAZE_R_MID,  IDR_PUPPY_GAZE_R_DOWN  }
        };
        return ids[c][r];
    }
    static int specResId(int s) {
        static const int ids[S_COUNT] = {
            IDR_PUPPY_HAPPY, IDR_PUPPY_HUNT, IDR_PUPPY_TYPE, IDR_PUPPY_BLINK
        };
        return ids[s];
    }

    std::wstring skinDir() const {
        if (!dirOverride_.empty()) {
            std::wstring d = dirOverride_;
            if (!d.empty() && d.back() != L'\\' && d.back() != L'/') d += L'\\';
            return d;
        }
        wchar_t buf[MAX_PATH] = {};
        DWORD n = GetModuleFileNameW(nullptr, buf, MAX_PATH);
        std::wstring path(buf, n);
        size_t pos = path.find_last_of(L"\\/");
        path = (pos != std::wstring::npos) ? path.substr(0, pos + 1) : std::wstring();
        return path + L"skin\\";
    }

    // MARK: - 加载
    bool ensureLoaded() {
        if (loaded_) return ok_;
        loaded_ = true;

        const std::wstring dir = skinDir();
        HINSTANCE hInst = GetModuleHandleW(nullptr);

        // 1) 解出所有 13 帧并求内容包围盒
        struct Pending { Gdiplus::Bitmap* bmp; IStream* stream; Gdiplus::Rect box; bool ok; };
        Pending pend[13];
        for (int i = 0; i < 13; ++i) { pend[i].bmp = nullptr; pend[i].stream = nullptr; pend[i].ok = false; }

        for (int c = 0; c < 3; ++c) {
            for (int r = 0; r < 3; ++r) {
                int idx = c * 3 + r;
                Gdiplus::Bitmap* b = loadFromFile(dir + gridFile(c, r));
                IStream* st = nullptr;
                if (!b) b = loadFromResource(hInst, gridResId(c, r), &st);
                if (!b) continue;
                Gdiplus::Rect box;
                if (!contentBox(b, box)) { delete b; if (st) st->Release(); continue; }
                pend[idx].bmp = b; pend[idx].stream = st; pend[idx].box = box; pend[idx].ok = true;
            }
        }
        for (int s = 0; s < S_COUNT; ++s) {
            int idx = 9 + s;
            Gdiplus::Bitmap* b = loadFromFile(dir + specFile(s));
            IStream* st = nullptr;
            if (!b) b = loadFromResource(hInst, specResId(s), &st);
            if (!b) continue;
            Gdiplus::Rect box;
            if (!contentBox(b, box)) { delete b; if (st) st->Release(); continue; }
            pend[idx].bmp = b; pend[idx].stream = st; pend[idx].box = box; pend[idx].ok = true;
        }

        const int MID = G_C * 3 + R_MID;
        if (!pend[MID].ok) {                       // 没有正中待机帧就整体放弃，回退矢量美术
            for (int i = 0; i < 13; ++i) {
                if (pend[i].bmp) delete pend[i].bmp;
                if (pend[i].stream) pend[i].stream->Release();
            }
            ok_ = false;
            return false;
        }

        // 2) 统一缩放：以正中待机帧的内容高度为基准
        scale_ = kRefHeight / (float)pend[MID].box.Height;

        // 3) 网格帧共用同一个裁切矩形（并集），保证头部转动时身体不会跟着漂
        Gdiplus::Rect uni = pend[MID].box;
        for (int c = 0; c < 3; ++c) {
            for (int r = 0; r < 3; ++r) {
                int idx = c * 3 + r;
                if (!pend[idx].ok) continue;
                const Gdiplus::Rect& b = pend[idx].box;
                int x0 = std::min(uni.X, b.X), y0 = std::min(uni.Y, b.Y);
                int x1 = std::max(uni.X + uni.Width, b.X + b.Width);
                int y1 = std::max(uni.Y + uni.Height, b.Y + b.Height);
                uni = Gdiplus::Rect(x0, y0, x1 - x0, y1 - y0);
            }
        }

        // 锚点：待机帧的脚底中心在并集框里的位置（所有网格帧共用，保证身体不漂）
        const float gax = (pend[MID].box.X + pend[MID].box.Width * 0.5f) - uni.X;
        const float gay = (pend[MID].box.Y + pend[MID].box.Height) - uni.Y;
        // 4) 裁切 + 缩放
        for (int c = 0; c < 3; ++c) {
            for (int r = 0; r < 3; ++r) {
                int idx = c * 3 + r;
                if (!pend[idx].ok) continue;
                buildFrame(grid_[c][r], pend[idx].bmp, pend[idx].stream, uni, gax, gay);
            }
        }
        for (int s = 0; s < S_COUNT; ++s) {
            int idx = 9 + s;
            if (!pend[idx].ok) continue;
            buildFrame(spec_[s], pend[idx].bmp, pend[idx].stream, pend[idx].box,
                       pend[idx].box.Width * 0.5f, (float)pend[idx].box.Height);
        }
        for (int i = 0; i < 13; ++i) {   // 源图用完即弃（成品是新的位图）
            if (pend[i].bmp) delete pend[i].bmp;
            if (pend[i].stream) pend[i].stream->Release();
            pend[i].bmp = nullptr; pend[i].stream = nullptr;
        }

        // 5) 缺帧兜底：先补中列，再补两侧（同列中排 → 中列同排 → 正中待机），
        //    这样只给 gaze_l_mid / gaze_r_mid 两张也能让头左右转起来。
        for (int r = 0; r < 3; ++r) {
            if (!grid_[G_C][r].bmp) shareFrame(grid_[G_C][r], grid_[G_C][R_MID]);
        }
        for (int c = 0; c < 3; c += 2) {
            for (int r = 0; r < 3; ++r) {
                if (grid_[c][r].bmp) continue;
                if (grid_[c][R_MID].bmp) shareFrame(grid_[c][r], grid_[c][R_MID]);
                else shareFrame(grid_[c][r], grid_[G_C][r]);
            }
        }
        for (int s = 0; s < S_COUNT; ++s) {
            if (!spec_[s].bmp) shareFrame(spec_[s], grid_[G_C][R_MID]);
        }
        // 打字姿势：下压一点，让前爪实实在在压在键帽上（只对自有帧生效，兜底共享帧不动）
        if (spec_[S_TYPE].bmp && spec_[S_TYPE].own) spec_[S_TYPE].yBias = kTypeDrop;

        ok_ = grid_[G_C][R_MID].bmp != nullptr;
        cur_ = prev_ = State();
        fade_ = 1.0f;
        return ok_;
    }

    static void releaseFrame(Frame& f) {
        if (f.own && f.bmp) delete f.bmp;
        f.bmp = nullptr;
        f.w = f.h = 0;
        f.own = true;
    }

    static void shareFrame(Frame& dst, const Frame& src) {
        dst.bmp = src.bmp;
        dst.w = src.w;
        dst.h = src.h;
        dst.offX = src.offX;
        dst.offY = src.offY;
        dst.stream = nullptr;
        dst.own = false;
    }

    // anchorX/anchorY：脚底中心在 box 内的像素坐标。落点按这个锚算，帧的外框变大也不会把身体带偏。
    void buildFrame(Frame& dst, Gdiplus::Bitmap* src, IStream* stream, const Gdiplus::Rect& box,
                    float anchorX, float anchorY) {
        int dw = std::max(1, (int)std::lround(box.Width * scale_));
        int dh = std::max(1, (int)std::lround(box.Height * scale_));
        Gdiplus::Bitmap* out = cropAndResize(src, box, dw, dh);
        if (!out) return;
        dst.bmp = out;
        dst.stream = stream;
        dst.w = dw;
        dst.h = dh;
        // 取整，避免每帧落在半像素上被双线性插值糊掉
        dst.offX = (float)std::lround(-anchorX * scale_);
        dst.offY = (float)std::lround(-anchorY * scale_);
        dst.own = true;
    }

    static Gdiplus::Bitmap* loadFromFile(const std::wstring& path) {
        if (GetFileAttributesW(path.c_str()) == INVALID_FILE_ATTRIBUTES) return nullptr;
        Gdiplus::Bitmap* b = Gdiplus::Bitmap::FromFile(path.c_str(), FALSE);
        if (!b) return nullptr;
        if (b->GetLastStatus() != Gdiplus::Ok) { delete b; return nullptr; }
        return b;
    }

    static Gdiplus::Bitmap* loadFromResource(HINSTANCE hInst, int id, IStream** outStream) {
        if (!id) return nullptr;
        HRSRC res = FindResourceW(hInst, MAKEINTRESOURCEW(id), MAKEINTRESOURCEW(10 /*RT_RCDATA*/));
        if (!res) return nullptr;
        DWORD size = SizeofResource(hInst, res);
        if (!size) return nullptr;
        HGLOBAL handle = LoadResource(hInst, res);
        if (!handle) return nullptr;
        const void* data = LockResource(handle);
        if (!data) return nullptr;

        IStream* stream = SHCreateMemStream((const BYTE*)data, size);
        if (!stream) return nullptr;
        Gdiplus::Bitmap* b = Gdiplus::Bitmap::FromStream(stream, FALSE);
        if (!b || b->GetLastStatus() != Gdiplus::Ok) {
            delete b;
            stream->Release();
            return nullptr;
        }
        if (outStream) *outStream = stream;
        else stream->Release();
        return b;
    }

    // 扫 alpha 求内容包围盒
    static bool contentBox(Gdiplus::Bitmap* bmp, Gdiplus::Rect& out) {
        const int w = (int)bmp->GetWidth(), h = (int)bmp->GetHeight();
        if (w <= 0 || h <= 0) return false;

        Gdiplus::Rect lockRect(0, 0, w, h);
        Gdiplus::BitmapData bd;
        if (bmp->LockBits(&lockRect, Gdiplus::ImageLockModeRead,
                          PixelFormat32bppARGB, &bd) != Gdiplus::Ok) return false;

        const bool bottomUp = (bd.Stride < 0);
        const int stride = bottomUp ? -bd.Stride : bd.Stride;
        const BYTE* base = (const BYTE*)bd.Scan0;
        int minX = w, minY = h, maxX = -1, maxY = -1;
        for (int y = 0; y < h; ++y) {
            const BYTE* row = base + (size_t)(bottomUp ? (h - 1 - y) : y) * (size_t)stride;
            for (int x = 0; x < w; ++x) {
                if (row[x * 4 + 3] > 8) {
                    if (x < minX) minX = x;
                    if (x > maxX) maxX = x;
                    if (y < minY) minY = y;
                    if (y > maxY) maxY = y;
                }
            }
        }
        bmp->UnlockBits(&bd);
        if (maxX < minX || maxY < minY) return false;
        out = Gdiplus::Rect(minX, minY, maxX - minX + 1, maxY - minY + 1);
        return true;
    }

    // 裁到指定矩形 → 连续 2x 降采样 → 精确缩到目标尺寸
    static Gdiplus::Bitmap* cropAndResize(Gdiplus::Bitmap* src, const Gdiplus::Rect& box, int dw, int dh) {
        using namespace Gdiplus;
        Bitmap* cur = new Bitmap(box.Width, box.Height, PixelFormat32bppARGB);
        if (!cur || cur->GetLastStatus() != Ok) { delete cur; return nullptr; }
        {
            Graphics gg(cur);
            gg.SetCompositingMode(CompositingModeSourceCopy);
            gg.SetPixelOffsetMode(PixelOffsetModeHalf);
            gg.DrawImage(src, Rect(0, 0, box.Width, box.Height),
                         box.X, box.Y, box.Width, box.Height, UnitPixel);
        }

        while (cur->GetWidth() >= dw * 2 && cur->GetHeight() >= dh * 2) {
            int hw = cur->GetWidth() / 2, hh = cur->GetHeight() / 2;
            Bitmap* half = new Bitmap(hw, hh, PixelFormat32bppARGB);
            if (!half || half->GetLastStatus() != Ok) { delete half; break; }
            Graphics gg(half);
            gg.SetCompositingMode(CompositingModeSourceCopy);
            gg.SetInterpolationMode(InterpolationModeHighQualityBicubic);
            gg.SetPixelOffsetMode(PixelOffsetModeHighQuality);
            gg.DrawImage(cur, Rect(0, 0, hw, hh), 0, 0, cur->GetWidth(), cur->GetHeight(), UnitPixel);
            delete cur;
            cur = half;
        }

        if (cur->GetWidth() == dw && cur->GetHeight() == dh) return cur;

        Bitmap* out = new Bitmap(dw, dh, PixelFormat32bppARGB);
        if (!out || out->GetLastStatus() != Ok) { delete out; delete cur; return nullptr; }
        {
            Graphics gg(out);
            gg.SetCompositingMode(CompositingModeSourceCopy);
            gg.SetInterpolationMode(InterpolationModeHighQualityBicubic);
            gg.SetPixelOffsetMode(PixelOffsetModeHighQuality);
            gg.DrawImage(cur, Rect(0, 0, dw, dh), 0, 0, cur->GetWidth(), cur->GetHeight(), UnitPixel);
        }
        delete cur;
        return out;
    }

    // MARK: - 绘制
    // 头部朝向网格：按 headYaw / headPitch 选一格。
    // 不做权重混合（半透明叠在一起会让贴图整体变透），而是硬选一帧、再接 170ms 交叠淡出。
    // 阈值带迟滞：偏出水平 12.7° 立刻切抬头/低头，回到 5.7° 以内才退回平视，临界处不抖。
    static int pickAxis(float v, int prev, int lo, int mid, int hi) {
        const float kEnter = 0.22f, kExit = 0.10f;
        if (prev == lo && v < -kExit) return lo;
        if (prev == hi && v >  kExit) return hi;
        if (v < -kEnter) return lo;
        if (v >  kEnter) return hi;
        return mid;
    }

    State cellState(const CatPhysics& p) const {
        const float yaw = std::max(-1.0f, std::min(1.0f, p.headYaw.value));    // 正 = 朝右
        const float pit = std::max(-1.0f, std::min(1.0f, p.headPitch.value));  // 正 = 朝下
        const bool fromGrid = (cur_.special < 0);   // 特殊姿态切回来时从中排重新判
        State s;
        s.c = pickAxis(yaw, fromGrid ? cur_.c : G_C, G_L, G_C, G_R);
        s.r = pickAxis(pit, fromGrid ? cur_.r : R_MID, R_UP, R_MID, R_DN);
        return s;
    }

    static bool sameState(const State& a, const State& b) {
        if (a.special != b.special) return false;
        if (a.special >= 0) return true;
        return a.c == b.c && a.r == b.r;
    }

    State targetState(const CatPhysics& p) const {
        State s;
        s.special = pickSpecial(p);
        if (s.special < 0) s = cellState(p);
        return s;
    }

    void drawState(Gdiplus::Graphics& g, const State& s, float alpha,
                   float footX, float footY, float rot) {
        if (alpha <= 0.004f) return;
        if (s.special >= 0) {
            drawOne(g, spec_[s.special], alpha, footX, footY, rot);
            return;
        }
        drawOne(g, grid_[s.c][s.r], alpha, footX, footY, rot);
    }

    void drawOne(Gdiplus::Graphics& g, const Frame& f, float alpha,
                 float footX, float footY, float rotDeg) {
        if (!f.bmp || alpha <= 0.004f) return;

        Gdiplus::GraphicsState st = g.Save();
        g.TranslateTransform(footX, footY);
        if (std::fabs(rotDeg) > 0.01f) g.RotateTransform(rotDeg);

        Gdiplus::RectF dst(f.offX, f.offY + f.yBias, (float)f.w, (float)f.h);
        if (alpha >= 0.996f) {
            g.DrawImage(f.bmp, dst);
        } else {
            Gdiplus::ColorMatrix cm = {};
            cm.m[0][0] = cm.m[1][1] = cm.m[2][2] = 1.0f;
            cm.m[3][3] = alpha;
            cm.m[4][4] = 1.0f;
            Gdiplus::ImageAttributes ia;
            ia.SetColorMatrix(&cm, Gdiplus::ColorMatrixFlagsDefault, Gdiplus::ColorAdjustTypeBitmap);
            g.DrawImage(f.bmp, dst, 0.0f, 0.0f, (float)f.w, (float)f.h,
                        Gdiplus::UnitPixel, &ia);
        }
        g.Restore(st);
    }

    static void drawShadow(Gdiplus::Graphics& g, float footX, float footY, float spriteW) {
        using namespace Gdiplus;
        float w = std::max(46.0f, spriteW * 0.86f);
        for (int i = 0; i < 2; ++i) {
            float k = (i == 0) ? 1.16f : 0.92f;
            BYTE a = (i == 0) ? (BYTE)20 : (BYTE)36;
            SolidBrush b(Color(a, 68, 58, 46));
            g.FillEllipse(&b, footX - w * k * 0.5f, footY - 7.0f, w * k, 12.0f);
        }
    }

    // MARK: - 前爪：跟着鼠标 / 按下的键
    static void drawFrontPaws(Gdiplus::Graphics& g, const CatPhysics& p, float footX, float footY) {
        drawOnePaw(g, p.leftPaw.point(), CatPhysics::LeftPawRest, footX - 12.0f, footY - 30.0f, true);
        drawOnePaw(g, p.rightPaw.point(), CatPhysics::RightPawRest, footX + 12.0f, footY - 30.0f, false);
    }

    static void drawOnePaw(Gdiplus::Graphics& g, const Pt& tgt, const Pt& rest,
                           float shX, float shY, bool isLeft) {
        using namespace Gdiplus;
        float dx = tgt.x - rest.x, dy = tgt.y - rest.y;
        float dist = std::sqrt(dx * dx + dy * dy);
        // 离位越远越实：静止时完全交给画里的爪子
        float a = (dist - 5.0f) / 11.0f;
        a = std::max(0.0f, std::min(1.0f, a));
        if (a <= 0.02f) return;

        PointF from(shX, shY);
        PointF to(tgt.x, tgt.y);

        // 爪子落点稍微抬一点，压在键帽上沿
        float vx = to.X - from.X, vy = to.Y - from.Y;
        float len = std::sqrt(vx * vx + vy * vy);
        if (len < 1.0f) return;
        PointF dir(vx / len, vy / len);
        PointF nrm(-dir.Y, dir.X);

        // 中度弯曲：让手臂有个自然的弧
        PointF mid((from.X + to.X) * 0.5f + nrm.X * len * 0.10f,
                   (from.Y + to.Y) * 0.5f + nrm.Y * len * 0.10f);

        BYTE aa = (BYTE)(a * 255.0f);
        Color furHi(aa, 251, 236, 210);
        Color furLo((BYTE)(aa * 0.96f), 233, 205, 163);
        Color shade((BYTE)(aa * 0.55f), 190, 156, 112);

        PointF ring[26];
        const int N = 12;
        for (int i = 0; i <= N; ++i) {
            float t = (float)i / (float)N;
            float mt = 1.0f - t;
            float px = mt * mt * from.X + 2 * mt * t * mid.X + t * t * to.X;
            float py = mt * mt * from.Y + 2 * mt * t * mid.Y + t * t * to.Y;
            float hw = 7.6f * (1.0f - t) + 4.0f * t;      // 半宽：肩粗腕细
            ring[i] = PointF(px + nrm.X * hw, py + nrm.Y * hw);
            ring[2 * N + 1 - i] = PointF(px - nrm.X * hw, py - nrm.Y * hw);
        }
        GraphicsPath path;
        path.AddPolygon(ring, 2 * (N + 1));

        LinearGradientBrush lg(from, to, furHi, furLo);
        g.FillPath(&lg, &path);

        // 爪子本体
        float pr = 5.6f;
        SolidBrush paw(furLo);
        g.FillEllipse(&paw, to.X - pr, to.Y - pr * 0.86f, pr * 2.0f, pr * 1.72f);
        Pen toeC(Pen(Color((BYTE)(aa * 0.40f), 176, 141, 98), 0.9f));
        g.DrawLine(&toeC, to.X - 2.0f, to.Y + 1.4f, to.X - 1.0f, to.Y + 3.6f);
        g.DrawLine(&toeC, to.X + 1.6f, to.Y + 1.4f, to.X + 0.6f, to.Y + 3.6f);
        (void)shade; (void)isLeft;
    }

    static float widthAt(float t) { return 6.2f * (1.0f - t) + 4.0f * t; }
};

#endif // PUPPY_SKIN_HPP
