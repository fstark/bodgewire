#include "view/shelf_layout.hpp"

namespace bw {

ShelfPlaces LayOutShelf(std::span<const ShelfCell> items, float width,
                        const ShelfMetrics& metrics) {
    const auto widthOf = [&](const ShelfCell& item) {
        return metrics.cell * item.cells + metrics.cellGap * (item.cells - 1);
    };
    const float apartGap = metrics.groupGap / 2;
    float right = 0;
    for (const ShelfCell& item : items) {
        if (item.right) {
            right += widthOf(item) + (item.apart ? apartGap : metrics.cellGap);
        }
    }
    right -= metrics.cellGap;
    ShelfPlaces places;
    places.x.resize(items.size());
    float x = metrics.groupGap / 2;
    float rightX = width - metrics.groupGap / 2 - right;
    places.rightStart = rightX;
    places.leftEnd = x;
    for (std::size_t i = 0; i < items.size(); ++i) {
        const ShelfCell& item = items[i];
        if (i > 0 && items[i - 1].tray != item.tray && !item.right) {
            x += metrics.groupGap - metrics.cellGap;
        }
        if (item.right) {
            if (item.apart) {
                rightX += apartGap - metrics.cellGap;
            }
            places.x[i] = rightX;
            rightX += widthOf(item) + metrics.cellGap;
        } else {
            places.x[i] = x;
            x += widthOf(item) + metrics.cellGap;
            places.leftEnd = x - metrics.cellGap;
        }
    }
    return places;
}

float ShelfWidth(std::span<const ShelfCell> items, const ShelfMetrics& metrics) {
    // Laid out on no width: the right-hand tray starts at minus its width and its half gap.
    const ShelfPlaces places = LayOutShelf(items, 0, metrics);
    return places.leftEnd + metrics.groupGap - places.rightStart;
}

const ShelfMetrics& ShelfMetricsFor(std::span<const ShelfCell> items, float width) {
    return ShelfWidth(items, kShelfMetrics) <= width ? kShelfMetrics : kShelfNarrow;
}

}  // namespace bw
