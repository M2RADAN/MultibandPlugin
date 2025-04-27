#include "BandSelectControls.h"
// #include "Utilities.h" // �� ������������ ����� ��������

namespace MBRP_GUI
{

    BandSelectControls::BandSelectControls()
    {
        // ������ ��� ��������� ������ ������ ������
        auto setupBandButton = [&](juce::ToggleButton& button, const juce::String& name)
            {
                button.setName(name);
                button.setButtonText(name);
                // ���������� LookAndFeel, ������� �������� �����
                button.setRadioGroupId(1); // ���������� � ������
                button.setClickingTogglesState(true); // ����������� �� �����
                button.onClick = [this, &button]() { bandButtonClicked(&button); }; // ���������� �����
                addAndMakeVisible(button);
            };

        // ����������� ������
        setupBandButton(lowBandButton, "Low");
        setupBandButton(midBandButton, "Mid");
        setupBandButton(highBandButton, "High");

        // �������� "Low" �� ���������
        lowBandButton.setToggleState(true, juce::NotificationType::dontSendNotification);
        activeBandButton = &lowBandButton;
    }

    void BandSelectControls::resized()
    {
        auto bounds = getLocalBounds().reduced(5);
        juce::FlexBox flexBox;
        flexBox.flexDirection = juce::FlexBox::Direction::row;
        flexBox.justifyContent = juce::FlexBox::JustifyContent::spaceAround;
        flexBox.alignItems = juce::FlexBox::AlignItems::stretch;

        flexBox.items.add(juce::FlexItem(lowBandButton).withFlex(1.0f));
        flexBox.items.add(juce::FlexItem(midBandButton).withFlex(1.0f));
        flexBox.items.add(juce::FlexItem(highBandButton).withFlex(1.0f));

        flexBox.performLayout(bounds);
    }

    void BandSelectControls::paint(juce::Graphics& g)
    {
        // ����� �������� ������ ��� ���������� ���, ���� �����
        g.fillAll(juce::Colours::transparentBlack);
    }

    // ���������� ������ �� ������� ������ ������
    void BandSelectControls::bandButtonClicked(juce::Button* button)
    {
        int bandIndex = -1;
        if (button == &lowBandButton) { bandIndex = 0; activeBandButton = &lowBandButton; }
        else if (button == &midBandButton) { bandIndex = 1; activeBandButton = &midBandButton; }
        else if (button == &highBandButton) { bandIndex = 2; activeBandButton = &highBandButton; }

        // ���������� ���������� (��������) ������ ��� �������� �����
        if (bandIndex != -1 && onBandSelected)
        {
            onBandSelected(bandIndex);
        }
        // �������������� ��� ���������� ���� ������ LookAndFeel'��
        repaint();
    }

    // --- ���������: ���������� setSelectedBand ---
    // ���� ����� ���������� ����� (�� ���������), ����� ����������
    // ������� ������ ������, ��������, ����� ����� �� �����������.
    void BandSelectControls::setSelectedBand(int bandIndex)
    {
        juce::ToggleButton* buttonToSelect = nullptr;
        switch (bandIndex)
        {
        case 0: buttonToSelect = &lowBandButton; break;
        case 1: buttonToSelect = &midBandButton; break;
        case 2: buttonToSelect = &highBandButton; break;
        default: return; // �������� ������, ������ �� ������
        }

        // ������ ���������, ������ ���� ������� ������ ������
        if (buttonToSelect != nullptr && buttonToSelect != activeBandButton)
        {
            // ������������� ��������� ����� ������ ��� ������ onClick
            // � ��� �������� ����������� ���������� ���� ������
            buttonToSelect->setToggleState(true, juce::NotificationType::dontSendNotification);

            // activeBandButton ��������� �������������, ��� ��� setToggleState(true)
            // ������ �����-������ ������� ��������� ������ ������.
            // �� ��� ���������� ����� �������� � �������:
            activeBandButton = buttonToSelect;

            // �������������� ���������, ����� LookAndFeel ������� ��� ������
            repaint();
        }
    }
    // -------------------------------------------

} // ����� namespace MBRP_GUI