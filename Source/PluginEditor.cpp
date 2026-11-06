#include "PluginEditor.h"
#include "BinaryData.h"

namespace
{
    // Tutte le coordinate sono in pixel dell'immagine originale (2592 x 898)
    constexpr float imgW = 2592.0f;
    constexpr float imgH = 898.0f;

    struct KnobInfo
    {
        const char* id;
        const char* label;
        float hx, hy, hr;   // area cliccabile (gonna della manopola)
        float fx, fy;       // centro della faccia (indice disegnato)
        bool pointer;       // false = manopola senza indice (HISS)
    };

    const KnobInfo knobs[] = {
        { "volume",  "DRIVE",   803.0f, 697.0f, 78.0f,  787.0f, 707.0f, true },
        { "mix",     "DRY/WET", 1085.0f, 689.0f, 74.0f, 1078.0f, 707.0f, true },
        { "time",    "DELAY",   1363.0f, 690.0f, 74.0f, 1364.0f, 707.0f, true },
        { "repeat",  "REPEAT",  1643.0f, 690.0f, 74.0f, 1653.0f, 706.0f, true },
        { "output",  "OUTPUT",  2077.0f, 694.0f, 78.0f, 2092.0f, 707.0f, true },
        { "hiss",    "HISS",    2264.0f, 558.0f, 40.0f, 2264.0f, 558.0f, false }
    };
    constexpr int numKnobs = (int) (sizeof (knobs) / sizeof (knobs[0]));

    const juce::Point<float> tapCentre   { 1504.0f, 705.0f };  constexpr float tapR  = 48.0f;
    const juce::Point<float> modeCentre  { 1867.0f, 698.0f };  constexpr float modeR = 58.0f;
    const juce::Point<float> in1Centre   {  333.0f, 598.0f };
    const juce::Point<float> in2Centre   {  333.0f, 725.0f };
    const juce::Point<float> bypCentre   { 2264.0f, 719.0f };  constexpr float jackR = 48.0f;
    const juce::Point<float> powerLed    {  537.0f, 272.0f };
    const juce::Point<float> tapLed      { 1504.0f, 596.0f };
    const juce::Point<float> vuPivot     { 2080.0f, 382.0f };
    const juce::Rectangle<int>   vuFace     { 1922, 159, 332, 175 };
    const juce::Rectangle<float> powerArea  { 288.0f, 197.0f, 124.0f, 144.0f };
    const juce::Rectangle<float> syncArea   { 1770.0f, 760.0f, 190.0f, 70.0f };
    const juce::Rectangle<float> syncLine1  { 1772.0f, 766.0f, 168.0f, 32.0f };  // "MIDI CLOCK/"
    const juce::Rectangle<float> syncLine2  { 1810.0f, 795.0f, 114.0f, 30.0f };  // "MANUAL"

    // rulli del meccanismo nel vano del nastro (centro, raggio, verso di rotazione)
    struct RollerInfo { const char* data; int size; float cx, cy, r, dir; };

    const RollerInfo rollers[] = {
        { BinaryData::reel_png,     BinaryData::reel_pngSize,     1077.0f, 269.0f, 62.0f,  1.0f },
        { BinaryData::capstan_png,  BinaryData::capstan_pngSize,  1452.0f, 261.0f, 42.0f,  1.0f },
        { BinaryData::roller_a_png, BinaryData::roller_a_pngSize, 1413.0f, 434.0f, 30.0f, -1.0f },
        { BinaryData::roller_b_png, BinaryData::roller_b_pngSize, 1485.0f, 415.0f, 32.0f,  1.0f },
        { BinaryData::roller_c_png, BinaryData::roller_c_pngSize, 1595.0f, 407.0f, 28.0f, -1.0f },
        { BinaryData::guide_a_png,  BinaryData::guide_a_pngSize,   891.0f, 384.0f, 18.0f, -1.0f },
        { BinaryData::guide_b_png,  BinaryData::guide_b_pngSize,   933.0f, 420.0f, 22.0f,  1.0f }
    };
    constexpr int numRollers = (int) (sizeof (rollers) / sizeof (rollers[0]));

    // tratti di nastro visibili, nel verso di scorrimento
    struct TapeSeg { float x1, y1, x2, y2; };
    const TapeSeg tapeSegs[] = {
        { 1095.0f, 200.0f, 1445.0f, 217.0f },   // bobina -> capstan (sopra)
        { 1500.0f, 273.0f, 1459.0f, 384.0f },   // capstan -> rulli
        { 1119.0f, 423.0f,  902.0f, 372.0f },   // testine -> guida
        {  895.0f, 364.0f, 1021.0f, 231.0f },   // guida -> bobina
        { 1119.0f, 421.0f, 1052.0f, 328.0f }    // testine -> bobina (sotto)
    };

    // oscillazione visibile della velocita' per tipo di nastro
    const float modeWobble[] = { 0.03f, 0.06f, 0.12f, 0.22f };
    const float modeJitter[] = { 0.0f, 0.0f, 0.5f, 1.2f };

    constexpr float modeOriginalDeg = -30.0f;  // orientamento della manopola nella foto

    const juce::Colour amber (0xffffb347);
}

MagnetoEditor::MagnetoEditor (MagnetoProcessor& p)
    : AudioProcessorEditor (&p), proc (p)
{
    background = juce::ImageCache::getFromMemory (BinaryData::background_jpg,   BinaryData::background_jpgSize);
    ledPowerOn = juce::ImageCache::getFromMemory (BinaryData::led_power_on_png, BinaryData::led_power_on_pngSize);
    ledTapOn   = juce::ImageCache::getFromMemory (BinaryData::led_tap_on_png,   BinaryData::led_tap_on_pngSize);
    toggleOff  = juce::ImageCache::getFromMemory (BinaryData::toggle_off_png,   BinaryData::toggle_off_pngSize);
    modeKnob   = juce::ImageCache::getFromMemory (BinaryData::mode_knob_png,    BinaryData::mode_knob_pngSize);

    for (const auto& r : rollers)
        rollerImages.push_back (juce::ImageCache::getFromMemory (r.data, r.size));

    setOpaque (true);
    setResizable (true, true);
    setResizeLimits (864, 299, 2592, 898);
    if (auto* c = getConstrainer())
        c->setFixedAspectRatio ((double) imgW / (double) imgH);

    setSize (1296, 449);
    startTimerHz (60);
}

MagnetoEditor::~MagnetoEditor()
{
    stopTimer();
}

void MagnetoEditor::resized()
{
    cacheScale = 0.0f; // forza la ricostruzione dello sfondo alla nuova dimensione
}

juce::Point<float> MagnetoEditor::toImage (juce::Point<float> p) const
{
    const float s = (float) getWidth() / imgW;
    return s > 0.0f ? p / s : p;
}

// ============================================================================ helpers parametri
bool MagnetoEditor::getBool (const juce::String& id) const
{
    if (auto* v = proc.apvts.getRawParameterValue (id))
        return v->load() > 0.5f;
    return false;
}

void MagnetoEditor::setParamNorm (const juce::String& id, float norm)
{
    if (auto* prm = proc.apvts.getParameter (id))
    {
        prm->beginChangeGesture();
        prm->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, norm));
        prm->endChangeGesture();
    }
}

void MagnetoEditor::setBool (const juce::String& id, bool v)
{
    setParamNorm (id, v ? 1.0f : 0.0f);
}

void MagnetoEditor::stepMode (int delta)
{
    if (auto* prm = proc.apvts.getParameter ("mode"))
    {
        const int idx = juce::jlimit (0, 3, juce::roundToInt (prm->convertFrom0to1 (prm->getValue())) + delta);
        setParamNorm ("mode", prm->convertTo0to1 ((float) idx));
    }
}

void MagnetoEditor::handleTap()
{
    // TAP TEMPO: attiva/disattiva il delay a tempo con il progetto
    setBool ("sync", ! getBool ("sync"));
}

juce::String MagnetoEditor::valueText (int i) const
{
    auto* prm = proc.apvts.getParameter (knobs[i].id);
    if (prm == nullptr)
        return {};

    const float v = prm->convertFrom0to1 (prm->getValue());
    const juce::String id (knobs[i].id);

    if (id == "time")
    {
        if (getBool ("sync"))
            return "DELAY " + MagnetoProcessor::syncName (MagnetoProcessor::syncIndexFromNorm (prm->getValue()));
        return "DELAY " + juce::String (juce::roundToInt (v)) + " ms";
    }

    if (id == "output")
    {
        const float db = MagnetoProcessor::outputKnobToDb (v);
        return "OUTPUT " + (db <= -99.0f ? juce::String ("-inf") : juce::String (db, 1)) + " dB";
    }

    if (id == "mix")
        return "DRY/WET " + juce::String (juce::roundToInt (v * 10.0f)) + "%";

    return juce::String (knobs[i].label) + " " + juce::String (v, 1);
}

// ============================================================================ disegno
float MagnetoEditor::vuToDegrees (float vu)
{
    static const float dbs[]  = { -20.0f, -10.0f, -7.0f, -5.0f, -3.0f, -2.0f, -1.0f, 0.0f, 1.0f, 2.0f, 3.0f };
    static const float frac[] = { 0.0f, 0.22f, 0.33f, 0.44f, 0.56f, 0.63f, 0.70f, 0.77f, 0.85f, 0.92f, 1.0f };
    constexpr int n = 11;

    float f;
    if (vu <= dbs[0])
        f = (vu - dbs[0]) / 10.0f * frac[1];
    else if (vu >= dbs[n - 1])
        f = 1.0f + (vu - dbs[n - 1]) * 0.05f;
    else
    {
        int k = 0;
        while (k < n - 2 && vu > dbs[k + 1]) ++k;
        const float t = (vu - dbs[k]) / (dbs[k + 1] - dbs[k]);
        f = frac[k] + t * (frac[k + 1] - frac[k]);
    }

    return juce::jlimit (-46.0f, 45.0f, -42.0f + 84.0f * f);
}

void MagnetoEditor::drawPointer (juce::Graphics& g, float cx, float cy, float norm) const
{
    const float a = juce::degreesToRadians (-135.0f + 270.0f * norm);
    const juce::Point<float> dir (std::sin (a), -std::cos (a));
    const juce::Point<float> c (cx, cy);
    const auto p1 = c + dir * 8.0f;
    const auto p2 = c + dir * 44.0f;

    g.setColour (juce::Colour (0xff262626));
    g.drawLine ({ p1, p2 }, 5.0f);
    g.setColour (juce::Colours::white.withAlpha (0.55f));
    g.drawLine ({ p1 + juce::Point<float> (1.4f, 1.4f), p2 + juce::Point<float> (1.4f, 1.4f) }, 1.3f);
}

void MagnetoEditor::drawRing (juce::Graphics& g, float cx, float cy, float r, juce::Colour col) const
{
    g.setColour (col.withAlpha (0.22f));
    g.drawEllipse (cx - r, cy - r, r * 2.0f, r * 2.0f, 12.0f);
    g.setColour (col.withAlpha (0.85f));
    g.drawEllipse (cx - r, cy - r, r * 2.0f, r * 2.0f, 3.0f);
}

void MagnetoEditor::paint (juce::Graphics& g)
{
    const float s = (float) getWidth() / imgW;
    if (s <= 0.0f)
        return;

    // sfondo in cache alla risoluzione fisica dello schermo
    const float phys = g.getInternalContext().getPhysicalPixelScaleFactor();
    const float want = s * phys;
    if (bgCache.isNull() || std::abs (want - cacheScale) > 0.001f)
    {
        bgCache = background.rescaled (juce::jmax (1, juce::roundToInt (imgW * want)),
                                       juce::jmax (1, juce::roundToInt (imgH * want)),
                                       juce::Graphics::highResamplingQuality);
        cacheScale = want;
    }
    g.drawImage (bgCache, getLocalBounds().toFloat());

    juce::Graphics::ScopedSaveState state (g);
    g.addTransform (juce::AffineTransform::scale (s));
    g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);

    const bool power = getBool ("power");

    // ---- meccanismo del nastro animato
    drawTapeTransport (g);

    // ---- interruttore ON/OFF + LED rosso
    if (power)
        g.drawImageAt (ledPowerOn, juce::roundToInt (powerLed.x - ledPowerOn.getWidth() * 0.5f),
                                   juce::roundToInt (powerLed.y - ledPowerOn.getHeight() * 0.5f));
    else
        g.drawImageAt (toggleOff, juce::roundToInt (powerArea.getX()), juce::roundToInt (powerArea.getY()));

    // ---- LED del tap tempo: lampeggia al tempo del delay quando e' a tempo col progetto
    if (power && getBool ("sync"))
    {
        const double period = juce::jmax (40.0, (double) proc.currentDelayMs.load());
        const double phase = std::fmod (juce::Time::getMillisecondCounterHiRes(), period) / period;
        if (phase < 0.15)
            g.drawImageAt (ledTapOn, juce::roundToInt (tapLed.x - ledTapOn.getWidth() * 0.5f),
                                     juce::roundToInt (tapLed.y - ledTapOn.getHeight() * 0.5f));
    }

    // ---- manopole
    const bool synced = getBool ("sync");
    for (int i = 0; i < numKnobs; ++i)
    {
        if (auto* prm = proc.apvts.getParameter (knobs[i].id))
        {
            float norm = prm->getValue();
            if (synced && juce::String (knobs[i].id) == "time")
            {
                const int n = MagnetoProcessor::numSyncDivisions();
                norm = (float) MagnetoProcessor::syncIndexFromNorm (norm) / (float) (n - 1);
            }
            if (knobs[i].pointer)
                drawPointer (g, knobs[i].fx, knobs[i].fy, norm);
        }
    }

    // in sincro: targhetta con la suddivisione sopra DELAY TIME
    if (synced)
        if (auto* prm = proc.apvts.getParameter ("time"))
        {
            const juce::Rectangle<float> plate (1300.0f, 583.0f, 126.0f, 40.0f);
            g.setColour (juce::Colour (0xe0141414));
            g.fillRoundedRectangle (plate, 8.0f);
            g.setColour (amber);
            g.setFont (juce::Font (juce::FontOptions (28.0f, juce::Font::bold)));
            g.drawFittedText (MagnetoProcessor::syncName (MagnetoProcessor::syncIndexFromNorm (prm->getValue())),
                              plate.toNearestInt(), juce::Justification::centred, 1);
        }

    // ---- selettore MODE (tipo di nastro)
    if (auto* prm = proc.apvts.getParameter ("mode"))
    {
        const int idx = juce::jlimit (0, 3, juce::roundToInt (prm->convertFrom0to1 (prm->getValue())));
        const float rot = juce::degreesToRadians ((-45.0f + 30.0f * (float) idx) - modeOriginalDeg);
        g.drawImageTransformed (modeKnob,
                                juce::AffineTransform::translation (-modeKnob.getWidth() * 0.5f, -modeKnob.getHeight() * 0.5f)
                                    .rotated (rot)
                                    .translated (modeCentre.x, modeCentre.y));
    }

    // ---- tasto TAP premuto
    if (tapPressed)
    {
        g.setColour (juce::Colours::black.withAlpha (0.28f));
        g.fillEllipse (tapCentre.x - 40.0f, tapCentre.y - 40.0f, 80.0f, 80.0f);
    }

    // ---- VU meter
    {
        juce::Graphics::ScopedSaveState vuState (g);
        g.reduceClipRegion (vuFace);

        const float a = juce::degreesToRadians (needleDeg);
        const auto tip = vuPivot + juce::Point<float> (std::sin (a), -std::cos (a)) * 190.0f;
        g.setColour (juce::Colour (0x40000000));
        g.drawLine ({ vuPivot + juce::Point<float> (3.0f, 3.0f), tip + juce::Point<float> (3.0f, 3.0f) }, 3.0f);
        g.setColour (juce::Colour (0xff1c1a18));
        g.drawLine ({ vuPivot, tip }, 3.2f);

        if (! power)
        {
            g.setColour (juce::Colours::black.withAlpha (0.5f));
            g.fillRect (vuFace);
        }
    }

    // ---- ingressi (1 = mono classico, 2 = stereo)
    if (getBool ("stereo"))
        drawRing (g, in2Centre.x, in2Centre.y, jackR, amber);
    else
        drawRing (g, in1Centre.x, in1Centre.y, jackR, amber);

    // ---- bypass echo
    if (getBool ("echoBypass"))
        drawRing (g, bypCentre.x, bypCentre.y, jackR, juce::Colour (0xffff3b30));

    // ---- MIDI CLOCK / MANUAL
    {
        const auto r = getBool ("sync") ? syncLine1 : syncLine2;
        g.setColour (amber.withAlpha (0.28f));
        g.fillRoundedRectangle (r.expanded (6.0f, 2.0f), 10.0f);
    }

    // ---- valore della manopola mentre la giri
    if (activeKnob >= 0)
    {
        const auto& k = knobs[activeKnob];
        const juce::Rectangle<float> bubble (k.hx - 110.0f, k.hy + 92.0f, 220.0f, 44.0f);
        g.setColour (juce::Colour (0xe0141414));
        g.fillRoundedRectangle (bubble, 10.0f);
        g.setColour (amber);
        g.setFont (juce::Font (juce::FontOptions (26.0f, juce::Font::bold)));
        g.drawFittedText (valueText (activeKnob), bubble.toNearestInt(), juce::Justification::centred, 1);
    }
}

void MagnetoEditor::drawTapeTransport (juce::Graphics& g) const
{
    // rulli e bobina che girano
    for (int i = 0; i < numRollers && i < (int) rollerImages.size(); ++i)
    {
        const auto& r = rollers[i];
        const auto& img = rollerImages[(size_t) i];
        const float ang = (float) (r.dir * tapePos / r.r);
        const float cx = r.cx + jitterX, cy = r.cy + jitterY;

        g.drawImageTransformed (img, juce::AffineTransform::translation (-img.getWidth() * 0.5f, -img.getHeight() * 0.5f)
                                         .rotated (ang)
                                         .translated (cx, cy));

        // viti sul mozzo (rendono visibile la rotazione)
        const int screws = i == 0 ? 3 : (i == 1 ? 2 : 0);
        const float sr = i == 0 ? 24.0f : 14.0f;
        for (int s = 0; s < screws; ++s)
        {
            const float a = ang + juce::MathConstants<float>::twoPi * (float) s / (float) screws;
            const float x = cx + std::sin (a) * sr, y = cy - std::cos (a) * sr;
            g.setColour (juce::Colours::black.withAlpha (0.55f));
            g.fillEllipse (x - 3.5f, y - 3.5f, 7.0f, 7.0f);
            g.setColour (juce::Colours::white.withAlpha (0.35f));
            g.fillEllipse (x - 2.5f, y - 3.0f, 2.5f, 2.5f);
        }
    }

    // riflessi che scorrono lungo il nastro
    constexpr float gap = 34.0f, dash = 9.0f;
    const float offset = (float) std::fmod (tapePos, (double) gap);
    for (const auto& s : tapeSegs)
    {
        const juce::Point<float> a (s.x1 + jitterX, s.y1 + jitterY), b (s.x2 + jitterX, s.y2 + jitterY);
        const float len = a.getDistanceFrom (b);
        if (len < 1.0f) continue;
        const auto dir = (b - a) / len;

        for (float d = offset; d + dash < len; d += gap)
        {
            g.setColour (juce::Colours::white.withAlpha (0.40f));
            g.drawLine ({ a + dir * d, a + dir * (d + dash) }, 2.2f);
        }
    }
}

// ============================================================================ animazione
void MagnetoEditor::timerCallback()
{
    const double now = juce::Time::getMillisecondCounterHiRes();
    const double dt = lastTimerMs > 0.0 ? juce::jlimit (0.001, 0.2, (now - lastTimerMs) * 0.001) : 1.0 / 60.0;
    lastTimerMs = now;
    animTime += dt;

    // velocita' del nastro: piu' corto il delay, piu' veloce il nastro (come sulle macchine vere)
    const bool power = getBool ("power");
    const int mode = juce::jlimit (0, 3, juce::roundToInt (proc.apvts.getRawParameterValue ("mode")->load()));
    const double delayMs = juce::jmax (40.0, (double) proc.currentDelayMs.load());
    const double target = power ? juce::jlimit (40.0, 1200.0, 220.0 * 350.0 / delayMs) : 0.0;
    const double tau = power ? 0.35 : 0.8;   // allo spegnimento rallenta piano
    tapeSpeed += (target - tapeSpeed) * (1.0 - std::exp (-dt / tau));

    // wow visibile in base al tipo di nastro (MODE)
    const double wobble = 1.0 + modeWobble[mode] * (0.7 * std::sin (animTime * juce::MathConstants<double>::twoPi * 0.65)
                                                  + 0.3 * std::sin (animTime * juce::MathConstants<double>::twoPi * 0.23));
    tapePos += tapeSpeed * wobble * dt;

    // nastri usurati: il meccanismo trema un po'
    const float jit = power ? modeJitter[mode] * (float) juce::jmin (1.0, tapeSpeed / 100.0) : 0.0f;
    jitterX = jit * (float) std::sin (animTime * 43.0);
    jitterY = jit * (float) std::sin (animTime * 57.0 + 1.3);

    // VU: 0 VU = -18 dBFS, balistica ~300 ms
    const float vuTarget = power ? vuToDegrees (proc.outLevelDb.load() + 18.0f) : -46.0f;
    const float k = 1.0f - (float) std::exp (-dt / 0.065);
    needleDeg += (vuTarget - needleDeg) * k;

    repaint();
}

// ============================================================================ mouse
int MagnetoEditor::knobAt (juce::Point<float> ip) const
{
    for (int i = 0; i < numKnobs; ++i)
        if (ip.getDistanceFrom ({ knobs[i].hx, knobs[i].hy }) <= knobs[i].hr)
            return i;
    return -1;
}

bool MagnetoEditor::isClickable (juce::Point<float> ip) const
{
    return knobAt (ip) >= 0
        || ip.getDistanceFrom (tapCentre)  <= tapR
        || ip.getDistanceFrom (modeCentre) <= modeR
        || ip.getDistanceFrom (in1Centre)  <= jackR
        || ip.getDistanceFrom (in2Centre)  <= jackR
        || ip.getDistanceFrom (bypCentre)  <= jackR
        || powerArea.contains (ip)
        || syncArea.contains (ip);
}

void MagnetoEditor::mouseMove (const juce::MouseEvent& e)
{
    setMouseCursor (isClickable (toImage (e.position)) ? juce::MouseCursor::PointingHandCursor
                                                       : juce::MouseCursor::NormalCursor);
}

void MagnetoEditor::mouseDown (const juce::MouseEvent& e)
{
    const auto ip = toImage (e.position);

    const int k = knobAt (ip);
    if (k >= 0)
    {
        if (auto* prm = proc.apvts.getParameter (knobs[k].id))
        {
            activeKnob = k;
            dragStartNorm = prm->getValue();
            dragStartY = ip.y;
            prm->beginChangeGesture();
        }
        return;
    }

    if (ip.getDistanceFrom (tapCentre) <= tapR)       { tapPressed = true; handleTap(); return; }
    if (ip.getDistanceFrom (modeCentre) <= modeR)     { stepMode (ip.x < modeCentre.x ? -1 : 1); return; }
    if (ip.getDistanceFrom (in1Centre) <= jackR)      { setBool ("stereo", false); return; }
    if (ip.getDistanceFrom (in2Centre) <= jackR)      { setBool ("stereo", true); return; }
    if (ip.getDistanceFrom (bypCentre) <= jackR)      { setBool ("echoBypass", ! getBool ("echoBypass")); return; }
    if (powerArea.contains (ip))                      { setBool ("power", ! getBool ("power")); return; }
    if (syncArea.contains (ip))                       { setBool ("sync", ! getBool ("sync")); return; }
}

void MagnetoEditor::mouseDrag (const juce::MouseEvent& e)
{
    if (activeKnob < 0)
        return;

    if (auto* prm = proc.apvts.getParameter (knobs[activeKnob].id))
    {
        const auto ip = toImage (e.position);
        const float range = e.mods.isShiftDown() ? 2400.0f : 520.0f;   // Shift = regolazione fine
        const float norm = juce::jlimit (0.0f, 1.0f, dragStartNorm + (dragStartY - ip.y) / range);
        prm->setValueNotifyingHost (norm);
    }
}

void MagnetoEditor::mouseUp (const juce::MouseEvent&)
{
    if (activeKnob >= 0)
        if (auto* prm = proc.apvts.getParameter (knobs[activeKnob].id))
            prm->endChangeGesture();

    activeKnob = -1;
    tapPressed = false;
}

void MagnetoEditor::mouseDoubleClick (const juce::MouseEvent& e)
{
    const int k = knobAt (toImage (e.position));
    if (k >= 0)
        if (auto* prm = proc.apvts.getParameter (knobs[k].id))
            setParamNorm (knobs[k].id, prm->getDefaultValue());
}

void MagnetoEditor::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    const auto ip = toImage (e.position);

    const int k = knobAt (ip);
    if (k >= 0)
    {
        if (auto* prm = proc.apvts.getParameter (knobs[k].id))
            setParamNorm (knobs[k].id, prm->getValue() + w.deltaY * 0.05f);
        return;
    }

    if (ip.getDistanceFrom (modeCentre) <= modeR && w.deltaY != 0.0f)
        stepMode (w.deltaY > 0.0f ? 1 : -1);
}
