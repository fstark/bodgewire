#pragma once

#include "board/board.hpp"

// Board geometry on whole nm (Points). copper.cpp keeps its own, on unrounded doubles: its pad
// hulls are rotated corners that rounding would move.

namespace bw {

double Distance(Point a, Point b);  // nm

// The point of segment a-b nearest to `p`, as t from 0 (a) to 1 (b); 0 when a == b.
double NearestT(Point p, Point a, Point b);
// That point, rounded to the nm.
Point NearestOnSegment(Point p, Point a, Point b);
// nm from `p` to segment a-b.
double SegmentDistance(Point p, Point a, Point b);

// The offset (x, y) nm of a footprint or pad turned by `degrees` the KiCad way (y down, positive
// counter-clockwise on screen), from `origin`; rounded to the nm.
Point RotatedOffset(Point origin, double x, double y, double degrees);

}  // namespace bw
