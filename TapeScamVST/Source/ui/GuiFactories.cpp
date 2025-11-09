#include "GuiFactories.h"

#include "MagicKnobWithOverlay.h"
#include "MagicThreeWaySwitch.h"

namespace tape
{

void registerTapeScamFactories (foleys::MagicGUIBuilder& builder)
{
    builder.registerFactory ("TapeKnob", &MagicKnobWithOverlay::factory);
    builder.registerFactory ("TapeSwitch", &MagicThreeWaySwitch::factory);
}

} // namespace tape
