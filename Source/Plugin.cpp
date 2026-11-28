// Oktoplus Synth - additive / subtractive / granular synthesizer (VST3, JUCE 8)
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_dsp/juce_dsp.h>
#include <algorithm>
#include <BinaryData.h>
using namespace juce;
using APVTS = AudioProcessorValueTreeState;

namespace K { enum { aLvl, aPart, aTilt, aOdd, aSemi, aUni, aDet,  bLvl, bSize, bDens, bSpr, bSemi,  nLvl, nTone, nDec, nDens,
  cut, res, fEnv, fDrv, fBass, fKey,  fA, fD, fS, fR, pEnv,  aA, aD, aS, aR,  l1R, l1C, l1P,  l2R, l2A, l2B,  gld, ana, mst,
  sDrv, sTone, sMix, sOut,  tDrv, tAsym, tWarm, tMix,  pDep, pRel, pCrv,
  e1F, e1G, e1Q, e2F, e2G, e2Q, e3F, e3G, e3Q, e4F, e4G, e4Q, e5F, e5G, e5Q, e6F, e6G, e6Q,  dFb, dTone, dMix,  rSize, rDamp, rMix, NP }; }

struct PD { const char *id, *grp, *name; float mn, mx, def, sk; char u; };
static const PD kP[] = {
 {"aLvl","OSC A (ADDITIVE)","Level",0,1,.7f,1,'p'},{"aPart","OSC A (ADDITIVE)","Partials",1,32,16,1,'c'},{"aTilt","OSC A (ADDITIVE)","Tilt",0,1,.4f,1,'p'},
 {"aOdd","OSC A (ADDITIVE)","Odd",0,1,0,1,'p'},{"aSemi","OSC A (ADDITIVE)","Pitch",-24,24,0,1,'t'},{"aUni","OSC A (ADDITIVE)","Unison",0,1,0,1,'p'},
 {"aDet","OSC A (ADDITIVE)","Detune",0,50,10,1,'e'},
 {"bLvl","OSC B (GRANULAR)","Level",0,1,0,1,'p'},{"bSize","OSC B (GRANULAR)","Size",5,400,80,.5f,'m'},{"bDens","OSC B (GRANULAR)","Density",1,150,30,.5f,'n'},
 {"bSpr","OSC B (GRANULAR)","Spread",0,1,.3f,1,'p'},{"bSemi","OSC B (GRANULAR)","Pitch",-24,24,12,1,'t'},
 {"nLvl","NOISE","Level",0,1,0,1,'p'},{"nTone","NOISE","Tone",-1,1,0,1,'p'},{"nDec","NOISE","Decay",0,2,0,.4f,'s'},{"nDens","NOISE","Density",1,500,40,.4f,'n'},
 {"cut","FILTER","Cutoff",20,18000,2500,.3f,'h'},{"res","FILTER","Reso",0,1,.2f,1,'p'},{"fEnv","FILTER","Env Amt",-1,1,.4f,1,'p'},
 {"fDrv","FILTER","Drive",0,1,.2f,1,'p'},{"fBass","FILTER","Bass",0,1,.4f,1,'p'},{"fKey","FILTER","Key Trk",0,1,0,1,'p'},
 {"fA","FILTER ENV","A",.001f,5,.01f,.3f,'s'},{"fD","FILTER ENV","D",.001f,5,.4f,.3f,'s'},{"fS","FILTER ENV","S",0,1,.3f,1,'p'},{"fR","FILTER ENV","R",.001f,8,.4f,.3f,'s'},
 {"pEnv","FILTER ENV","Pitch Env",-48,48,0,1,'t'},
 {"aA","AMP ENV","A",.001f,5,.01f,.3f,'s'},{"aD","AMP ENV","D",.001f,5,.3f,.3f,'s'},{"aS","AMP ENV","S",0,1,.7f,1,'p'},{"aR","AMP ENV","R",.001f,8,.4f,.3f,'s'},
 {"l1R","LFO 1","Rate",.05f,20,2,.4f,'h'},{"l1C","LFO 1","Filter",0,1,0,1,'p'},{"l1P","LFO 1","Pitch",0,1,0,1,'p'},
 {"l2R","LFO 2","Rate",.05f,100,.5f,.4f,'h'},{"l2A","LFO 2","Amp",0,1,0,1,'p'},{"l2B","LFO 2","Grains",0,1,0,1,'p'},
 {"gld","GLOBAL","Glide",0,2,0,.4f,'s'},{"ana","GLOBAL","Analog",0,1,.3f,1,'p'},{"mst","GLOBAL","Volume",0,1,.7f,1,'p'},
 {"sDrv","SATURATION","Drive",0,1,.3f,1,'p'},{"sTone","SATURATION","Tone",1000,20000,20000,.3f,'h'},{"sMix","SATURATION","Mix",0,1,0,1,'p'},{"sOut","SATURATION","Output",-12,12,0,1,'d'},
 {"tDrv","TUBE PREAMP","Drive",0,1,.4f,1,'p'},{"tAsym","TUBE PREAMP","Bias",0,1,.4f,1,'p'},{"tWarm","TUBE PREAMP","Warmth",0,1,.5f,1,'p'},{"tMix","TUBE PREAMP","Mix",0,1,.6f,1,'p'},
 {"pDep","PUMP","Depth",0,1,0,1,'p'},{"pRel","PUMP","Release",.05f,1,.5f,1,'p'},{"pCrv","PUMP","Curve",0,1,.5f,1,'p'},
 {"e1F","EQ","B1 Freq",20,20000,30,.3f,'h'},{"e1G","EQ","B1 Gain",-18,18,0,1,'d'},{"e1Q","EQ","B1 Q",.1f,12,.71f,.4f,'q'},
 {"e2F","EQ","B2 Freq",20,20000,120,.3f,'h'},{"e2G","EQ","B2 Gain",-18,18,0,1,'d'},{"e2Q","EQ","B2 Q",.1f,12,.71f,.4f,'q'},
 {"e3F","EQ","B3 Freq",20,20000,400,.3f,'h'},{"e3G","EQ","B3 Gain",-18,18,0,1,'d'},{"e3Q","EQ","B3 Q",.1f,12,1,.4f,'q'},
 {"e4F","EQ","B4 Freq",20,20000,1500,.3f,'h'},{"e4G","EQ","B4 Gain",-18,18,0,1,'d'},{"e4Q","EQ","B4 Q",.1f,12,1,.4f,'q'},
 {"e5F","EQ","B5 Freq",20,20000,5000,.3f,'h'},{"e5G","EQ","B5 Gain",-18,18,0,1,'d'},{"e5Q","EQ","B5 Q",.1f,12,1,.4f,'q'},
 {"e6F","EQ","B6 Freq",20,20000,10000,.3f,'h'},{"e6G","EQ","B6 Gain",-18,18,0,1,'d'},{"e6Q","EQ","B6 Q",.1f,12,.71f,.4f,'q'},
 {"dFb","ECHO","Feedback",0,.95f,.4f,1,'p'},{"dTone","ECHO","Tone",0,1,.6f,1,'p'},{"dMix","ECHO","Mix",0,1,0,1,'p'},
 {"rSize","REVERB","Size",0,1,.5f,1,'p'},{"rDamp","REVERB","Damp",0,1,.5f,1,'p'},{"rMix","REVERB","Mix",0,1,0,1,'p'}};

static const char* kEqT[6] = {"e1T", "e2T", "e3T", "e4T", "e5T", "e6T"};
static const char* kEqO[6] = {"e1O", "e2O", "e3O", "e4O", "e5O", "e6O"};
static const char* kX[] = {"l1S", "l2S", "fT", "fSl", "fCar", "nTyp", "sMode", "l1Y", "l2Y", "bY", "pY", "dY", "leg", "tOn", "eOn", "dPP",
  "e1T", "e1O", "e2T", "e2O", "e3T", "e3O", "e4T", "e4O", "e5T", "e5O", "e6T", "e6O"};

#include "Presets.h"

static String fmt(char u, float v) {
  switch (u) {
    case 'h': return v >= 1000.f ? String(v / 1000.f, 2) + " kHz" : String(v, v < 100.f ? 1 : 0) + " Hz";
    case 's': return String(v, v < 1.f ? 3 : 2) + " s";
    case 'm': return String((int) v) + " ms";
    case 't': return String((int) std::round(v)) + " st";
    case 'n': return String((int) std::round(v)) + " /s";
    case 'p': return String((int) std::round(v * 100.f)) + " %";
    case 'e': return String((int) std::round(v)) + " ct";
    case 'd': return String(v, 1) + " dB";
    case 'q': return String(v, 2);
    case 'c': return String((int) std::round(v));
    default:  return String(v, 2); } }
static float parseVal(char u, const String& t) {
  float v = t.retainCharacters("0123456789.-").getFloatValue(); auto s = t.toLowerCase();
  switch (u) { case 'p': return v / 100.f; case 'h': return s.contains("k") ? v * 1000.f : v; case 's': return s.contains("ms") ? v / 1000.f : v; default: return v; } }

static StringArray syncNames(bool withFree) {
  StringArray s; if (withFree) s.add("Free");
  const char* d[] = {"1/1", "1/2", "1/4", "1/8", "1/16", "1/32", "1/64", "1/128"};
  for (auto* n : d) { s.add(n); s.add(String(n) + " D"); s.add(String(n) + " T"); }
  return s; }
static float beatsOf(int sy) {  // sy >= 1 : 1 = 1/1, 2 = 1/1 dotted, 3 = 1/1 triplet, 4 = 1/2 ...
  static const float len[8] = {4.f, 2.f, 1.f, .5f, .25f, .125f, .0625f, .03125f};
  int d = jlimit(0, 7, (sy - 1) / 3), t = (sy - 1) % 3; return len[d] * (t == 1 ? 1.5f : t == 2 ? 2.f / 3.f : 1.f); }
static float rateOf(int sy, float freeHz, double bpm) { return sy <= 0 ? freeHz : (float) (bpm / 60.0) / beatsOf(sy); }
static void makeCycle(float* o, int n, float P, float T, float O) {
  int N = jlimit(1, 32, (int) (P + .5f)); float s = .2f + T * 2.f, pk = 0, a[33];
  for (int k = 1; k <= N; ++k) a[k] = std::pow((float) k, -s) * ((k % 2 == 0) ? 1.f - O : 1.f);
  for (int i = 0; i < n; ++i) { float ph = MathConstants<float>::twoPi * (float) i / (float) n, y = 0;
    for (int k = 1; k <= N; ++k) y += a[k] * std::sin(ph * (float) k);
    o[i] = y; pk = jmax(pk, std::abs(y)); }
  if (pk > 0) for (int i = 0; i < n; ++i) o[i] /= pk; }

using Cf = dsp::IIR::Coefficients<float>;
static void bandCoefs(int ty, double sr, float f, float g, float q, Cf::Ptr& c1, Cf::Ptr& c2, bool& two) {
  f = jlimit(20.f, (float) (sr * .45), f); float gf = Decibels::decibelsToGain(g); two = false;
  switch (ty) {
    case 0: c1 = Cf::makePeakFilter(sr, f, q, gf); break;
    case 1: c1 = Cf::makeLowShelf(sr, f, q, gf); break;
    case 2: c1 = Cf::makeHighShelf(sr, f, q, gf); break;
    case 3: c1 = Cf::makeHighPass(sr, f, q); break;
    case 4: c1 = Cf::makeHighPass(sr, f, .5412f); c2 = Cf::makeHighPass(sr, f, 1.3066f); two = true; break;
    case 5: c1 = Cf::makeLowPass(sr, f, q); break;
    case 6: c1 = Cf::makeLowPass(sr, f, .5412f); c2 = Cf::makeLowPass(sr, f, 1.3066f); two = true; break;
    default: c1 = Cf::makeNotch(sr, f, q); } }

static APVTS::ParameterLayout layout() {
  APVTS::ParameterLayout L;
  for (auto& p : kP)
    L.add(std::make_unique<AudioParameterFloat>(ParameterID{p.id, 1}, String(p.grp) + " " + p.name, NormalisableRange<float>(p.mn, p.mx, 0.f, p.sk), p.def,
      AudioParameterFloatAttributes().withStringFromValueFunction([u = p.u](float v, int) { return fmt(u, v); })
                                     .withValueFromStringFunction([u = p.u](const String& t) { return parseVal(u, t); })));
  auto ch = [&](const char* id, const String& nm, StringArray a, int d) { L.add(std::make_unique<AudioParameterChoice>(ParameterID{id, 1}, nm, a, d)); };
  StringArray sh{"Sine", "Triangle", "Saw", "Square", "Random"};
  ch("l1S", "LFO 1 shape", sh, 0); ch("l2S", "LFO 2 shape", sh, 0);
  ch("fT", "Filter mode", StringArray{"LP", "HP", "BP", "Notch"}, 0); ch("fSl", "Filter slope", StringArray{"12 dB", "24 dB"}, 1);
  ch("fCar", "Filter character", StringArray{"Smooth", "Scream"}, 0);
  ch("nTyp", "Noise type", StringArray{"White", "Pink", "Brown", "Blue", "Violet", "Crackle"}, 0);
  ch("sMode", "Saturation mode", StringArray{"Soft", "Asym", "Clip", "Fold"}, 0);
  ch("l1Y", "LFO 1 sync", syncNames(true), 0); ch("l2Y", "LFO 2 sync", syncNames(true), 0); ch("bY", "Grains sync", syncNames(true), 0);
  ch("pY", "Pump sync", syncNames(false), 6); ch("dY", "Echo time", syncNames(false), 10);
  L.add(std::make_unique<AudioParameterBool>(ParameterID{"leg", 1}, "Legato", false));
  L.add(std::make_unique<AudioParameterBool>(ParameterID{"tOn", 1}, "Tube active", false));
  L.add(std::make_unique<AudioParameterBool>(ParameterID{"eOn", 1}, "EQ active", true));
  L.add(std::make_unique<AudioParameterBool>(ParameterID{"dPP", 1}, "Echo ping-pong", false));
  StringArray et{"Bell", "Low Shelf", "High Shelf", "Low Cut 12", "Low Cut 24", "High Cut 12", "High Cut 24", "Notch"};
  const int defT[6] = {4, 1, 0, 0, 0, 2};
  for (int b = 0; b < 6; ++b) { ch(kEqT[b], "EQ B" + String(b + 1) + " type", et, defT[b]);
    L.add(std::make_unique<AudioParameterBool>(ParameterID{kEqO[b], 1}, "EQ B" + String(b + 1) + " on", false)); }
  return L;
}

// <LADDER>
static inline float ft(float x) { x = jlimit(-3.f, 3.f, x); float x2 = x * x; return x * (27.f + x2) / (27.f + 9.f * x2); }
static inline float fa(float x) { return x >= 0.f ? ft(x) : ft(x * 2.2f) * .4545f; }
// 4-pole ladder, zero-delay-feedback (trapezoidal) integration, saturation between every pole and inside the feedback path
struct Ladder { float s0 = 0, s1 = 0, s2 = 0, s3 = 0;
  inline float run(float in, float g, float k, int car, const float* c) {
    float ig = 1.f - g, g2 = g * g, G = g2 * g2;
    float S = ig * (g2 * g * s0 + g2 * s1 + g * s2 + s3);
    float x0 = (in - k * S) / (1.f + k * G);
    float y3a = G * x0 + S;
    x0 = in - k * (car ? fa(y3a * 1.3f) : ft(y3a));
    float a0 = car ? fa(x0) : ft(x0);
    float y0 = g * a0 + ig * s0, a1 = ft(y0);
    float y1 = g * a1 + ig * s1, a2 = ft(y1);
    float y2 = g * a2 + ig * s2, a3 = ft(y2);
    float y3 = g * a3 + ig * s3;
    s0 = 2.f * y0 - s0; s1 = 2.f * y1 - s1; s2 = 2.f * y2 - s2; s3 = 2.f * y3 - s3;
    return jlimit(-8.f, 8.f, c[0] * a0 + c[1] * y0 + c[2] * y1 + c[3] * y2 + c[4] * y3); } };
// </LADDER>
static const float kMix[4][2][5] = {
  {{0, 0, 1, 0, 0}, {0, 0, 0, 0, 1}}, {{1, -2, 1, 0, 0}, {1, -4, 6, -4, 1}}, {{0, 2, -2, 0, 0}, {0, 0, 4, -8, 4}}, {{1, -2, 2, 0, 0}, {1, -4, 6, -4, 2}}};
// output level calibration [character][mode][slope], measured so that every mode has the same loudness
static const float kOut[2][4][2] = {{{0.79f, 0.98f}, {3.44f, 2.21f}, {1.59f, 3.68f}, {0.99f, 1.44f}},
                                    {{0.96f, 1.15f}, {4.97f, 3.31f}, {2.23f, 5.39f}, {1.13f, 1.43f}}};

struct Grain { bool on = false; float ph = 0, inc = 0, len = 1, age = 0; const float* tb = nullptr; };
struct Voice { bool on = false, gate = false; int note = -1;
  float freq = 440, tgt = 440, pA = 0, pA2 = 0, pA3 = 0, gAcc = 0, vel = 1, age = 0, dr1 = 0, dr2 = 0, k0 = 0, k1 = 0, k2 = 0, nb = 0, pp = 0, w1 = 0, nlp = 0, ne = 1, cr = 0;
  ADSR ea, ef; Ladder lad; Grain g[24]; };
struct LFO { float ph = 0, sh = 0; };
struct Fx { float lp = 0, d1x = 0, d1y = 0, d2x = 0, d2y = 0, tl = 0, th = 0; };

class Proc : public AudioProcessor {
public:
  Proc() : AudioProcessor(BusesProperties().withOutput("Out", AudioChannelSet::stereo(), true)), apvts(*this, nullptr, "ST", layout()) {
    for (int i = 0; i < 2048; ++i) sinT[i] = std::sin(MathConstants<float>::twoPi * (float) i / 2048.f);
    apvts.state.setProperty("presetName", "Initialization", nullptr); }
  const String getName() const override { return "Oktoplus Synth"; }
  void prepareToPlay(double sr, int blk) override {
    fs = sr; maxB = jmax(blk, 64);
    sIn.assign((size_t) maxB, 0.f); sG = sIn; sAe = sIn; sL1 = sIn; sL2 = sIn;
    for (auto& o : os) { o = std::make_unique<dsp::Oversampling<float>>(1, 1, dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, false); o->initProcessing((size_t) maxB); }
    osM = std::make_unique<dsp::Oversampling<float>>(2, 1, dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, false); osM->initProcessing((size_t) maxB);
    for (auto& v : vs) { v = Voice(); v.ea.setSampleRate(sr); v.ef.setSampleRate(sr); }
    stack.clear(); lastF = 261.6f; cf[0] = cf[1] = Fx(); eqSig[0] = -999.f; pumpPh = 0; pumpG = 1;
    dlSize = (int) (sr * 4.0) + 8; for (auto& d : dl) d.assign((size_t) dlSize, 0.f); dw = 0; dfl[0] = dfl[1] = 0; dCur = (float) sr * .25f;
    for (auto& c : eq) for (auto& b : c) for (auto& f : b) f.reset();
    rev.reset(); rev.setSampleRate(sr);
    rebuild(true); setLatencySamples(roundToInt(os[0]->getLatencyInSamples() + osM->getLatencyInSamples())); }
  void releaseResources() override {}
  bool isBusesLayoutSupported(const BusesLayout& l) const override { return l.getMainOutputChannelSet() == AudioChannelSet::stereo(); }
  bool acceptsMidi() const override { return true; }
  bool producesMidi() const override { return false; }
  double getTailLengthSeconds() const override { return 8.0; }
  AudioProcessorEditor* createEditor() override;
  bool hasEditor() const override { return true; }
  int getNumPrograms() override { return (int) kPre.size(); }
  int getCurrentProgram() override { return cur; }
  const String getProgramName(int i) override { return String(kPre[(size_t) i].n); }
  String getProgramCategory(int i) const { return String(kPre[(size_t) i].cat); }
  void changeProgramName(int, const String&) override {}
  void setCurrentProgram(int i) override {
    cur = i;
    for (auto* p : getParameters()) p->setValueNotifyingHost(p->getDefaultValue());
    for (auto& kv : kPre[(size_t) i].v) { auto* p = apvts.getParameter(kv.first); if (p) p->setValueNotifyingHost(p->convertTo0to1(kv.second)); }
    apvts.state.setProperty("presetName", String(kPre[(size_t) i].n), nullptr); }
  String currentName() { return apvts.state.getProperty("presetName", "Initialization").toString(); }
  static File presetDir() { auto d = File::getSpecialLocation(File::userApplicationDataDirectory).getChildFile("Oktoplus").getChildFile("Presets Oktoplus Synth"); d.createDirectory(); return d; }
  bool saveUser(const String& name) {
    apvts.state.setProperty("presetName", name, nullptr);
    auto f = presetDir().getChildFile(File::createLegalFileName(name) + ".xml");
    if (auto x = apvts.copyState().createXml()) return x->writeTo(f);
    return false; }
  void loadUser(const File& f) { if (auto x = parseXML(f)) { apvts.replaceState(ValueTree::fromXml(*x)); apvts.state.setProperty("presetName", f.getFileNameWithoutExtension(), nullptr); } }
  void getStateInformation(MemoryBlock& b) override { if (auto x = apvts.copyState().createXml()) copyXmlToBinary(*x, b); }
  void setStateInformation(const void* d, int n) override { if (auto x = getXmlFromBinary(d, n)) apvts.replaceState(ValueTree::fromXml(*x)); }
  void copySpectrumInput(float* dst) const { int w = specW.load(); for (int i = 0; i < 4096; ++i) dst[i] = specBuf[(w + i) & 4095]; }

  void processBlock(AudioBuffer<float>& b, MidiBuffer& m) override {
    ScopedNoDenormals nd; b.clear(); rebuild(false);
    for (int i = 0; i < K::NP; ++i) q[i] = apvts.getRawParameterValue(kP[i].id)->load();
    auto gi = [&](const char* id) { return (int) apvts.getRawParameterValue(id)->load(); };
    sh1 = gi("l1S"); sh2 = gi("l2S"); fTy = gi("fT"); fSl = gi("fSl"); fCar = gi("fCar"); nTy = gi("nTyp"); sMd = gi("sMode"); pSy = gi("pY"); dSy = gi("dY"); dPPb = gi("dPP") > 0;
    leg = gi("leg") > 0; tOnB = gi("tOn") > 0; eqOn = gi("eOn") > 0;
    double bpm = 120.0, ppq = 0; bool havePpq = false;
    if (auto* ph = getPlayHead()) if (auto info = ph->getPosition()) {
      if (auto bp = info->getBpm()) bpm = *bp;
      if (info->getIsPlaying()) if (auto pq = info->getPpqPosition()) { havePpq = true; ppq = *pq; } }
    bpmNow = (float) bpm; pumpL = beatsOf(pSy + 1); if (havePpq) pumpPh = (float) std::fmod(ppq / (double) pumpL, 1.0);
    rt1 = rateOf(gi("l1Y"), q[K::l1R], bpm); rt2 = rateOf(gi("l2Y"), q[K::l2R], bpm); dens = rateOf(gi("bY"), q[K::bDens], bpm);
    for (auto& v : vs) { v.ea.setParameters({q[K::aA], q[K::aD], q[K::aS], q[K::aR]}); v.ef.setParameters({q[K::fA], q[K::fD], q[K::fS], q[K::fR]}); }
    prepMaster(); eqUpdate();
    int pos = 0, n = b.getNumSamples();
    for (const auto md : m) { int ts = jlimit(0, n, md.samplePosition); render(b, pos, ts); pos = ts; handle(md.getMessage()); }
    render(b, pos, n); master(b);
    int cnt = 0; for (auto& v : vs) cnt += v.on ? 1 : 0; nVoices = cnt; }

  APVTS apvts; std::atomic<int> nVoices{0}; std::atomic<float> outL{0}, outR{0}, tubeLvl{0};
private:
  double fs = 44100; int maxB = 512; Voice vs[8]; std::unique_ptr<dsp::Oversampling<float>> os[8], osM;
  std::vector<int> stack; std::vector<float> sIn, sG, sAe, sL1, sL2, dl[2]; float q[K::NP] {};
  int sh1 = 0, sh2 = 0, fTy = 0, fSl = 1, fCar = 0, nTy = 0, sMd = 0, pSy = 6, dSy = 10, cur = 0, dlSize = 1, dw = 0;
  bool leg = false, tOnB = false, eqOn = true, sOn = false, dPPb = false, eqB[6] {}, eq2[6] {};
  float rt1 = 2, rt2 = .5f, dens = 30, cP = -1, cT = -1, cO = -1, lastF = 261.6f, sinT[2048] {}, tabs[8][2048] {}, bpmNow = 120, pumpL = 1, pumpPh = 0, pumpG = 1;
  float sd = 1, sOc = 1, aTone = 1, td = 1, tb = 0, tNorm = 1, aLow = .01f, aHf = .5f, eqSig[30] {}, tubePk = 0, dCur = 12000, dfl[2] {};
  float specBuf[4096] {}; std::atomic<int> specW{0};
  Fx cf[2]; dsp::IIR::Filter<float> eq[2][6][2]; LFO a1, a2; Random rnd; Reverb rev;

  void rebuild(bool force) {
    float P = apvts.getRawParameterValue("aPart")->load(), T = apvts.getRawParameterValue("aTilt")->load(), O = apvts.getRawParameterValue("aOdd")->load();
    if (!force && P == cP && T == cT && O == cO) return;
    cP = P; cT = T; cO = O;
    int N = jlimit(1, 32, (int) (P + .5f)); float s = .2f + T * 2.f, a[33], pk = 0;
    for (int k = 1; k <= N; ++k) a[k] = std::pow((float) k, -s) * ((k % 2 == 0) ? 1.f - O : 1.f);
    for (int b = 0; b < 8; ++b) {
      int nm = jlimit(1, N, (int) ((float) fs * .45f / (32.f * std::pow(2.f, (float) b + 1.f))));
      for (int i = 0; i < 2048; ++i) { float y = 0; for (int k = 1; k <= nm; ++k) y += a[k] * sinT[(k * i) & 2047]; tabs[b][i] = y; if (b == 0) pk = jmax(pk, std::abs(y)); } }
    if (pk > 0) for (auto& t : tabs) for (auto& v : t) v /= pk; }
  static int bandOf(float f) { return jlimit(0, 7, (int) std::floor(std::log2(jmax(f, 1.f) / 32.f))); }
  static float rd(const float* T, float ph) { float x = ph * 2048.f; int i = (int) x & 2047; float fr = x - std::floor(x); return T[i] + (T[(i + 1) & 2047] - T[i]) * fr; }
  float lfo(LFO& l, float rate, int shape) {
    l.ph += rate / (float) fs; if (l.ph >= 1.f) { l.ph -= 1.f; l.sh = rnd.nextFloat() * 2.f - 1.f; }
    switch (shape) { case 0: return std::sin(6.2831853f * l.ph); case 1: return 4.f * std::abs(l.ph - .5f) - 1.f;
      case 2: return 2.f * l.ph - 1.f; case 3: return l.ph < .5f ? 1.f : -1.f; default: return l.sh; } }
  void start(Voice& v, int n, float f, float vel) {
    int vi = (int) (&v - vs); os[vi]->reset();
    v.on = true; v.gate = true; v.note = n; v.age = 0; v.tgt = f; v.freq = q[K::gld] > .002f ? lastF : f; v.vel = vel; v.gAcc = 0;
    v.pA = q[K::ana] > .01f ? rnd.nextFloat() : 0.f; v.pA2 = rnd.nextFloat(); v.pA3 = rnd.nextFloat(); v.lad = Ladder();
    v.k0 = v.k1 = v.k2 = v.nb = v.pp = v.w1 = v.nlp = v.cr = 0.f; v.ne = 1.f;
    for (auto& g : v.g) g.on = false;
    v.ea.setParameters({q[K::aA], q[K::aD], q[K::aS], q[K::aR]}); v.ef.setParameters({q[K::fA], q[K::fD], q[K::fS], q[K::fR]});
    v.ea.noteOn(); v.ef.noteOn(); }
  void release(Voice& v) { v.gate = false; v.ea.noteOff(); v.ef.noteOff(); }
  void handle(const MidiMessage& m) {
    if (m.isNoteOn()) {
      int n = m.getNoteNumber(); float f = 440.f * std::pow(2.f, (float) (n - 69) / 12.f), vel = m.getFloatVelocity();
      if (leg) { bool had = !stack.empty(); stack.push_back(n); Voice& v = vs[0]; v.tgt = f; if (!had || !v.on) start(v, n, f, vel); else v.note = n; }
      else { Voice* v = nullptr; for (auto& x : vs) if (!x.on) { v = &x; break; }
        if (!v) { v = &vs[0]; for (auto& x : vs) if (x.age > v->age) v = &x; }
        start(*v, n, f, vel); }
      lastF = f; }
    else if (m.isNoteOff()) {
      int n = m.getNoteNumber();
      if (leg) { stack.erase(std::remove(stack.begin(), stack.end(), n), stack.end()); Voice& v = vs[0];
        if (stack.empty()) release(v); else { int k = stack.back(); v.tgt = 440.f * std::pow(2.f, (float) (k - 69) / 12.f); v.note = k; } }
      else for (auto& v : vs) if (v.on && v.gate && v.note == n) release(v); }
    else if (m.isAllNotesOff() || m.isAllSoundOff()) { stack.clear(); for (auto& v : vs) if (v.on) release(v); } }
  void spawn(Voice& v, float pitB) {
    for (auto& g : v.g) if (!g.on) {
      float sp = q[K::bSpr], F = (float) fs; g.on = true; g.age = 0; g.ph = rnd.nextFloat();
      g.len = jmax(32.f, q[K::bSize] * .001f * F * (1.f + (rnd.nextFloat() - .5f) * sp));
      float fi = v.freq * pitB * std::exp2((rnd.nextFloat() - .5f) * sp * 100.f / 1200.f);
      g.inc = fi / F; g.tb = tabs[bandOf(fi)]; return; } }

  void render(AudioBuffer<float>& b, int s, int e) { while (s < e) { int len = jmin(maxB, e - s); chunk(b, s, len); s += len; } }
  void chunk(AudioBuffer<float>& b, int s, int len) {
    float* L = b.getWritePointer(0) + s; float* R = b.getWritePointer(1) + s; const float F = (float) fs;
    float pitA = std::exp2(q[K::aSemi] / 12.f), pitB = std::exp2(q[K::bSemi] / 12.f);
    float gn = 1.f / std::sqrt(jmax(1.f, dens * q[K::bSize] * .001f));
    float gc = q[K::gld] < .002f ? 1.f : 1.f - std::exp(-1.f / (q[K::gld] * F));
    float uni = q[K::aUni], dm = std::exp2(q[K::aDet] / 1200.f), ana = q[K::ana], nl = q[K::nLvl], nd = q[K::nDens], tn = q[K::nTone];
    float nA = 1.f - std::exp(-MathConstants<float>::twoPi * 20000.f * std::pow(.01f, std::abs(tn)) / F), nDk = q[K::nDec] > .001f ? std::exp(-1.f / (q[K::nDec] * F)) : 1.f;
    float kk = q[K::res] * (fCar ? 4.3f : 4.1f), drv = 1.f + q[K::fDrv] * 9.f, comp = (1.f + q[K::fBass] * kk) * drv;
    int ty = jlimit(0, 3, fTy), sl = fSl > 0 ? 1 : 0; const float* mix = kMix[ty][sl];
    float oc = kOut[fCar ? 1 : 0][ty][sl] / std::sqrt(1.f + q[K::fDrv] * 3.f); const float fcMax = .42f * F;
    float *l1 = sL1.data(), *l2 = sL2.data(), *in = sIn.data(), *gg = sG.data(), *ae = sAe.data();
    for (int i = 0; i < len; ++i) { l1[i] = lfo(a1, rt1, sh1); l2[i] = lfo(a2, rt2, sh2); }
    for (int vi = 0; vi < 8; ++vi) {
      Voice& v = vs[vi]; if (!v.on) continue;
      for (int i = 0; i < len; ++i) {
        v.age += 1.f; v.freq += (v.tgt - v.freq) * gc;
        v.dr1 = v.dr1 * .9999f + (rnd.nextFloat() - .5f) * .0008f; v.dr2 = v.dr2 * .9999f + (rnd.nextFloat() - .5f) * .0008f;
        float fe = v.ef.getNextSample(); ae[i] = v.ea.getNextSample();
        float pm = std::exp2(l1[i] * q[K::l1P] * .5f + q[K::pEnv] * fe / 12.f + v.dr1 * ana * .25f);
        float fA = v.freq * pitA * pm; const float* T = tabs[bandOf(fA)];
        v.pA += fA / F; v.pA -= std::floor(v.pA); float x = rd(T, v.pA);
        if (uni > .001f) { v.pA2 += fA * dm / F; v.pA2 -= std::floor(v.pA2); v.pA3 += fA / dm / F; v.pA3 -= std::floor(v.pA3);
          x = (x + uni * (rd(T, v.pA2) + rd(T, v.pA3))) / (1.f + uni); }
        x *= q[K::aLvl];
        v.gAcc += dens / F; if (v.gAcc >= 1.f) { v.gAcc -= 1.f; spawn(v, pitB); }
        float bs = 0;
        for (auto& g : v.g) { if (!g.on) continue;
          bs += (.5f - .5f * std::cos(6.2831853f * g.age / g.len)) * rd(g.tb, g.ph);
          g.ph += g.inc; g.ph -= std::floor(g.ph); if (++g.age >= g.len) g.on = false; }
        x += bs * gn * q[K::bLvl] * jmax(0.f, 1.f + l2[i] * q[K::l2B]);
        if (nl > .001f) {
          float w = rnd.nextFloat() * 2.f - 1.f, nz = 0, pink = 0;
          if (nTy == 1 || nTy == 3) { v.k0 = .99765f * v.k0 + w * .099046f; v.k1 = .963f * v.k1 + w * .2965164f; v.k2 = .57f * v.k2 + w * 1.0526913f; pink = v.k0 + v.k1 + v.k2 + w * .1848f; }
          switch (nTy) {
            case 0: nz = w * .43f; break;
            case 1: nz = pink * .145f; break;
            case 2: v.nb = (v.nb + .02f * w) / 1.02f; nz = v.nb * 4.37f; break;
            case 3: nz = (pink - v.pp) * .242f; v.pp = pink; break;
            case 4: nz = (w - v.w1) * .306f; v.w1 = w; break;
            default: if (rnd.nextFloat() < nd / F) v.cr = (rnd.nextFloat() * 2.f - 1.f) * 2.f; nz = v.cr; v.cr *= .75f; break; }
          if (std::abs(tn) > .01f) { v.nlp += nA * (nz - v.nlp); nz = tn < 0.f ? v.nlp : nz - v.nlp; }
          if (nDk < 1.f) { v.ne *= nDk; nz *= v.ne; }
          x += nz * nl; }
        in[i] = x * comp;
        float fc = q[K::cut] * std::exp2(q[K::fEnv] * fe * 5.f + l1[i] * q[K::l1C] * 3.f + v.dr2 * ana * .4f + q[K::fKey] * std::log2(v.freq / 261.63f));
        float f = std::tan(3.14159265f * jlimit(20.f, fcMax, fc) / (2.f * F)); gg[i] = f / (1.f + f); }
      float* ch[1] = {in}; dsp::AudioBlock<float> blk(ch, 1, (size_t) len);
      auto up = os[vi]->processSamplesUp(blk); float* u = up.getChannelPointer(0);
      for (int j = 0; j < 2 * len; ++j) u[j] = v.lad.run(u[j], gg[j >> 1], kk, fCar, mix) * oc;
      os[vi]->processSamplesDown(blk);
      for (int i = 0; i < len; ++i) { float y = in[i] * ae[i] * v.vel * (1.f - q[K::l2A] * (.5f + .5f * l2[i])) * .5f; L[i] += y; R[i] += y; }
      if (!v.ea.isActive()) v.on = false; } }

  // ---------- output stage : saturation, tube preamp, pump, EQ, echo, reverb, volume ----------
  void prepMaster() {
    sOn = q[K::sMix] > .001f; sd = 1.f + q[K::sDrv] * 14.f; sOc = std::pow(10.f, q[K::sOut] / 20.f) / std::pow(sd, .45f);
    float F2 = 2.f * (float) fs, tp = MathConstants<float>::twoPi;
    aTone = 1.f - std::exp(-tp * q[K::sTone] / F2); td = 1.f + q[K::tDrv] * 6.f; tb = q[K::tAsym] * .7f; tNorm = 1.f / std::sqrt(td);
    aLow = 1.f - std::exp(-tp * 150.f / F2); aHf = 1.f - std::exp(-tp * (20000.f - q[K::tWarm] * 10000.f) / F2); }
  static float shape(int m, float x) { switch (m) { case 0: return std::tanh(x); case 1: return std::tanh(x + .3f) - .2913f; case 2: return jlimit(-1.f, 1.f, x); default: return std::sin(x * 1.2f); } }
  float fx(int c, float x) {
    Fx& s = cf[c];
    if (sOn) { float y = shape(sMd, x * sd) * sOc;
      if (sMd == 1) { float o = y - s.d1x + .9975f * s.d1y; s.d1x = y; s.d1y = o; y = o; }
      s.lp += aTone * (y - s.lp); x += (s.lp - x) * q[K::sMix]; }
    if (tOnB) { tubePk = jmax(tubePk, std::abs(x));
      float t = (std::tanh(x * td + tb) - std::tanh(tb)) * tNorm;
      float o = t - s.d2x + .9975f * s.d2y; s.d2x = t; s.d2y = o; t = o;
      s.tl += aLow * (t - s.tl); t += q[K::tWarm] * 1.5f * s.tl; s.th += aHf * (t - s.th);
      x += (s.th - x) * q[K::tMix]; }
    return x; }
  void eqUpdate() {
    float sig[30]; bool ch = false;
    for (int bnd = 0; bnd < 6; ++bnd) { sig[bnd * 5] = q[K::e1F + bnd * 3]; sig[bnd * 5 + 1] = q[K::e1G + bnd * 3]; sig[bnd * 5 + 2] = q[K::e1Q + bnd * 3];
      sig[bnd * 5 + 3] = (float) (int) apvts.getRawParameterValue(kEqT[bnd])->load(); sig[bnd * 5 + 4] = apvts.getRawParameterValue(kEqO[bnd])->load() > .5f ? 1.f : 0.f; }
    for (int i = 0; i < 30; ++i) if (sig[i] != eqSig[i]) ch = true;
    if (!ch) return;
    for (int i = 0; i < 30; ++i) eqSig[i] = sig[i];
    for (int bnd = 0; bnd < 6; ++bnd) {
      Cf::Ptr c1, c2; bool two = false; bandCoefs((int) sig[bnd * 5 + 3], fs, sig[bnd * 5], sig[bnd * 5 + 1], sig[bnd * 5 + 2], c1, c2, two);
      eqB[bnd] = sig[bnd * 5 + 4] > .5f; eq2[bnd] = two;
      for (int c = 0; c < 2; ++c) { eq[c][bnd][0].coefficients = c1; if (two) eq[c][bnd][1].coefficients = c2; } } }
  void master(AudioBuffer<float>& b) {
    int n = b.getNumSamples(); if (n <= 0) return; dsp::AudioBlock<float> all(b); float F = (float) fs;
    for (int pos = 0; pos < n; pos += maxB) {
      auto sub = all.getSubBlock((size_t) pos, (size_t) jmin(maxB, n - pos));
      auto up = osM->processSamplesUp(sub);
      for (size_t c = 0; c < 2; ++c) { float* d = up.getChannelPointer(c); for (size_t j = 0; j < up.getNumSamples(); ++j) d[j] = fx((int) c, d[j]); }
      osM->processSamplesDown(sub); }
    tubeLvl.store(tubePk); tubePk = 0;
    float* L = b.getWritePointer(0); float* R = b.getWritePointer(1);
    // pump (tempo-synced ducking, follows the host song position)
    float depth = q[K::pDep], rel = jlimit(.05f, 1.f, q[K::pRel]), cexp = std::exp2(-1.5f + 3.5f * q[K::pCrv]), dp = (bpmNow / 60.f) / F / pumpL, pa = 1.f - std::exp(-1.f / (.003f * F));
    for (int i = 0; i < n; ++i) {
      if (depth > .001f) { float g = pumpPh < rel ? 1.f - depth * (1.f - std::pow(pumpPh / rel, cexp)) : 1.f; pumpG += (g - pumpG) * pa; L[i] *= pumpG; R[i] *= pumpG; }
      pumpPh += dp; if (pumpPh >= 1.f) pumpPh -= 1.f; }
    // parametric EQ (6 bands)
    if (eqOn) for (int c = 0; c < 2; ++c) { float* d = b.getWritePointer(c);
      for (int i = 0; i < n; ++i) { float x = d[i]; for (int bnd = 0; bnd < 6; ++bnd) if (eqB[bnd]) { x = eq[c][bnd][0].processSample(x); if (eq2[bnd]) x = eq[c][bnd][1].processSample(x); } d[i] = x; } }
    // echo (tempo-synced, optional ping-pong)
    float fb = q[K::dFb], mixD = q[K::dMix], aT = 1.f - std::exp(-MathConstants<float>::twoPi * 500.f * std::pow(32.f, q[K::dTone]) / F);
    float dTgt = jlimit(64.f, (float) dlSize - 4.f, 60.f / jmax(bpmNow, 20.f) * beatsOf(dSy + 1) * F);
    for (int i = 0; i < n; ++i) {
      dCur += (dTgt - dCur) * .002f; float rp = (float) dw - dCur; if (rp < 0.f) rp += (float) dlSize;
      int i0 = (int) rp, i1 = (i0 + 1) % dlSize; float fr = rp - (float) i0;
      float yL = dl[0][(size_t) i0] + (dl[0][(size_t) i1] - dl[0][(size_t) i0]) * fr, yR = dl[1][(size_t) i0] + (dl[1][(size_t) i1] - dl[1][(size_t) i0]) * fr;
      dfl[0] += aT * ((dPPb ? yR : yL) - dfl[0]); dfl[1] += aT * ((dPPb ? yL : yR) - dfl[1]);
      dl[0][(size_t) dw] = ft(L[i] + dfl[0] * fb); dl[1][(size_t) dw] = ft(R[i] + dfl[1] * fb);
      L[i] += yL * mixD; R[i] += yR * mixD; dw = (dw + 1) % dlSize; }
    // reverb
    if (q[K::rMix] > .001f) { Reverb::Parameters rp; rp.roomSize = q[K::rSize]; rp.damping = q[K::rDamp]; rp.wetLevel = q[K::rMix] * .4f; rp.dryLevel = 1.f; rp.width = 1.f; rp.freezeMode = 0.f;
      rev.setParameters(rp); rev.processStereo(L, R, n); }
    b.applyGain(q[K::mst]);
    outL.store(b.getMagnitude(0, 0, n)); outR.store(b.getMagnitude(1, 0, n));
    int w = specW.load(); for (int i = 0; i < n; ++i) specBuf[(w + i) & 4095] = .5f * (L[i] + R[i]); specW.store((w + n) & 4095); }
};

// ======================= user interface =======================
static Colour secColor(const String& t) {
  static const std::pair<const char*, uint32> m[] = {{"OSC A (ADDITIVE)", 0xff4aa8ff}, {"OSC B (GRANULAR)", 0xff35d0b0}, {"NOISE", 0xffc8ccd0}, {"FILTER", 0xffff8a2a},
    {"FILTER ENV", 0xffffc23a}, {"AMP ENV", 0xffa6e04a}, {"LFO 1", 0xffc06bff}, {"LFO 2", 0xffff6bd6}, {"GLOBAL", 0xffff5c5c}, {"OUTPUT", 0xff5cff8a},
    {"SATURATION", 0xffff7a45}, {"TUBE PREAMP", 0xffffb347}, {"PUMP", 0xff3adbd8}, {"EQ", 0xff6c8cff}, {"ECHO", 0xff7fb3ff}, {"REVERB", 0xff9a7bff}};
  for (auto& e : m) if (t == e.first) return Colour(e.second);
  return Colours::white; }
static Colour bandCol(int i) { static const uint32 c[6] = {0xffff5c5c, 0xffffa23a, 0xffffe03a, 0xff6be06b, 0xff4fc3ff, 0xffc06bff}; return Colour(c[jlimit(0, 5, i)]); }

static void screw(Graphics& g, float cx, float cy, float r, float rot) {
  g.setColour(Colours::black.withAlpha(.5f)); g.fillEllipse(cx - r, cy - r + 1.6f, r * 2, r * 2);
  g.setGradientFill(ColourGradient(Colour(0xffd0d3d6), cx - r * .6f, cy - r * .7f, Colour(0xff55595e), cx + r * .7f, cy + r * .8f, false));
  g.fillEllipse(cx - r, cy - r, r * 2, r * 2);
  g.setColour(Colour(0xff17181a)); g.drawEllipse(cx - r, cy - r, r * 2, r * 2, 1.f);
  Path p; p.addRectangle(-r * .72f, -1.1f, r * 1.44f, 2.2f); p.addRectangle(-1.1f, -r * .72f, 2.2f, r * 1.44f);
  auto t = AffineTransform::rotation(rot).translated(cx, cy);
  g.setColour(Colour(0x66ffffff)); g.fillPath(p, t.translated(.6f, .9f)); g.setColour(Colour(0xff1b1c1e)); g.fillPath(p, t); }
static void led(Graphics& g, float cx, float cy, Colour on, bool lit, float r = 5.5f) {
  if (lit) { g.setGradientFill(ColourGradient(on.withAlpha(.55f), cx, cy, on.withAlpha(0.f), cx + r * 2.4f, cy, true)); g.fillEllipse(cx - r * 2.4f, cy - r * 2.4f, r * 4.8f, r * 4.8f); }
  g.setColour(Colours::black); g.fillEllipse(cx - r, cy - r, r * 2, r * 2);
  g.setGradientFill(ColourGradient(lit ? on.brighter(.8f) : on.withMultipliedBrightness(.25f), cx - r * .3f, cy - r * .4f, lit ? on : on.withMultipliedBrightness(.1f), cx + r * .5f, cy + r * .7f, true));
  g.fillEllipse(cx - r * .72f, cy - r * .72f, r * 1.44f, r * 1.44f);
  g.setColour(Colours::white.withAlpha(.55f)); g.fillEllipse(cx - r * .4f, cy - r * .58f, r * .5f, r * .33f); }

struct Knob : Slider {
  AudioProcessorParameter* par = nullptr;
  Knob() : Slider(Slider::RotaryHorizontalVerticalDrag, Slider::TextBoxBelow) {}
  void mouseDoubleClick(const MouseEvent& e) override {   // double-click on the knob = back to the default value
    if (par != nullptr && e.position.y < (float) getHeight() - 18.f) { par->beginChangeGesture(); par->setValueNotifyingHost(par->getDefaultValue()); par->endChangeGesture(); }
    else Slider::mouseDoubleClick(e); } };

struct Dark : LookAndFeel_V4 {
  Dark() {
    setColour(Slider::textBoxTextColourId, Colour(0xffb8c0c8)); setColour(Slider::textBoxOutlineColourId, Colours::transparentBlack);
    setColour(Slider::textBoxBackgroundColourId, Colours::transparentBlack); setColour(Label::textColourId, Colour(0xffe9ecef));
    setColour(ComboBox::textColourId, Colour(0xffe6e9eb)); setColour(ComboBox::arrowColourId, Colour(0xffe6e9eb));
    setColour(PopupMenu::backgroundColourId, Colour(0xff1f2124)); setColour(PopupMenu::textColourId, Colour(0xffe6e9eb)); setColour(PopupMenu::headerTextColourId, Colour(0xff8fb8d8));
    setColour(PopupMenu::highlightedBackgroundColourId, Colour(0xff3a8fd8)); setColour(PopupMenu::highlightedTextColourId, Colours::white);
    setColour(TextButton::buttonColourId, Colour(0xff2a2d31)); setColour(TextButton::buttonOnColourId, Colour(0xff4a4f56));
    setColour(TextButton::textColourOffId, Colour(0xffe6e9eb)); setColour(TextButton::textColourOnId, Colours::white); }
  void drawRotarySlider(Graphics& g, int x, int y, int w, int h, float pos, float a0, float a1, Slider& s) override {
    float d = (float) jmin(w, h) - 20.f, r = d * .5f; Point<float> c((float) x + (float) w * .5f, (float) y + (float) h * .5f);
    auto acc = s.findColour(Slider::rotarySliderFillColourId);
    for (int i = 0; i <= 20; ++i) { float t = (float) i / 20.f, an = a0 + t * (a1 - a0);
      g.setColour(t <= pos + .001f ? acc : Colour(0x44ffffff));
      g.fillEllipse(c.x + std::sin(an) * (r + 7.f) - 1.4f, c.y - std::cos(an) * (r + 7.f) - 1.4f, 2.8f, 2.8f); }
    for (int i = 6; i >= 1; --i) { float e = (float) i * 1.4f; g.setColour(Colours::black.withAlpha(.11f)); g.fillEllipse(c.x - r - e, c.y - r + 4.f - e * .5f, d + 2 * e, d + 2 * e); }
    g.setGradientFill(ColourGradient(Colour(0xff4c4e52), c.x, c.y - r, Colour(0xff060607), c.x, c.y + r, false)); g.fillEllipse(c.x - r, c.y - r, d, d);
    for (int i = 0; i < 64; ++i) { float an = (float) i * MathConstants<float>::twoPi / 64.f; Point<float> u(std::sin(an), -std::cos(an));
      float lit = jmax(0.f, .5f + .5f * (-u.y * .8f - u.x * .2f));
      g.setColour(Colours::white.withAlpha(.04f + .24f * lit * (i % 2 == 0 ? 1.f : .35f)));
      g.drawLine(c.x + u.x * r * .84f, c.y + u.y * r * .84f, c.x + u.x * (r - .5f), c.y + u.y * (r - .5f), 1.f); }
    Path rim; rim.addCentredArc(c.x, c.y, r - .8f, r - .8f, 0.f, -1.1f, 1.1f, true);
    g.setColour(Colours::white.withAlpha(.35f)); g.strokePath(rim, PathStrokeType(1.4f));
    g.setColour(Colours::black); g.drawEllipse(c.x - r, c.y - r, d, d, 1.3f);
    float cr = r * .66f;
    g.setColour(Colours::black.withAlpha(.55f)); g.fillEllipse(c.x - cr - 1, c.y - cr, cr * 2 + 2, cr * 2 + 2);
    g.setGradientFill(ColourGradient(Colour(0xfff1f2f3), c.x - cr * .7f, c.y - cr * .9f, Colour(0xff8c9095), c.x + cr * .7f, c.y + cr * .9f, false));
    g.fillEllipse(c.x - cr, c.y - cr, cr * 2, cr * 2);
    { Graphics::ScopedSaveState ss(g); Path clip; clip.addEllipse(c.x - cr, c.y - cr, cr * 2, cr * 2); g.reduceClipRegion(clip);
      for (float rr = 1.f; rr < cr; rr += 1.15f) { g.setColour(((int) (rr * 10.f)) % 3 == 0 ? Colours::white.withAlpha(.10f) : Colours::black.withAlpha(.06f));
        g.drawEllipse(c.x - rr, c.y - rr, rr * 2, rr * 2, .6f); }
      for (int k = 0; k < 2; ++k) { Path wdg; float a = k ? 3.4f : .3f; wdg.addPieSegment(c.x - cr, c.y - cr, cr * 2, cr * 2, a, a + .55f, 0.f);
        g.setColour(Colours::white.withAlpha(.16f)); g.fillPath(wdg); } }
    g.setColour(Colour(0xff111214)); g.drawEllipse(c.x - cr, c.y - cr, cr * 2, cr * 2, 1.2f);
    auto t = AffineTransform::rotation(a0 + pos * (a1 - a0)).translated(c);
    Path groove; groove.addRoundedRectangle(-2.8f, -cr + 1.5f, 5.6f, cr * .55f + 2.f, 2.3f);
    Path mark; mark.addRoundedRectangle(-1.7f, -cr + 2.5f, 3.4f, cr * .55f, 1.5f);
    g.setColour(Colours::black.withAlpha(.6f)); g.fillPath(groove, t); g.setColour(Colours::white); g.fillPath(mark, t); }
  void drawComboBox(Graphics& g, int w, int h, bool, int, int, int, int, ComboBox&) override {
    Rectangle<float> b(0.f, 0.f, (float) w, (float) h);
    g.setGradientFill(ColourGradient(Colour(0xff111214), 0, 0, Colour(0xff2c2f33), 0, (float) h, false)); g.fillRoundedRectangle(b, 4.f);
    g.setColour(Colour(0xff000000)); g.drawRoundedRectangle(b.reduced(.5f), 4.f, 1.f);
    g.setColour(Colour(0x30ffffff)); g.drawHorizontalLine(h - 1, 4.f, (float) w - 4.f);
    Path p; p.addTriangle((float) w - 15.f, (float) h * .42f, (float) w - 7.f, (float) h * .42f, (float) w - 11.f, (float) h * .62f);
    g.setColour(Colour(0xffd5d9dc)); g.fillPath(p); }
  void positionComboBoxText(ComboBox& b, Label& l) override { l.setBounds(6, 1, b.getWidth() - 22, b.getHeight() - 2); l.setFont(getComboBoxFont(b)); }
  void drawToggleButton(Graphics& g, ToggleButton& b, bool, bool) override {
    auto a = b.getLocalBounds().toFloat(); led(g, a.getX() + 14.f, a.getCentreY(), Colour(0xffff8a2a), b.getToggleState());
    g.setColour(Colour(0xffe9ecef)); g.setFont(14.f);
    g.drawText(b.getButtonText(), (int) a.getX() + 28, (int) a.getY(), (int) a.getWidth() - 28, (int) a.getHeight(), Justification::left); } };

class ScreenView : public Component, private Timer {
  Proc& pr; const std::atomic<int>& last; const std::vector<const char*>& ids; Image grid;
  float P(const char* id) const { return pr.apvts.getRawParameterValue(id)->load(); }
  void frame(Graphics& g, Rectangle<float> r, const String& cap) {
    g.setColour(Colour(0xff040e12)); g.fillRoundedRectangle(r, 3.f); g.setColour(Colour(0xff17414f));
    for (int i = 1; i < 4; ++i) g.drawHorizontalLine((int) (r.getY() + r.getHeight() * (float) i / 4.f), r.getX(), r.getRight());
    g.drawRoundedRectangle(r, 3.f, 1.f); g.setColour(Colour(0xff6fb4c8)); g.setFont(10.f);
    g.drawText(cap, (int) r.getX(), (int) r.getBottom() + 2, (int) r.getWidth(), 12, Justification::centred); }
public:
  ScreenView(Proc& p, const std::atomic<int>& l, const std::vector<const char*>& i) : pr(p), last(l), ids(i) { startTimerHz(20); }
  void timerCallback() override { repaint(); }
  void resized() override {
    grid = Image(Image::ARGB, jmax(1, getWidth()), jmax(1, getHeight()), true); Graphics g(grid); g.setColour(Colour(0x38000000));
    for (int x = 0; x < getWidth(); x += 3) g.drawVerticalLine(x, 0.f, (float) getHeight());
    for (int y = 0; y < getHeight(); y += 3) g.drawHorizontalLine(y, 0.f, (float) getWidth()); }
  void paint(Graphics& g) override {
    auto b = getLocalBounds().toFloat();
    g.setGradientFill(ColourGradient(Colour(0xff484b4f), 0, 0, Colour(0xff0c0d0e), 0, b.getHeight(), false)); g.fillRoundedRectangle(b, 9.f);
    g.setColour(Colour(0x66ffffff)); g.drawRoundedRectangle(b.reduced(.5f), 9.f, 1.f);
    auto in = b.reduced(6.f); Path glassPath; glassPath.addRoundedRectangle(in, 5.f);
    g.setColour(Colours::black); g.fillPath(glassPath);
    g.setGradientFill(ColourGradient(Colour(0xff07181e), 0, in.getY(), Colour(0xff020708), 0, in.getBottom(), false)); g.fillPath(glassPath);
    { Graphics::ScopedSaveState ss(g); g.reduceClipRegion(glassPath);
      Colour amber(0xffffb347), cyan(0xff58d6ff);
      g.setColour(Colour(0xff6fb4c8)); g.setFont(11.f); g.drawText("PRESET", 18, 12, 200, 14, Justification::left);
      g.setColour(amber); g.setFont(21.f); g.drawText(pr.currentName(), 18, 26, 205, 26, Justification::left, true);
      int li = last.load();
      if (li >= 0 && li < (int) ids.size()) if (auto* p = pr.apvts.getParameter(ids[(size_t) li])) {
        g.setColour(cyan); g.setFont(12.f); g.drawText(p->getName(26), 18, 62, 205, 16, Justification::left, true);
        g.setColour(Colours::white); g.setFont(21.f); g.drawText(p->getCurrentValueAsText(), 18, 76, 205, 24, Justification::left, true); }
      float top = 14.f, hh = 58.f, w = 96.f;
      { auto r = Rectangle<float>(236.f, top, w, hh); frame(g, r, "OSC A");
        float t[96]; makeCycle(t, 96, P("aPart"), P("aTilt"), P("aOdd")); Path p;
        for (int i = 0; i < 96; ++i) { float x = r.getX() + r.getWidth() * (float) i / 95.f, y = r.getCentreY() - t[i] * r.getHeight() * .42f; if (i) p.lineTo(x, y); else p.startNewSubPath(x, y); }
        g.setColour(cyan); g.strokePath(p, PathStrokeType(1.6f)); }
      { auto r = Rectangle<float>(348.f, top, w, hh); frame(g, r, "FILTER");
        float cut = P("cut"), res = P("res"), R1 = .7f - .64f * res; int ty = (int) P("fT"), sl = (int) P("fSl"); Path p;
        for (int i = 0; i < 96; ++i) { float f = 20.f * std::pow(1000.f, (float) i / 95.f), rr = f / cut;
          auto st = [&](float R) { float den = std::sqrt((1 - rr * rr) * (1 - rr * rr) + 4 * R * R * rr * rr);
            return (ty == 0 ? 1.f : ty == 1 ? rr * rr : ty == 2 ? 2 * R * rr : std::abs(1 - rr * rr)) / den; };
          float m = st(R1); if (sl) m *= st(.7f);
          float db = jlimit(-48.f, 24.f, 20.f * std::log10(jmax(m, 1e-4f)));
          float x = r.getX() + r.getWidth() * (float) i / 95.f, y = jmap(db, -48.f, 24.f, r.getBottom() - 2.f, r.getY() + 2.f);
          if (i) p.lineTo(x, y); else p.startNewSubPath(x, y); }
        g.setColour(amber); g.strokePath(p, PathStrokeType(1.6f)); }
      { auto r = Rectangle<float>(460.f, top, w, hh); frame(g, r, "AMP ENV");
        float A = P("aA"), D = P("aD"), S = P("aS"), Rl = P("aR");
        float wa = std::pow(A, .35f), wd = std::pow(D, .35f), wr = std::pow(Rl, .35f), ws = .6f * (wa + wd + wr) / 3.f, tot = wa + wd + ws + wr, k = r.getWidth() / tot;
        float yb = r.getBottom() - 3.f, yt = r.getY() + 3.f, ys = jmap(S, 0.f, 1.f, yb, yt); Path p;
        p.startNewSubPath(r.getX(), yb); p.lineTo(r.getX() + wa * k, yt); p.lineTo(r.getX() + (wa + wd) * k, ys);
        p.lineTo(r.getX() + (wa + wd + ws) * k, ys); p.lineTo(r.getRight(), yb);
        g.setColour(Colour(0xff7dff9b)); g.strokePath(p, PathStrokeType(1.6f)); }
      g.drawImageAt(grid, 0, 0);
      g.setGradientFill(ColourGradient(Colour(0x2cffffff), in.getX(), in.getY(), Colour(0x00ffffff), in.getX() + in.getWidth() * .55f, in.getBottom(), false));
      Path refl; refl.startNewSubPath(in.getX(), in.getY()); refl.lineTo(in.getX() + in.getWidth() * .72f, in.getY());
      refl.lineTo(in.getX() + in.getWidth() * .30f, in.getBottom()); refl.lineTo(in.getX(), in.getBottom()); refl.closeSubPath(); g.fillPath(refl);
      for (int i = 0; i < 10; ++i) { g.setColour(Colours::black.withAlpha(.5f * (1.f - (float) i / 10.f))); g.drawRoundedRectangle(in.reduced((float) i * .6f), 5.f, 1.f); } }
    g.setColour(Colour(0xff000000)); g.drawRoundedRectangle(in, 5.f, 1.2f); } };

// ---- EQ display : live output spectrum + EQ curve + draggable band nodes ----
class EqDisplay : public Component, private Timer {
  Proc& pr; int sel = 0, drag = -1; float spec[2048] {}; std::vector<float> data = std::vector<float>(8192, 0.f); float ring[4096] {};
  dsp::FFT fft{12}; dsp::WindowingFunction<float> win{4096, dsp::WindowingFunction<float>::hann};
  float x2f(float x) const { return 20.f * std::pow(1000.f, x / (float) getWidth()); }
  float f2x(float f) const { return (float) getWidth() * std::log10(f / 20.f) / 3.f; }
  float g2y(float g) const { return (float) getHeight() * (.5f - g / 36.f); }
  float y2g(float y) const { return (.5f - y / (float) getHeight()) * 36.f; }
  float P(const char* id) const { return pr.apvts.getRawParameterValue(id)->load(); }
  void setP(const char* id, float v) { if (auto* p = pr.apvts.getParameter(id)) { p->beginChangeGesture(); p->setValueNotifyingHost(p->convertTo0to1(v)); p->endChangeGesture(); } }
  static const char* idF(int b) { static const char* a[6] = {"e1F", "e2F", "e3F", "e4F", "e5F", "e6F"}; return a[b]; }
  static const char* idG(int b) { static const char* a[6] = {"e1G", "e2G", "e3G", "e4G", "e5G", "e6G"}; return a[b]; }
  static const char* idQ(int b) { static const char* a[6] = {"e1Q", "e2Q", "e3Q", "e4Q", "e5Q", "e6Q"}; return a[b]; }
  bool isOn(int b) const { return P(kEqO[b]) > .5f; }
  int typeOf(int b) const { return (int) P(kEqT[b]); }
  int hit(Point<float> p) const { for (int b = 5; b >= 0; --b) if (isOn(b)) { float x = f2x(P(idF(b))), y = typeOf(b) <= 2 ? g2y(P(idG(b))) : g2y(0.f); if (p.getDistanceFrom({x, y}) < 12.f) return b; } return -1; }
  double mag(int b, double f) const {
    Cf::Ptr c1, c2; bool two = false; bandCoefs(typeOf(b), 48000.0, P(idF(b)), P(idG(b)), P(idQ(b)), c1, c2, two);
    double m = c1->getMagnitudeForFrequency(f, 48000.0); if (two) m *= c2->getMagnitudeForFrequency(f, 48000.0); return m; }
public:
  std::function<void(int)> onSelect;
  EqDisplay(Proc& p) : pr(p) { startTimerHz(30); }
  void setSelected(int b) { sel = b; repaint(); }
  void timerCallback() override {
    pr.copySpectrumInput(ring); for (int i = 0; i < 4096; ++i) data[(size_t) i] = ring[i];
    std::fill(data.begin() + 4096, data.end(), 0.f); win.multiplyWithWindowingTable(data.data(), 4096); fft.performFrequencyOnlyForwardTransform(data.data());
    for (int k = 0; k < 2048; ++k) { float db = 20.f * std::log10(jmax(data[(size_t) k] / 1024.f, 1e-6f)); spec[k] += (db - spec[k]) * (db > spec[k] ? .6f : .12f); }
    repaint(); }
  void paint(Graphics& g) override {
    auto b = getLocalBounds().toFloat(); float w = b.getWidth(), h = b.getHeight(); double sr = jmax(8000.0, pr.getSampleRate());
    g.setGradientFill(ColourGradient(Colour(0xff0a1a20), 0, 0, Colour(0xff03090c), 0, h, false)); g.fillRoundedRectangle(b, 6.f);
    g.setColour(Colour(0xff17414f)); g.setFont(10.f);
    for (float fr : {50.f, 100.f, 200.f, 500.f, 1000.f, 2000.f, 5000.f, 10000.f}) { g.drawVerticalLine((int) f2x(fr), 0.f, h); g.setColour(Colour(0xff4d8ea3)); g.drawText(fr >= 1000.f ? String((int) (fr / 1000.f)) + "k" : String((int) fr), (int) f2x(fr) + 2, (int) h - 13, 30, 12, Justification::left); g.setColour(Colour(0xff17414f)); }
    for (float db : {-12.f, -6.f, 0.f, 6.f, 12.f}) { g.setColour(db == 0.f ? Colour(0xff2b6477) : Colour(0xff17414f)); g.drawHorizontalLine((int) g2y(db), 0.f, w); g.setColour(Colour(0xff4d8ea3)); g.drawText(String((int) db), 3, (int) g2y(db) - 12, 28, 12, Justification::left); }
    { Path sp; sp.startNewSubPath(0.f, h);       // live spectrum of the output
      for (int x = 0; x <= (int) w; x += 2) { float f0 = x2f((float) x), f1 = x2f((float) x + 2.f); int k0 = jlimit(1, 2047, (int) (f0 * 4096.f / (float) sr)), k1 = jlimit(k0, 2047, (int) (f1 * 4096.f / (float) sr));
        float m = -120.f; for (int k = k0; k <= k1; ++k) m = jmax(m, spec[k]); sp.lineTo((float) x, h - jlimit(0.f, 1.f, (m + 96.f) / 90.f) * h); }
      sp.lineTo(w, h); sp.closeSubPath();
      g.setGradientFill(ColourGradient(Colour(0x6658b4ff), 0, 0, Colour(0x0858b4ff), 0, h, false)); g.fillPath(sp);
      g.setColour(Colour(0xaa58b4ff)); g.strokePath(sp, PathStrokeType(1.1f)); }
    bool eqOn = P("eOn") > .5f; Path tot; bool any = false;
    for (int i = 0; i <= (int) w; i += 3) { double f = (double) x2f((float) i), m = 1.0; for (int bnd = 0; bnd < 6; ++bnd) if (isOn(bnd)) { m *= mag(bnd, f); any = true; }
      float d = jlimit(-18.f, 18.f, (float) (20.0 * std::log10(jmax(m, 1e-4)))), y = g2y(d); if (i) tot.lineTo((float) i, y); else tot.startNewSubPath((float) i, y); }
    if (isOn(sel)) { Path bp; for (int i = 0; i <= (int) w; i += 3) { float d = jlimit(-18.f, 18.f, (float) (20.0 * std::log10(jmax(mag(sel, (double) x2f((float) i)), 1e-4)))), y = g2y(d); if (i) bp.lineTo((float) i, y); else bp.startNewSubPath((float) i, y); }
      Path fill(bp); fill.lineTo(w, g2y(0.f)); fill.lineTo(0.f, g2y(0.f)); fill.closeSubPath(); g.setColour(bandCol(sel).withAlpha(.18f)); g.fillPath(fill); }
    if (any) { g.setColour(Colour(0xffffb347).withAlpha(eqOn ? 1.f : .3f)); g.strokePath(tot, PathStrokeType(2.f)); }
    for (int bnd = 0; bnd < 6; ++bnd) if (isOn(bnd)) { float x = f2x(P(idF(bnd))), y = typeOf(bnd) <= 2 ? g2y(P(idG(bnd))) : g2y(0.f), r = bnd == sel ? 9.f : 7.f;
      g.setColour(Colours::black.withAlpha(.6f)); g.fillEllipse(x - r, y - r + 2.f, r * 2, r * 2); g.setColour(bandCol(bnd).withAlpha(eqOn ? 1.f : .45f)); g.fillEllipse(x - r, y - r, r * 2, r * 2);
      g.setColour(Colours::white.withAlpha(bnd == sel ? 1.f : .5f)); g.drawEllipse(x - r, y - r, r * 2, r * 2, 1.4f); g.setColour(Colours::black); g.setFont(11.f); g.drawText(String(bnd + 1), (int) (x - r), (int) (y - r), (int) (r * 2), (int) (r * 2), Justification::centred); }
    g.setColour(Colour(0xff6fb4c8)); g.setFont(11.f); g.drawText("LIVE OUTPUT SPECTRUM + EQ CURVE  |  drag nodes, wheel = Q, double-click = add / remove", 8, 4, (int) w - 16, 14, Justification::left); }
  void mouseDown(const MouseEvent& e) override { drag = hit(e.position); if (drag >= 0) { sel = drag; if (onSelect) onSelect(sel); } }
  void mouseUp(const MouseEvent&) override { drag = -1; }
  void mouseDrag(const MouseEvent& e) override {
    if (drag < 0) return; setP(idF(drag), jlimit(20.f, 20000.f, x2f(e.position.x)));
    if (typeOf(drag) <= 2) setP(idG(drag), jlimit(-18.f, 18.f, y2g(e.position.y))); }
  void mouseDoubleClick(const MouseEvent& e) override {
    int h = hit(e.position);
    if (h >= 0) { if (auto* p = pr.apvts.getParameter(kEqO[h])) { p->beginChangeGesture(); p->setValueNotifyingHost(0.f); p->endChangeGesture(); } return; }
    for (int bnd = 0; bnd < 6; ++bnd) if (!isOn(bnd)) { setP(idF(bnd), jlimit(20.f, 20000.f, x2f(e.position.x))); if (typeOf(bnd) <= 2) setP(idG(bnd), jlimit(-18.f, 18.f, y2g(e.position.y)));
      if (auto* p = pr.apvts.getParameter(kEqO[bnd])) { p->beginChangeGesture(); p->setValueNotifyingHost(1.f); p->endChangeGesture(); }
      sel = bnd; if (onSelect) onSelect(sel); return; } }
  void mouseWheelMove(const MouseEvent& e, const MouseWheelDetails& wd) override {
    int h = hit(e.position); if (h < 0) h = isOn(sel) ? sel : -1;
    if (h >= 0) setP(idQ(h), jlimit(.1f, 12.f, P(idQ(h)) * std::exp(wd.deltaY * 1.2f))); } };

// ---- tube preamp window : glows with heat and signal level ----
class TubeView : public Component, private Timer {
  Proc& pr; float lvl = 0, heat = 0;
  float P(const char* id) const { return pr.apvts.getRawParameterValue(id)->load(); }
public:
  TubeView(Proc& p) : pr(p) { startTimerHz(30); }
  void timerCallback() override {
    bool on = P("tOn") > .5f; float drv = P("tDrv"), mix = P("tMix"), in = jmin(1.f, pr.tubeLvl.load() * (1.f + drv * 3.f)), target = on ? (.3f + .7f * drv) * (.45f + .55f * mix) : .04f;
    lvl += (in - lvl) * (in > lvl ? .5f : .07f); heat += (target - heat) * .04f; repaint(); }
  void paint(Graphics& g) override {
    auto b = getLocalBounds().toFloat(); bool on = P("tOn") > .5f; float glow = jlimit(0.f, 1.f, heat * .6f + (on ? lvl * .7f : 0.f));
    g.setGradientFill(ColourGradient(Colour(0xff15181b), 0, 0, Colour(0xff0a0b0d), 0, b.getHeight(), false)); g.fillRoundedRectangle(b, 8.f);
    g.setColour(Colour(0xff000000)); g.drawRoundedRectangle(b.reduced(.5f), 8.f, 1.2f);
    for (int t = 0; t < 2; ++t) {
      float tw = 52.f, th = b.getHeight() - 52.f, cx = b.getWidth() * (t ? .7f : .3f), x = cx - tw * .5f, y = 12.f;
      Path glass; glass.addRoundedRectangle(x, y, tw, th, tw * .5f, tw * .5f, true, true, false, false);
      g.setColour(Colour(0xff0c0e10)); g.fillPath(glass);
      Colour hot = Colour(0xffa02000).interpolatedWith(Colour(0xffffd070), jlimit(0.f, 1.f, heat * .7f + lvl * .5f));
      g.setGradientFill(ColourGradient(hot.withAlpha(.15f + .75f * glow), cx, y + th * .55f, hot.withAlpha(0.f), cx + tw * 1.15f, y + th * .55f, true)); g.fillPath(glass);
      { Graphics::ScopedSaveState ss(g); g.reduceClipRegion(glass);
        for (int s = -1; s <= 1; s += 2) { g.setColour(Colour(0xff3b3f45).interpolatedWith(hot, glow * .5f)); g.fillRect(cx + (float) s * 9.f - 3.f, y + th * .22f, 6.f, th * .56f);
          g.setColour(Colours::white.withAlpha(.12f)); g.fillRect(cx + (float) s * 9.f - 3.f, y + th * .22f, 1.5f, th * .56f); }
        for (float yy = y + th * .3f; yy < y + th * .74f; yy += 5.f) { g.setColour(Colour(0x66000000)); g.drawHorizontalLine((int) yy, cx - 7.f, cx + 7.f); }
        g.setColour(hot.brighter(.6f).withAlpha(.35f + .65f * glow)); g.fillRoundedRectangle(cx - 1.5f, y + th * .28f, 3.f, th * .5f, 1.5f);
        g.setColour(Colour(0xffb8bec4).withAlpha(.6f)); g.fillEllipse(cx - 7.f, y + 5.f, 14.f, 7.f);
        g.setGradientFill(ColourGradient(Colour(0x40ffffff), x, 0, Colour(0x00ffffff), x + tw * .5f, 0, false)); g.fillRect(x, y, tw * .5f, th); }
      g.setColour(Colour(0x66ffffff)); g.strokePath(glass, PathStrokeType(1.2f));
      Rectangle<float> base(x - 3.f, y + th, tw + 6.f, 16.f); g.setGradientFill(ColourGradient(Colour(0xffb5bac0), base.getX(), 0, Colour(0xff4a4e53), base.getRight(), 0, false)); g.fillRoundedRectangle(base, 3.f);
      g.setColour(Colour(0xff111214)); g.drawRoundedRectangle(base, 3.f, 1.f);
      for (int p = 0; p < 5; ++p) { g.setColour(Colour(0xffa0a5ab)); g.fillRect(base.getX() + 6.f + (float) p * ((base.getWidth() - 12.f) / 4.f) - 1.f, base.getBottom(), 2.f, 7.f); } }
    for (int i = 0; i < 5; ++i) { static const float th[5] = {.08f, .2f, .4f, .65f, .88f}; led(g, b.getWidth() * .5f + ((float) i - 2.f) * 15.f, b.getBottom() - 11.f, i < 3 ? Colour(0xffffb347) : Colour(0xffff4b3a), on && lvl > th[i], 3.4f); } } };

// ---- output level meter ----
class MeterView : public Component, private Timer {
  Proc& pr; float dl = -90, dr = -90, pl = -90, pr2 = -90;
public:
  MeterView(Proc& p) : pr(p) { startTimerHz(30); }
  void timerCallback() override {
    auto cv = [](float v) { return 20.f * std::log10(jmax(v, 1e-5f)); };
    float l = cv(pr.outL.load()), r = cv(pr.outR.load()); dl = jmax(l, dl - 1.6f); dr = jmax(r, dr - 1.6f); pl = jmax(l, pl - .25f); pr2 = jmax(r, pr2 - .25f); repaint(); }
  void paint(Graphics& g) override {
    auto b = getLocalBounds().toFloat(); g.setGradientFill(ColourGradient(Colour(0xff15181b), 0, 0, Colour(0xff0a0b0d), 0, b.getHeight(), false)); g.fillRoundedRectangle(b, 8.f);
    g.setColour(Colour(0xff000000)); g.drawRoundedRectangle(b.reduced(.5f), 8.f, 1.2f);
    auto bar = [&](float y, float db, float pk, const char* nm) {
      float x0 = 28.f, w = b.getWidth() - 40.f, h = 22.f; g.setColour(Colour(0xff060708)); g.fillRoundedRectangle(x0, y, w, h, 3.f);
      float v = jlimit(0.f, 1.f, (db + 60.f) / 60.f); g.setGradientFill(ColourGradient(Colour(0xff35d06b), x0, 0, Colour(0xffff4b3a), x0 + w, 0, false)); g.fillRoundedRectangle(x0, y, w * v, h, 3.f);
      g.setColour(Colours::white); g.fillRect(x0 + w * jlimit(0.f, 1.f, (pk + 60.f) / 60.f) - 1.f, y, 2.f, h);
      g.setColour(Colour(0xffc9ced3)); g.setFont(13.f); g.drawText(nm, 6, (int) y, 20, (int) h, Justification::centred); };
    bar(26.f, dl, pl, "L"); bar(58.f, dr, pr2, "R");
    g.setColour(Colour(0xff8f969c)); g.setFont(10.f);
    for (float d : {-48.f, -24.f, -12.f, -6.f, 0.f}) { float x = 28.f + (b.getWidth() - 40.f) * (d + 60.f) / 60.f; g.drawVerticalLine((int) x, 86.f, 92.f); g.drawText(String((int) d), (int) x - 14, 92, 28, 12, Justification::centred); } } };

struct SaveCb : ModalComponentManager::Callback { std::function<void(int)> f; void modalStateFinished(int r) override { if (f) f(r); } };

class Panel : public Component, private APVTS::Listener, private Timer {
  struct Cell { Label l; std::unique_ptr<Knob> s; std::unique_ptr<ComboBox> c; std::unique_ptr<ToggleButton> t;
    std::unique_ptr<APVTS::SliderAttachment> sa; std::unique_ptr<APVTS::ComboBoxAttachment> ca; std::unique_ptr<APVTS::ButtonAttachment> ta;
    Component* main() { if (s) return s.get(); if (c) return c.get(); return t.get(); } };
  struct Sec { String title; Rectangle<int> area; int extraW = 0; std::vector<std::unique_ptr<Cell>> cells; };
  struct Entry { String name, cat; int factory; File file; };
  static constexpr int CW = 78, CH = 112, CHEEK = 26;
  static constexpr int EQW = 940, EQH = 230;
public:
  static constexpr int W = 1600, H = 1212;
private:
  Proc& pr; Dark lf; std::vector<std::unique_ptr<Sec>> secs; std::vector<Entry> entries; ComboBox pre; TextButton prev{"<"}, next{">"}, save{"Save"};
  std::vector<const char*> ids; std::atomic<int> last{-1}; std::unique_ptr<ScreenView> screen; std::unique_ptr<EqDisplay> eqd; std::unique_ptr<TubeView> tube; std::unique_ptr<MeterView> meter;
  Image bg, logo; bool voiceLit = false;
  // EQ band panel
  int selBand = 0; TextButton bandBtn[6]; Knob kF, kG, kQ; Label lF, lG, lQ; ComboBox bType; ToggleButton bOn{"Band on"}, eqAct{"EQ active"};
  std::unique_ptr<APVTS::SliderAttachment> aF, aG, aQ; std::unique_ptr<APVTS::ComboBoxAttachment> aT; std::unique_ptr<APVTS::ButtonAttachment> aO, aA;

  void timerCallback() override { bool l = pr.nVoices.load() > 0; if (l != voiceLit) { voiceLit = l; repaint(180, 84, 160, 30); } }
  void parameterChanged(const String& id, float) override { for (size_t i = 0; i < ids.size(); ++i) if (id == ids[i]) { last = (int) i; break; } }
  Sec& sec(const String& t) {
    for (auto& s : secs) if (s->title == t) return *s;
    secs.push_back(std::make_unique<Sec>()); secs.back()->title = t; return *secs.back(); }
  Cell& cell(Sec& s, const String& label) {
    s.cells.push_back(std::make_unique<Cell>()); auto& c = *s.cells.back();
    c.l.setText(label, dontSendNotification); c.l.setJustificationType(Justification::centred); addAndMakeVisible(c.l); return c; }
  void extra(const char* grp, const char* id, const char* label, const char* toggleText) {
    auto& c = cell(sec(grp), label);
    if (toggleText) { c.t = std::make_unique<ToggleButton>(toggleText); c.ta = std::make_unique<APVTS::ButtonAttachment>(pr.apvts, id, *c.t); addAndMakeVisible(*c.t); }
    else { c.c = std::make_unique<ComboBox>(); if (auto* pc = dynamic_cast<AudioParameterChoice*>(pr.apvts.getParameter(id))) c.c->addItemList(pc->choices, 1);
      c.ca = std::make_unique<APVTS::ComboBoxAttachment>(pr.apvts, id, *c.c); addAndMakeVisible(*c.c); } }
  void selectBand(int b) {
    selBand = jlimit(0, 5, b); auto col = bandCol(selBand); kF.setColour(Slider::rotarySliderFillColourId, col); kG.setColour(Slider::rotarySliderFillColourId, col); kQ.setColour(Slider::rotarySliderFillColourId, col);
    static const char* F[6] = {"e1F", "e2F", "e3F", "e4F", "e5F", "e6F"}; static const char* G[6] = {"e1G", "e2G", "e3G", "e4G", "e5G", "e6G"}; static const char* Q[6] = {"e1Q", "e2Q", "e3Q", "e4Q", "e5Q", "e6Q"};
    aF.reset(); aG.reset(); aQ.reset(); aT.reset(); aO.reset();
    kF.par = pr.apvts.getParameter(F[selBand]); kG.par = pr.apvts.getParameter(G[selBand]); kQ.par = pr.apvts.getParameter(Q[selBand]);
    aF = std::make_unique<APVTS::SliderAttachment>(pr.apvts, F[selBand], kF); aG = std::make_unique<APVTS::SliderAttachment>(pr.apvts, G[selBand], kG); aQ = std::make_unique<APVTS::SliderAttachment>(pr.apvts, Q[selBand], kQ);
    aT = std::make_unique<APVTS::ComboBoxAttachment>(pr.apvts, kEqT[selBand], bType); aO = std::make_unique<APVTS::ButtonAttachment>(pr.apvts, kEqO[selBand], bOn);
    for (int i = 0; i < 6; ++i) bandBtn[i].setToggleState(i == selBand, dontSendNotification);
    if (eqd) eqd->setSelected(selBand); repaint(); }
  void refreshPresets() {
    entries.clear(); pre.clear(dontSendNotification);
    for (int i = 0; i < pr.getNumPrograms(); ++i) entries.push_back({pr.getProgramName(i), pr.getProgramCategory(i), i, File()});
    auto files = Proc::presetDir().findChildFiles(File::findFiles, false, "*.xml");
    std::sort(files.begin(), files.end(), [](const File& a, const File& b) { return a.getFileName() < b.getFileName(); });
    for (auto& f : files) entries.push_back({f.getFileNameWithoutExtension(), "USER", -1, f});
    String lastCat; for (size_t i = 0; i < entries.size(); ++i) { if (entries[i].cat != lastCat) { lastCat = entries[i].cat; if (i > 0) pre.addSeparator(); pre.addSectionHeading(lastCat); } pre.addItem(entries[i].name, (int) i + 1); }
    auto cur = pr.currentName(); for (size_t i = 0; i < entries.size(); ++i) if (entries[i].name == cur) pre.setSelectedId((int) i + 1, dontSendNotification); }

  void wood(Graphics& g, int x0, int h, int seed) {
    Rectangle<float> a((float) x0, 0.f, (float) CHEEK, (float) h);
    ColourGradient gr(Colour(0xff3a1c0c), a.getX(), 0, Colour(0xff3a1c0c), a.getRight(), 0, false);
    gr.addColour(.35, Colour(0xff94562c)); gr.addColour(.7, Colour(0xff6e3d1d)); g.setGradientFill(gr); g.fillRect(a);
    Random r(seed);
    for (int i = 0; i < 300; ++i) { float x = a.getX() + r.nextFloat() * a.getWidth(), ph = r.nextFloat() * 6.28f, amp = .4f + r.nextFloat() * 1.6f, fq = .004f + r.nextFloat() * .01f; Path p;
      for (int y = 0; y <= h; y += 16) { float xx = x + std::sin((float) y * fq + ph) * amp; if (y) p.lineTo(xx, (float) y); else p.startNewSubPath(xx, 0.f); }
      g.setColour((r.nextBool() ? Colour(0xff1d0d04) : Colour(0xffc88a55)).withAlpha(.05f + r.nextFloat() * .2f)); g.strokePath(p, PathStrokeType(.5f + r.nextFloat() * 1.1f)); }
    for (int i = 0; i < 500; ++i) { g.setColour(Colour(0xff1d0d04).withAlpha(r.nextFloat() * .3f)); g.fillRect(a.getX() + r.nextFloat() * a.getWidth(), r.nextFloat() * (float) h, 1.f, 3.f + r.nextFloat() * 8.f); }
    g.setGradientFill(ColourGradient(Colour(0x40ffffff), a.getX(), 0, Colour(0x00ffffff), a.getX() + a.getWidth() * .55f, 0, false)); g.fillRect(a);
    g.setGradientFill(ColourGradient(Colour(0x30ffffff), 0, 0, Colour(0x00ffffff), 0, (float) h * .5f, false)); g.fillRect(a);
    g.setColour(Colour(0xcc000000)); g.drawVerticalLine(x0 == 0 ? CHEEK - 1 : x0, 0.f, (float) h); }
  void background(Graphics& g, int w, int h) {
    g.setGradientFill(ColourGradient(Colour(0xff414549), 0, 0, Colour(0xff1f2124), 0, (float) h, false)); g.fillAll();
    Random r(5); int cw = w - 2 * CHEEK;
    for (int y = 0; y < h; ++y) { g.setColour((r.nextBool() ? Colours::white : Colours::black).withAlpha(r.nextFloat() * .06f)); g.drawHorizontalLine(y, (float) CHEEK, (float) (w - CHEEK)); }
    for (int i = 0; i < 2000; ++i) { int y = r.nextInt(h), x = CHEEK + r.nextInt(cw), len = 40 + r.nextInt(300); g.setColour(Colours::white.withAlpha(r.nextFloat() * .05f)); g.drawHorizontalLine(y, (float) x, (float) (x + len)); }
    for (int i = 0; i < 60000; ++i) { g.setColour((r.nextBool() ? Colours::white : Colours::black).withAlpha(r.nextFloat() * .05f)); g.fillRect(CHEEK + r.nextInt(cw), r.nextInt(h), 1, 1); }
    g.setGradientFill(ColourGradient(Colour(0x22ffffff), 0, 0, Colour(0x00ffffff), 0, (float) h * .35f, false)); g.fillRect(CHEEK, 0, cw, h);
    Rectangle<float> hp((float) CHEEK + 10.f, 8.f, (float) cw - 20.f, 120.f);
    g.setColour(Colour(0x22000000)); g.fillRoundedRectangle(hp, 8.f);
    g.setColour(Colour(0x66ffffff)); g.drawRoundedRectangle(hp.translated(0.f, .9f), 8.f, 1.f);
    g.setColour(Colour(0xaa000000)); g.drawRoundedRectangle(hp, 8.f, 1.2f);
    for (int i = 0; i < 22; ++i) { float a = .42f * std::pow(1.f - (float) i / 22.f, 2.f); g.setColour(Colours::black.withAlpha(a));
      g.drawVerticalLine(CHEEK + i, 0.f, (float) h); g.drawVerticalLine(w - CHEEK - 1 - i, 0.f, (float) h); }
    wood(g, 0, h, 11); wood(g, w - CHEEK, h, 29);
    float sr = 6.f; screw(g, (float) CHEEK + 24.f, 22.f, sr, .3f); screw(g, (float) CHEEK + 24.f, 114.f, sr, 1.1f);
    screw(g, (float) (w - CHEEK) - 24.f, 22.f, sr, .8f); screw(g, (float) (w - CHEEK) - 24.f, 114.f, sr, 2.f);
    screw(g, (float) CHEEK + 24.f, (float) h - 16.f, sr, 1.5f); screw(g, (float) (w - CHEEK) - 24.f, (float) h - 16.f, sr, .1f); }
public:
  Panel(Proc& p) : pr(p) {
    setLookAndFeel(&lf);
    logo = ImageCache::getFromMemory(BinaryData::logo_png, BinaryData::logo_pngSize);
    for (auto& d : kP) ids.push_back(d.id);
    for (auto* x : kX) ids.push_back(x);
    for (auto& d : kP) {
      if (String(d.grp) == "EQ") continue;
      auto& c = cell(sec(d.grp), d.name);
      c.s = std::make_unique<Knob>(); c.s->par = pr.apvts.getParameter(d.id);
      c.s->setRotaryParameters(MathConstants<float>::pi * 1.2f, MathConstants<float>::pi * 2.8f, true);
      c.s->setTextBoxStyle(Slider::TextBoxBelow, false, CW - 6, 16); c.s->setColour(Slider::rotarySliderFillColourId, secColor(d.grp));
      c.sa = std::make_unique<APVTS::SliderAttachment>(pr.apvts, d.id, *c.s); addAndMakeVisible(*c.s); }
    extra("OSC B (GRANULAR)", "bY", "Sync", nullptr); extra("NOISE", "nTyp", "Type", nullptr);
    extra("FILTER", "fT", "Mode", nullptr); extra("FILTER", "fSl", "Slope", nullptr); extra("FILTER", "fCar", "Character", nullptr);
    extra("LFO 1", "l1S", "Shape", nullptr); extra("LFO 1", "l1Y", "Sync", nullptr); extra("LFO 2", "l2S", "Shape", nullptr); extra("LFO 2", "l2Y", "Sync", nullptr);
    extra("GLOBAL", "leg", "Mode", "Legato"); extra("SATURATION", "sMode", "Mode", nullptr); extra("TUBE PREAMP", "tOn", "Insert", "Active");
    extra("PUMP", "pY", "Sync", nullptr); extra("ECHO", "dY", "Time", nullptr); extra("ECHO", "dPP", "Mode", "Ping-pong");
    sec("OUTPUT").extraW = 3 * CW; sec("TUBE PREAMP").extraW = 3 * CW + 6; sec("EQ").extraW = EQW + 14 + 270;
    for (auto* id : ids) pr.apvts.addParameterListener(id, this);
    refreshPresets();
    pre.onChange = [this] { int i = pre.getSelectedId() - 1; if (i < 0 || i >= (int) entries.size()) return; auto& e = entries[(size_t) i];
      if (e.factory >= 0) pr.setCurrentProgram(e.factory); else pr.loadUser(e.file); };
    auto step = [this](int dlt) { int n = (int) entries.size(), id = pre.getSelectedId(); if (n > 0) pre.setSelectedId(((id - 1 + dlt + n) % n) + 1, sendNotificationSync); };
    prev.onClick = [step] { step(-1); }; next.onClick = [step] { step(1); };
    save.onClick = [this] {
      auto* aw = new AlertWindow("Save preset", "Preset name:", MessageBoxIconType::NoIcon);
      aw->addTextEditor("name", pr.currentName()); aw->addButton("Save", 1, KeyPress(KeyPress::returnKey)); aw->addButton("Cancel", 0, KeyPress(KeyPress::escapeKey));
      auto* cb = new SaveCb(); Component::SafePointer<Panel> ed(this); Component::SafePointer<AlertWindow> sp(aw);
      cb->f = [ed, sp](int r) { if (r == 1 && ed != nullptr && sp != nullptr) { auto n = sp->getTextEditorContents("name").trim(); if (n.isNotEmpty()) { ed->pr.saveUser(n); ed->refreshPresets(); } } };
      aw->enterModalState(true, cb, true); };
    addAndMakeVisible(pre); addAndMakeVisible(prev); addAndMakeVisible(next); addAndMakeVisible(save);
    screen = std::make_unique<ScreenView>(pr, last, ids); addAndMakeVisible(*screen);
    tube = std::make_unique<TubeView>(pr); addAndMakeVisible(*tube); meter = std::make_unique<MeterView>(pr); addAndMakeVisible(*meter);
    eqd = std::make_unique<EqDisplay>(pr); eqd->onSelect = [this](int b) { selectBand(b); }; addAndMakeVisible(*eqd);
    for (int i = 0; i < 6; ++i) { bandBtn[i].setButtonText(String(i + 1)); bandBtn[i].setClickingTogglesState(false); bandBtn[i].onClick = [this, i] { selectBand(i); }; addAndMakeVisible(bandBtn[i]); }
    for (auto* k : {&kF, &kG, &kQ}) { k->setRotaryParameters(MathConstants<float>::pi * 1.2f, MathConstants<float>::pi * 2.8f, true); k->setTextBoxStyle(Slider::TextBoxBelow, false, CW - 6, 16); addAndMakeVisible(*k); }
    for (auto* l : {&lF, &lG, &lQ}) { l->setJustificationType(Justification::centred); addAndMakeVisible(*l); }
    lF.setText("Freq", dontSendNotification); lG.setText("Gain", dontSendNotification); lQ.setText("Q", dontSendNotification);
    bType.addItemList(StringArray{"Bell", "Low Shelf", "High Shelf", "Low Cut 12", "Low Cut 24", "High Cut 12", "High Cut 24", "Notch"}, 1);
    addAndMakeVisible(bType); addAndMakeVisible(bOn); addAndMakeVisible(eqAct); aA = std::make_unique<APVTS::ButtonAttachment>(pr.apvts, "eOn", eqAct);
    selectBand(0); setSize(W, H); startTimerHz(15); }
  ~Panel() override { stopTimer(); for (auto* id : ids) pr.apvts.removeParameterListener(id, this); setLookAndFeel(nullptr); }
  void paint(Graphics& g) override {
    g.drawImageAt(bg, 0, 0);
    g.drawImageWithin(logo, CHEEK + 44, 20, 92, 92, RectanglePlacement::centred);
    g.setFont(27.f); g.setColour(Colours::black.withAlpha(.75f)); g.drawText("OKTOPLUS SYNTH", 172, 33, 250, 34, Justification::left);
    g.setColour(Colour(0xfff0f2f4)); g.drawText("OKTOPLUS SYNTH", 172, 31, 250, 34, Justification::left);
    led(g, 184.f, 92.f, Colour(0xff58b4ff), true); led(g, 262.f, 92.f, Colour(0xff5cff8a), voiceLit);
    g.setFont(11.f); g.setColour(Colour(0xffc9ced3)); g.drawText("POWER", 194, 84, 60, 16, Justification::left); g.drawText("VOICE", 272, 84, 50, 16, Justification::left);
    g.setColour(Colour(0xaa000000)); g.drawHorizontalLine(134, (float) CHEEK + 10.f, (float) getWidth() - CHEEK - 10.f);
    g.setColour(Colour(0x44ffffff)); g.drawHorizontalLine(135, (float) CHEEK + 10.f, (float) getWidth() - CHEEK - 10.f);
    for (auto& s : secs) { auto a = s->area.toFloat(); if (a.isEmpty()) continue; auto col = secColor(s->title);
      g.setColour(Colour(0x1c000000)); g.fillRoundedRectangle(a, 6.f);
      g.setColour(Colour(0xaa000000)); g.drawRoundedRectangle(a, 6.f, 1.2f); g.setColour(Colour(0x30ffffff)); g.drawRoundedRectangle(a.translated(0.f, 1.2f), 6.f, 1.f);
      g.setColour(Colour(0xfff0f2f4)); g.setFont(13.f); g.drawText(s->title, s->area.getX() + 10, s->area.getY() + 3, s->area.getWidth() - 16, 18, Justification::left);
      g.setColour(col); g.fillRect(s->area.getX() + 8, s->area.getY() + 22, s->area.getWidth() - 16, 2); } }
  void resized() override {
    bg = Image(Image::RGB, getWidth(), getHeight(), true); { Graphics g(bg); background(g, getWidth(), getHeight()); }
    screen->setBounds(520, 12, 560, 104);
    prev.setBounds(1130, 34, 30, 28); pre.setBounds(1164, 34, 260, 28); next.setBounds(1428, 34, 30, 28); save.setBounds(1130, 70, 110, 28);
    static const char* rows[6][4] = {{"OSC A (ADDITIVE)", "OSC B (GRANULAR)", "NOISE", nullptr}, {"FILTER", "FILTER ENV", "AMP ENV", nullptr}, {"LFO 1", "LFO 2", "GLOBAL", "OUTPUT"},
                                     {"SATURATION", "TUBE PREAMP", "PUMP", nullptr}, {"EQ", nullptr, nullptr, nullptr}, {"ECHO", "REVERB", nullptr, nullptr}};
    int y = 142;
    for (int r = 0; r < 6; ++r) { int x = CHEEK + 14, rh = (r == 4) ? 28 + EQH + 8 : 28 + CH + 8;
      for (int k = 0; k < 4 && rows[r][k]; ++k) { auto& s = sec(rows[r][k]); int n = (int) s.cells.size(); s.area = {x, y, n * CW + s.extraW + 12, rh};
        for (int i = 0; i < n; ++i) { auto& c = *s.cells[(size_t) i]; int bx = x + 6 + i * CW, by = y + 28;
          c.l.setBounds(bx, by, CW, 16);
          if (c.s) c.s->setBounds(bx, by + 16, CW, CH - 16); else c.main()->setBounds(bx + 4, by + 46, CW - 8, 26); }
        int ex = x + 6 + n * CW;
        if (s.title == "TUBE PREAMP") tube->setBounds(ex + 4, y + 30, 3 * CW - 2, CH - 4);
        if (s.title == "OUTPUT") meter->setBounds(x + 8, y + 30, 3 * CW - 6, CH - 4);
        if (s.title == "EQ") { eqd->setBounds(x + 8, y + 30, EQW, EQH - 4); int px = x + 8 + EQW + 16;
          for (int i = 0; i < 6; ++i) bandBtn[i].setBounds(px + i * 44, y + 30, 40, 26);
          lF.setBounds(px, y + 62, CW, 16); lG.setBounds(px + CW, y + 62, CW, 16); lQ.setBounds(px + 2 * CW, y + 62, CW, 16);
          kF.setBounds(px, y + 78, CW, CH - 16); kG.setBounds(px + CW, y + 78, CW, CH - 16); kQ.setBounds(px + 2 * CW, y + 78, CW, CH - 16);
          bType.setBounds(px + 4, y + 184, 150, 26); bOn.setBounds(px + 160, y + 184, 100, 26); eqAct.setBounds(px + 4, y + 216, 150, 26); }
        x += n * CW + s.extraW + 12 + 22; }
      y += rh + 8; } }
};

class Editor : public AudioProcessorEditor {
  Panel panel;
public:
  Editor(Proc& p) : AudioProcessorEditor(&p), panel(p) {
    addAndMakeVisible(panel); setResizable(true, true);
    setResizeLimits(640, (int) (640.0 * Panel::H / Panel::W), 2200, (int) (2200.0 * Panel::H / Panel::W)); getConstrainer()->setFixedAspectRatio((double) Panel::W / (double) Panel::H);
    setSize((int) (Panel::W * .8), (int) (Panel::H * .8)); }
  void resized() override { panel.setBounds(0, 0, Panel::W, Panel::H); panel.setTransform(AffineTransform::scale((float) getWidth() / (float) Panel::W)); }
};

AudioProcessorEditor* Proc::createEditor() { return new Editor(*this); }
AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new Proc(); }
