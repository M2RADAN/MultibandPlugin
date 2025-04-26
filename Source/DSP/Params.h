/*
  ==============================================================================

    Params.h
    Created: 30 Oct 2021 1:05:52am
    Author:  matkatmusic

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

namespace Params
{
enum Names
{
    Low_Mid_Crossover_Freq,
    Mid_High_Crossover_Freq,
    

};

inline const std::map<Names, juce::String>& GetParams()
{
    static std::map<Names, juce::String> params =
    {
        {Low_Mid_Crossover_Freq, "Low-Mid Crossover Freq"},
        {Mid_High_Crossover_Freq,"Mid_High Crossover Freq"},

    };
    
    return params;
}
}
