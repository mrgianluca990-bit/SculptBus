#include "PluginEditor.h"
#include "BinaryData.h"
#include <cmath>

namespace {
constexpr float kStart = juce::MathConstants<float>::pi * 1.20f;
constexpr float kEnd   = juce::MathConstants<float>::pi * 2.80f;
}

void SculptPreciseLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                                  float pos, float start, float end, juce::Slider&)
{
    auto a=juce::Rectangle<float>((float)x,(float)y,(float)width,(float)height).reduced(2.0f);
    auto c=a.getCentre();
    float r=juce::jmin(a.getWidth(),a.getHeight())*0.47f;
    float angle=start+pos*(end-start);

    auto shadow=juce::Rectangle<float>(2.0f*r,2.0f*r).withCentre(c.translated(0.0f,4.0f));
    g.setColour(juce::Colour(0x88000000)); g.fillEllipse(shadow);

    auto outer=juce::Rectangle<float>(2.0f*r,2.0f*r).withCentre(c);
    juce::ColourGradient rim(juce::Colour(0xffb58e61),outer.getX(),outer.getY(),juce::Colour(0xff3a2b1e),outer.getRight(),outer.getBottom(),false);
    g.setGradientFill(rim); g.fillEllipse(outer);
    g.setColour(juce::Colour(0xffd2aa78)); g.drawEllipse(outer.reduced(1.0f),1.0f);

    auto blackRing=outer.reduced(r*0.10f);
    g.setColour(juce::Colour(0xff090a0b)); g.fillEllipse(blackRing);

    juce::Path arc;
    arc.addCentredArc(c.x,c.y,blackRing.getWidth()*0.49f,blackRing.getHeight()*0.49f,0.0f,start,angle,true);
    g.setColour(juce::Colour(0x65ff5e16)); g.strokePath(arc,juce::PathStrokeType(6.0f));
    g.setColour(juce::Colour(0xeaff9b28)); g.strokePath(arc,juce::PathStrokeType(2.0f));

    auto knob=blackRing.reduced(r*0.18f);
    juce::ColourGradient body(juce::Colour(0xff3d3e40),knob.getCentreX(),knob.getY(),juce::Colour(0xff0b0c0d),knob.getCentreX(),knob.getBottom(),false);
    g.setGradientFill(body); g.fillEllipse(knob);
    g.setColour(juce::Colour(0xff606166)); g.drawEllipse(knob,1.0f);

    for(int i=14;i>=1;--i){
        float rr=knob.getWidth()*0.32f*(float)i/14.0f;
        auto ring=juce::Rectangle<float>(2*rr,2*rr).withCentre(c);
        auto q=(juce::uint8)juce::jlimit(25,74,25+i*3);
        g.setColour(juce::Colour(q,q,q)); g.drawEllipse(ring,0.55f);
    }

    auto p1=c+juce::Point<float>(std::sin(angle),-std::cos(angle))*(knob.getWidth()*0.11f);
    auto p2=c+juce::Point<float>(std::sin(angle),-std::cos(angle))*(knob.getWidth()*0.41f);
    g.setColour(juce::Colour(0xfff2e7d2)); g.drawLine({p1,p2},3.0f);
}

void SculptPreciseLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool, bool)
{
    auto a=b.getLocalBounds().toFloat().reduced(3.0f);
    bool on=b.getToggleState();
    g.setColour(juce::Colour(0x77000000)); g.fillRoundedRectangle(a.translated(0,3),7.0f);
    g.setColour(juce::Colour(0xff2b0907)); g.fillRoundedRectangle(a,7.0f);
    g.setColour(juce::Colour(0xff9a704d)); g.drawRoundedRectangle(a,7.0f,1.0f);
    auto in=a.reduced(9.0f,8.0f);
    juce::ColourGradient face(on?juce::Colour(0xffff8064):juce::Colour(0xff70231b),in.getCentreX(),in.getY(),
                              on?juce::Colour(0xffffd4a8):juce::Colour(0xffb53b2e),in.getCentreX(),in.getBottom(),false);
    g.setGradientFill(face); g.fillRoundedRectangle(in,5.0f);
    if(on){g.setColour(juce::Colour(0x55ff543f));g.fillRoundedRectangle(in.expanded(7.0f),9.0f);}
    g.setColour(juce::Colour(0xfff5ead7));g.fillEllipse(juce::Rectangle<float>(8,8).withCentre(in.getCentre()));
}

SculptBusAudioProcessorEditor::SculptBusAudioProcessorEditor (SculptBusAudioProcessor& p)
: AudioProcessorEditor(&p), processor(p)
{
    background=juce::ImageFileFormat::loadFrom(BinaryData::sculpt_gui_bg_precise_png,BinaryData::sculpt_gui_bg_precise_pngSize);
    setLookAndFeel(&look);
    setupKnob(low,"low"); setupKnob(mid,"mid"); setupKnob(high,"high"); setupKnob(presence,"presence"); setupKnob(output,"output",true);
    variationButton.setClickingTogglesState(true); variationButton.setColour(juce::ToggleButton::textColourId,juce::Colours::transparentBlack);
    addAndMakeVisible(variationButton);
    variationAttachment=std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.getAPVTS(),"variation",variationButton);
    setSize(836,471); setResizable(true,true); setResizeLimits(836,471,1672,941); startTimerHz(30);
}

SculptBusAudioProcessorEditor::~SculptBusAudioProcessorEditor(){stopTimer();setLookAndFeel(nullptr);}

void SculptBusAudioProcessorEditor::setupKnob (KnobUI& k,const juce::String& id,bool outputKnob)
{
    k.slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag); k.slider.setRotaryParameters(kStart,kEnd,true);
    k.slider.setTextBoxStyle(juce::Slider::NoTextBox,false,0,0); addAndMakeVisible(k.slider);
    k.attachment=std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.getAPVTS(),id,k.slider);
    k.slider.setMouseDragSensitivity(outputKnob?220:180);
}

juce::Rectangle<int> SculptBusAudioProcessorEditor::scaledRect (float x,float y,float w,float h) const
{
    float sx=(float)getWidth()/1672.0f, sy=(float)getHeight()/941.0f;
    return {(int)std::round(x*sx),(int)std::round(y*sy),(int)std::round(w*sx),(int)std::round(h*sy)};
}

void SculptBusAudioProcessorEditor::drawSegmentMeter (juce::Graphics& g,juce::Rectangle<float> a,float value,int segments)
{
    value=juce::jlimit(0.0f,1.0f,value); float gap=a.getWidth()*0.025f;
    float w=(a.getWidth()-gap*(segments-1))/(float)segments; int lit=(int)std::round(value*segments);
    for(int i=0;i<segments;++i){
        auto s=juce::Rectangle<float>(a.getX()+i*(w+gap),a.getY(),w,a.getHeight());
        g.setColour(juce::Colour(0xff221713));g.fillRoundedRectangle(s,1.0f);
        if(i<lit){float t=(float)i/(float)juce::jmax(1,segments-1);auto c=juce::Colour(0xfffff0c8).interpolatedWith(juce::Colour(0xffff2f29),t);
            g.setColour(c);g.fillRoundedRectangle(s.reduced(0.4f),1.0f);g.setColour(c.withAlpha(0.25f));g.fillRoundedRectangle(s.expanded(1.0f),1.5f);}
    }
}

void SculptBusAudioProcessorEditor::drawAllMeters (juce::Graphics& g)
{
    float sx=(float)getWidth()/1672.0f, sy=(float)getHeight()/941.0f;
    drawSegmentMeter(g,{1172*sx,131*sy,262*sx,18*sy},processor.getInputMeter(),19);
    drawSegmentMeter(g,{1172*sx,207*sy,262*sx,18*sy},processor.getOutputMeter(),19);
    const int xs[4]={214,525,828,1131}; const int ys[3]={602,650,701};
    for(int b=0;b<4;++b){
        drawSegmentMeter(g,{xs[b]*sx,ys[0]*sy,156*sx,17*sy},processor.getResMeter(b),12);
        drawSegmentMeter(g,{xs[b]*sx,ys[1]*sy,156*sx,18*sy},processor.getCompMeter(b),12);
        drawSegmentMeter(g,{xs[b]*sx,ys[2]*sy,156*sx,18*sy},processor.getSatMeter(b),12);
    }
}

void SculptBusAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll(juce::Colours::black); if(background.isValid())g.drawImage(background,getLocalBounds().toFloat()); drawAllMeters(g);
}

void SculptBusAudioProcessorEditor::resized()
{
    // Exact measured centres: 272/464, 578/459, 879/457, 1179/457, 1458/459.
    low.slider.setBounds(scaledRect(197,389,150,150));
    mid.slider.setBounds(scaledRect(503,384,150,150));
    high.slider.setBounds(scaledRect(809,387,140,140));
    presence.slider.setBounds(scaledRect(1109,387,140,140));
    output.slider.setBounds(scaledRect(1403,404,110,110));
    variationButton.setBounds(scaledRect(1417,573,84,65));
}

void SculptBusAudioProcessorEditor::timerCallback(){repaint();}
