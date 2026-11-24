#pragma once
#define NOMINMAX
#include <windows.h>
#include <objidl.h>
#include <gdiplus.h>
#include <cmath>
#include <vector>
#include <algorithm>
#include "CatPhysics.hpp"

namespace gd = Gdiplus;

// ============================================================================
//  小金毛（Golden Retriever Puppy）美术模块
//  目标：动画风——蓬松圆润的毛簇边缘、厚深棕描边、大圆棕眼 + 大白高光、
//        大黑鼻、大张嘴吐大粉舌、湖绿项圈 + 金属吊牌、奶油胸口毛领。
//  坐标：所有 x/y 为「头部局部坐标」（原点 = 头心，+y 向下），z 为朝前深度。
// ============================================================================
struct PuppyArt {
    // ---------------- 调色板 ----------------
    static gd::Color furLight(BYTE a = 255) { return gd::Color(a, 252, 236, 205); } // 头顶受光
    static gd::Color fur(BYTE a = 255)      { return gd::Color(a, 246, 214, 158); } // 主体被毛
    static gd::Color cream(BYTE a = 255)    { return gd::Color(a, 253, 245, 226); } // 口吻 / 胸口
    static gd::Color gold(BYTE a = 255)     { return gd::Color(a, 235, 188, 121); } // 耳 / 暗部
    static gd::Color deep(BYTE a = 255)     { return gd::Color(a, 206, 149, 88);  } // 阴影
    static gd::Color earIn(BYTE a = 255)    { return gd::Color(a, 230, 167, 132); } // 耳内
    static gd::Color ink(BYTE a = 255)      { return gd::Color(a, 79, 55, 37);    } // 轮廓线
    static gd::Color pupil(BYTE a = 255)    { return gd::Color(a, 34, 26, 21);    }
    static gd::Color irisTop(BYTE a = 255)  { return gd::Color(a, 124, 76, 40);   }
    static gd::Color irisLow(BYTE a = 255)  { return gd::Color(a, 203, 141, 80);  }
    static gd::Color noseInk(BYTE a = 255)  { return gd::Color(a, 48, 38, 34);    }
    static gd::Color mouthIn(BYTE a = 255)  { return gd::Color(a, 70, 40, 36);    }
    static gd::Color tongue(BYTE a = 255)   { return gd::Color(a, 242, 141, 152); }
    static gd::Color tongueDeep(BYTE a = 255) { return gd::Color(a, 199, 86, 106); }
    static gd::Color collarC(BYTE a = 255)  { return gd::Color(a, 42, 148, 137);  }
    static gd::Color collarDeep(BYTE a = 255) { return gd::Color(a, 24, 102, 96); }
    static gd::Color steel(BYTE a = 255)    { return gd::Color(a, 188, 192, 196); }
    static gd::Color white(BYTE a = 255)    { return gd::Color(a, 255, 255, 255); }

    // ---------------- 基础工具 ----------------
    static Pt project(float x, float y, float z, float yawRad, float pitchRad) {
        float cy = std::cos(yawRad), sy = std::sin(yawRad);
        float x1 = x * cy + z * sy;
        float z1 = -x * sy + z * cy;
        float cp = std::cos(pitchRad), sp = std::sin(pitchRad);
        float y2 = y * cp + z1 * sp;
        return { x1, y2 };
    }

    static void quadBezier(gd::GraphicsPath& p, Pt p0, Pt ctrl, Pt p2) {
        gd::PointF c1(p0.x + (2.0f / 3.0f) * (ctrl.x - p0.x), p0.y + (2.0f / 3.0f) * (ctrl.y - p0.y));
        gd::PointF c2(p2.x + (2.0f / 3.0f) * (ctrl.x - p2.x), p2.y + (2.0f / 3.0f) * (ctrl.y - p2.y));
        p.AddBezier(gd::PointF(p0.x, p0.y), c1, c2, gd::PointF(p2.x, p2.y));
    }

    // 把平滑轮廓变成「圆润毛簇」边缘：
    // 等弧长重采样 → 每段用一条向外鼓起的贝塞尔，形成一个个柔软的毛团，
    // 而不是尖锯齿。头顶区域收得更平，贴地一圈几乎不长毛。
    static void furOutline(const gd::GraphicsPath& smooth, gd::PointF cHead, gd::PointF cBody,
                           float flatBaseY, float ampScale, gd::GraphicsPath& out) {
        gd::GraphicsPath flat;
        flat.AddPath(&smooth, FALSE);
        gd::Matrix id;
        flat.Flatten(&id, 0.4f);
        const INT n = flat.GetPointCount();
        if (n < 6) { out.AddPath(&smooth, FALSE); return; }

        std::vector<gd::PointF> pts((size_t)n);
        flat.GetPathPoints(pts.data(), n);
        if (std::hypot(pts[0].X - pts[pts.size() - 1].X, pts[0].Y - pts[pts.size() - 1].Y) > 0.05f) {
            pts.push_back(pts[0]);
        }

        std::vector<float> cum(pts.size(), 0.0f);
        for (size_t j = 1; j < pts.size(); ++j) {
            cum[j] = cum[j - 1] + std::hypot(pts[j].X - pts[j - 1].X, pts[j].Y - pts[j - 1].Y);
        }
        const float total = cum.back();
        if (total < 10.0f) { out.AddPath(&smooth, FALSE); return; }

        const int m = std::max(14, (int)std::lround(total / 5.4f));
        const float ds = total / (float)m;

        std::vector<gd::PointF> rs((size_t)m + 1);
        size_t j = 1;
        for (int q = 0; q <= m; ++q) {
            float target = ds * (float)q;
            while (j + 1 < pts.size() && cum[j] < target) ++j;
            float segLen = cum[j] - cum[j - 1];
            float f = (segLen > 1e-5f) ? (target - cum[j - 1]) / segLen : 0.0f;
            f = std::clamp(f, 0.0f, 1.0f);
            rs[(size_t)q].X = pts[j - 1].X + (pts[j].X - pts[j - 1].X) * f;
            rs[(size_t)q].Y = pts[j - 1].Y + (pts[j].Y - pts[j - 1].Y) * f;
        }

        out.Reset();
        out.StartFigure();
        for (int q = 0; q < m; ++q) {
            gd::PointF a = rs[(size_t)q], b = rs[(size_t)q + 1];
            float mx = (a.X + b.X) * 0.5f, my = (a.Y + b.Y) * 0.5f;
            float dx = b.X - a.X, dy = b.Y - a.Y;
            float l = std::hypot(dx, dy);
            if (l < 1e-5f) l = 1.0f;
            float nx = -dy / l, ny = dx / l;
            gd::PointF c = (my < cHead.Y + 20.0f) ? cHead : cBody;
            if (nx * (mx - c.X) + ny * (my - c.Y) < 0.0f) { nx = -nx; ny = -ny; }

            float amp = (2.5f + 1.15f * std::sin((float)q * 2.31f + 0.7f)
                              + 0.55f * std::sin((float)q * 5.13f + 2.1f)) * ampScale;
            if (my < cHead.Y - 14.0f) amp *= 0.5f;    // 头顶更平顺
            if (my > flatBaseY - 12.0f) amp *= 0.18f; // 贴地一圈几乎无毛尖
            if (amp < 0.25f) amp = 0.25f;

            gd::PointF c1(a.X + nx * amp * 1.32f, a.Y + ny * amp * 1.32f);
            gd::PointF c2(b.X + nx * amp * 1.32f, b.Y + ny * amp * 1.32f);
            out.AddBezier(a, c1, c2, b);
        }
        out.CloseFigure();
    }

    // ---------------- 身体 ----------------
    static void drawBody(gd::Graphics& g, const gd::GraphicsPath& silhouette,
                         float seatX, float seatY, Pt headCenter) {
        gd::PointF cHead(headCenter.x, headCenter.y);
        gd::PointF cBody(seatX, seatY - 40.0f);

        // 1) 身体本体（毛簇边缘）
        gd::GraphicsPath bodyFur;
        furOutline(silhouette, cHead, cBody, seatY, 1.0f, bodyFur);
        {
            gd::LinearGradientBrush gr(gd::PointF(seatX, headCenter.y + 2.0f),
                                       gd::PointF(seatX, seatY + 8.0f), fur(), gold());
            g.FillPath(&gr, &bodyFur);
        }
        // 两侧压暗
        {
            gd::GraphicsState s0 = g.Save();
            g.SetClip(&bodyFur, gd::CombineModeIntersect);
            gd::GraphicsPath vg;
            vg.AddEllipse(seatX - 36.0f, headCenter.y - 6.0f, 72.0f, 176.0f);
            gd::PathGradientBrush pg(&vg);
            gd::Color edge(96, 206, 149, 88), mid(0, 206, 149, 88);
            int cc = 1;
            pg.SetSurroundColors(&edge, &cc);
            pg.SetCenterColor(mid);
            pg.SetCenterPoint(gd::PointF(seatX, headCenter.y + 50.0f));
            g.FillPath(&pg, &vg);
            g.Restore(s0);
        }
        // 胸口奶油毛领（在头之后被下巴压住上部）
        drawRuff(g, bodyFur, seatX, headCenter.y + 22.0f);
        // 身体轮廓
        {
            gd::Pen rim(ink(228), 1.5f);
            rim.SetLineJoin(gd::LineJoinRound);
            g.DrawPath(&rim, &bodyFur);
        }

        // 2) 头（画在身体前面：下巴与胸口之间有一条清楚的分界线）
        drawHeadMass(g, cHead, 1.0f, seatY);
    }

    // 头：比身体轮廓略大的圆脑袋，带毛簇边缘与厚描边
    static void drawHeadMass(gd::Graphics& g, gd::PointF c, float k, float gSeatY) {
        const float rx = 46.0f * k, ry = 38.5f * k;
        gd::GraphicsPath head;
        head.AddEllipse(c.X - rx, c.Y - ry, rx * 2.0f, ry * 2.0f);

        gd::GraphicsPath headFur;
        furOutline(head, c, c, 9999.0f, 0.95f, headFur);

        {
            gd::LinearGradientBrush gr(gd::PointF(c.X, c.Y - ry), gd::PointF(c.X, gSeatY + 8.0f),
                                       fur(), gold());
            g.FillPath(&gr, &headFur);
        }
        // 脸颊两侧 / 下巴压暗，做出球感
        {
            gd::GraphicsState s = g.Save();
            g.SetClip(&headFur, gd::CombineModeIntersect);
            gd::GraphicsPath vg;
            vg.AddEllipse(c.X - rx * 0.52f, c.Y - ry * 0.75f, rx * 1.04f, ry * 1.62f);
            gd::PathGradientBrush pg(&vg);
            gd::Color edge(88, 206, 149, 88), mid(0, 206, 149, 88);
            int cc = 1;
            pg.SetSurroundColors(&edge, &cc);
            pg.SetCenterColor(mid);
            pg.SetCenterPoint(gd::PointF(c.X, c.Y - ry * 0.06f));
            g.FillPath(&pg, &vg);
            g.Restore(s);
        }
        // 腮边毛簇
        {
            gd::Pen p(deep(70), 1.1f);
            p.SetStartCap(gd::LineCapRound); p.SetEndCap(gd::LineCapRound);
            for (int sgn = -1; sgn <= 1; sgn += 2) {
                for (int i = 0; i < 3; ++i) {
                    float y = c.Y + 6.0f + i * 7.0f;
                    float x = c.X + sgn * (rx - 6.0f - i * 2.0f);
                    g.DrawLine(&p, x, y, x + sgn * 6.0f, y + 7.0f);
                }
            }
        }
        // 厚描边
        {
            gd::Pen rim(ink(235), 1.6f);
            rim.SetLineJoin(gd::LineJoinRound);
            g.DrawPath(&rim, &headFur);
        }
    }

    // 胸口奶油毛领：一团柔软的奶油色，下缘是圆毛球
    static void drawRuff(gd::Graphics& g, const gd::GraphicsPath& clipPath, float seatX, float topY) {
        gd::GraphicsState s = g.Save();
        g.SetClip(&clipPath, gd::CombineModeIntersect);

        const float halfW = 23.0f;
        std::vector<gd::PointF> mp;
        const int N = 12;
        for (int q = 0; q <= N; ++q) {
            float t = (float)q / (float)N;
            mp.push_back(gd::PointF(seatX + (t - 0.5f) * 2.0f * halfW, topY - 10.0f));
        }
        for (int q = N; q >= 0; --q) {
            float t = (float)q / (float)N;
            float b = std::sin(t * 3.14159265f);
            mp.push_back(gd::PointF(seatX + (t - 0.5f) * 2.0f * halfW * 0.94f,
                                    topY + 8.0f + b * 34.0f));
        }
        gd::GraphicsPath mane;
        mane.AddPolygon(mp.data(), (INT)mp.size());
        {
            gd::LinearGradientBrush gr(gd::PointF(seatX, topY), gd::PointF(seatX, topY + 44.0f),
                                       cream(235), gd::Color(255, 249, 234, 210));
            g.FillPath(&gr, &mane);
        }
        // 下缘的毛球，让边变得蓬松
        {
            gd::SolidBrush ball(gd::Color(255, 250, 238, 218));
            for (int i = 0; i <= 6; ++i) {
                float t = (float)i / 6.0f;
                float b = std::sin(t * 3.14159265f);
                float x = seatX + (t - 0.5f) * 2.0f * halfW * 0.86f;
                float y = topY + 6.0f + b * 32.0f;
                float r = 6.2f + b * 2.4f;
                g.FillEllipse(&ball, x - r, y - r * 0.85f, r * 2.0f, r * 1.7f);
            }
        }
        g.Restore(s);
    }

    // ---------------- 垂耳 ----------------
    // 上宽下窄、贴着颊侧垂下来，耳缘也是毛簇
    static void drawEar(gd::Graphics& g, bool isLeft, float yawRad, float pitchRad, float fold, float sway) {
        const float sign = isLeft ? -1.0f : 1.0f;
        const float droop = 6.0f + fold * 14.0f;

        Pt top   = project(sign * 28.0f,                   -28.0f,                 -3.0f, yawRad, pitchRad);
        Pt outer = project(sign * 45.0f,                    -9.0f,                 -7.0f, yawRad, pitchRad);
        Pt bulge = project(sign * 45.5f + sway,              13.0f,                 -8.0f, yawRad, pitchRad);
        Pt tip   = project(sign * 34.0f + sway * 1.3f,       36.0f + droop,         -3.0f, yawRad, pitchRad);
        Pt inner = project(sign * 29.0f,                     26.0f + droop * 0.30f,  6.0f, yawRad, pitchRad);

        gd::GraphicsPath ear;
        quadBezier(ear, top, { (top.x + outer.x) * 0.5f + sign * 8.0f, (top.y + outer.y) * 0.5f - 5.0f }, outer);
        quadBezier(ear, outer, { outer.x + sign * 3.0f, (outer.y + bulge.y) * 0.5f }, bulge);
        quadBezier(ear, bulge, { bulge.x - sign * 5.0f, bulge.y + 11.0f }, tip);
        quadBezier(ear, tip, { (tip.x + inner.x) * 0.5f + sign * 1.0f, tip.y + 3.0f }, inner);
        quadBezier(ear, inner, { (inner.x + top.x) * 0.5f - sign * 8.0f, (inner.y + top.y) * 0.5f + 4.0f }, top);
        ear.CloseFigure();

        gd::GraphicsPath earFur;
        gd::PointF c((top.x + tip.x) * 0.5f, (top.y + tip.y) * 0.5f);
        furOutline(ear, c, c, 9999.0f, 0.55f, earFur);

        {
            gd::LinearGradientBrush eg(gd::PointF(top.x, top.y), gd::PointF(tip.x, tip.y + 10.0f), gold(), deep());
            g.FillPath(&eg, &earFur);
        }
        // 内耳
        {
            Pt iTop   = project(sign * 32.0f, -13.0f, 2.0f, yawRad, pitchRad);
            Pt iOut   = project(sign * 41.0f,  2.0f,  2.0f, yawRad, pitchRad);
            Pt iTip   = project(sign * 34.0f + sway * 1.1f, 31.0f + droop * 0.8f, 4.0f, yawRad, pitchRad);
            Pt iInner = project(sign * 30.0f,  18.0f + droop * 0.25f, 4.0f, yawRad, pitchRad);
            gd::GraphicsPath ie;
            quadBezier(ie, iTop, { (iTop.x + iOut.x) * 0.5f + sign * 5.0f, (iTop.y + iOut.y) * 0.5f - 2.0f }, iOut);
            quadBezier(ie, iOut, { iOut.x - sign * 3.0f, iOut.y + 7.0f }, iTip);
            quadBezier(ie, iTip, { (iTip.x + iInner.x) * 0.5f, iTip.y + 3.0f }, iInner);
            quadBezier(ie, iInner, { (iInner.x + iTop.x) * 0.5f - sign * 4.0f, (iInner.y + iTop.y) * 0.5f + 3.0f }, iTop);
            ie.CloseFigure();
            gd::SolidBrush ib(earIn(140));
            g.FillPath(&ib, &ie);
        }
        // 耳毛线条
        {
            gd::Pen p(deep(80), 1.0f);
            p.SetStartCap(gd::LineCapRound); p.SetEndCap(gd::LineCapRound);
            for (int k = 0; k < 3; ++k) {
                float t = 0.22f + 0.22f * (float)k;
                float x0 = outer.x + (bulge.x - outer.x) * t;
                float y0 = outer.y + (bulge.y - outer.y) * t;
                g.DrawLine(&p, x0, y0, x0 + sign * 2.5f, y0 + 11.0f);
            }
        }
        {
            gd::Pen rim(ink(225), 1.4f);
            rim.SetLineJoin(gd::LineJoinRound);
            g.DrawPath(&rim, &earFur);
        }
    }

    // ---------------- 脸 ----------------
    static void drawFace(gd::Graphics& g, float yawRad, float pitchRad, const CatPhysics& physics) {
        (void)physics;
        const float sqX = std::clamp(std::cos(yawRad), 0.74f, 1.0f);
        const float sqY = std::clamp(std::cos(pitchRad), 0.82f, 1.0f);

        // 1) 奶油色口吻（下半张脸），软边
        {
            Pt mc = project(0.0f, 20.6f, 30.0f, yawRad, pitchRad);
            float mw = 25.5f * sqX, mh = 15.8f * sqY;
            gd::GraphicsPath muzzle;
            muzzle.AddEllipse(mc.x - mw, mc.y - mh, mw * 2.0f, mh * 2.0f);
            gd::PathGradientBrush mb(&muzzle);
            gd::Color edge(0, 253, 245, 226);
            gd::Color ctr(244, 253, 246, 228);
            int cc = 1;
            mb.SetSurroundColors(&edge, &cc);
            mb.SetCenterColor(ctr);
            mb.SetCenterPoint(gd::PointF(mc.x, mc.y + 2.0f));
            g.FillPath(&mb, &muzzle);
        }

        // 2) 大黑鼻头（动画风：宽、圆、亮）
        Pt n = project(0.0f, 7.2f, 31.0f, yawRad, pitchRad);
        const float nw = 10.4f * sqX, nh = 7.4f * sqY;
        {
            gd::GraphicsPath nose;
            nose.AddBezier(gd::PointF(n.x - nw, n.y - nh * 0.10f),
                           gd::PointF(n.x - nw * 0.92f, n.y - nh * 1.18f),
                           gd::PointF(n.x + nw * 0.92f, n.y - nh * 1.18f),
                           gd::PointF(n.x + nw, n.y - nh * 0.10f));
            nose.AddBezier(gd::PointF(n.x + nw, n.y - nh * 0.10f),
                           gd::PointF(n.x + nw * 0.96f, n.y + nh * 0.70f),
                           gd::PointF(n.x + nw * 0.40f, n.y + nh * 1.10f),
                           gd::PointF(n.x, n.y + nh * 1.12f));
            nose.AddBezier(gd::PointF(n.x, n.y + nh * 1.12f),
                           gd::PointF(n.x - nw * 0.40f, n.y + nh * 1.10f),
                           gd::PointF(n.x - nw * 0.96f, n.y + nh * 0.70f),
                           gd::PointF(n.x - nw, n.y - nh * 0.10f));
            nose.CloseFigure();
            gd::SolidBrush nb(noseInk());
            g.FillPath(&nb, &nose);

            gd::SolidBrush nostril(gd::Color(240, 14, 10, 9));
            g.FillEllipse(&nostril, n.x - nw * 0.62f, n.y - nh * 0.02f, nw * 0.36f, nh * 0.46f);
            g.FillEllipse(&nostril, n.x + nw * 0.26f, n.y - nh * 0.02f, nw * 0.36f, nh * 0.46f);

            gd::SolidBrush gloss(gd::Color(130, 255, 255, 255));
            g.FillEllipse(&gloss, n.x - nw * 0.60f, n.y - nh * 0.80f, nw * 0.46f, nh * 0.30f);
        }

        // 3) 人中
        Pt mt = project(0.0f, 17.0f, 30.6f, yawRad, pitchRad);
        {
            gd::Pen ph(ink(120), 1.2f);
            ph.SetStartCap(gd::LineCapRound); ph.SetEndCap(gd::LineCapRound);
            g.DrawLine(&ph, n.x, n.y + nh * 1.10f, mt.x, mt.y - 0.6f);
        }

        // 4) 大张嘴（上唇在中间微微下垂的宽笑口）
        Pt cL = project(-17.2f, 14.2f, 30.0f, yawRad, pitchRad);
        Pt cR = project(17.2f, 14.2f, 30.0f, yawRad, pitchRad);
        Pt lm = project(0.0f, 18.6f, 30.4f, yawRad, pitchRad);
        Pt mb = project(0.0f, 32.4f, 30.6f, yawRad, pitchRad);
        gd::GraphicsPath mouth;
        quadBezier(mouth, { cL.x, cL.y }, { lm.x, lm.y }, { cR.x, cR.y });
        mouth.AddBezier(gd::PointF(cR.x, cR.y),
                        gd::PointF(cR.x + 0.6f, cR.y + 9.5f),
                        gd::PointF(10.5f * sqX, mb.y - 0.4f),
                        gd::PointF(mb.x, mb.y));
        mouth.AddBezier(gd::PointF(mb.x, mb.y),
                        gd::PointF(-10.5f * sqX, mb.y - 0.4f),
                        gd::PointF(cL.x - 0.6f, cL.y + 9.5f),
                        gd::PointF(cL.x, cL.y));
        mouth.CloseFigure();
        {
            gd::SolidBrush mi(mouthIn(252));
            g.FillPath(&mi, &mouth);
        }

        // 5) 大粉舌（几乎填满口腔，舌尖垂到嘴外，带中缝与高光）
        {
            const float tw = 14.0f * sqX;
            Pt tt = project(0.0f, 31.8f, 31.4f, yawRad, pitchRad);
            gd::GraphicsPath tg;
            tg.AddBezier(gd::PointF(-tw * 0.84f, mt.y + 1.2f),
                         gd::PointF(-tw * 1.02f, tt.y - 6.0f),
                         gd::PointF(-tw * 0.60f, tt.y + 1.2f),
                         gd::PointF(0.0f, tt.y + 1.6f));
            tg.AddBezier(gd::PointF(0.0f, tt.y + 1.6f),
                         gd::PointF(tw * 0.60f, tt.y + 1.2f),
                         gd::PointF(tw * 1.02f, tt.y - 6.0f),
                         gd::PointF(tw * 0.84f, mt.y + 1.2f));
            tg.CloseFigure();
            gd::LinearGradientBrush tgr(gd::PointF(0.0f, mt.y + 2.0f),
                                        gd::PointF(0.0f, tt.y + 4.0f), tongue(), tongueDeep());
            g.FillPath(&tgr, &tg);
            gd::Pen tc(tongueDeep(190), 1.0f);
            tc.SetLineJoin(gd::LineJoinRound);
            g.DrawPath(&tc, &tg);

            gd::Pen crease(tongueDeep(120), 0.9f);
            crease.SetStartCap(gd::LineCapRound); crease.SetEndCap(gd::LineCapRound);
            g.DrawLine(&crease, 0.0f, mt.y + 6.0f, 0.0f, tt.y - 1.0f);

            gd::SolidBrush gloss(gd::Color(115, 255, 255, 255));
            g.FillEllipse(&gloss, -tw * 0.46f, mt.y + 5.5f, tw * 0.58f, 3.2f);
        }

        // 6) 嘴线 + 嘴角上扬
        {
            gd::Pen lip(ink(238), 1.6f);
            lip.SetStartCap(gd::LineCapRound); lip.SetEndCap(gd::LineCapRound);
            lip.SetLineJoin(gd::LineJoinRound);
            g.DrawPath(&lip, &mouth);

            gd::Pen smile(ink(190), 1.4f);
            smile.SetStartCap(gd::LineCapRound); smile.SetEndCap(gd::LineCapRound);
            g.DrawLine(&smile, cL.x + 1.0f, cL.y - 0.6f, cL.x - 3.6f, cL.y - 4.2f);
            g.DrawLine(&smile, cR.x - 1.0f, cR.y - 0.6f, cR.x + 3.6f, cR.y - 4.2f);
        }

        // 7) 下巴阴影（把嘴和胸口分开）
        {
            Pt cb = project(0.0f, 34.5f, 29.0f, yawRad, pitchRad);
            gd::SolidBrush sh(deep(50));
            g.FillEllipse(&sh, cb.x - 13.0f * sqX, cb.y - 2.0f, 26.0f * sqX, 6.0f * sqY);
        }
    }

    // ---------------- 眼睛 ----------------
    // mode: 0 正常 / 1 开心闭眼 ^ ^ / 2 眯眼
    static void drawEye(gd::Graphics& g, bool isLeft, float yawRad, float pitchRad, int mode, float hunt) {
        const float sign = isLeft ? -1.0f : 1.0f;
        Pt proj = project(sign * 17.8f, -4.8f, 27.5f, yawRad, pitchRad);

        const float eyeNormal = yawRad + sign * 0.34f;
        const float foreX = std::clamp(std::cos(eyeNormal) / std::cos(0.34f), 0.68f, 1.05f);
        const float foreY = std::clamp(std::cos(pitchRad * 0.85f), 0.82f, 1.02f);

        gd::GraphicsState st = g.Save();
        g.TranslateTransform(proj.x, proj.y);
        g.ScaleTransform(foreX, foreY);

        if (mode == 1) {
            gd::GraphicsPath hp;
            quadBezier(hp, { -10.2f, 1.6f }, { 0.0f, -6.0f }, { 10.2f, 1.6f });
            gd::Pen p(ink(240), 3.0f);
            p.SetStartCap(gd::LineCapRound); p.SetEndCap(gd::LineCapRound);
            g.DrawPath(&p, &hp);
            g.Restore(st);
            return;
        }
        if (mode == 2) {
            gd::GraphicsPath sp;
            quadBezier(sp, { -10.6f, -0.8f }, { 0.0f, 0.8f }, { 10.6f, -0.8f });
            quadBezier(sp, { 10.6f, -0.8f }, { 0.0f, 6.4f }, { -10.6f, -0.8f });
            sp.CloseFigure();
            gd::SolidBrush sb(irisTop());
            g.FillPath(&sb, &sp);
            gd::Pen sp2(ink(200), 1.2f);
            g.DrawPath(&sp2, &sp);
            g.Restore(st);
            return;
        }

        const float rx = 9.9f + hunt * 0.7f;
        const float ry = 10.7f + hunt * 0.9f;

        // 眼眶（深棕描边）
        {
            gd::GraphicsPath ring;
            ring.AddEllipse(-rx - 1.05f, -ry - 1.05f, (rx + 1.05f) * 2.0f, (ry + 1.05f) * 2.0f);
            gd::SolidBrush rk(ink());
            g.FillPath(&rk, &ring);
        }
        // 虹膜
        {
            gd::GraphicsPath iris;
            iris.AddEllipse(-rx, -ry, rx * 2.0f, ry * 2.0f);
            gd::LinearGradientBrush ig(gd::PointF(0.0f, -ry), gd::PointF(0.0f, ry),
                                       gd::Color(255, 112, 66, 36), gd::Color(255, 165, 106, 58));
            g.FillPath(&ig, &iris);
        }
        // 瞳孔
        {
            const float pw = 8.0f + hunt * 1.2f, ph = 10.0f + hunt * 1.4f;
            gd::SolidBrush pb(pupil());
            g.FillEllipse(&pb, -pw * 0.5f, -ph * 0.5f - 1.0f, pw, ph);
        }
        // 高光：一大一小
        {
            gd::SolidBrush hi(gd::Color(245, 255, 255, 255));
            g.FillEllipse(&hi, -rx * 0.70f, -ry * 0.80f, 5.6f, 5.6f);
            gd::SolidBrush hi2(gd::Color(160, 255, 255, 255));
            g.FillEllipse(&hi2, rx * 0.16f, ry * 0.20f, 2.8f, 2.8f);
        }
        g.Restore(st);
    }

    // ---------------- 湖绿项圈 + 吊牌 ----------------
    static void drawCollar(gd::Graphics& g, float seatX, float seatY) {
        const float y = seatY - 52.0f;
        gd::GraphicsPath band;
        band.AddBezier(gd::PointF(seatX - 31.0f, y - 6.0f),
                       gd::PointF(seatX - 17.0f, y + 13.0f),
                       gd::PointF(seatX + 17.0f, y + 13.0f),
                       gd::PointF(seatX + 31.0f, y - 6.0f));
        band.AddLine(gd::PointF(seatX + 31.0f, y - 6.0f), gd::PointF(seatX + 31.0f, y + 3.0f));
        band.AddBezier(gd::PointF(seatX + 31.0f, y + 3.0f),
                       gd::PointF(seatX + 17.0f, y + 22.0f),
                       gd::PointF(seatX - 17.0f, y + 22.0f),
                       gd::PointF(seatX - 31.0f, y + 3.0f));
        band.CloseFigure();
        {
            gd::LinearGradientBrush grad(gd::PointF(seatX, y - 6.0f), gd::PointF(seatX, y + 20.0f),
                                         collarC(), collarDeep());
            g.FillPath(&grad, &band);
        }
        // 缝线
        {
            gd::Pen stitch(collarDeep(130), 0.8f);
            gd::GraphicsPath line;
            line.AddBezier(gd::PointF(seatX - 26.0f, y - 2.0f),
                           gd::PointF(seatX - 14.0f, y + 13.0f),
                           gd::PointF(seatX + 14.0f, y + 13.0f),
                           gd::PointF(seatX + 26.0f, y - 2.0f));
            g.DrawPath(&stitch, &line);
        }
        {
            gd::Pen rim(ink(110), 0.95f);
            rim.SetLineJoin(gd::LineJoinRound);
            g.DrawPath(&rim, &band);
        }
        // 金属圆环 + 吊牌
        {
            gd::Pen ring(steel(), 1.8f);
            g.DrawEllipse(&ring, seatX + 6.0f, y + 3.0f, 8.0f, 8.0f);
            gd::Pen ring2(gd::Color(255, 120, 126, 132), 0.7f);
            g.DrawEllipse(&ring2, seatX + 6.0f, y + 3.0f, 8.0f, 8.0f);

            gd::GraphicsPath tag;
            tag.AddBezier(gd::PointF(seatX + 10.0f, y + 10.0f),
                          gd::PointF(seatX + 15.0f, y + 13.0f),
                          gd::PointF(seatX + 15.0f, y + 22.0f),
                          gd::PointF(seatX + 10.0f, y + 25.0f));
            tag.AddBezier(gd::PointF(seatX + 10.0f, y + 25.0f),
                          gd::PointF(seatX + 5.0f, y + 22.0f),
                          gd::PointF(seatX + 5.0f, y + 13.0f),
                          gd::PointF(seatX + 10.0f, y + 10.0f));
            tag.CloseFigure();
            gd::SolidBrush tb(gd::Color(255, 198, 202, 206));
            g.FillPath(&tb, &tag);
            gd::Pen tp(gd::Color(255, 138, 143, 148), 0.9f);
            g.DrawPath(&tp, &tag);
        }
        // 项圈上方压住的几缕奶油毛
        {
            gd::SolidBrush fluff(cream(235));
            for (int i = -2; i <= 2; ++i) {
                float x = seatX + i * 8.5f;
                float r = 4.6f - std::fabs((float)i) * 0.5f;
                float yy = y + 1.5f + std::fabs((float)i) * 1.4f;
                g.FillEllipse(&fluff, x - r, yy - r, r * 2.0f, r * 1.9f);
            }
        }
    }
};
