#ifndef MAP_WIDGET_H
#define MAP_WIDGET_H

#include <vector>
#include <string>

class MapWidget {
public:
    MapWidget();
    ~MapWidget();

    void Draw();
private:
    std::vector<std::string> mock_map_;
};

#endif // MAP_WIDGET_H
