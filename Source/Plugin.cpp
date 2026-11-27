// Atelier Grain - synthe additif / soustractif / granulaire (VST3, JUCE 8)
#include <juce_audio_utils/juce_audio_utils.h>
#include <algorithm>
#include <BinaryData.h>
using namespace juce;
using APVTS = AudioProcessorValueTreeState;

namespace K { enum { aLvl, aPart, aTilt, aOdd, aSemi, bLvl, bSize, bDens, bSpr, bSemi, cut, res, fEnv,
  aA, aD, aS, aR, fA, fD, fS, fR, l1R, l1C, l1P, l2R, l2A, l2B, gld, drv, mst, NP }; }

struct PD { const char *id, *grp, *name; float mn, mx, def, sk; char u; };
static const PD kP[] = {
 {"aLvl","OSC A additif","Niveau",0,1,.7f,1,'p'},{"aPart","OSC A additif","Partiels",1,32,12,1,'c'},
 {"aTilt","OSC A additif","Pente",0,1,.45f,1,'p'},{"aOdd","OSC A additif","Impair",0,1,0,1,'p'},{"aSemi","OSC A additif","Hauteur",-24,24,0,1,'t'},
 {"bLvl","OSC B granulaire","Niveau",0,1,.4f,1,'p'},{"bSize","OSC B granulaire","Taille",5,400,80,.5f,'m'},
 {"bDens","OSC B granulaire","Densite",1,150,30,.5f,'n'},{"bSpr","OSC B granulaire","Disperse",0,1,.3f,1,'p'},{"bSemi","OSC B granulaire","Hauteur",-24,24,12,1,'t'},
 {"cut","FILTRE","Freq",20,18000,2500,.3f,'h'},{"res","FILTRE","Reso",0,1,.2f,1,'p'},{"fEnv","FILTRE","Env",-1,1,.4f,1,'p'},
 {"aA","ENV AMP","A",.001f,5,.01f,.3f,'s'},{"aD","ENV AMP","D",.001f,5,.3f,.3f,'s'},{"aS","ENV AMP","S",0,1,.7f,1,'p'},{"aR","ENV AMP","R",.001f,8,.4f,.3f,'s'},
 {"fA","ENV FILTRE","A",.001f,5,.01f,.3f,'s'},{"fD","ENV FILTRE","D",.001f,5,.4f,.3f,'s'},{"fS","ENV FILTRE","S",0,1,.3f,1,'p'},{"fR","ENV FILTRE","R",.001f,8,.4f,.3f,'s'},
 {"l1R","LFO 1","Vitesse",.05f,20,2,.4f,'h'},{"l1C","LFO 1","Filtre",0,1,0,1,'p'},{"l1P","LFO 1","Hauteur",0,1,0,1,'p'},
 {"l2R","LFO 2","Vitesse",.05f,20,.5f,.4f,'h'},{"l2A","LFO 2","Volume",0,1,0,1,'p'},{"l2B","LFO 2","Grains",0,1,0,1,'p'},
 {"gld","GLOBAL","Glide",0,2,0,.4f,'s'},{"drv","GLOBAL","Satur.",0,1,.2f,1,'p'},{"mst","GLOBAL","Master",0,1,.7f,1,'p'}};

struct Pre { const char* n; std::vector<std::pair<const char*, float>> v; };
static const std::vector<Pre> kPre = {
 {"Init", {}},
 {"Pad Chaleureux", {{"aLvl",.6f},{"aPart",10.f},{"aTilt",.7f},{"bLvl",.25f},{"bSize",200.f},{"bDens",20.f},{"cut",1800.f},{"res",.15f},{"fEnv",.3f},
   {"aA",1.2f},{"aD",1.f},{"aS",.8f},{"aR",2.f},{"fA",1.5f},{"fS",.6f},{"l1R",.3f},{"l1C",.25f},{"drv",.2f}}},
 {"Nuage de Verre", {{"aLvl",.15f},{"aPart",6.f},{"aTilt",.3f},{"bLvl",.8f},{"bSize",60.f},{"bDens",70.f},{"bSpr",.8f},{"bSemi",12.f},
   {"cut",7000.f},{"res",.1f},{"aA",.6f},{"aS",1.f},{"aR",2.5f},{"l2R",.2f},{"l2B",.5f}}},
 {"Basse Legato", {{"aLvl",.8f},{"aPart",20.f},{"aTilt",.25f},{"bLvl",0.f},{"cut",500.f},{"res",.35f},{"fEnv",.5f},{"aA",.005f},{"aD",.2f},{"aS",.8f},{"aR",.15f},
   {"fD",.25f},{"fS",.1f},{"gld",.08f},{"drv",.35f},{"leg",1.f}}},
 {"Orgue Additif", {{"aLvl",.8f},{"aPart",16.f},{"aTilt",.4f},{"aOdd",1.f},{"bLvl",0.f},{"cut",12000.f},{"res",0.f},{"fEnv",0.f},{"aA",.01f},{"aS",1.f},{"aR",.12f},{"l1R",5.f},{"l1P",.04f}}},
 {"Pluck Cristal", {{"aLvl",.7f},{"aPart",8.f},{"aTilt",.5f},{"aSemi",12.f},{"bLvl",.2f},{"bSize",30.f},{"bDens",50.f},{"cut",900.f},{"res",.3f},{"fEnv",.9f},
   {"aA",.002f},{"aD",.35f},{"aS",0.f},{"aR",.3f},{"fD",.3f},{"fS",0.f}}},
 {"Poussiere", {{"aLvl",0.f},{"bLvl",.9f},{"bSize",15.f},{"bDens",120.f},{"bSpr",1.f},{"bSemi",0.f},{"cut",5000.f},{"res",.25f},{"fT",1.f},{"drv",.5f},{"aA",.3f},{"aS",1.f},{"aR",1.f},{"l2R",.7f},{"l2B",.6f}}},
 {"Gate 1/8", {{"aLvl",.8f},{"aPart",14.f},{"aTilt",.4f},{"bLvl",0.f},{"cut",3500.f},{"l2Y",10.f},{"l2S",3.f},{"l2A",.9f},{"aA",.01f},{"aS",1.f},{"aR",.2f}}},
 {"Acid Lead", {{"aLvl",.8f},{"aPart",24.f},{"aTilt",.2f},{"bLvl",0.f},{"cut",700.f},{"res",.7f},{"fEnv",.8f},{"aA",.003f},{"aD",.25f},{"aS",.6f},{"aR",.1f},{"fD",.2f},{"fS",0.f},{"gld",.06f},{"drv",.55f},{"leg",1.f}}}
};

static String fmt(char u, float v) {
  switch (u) {
    case 'h': return v >= 1000.f ? String(v / 1000.f, 2) + " kHz" : String(v, v < 100.f ? 1 : 0) + " Hz";
    case 's': return String(v, v < 1.f ? 3 : 2) + " s";
    case 'm': return String((int) v) + " ms";
    case 't': return String((int) std::round(v)) + " st";
    case 'n': return String((int) std::round(v)) + " /s";
    case 'p': return String((int) std::round(v * 100.f)) + " %";
    case 'c': return String((int) std::round(v));
    default:  return String(v, 2); } }

static const char* kX[] = {"l1S", "l2S", "fT", "fSl", "leg", "l1Y", "l2Y", "bY"};
static StringArray syncNames() {
  StringArray s{"Libre"}; const char* d[] = {"1/1", "1/2", "1/4", "1/8", "1/16", "1/32", "1/64", "1/128"};
  for (auto* n : d) { s.add(n); s.add(String(n) + " pt"); s.add(String(n) + " tr"); }
  return s; }
static float rateOf(int sy, float freeHz, double bpm) {
  if (sy <= 0) return freeHz;
  static const float len[8] = {4.f, 2.f, 1.f, .5f, .25f, .125f, .0625f, .03125f};
  int d = (sy - 1) / 3, t = (sy - 1) % 3;
  return (float) (bpm / 60.0) / (len[d] * (t == 1 ? 1.5f : t == 2 ? 2.f / 3.f : 1.f)); }
static void makeCycle(float* o, int n, float P, float T, float O) {
  int N = jlimit(1, 32, (int) (P + .5f)); float s = .2f + T * 2.f, pk = 0, a[33];
  for (int k = 1; k <= N; ++k) a[k] = std::pow((float) k, -s) * ((k % 2 == 0) ? 1.f - O : 1.f);
  for (int i = 0; i < n; ++i) { float ph = MathConstants<float>::twoPi * (float) i / (float) n, y = 0;
    for (int k = 1; k <= N; ++k) y += a[k] * std::sin(ph * (float) k);
    o[i] = y; pk = jmax(pk, std::abs(y)); }
  if (pk > 0) for (int i = 0; i < n; ++i) o[i] /= pk; }

static APVTS::ParameterLayout layout() {
  APVTS::ParameterLayout L;
  for (auto& p : kP)
    L.add(std::make_unique<AudioParameterFloat>(ParameterID{p.id, 1}, String(p.grp) + " " + p.name, NormalisableRange<float>(p.mn, p.mx, 0.f, p.sk), p.def,
      AudioParameterFloatAttributes().withStringFromValueFunction([u = p.u](float v, int) { return fmt(u, v); })));
  StringArray sh{"Sinus", "Triangle", "Dent", "Carre", "Hasard"};
  L.add(std::make_unique<AudioParameterChoice>(ParameterID{"l1S", 1}, "LFO 1 forme", sh, 0));
  L.add(std::make_unique<AudioParameterChoice>(ParameterID{"l2S", 1}, "LFO 2 forme", sh, 0));
  L.add(std::make_unique<AudioParameterChoice>(ParameterID{"fT", 1}, "Filtre type", StringArray{"Passe-bas", "Passe-haut"}, 0));
  L.add(std::make_unique<AudioParameterChoice>(ParameterID{"fSl", 1}, "Filtre pente", StringArray{"12 dB", "24 dB"}, 1));
  L.add(std::make_unique<AudioParameterBool>(ParameterID{"leg", 1}, "Legato", false));
  L.add(std::make_unique<AudioParameterChoice>(ParameterID{"l1Y", 1}, "LFO 1 sync", syncNames(), 0));
  L.add(std::make_unique<AudioParameterChoice>(ParameterID{"l2Y", 1}, "LFO 2 sync", syncNames(), 0));
  L.add(std::make_unique<AudioParameterChoice>(ParameterID{"bY", 1}, "Grains sync", syncNames(), 0));
  return L;
}

struct SVF { float s1 = 0, s2 = 0;
  void run(float x, float g, float R, float& lp, float& hp) {
    hp = (x - (2 * R + g) * s1 - s2) / (1 + 2 * R * g + g * g);
    float bp = g * hp + s1; lp = g * bp + s2; s1 = g * hp + bp; s2 = g * bp + lp; } };
struct Grain { bool on = false; float ph = 0, inc = 0, len = 1, age = 0; };
struct Voice { bool on = false, gate = false; int note = -1; float freq = 440, tgt = 440, pA = 0, gAcc = 0, vel = 1, age = 0;
  ADSR ea, ef; SVF s1, s2; Grain g[24]; };
struct LFO { float ph = 0, sh = 0; };

class Proc : public AudioProcessor {
public:
  Proc() : AudioProcessor(BusesProperties().withOutput("Out", AudioChannelSet::stereo(), true)), apvts(*this, nullptr, "ST", layout()) {}
  const String getName() const override { return "Atelier Grain"; }
  void prepareToPlay(double sr, int) override { fs = sr; for (auto& v : vs) { v = Voice(); v.ea.setSampleRate(sr); v.ef.setSampleRate(sr); } stack.clear(); lastF = 261.6f; rebuild(true); }
  void releaseResources() override {}
  bool isBusesLayoutSupported(const BusesLayout& l) const override { return l.getMainOutputChannelSet() == AudioChannelSet::stereo(); }
  bool acceptsMidi() const override { return true; }
  bool producesMidi() const override { return false; }
  double getTailLengthSeconds() const override { return 8.0; }
  AudioProcessorEditor* createEditor() override;
  bool hasEditor() const override { return true; }
  int getNumPrograms() override { return (int) kPre.size(); }
  int getCurrentProgram() override { return cur; }
  const String getProgramName(int i) override { return kPre[(size_t) i].n; }
  void changeProgramName(int, const String&) override {}
  void setCurrentProgram(int i) override {
    cur = i;
    for (auto* p : getParameters()) p->setValueNotifyingHost(p->getDefaultValue());
    for (auto& kv : kPre[(size_t) i].v) { auto* p = apvts.getParameter(kv.first); if (p) p->setValueNotifyingHost(p->convertTo0to1(kv.second)); } }
  void getStateInformation(MemoryBlock& b) override { if (auto x = apvts.copyState().createXml()) copyXmlToBinary(*x, b); }
  void setStateInformation(const void* d, int n) override { if (auto x = getXmlFromBinary(d, n)) apvts.replaceState(ValueTree::fromXml(*x)); }

  void processBlock(AudioBuffer<float>& b, MidiBuffer& m) override {
    ScopedNoDenormals nd; b.clear(); rebuild(false);
    for (int i = 0; i < K::NP; ++i) q[i] = apvts.getRawParameterValue(kP[i].id)->load();
    sh1 = (int) apvts.getRawParameterValue("l1S")->load(); sh2 = (int) apvts.getRawParameterValue("l2S")->load();
    fT = (int) apvts.getRawParameterValue("fT")->load(); fSl = (int) apvts.getRawParameterValue("fSl")->load();
    leg = apvts.getRawParameterValue("leg")->load() > .5f;
    double bpm = 120.0; if (auto* ph = getPlayHead()) if (auto info = ph->getPosition()) if (auto bp = info->getBpm()) bpm = *bp;
    rt1 = rateOf((int) apvts.getRawParameterValue("l1Y")->load(), q[K::l1R], bpm);
    rt2 = rateOf((int) apvts.getRawParameterValue("l2Y")->load(), q[K::l2R], bpm);
    dens = rateOf((int) apvts.getRawParameterValue("bY")->load(), q[K::bDens], bpm);
    int pos = 0, n = b.getNumSamples();
    for (const auto md : m) { int ts = jlimit(0, n, md.samplePosition); render(b, pos, ts); pos = ts; handle(md.getMessage()); }
    render(b, pos, n); }

  APVTS apvts;
private:
  double fs = 44100; Voice vs[8]; std::vector<int> stack; float q[K::NP] {}; int sh1 = 0, sh2 = 0, fT = 0, fSl = 1, cur = 0; bool leg = false; float rt1 = 2, rt2 = .5f, dens = 30;
  float tab[2048] {}, cP = -1, cT = -1, cO = -1, lastF = 261.6f; LFO a1, a2; Random rnd;

  void rebuild(bool force) {
    float P = apvts.getRawParameterValue("aPart")->load(), T = apvts.getRawParameterValue("aTilt")->load(), O = apvts.getRawParameterValue("aOdd")->load();
    if (!force && P == cP && T == cT && O == cO) return;
    cP = P; cT = T; cO = O; makeCycle(tab, 2048, P, T, O); }
  float rd(float ph) const { float x = ph * 2048.f; int i = (int) x & 2047; float fr = x - std::floor(x); return tab[i] + (tab[(i + 1) & 2047] - tab[i]) * fr; }
  float lfo(LFO& l, float rate, int shape) {
    l.ph += rate / (float) fs; if (l.ph >= 1.f) { l.ph -= 1.f; l.sh = rnd.nextFloat() * 2.f - 1.f; }
    switch (shape) { case 0: return std::sin(6.2831853f * l.ph); case 1: return 4.f * std::abs(l.ph - .5f) - 1.f;
      case 2: return 2.f * l.ph - 1.f; case 3: return l.ph < .5f ? 1.f : -1.f; default: return l.sh; } }
  void start(Voice& v, int n, float f, float vel) {
    v.on = true; v.gate = true; v.note = n; v.age = 0; v.tgt = f; v.freq = q[K::gld] > .002f ? lastF : f; v.vel = vel; v.pA = 0; v.gAcc = 0;
    v.s1 = SVF(); v.s2 = SVF(); for (auto& g : v.g) g.on = false;
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
      float sp = q[K::bSpr]; g.on = true; g.age = 0; g.ph = rnd.nextFloat();
      g.len = jmax(32.f, q[K::bSize] * .001f * (float) fs * (1.f + (rnd.nextFloat() - .5f) * sp));
      g.inc = v.freq * pitB * std::exp2((rnd.nextFloat() - .5f) * sp * 100.f / 1200.f) / (float) fs; return; } }
  void render(AudioBuffer<float>& b, int s, int e) {
    float* L = b.getWritePointer(0); float* R = b.getWritePointer(1);
    float pitA = std::exp2(q[K::aSemi] / 12.f), pitB = std::exp2(q[K::bSemi] / 12.f), F = (float) fs;
    float gn = 1.f / std::sqrt(jmax(1.f, dens * q[K::bSize] * .001f));
    float gc = q[K::gld] < .002f ? 1.f : 1.f - std::exp(-1.f / (q[K::gld] * F));
    float dr = 1.f + q[K::drv] * 8.f, sn = 1.f / std::sqrt(dr);
    for (int i = s; i < e; ++i) {
      float l1 = lfo(a1, rt1, sh1), l2 = lfo(a2, rt2, sh2), pm = std::exp2(l1 * q[K::l1P] * .5f), out = 0;
      for (auto& v : vs) {
        if (!v.on) continue;
        v.age += 1.f; v.freq += (v.tgt - v.freq) * gc;
        v.pA += v.freq * pitA * pm / F; v.pA -= std::floor(v.pA);
        float x = rd(v.pA) * q[K::aLvl];
        v.gAcc += dens / F; if (v.gAcc >= 1.f) { v.gAcc -= 1.f; spawn(v, pitB); }
        float bs = 0;
        for (auto& g : v.g) { if (!g.on) continue;
          bs += (.5f - .5f * std::cos(6.2831853f * g.age / g.len)) * rd(g.ph);
          g.ph += g.inc; g.ph -= std::floor(g.ph); if (++g.age >= g.len) g.on = false; }
        x += bs * gn * q[K::bLvl] * jmax(0.f, 1.f + l2 * q[K::l2B]);
        float fe = v.ef.getNextSample(), ae = v.ea.getNextSample();
        float fc = jlimit(20.f, F * .45f, q[K::cut] * std::exp2(q[K::fEnv] * fe * 5.f + l1 * q[K::l1C] * 3.f));
        float g = std::tan(3.14159265f * fc / F), lp, hp;
        v.s1.run(x, g, .7f - .64f * q[K::res], lp, hp); float y = fT ? hp : lp;
        if (fSl >= 1) { v.s2.run(y, g, .7f, lp, hp); y = fT ? hp : lp; }
        y = std::tanh(y * dr) * sn;
        out += y * ae * v.vel * (1.f - q[K::l2A] * (.5f + .5f * l2));
        if (!v.ea.isActive()) v.on = false; }
      out *= q[K::mst] * .5f; L[i] += out; R[i] += out; } }
};

// ---------------- interface : panneau noir, gros boutons, joues en bois ----------------
static const Colour kAcc[8] = {Colour(0xff4aa8ff), Colour(0xff35d0b0), Colour(0xffff8a2a), Colour(0xffe8c23a),
                               Colour(0xffe8c23a), Colour(0xffc06bff), Colour(0xffc06bff), Colour(0xffff5c7a)};

struct Dark : LookAndFeel_V4 {
  Dark() {
    setColour(Slider::textBoxTextColourId, Colour(0xffa9b0b6)); setColour(Slider::textBoxOutlineColourId, Colours::transparentBlack);
    setColour(Slider::textBoxBackgroundColourId, Colours::transparentBlack); setColour(Label::textColourId, Colour(0xffd5d9dc));
    setColour(ComboBox::backgroundColourId, Colour(0xff2a2d31)); setColour(ComboBox::outlineColourId, Colour(0xff484c52));
    setColour(ComboBox::textColourId, Colour(0xffe6e9eb)); setColour(ComboBox::arrowColourId, Colour(0xffe6e9eb));
    setColour(PopupMenu::backgroundColourId, Colour(0xff1f2124)); setColour(PopupMenu::textColourId, Colour(0xffe6e9eb));
    setColour(PopupMenu::highlightedBackgroundColourId, Colour(0xffff8a2a)); setColour(PopupMenu::highlightedTextColourId, Colours::black);
    setColour(ToggleButton::textColourId, Colour(0xffd5d9dc)); setColour(ToggleButton::tickColourId, Colour(0xffff8a2a));
    setColour(TextButton::buttonColourId, Colour(0xff2a2d31)); setColour(TextButton::textColourOffId, Colour(0xffe6e9eb)); }
  void drawRotarySlider(Graphics& g, int x, int y, int w, int h, float pos, float a0, float a1, Slider& s) override {
    float d = (float) jmin(w, h) - 18.f, r = d * .5f; Point<float> c((float) x + (float) w * .5f, (float) y + (float) h * .5f);
    auto acc = s.findColour(Slider::rotarySliderFillColourId);
    for (int i = 0; i <= 20; ++i) { float t = (float) i / 20.f, an = a0 + t * (a1 - a0);
      g.setColour(t <= pos + .001f ? acc : Colour(0x55ffffff));
      g.fillEllipse(c.x + std::sin(an) * (r + 6.f) - 1.5f, c.y - std::cos(an) * (r + 6.f) - 1.5f, 3.f, 3.f); }
    g.setColour(Colours::black.withAlpha(.65f)); g.fillEllipse(c.x - r + 1, c.y - r + 3, d, d);
    g.setGradientFill(ColourGradient(Colour(0xff4b4d51), c.x - r * .6f, c.y - r * .8f, Colour(0xff0b0c0d), c.x + r * .7f, c.y + r * .9f, false));
    g.fillEllipse(c.x - r, c.y - r, d, d);
    for (int i = 0; i < 36; ++i) { float an = (float) i * MathConstants<float>::twoPi / 36.f; Point<float> u(std::sin(an), -std::cos(an));
      g.setColour(Colour(0x1fffffff)); g.drawLine(c.x + u.x * (r - 1), c.y + u.y * (r - 1), c.x + u.x * (r - 4), c.y + u.y * (r - 4), 1.f); }
    g.setColour(Colours::black); g.drawEllipse(c.x - r, c.y - r, d, d, 1.4f);
    float cr = r * .64f;
    g.setGradientFill(ColourGradient(Colour(0xff6c7076), c.x - cr * .5f, c.y - cr * .7f, Colour(0xff222427), c.x + cr * .6f, c.y + cr * .8f, false));
    g.fillEllipse(c.x - cr, c.y - cr, cr * 2, cr * 2); g.setColour(Colour(0xff08090a)); g.drawEllipse(c.x - cr, c.y - cr, cr * 2, cr * 2, 1.f);
    Path p; p.addRoundedRectangle(-1.8f, -cr + 2.f, 3.6f, cr * .62f, 1.6f);
    g.setColour(Colours::white); g.fillPath(p, AffineTransform::rotation(a0 + pos * (a1 - a0)).translated(c)); }
};

class ScreenView : public Component, private Timer {
  Proc& pr; const std::atomic<int>& last; const std::vector<const char*>& ids;
  float P(const char* id) const { return pr.apvts.getRawParameterValue(id)->load(); }
  void frame(Graphics& g, Rectangle<float> r, const String& cap) {
    g.setColour(Colour(0xff0a1a20)); g.fillRoundedRectangle(r, 4.f); g.setColour(Colour(0xff1d4250));
    for (int i = 1; i < 4; ++i) g.drawHorizontalLine((int) (r.getY() + r.getHeight() * (float) i / 4.f), r.getX(), r.getRight());
    g.drawRoundedRectangle(r, 4.f, 1.f); g.setColour(Colour(0xff6fb4c8)); g.setFont(10.f);
    g.drawText(cap, (int) r.getX(), (int) r.getBottom() + 2, (int) r.getWidth(), 12, Justification::centred); }
public:
  ScreenView(Proc& p, const std::atomic<int>& l, const std::vector<const char*>& i) : pr(p), last(l), ids(i) { startTimerHz(20); }
  void timerCallback() override { repaint(); }
  void paint(Graphics& g) override {
    auto b = getLocalBounds().toFloat();
    g.setColour(Colour(0xff030506)); g.fillRoundedRectangle(b, 9.f);
    g.setGradientFill(ColourGradient(Colour(0xff0e1d23), 0, 0, Colour(0xff060d10), 0, b.getHeight(), false)); g.fillRoundedRectangle(b.reduced(4.f), 6.f);
    Colour amber(0xffffb347), cyan(0xff58d6ff);
    int cur = pr.getCurrentProgram();
    g.setColour(Colour(0xff6fb4c8)); g.setFont(11.f);
    g.drawText("PATCH " + String(cur + 1).paddedLeft('0', 2) + " / " + String(pr.getNumPrograms()), 18, 12, 200, 14, Justification::left);
    g.setColour(amber); g.setFont(21.f); g.drawText(pr.getProgramName(cur), 18, 26, 205, 26, Justification::left, true);
    int li = last.load();
    if (li >= 0 && li < (int) ids.size()) if (auto* p = pr.apvts.getParameter(ids[(size_t) li])) {
      g.setColour(cyan); g.setFont(12.f); g.drawText(p->getName(24), 18, 62, 205, 16, Justification::left, true);
      g.setColour(Colours::white); g.setFont(21.f); g.drawText(p->getCurrentValueAsText(), 18, 76, 205, 24, Justification::left, true); }
    float top = 14.f, hh = 58.f, w = 96.f;
    { auto r = Rectangle<float>(236.f, top, w, hh); frame(g, r, "OSC A");
      float t[96]; makeCycle(t, 96, P("aPart"), P("aTilt"), P("aOdd")); Path p;
      for (int i = 0; i < 96; ++i) { float x = r.getX() + r.getWidth() * (float) i / 95.f, y = r.getCentreY() - t[i] * r.getHeight() * .42f; if (i) p.lineTo(x, y); else p.startNewSubPath(x, y); }
      g.setColour(cyan); g.strokePath(p, PathStrokeType(1.6f)); }
    { auto r = Rectangle<float>(348.f, top, w, hh); frame(g, r, "FILTRE");
      float cut = P("cut"), res = P("res"), R1 = .7f - .64f * res; int ty = (int) P("fT"), sl = (int) P("fSl"); Path p;
      for (int i = 0; i < 96; ++i) { float f = 20.f * std::pow(1000.f, (float) i / 95.f), rr = f / cut;
        auto st = [&](float R) { return (ty ? rr * rr : 1.f) / std::sqrt((1 - rr * rr) * (1 - rr * rr) + 4 * R * R * rr * rr); };
        float m = st(R1); if (sl) m *= st(.7f);
        float db = jlimit(-48.f, 24.f, 20.f * std::log10(jmax(m, 1e-4f)));
        float x = r.getX() + r.getWidth() * (float) i / 95.f, y = jmap(db, -48.f, 24.f, r.getBottom() - 2.f, r.getY() + 2.f);
        if (i) p.lineTo(x, y); else p.startNewSubPath(x, y); }
      g.setColour(amber); g.strokePath(p, PathStrokeType(1.6f)); }
    { auto r = Rectangle<float>(460.f, top, w, hh); frame(g, r, "ENV AMP");
      float A = P("aA"), D = P("aD"), S = P("aS"), Rl = P("aR");
      float wa = std::pow(A, .35f), wd = std::pow(D, .35f), wr = std::pow(Rl, .35f), ws = .6f * (wa + wd + wr) / 3.f, tot = wa + wd + ws + wr, k = r.getWidth() / tot;
      float yb = r.getBottom() - 3.f, yt = r.getY() + 3.f, ys = jmap(S, 0.f, 1.f, yb, yt); Path p;
      p.startNewSubPath(r.getX(), yb); p.lineTo(r.getX() + wa * k, yt); p.lineTo(r.getX() + (wa + wd) * k, ys);
      p.lineTo(r.getX() + (wa + wd + ws) * k, ys); p.lineTo(r.getRight(), yb);
      g.setColour(Colour(0xff7dff9b)); g.strokePath(p, PathStrokeType(1.6f)); } }
};

class Editor : public AudioProcessorEditor, private APVTS::Listener {
  struct Cell { Label l; std::unique_ptr<Slider> s; std::unique_ptr<ComboBox> c; std::unique_ptr<ToggleButton> t;
    std::unique_ptr<APVTS::SliderAttachment> sa; std::unique_ptr<APVTS::ComboBoxAttachment> ca; std::unique_ptr<APVTS::ButtonAttachment> ta;
    Component* main() { if (s) return s.get(); if (c) return c.get(); return t.get(); } };
  struct Sec { String title; Rectangle<int> area; std::vector<std::unique_ptr<Cell>> cells; };
  static constexpr int CW = 82, CH = 132, CHEEK = 26;
  Proc& pr; Dark lf; std::vector<std::unique_ptr<Sec>> secs; ComboBox pre; TextButton prev{"<"}, next{">"};
  std::vector<const char*> ids; std::atomic<int> last{-1}; std::unique_ptr<ScreenView> screen; Image bg, logo;

  void parameterChanged(const String& id, float) override { for (size_t i = 0; i < ids.size(); ++i) if (id == ids[i]) { last = (int) i; break; } }
  Sec& sec(const String& t) {
    for (auto& s : secs) if (s->title == t) return *s;
    secs.push_back(std::make_unique<Sec>()); secs.back()->title = t; return *secs.back(); }
  Cell& cell(Sec& s, const String& label) {
    s.cells.push_back(std::make_unique<Cell>()); auto& c = *s.cells.back();
    c.l.setText(label, dontSendNotification); c.l.setJustificationType(Justification::centred); addAndMakeVisible(c.l); return c; }
  void extra(const char* grp, const char* id, const char* label, bool toggle) {
    auto& c = cell(sec(grp), label);
    if (toggle) { c.t = std::make_unique<ToggleButton>("Legato"); c.ta = std::make_unique<APVTS::ButtonAttachment>(pr.apvts, id, *c.t); addAndMakeVisible(*c.t); }
    else { c.c = std::make_unique<ComboBox>(); if (auto* pc = dynamic_cast<AudioParameterChoice*>(pr.apvts.getParameter(id))) c.c->addItemList(pc->choices, 1);
      c.ca = std::make_unique<APVTS::ComboBoxAttachment>(pr.apvts, id, *c.c); addAndMakeVisible(*c.c); } }
  void background(Graphics& g, int w, int h) {
    g.setGradientFill(ColourGradient(Colour(0xff2a2c30), 0, 0, Colour(0xff17181a), 0, (float) h, false)); g.fillAll();
    Random r(5);
    for (int i = 0; i < 9000; ++i) { g.setColour(Colours::white.withAlpha(r.nextFloat() * .045f)); g.fillRect(r.nextInt(w), r.nextInt(h), 1, 1); }
    g.setColour(Colour(0x30000000)); g.fillRect(CHEEK, 0, w - 2 * CHEEK, 128);
    g.setColour(Colour(0x40ffffff)); g.drawHorizontalLine(128, (float) CHEEK, (float) (w - CHEEK));
    for (int side = 0; side < 2; ++side) { int x0 = side ? w - CHEEK : 0;
      g.setGradientFill(ColourGradient(Colour(0xff7a4a2a), (float) x0, 0, Colour(0xff3b1f10), (float) (x0 + CHEEK), 0, false)); g.fillRect(x0, 0, CHEEK, h);
      Random wr((juce::int64) (side + 3));
      for (int i = 0; i < 220; ++i) { int xx = x0 + wr.nextInt(CHEEK), y1 = wr.nextInt(h), len = 30 + wr.nextInt(260);
        g.setColour((wr.nextBool() ? Colours::black : Colour(0xffc58a55)).withAlpha(wr.nextFloat() * .18f)); g.drawVerticalLine(xx, (float) y1, (float) (y1 + len)); }
      g.setColour(Colours::black.withAlpha(.6f)); g.drawVerticalLine(side ? x0 : x0 + CHEEK - 1, 0.f, (float) h); } }
public:
  Editor(Proc& p) : AudioProcessorEditor(&p), pr(p) {
    setLookAndFeel(&lf);
    logo = ImageCache::getFromMemory(BinaryData::logo_png, BinaryData::logo_pngSize);
    for (auto& d : kP) ids.push_back(d.id);
    for (auto* x : kX) ids.push_back(x);
    for (size_t i = 0; i < sizeof(kP) / sizeof(kP[0]); ++i) {
      auto& d = kP[i]; auto& c = cell(sec(d.grp), d.name);
      c.s = std::make_unique<Slider>(Slider::RotaryHorizontalVerticalDrag, Slider::TextBoxBelow);
      c.s->setRotaryParameters(MathConstants<float>::pi * 1.2f, MathConstants<float>::pi * 2.8f, true);
      c.s->setTextBoxStyle(Slider::TextBoxBelow, false, CW - 6, 16);
      c.sa = std::make_unique<APVTS::SliderAttachment>(pr.apvts, d.id, *c.s); addAndMakeVisible(*c.s); }
    for (size_t i = 0; i < secs.size(); ++i) for (auto& c : secs[i]->cells) if (c->s) c->s->setColour(Slider::rotarySliderFillColourId, kAcc[i]);
    extra("FILTRE", "fT", "Type", false); extra("FILTRE", "fSl", "Pente", false);
    extra("OSC B granulaire", "bY", "Sync", false);
    extra("LFO 1", "l1S", "Forme", false); extra("LFO 1", "l1Y", "Sync", false);
    extra("LFO 2", "l2S", "Forme", false); extra("LFO 2", "l2Y", "Sync", false);
    extra("GLOBAL", "leg", "Mode", true);
    for (auto* id : ids) pr.apvts.addParameterListener(id, this);
    for (int i = 0; i < pr.getNumPrograms(); ++i) pre.addItem(pr.getProgramName(i), i + 1);
    pre.setSelectedItemIndex(pr.getCurrentProgram(), dontSendNotification);
    pre.onChange = [this] { pr.setCurrentProgram(pre.getSelectedItemIndex()); };
    auto step = [this](int dlt) { int n = pr.getNumPrograms(); pre.setSelectedItemIndex((pre.getSelectedItemIndex() + dlt + n) % n, sendNotificationSync); };
    prev.onClick = [step] { step(-1); }; next.onClick = [step] { step(1); };
    addAndMakeVisible(pre); addAndMakeVisible(prev); addAndMakeVisible(next);
    screen = std::make_unique<ScreenView>(pr, last, ids); addAndMakeVisible(*screen);
    setSize(1280, 700); }
  ~Editor() override { for (auto* id : ids) pr.apvts.removeParameterListener(id, this); setLookAndFeel(nullptr); }
  void paint(Graphics& g) override {
    g.drawImageAt(bg, 0, 0);
    g.drawImageWithin(logo, CHEEK + 8, 14, 96, 96, RectanglePlacement::centred);
    g.setColour(Colour(0xffeceef0)); g.setFont(30.f); g.drawText("ATELIER GRAIN", 144, 30, 240, 36, Justification::left);
    g.setColour(Colour(0xff8f969c)); g.setFont(12.f); g.drawText("additif / soustractif / granulaire", 146, 66, 240, 18, Justification::left);
    for (size_t i = 0; i < secs.size(); ++i) { auto& s = *secs[i]; auto a = s.area;
      g.setColour(Colour(0x1a000000)); g.fillRoundedRectangle(a.toFloat(), 6.f);
      g.setColour(Colour(0xffd5d9dc)); g.setFont(13.f); g.drawText(s.title.toUpperCase(), a.getX() + 10, a.getY() + 3, a.getWidth() - 16, 18, Justification::left);
      g.setColour(kAcc[i]); g.fillRect(a.getX() + 8, a.getY() + 22, a.getWidth() - 16, 2); } }
  void resized() override {
    bg = Image(Image::RGB, getWidth(), getHeight(), true); { Graphics g(bg); background(g, getWidth(), getHeight()); }
    screen->setBounds(392, 12, 560, 104);
    prev.setBounds(972, 44, 30, 28); pre.setBounds(1006, 44, 206, 28); next.setBounds(1216, 44, 30, 28);
    static const int R[3][3] = {{0, 1, -1}, {2, 3, 4}, {5, 6, 7}};
    for (int r = 0; r < 3; ++r) { int x = CHEEK + 14, y = 142 + r * 184;
      for (int k = 0; k < 3; ++k) { int id = R[r][k]; if (id < 0 || id >= (int) secs.size()) continue;
        auto& s = *secs[(size_t) id]; int n = (int) s.cells.size(); s.area = {x, y, n * CW + 12, 28 + CH + 8};
        for (int i = 0; i < n; ++i) { auto& c = *s.cells[(size_t) i]; int bx = x + 6 + i * CW, by = y + 28;
          c.l.setBounds(bx, by, CW, 16);
          if (c.s) c.s->setBounds(bx, by + 16, CW, CH - 16); else c.main()->setBounds(bx + 4, by + 56, CW - 8, 26); }
        x += n * CW + 22; } } }
};

AudioProcessorEditor* Proc::createEditor() { return new Editor(*this); }
AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new Proc(); }
