#ifndef SOLUTIONS_HPP
#define SOLUTIONS_HPP

#include <iostream>
#include <vector>
using namespace std;

struct Point2D
{
    double x = 0.0, y = 0.0;
};

using Solution = vector<Point2D>;

inline ostream &operator<<(ostream &out, const Solution &sol)
{
    // affiche juste coord
    for (const auto &p : sol)
        out << p.x << " " << p.y << "\n";
    return out;
}

inline istream &operator>>(istream &in, Solution &sol)
{
    // lit dedans si taille ok
    for (auto &p : sol)
        in >> p.x >> p.y;

    return in;
}

#endif
