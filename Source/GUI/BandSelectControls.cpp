#include "BandSelectControls.h"
#include "Utilities.h" // ��� drawModuleBackground � mapX/mapY, ���� �����

namespace MBRP_GUI
{

    BandSelectControls::BandSelectControls()
    {
        // ������ ��� ��������� ������ ������ ������
        auto setupBandButton = [&](juce::ToggleButton& button, const juce::String& name, juce::Colour /*color*/) // ���� ���� �� ���������� ��������
            {
                button.setName(name);
                button.setButtonText(name); // ������������� ����� ������
                // ���������� ����������� LookAndFeel ��� ToggleButton, ������� �� �������� � LookAndFeel.cpp
                // (���������� ����� ��������� ����� LookAndFeel::drawToggleButton)
                button.setRadioGroupId(1); // ���������� � ������ �����-������
                button.setClickingTogglesState(true); // ����������� ��������� �� �����
                button.onClick = [this, &button]() { bandButtonClicked(&button); }; // ���������� �����
                addAndMakeVisible(button);
            };

        // ����������� ������
        setupBandButton(lowBandButton, "Low", juce::Colours::orange); // ���� ����������, �� �� ������������ �������� �����
        setupBandButton(midBandButton, "Mid", juce::Colours::lightgreen);
        setupBandButton(highBandButton, "High", juce::Colours::cyan);

        // �������� "Low" �� ���������
        lowBandButton.setToggleState(true, juce::NotificationType::dontSendNotification);
        activeBandButton = &lowBandButton; // ������������� �������� ������
    }

    void BandSelectControls::resized()
    {
        auto bounds = getLocalBounds().reduced(5); // ��������� ������
        juce::FlexBox flexBox;
        flexBox.flexDirection = juce::FlexBox::Direction::row; // ����������� � ���
        flexBox.justifyContent = juce::FlexBox::JustifyContent::spaceAround; // ������������ ���������
        flexBox.alignItems = juce::FlexBox::AlignItems::stretch; // ����������� �� ������

        // ��������� ������ �� FlexBox
        flexBox.items.add(juce::FlexItem(lowBandButton).withFlex(1.0f));
        flexBox.items.add(juce::FlexItem(midBandButton).withFlex(1.0f));
        flexBox.items.add(juce::FlexItem(highBandButton).withFlex(1.0f));

        // ��������� �����
        flexBox.performLayout(bounds);
    }

    void BandSelectControls::paint(juce::Graphics& g)
    {
        // �����������: ������ ��� ������, ���� �����
        // MBRP_GUI::drawModuleBackground(g, getLocalBounds());
        g.fillAll(juce::Colours::transparentBlack); // ���������� ��� �� ���������
    }

    // ���������� ������ �� ������� ������ ������
    void BandSelectControls::bandButtonClicked(juce::Button* button)
    {
        // ���������� ������ ��������� ������
        int bandIndex = -1;
        if (button == &lowBandButton)
        {
            bandIndex = 0;
            activeBandButton = &lowBandButton;
        }
        else if (button == &midBandButton)
        {
            bandIndex = 1;
            activeBandButton = &midBandButton;
        }
        else if (button == &highBandButton)
        {
            bandIndex = 2;
            activeBandButton = &highBandButton;
        }

        // ���� ���� ��������� (��������) � ������ ���������, ���������� ���
        if (bandIndex != -1 && onBandSelected)
        {
            onBandSelected(bandIndex);
        }
        // ��������������, ����� LookAndFeel ������� ��� ������
        repaint();
    }

} // ����� namespace MBRP_GUI