#include <iostream>
#include <vector>
#include <cmath>
#include "gurobi_c++.h"
#include "Instance.hpp"
#include <ctime>
using namespace std;

// colonne = un depot + des clients
struct ColonnePmed
{
    int depotChoisi = -1;
    vector<int> clientsPris;
    double coutCol = 0.0;
};

// modele maitre )
struct ModeleMaitre
{
    GRBEnv &env;
    GRBModel model;

    const Instance &inst;

    vector<GRBConstr> contrClients; // une par client
    GRBConstr limite;
    vector<GRBVar> lambdas; // une var par colonne

    ModeleMaitre(const Instance &instance, GRBEnv &env_)
        : env(env_), model(env_), inst(instance)
    {
        // contr couverture: pour chaque client, sum lambdas == 1
        double bigM = 1e6;
        for (int clientIdx = 0; clientIdx < inst.C; clientIdx++)
        {
            string name = "lambda_init_client_" + to_string(clientIdx);
            GRBVar lambda = model.addVar(0.0, GRB_INFINITY, bigM, GRB_CONTINUOUS, name);
            lambdas.push_back(lambda);
            string nom = "couv_" + to_string(clientIdx);
            contrClients.push_back(model.addConstr(lambda == 1.0, nom));
        }

        limite = model.addConstr(0.0, GRB_LESS_EQUAL, inst.p, "limite_p");
    }


    void ajouterColonne(const ColonnePmed &col)
    {
        // // j ajoute lambda_j dans le modele

        GRBColumn colonne;

    
        for (int cli : col.clientsPris)
        {
            colonne.addTerm(1.0, contrClients[cli]);
        }
        colonne.addTerm(1.0, limite);

        // var continue avec cout = coutCol
        GRBVar lambda = model.addVar(
            0.0, GRB_INFINITY,
            col.coutCol,
            GRB_CONTINUOUS,
            colonne);

        lambdas.push_back(lambda);
    }

    void optimiser()
    {
        model.optimize();
    }

    // dual des ctes clients
    vector<double> dualClients()
    {
        vector<double> d(inst.C);
        for (int clientIdx = 0; clientIdx < inst.C; clientIdx++)
        {
            d[clientIdx] = contrClients[clientIdx].get(GRB_DoubleAttr_Pi);
        }
        return d;
    }

    double valeurObj() const
    {
        return model.get(GRB_DoubleAttr_ObjVal);
    }
};

// pricing pour un depot f
ColonnePmed pricing_depot(
    const Instance &inst,
    int depotIdx,
    const vector<double> &dualCli,
    double limite,
    GRBEnv &env)
{
    // // pricing pour ce depot

    GRBModel mip(env);
    mip.set(GRB_IntParam_LogToConsole, 0);

    vector<GRBVar> varsClientPris;
    GRBLinExpr contrCapacite = 0.0;

    for (int clientIdx = 0; clientIdx < inst.C; clientIdx++)
    {
        double distcf = Instance::dist(inst.clients[clientIdx].pos,
                                       inst.facilities[depotIdx].pos);

        double coeffZ = distcf - dualCli[clientIdx];

        string nom = "z_" + to_string(clientIdx);
        varsClientPris.push_back(
            mip.addVar(0.0, 1.0, coeffZ, GRB_BINARY, nom));

        contrCapacite += inst.clients[clientIdx].demand * varsClientPris.back();
    }

    mip.addConstr(contrCapacite <= inst.facilities[depotIdx].capacity, "cap_depot");

    mip.optimize();

    ColonnePmed col;
    col.depotChoisi = depotIdx;

    int status = mip.get(GRB_IntAttr_Status);
    if (status != GRB_OPTIMAL)
    {
        col.coutCol = 1e100;
        return col;
    }

    double coutReduit = mip.get(GRB_DoubleAttr_ObjVal);


    if (coutReduit >= limite - 1e-6)
    {
        col.coutCol = 1e100;
        return col;
    }
    // cout << "cout reduit calculé : " << coutReduit << endl;


    col.coutCol = 0.0;
    for (int clientIdx = 0; clientIdx < inst.C; clientIdx++)
    {
        if (varsClientPris[clientIdx].get(GRB_DoubleAttr_X) > 0.5)
        {
            col.clientsPris.push_back(clientIdx);

            double d = Instance::dist(inst.clients[clientIdx].pos,
                                      inst.facilities[depotIdx].pos);
            col.coutCol += d;
        }
    }
    return col;
}

// boucle colgen
void generation_colonnes(const Instance &inst, GRBEnv &env)
{

    ModeleMaitre master(inst, env);

    bool encore = true;
    int iteration = 0;

    while (encore)
    {
        iteration++;

        master.optimiser();
        // cout << "=== itération " << iteration << " ===\n";
        // cout << "valeur LP master : " << master.valeurObj() << "\n";

        int status = master.model.get(GRB_IntAttr_Status);
        if (status != GRB_OPTIMAL)
        {
            cerr << "master pas optimal, status = " << status << "\n";
            break; // cout << "On ajoute la colonne ";
                   // for (int i = 0; i < col.clientsPris.size(); i++)
                   // {
                   //     cout << col.clientsPris[i];
                   // }
                   // cout << endl;
        }

        vector<double> dcli = master.dualClients();
        double d_limite = master.limite.get(GRB_DoubleAttr_Pi);

        encore = false;

        // pricing pour chaque depot
        ColonnePmed best_col;
        for (int f = 0; f < inst.F; f++)
        {
            ColonnePmed col = pricing_depot(inst, f, dcli, d_limite, env);
            if (col.coutCol < 1e50)
            {
                master.ajouterColonne(col);

                encore = true;
            }
        }
    }

    master.optimiser();
    cout << "valeur LP colgen = " << master.valeurObj() << "\n";
}

int main(int argc, char **argv)
{
    try
    {
        double temps_initial = clock();
        string nomInstance = "../instances/sample.inst";
        if (argc > 1)
            nomInstance = string("../instances/") + argv[1];

        Instance inst(nomInstance);
        if (inst.C <= 0 || inst.F <= 0)
        {
            cerr << "err : instance pas lue ou vide\n";
            return 1;
        }

        GRBEnv env(true);
        env.set(GRB_IntParam_LogToConsole, 0);
        env.start();

        generation_colonnes(inst, env);
        double temps_final = clock();
        double temps_cpu = (temps_final - temps_initial) / CLOCKS_PER_SEC;
        cout << "Runtime = " << temps_cpu << "\n";
    }
    catch (GRBException &e)
    {
        cerr << "gurobi exception : " << e.getMessage()
             << " (code " << e.getErrorCode() << ")\n";
        return 1;
    }
    catch (exception &e)
    {
        cerr << "exception : " << e.what() << "\n";
        return 1;
    }

    return 0;
}
