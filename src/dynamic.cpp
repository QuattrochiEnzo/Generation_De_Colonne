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
vector<ColonnePmed> pricing_depot_dp(
    const Instance &inst,
    int depotIdx,
    const vector<double> &dualCli,
    double dualLimite)
{
    const int C = inst.C;
    const int CAP = inst.facilities[depotIdx].capacity;
    const double INF = 1e100;
    const double EPS = 1e-6;

    vector<int> poids(C);
    vector<double> coutMod(C);

    for (int c = 0; c < C; c++)
    {
        poids[c] = inst.clients[c].demand;
        double dist = Instance::dist(inst.clients[c].pos,
                                     inst.facilities[depotIdx].pos);
        coutMod[c] = dist - dualCli[c];
    }

    vector<double> dp(CAP + 1, INF);
    vector<vector<int>> parent(CAP + 1);

    dp[0] = 0.0;

    for (int c = 0; c < C; c++)
    {
        int w = poids[c];
        if (w > CAP)
            continue;

        for (int W = CAP; W >= w; W--)
        {
            if (dp[W - w] >= INF)
                continue;

            double cand = dp[W - w] + coutMod[c];
            if (cand < dp[W])
            {
                dp[W] = cand;
                parent[W] = parent[W - w];
                parent[W].push_back(c);
            }
        }
    }

    double bestVal = INF;
    for (int W = 0; W <= CAP; W++)
        bestVal = min(bestVal, dp[W]);

    vector<ColonnePmed> colonnes;

    for (int W = 0; W <= CAP; W++)
    {
        if (dp[W] <= bestVal + EPS)
        {
            double rc = dp[W] - dualLimite;
            if (rc >= -1e-6)
                continue;

            ColonnePmed col;
            col.depotChoisi = depotIdx;
            col.clientsPris = parent[W];

            if (col.clientsPris.empty())
                continue;

            col.coutCol = 0.0;
            for (int c : col.clientsPris)
                col.coutCol += Instance::dist(
                    inst.clients[c].pos,
                    inst.facilities[depotIdx].pos);

            colonnes.push_back(col);
        }
    }

    return colonnes;
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
            vector<ColonnePmed> cols =
                pricing_depot_dp(inst, f, dcli, d_limite);

            for (const ColonnePmed &col : cols)
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
