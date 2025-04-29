#pragma once

#include <JuceHeader.h>
#include "LookAndFeel.h"

namespace MBRP_GUI 
{


    struct BandSelectControls : juce::Component
    {
        BandSelectControls(); 
        ~BandSelectControls() override = default;

        void resized() override;
        void paint(juce::Graphics& g) override;


        juce::ToggleButton lowBandButton, midBandButton, highBandButton;


        std::function<void(int bandIndex)> onBandSelected;

    private:

        void bandButtonClicked(juce::Button* button);

        juce::ToggleButton* activeBandButton = &lowBandButton;
    };

} 