#include <iostream>
#include <vector>
#include <cmath>
#include "gurobi_c++.h"
#include "Instance.hpp"
#include <ctime>
using namespace std;

// colonne = depot + des clients
struct Colonne
{
    int indiceFacility;
    vector<int> indicesClients;
    double coutTotal;
};

// maitre restreint
struct MaitreRestreint
{
    GRBEnv &environnement;
    GRBModel modele;
    const Instance &instance;

    vector<GRBConstr> contrainteCouverture;
    GRBConstr contrainteNbDepots;
    vector<GRBVar> variables;

    MaitreRestreint(const Instance &inst, GRBEnv &env)
        : environnement(env), modele(env), instance(inst)
    {
        // colonnes artificielles pour demarrer
        double coutArtif = 1e6;
        for (int indiceClient = 0; indiceClient < instance.C; indiceClient++)
        {
            string nomVar = "lambda_art_" + to_string(indiceClient);
            GRBVar lambda = modele.addVar(0.0, GRB_INFINITY, coutArtif,
                                          GRB_CONTINUOUS, nomVar);
            variables.push_back(lambda);

            string nomContr = "couv_" + to_string(indiceClient);
            contrainteCouverture.push_back(modele.addConstr(lambda == 1.0, nomContr));
        }

        contrainteNbDepots = modele.addConstr(0.0, GRB_LESS_EQUAL, instance.p, "limite_p");
    }

    void ajouterColonne(const Colonne &col)
    {
        GRBColumn nouvelleCol;

        // couvre des clients
        for (int ic : col.indicesClients)
            nouvelleCol.addTerm(1.0, contrainteCouverture[ic]);

        // consomme un depot
        nouvelleCol.addTerm(1.0, contrainteNbDepots);

        GRBVar lambda = modele.addVar(0.0, GRB_INFINITY, col.coutTotal,
                                      GRB_CONTINUOUS, nouvelleCol);
        variables.push_back(lambda);
    }

    void resoudre()
    {
        modele.optimize();
    }

    vector<double> obtenirDuauxClients()
    {
        vector<double> duaux(instance.C);
        for (int ic = 0; ic < instance.C; ic++)
            duaux[ic] = contrainteCouverture[ic].get(GRB_DoubleAttr_Pi);
        return duaux;
    }

    double obtenirDualDepots()
    {
        return contrainteNbDepots.get(GRB_DoubleAttr_Pi);
    }

    double obtenirValeurObjectif() const
    {
        return modele.get(GRB_DoubleAttr_ObjVal);
    }
};

// pricing pour un depot
Colonne resoudrePricing(const Instance &instance, int indiceFacility,
                        const vector<double> &duauxClients, GRBEnv &env)
{
    GRBModel sousPb(env);
    sousPb.set(GRB_IntParam_LogToConsole, 0);

    vector<GRBVar> varSelect;
    GRBLinExpr capa = 0.0;

    for (int ic = 0; ic < instance.C; ic++)
    {
        double dist = Instance::dist(instance.clients[ic].pos,
                                     instance.facilities[indiceFacility].pos);

        double coutReduit = dist - duauxClients[ic];

        string nomV = "x_" + to_string(ic);
        GRBVar var = sousPb.addVar(0.0, 1.0, coutReduit, GRB_BINARY, nomV);
        varSelect.push_back(var);

        capa += instance.clients[ic].demand * var;
    }

    sousPb.addConstr(capa <= instance.facilities[indiceFacility].capacity);
    sousPb.optimize();

    Colonne res;
    res.indiceFacility = indiceFacility;
    res.coutTotal = 0.0;

    int status = sousPb.get(GRB_IntAttr_Status);
    if (status != GRB_OPTIMAL)
    {
        res.coutTotal = 1e100;
        return res;
    }

    // recuperer solution
    for (int ic = 0; ic < instance.C; ic++)
    {
        double val = varSelect[ic].get(GRB_DoubleAttr_X);
        if (val > 0.5)
        {
            res.indicesClients.push_back(ic);

            double dist = Instance::dist(instance.clients[ic].pos,
                                         instance.facilities[indiceFacility].pos);
            res.coutTotal += dist;
        }
    }

    if (res.indicesClients.empty())
        res.coutTotal = 1e100;

    return res;
}

double calculerCoutReduit(const Colonne &col, const vector<double> &duauxCli,
                          double dualDep)
{
    double rc = col.coutTotal - dualDep;
    for (int ic : col.indicesClients)
        rc -= duauxCli[ic];
    return rc;
}

double calculerBorneLag(const Instance &instance, const vector<double> &duauxCli,
                        double dualDep)
{
    double borne = 0.0;
    for (int ic = 0; ic < instance.C; ic++)
        borne += duauxCli[ic];
    borne += instance.p * dualDep;
    return borne;
}

void generationColonnesStabilisee(const Instance &instance, GRBEnv &env)
{
    MaitreRestreint maitre(instance, env);

    // param in-out separation
    double alpha = 0.5;
    double tolerance = 1e-6;

    // centre y_chapeau
    vector<double> centreCli(instance.C, 0.0);
    double centreDep = 0.0;

    double meilleureBorne = -1e100;

    bool continuer = true;
    int iter = 0;

    while (continuer)
    {
        iter++;
        maitre.resoudre();

        int status = maitre.modele.get(GRB_IntAttr_Status);
        if (status != GRB_OPTIMAL)
        {
            cerr << "pb maitre\n";
            break;
        }

        // duaux actuels y_barre
        vector<double> duauxActuelsCli = maitre.obtenirDuauxClients();
        double dualActuelDep = maitre.obtenirDualDepots();

        // duaux modifies = alpha*y_chapeau + (1-alpha)*y_barre
        vector<double> duauxModifCli(instance.C);
        for (int ic = 0; ic < instance.C; ic++)
            duauxModifCli[ic] = alpha * centreCli[ic] + (1.0 - alpha) * duauxActuelsCli[ic];
        double dualModifDep = alpha * centreDep + (1.0 - alpha) * dualActuelDep;

        // borne lag avec duaux modif
        double borneMod = calculerBorneLag(instance, duauxModifCli, dualModifDep);

        continuer = false;
        int nbAjout = 0;

        // pricing avec duaux modifies
        for (int ifac = 0; ifac < instance.F; ifac++)
        {
            Colonne col = resoudrePricing(instance, ifac, duauxModifCli, env);

            if (col.coutTotal > 1e50)
                continue;

            double rcActuel = calculerCoutReduit(col, duauxActuelsCli, dualActuelDep);

            if (rcActuel < -tolerance)
            {
                maitre.ajouterColonne(col);
                nbAjout++;
                continuer = true;
            }
        }

        if (nbAjout == 0)
        {
            if (borneMod > meilleureBorne + tolerance)
            {
                meilleureBorne = borneMod;
                centreCli = duauxModifCli;
                centreDep = dualModifDep;

                continuer = true;
            }
        }
    }

    maitre.resoudre();
    cout << "\nValeur LP finale: " << maitre.obtenirValeurObjectif() << "\n";
}

int main(int argc, char **argv)
{
    try
    {
        double tDebut = clock();

        string nomInst = "../instances/sample.inst";
        if (argc > 1)
            nomInst = string("../instances/") + argv[1];

        Instance instance(nomInst);
        if (instance.C <= 0 || instance.F <= 0)
        {
            cerr << "err instance\n";
            return 1;
        }

        GRBEnv env(true);
        env.set(GRB_IntParam_LogToConsole, 0);
        env.start();

        generationColonnesStabilisee(instance, env);

        double tFin = clock();
        double temps = (tFin - tDebut) / CLOCKS_PER_SEC;
        cout << "Temps: " << temps << " s\n";
    }
    catch (GRBException &e)
    {
        cerr << "exception gurobi: " << e.getMessage() << "\n";
        return 1;
    }
    catch (exception &e)
    {
        cerr << "exception: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
