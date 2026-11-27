// Oktoplus Synth - synthe additif / soustractif / granulaire (VST3, JUCE 8)
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_dsp/juce_dsp.h>
#include <algorithm>
#include <BinaryData.h>
using namespace juce;
using APVTS = AudioProcessorValueTreeState;
static String U(const char* s) { return String::fromUTF8(s); }

namespace K { enum { aLvl, aPart, aTilt, aOdd, aSemi, aUni, aDet,  bLvl, bSize, bDens, bSpr, bSemi,  nLvl,
  cut, res, fEnv, fDrv, fBass, fKey,  aA, aD, aS, aR,  fA, fD, fS, fR, pEnv,  l1R, l1C, l1P,  l2R, l2A, l2B,  gld, ana, mst,
  sDrv, sTone, sMix, sOut,  tDrv, tAsym, tWarm, tMix,  eLF, eLG, eM1F, eM1G, eM1Q, eM2F, eM2G, eM2Q, eHF, eHG, NP }; }

#define GA "OSC A (additif)"
#define GB "OSC B (granulaire)"
#define GN "BRUIT"
#define GF "FILTRE"
#define GV "ENV VOLUME"
#define GE "ENV FILTRE"
#define G1 "LFO 1"
#define G2 "LFO 2"
#define GG "GLOBAL"
#define GS "SATURATION"
#define GL "LAMPE"
#define GQ "ÉGALISEUR"

struct PD { const char *id, *grp, *name; float mn, mx, def, sk; char u; };
static const PD kP[] = {
 {"aLvl",GA,"Niveau",0,1,.7f,1,'p'},{"aPart",GA,"Partiels",1,32,16,1,'c'},{"aTilt",GA,"Pente",0,1,.4f,1,'p'},{"aOdd",GA,"Impair",0,1,0,1,'p'},
 {"aSemi",GA,"Hauteur",-24,24,0,1,'t'},{"aUni",GA,"Unisson",0,1,0,1,'p'},{"aDet",GA,"Désaccord",0,50,10,1,'e'},
 {"bLvl",GB,"Niveau",0,1,0,1,'p'},{"bSize",GB,"Taille",5,400,80,.5f,'m'},{"bDens",GB,"Densité",1,150,30,.5f,'n'},
 {"bSpr",GB,"Dispersion",0,1,.3f,1,'p'},{"bSemi",GB,"Hauteur",-24,24,12,1,'t'},
 {"nLvl",GN,"Niveau",0,1,0,1,'p'},
 {"cut",GF,"Fréquence",20,18000,2500,.3f,'h'},{"res",GF,"Résonance",0,1,.2f,1,'p'},{"fEnv",GF,"Env.",-1,1,.4f,1,'p'},
 {"fDrv",GF,"Drive",0,1,.2f,1,'p'},{"fBass",GF,"Basses",0,1,.4f,1,'p'},{"fKey",GF,"Clavier",0,1,0,1,'p'},
 {"aA",GV,"A",.001f,5,.01f,.3f,'s'},{"aD",GV,"D",.001f,5,.3f,.3f,'s'},{"aS",GV,"S",0,1,.7f,1,'p'},{"aR",GV,"R",.001f,8,.4f,.3f,'s'},
 {"fA",GE,"A",.001f,5,.01f,.3f,'s'},{"fD",GE,"D",.001f,5,.4f,.3f,'s'},{"fS",GE,"S",0,1,.3f,1,'p'},{"fR",GE,"R",.001f,8,.4f,.3f,'s'},
 {"pEnv",GE,"Hauteur",-48,48,0,1,'t'},
 {"l1R",G1,"Vitesse",.05f,20,2,.4f,'h'},{"l1C",G1,"Filtre",0,1,0,1,'p'},{"l1P",G1,"Hauteur",0,1,0,1,'p'},
 {"l2R",G2,"Vitesse",.05f,20,.5f,.4f,'h'},{"l2A",G2,"Volume",0,1,0,1,'p'},{"l2B",G2,"Grains",0,1,0,1,'p'},
 {"gld",GG,"Portamento",0,2,0,.4f,'s'},{"ana",GG,"Analogique",0,1,.3f,1,'p'},{"mst",GG,"Volume",0,1,.7f,1,'p'},
 {"sDrv",GS,"Drive",0,1,.3f,1,'p'},{"sTone",GS,"Tonalité",1000,20000,20000,.3f,'h'},{"sMix",GS,"Mélange",0,1,0,1,'p'},{"sOut",GS,"Sortie",-12,12,0,1,'d'},
 {"tDrv",GL,"Chauffe",0,1,.4f,1,'p'},{"tAsym",GL,"Asymétrie",0,1,.4f,1,'p'},{"tWarm",GL,"Chaleur",0,1,.5f,1,'p'},{"tMix",GL,"Mélange",0,1,.6f,1,'p'},
 {"eLF",GQ,"Grave Hz",40,400,100,.5f,'h'},{"eLG",GQ,"Grave dB",-12,12,0,1,'d'},
 {"eM1F",GQ,"Méd.1 Hz",200,4000,800,.4f,'h'},{"eM1G",GQ,"Méd.1 dB",-12,12,0,1,'d'},{"eM1Q",GQ,"Méd.1 Q",.3f,8,1,.5f,'q'},
 {"eM2F",GQ,"Méd.2 Hz",1000,12000,3500,.4f,'h'},{"eM2G",GQ,"Méd.2 dB",-12,12,0,1,'d'},{"eM2Q",GQ,"Méd.2 Q",.3f,8,1,.5f,'q'},
 {"eHF",GQ,"Aigu Hz",3000,16000,8000,.4f,'h'},{"eHG",GQ,"Aigu dB",-12,12,0,1,'d'}};

static const char* kX[] = {"l1S", "l2S", "fT", "fSl", "leg", "l1Y", "l2Y", "bY", "nTyp", "fCar", "sMode", "tOn", "eOn"};

struct Pre { const char* n; std::vector<std::pair<const char*, float>> v; };
static const std::vector<Pre> kPre = {
 {"Initialisation", {}},
 {"Lead Scie Solo", {{"aLvl",.8f},{"aPart",24.f},{"aTilt",.4f},{"aUni",.6f},{"aDet",12.f},{"cut",3200.f},{"res",.3f},{"fEnv",.4f},{"fDrv",.3f},{"fKey",.5f},{"aA",.005f},{"aD",.2f},{"aS",.8f},{"aR",.25f},{"fA",.005f},{"fD",.3f},{"fS",.5f},{"fR",.3f},{"l1R",5.5f},{"l1P",.05f},{"gld",.06f},{"leg",1.f},{"ana",.4f},{"sMix",.3f},{"sDrv",.3f},{"tOn",1.f},{"tMix",.4f}}},
 {"Lead Carré Creux", {{"aOdd",1.f},{"aPart",20.f},{"aTilt",.35f},{"aUni",.3f},{"aDet",8.f},{"cut",2500.f},{"res",.35f},{"fEnv",.5f},{"fDrv",.25f},{"fKey",.4f},{"aA",.01f},{"aS",.85f},{"aR",.3f},{"gld",.04f},{"leg",1.f},{"l1R",5.f},{"l1P",.04f}}},
 {"Lead Hurlant", {{"aPart",28.f},{"aTilt",.3f},{"aUni",.5f},{"aDet",14.f},{"cut",1800.f},{"res",.75f},{"fEnv",.6f},{"fDrv",.6f},{"fCar",1.f},{"fKey",.4f},{"aS",.8f},{"gld",.05f},{"leg",1.f},{"sDrv",.5f},{"sMix",.5f},{"sMode",1.f}}},
 {"Lead Cristal", {{"aPart",10.f},{"aTilt",.6f},{"aSemi",12.f},{"bLvl",.2f},{"bSize",30.f},{"bDens",40.f},{"cut",6000.f},{"res",.15f},{"fEnv",.2f},{"aA",.01f},{"aD",.5f},{"aS",.6f},{"aR",.6f},{"l1R",4.8f},{"l1P",.03f},{"tOn",1.f},{"tWarm",.3f},{"tMix",.3f}}},
 {"Lead Trance", {{"aUni",1.f},{"aDet",22.f},{"aPart",28.f},{"aTilt",.35f},{"cut",4200.f},{"res",.2f},{"aA",.01f},{"aD",.3f},{"aS",.85f},{"aR",.5f},{"ana",.6f},{"sDrv",.35f},{"sMix",.25f}}},
 {"Pluck Cristal", {{"aLvl",.7f},{"aPart",8.f},{"aTilt",.5f},{"aSemi",12.f},{"bLvl",.2f},{"bSize",30.f},{"bDens",50.f},{"cut",900.f},{"res",.3f},{"fEnv",.9f},{"aA",.002f},{"aD",.35f},{"aS",0.f},{"aR",.3f},{"fD",.3f},{"fS",0.f}}},
 {"Pluck Boisé", {{"aPart",6.f},{"aTilt",.7f},{"aOdd",.3f},{"cut",1200.f},{"res",.2f},{"fEnv",.8f},{"fD",.25f},{"fS",0.f},{"fKey",.6f},{"aA",.002f},{"aD",.4f},{"aS",0.f},{"aR",.3f}}},
 {"Pluck Cordes", {{"aPart",18.f},{"aTilt",.5f},{"aUni",.4f},{"aDet",10.f},{"cut",900.f},{"res",.25f},{"fEnv",.9f},{"fD",.35f},{"fS",.05f},{"aD",.6f},{"aS",0.f},{"aR",.5f},{"tOn",1.f},{"tMix",.5f}}},
 {"Pluck Électro", {{"aPart",14.f},{"aTilt",.4f},{"cut",600.f},{"res",.55f},{"fEnv",1.f},{"fD",.18f},{"fS",0.f},{"aD",.22f},{"aS",0.f},{"aR",.15f},{"fDrv",.5f},{"fCar",1.f},{"sDrv",.4f},{"sMix",.3f}}},
 {"Pluck Bulle", {{"aPart",1.f},{"pEnv",12.f},{"fD",.08f},{"fS",0.f},{"aD",.25f},{"aS",0.f},{"aR",.2f},{"cut",8000.f},{"fEnv",0.f}}},
 {"Basse Sub", {{"aPart",1.f},{"aLvl",.9f},{"cut",18000.f},{"fEnv",0.f},{"aA",.005f},{"aD",.3f},{"aS",.9f},{"aR",.15f},{"sDrv",.3f},{"sMix",.3f}}},
 {"Basse Échelle", {{"aLvl",.8f},{"aPart",20.f},{"aTilt",.25f},{"cut",500.f},{"res",.35f},{"fEnv",.5f},{"aA",.005f},{"aD",.2f},{"aS",.8f},{"aR",.15f},{"fD",.25f},{"fS",.1f},{"gld",.08f},{"fDrv",.4f},{"fBass",.5f},{"leg",1.f},{"tOn",1.f},{"tDrv",.5f}}},
 {"Basse Acide", {{"aPart",28.f},{"aTilt",.25f},{"cut",450.f},{"res",.75f},{"fEnv",.9f},{"fD",.22f},{"fS",0.f},{"fDrv",.6f},{"fCar",1.f},{"aA",.003f},{"aD",.3f},{"aS",.6f},{"aR",.1f},{"gld",.06f},{"leg",1.f},{"sDrv",.5f},{"sMix",.4f},{"sMode",1.f}}},
 {"Basse Wobble", {{"aPart",16.f},{"aTilt",.3f},{"aUni",.3f},{"aDet",10.f},{"cut",700.f},{"res",.45f},{"fEnv",.2f},{"l1Y",7.f},{"l1C",.8f},{"aS",1.f},{"aR",.15f},{"fDrv",.4f}}},
 {"Basse Growl", {{"aPart",30.f},{"aTilt",.2f},{"cut",300.f},{"res",.6f},{"fEnv",1.f},{"fD",.4f},{"aS",1.f},{"fCar",1.f},{"fDrv",.7f},{"sMode",2.f},{"sDrv",.5f},{"sMix",.4f},{"nLvl",.05f}}},
 {"Kick Profond", {{"aPart",1.f},{"aLvl",1.f},{"pEnv",36.f},{"fA",.001f},{"fD",.09f},{"fS",0.f},{"fR",.05f},{"aA",.001f},{"aD",.45f},{"aS",0.f},{"aR",.1f},{"cut",18000.f},{"res",0.f},{"fEnv",0.f},{"nLvl",.08f},{"sDrv",.5f},{"sMix",.5f}}},
 {"Kick Techno", {{"aPart",3.f},{"aTilt",.6f},{"aLvl",1.f},{"pEnv",30.f},{"fA",.001f},{"fD",.06f},{"fS",0.f},{"aA",.001f},{"aD",.3f},{"aS",0.f},{"aR",.08f},{"nLvl",.12f},{"cut",5000.f},{"res",.1f},{"fEnv",0.f},{"fDrv",.5f},{"sDrv",.7f},{"sMix",.5f},{"sMode",2.f}}},
 {"Caisse Claire", {{"aPart",3.f},{"aLvl",.5f},{"pEnv",10.f},{"fA",.001f},{"fD",.05f},{"fS",0.f},{"nLvl",.8f},{"cut",3000.f},{"fT",2.f},{"res",.2f},{"fEnv",0.f},{"aA",.001f},{"aD",.18f},{"aS",0.f},{"aR",.1f},{"sDrv",.4f},{"sMix",.3f}}},
 {"Charleston Fermé", {{"aLvl",0.f},{"nLvl",1.f},{"fT",1.f},{"cut",7500.f},{"res",.2f},{"fEnv",0.f},{"aA",.001f},{"aD",.05f},{"aS",0.f},{"aR",.03f}}},
 {"Charleston Ouvert", {{"aLvl",0.f},{"nLvl",1.f},{"fT",1.f},{"cut",6500.f},{"res",.2f},{"fEnv",0.f},{"aA",.001f},{"aD",.35f},{"aS",0.f},{"aR",.2f}}},
 {"Clap", {{"aLvl",0.f},{"nLvl",1.f},{"fT",2.f},{"cut",1600.f},{"res",.35f},{"fEnv",0.f},{"aA",.001f},{"aD",.2f},{"aS",0.f},{"aR",.15f},{"sDrv",.4f},{"sMix",.3f}}},
 {"Tom", {{"aPart",1.f},{"aLvl",1.f},{"pEnv",14.f},{"fA",.001f},{"fD",.12f},{"fS",0.f},{"aA",.001f},{"aD",.5f},{"aS",0.f},{"aR",.2f},{"nLvl",.05f},{"cut",6000.f},{"fEnv",0.f}}},
 {"Zap Laser", {{"aPart",6.f},{"aTilt",.4f},{"pEnv",48.f},{"fA",.001f},{"fD",.25f},{"fS",0.f},{"aA",.001f},{"aD",.35f},{"aS",0.f},{"aR",.2f},{"cut",9000.f},{"res",.3f},{"fEnv",0.f},{"sDrv",.3f},{"sMix",.3f}}},
 {"Pad Chaleureux", {{"aLvl",.6f},{"aPart",10.f},{"aTilt",.7f},{"aUni",.5f},{"aDet",14.f},{"bLvl",.25f},{"bSize",200.f},{"bDens",20.f},{"cut",1800.f},{"res",.15f},{"fEnv",.3f},{"aA",1.2f},{"aD",1.f},{"aS",.8f},{"aR",2.f},{"fA",1.5f},{"fS",.6f},{"l1R",.3f},{"l1C",.25f},{"ana",.6f},{"tOn",1.f},{"tWarm",.6f},{"tMix",.5f}}},
 {"Nuage de Verre", {{"aLvl",.15f},{"aPart",6.f},{"aTilt",.3f},{"bLvl",.8f},{"bSize",60.f},{"bDens",70.f},{"bSpr",.8f},{"bSemi",12.f},{"cut",7000.f},{"res",.1f},{"aA",.6f},{"aS",1.f},{"aR",2.5f},{"l2R",.2f},{"l2B",.5f}}},
 {"Poussière", {{"aLvl",0.f},{"bLvl",.7f},{"bSize",15.f},{"bDens",120.f},{"bSpr",1.f},{"bSemi",12.f},{"nLvl",.5f},{"nTyp",1.f},{"cut",4500.f},{"fT",1.f},{"res",.25f},{"fEnv",0.f},{"aA",.3f},{"aS",1.f},{"aR",1.f},{"l2R",.7f},{"l2B",.6f},{"sDrv",.4f},{"sMix",.4f}}},
 {"Orgue Additif", {{"aLvl",.8f},{"aPart",16.f},{"aTilt",.4f},{"aOdd",1.f},{"cut",12000.f},{"res",0.f},{"fEnv",0.f},{"aA",.01f},{"aS",1.f},{"aR",.12f},{"l1R",5.f},{"l1P",.04f}}},
 {"Gate 1/8", {{"aLvl",.8f},{"aPart",14.f},{"aTilt",.4f},{"cut",3500.f},{"l2Y",10.f},{"l2S",3.f},{"l2A",.9f},{"aA",.01f},{"aS",1.f},{"aR",.2f}}}
};

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
    L.add(std::make_unique<AudioParameterFloat>(ParameterID{p.id, 1}, U(p.grp) + " " + U(p.name), NormalisableRange<float>(p.mn, p.mx, 0.f, p.sk), p.def,
      AudioParameterFloatAttributes().withStringFromValueFunction([u = p.u](float v, int) { return fmt(u, v); })));
  StringArray sh{"Sinus", "Triangle", "Dent", U("Carré")}; sh.add("Hasard");
  auto ch = [&](const char* id, const String& nm, StringArray a, int d) { L.add(std::make_unique<AudioParameterChoice>(ParameterID{id, 1}, nm, a, d)); };
  ch("l1S", "LFO 1 forme", sh, 0); ch("l2S", "LFO 2 forme", sh, 0);
  ch("fT", "Filtre mode", StringArray{"P-bas", "P-haut", "P-bande", "C-bande"}, 0);
  ch("fSl", "Filtre pente", StringArray{"12 dB", "24 dB"}, 1);
  ch("fCar", U("Filtre caractère"), StringArray{U("Échelle"), "Hurlant"}, 0);
  ch("nTyp", "Bruit couleur", StringArray{"Blanc", "Rose"}, 0);
  ch("sMode", "Saturation mode", StringArray{"Douce", "Asym.", U("Écrêtage"), "Repli"}, 0);
  ch("l1Y", "LFO 1 synchro", syncNames(), 0); ch("l2Y", "LFO 2 synchro", syncNames(), 0); ch("bY", "Grains synchro", syncNames(), 0);
  L.add(std::make_unique<AudioParameterBool>(ParameterID{"leg", 1}, "Legato", false));
  L.add(std::make_unique<AudioParameterBool>(ParameterID{"tOn", 1}, "Lampe actif", false));
  L.add(std::make_unique<AudioParameterBool>(ParameterID{"eOn", 1}, U("Égaliseur actif"), false));
  return L;
}

// <LADDER>
static inline float ft(float x) { x = jlimit(-3.f, 3.f, x); float x2 = x * x; return x * (27.f + x2) / (27.f + 9.f * x2); }
static inline float fa(float x) { return x >= 0.f ? ft(x) : ft(x * 2.2f) * .4545f; }
// filtre en echelle 4 poles, integration ZDF (trapezes), saturation entre chaque pole et dans la boucle de retour
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
    return jlimit(-8.f, 8.f, c[0] * x0 + c[1] * y0 + c[2] * y1 + c[3] * y2 + c[4] * y3); } };
// </LADDER>
static const float kMix[4][2][5] = {
  {{0, 0, 1, 0, 0}, {0, 0, 0, 0, 1}},       // passe-bas 12 / 24
  {{1, -2, 1, 0, 0}, {1, -4, 6, -4, 1}},    // passe-haut
  {{0, 2, -2, 0, 0}, {0, 0, 4, -8, 4}},     // passe-bande
  {{1, -2, 2, 0, 0}, {1, -4, 6, -4, 2}}};   // coupe-bande

struct Grain { bool on = false; float ph = 0, inc = 0, len = 1, age = 0; const float* tb = nullptr; };
struct Voice { bool on = false, gate = false; int note = -1;
  float freq = 440, tgt = 440, pA = 0, pA2 = 0, pA3 = 0, gAcc = 0, vel = 1, age = 0, dr1 = 0, dr2 = 0, k0 = 0, k1 = 0, k2 = 0;
  ADSR ea, ef; Ladder lad; Grain g[24]; };
struct LFO { float ph = 0, sh = 0; };
struct Fx { float lp = 0, d1x = 0, d1y = 0, d2x = 0, d2y = 0, tl = 0, th = 0; };

class Proc : public AudioProcessor {
public:
  Proc() : AudioProcessor(BusesProperties().withOutput("Out", AudioChannelSet::stereo(), true)), apvts(*this, nullptr, "ST", layout()) {
    for (int i = 0; i < 2048; ++i) sinT[i] = std::sin(MathConstants<float>::twoPi * (float) i / 2048.f);
    apvts.state.setProperty("presetName", "Initialisation", nullptr); }
  const String getName() const override { return "Oktoplus Synth"; }
  void prepareToPlay(double sr, int blk) override {
    fs = sr; maxB = jmax(blk, 64);
    sIn.assign((size_t) maxB, 0.f); sG = sIn; sFc = sIn; sAe = sIn; sL1 = sIn; sL2 = sIn;
    for (auto& o : os) { o = std::make_unique<dsp::Oversampling<float>>(1, 1, dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, false); o->initProcessing((size_t) maxB); }
    osM = std::make_unique<dsp::Oversampling<float>>(2, 1, dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, false); osM->initProcessing((size_t) maxB);
    for (auto& v : vs) { v = Voice(); v.ea.setSampleRate(sr); v.ef.setSampleRate(sr); }
    stack.clear(); lastF = 261.6f; cf[0] = cf[1] = Fx(); eqLast[0] = -999.f;
    for (auto& c : eq) for (auto& f : c) f.reset();
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
  const String getProgramName(int i) override { return U(kPre[(size_t) i].n); }
  void changeProgramName(int, const String&) override {}
  void setCurrentProgram(int i) override {
    cur = i;
    for (auto* p : getParameters()) p->setValueNotifyingHost(p->getDefaultValue());
    for (auto& kv : kPre[(size_t) i].v) { auto* p = apvts.getParameter(kv.first); if (p) p->setValueNotifyingHost(p->convertTo0to1(kv.second)); }
    apvts.state.setProperty("presetName", U(kPre[(size_t) i].n), nullptr); }
  String currentName() { return apvts.state.getProperty("presetName", "Initialisation").toString(); }
  static File presetDir() { auto d = File::getSpecialLocation(File::userApplicationDataDirectory).getChildFile("Oktoplus").getChildFile("Presets Oktoplus Synth"); d.createDirectory(); return d; }
  bool saveUser(const String& name) {
    apvts.state.setProperty("presetName", name, nullptr);
    auto f = presetDir().getChildFile(File::createLegalFileName(name) + ".xml");
    if (auto x = apvts.copyState().createXml()) return x->writeTo(f);
    return false; }
  void loadUser(const File& f) { if (auto x = parseXML(f)) { apvts.replaceState(ValueTree::fromXml(*x)); apvts.state.setProperty("presetName", f.getFileNameWithoutExtension(), nullptr); } }
  void getStateInformation(MemoryBlock& b) override { if (auto x = apvts.copyState().createXml()) copyXmlToBinary(*x, b); }
  void setStateInformation(const void* d, int n) override { if (auto x = getXmlFromBinary(d, n)) apvts.replaceState(ValueTree::fromXml(*x)); }

  void processBlock(AudioBuffer<float>& b, MidiBuffer& m) override {
    ScopedNoDenormals nd; b.clear(); rebuild(false);
    for (int i = 0; i < K::NP; ++i) q[i] = apvts.getRawParameterValue(kP[i].id)->load();
    auto gi = [&](const char* id) { return (int) apvts.getRawParameterValue(id)->load(); };
    sh1 = gi("l1S"); sh2 = gi("l2S"); fTy = gi("fT"); fSl = gi("fSl"); fCar = gi("fCar"); nTy = gi("nTyp"); sMd = gi("sMode");
    leg = gi("leg") > 0; tOnB = gi("tOn") > 0; eqOn = gi("eOn") > 0;
    double bpm = 120.0; if (auto* ph = getPlayHead()) if (auto info = ph->getPosition()) if (auto bp = info->getBpm()) bpm = *bp;
    rt1 = rateOf(gi("l1Y"), q[K::l1R], bpm); rt2 = rateOf(gi("l2Y"), q[K::l2R], bpm); dens = rateOf(gi("bY"), q[K::bDens], bpm);
    for (auto& v : vs) { v.ea.setParameters({q[K::aA], q[K::aD], q[K::aS], q[K::aR]}); v.ef.setParameters({q[K::fA], q[K::fD], q[K::fS], q[K::fR]}); }
    prepMaster(); eqUpdate();
    int pos = 0, n = b.getNumSamples();
    for (const auto md : m) { int ts = jlimit(0, n, md.samplePosition); render(b, pos, ts); pos = ts; handle(md.getMessage()); }
    render(b, pos, n); master(b);
    int cnt = 0; for (auto& v : vs) cnt += v.on ? 1 : 0; nVoices = cnt; }

  APVTS apvts; std::atomic<int> nVoices{0};
private:
  double fs = 44100; int maxB = 512; Voice vs[8]; std::unique_ptr<dsp::Oversampling<float>> os[8], osM;
  std::vector<int> stack; std::vector<float> sIn, sG, sFc, sAe, sL1, sL2; float q[K::NP] {};
  int sh1 = 0, sh2 = 0, fTy = 0, fSl = 1, fCar = 0, nTy = 0, sMd = 0, cur = 0; bool leg = false, tOnB = false, eqOn = false, sOn = false;
  float rt1 = 2, rt2 = .5f, dens = 30, cP = -1, cT = -1, cO = -1, lastF = 261.6f, sinT[2048] {}, tabs[8][2048] {};
  float sd = 1, sOc = 1, aTone = 1, td = 1, tb = 0, tNorm = 1, aLow = .01f, aHf = .5f, eqLast[10] {};
  Fx cf[2]; dsp::IIR::Filter<float> eq[2][4]; LFO a1, a2; Random rnd;

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
    float uni = q[K::aUni], dm = std::exp2(q[K::aDet] / 1200.f), ana = q[K::ana], nl = q[K::nLvl];
    float kk = q[K::res] * (fCar ? 4.3f : 4.1f), drv = 1.f + q[K::fDrv] * 9.f, comp = (1.f + q[K::fBass] * kk) * drv, oc = 1.f / std::sqrt(drv);
    const float* mix = kMix[jlimit(0, 3, fTy)][fSl > 0 ? 1 : 0]; const float fcMax = .42f * F;
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
        if (nl > .001f) { float w = rnd.nextFloat() * 2.f - 1.f, nz;
          if (nTy) { v.k0 = .99765f * v.k0 + w * .099046f; v.k1 = .963f * v.k1 + w * .2965164f; v.k2 = .57f * v.k2 + w * 1.0526913f; nz = (v.k0 + v.k1 + v.k2 + w * .1848f) * .22f; }
          else nz = w * .5f;
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

  // ---------- sortie : saturation, lampe, egaliseur, volume ----------
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
    if (tOnB) { float t = (std::tanh(x * td + tb) - std::tanh(tb)) * tNorm;
      float o = t - s.d2x + .9975f * s.d2y; s.d2x = t; s.d2y = o; t = o;
      s.tl += aLow * (t - s.tl); t += q[K::tWarm] * 1.5f * s.tl; s.th += aHf * (t - s.th);
      x += (s.th - x) * q[K::tMix]; }
    return x; }
  void eqUpdate() {
    bool ch = false; for (int i = 0; i < 10; ++i) if (q[K::eLF + i] != eqLast[i]) ch = true;
    if (!ch) return;
    for (int i = 0; i < 10; ++i) eqLast[i] = q[K::eLF + i];
    using C = dsp::IIR::Coefficients<float>; auto db = [](float g) { return Decibels::decibelsToGain(g); };
    auto c0 = C::makeLowShelf(fs, q[K::eLF], .707f, db(q[K::eLG])), c1 = C::makePeakFilter(fs, q[K::eM1F], q[K::eM1Q], db(q[K::eM1G])),
         c2 = C::makePeakFilter(fs, q[K::eM2F], q[K::eM2Q], db(q[K::eM2G])), c3 = C::makeHighShelf(fs, q[K::eHF], .707f, db(q[K::eHG]));
    for (auto& c : eq) { c[0].coefficients = c0; c[1].coefficients = c1; c[2].coefficients = c2; c[3].coefficients = c3; } }
  void master(AudioBuffer<float>& b) {
    int n = b.getNumSamples(); dsp::AudioBlock<float> all(b);
    for (int pos = 0; pos < n; pos += maxB) {
      auto sub = all.getSubBlock((size_t) pos, (size_t) jmin(maxB, n - pos));
      auto up = osM->processSamplesUp(sub);
      for (size_t c = 0; c < 2; ++c) { float* d = up.getChannelPointer(c); for (size_t j = 0; j < up.getNumSamples(); ++j) d[j] = fx((int) c, d[j]); }
      osM->processSamplesDown(sub); }
    if (eqOn) for (int c = 0; c < 2; ++c) { float* d = b.getWritePointer(c); for (int i = 0; i < n; ++i) { float x = d[i]; for (auto& f : eq[c]) x = f.processSample(x); d[i] = x; } }
    b.applyGain(q[K::mst]); }
};

// ---------------- interface : chassis metal, bois verni, potentiometres 3D ----------------
static const Colour kBlue(0xff58b4ff);

static void screw(Graphics& g, float cx, float cy, float r, float rot) {
  g.setColour(Colours::black.withAlpha(.5f)); g.fillEllipse(cx - r, cy - r + 1.6f, r * 2, r * 2);
  g.setGradientFill(ColourGradient(Colour(0xffd0d3d6), cx - r * .6f, cy - r * .7f, Colour(0xff55595e), cx + r * .7f, cy + r * .8f, false));
  g.fillEllipse(cx - r, cy - r, r * 2, r * 2);
  g.setColour(Colour(0xff17181a)); g.drawEllipse(cx - r, cy - r, r * 2, r * 2, 1.f);
  Path p; p.addRectangle(-r * .72f, -1.1f, r * 1.44f, 2.2f); p.addRectangle(-1.1f, -r * .72f, 2.2f, r * 1.44f);
  auto t = AffineTransform::rotation(rot).translated(cx, cy);
  g.setColour(Colour(0x66ffffff)); g.fillPath(p, t.translated(.6f, .9f));
  g.setColour(Colour(0xff1b1c1e)); g.fillPath(p, t); }

static void led(Graphics& g, float cx, float cy, Colour on, bool lit) {
  if (lit) { g.setGradientFill(ColourGradient(on.withAlpha(.55f), cx, cy, on.withAlpha(0.f), cx + 13.f, cy, true)); g.fillEllipse(cx - 13, cy - 13, 26, 26); }
  g.setColour(Colours::black); g.fillEllipse(cx - 5.5f, cy - 5.5f, 11.f, 11.f);
  g.setGradientFill(ColourGradient(lit ? on.brighter(.8f) : on.withMultipliedBrightness(.25f), cx - 1.5f, cy - 2.f, lit ? on : on.withMultipliedBrightness(.1f), cx + 3.f, cy + 4.f, true));
  g.fillEllipse(cx - 4.f, cy - 4.f, 8.f, 8.f);
  g.setColour(Colours::white.withAlpha(.55f)); g.fillEllipse(cx - 2.2f, cy - 3.2f, 2.6f, 1.8f); }

struct Dark : LookAndFeel_V4 {
  Dark() {
    setColour(Slider::textBoxTextColourId, Colour(0xffb8c0c8)); setColour(Slider::textBoxOutlineColourId, Colours::transparentBlack);
    setColour(Slider::textBoxBackgroundColourId, Colours::transparentBlack); setColour(Label::textColourId, Colour(0xffe9ecef));
    setColour(ComboBox::textColourId, Colour(0xffe6e9eb)); setColour(ComboBox::arrowColourId, Colour(0xffe6e9eb));
    setColour(PopupMenu::backgroundColourId, Colour(0xff1f2124)); setColour(PopupMenu::textColourId, Colour(0xffe6e9eb));
    setColour(PopupMenu::headerTextColourId, Colour(0xff8fb8d8));
    setColour(PopupMenu::highlightedBackgroundColourId, Colour(0xff3a8fd8)); setColour(PopupMenu::highlightedTextColourId, Colours::white);
    setColour(TextButton::buttonColourId, Colour(0xff2a2d31)); setColour(TextButton::textColourOffId, Colour(0xffe6e9eb)); }

  void drawRotarySlider(Graphics& g, int x, int y, int w, int h, float pos, float a0, float a1, Slider& s) override {
    float d = (float) jmin(w, h) - 20.f, r = d * .5f; Point<float> c((float) x + (float) w * .5f, (float) y + (float) h * .5f);
    auto acc = s.findColour(Slider::rotarySliderFillColourId);
    for (int i = 0; i <= 20; ++i) { float t = (float) i / 20.f, an = a0 + t * (a1 - a0);
      g.setColour(t <= pos + .001f ? acc : Colour(0x44ffffff));
      g.fillEllipse(c.x + std::sin(an) * (r + 7.f) - 1.4f, c.y - std::cos(an) * (r + 7.f) - 1.4f, 2.8f, 2.8f); }
    for (int i = 6; i >= 1; --i) { float e = (float) i * 1.4f; g.setColour(Colours::black.withAlpha(.11f));
      g.fillEllipse(c.x - r - e, c.y - r + 4.f - e * .5f, d + 2 * e, d + 2 * e); }
    g.setGradientFill(ColourGradient(Colour(0xff4c4e52), c.x, c.y - r, Colour(0xff060607), c.x, c.y + r, false));
    g.fillEllipse(c.x - r, c.y - r, d, d);
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
    g.drawText(b.getButtonText(), (int) a.getX() + 28, (int) a.getY(), (int) a.getWidth() - 28, (int) a.getHeight(), Justification::left); }
};

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
          auto st = [&](float R) { float den = std::sqrt((1 - rr * rr) * (1 - rr * rr) + 4 * R * R * rr * rr);
            return (ty == 0 ? 1.f : ty == 1 ? rr * rr : ty == 2 ? 2 * R * rr : std::abs(1 - rr * rr)) / den; };
          float m = st(R1); if (sl) m *= st(.7f);
          float db = jlimit(-48.f, 24.f, 20.f * std::log10(jmax(m, 1e-4f)));
          float x = r.getX() + r.getWidth() * (float) i / 95.f, y = jmap(db, -48.f, 24.f, r.getBottom() - 2.f, r.getY() + 2.f);
          if (i) p.lineTo(x, y); else p.startNewSubPath(x, y); }
        g.setColour(amber); g.strokePath(p, PathStrokeType(1.6f)); }
      { auto r = Rectangle<float>(460.f, top, w, hh); frame(g, r, "ENV. VOLUME");
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
      for (int i = 0; i < 10; ++i) { g.setColour(Colours::black.withAlpha(.5f * (1.f - (float) i / 10.f)));
        g.drawRoundedRectangle(in.reduced((float) i * .6f), 5.f, 1.f); } }
    g.setColour(Colour(0xff000000)); g.drawRoundedRectangle(in, 5.f, 1.2f); }
};

class EqView : public Component {
  Proc& pr;
public:
  EqView(Proc& p) : pr(p) {}
  void paint(Graphics& g) override {
    auto b = getLocalBounds().toFloat();
    g.setGradientFill(ColourGradient(Colour(0xff0a1a20), 0, 0, Colour(0xff03090c), 0, b.getHeight(), false)); g.fillRoundedRectangle(b, 6.f);
    g.setColour(Colour(0xff17414f)); for (int i = 1; i < 6; ++i) g.drawHorizontalLine((int) (b.getHeight() * (float) i / 6.f), 2.f, b.getWidth() - 2.f);
    for (float fr : {100.f, 1000.f, 10000.f}) g.drawVerticalLine((int) (b.getWidth() * std::log10(fr / 20.f) / 3.f), 2.f, b.getHeight() - 2.f);
    auto P = [&](const char* id) { return pr.apvts.getRawParameterValue(id)->load(); };
    using C = dsp::IIR::Coefficients<float>; auto db = [](float v) { return Decibels::decibelsToGain(v); };
    auto c0 = C::makeLowShelf(48000.0, P("eLF"), .707f, db(P("eLG"))), c1 = C::makePeakFilter(48000.0, P("eM1F"), P("eM1Q"), db(P("eM1G"))),
         c2 = C::makePeakFilter(48000.0, P("eM2F"), P("eM2Q"), db(P("eM2G"))), c3 = C::makeHighShelf(48000.0, P("eHF"), .707f, db(P("eHG")));
    Path p;
    for (int i = 0; i < 160; ++i) { double f = 20.0 * std::pow(1000.0, (double) i / 159.0);
      double m = c0->getMagnitudeForFrequency(f, 48000.0) * c1->getMagnitudeForFrequency(f, 48000.0) * c2->getMagnitudeForFrequency(f, 48000.0) * c3->getMagnitudeForFrequency(f, 48000.0);
      float d = jlimit(-15.f, 15.f, (float) (20.0 * std::log10(jmax(m, 1e-4)))), x = b.getWidth() * (float) i / 159.f, y = jmap(d, -15.f, 15.f, b.getBottom() - 4.f, b.getY() + 4.f);
      if (i) p.lineTo(x, y); else p.startNewSubPath(x, y); }
    bool on = P("eOn") > .5f; g.setColour(Colour(0xffffb347).withAlpha(on ? 1.f : .35f)); g.strokePath(p, PathStrokeType(1.8f));
    g.setColour(Colour(0xff6fb4c8)); g.setFont(11.f); g.drawText(U("COURBE ÉGALISEUR"), 8, 4, 200, 14, Justification::left); }
};

struct SaveCb : ModalComponentManager::Callback { std::function<void(int)> f; void modalStateFinished(int r) override { if (f) f(r); } };

class Editor : public AudioProcessorEditor, private APVTS::Listener, private Timer {
  struct Cell { Label l; std::unique_ptr<Slider> s; std::unique_ptr<ComboBox> c; std::unique_ptr<ToggleButton> t;
    std::unique_ptr<APVTS::SliderAttachment> sa; std::unique_ptr<APVTS::ComboBoxAttachment> ca; std::unique_ptr<APVTS::ButtonAttachment> ta;
    Component* main() { if (s) return s.get(); if (c) return c.get(); return t.get(); } };
  struct Sec { String title; Rectangle<int> area; std::vector<std::unique_ptr<Cell>> cells; };
  struct Entry { String name; int factory; File file; };
  static constexpr int CW = 78, CH = 112, CHEEK = 26;
  Proc& pr; Dark lf; std::vector<std::unique_ptr<Sec>> secs; std::vector<Entry> entries; ComboBox pre; TextButton prev{"<"}, next{">"}, save{"Sauver"};
  std::vector<const char*> ids; std::atomic<int> last{-1}; std::unique_ptr<ScreenView> screen; std::unique_ptr<EqView> eqv; Image bg, logo; bool voiceLit = false;

  void timerCallback() override { bool l = pr.nVoices.load() > 0; if (l != voiceLit) { voiceLit = l; repaint(180, 84, 160, 30); } if (eqv) eqv->repaint(); }
  void parameterChanged(const String& id, float) override { for (size_t i = 0; i < ids.size(); ++i) if (id == ids[i]) { last = (int) i; break; } }
  Sec& sec(const String& t) {
    for (auto& s : secs) if (s->title == t) return *s;
    secs.push_back(std::make_unique<Sec>()); secs.back()->title = t; return *secs.back(); }
  Cell& cell(Sec& s, const String& label) {
    s.cells.push_back(std::make_unique<Cell>()); auto& c = *s.cells.back();
    c.l.setText(label, dontSendNotification); c.l.setJustificationType(Justification::centred); addAndMakeVisible(c.l); return c; }
  void extra(const char* grp, const char* id, const char* label, const char* toggleText) {
    auto& c = cell(sec(U(grp)), U(label));
    if (toggleText) { c.t = std::make_unique<ToggleButton>(U(toggleText)); c.ta = std::make_unique<APVTS::ButtonAttachment>(pr.apvts, id, *c.t); addAndMakeVisible(*c.t); }
    else { c.c = std::make_unique<ComboBox>(); if (auto* pc = dynamic_cast<AudioParameterChoice*>(pr.apvts.getParameter(id))) c.c->addItemList(pc->choices, 1);
      c.ca = std::make_unique<APVTS::ComboBoxAttachment>(pr.apvts, id, *c.c); addAndMakeVisible(*c.c); } }
  void refreshPresets() {
    entries.clear(); pre.clear(dontSendNotification);
    for (int i = 0; i < pr.getNumPrograms(); ++i) entries.push_back({pr.getProgramName(i), i, File()});
    auto files = Proc::presetDir().findChildFiles(File::findFiles, false, "*.xml");
    std::sort(files.begin(), files.end(), [](const File& a, const File& b) { return a.getFileName() < b.getFileName(); });
    for (auto& f : files) entries.push_back({f.getFileNameWithoutExtension(), -1, f});
    int nf = pr.getNumPrograms();
    for (size_t i = 0; i < entries.size(); ++i) { if ((int) i == nf) { pre.addSeparator(); pre.addSectionHeading("Mes presets"); } pre.addItem(entries[i].name, (int) i + 1); }
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
    for (int i = 0; i < 1500; ++i) { int y = r.nextInt(h), x = CHEEK + r.nextInt(cw), len = 40 + r.nextInt(300); g.setColour(Colours::white.withAlpha(r.nextFloat() * .05f)); g.drawHorizontalLine(y, (float) x, (float) (x + len)); }
    for (int i = 0; i < 40000; ++i) { g.setColour((r.nextBool() ? Colours::white : Colours::black).withAlpha(r.nextFloat() * .05f)); g.fillRect(CHEEK + r.nextInt(cw), r.nextInt(h), 1, 1); }
    g.setGradientFill(ColourGradient(Colour(0x22ffffff), 0, 0, Colour(0x00ffffff), 0, (float) h * .45f, false)); g.fillRect(CHEEK, 0, cw, h);
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
  Editor(Proc& p) : AudioProcessorEditor(&p), pr(p) {
    setLookAndFeel(&lf);
    logo = ImageCache::getFromMemory(BinaryData::logo_png, BinaryData::logo_pngSize);
    for (auto& d : kP) ids.push_back(d.id);
    for (auto* x : kX) ids.push_back(x);
    for (auto& d : kP) {
      auto& c = cell(sec(U(d.grp)), U(d.name));
      c.s = std::make_unique<Slider>(Slider::RotaryHorizontalVerticalDrag, Slider::TextBoxBelow);
      c.s->setRotaryParameters(MathConstants<float>::pi * 1.2f, MathConstants<float>::pi * 2.8f, true);
      c.s->setTextBoxStyle(Slider::TextBoxBelow, false, CW - 6, 16); c.s->setColour(Slider::rotarySliderFillColourId, kBlue);
      c.sa = std::make_unique<APVTS::SliderAttachment>(pr.apvts, d.id, *c.s); addAndMakeVisible(*c.s); }
    extra(GB, "bY", "Synchro", nullptr); extra(GN, "nTyp", "Couleur", nullptr);
    extra(GF, "fT", "Mode", nullptr); extra(GF, "fSl", "Pente", nullptr); extra(GF, "fCar", "Caractère", nullptr);
    extra(G1, "l1S", "Forme", nullptr); extra(G1, "l1Y", "Synchro", nullptr);
    extra(G2, "l2S", "Forme", nullptr); extra(G2, "l2Y", "Synchro", nullptr);
    extra(GG, "leg", "Mode", "Legato"); extra(GS, "sMode", "Mode", nullptr);
    extra(GL, "tOn", "Insert", "Actif"); extra(GQ, "eOn", "Insert", "Actif");
    for (auto* id : ids) pr.apvts.addParameterListener(id, this);
    refreshPresets();
    pre.onChange = [this] { int i = pre.getSelectedId() - 1; if (i < 0 || i >= (int) entries.size()) return; auto& e = entries[(size_t) i];
      if (e.factory >= 0) pr.setCurrentProgram(e.factory); else pr.loadUser(e.file); };
    auto step = [this](int dlt) { int n = (int) entries.size(), id = pre.getSelectedId(); if (n > 0) pre.setSelectedId(((id - 1 + dlt + n) % n) + 1, sendNotificationSync); };
    prev.onClick = [step] { step(-1); }; next.onClick = [step] { step(1); };
    save.onClick = [this] {
      auto* aw = new AlertWindow("Sauvegarder le preset", "Nom du preset :", MessageBoxIconType::NoIcon);
      aw->addTextEditor("nom", pr.currentName()); aw->addButton("Sauver", 1, KeyPress(KeyPress::returnKey)); aw->addButton("Annuler", 0, KeyPress(KeyPress::escapeKey));
      auto* cb = new SaveCb(); Component::SafePointer<Editor> ed(this); Component::SafePointer<AlertWindow> sp(aw);
      cb->f = [ed, sp](int r) { if (r == 1 && ed != nullptr && sp != nullptr) { auto n = sp->getTextEditorContents("nom").trim(); if (n.isNotEmpty()) { ed->pr.saveUser(n); ed->refreshPresets(); } } };
      aw->enterModalState(true, cb, true); };
    addAndMakeVisible(pre); addAndMakeVisible(prev); addAndMakeVisible(next); addAndMakeVisible(save);
    screen = std::make_unique<ScreenView>(pr, last, ids); addAndMakeVisible(*screen);
    eqv = std::make_unique<EqView>(pr); addAndMakeVisible(*eqv);
    setSize(1360, 930); startTimerHz(15); }
  ~Editor() override { stopTimer(); for (auto* id : ids) pr.apvts.removeParameterListener(id, this); setLookAndFeel(nullptr); }
  void paint(Graphics& g) override {
    g.drawImageAt(bg, 0, 0);
    g.drawImageWithin(logo, CHEEK + 44, 20, 92, 92, RectanglePlacement::centred);
    g.setFont(27.f); g.setColour(Colours::black.withAlpha(.75f)); g.drawText("OKTOPLUS SYNTH", 172, 33, 250, 34, Justification::left);
    g.setColour(Colour(0xfff0f2f4)); g.drawText("OKTOPLUS SYNTH", 172, 31, 250, 34, Justification::left);
    led(g, 184.f, 92.f, kBlue, true); led(g, 262.f, 92.f, Colour(0xff5cff8a), voiceLit);
    g.setFont(11.f); g.setColour(Colour(0xffc9ced3)); g.drawText("MARCHE", 194, 84, 60, 16, Justification::left); g.drawText("VOIX", 272, 84, 50, 16, Justification::left);
    g.setColour(Colour(0xaa000000)); g.drawHorizontalLine(134, (float) CHEEK + 10.f, (float) getWidth() - CHEEK - 10.f);
    g.setColour(Colour(0x44ffffff)); g.drawHorizontalLine(135, (float) CHEEK + 10.f, (float) getWidth() - CHEEK - 10.f);
    for (auto& s : secs) { auto a = s->area.toFloat();
      g.setColour(Colour(0x1c000000)); g.fillRoundedRectangle(a, 6.f);
      g.setColour(Colour(0xaa000000)); g.drawRoundedRectangle(a, 6.f, 1.2f); g.setColour(Colour(0x30ffffff)); g.drawRoundedRectangle(a.translated(0.f, 1.2f), 6.f, 1.f);
      g.setColour(Colour(0xfff0f2f4)); g.setFont(13.f); g.drawText(s->title, s->area.getX() + 10, s->area.getY() + 3, s->area.getWidth() - 16, 18, Justification::left);
      g.setColour(kBlue); g.fillRect(s->area.getX() + 8, s->area.getY() + 22, s->area.getWidth() - 16, 2); } }
  void resized() override {
    bg = Image(Image::RGB, getWidth(), getHeight(), true); { Graphics g(bg); background(g, getWidth(), getHeight()); }
    screen->setBounds(430, 12, 560, 104);
    prev.setBounds(1010, 34, 30, 28); pre.setBounds(1044, 34, 214, 28); next.setBounds(1262, 34, 30, 28); save.setBounds(1010, 70, 110, 28);
    static const int R[5][3] = {{0, 1, 2}, {3, 4, -1}, {5, 6, 7}, {8, 9, 10}, {11, -1, -1}};
    for (int r = 0; r < 5; ++r) { int x = CHEEK + 14, y = 142 + r * 156;
      for (int k = 0; k < 3; ++k) { int id = R[r][k]; if (id < 0 || id >= (int) secs.size()) continue;
        auto& s = *secs[(size_t) id]; int n = (int) s.cells.size(); s.area = {x, y, n * CW + 12, 28 + CH + 8};
        for (int i = 0; i < n; ++i) { auto& c = *s.cells[(size_t) i]; int bx = x + 6 + i * CW, by = y + 28;
          c.l.setBounds(bx, by, CW, 16);
          if (c.s) c.s->setBounds(bx, by + 16, CW, CH - 16); else c.main()->setBounds(bx + 4, by + 46, CW - 8, 26); }
        if (id == 11) eqv->setBounds(x + n * CW + 34, y, getWidth() - CHEEK - 14 - (x + n * CW + 34), 28 + CH + 8);
        x += n * CW + 22; } } }
};

AudioProcessorEditor* Proc::createEditor() { return new Editor(*this); }
AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new Proc(); }
