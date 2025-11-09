#pragma once

#include <juce_graphics/juce_graphics.h>
#include "BinaryData.h"

namespace tape
{

inline juce::Image loadBinaryImage (const juce::String& name)
{
    if (name.isEmpty())
        return {};

    juce::String resourceName (name);
    resourceName = resourceName.replaceCharacter ('.', '_');

    int dataSize = 0;
    if (const auto* data = BinaryData::getNamedResource (resourceName.toRawUTF8(), dataSize))
        return juce::ImageCache::getFromMemory (data, dataSize);

    if (const auto* data = BinaryData::getNamedResource (name.toRawUTF8(), dataSize))
        return juce::ImageCache::getFromMemory (data, dataSize);

    return {};
}

inline juce::StringArray getBinaryAssetNames()
{
    juce::StringArray files;
    for (int i = 0; i < BinaryData::namedResourceListSize; ++i)
        if (const auto* original = BinaryData::getNamedResourceOriginalFilename (BinaryData::namedResourceList[i]))
            files.add (original);

    files.removeDuplicates (true);
    return files;
}

} // namespace tape
