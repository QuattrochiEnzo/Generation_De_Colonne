#ifndef INSTANCE_HPP
#define INSTANCE_HPP

#include <iostream>
#include <vector>
#include <cmath>
#include <limits>
#include <fstream>
#include "Solutions.hpp"
using namespace std;

struct Client
{
    Point2D pos;
    int demand = 0;
};

struct Facility
{
    Point2D pos;
    int capacity = 0;
};

struct Instance
{
    int C = 0, F = 0, p = 0, U = 0;
    vector<Client> clients;
    vector<Facility> facilities;

    Instance() {}

    Instance(const string &nomf)
    {
        ifstream in(nomf);
        if (!in)
        {
            C = -1; // pour dire que ca a raté
            return;
        }
        in >> *this;
    }

    static double dist(const Point2D &a, const Point2D &b)
    {
        double dx = a.x - b.x;
        double dy = a.y - b.y;
        return sqrt(dx * dx + dy * dy);
    }

    // lecture instance
    friend istream &operator>>(istream &in, Instance &inst)
    {
        in >> inst.C >> inst.F >> inst.p >> inst.U;

        inst.clients.resize(inst.C);
        inst.facilities.resize(inst.F);

        for (int i = 0; i < inst.C; i++)
            in >> inst.clients[i].pos.x >> inst.clients[i].pos.y >> inst.clients[i].demand;

        for (int i = 0; i < inst.F; i++)
            in >> inst.facilities[i].pos.x >> inst.facilities[i].pos.y >> inst.facilities[i].capacity;

        return in;
    }

    // affichage instance
    friend ostream &operator<<(ostream &out, const Instance &inst)
    {
        for (int i = 0; i < inst.C; i++)
        {
            const auto &c = inst.clients[i];
            out << "Client " << i << " : ("
                << c.pos.x << "," << c.pos.y << ") // "
                << c.demand << "\n";
        }

        for (int i = 0; i < inst.F; i++)
        {
            const auto &f = inst.facilities[i];
            out << "Facility " << i << " : ("
                << f.pos.x << "," << f.pos.y << ") // "
                << f.capacity << "\n";
        }

        return out;
    }

    // ppour coller au main
    void write(ostream &out) const
    {
        out << *this;
    }

    // Vérif + valeur sol
    double calculerValeur(const Solution &sol) const
    {
        // taille pas bonne donc direct inf
        if ((int)sol.size() != C)
            return numeric_limits<double>::infinity();

        vector<int> load(F, 0);
        double total = 0.0;

        for (int client = 0; client < C; client++)
        {
            const Point2D &assigned = sol[client];
            int f_idx = -1;

            // matche les coords
            for (int f = 0; f < F; f++)
            {
                if (fabs(assigned.x - facilities[f].pos.x) < 1e-9 &&
                    fabs(assigned.y - facilities[f].pos.y) < 1e-9)
                {
                    f_idx = f;
                    break;
                }
            }

            if (f_idx == -1)
                return numeric_limits<double>::infinity();

            load[f_idx] += clients[client].demand;
            total += dist(clients[client].pos, facilities[f_idx].pos);
        }

        // check capacites
        for (int f = 0; f < F; f++)
            if (load[f] > facilities[f].capacity)
                return numeric_limits<double>::infinity();

        return total;
    }
};

#endif
