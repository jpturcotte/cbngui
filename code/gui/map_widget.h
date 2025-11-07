#ifndef MAP_WIDGET_H
#define MAP_WIDGET_H

#include <vector>
#include <string>
#include "imgui.h"

struct Tile {
    char character;
    ImVec4 color;
};

class MapWidget {
public:
    MapWidget();
    ~MapWidget();

    void Draw();
private:
    std::vector<std::vector<Tile>> mock_map_;
};

#endif // MAP_WIDGET_H
