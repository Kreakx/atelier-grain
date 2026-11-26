// Atelier Grain - synthe additif / soustractif / granulaire (VST3, JUCE 8)
#include <juce_audio_utils/juce_audio_utils.h>
#include <algorithm>
using namespace juce;
using APVTS = AudioProcessorValueTreeState;

namespace K { enum { aLvl, aPart, aTilt, aOdd, aSemi, bLvl, bSize, bDens, bSpr, bSemi, cut, res, fEnv,
  aA, aD, aS, aR, fA, fD, fS, fR, l1R, l1C, l1P, l2R, l2A, l2B, gld, drv, mst, NP }; }

struct PD { const char *id, *grp, *name; float mn, mx, def, sk; };
static const PD kP[] = {
 {"aLvl","OSC A additif","Niveau",0,1,.7f,1},{"aPart","OSC A additif","Partiels",1,32,12,1},
 {"aTilt","OSC A additif","Pente",0,1,.45f,1},{"aOdd","OSC A additif","Impair",0,1,0,1},{"aSemi","OSC A additif","Hauteur",-24,24,0,1},
 {"bLvl","OSC B granulaire","Niveau",0,1,.4f,1},{"bSize","OSC B granulaire","Taille",5,400,80,.5f},
 {"bDens","OSC B granulaire","Densite",1,150,30,.5f},{"bSpr","OSC B granulaire","Disperse",0,1,.3f,1},{"bSemi","OSC B granulaire","Hauteur",-24,24,12,1},
 {"cut","FILTRE","Freq",20,18000,2500,.3f},{"res","FILTRE","Reso",0,1,.2f,1},{"fEnv","FILTRE","Env",-1,1,.4f,1},
 {"aA","ENV AMP","A",.001f,5,.01f,.3f},{"aD","ENV AMP","D",.001f,5,.3f,.3f},{"aS","ENV AMP","S",0,1,.7f,1},{"aR","ENV AMP","R",.001f,8,.4f,.3f},
 {"fA","ENV FILTRE","A",.001f,5,.01f,.3f},{"fD","ENV FILTRE","D",.001f,5,.4f,.3f},{"fS","ENV FILTRE","S",0,1,.3f,1},{"fR","ENV FILTRE","R",.001f,8,.4f,.3f},
 {"l1R","LFO 1","Vitesse",.05f,20,2,.4f},{"l1C","LFO 1","Filtre",0,1,0,1},{"l1P","LFO 1","Hauteur",0,1,0,1},
 {"l2R","LFO 2","Vitesse",.05f,20,.5f,.4f},{"l2A","LFO 2","Volume",0,1,0,1},{"l2B","LFO 2","Grains",0,1,0,1},
 {"gld","GLOBAL","Glide",0,2,0,.4f},{"drv","GLOBAL","Satur.",0,1,.2f,1},{"mst","GLOBAL","Master",0,1,.7f,1}};

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
 {"Acid Lead", {{"aLvl",.8f},{"aPart",24.f},{"aTilt",.2f},{"bLvl",0.f},{"cut",700.f},{"res",.7f},{"fEnv",.8f},{"aA",.003f},{"aD",.25f},{"aS",.6f},{"aR",.1f},{"fD",.2f},{"fS",0.f},{"gld",.06f},{"drv",.55f},{"leg",1.f}}}
};

static APVTS::ParameterLayout layout() {
  APVTS::ParameterLayout L;
  for (auto& p : kP)
    L.add(std::make_unique<AudioParameterFloat>(ParameterID{p.id, 1}, String(p.grp) + " " + p.name, NormalisableRange<float>(p.mn, p.mx, 0.f, p.sk), p.def));
  StringArray sh{"Sinus", "Triangle", "Dent", "Carre", "Hasard"};
  L.add(std::make_unique<AudioParameterChoice>(ParameterID{"l1S", 1}, "LFO 1 forme", sh, 0));
  L.add(std::make_unique<AudioParameterChoice>(ParameterID{"l2S", 1}, "LFO 2 forme", sh, 0));
  L.add(std::make_unique<AudioParameterChoice>(ParameterID{"fT", 1}, "Filtre type", StringArray{"Passe-bas", "Passe-haut"}, 0));
  L.add(std::make_unique<AudioParameterChoice>(ParameterID{"fSl", 1}, "Filtre pente", StringArray{"12 dB", "24 dB"}, 1));
  L.add(std::make_unique<AudioParameterBool>(ParameterID{"leg", 1}, "Legato", false));
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
    int pos = 0, n = b.getNumSamples();
    for (const auto md : m) { int ts = jlimit(0, n, md.samplePosition); render(b, pos, ts); pos = ts; handle(md.getMessage()); }
    render(b, pos, n); }

  APVTS apvts;
private:
  double fs = 44100; Voice vs[8]; std::vector<int> stack; float q[K::NP] {}; int sh1 = 0, sh2 = 0, fT = 0, fSl = 1, cur = 0; bool leg = false;
  float tab[2048] {}, cP = -1, cT = -1, cO = -1, lastF = 261.6f; LFO a1, a2; Random rnd;

  void rebuild(bool force) {
    float P = apvts.getRawParameterValue("aPart")->load(), T = apvts.getRawParameterValue("aTilt")->load(), O = apvts.getRawParameterValue("aOdd")->load();
    if (!force && P == cP && T == cT && O == cO) return;
    cP = P; cT = T; cO = O;
    int N = jlimit(1, 32, (int) (P + .5f)); float s = .2f + T * 2.f, pk = 0;
    for (int i = 0; i < 2048; ++i) {
      float ph = MathConstants<float>::twoPi * (float) i / 2048.f, y = 0;
      for (int k = 1; k <= N; ++k) y += std::pow((float) k, -s) * ((k % 2 == 0) ? 1.f - O : 1.f) * std::sin(ph * (float) k);
      tab[i] = y; pk = jmax(pk, std::abs(y)); }
    if (pk > 0) for (auto& t : tab) t /= pk; }
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
    float gn = 1.f / std::sqrt(jmax(1.f, q[K::bDens] * q[K::bSize] * .001f));
    float gc = q[K::gld] < .002f ? 1.f : 1.f - std::exp(-1.f / (q[K::gld] * F));
    float dr = 1.f + q[K::drv] * 8.f, sn = 1.f / std::sqrt(dr);
    for (int i = s; i < e; ++i) {
      float l1 = lfo(a1, q[K::l1R], sh1), l2 = lfo(a2, q[K::l2R], sh2), pm = std::exp2(l1 * q[K::l1P] * .5f), out = 0;
      for (auto& v : vs) {
        if (!v.on) continue;
        v.age += 1.f; v.freq += (v.tgt - v.freq) * gc;
        v.pA += v.freq * pitA * pm / F; v.pA -= std::floor(v.pA);
        float x = rd(v.pA) * q[K::aLvl];
        v.gAcc += q[K::bDens] / F; if (v.gAcc >= 1.f) { v.gAcc -= 1.f; spawn(v, pitB); }
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

// ---------------- interface : metal brosse, gros boutons ----------------
struct Metal : LookAndFeel_V4 {
  Metal() { setColour(Slider::textBoxTextColourId, Colour(0xff202326)); setColour(Slider::textBoxOutlineColourId, Colours::transparentBlack);
    setColour(Slider::textBoxBackgroundColourId, Colours::transparentBlack); setColour(Label::textColourId, Colour(0xff202326));
    setColour(ComboBox::backgroundColourId, Colour(0xff34373c)); setColour(ComboBox::textColourId, Colours::white); setColour(ComboBox::arrowColourId, Colours::white);
    setColour(ToggleButton::textColourId, Colour(0xff202326)); setColour(ToggleButton::tickColourId, Colour(0xffff7a1a)); }
  void drawRotarySlider(Graphics& g, int x, int y, int w, int h, float pos, float a0, float a1, Slider&) override {
    auto b = Rectangle<float>((float) x, (float) y, (float) w, (float) h).reduced(8.f); float r = jmin(b.getWidth(), b.getHeight()) / 2; auto c = b.getCentre();
    Path arc; arc.addCentredArc(c.x, c.y, r + 4, r + 4, 0, a0, a0 + pos * (a1 - a0), true);
    g.setColour(Colour(0xffff7a1a)); g.strokePath(arc, PathStrokeType(3.f));
    g.setColour(Colours::black.withAlpha(.45f)); g.fillEllipse(b.translated(2, 3));
    g.setGradientFill(ColourGradient(Colour(0xffd8dbde), c.x - r, c.y - r, Colour(0xff4a4e53), c.x + r, c.y + r, false)); g.fillEllipse(b);
    for (int i = 0; i < 40; ++i) { float an = (float) i * MathConstants<float>::twoPi / 40.f; Point<float> d(std::sin(an), -std::cos(an));
      g.setColour(Colour(0x40000000)); g.drawLine(c.x + d.x * (r - 1), c.y + d.y * (r - 1), c.x + d.x * (r - 5), c.y + d.y * (r - 5), 1.f); }
    g.setGradientFill(ColourGradient(Colour(0xffaeb2b6), c.x - r * .5f, c.y - r * .5f, Colour(0xff6a6e73), c.x + r * .5f, c.y + r * .5f, false)); g.fillEllipse(b.reduced(r * .28f));
    g.setColour(Colour(0xff17191c)); g.drawEllipse(b, 1.8f);
    Path p; p.addRoundedRectangle(-2.2f, -r + 3, 4.4f, r * .42f, 2.f); g.setColour(Colour(0xffff7a1a)); g.fillPath(p, AffineTransform::rotation(a0 + pos * (a1 - a0)).translated(c)); }
};

class Editor : public AudioProcessorEditor {
  struct Cell { Label l; std::unique_ptr<Slider> s; std::unique_ptr<ComboBox> c; std::unique_ptr<ToggleButton> t;
    std::unique_ptr<APVTS::SliderAttachment> sa; std::unique_ptr<APVTS::ComboBoxAttachment> ca; std::unique_ptr<APVTS::ButtonAttachment> ta;
    Component* main() { if (s) return s.get(); if (c) return c.get(); return t.get(); } };
  struct Sec { String title; Rectangle<int> area; std::vector<std::unique_ptr<Cell>> cells; };
  static constexpr int CW = 76, CH = 140;
  Proc& pr; Metal lf; std::vector<std::unique_ptr<Sec>> secs; ComboBox pre; Image bg;

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
  void brushed(Graphics& g, int w, int h) {
    g.setGradientFill(ColourGradient(Colour(0xffcdd0d4), 0, 0, Colour(0xff9a9ea3), 0, (float) h, false)); g.fillAll();
    Random r(11);
    for (int y = 0; y < h; ++y) { g.setColour((r.nextBool() ? Colours::white : Colours::black).withAlpha(r.nextFloat() * .10f)); g.drawHorizontalLine(y, 0.f, (float) w); }
    for (int i = 0; i < 900; ++i) { int y = r.nextInt(h), x = r.nextInt(w), l = 20 + r.nextInt(200);
      g.setColour(Colours::white.withAlpha(r.nextFloat() * .09f)); g.drawHorizontalLine(y, (float) x, (float) (x + l)); } }
public:
  Editor(Proc& p) : AudioProcessorEditor(&p), pr(p) {
    setLookAndFeel(&lf);
    for (auto& d : kP) {
      auto& c = cell(sec(d.grp), d.name);
      c.s = std::make_unique<Slider>(Slider::RotaryHorizontalVerticalDrag, Slider::TextBoxBelow);
      c.s->setRotaryParameters(MathConstants<float>::pi * 1.2f, MathConstants<float>::pi * 2.8f, true);
      c.s->setTextBoxStyle(Slider::TextBoxBelow, false, CW - 6, 16);
      c.sa = std::make_unique<APVTS::SliderAttachment>(pr.apvts, d.id, *c.s); addAndMakeVisible(*c.s); }
    extra("FILTRE", "fT", "Type", false); extra("FILTRE", "fSl", "Pente", false);
    extra("LFO 1", "l1S", "Forme", false); extra("LFO 2", "l2S", "Forme", false); extra("GLOBAL", "leg", "Mode", true);
    for (int i = 0; i < pr.getNumPrograms(); ++i) pre.addItem(pr.getProgramName(i), i + 1);
    pre.setSelectedItemIndex(pr.getCurrentProgram(), dontSendNotification);
    pre.onChange = [this] { pr.setCurrentProgram(pre.getSelectedItemIndex()); };
    addAndMakeVisible(pre); setSize(1160, 620); }
  ~Editor() override { setLookAndFeel(nullptr); }
  void paint(Graphics& g) override {
    g.drawImageAt(bg, 0, 0);
    g.setColour(Colour(0xff1d1f22)); g.setFont(30.f); g.drawText("ATELIER GRAIN", 20, 10, 400, 36, Justification::left);
    g.setColour(Colour(0xff3a3d42)); g.setFont(13.f); g.drawText("additif / soustractif / granulaire", 270, 14, 300, 30, Justification::left);
    for (auto& s : secs) { auto a = s->area.toFloat();
      g.setColour(Colour(0x22000000)); g.fillRoundedRectangle(a, 8.f); g.setColour(Colour(0x66ffffff)); g.drawRoundedRectangle(a, 8.f, 1.2f);
      g.setColour(Colour(0xff25282b)); g.setFont(14.f); g.drawText(s->title, s->area.getX() + 10, s->area.getY() + 4, s->area.getWidth() - 16, 18, Justification::left); } }
  void resized() override {
    bg = Image(Image::RGB, getWidth(), getHeight(), true); { Graphics g(bg); brushed(g, getWidth(), getHeight()); }
    pre.setBounds(getWidth() - 270, 14, 250, 28);
    static const int R[3][3] = {{0, 1, 7}, {2, 3, 4}, {5, 6, -1}};
    for (int r = 0; r < 3; ++r) { int x = 14, y = 62 + r * 184;
      for (int k = 0; k < 3; ++k) { int id = R[r][k]; if (id < 0 || id >= (int) secs.size()) continue;
        auto& s = *secs[(size_t) id]; int n = (int) s.cells.size(); s.area = {x, y, n * CW + 12, 26 + CH + 8};
        for (int i = 0; i < n; ++i) { auto& c = *s.cells[(size_t) i]; int bx = x + 6 + i * CW, by = y + 26;
          c.l.setBounds(bx, by, CW, 16);
          if (c.s) c.s->setBounds(bx, by + 16, CW, CH - 16); else c.main()->setBounds(bx + 3, by + 60, CW - 6, 26); }
        x += n * CW + 22; } } }
};

AudioProcessorEditor* Proc::createEditor() { return new Editor(*this); }
AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new Proc(); }
