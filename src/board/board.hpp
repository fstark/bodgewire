#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace bw {

// Lengths in nanometres (KiCad's internal unit); angles in degrees, counter-clockwise on screen.
enum class Layer : std::uint8_t { Front, Back };  // F.Cu, B.Cu
enum class PadShape : std::uint8_t { Circle, Oval, Rectangle, RoundedRectangle };

struct Point {
    std::int64_t x;
    std::int64_t y;  // grows downwards
    bool operator==(const Point&) const = default;
};

struct Pad {
    std::string number;         // Pin index + 1
    Point position;             // absolute
    double angle;               // absolute
    Point size;                 // before rotation
    std::int64_t cornerRadius;  // RoundedRectangle only
    std::int64_t drill;
    PadShape shape;
};

// Through-hole only: every pad is on both Layers.
struct BoardComponent {
    std::uint32_t line;  // in the source file
    std::string reference;
    std::string footprint;
    Point position;
    double angle;
    Layer side;
    std::vector<Pad> pads;
    std::vector<Point> courtyard;  // F.CrtYd/B.CrtYd fp_rect or fp_poly, absolute; empty if none
};

struct Trace {
    Layer layer;
    Point start;
    Point end;
    std::optional<Point> mid;  // set for an arc
    std::int64_t width;
};

struct Via {
    Point position;
    std::int64_t diameter;
    std::int64_t drill;
};

struct OutlineEdge {
    Point start;
    Point end;
    std::optional<Point> mid;  // set for an arc
};

struct Board {
    std::vector<BoardComponent> components;
    std::vector<Trace> traces;
    std::vector<Via> vias;
    std::vector<OutlineEdge> outline;
};

}  // namespace bw
