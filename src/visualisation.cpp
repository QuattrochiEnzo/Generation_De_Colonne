#include "Instance.hpp"
#include <fstream>
#include <iostream>
#include <vector>
using namespace std;

int main(int argc, char **argv)
{
    // noms par defaut
    string nomInstance = "../instances/sample.inst";
    string nomSolution = "../solutions/sample.sol";
    string nomSvg = "sortie.svg";

    if (argc > 1)
        nomInstance = string("../instances/") + argv[1];
    if (argc > 2)
        nomSolution = string("../solutions/") + argv[2];
    if (argc > 3)
        nomSvg = argv[3];

    // lire instance
    Instance inst(nomInstance);
    if (inst.C <= 0 || inst.F <= 0)
    {
        cerr << "err : instance pas lue\n";
        return 1;
    }

    // lire solution
    Solution sol(inst.C);
    ifstream inSol(nomSolution);
    if (!inSol)
    {
        cerr << "err : impossible ouvrir " << nomSolution << "\n";
        return 1;
    }
    inSol >> sol;

    // check valeur 
    double val = inst.calculerValeur(sol);
    cout << "valeur solution = " << val << "\n";

    ofstream svg(nomSvg);
    if (!svg)
    {
        cerr << "err : impossible creer " << nomSvg << "\n";
        return 1;
    }

    int largeur = 800;
    int hauteur = 800;
    double marge = 20.0;
    double echelle = largeur - 2 * marge;

    svg << "<svg xmlns=\"http://www.w3.org/2000/svg\" version=\"1.1\" "
        << "width=\"" << largeur << "\" height=\"" << hauteur << "\">\n";

    // trouvé les facilities ouvertes (celles qui servent au moins un client)
    vector<int> ouvert(inst.F, 0);
    vector<int> affect(inst.C, -1);

    for (int c = 0; c < inst.C; c++)
    {
        const Point2D &pf = sol[c];
        int idx = -1;
        for (int f = 0; f < inst.F; f++)
        {
            if (fabs(pf.x - inst.facilities[f].pos.x) < 1e-9 &&
                fabs(pf.y - inst.facilities[f].pos.y) < 1e-9)
            {
                idx = f;
                break;
            }
        }
        affect[c] = idx;
        if (idx >= 0)
            ouvert[idx] = 1;
    }

    // dessiner segments client/facility
    for (int c = 0; c < inst.C; c++)
    {
        int f = affect[c];
        if (f < 0)
            continue;

        double cx = marge + inst.clients[c].pos.x * echelle;
        double cy = marge + (1.0 - inst.clients[c].pos.y) * echelle;
        double fx = marge + inst.facilities[f].pos.x * echelle;
        double fy = marge + (1.0 - inst.facilities[f].pos.y) * echelle;

        svg << "<line x1=\"" << cx << "\" y1=\"" << cy
            << "\" x2=\"" << fx << "\" y2=\"" << fy
            << "\" stroke=\"green\" opacity=\"0.3\" stroke-width=\"1\" />\n";
    }

    // dessiner clients (petits cercles bleus)
    for (int c = 0; c < inst.C; c++)
    {
        double cx = marge + inst.clients[c].pos.x * echelle;
        double cy = marge + (1.0 - inst.clients[c].pos.y) * echelle;

        svg << "<circle cx=\"" << cx << "\" cy=\"" << cy
            << "\" r=\"3\" fill=\"blue\" stroke=\"black\" stroke-width=\"1\" />\n";
    }

    // dessiner facilities ouvertes (carrés rouges)
    for (int f = 0; f < inst.F; f++)
    {
        double fx = marge + inst.facilities[f].pos.x * echelle;
        double fy = marge + (1.0 - inst.facilities[f].pos.y) * echelle;

        if (ouvert[f])
        {
            double r = 5.0;
            svg << "<rect x=\"" << (fx - r) << "\" y=\"" << (fy - r)
                << "\" width=\"" << (2 * r) << "\" height=\"" << (2 * r)
                << "\" fill=\"red\" stroke=\"black\" stroke-width=\"1\" />\n";
        }
        else
        {
            // facilities fermees en gris
            double r = 3.0;
            svg << "<rect x=\"" << (fx - r) << "\" y=\"" << (fy - r)
                << "\" width=\"" << (2 * r) << "\" height=\"" << (2 * r)
                << "\" fill=\"lightgray\" stroke=\"black\" stroke-width=\"1\" />\n";
        }
    }

    svg << "</svg>\n";
    svg.close();

    cout << "svg ecrit dans " << nomSvg << "\n";
    return 0;
}
