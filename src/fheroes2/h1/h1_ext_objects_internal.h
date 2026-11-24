#pragma once

// H1-INTENDED(D16): extension object definitions shared by h1_ext_objects.cpp (pictures) and h1_visit.cpp (placing, visits).

#include <vector>

namespace Maps
{
    struct ObjectInfo;
}

namespace H1
{
    struct ExtObjectDef
    {
        int kind = 0;
        const char * name = "";
        const Maps::ObjectInfo * info = nullptr; // the HoMM2 object (fheroes2 map_object_info.cpp); nullptr if not found
        std::vector<int> groundPart; // part numbers (tile byte frame, set 15) of info->groundLevelParts, same order
        std::vector<int> topPart; // of info->topLevelParts
    };

    const std::vector<ExtObjectDef> & extensionObjectDefinitions();
}
