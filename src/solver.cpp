#include <iostream>
#include <vector>
#include <cmath>
using namespace std;

#include "gurobi_c++.h"
#include "Instance.hpp"

int main(int argc, char **argv)
{
    bool relax = false;
    string nomInstance = "../instances/sample.inst";
    if (argc > 1)
        nomInstance = string("../instances/") + argv[1];

    Instance inst(nomInstance);
    if (inst.C <= 0 || inst.F <= 0)
    {
        cerr << "err : instance pas lue ou vide\n";
        return 1;
    }

    try
    {
        GRBEnv env(true);
        env.set(GRB_IntParam_LogToConsole, 1);
        env.start();

        GRBModel model(env);

        // vars y_f : facility ouverte ou pas
        vector<GRBVar> y(inst.F);
        if (relax)
        {
            for (int f = 0; f < inst.F; f++)
            {
                string nomVar = "y_" + to_string(f);
                y[f] = model.addVar(0.0, 1.0, 0.0, GRB_CONTINUOUS, nomVar);
            }
        }
        else
        {
            for (int f = 0; f < inst.F; f++)
            {
                string nomVar = "y_" + to_string(f);
                y[f] = model.addVar(0.0, 1.0, 0.0, GRB_BINARY, nomVar);
            }
        }

        // vars x_cf : client c affecte a facility f
        vector<vector<GRBVar>> x(inst.C, vector<GRBVar>(inst.F));
        if (relax)
        {
            for (int c = 0; c < inst.C; c++)
            {
                for (int f = 0; f < inst.F; f++)
                {
                    double cout_cf = Instance::dist(inst.clients[c].pos, inst.facilities[f].pos);
                    string nomVar = "x_" + to_string(c) + "_" + to_string(f);
                    x[c][f] = model.addVar(0.0, 1.0, cout_cf, GRB_CONTINUOUS, nomVar);
                }
            }
        }
        else
        {
            for (int c = 0; c < inst.C; c++)
            {
                for (int f = 0; f < inst.F; f++)
                {
                    double cout_cf = Instance::dist(inst.clients[c].pos, inst.facilities[f].pos);
                    string nomVar = "x_" + to_string(c) + "_" + to_string(f);
                    x[c][f] = model.addVar(0.0, 1.0, cout_cf, GRB_BINARY, nomVar);
                }
            }
        }

        // chaque client affecte a 1 facility
        for (int c = 0; c < inst.C; c++)
        {
            GRBLinExpr somme = 0.0;
            for (int f = 0; f < inst.F; f++)
                somme += x[c][f];

            string nomCtr = "affect_client_" + to_string(c);
            model.addConstr(somme == 1.0, nomCtr);
        }

        // capacite des facilities
        for (int f = 0; f < inst.F; f++)
        {
            GRBLinExpr charge = 0.0;
            for (int c = 0; c < inst.C; c++)
                charge += inst.clients[c].demand * x[c][f];

            string nomCtr = "cap_facility_" + to_string(f);
            model.addConstr(charge <= inst.facilities[f].capacity * y[f], nomCtr);
        }

        // on ouvre exactement p facilities
        GRBLinExpr nbOuvert = 0.0;
        for (int f = 0; f < inst.F; f++)
            nbOuvert += y[f];

        model.addConstr(nbOuvert == inst.p, "nb_facilities_ouvertes");

        model.set(GRB_IntAttr_ModelSense, GRB_MINIMIZE);

        model.optimize();

        int status = model.get(GRB_IntAttr_Status);
        if (status != GRB_OPTIMAL)
        {
            cerr << "model pas optimal, status = " << status << "\n";
            return 1;
        }
        double obj = model.get(GRB_DoubleAttr_ObjVal);

        cout << "valeur obj gurobi = " << obj << "\n";

        // construire solution en coordonnees de facilities
        Solution sol(inst.C);
        for (int c = 0; c < inst.C; c++)
        {
            int facilityChoisie = -1;

            for (int f = 0; f < inst.F; f++)
            {
                double val = x[c][f].get(GRB_DoubleAttr_X);
                if (val > 0.5)
                {
                    facilityChoisie = f;
                    break;
                }
            }

            if (facilityChoisie == -1)
            {
                cerr << "client " << c << " sans facility choisie\n";
                sol[c].x = 0.0;
                sol[c].y = 0.0;
            }
            else
            {
                sol[c] = inst.facilities[facilityChoisie].pos;
            }
        }

        // affichage de la solution
        cout << "solution (coords facility pour chaque client):\n";
        cout << sol;

        double valVerif = inst.calculerValeur(sol);
        cout << "valeur solution (calculerValeur) = " << valVerif << "\n";
        cout << "Runtime: " << model.get(GRB_DoubleAttr_Runtime) << " seconds" << endl;
    }
    catch (GRBException &e)
    {
        cerr << "gurobi exception : " << e.getMessage() << "\n";
        return 1;
    }
    catch (exception &e)
    {
        cerr << "exception : " << e.what() << "\n";
        return 1;
    }

    return 0;
}
