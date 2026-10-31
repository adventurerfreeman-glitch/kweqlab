#pragma once
// KW EQ Lab - DSP core (plain C++17, no JUCE).  Four EQ architectures:
//   BAXENDALL, BBC EQ (Body/Bright/Cut), ROTARD (4-way split + band resonance), SUPERSCHALL (tonal module)
// Every tweakable number is a named constant in kwlab::cfg.
#include <algorithm>
#include <array>
#include <cmath>
#include <complex>

namespace kwlab
{
constexpr double kPi = 3.14159265358979323846;
using cd = std::complex<double>;

namespace cfg
{
// ---- BAXENDALL ----
constexpr double bxLowHz = 150.0, bxHighHz = 3500.0, bxShelfQ = 0.6, bxShelfDb = 12.0;
constexpr double bxMidDb = 10.0;                       // +/- at full MID
constexpr double bxSweepMinHz = 500.0, bxSweepMaxHz = 2500.0;   // MID SWEEP range (around the Bax overlap region)
constexpr double bxQNarrow = 1.6, bxQBroad = 0.4;
constexpr double bxSkirtShift = 2.2, bxSkirtQ = 0.4, bxSkirtAmount = 0.7, bxBellShare = 0.5;
constexpr double bxShortShift = 1.6, bxShortQ = 1.1, bxShortAmount = 0.45;
constexpr double bxBrightMul = 1.15, bxDarkMul = 0.87;           // VIBE shifts the mid centre
constexpr double bxBrightCornerMul = 1.2, bxDarkCornerMul = 0.85; // ...and the shelf corners
constexpr double bxVibeTiltDb = 1.2;                              // ...and adds a small overall tilt
constexpr double bxPostLoading = 0.25;                            // POST-BAX: mid Q narrows by up to this when shelves are boosted
// ---- BBC EQ ----
constexpr double bbBodyHz = 150.0, bbBodyDb = 5.0, bbBodyBellHz = 280.0, bbBodyBellDb = 2.0;
constexpr double bbBrightHz = 3500.0, bbBrightDb = 5.0, bbSparkHz = 6500.0, bbSparkDb = 1.5;
constexpr double bbCutLpMax = 14000.0, bbCutLpMin = 2800.0, bbCutPresHz = 4000.0, bbCutPresDb = -4.0;
// ---- ROTARD ----
constexpr double rtX1 = 160.0, rtX2 = 700.0, rtX3 = 2800.0;      // crossover points (Linkwitz-Riley 4th order)
constexpr double rtCentre[4] = { 90.0, 340.0, 1400.0, 5200.0 };  // resonance centre of each band
constexpr double rtResDb = 9.0, rtQLow = 0.7, rtQHigh = 3.0;     // resonance gain and Q range
// ---- SUPERSCHALL ----
constexpr double ssPunchDb = 5.5, ssAnchorHz = 600.0, ssAnchorLowHz = 360.0;   // anchor moves down as PUNCH rises
constexpr double ssEdgeLowHz = 1800.0, ssEdgeHighHz = 5500.0, ssEdgeBellDb = 3.0;
constexpr double ssDepthHz = 90.0, ssDepthDb = 6.0, ssPresHz = 4500.0, ssPresDb = 5.0;
constexpr double ssSubHpfHz = 35.0, ssFizzLpHz = 10000.0;         // keeps extreme subs and fizz out when the module is active
// ---- level match ----
constexpr double lmLoHz = 80.0, lmHiHz = 8000.0;
}

enum Arch { aBax = 0, aBbc, aRot, aSuper, kNumArch };
inline const char* archName (int a) { static const char* n[] = { "BAXENDALL", "BBC EQ", "ROTARD", "SUPERSCHALL" }; return n[a]; }
constexpr int kMaxGroups = 4;
inline int numGroups (int a) { static const int n[] = { 4, 3, 4, 3 }; return n[a]; }
inline const char* groupName (int a, int g)
{
    static const char* n[4][4] = { { "Bax shelves", "Mid", "Lean", "Contour/Vibe" }, { "Body", "Bright", "Cut", "" },
                                   { "Bass", "Low mid", "High mid", "Treble" },       { "Punch/Edge", "Depth", "Presence", "" } };
    return n[a][g];
}

struct Params
{
    int   arch = aBax;
    bool  levelMatch = false;
    float output = 0.0f;                                   // dB
    // BAXENDALL
    float bxLow = 0, bxHigh = 0, bxMid = 0, bxSweep = 0.5f, bxLean = 0, bxFeel = 0.5f;   // -1..1 (sweep/feel 0..1)
    int   bxLowC = 2, bxHighC = 1, bxRoute = 1, bxVibe = 1, bxQ = 0;   // LowC 0..4 (2 = flat), HighC 0..4 (1 = flat), Route 0..2 (0 pre, 1 post, 2 parallel), Vibe 0 dark/1 bright, Q 0 broad/1 narrow
    // BBC EQ
    float bbBody = 0, bbBright = 0, bbCut = 0;
    // ROTARD
    float rtBass = 0, rtLowMid = 0, rtHighMid = 0, rtTreble = 0;
    // SUPERSCHALL
    float ssPunch = 0, ssEdge = 0, ssDepth = 0, ssPresence = 0;
};

// one-pole smoothing of the continuous knobs (steps and toggles change immediately)
inline void smoothToward (Params& cur, const Params& t, double a)
{
    auto s = [a] (float& c, float v) { c += (float) ((v - c) * a); };
    cur.arch = t.arch; cur.levelMatch = t.levelMatch;
    cur.bxLowC = t.bxLowC; cur.bxHighC = t.bxHighC; cur.bxRoute = t.bxRoute; cur.bxVibe = t.bxVibe; cur.bxQ = t.bxQ;
    s (cur.output, t.output);
    s (cur.bxLow, t.bxLow); s (cur.bxHigh, t.bxHigh); s (cur.bxMid, t.bxMid); s (cur.bxSweep, t.bxSweep); s (cur.bxLean, t.bxLean); s (cur.bxFeel, t.bxFeel);
    s (cur.bbBody, t.bbBody); s (cur.bbBright, t.bbBright); s (cur.bbCut, t.bbCut);
    s (cur.rtBass, t.rtBass); s (cur.rtLowMid, t.rtLowMid); s (cur.rtHighMid, t.rtHighMid); s (cur.rtTreble, t.rtTreble);
    s (cur.ssPunch, t.ssPunch); s (cur.ssEdge, t.ssEdge); s (cur.ssDepth, t.ssDepth); s (cur.ssPresence, t.ssPresence);
}

struct Coef { double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0; };

namespace rbj
{
inline Coef norm (double b0, double b1, double b2, double a0, double a1, double a2)
{ Coef c; c.b0 = b0 / a0; c.b1 = b1 / a0; c.b2 = b2 / a0; c.a1 = a1 / a0; c.a2 = a2 / a0; return c; }
inline double lim (double f, double sr) { return std::min (std::max (f, 15.0), 0.45 * sr); }
inline Coef peak (double sr, double f, double Q, double dB)
{ const double A = std::pow (10.0, dB / 40), w = 2 * kPi * lim (f, sr) / sr, c = std::cos (w), al = std::sin (w) / (2 * Q);
  return norm (1 + al * A, -2 * c, 1 - al * A, 1 + al / A, -2 * c, 1 - al / A); }
inline Coef lowShelf (double sr, double f, double Q, double dB)
{ const double A = std::pow (10.0, dB / 40), w = 2 * kPi * lim (f, sr) / sr, c = std::cos (w), s = 2 * std::sqrt (A) * std::sin (w) / (2 * Q);
  return norm (A * ((A + 1) - (A - 1) * c + s), 2 * A * ((A - 1) - (A + 1) * c), A * ((A + 1) - (A - 1) * c - s),
               (A + 1) + (A - 1) * c + s, -2 * ((A - 1) + (A + 1) * c), (A + 1) + (A - 1) * c - s); }
inline Coef highShelf (double sr, double f, double Q, double dB)
{ const double A = std::pow (10.0, dB / 40), w = 2 * kPi * lim (f, sr) / sr, c = std::cos (w), s = 2 * std::sqrt (A) * std::sin (w) / (2 * Q);
  return norm (A * ((A + 1) + (A - 1) * c + s), -2 * A * ((A - 1) + (A + 1) * c), A * ((A + 1) + (A - 1) * c - s),
               (A + 1) - (A - 1) * c + s, 2 * ((A - 1) - (A + 1) * c), (A + 1) - (A - 1) * c - s); }
inline Coef highPass (double sr, double f, double Q)
{ const double w = 2 * kPi * lim (f, sr) / sr, c = std::cos (w), al = std::sin (w) / (2 * Q);
  return norm ((1 + c) / 2, -(1 + c), (1 + c) / 2, 1 + al, -2 * c, 1 - al); }
inline Coef lowPass (double sr, double f, double Q)
{ const double w = 2 * kPi * lim (f, sr) / sr, c = std::cos (w), al = std::sin (w) / (2 * Q);
  return norm ((1 - c) / 2, 1 - c, (1 - c) / 2, 1 + al, -2 * c, 1 - al); }
inline Coef lowPass1 (double sr, double f)
{ const double K = std::tan (kPi * lim (f, sr) / sr); return norm (K, K, 0, 1 + K, K - 1, 0); }
}

constexpr int kMaxF = 16;
struct Series
{
    std::array<Coef, kMaxF> c;
    std::array<int,  kMaxF> grp {};
    std::array<bool, kMaxF> on {};
    int n = 0;
    void add (int g, const Coef& co, bool enable) { if (n < kMaxF) { c[(size_t) n] = co; grp[(size_t) n] = g; on[(size_t) n] = enable; ++n; } }
};

struct Model
{
    int arch = aBax, route = 1;
    Series main, mid;                     // BAX uses both (mid = the mid-band path); others use main
    Coef lpS[3], hpS[3];                  // ROTARD: Butterworth sections for the three crossovers (each LR4 = two in cascade)
    Coef res[4]; bool resOn[4] = { false, false, false, false };
    double outGain = 1.0;                 // output + level match
    double levelDb = 0.0;
};

// ---------- complex responses ----------
inline cd H (const Coef& c, double f, double sr)
{
    const double w = 2 * kPi * f / sr;
    const cd z1 = std::polar (1.0, -w), z2 = std::polar (1.0, -2 * w);
    return (c.b0 + c.b1 * z1 + c.b2 * z2) / (1.0 + c.a1 * z1 + c.a2 * z2);
}
inline cd seriesH (const Series& s, double f, double sr, int onlyGroup = -1)
{
    cd r = 1.0;
    for (int i = 0; i < s.n; ++i) if (s.on[(size_t) i] && (onlyGroup < 0 || s.grp[(size_t) i] == onlyGroup)) r *= H (s.c[(size_t) i], f, sr);
    return r;
}
inline cd lr4LP (const Model& m, int k, double f, double sr) { const cd h = H (m.lpS[k], f, sr); return h * h; }
inline cd lr4HP (const Model& m, int k, double f, double sr) { const cd h = H (m.hpS[k], f, sr); return h * h; }
inline cd rotBand (const Model& m, int band, double f, double sr)   // each band including its resonance filter
{
    const cd ap1 = lr4LP (m, 0, f, sr) + lr4HP (m, 0, f, sr), ap3 = lr4LP (m, 2, f, sr) + lr4HP (m, 2, f, sr);
    cd h;
    switch (band)
    {
        case 0:  h = lr4LP (m, 1, f, sr) * lr4LP (m, 0, f, sr) * ap3; break;      // bass     = LP2 . LP1 . AP3
        case 1:  h = lr4LP (m, 1, f, sr) * lr4HP (m, 0, f, sr) * ap3; break;      // low mid  = LP2 . HP1 . AP3
        case 2:  h = lr4HP (m, 1, f, sr) * lr4LP (m, 2, f, sr) * ap1; break;      // high mid = HP2 . LP3 . AP1
        default: h = lr4HP (m, 1, f, sr) * lr4HP (m, 2, f, sr) * ap1; break;      // treble   = HP2 . HP3 . AP1
    }
    if (m.resOn[band]) h *= H (m.res[band], f, sr);
    return h;
}
inline cd totalH (const Model& m, double f, double sr)
{
    cd r;
    switch (m.arch)
    {
        case aBax: { const cd A = seriesH (m.main, f, sr), M = seriesH (m.mid, f, sr);
                     r = (m.route == 2) ? A + (M - 1.0) : A * M; break; }
        case aRot: r = rotBand (m, 0, f, sr) + rotBand (m, 1, f, sr) + rotBand (m, 2, f, sr) + rotBand (m, 3, f, sr); break;
        default:   r = seriesH (m.main, f, sr); break;
    }
    return r * m.outGain;
}
inline double db (cd h) { return 10.0 * std::log10 (std::norm (h) + 1e-30); }
inline double totalDb (const Model& m, double f, double sr) { return db (totalH (m, f, sr)); }
inline double groupDb (const Model& m, int g, double f, double sr)
{
    switch (m.arch)
    {
        case aBax: { const int ids[4] = { 0, 1, 2, 3 };
                     const cd h = seriesH (m.main, f, sr, ids[g]) * seriesH (m.mid, f, sr, ids[g]); return db (h); }
        case aRot: return db (rotBand (m, g, f, sr));
        default:   return db (seriesH (m.main, f, sr, g));
    }
}

// ---------- model construction ----------
inline double clampd (double v, double lo, double hi) { return std::min (std::max (v, lo), hi); }

inline Model makeModel (const Params& p, double sr)
{
    using namespace cfg;
    Model m; m.arch = p.arch; m.route = p.bxRoute;
    const double eps = 1e-4;

    if (p.arch == aBax)
    {
        const double v = p.bxVibe ? 1.0 : -1.0;
        const double lowC  = bxLowHz  * (v > 0 ? bxBrightCornerMul : bxDarkCornerMul);
        const double highC = bxHighHz * (v > 0 ? bxBrightCornerMul : bxDarkCornerMul);
        const double fc = bxSweepMinHz * std::pow (bxSweepMaxHz / bxSweepMinHz, (double) p.bxSweep) * (v > 0 ? bxBrightMul : bxDarkMul);
        const double midDb = p.bxMid * bxMidDb;
        double Q = p.bxQ ? bxQNarrow : bxQBroad;
        if (p.bxRoute == 1)   // POST-BAX: the mid stage loads the Bax network (conceptual interaction)
            Q *= 1.0 + bxPostLoading * clampd ((std::max (p.bxLow, 0.0f) + std::max (p.bxHigh, 0.0f)) / 2.0, 0.0, 1.0);
        // NOTE: route 0 = PRE-BAX, 1 = POST-BAX, 2 = PARALLEL
        const double asym = clampd (p.bxFeel * std::abs (p.bxLean), 0.0, 1.0);
        const bool   up = p.bxLean > 0;
        m.mid.add (1, rbj::peak (sr, fc, Q, midDb * (1.0 - bxBellShare * asym)), std::abs (midDb) > eps);
        m.mid.add (2, rbj::peak (sr, up ? fc * bxSkirtShift : fc / bxSkirtShift, bxSkirtQ, midDb * bxSkirtAmount * asym), std::abs (midDb * asym) > eps);
        m.mid.add (2, rbj::peak (sr, up ? fc / bxShortShift : fc * bxShortShift, bxShortQ, -midDb * bxShortAmount * asym), std::abs (midDb * asym) > eps);

        m.main.add (0, rbj::lowShelf  (sr, lowC,  bxShelfQ, p.bxLow  * bxShelfDb), std::abs (p.bxLow)  > eps);
        m.main.add (0, rbj::highShelf (sr, highC, bxShelfQ, p.bxHigh * bxShelfDb), std::abs (p.bxHigh) > eps);
        m.main.add (3, rbj::highShelf (sr, 2000.0, 0.6, v * bxVibeTiltDb), true);
        switch (p.bxLowC)   // LOW CONTOUR (stepped): 0 loose, 1 relaxed, 2 flat, 3 firm, 4 tight
        {
            case 0: m.main.add (3, rbj::lowShelf (sr, 120, 0.6, 2.5), true);  m.main.add (3, rbj::peak (sr, 300, 0.8, -1.2), true); break;
            case 1: m.main.add (3, rbj::lowShelf (sr, 120, 0.6, 1.2), true);  m.main.add (3, rbj::peak (sr, 300, 0.8, -0.6), true); break;
            case 3: m.main.add (3, rbj::highPass (sr, 70, 0.707), true);      m.main.add (3, rbj::peak (sr, 140, 1.1, 1.5), true); break;
            case 4: m.main.add (3, rbj::highPass (sr, 110, 0.707), true);     m.main.add (3, rbj::peak (sr, 150, 1.1, 2.5), true); break;
            default: break;
        }
        switch (p.bxHighC)  // HIGH CONTOUR (stepped): 0 smooth, 1 flat, 2 air, 3 sparkle, 4 edge
        {
            case 0: m.main.add (3, rbj::lowPass1 (sr, 9000), true);  m.main.add (3, rbj::highShelf (sr, 6000, 0.6, -2.0), true); break;
            case 2: m.main.add (3, rbj::highShelf (sr, 10000, 0.5, 2.5), true); break;
            case 3: m.main.add (3, rbj::peak (sr, 7000, 1.2, 3.0), true);   m.main.add (3, rbj::highShelf (sr, 12000, 0.6, 1.5), true); break;
            case 4: m.main.add (3, rbj::peak (sr, 3200, 1.4, 3.5), true);   m.main.add (3, rbj::lowPass1 (sr, 9500), true); break;
            default: break;
        }
    }
    else if (p.arch == aBbc)
    {
        m.main.add (0, rbj::lowShelf (sr, bbBodyHz, 0.6, p.bbBody * bbBodyDb), std::abs (p.bbBody) > eps);
        m.main.add (0, rbj::peak (sr, bbBodyBellHz, 0.7, p.bbBody * bbBodyBellDb), std::abs (p.bbBody) > eps);
        m.main.add (1, rbj::highShelf (sr, bbBrightHz, 0.6, p.bbBright * bbBrightDb), std::abs (p.bbBright) > eps);
        m.main.add (1, rbj::peak (sr, bbSparkHz, 0.8, p.bbBright * bbSparkDb), std::abs (p.bbBright) > eps);
        const double lp = bbCutLpMax * std::pow (bbCutLpMin / bbCutLpMax, (double) p.bbCut);
        m.main.add (2, rbj::lowPass1 (sr, lp), p.bbCut > eps);
        m.main.add (2, rbj::highShelf (sr, bbCutPresHz, 0.6, bbCutPresDb * p.bbCut), p.bbCut > eps);
    }
    else if (p.arch == aSuper)
    {
        const double P = p.ssPunch * ssPunchDb;
        const double fL = ssAnchorHz * std::pow (ssAnchorLowHz / ssAnchorHz, (double) p.ssPunch);   // anchor moves lower as PUNCH rises
        const double fH = ssEdgeLowHz * std::pow (ssEdgeHighHz / ssEdgeLowHz, (double) p.ssEdge);   // upper boundary of the upfront voice
        const double qH = 0.5 + 0.5 * p.ssEdge;                                                     // boundary slope
        m.main.add (0, rbj::highShelf (sr, fL, 0.7, +P), p.ssPunch > eps);
        m.main.add (0, rbj::highShelf (sr, fH, qH, -P), p.ssPunch > eps);
        m.main.add (0, rbj::peak (sr, fH * 0.8, 1.2, p.ssEdge * ssEdgeBellDb), p.ssEdge > eps);
        const bool active = p.ssPunch > eps || p.ssEdge > eps || std::abs (p.ssDepth) > eps || std::abs (p.ssPresence) > eps;
        m.main.add (1, rbj::lowShelf (sr, ssDepthHz, 0.5, p.ssDepth * ssDepthDb), std::abs (p.ssDepth) > eps);
        m.main.add (1, rbj::highPass (sr, ssSubHpfHz, 0.707), active);
        m.main.add (2, rbj::highShelf (sr, ssPresHz, 0.6, p.ssPresence * ssPresDb), std::abs (p.ssPresence) > eps);
        m.main.add (2, rbj::lowPass1 (sr, ssFizzLpHz), active);
    }
    else // aRot
    {
        const double xs[3] = { rtX1, rtX2, rtX3 };
        for (int k = 0; k < 3; ++k) { m.lpS[k] = rbj::lowPass (sr, xs[k], 0.7071067811865476); m.hpS[k] = rbj::highPass (sr, xs[k], 0.7071067811865476); }
        const double r[4] = { p.rtBass, p.rtLowMid, p.rtHighMid, p.rtTreble };
        for (int b = 0; b < 4; ++b)
        {
            m.res[b] = rbj::peak (sr, rtCentre[b], rtQLow + (rtQHigh - rtQLow) * r[b], rtResDb * r[b]);
            m.resOn[b] = r[b] > eps;
        }
    }

    // level match: remove the average gain of the response over a log-spaced grid (no detector, fully deterministic)
    double lm = 0.0;
    if (p.levelMatch)
    {
        Model tmp = m; tmp.outGain = 1.0;
        const int N = 40; double sum = 0;
        for (int i = 0; i < N; ++i) sum += totalDb (tmp, lmLoHz * std::pow (lmHiHz / lmLoHz, i / (double) (N - 1)), sr);
        lm = -sum / N;
    }
    m.levelDb = lm;
    m.outGain = std::pow (10.0, (p.output + lm) / 20.0);
    return m;
}

// ---------- audio ----------
struct Biquad
{
    double z1 = 0, z2 = 0;
    void reset() { z1 = z2 = 0; }
    double process (const Coef& c, double x) { const double y = c.b0 * x + z1; z1 = c.b1 * x - c.a1 * y + z2; z2 = c.b2 * x - c.a2 * y; return y; }
};

struct Channel
{
    std::array<Biquad, 40> st;
    void reset() { for (auto& b : st) b.reset(); }
};

inline double runSeries (const Series& s, Biquad* st, double x)
{ for (int i = 0; i < s.n; ++i) if (s.on[(size_t) i]) x = st[i].process (s.c[(size_t) i], x); return x; }

inline float processSample (const Model& m, Channel& ch, float xin)
{
    Biquad* st = ch.st.data();
    const double x = xin; double y = x;
    switch (m.arch)
    {
        case aBax:
        {
            if (m.route == 0)       y = runSeries (m.main, st, runSeries (m.mid, st + 16, x));            // PRE-BAX
            else if (m.route == 1)  y = runSeries (m.mid, st + 16, runSeries (m.main, st, x));            // POST-BAX
            else { const double a = runSeries (m.main, st, x), mm = runSeries (m.mid, st + 16, x); y = a + (mm - x); }   // PARALLEL
            break;
        }
        case aRot:
        {
            auto lr = [&] (int base, const Coef& c, double v) { return st[base + 1].process (c, st[base].process (c, v)); };   // LR4 = 2 x Butterworth
            const double L  = lr (0, m.lpS[1], x),  Hh = lr (2, m.hpS[1], x);
            const double Bs = lr (4, m.lpS[0], L),  LM = lr (6, m.hpS[0], L);
            const double HM = lr (8, m.lpS[2], Hh), Tr = lr (10, m.hpS[2], Hh);
            const double b  = lr (12, m.lpS[2], Bs) + lr (14, m.hpS[2], Bs);       // all-pass of crossover 3 on the low side
            const double lm = lr (16, m.lpS[2], LM) + lr (18, m.hpS[2], LM);
            const double hm = lr (20, m.lpS[0], HM) + lr (22, m.hpS[0], HM);       // all-pass of crossover 1 on the high side
            const double t  = lr (24, m.lpS[0], Tr) + lr (26, m.hpS[0], Tr);
            const double bands[4] = { b, lm, hm, t };
            y = 0;
            for (int i = 0; i < 4; ++i) y += m.resOn[i] ? st[28 + i].process (m.res[i], bands[i]) : bands[i];
            break;
        }
        default: y = runSeries (m.main, st, x); break;
    }
    return (float) (y * m.outGain);
}
} // namespace kwlab
