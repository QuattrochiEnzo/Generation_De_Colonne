import os
import math
import pandas as pd
import matplotlib.pyplot as plt


RACINE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

CHEMIN_CSV = os.path.join(RACINE, "resultats", "bench.csv")
DOSSIER_SORTIE = os.path.join(RACINE, "resultats", "analyse")
os.makedirs(DOSSIER_SORTIE, exist_ok=True)


def convertir_nombre(x):
    # convert en float sinon NaN
    if x is None:
        return float("nan")

    if isinstance(x, float) or isinstance(x, int):
        return float(x)

    s = str(x).strip().lower()
    if s in ["", "na", "nan", "inf", "+inf", "-inf"]:
        return float("nan")

    try:
        return float(s)
    except:
        return float("nan")


def mediane(liste):
    vals = [v for v in liste if v == v]  # vire NaN
    vals.sort()
    if len(vals) == 0:
        return float("nan")
    n = len(vals)
    if n % 2 == 1:
        return vals[n // 2]
    return 0.5 * (vals[n // 2 - 1] + vals[n // 2])


def ecrire_csv_stats(lignes, chemin):
    # ecrit une liste de dict en csv 
    df_out = pd.DataFrame(lignes)
    df_out.to_csv(chemin, index=False)



# lecture

df = pd.read_csv(CHEMIN_CSV)


df["valeur"] = df["valeur"].apply(convertir_nombre)
df["temps_s"] = df["temps_s"].apply(convertir_nombre)


# stats par methode 
methodes = sorted(df["methode"].dropna().unique().tolist())

stats_lignes = []
for methode in methodes:
    bloc = df[df["methode"] == methode]

    # nb d'instances
    instances = bloc["instance"].dropna().unique().tolist()
    nb_instances = len(instances)

    # nb de lignes
    nb_lignes = len(bloc)

    # nb ok = temps pas NaN
    temps = bloc["temps_s"].tolist()
    nb_ok = sum([1 for t in temps if t == t])

    # valeurs temps
    temps_ok = [t for t in temps if t == t]
    if len(temps_ok) > 0:
        temps_moy = sum(temps_ok) / len(temps_ok)
        temps_min = min(temps_ok)
        temps_max = max(temps_ok)
        temps_med = mediane(temps_ok)
    else:
        temps_moy = float("nan")
        temps_min = float("nan")
        temps_max = float("nan")
        temps_med = float("nan")

    # valeurs obj
    vals = bloc["valeur"].tolist()
    vals_ok = [v for v in vals if v == v]
    if len(vals_ok) > 0:
        val_moy = sum(vals_ok) / len(vals_ok)
        val_med = mediane(vals_ok)
    else:
        val_moy = float("nan")
        val_med = float("nan")

    stats_lignes.append({
        "methode": methode,
        "nb_instances": nb_instances,
        "nb_lignes": nb_lignes,
        "nb_ok": nb_ok,
        "temps_moy": temps_moy,
        "temps_med": temps_med,
        "temps_min": temps_min,
        "temps_max": temps_max,
        "val_moy": val_moy,
        "val_med": val_med,
    })

ecrire_csv_stats(stats_lignes, os.path.join(DOSSIER_SORTIE, "stats_par_methode.csv"))



# tableau temps/valeurs par instance 
# on fait une table "large" a la mano
instances_toutes = sorted(df["instance"].dropna().unique().tolist())

dico_temps = {inst: {} for inst in instances_toutes}
dico_val = {inst: {} for inst in instances_toutes}

for _, ligne in df.iterrows():
    inst_name = ligne["instance"]
    methode = ligne["methode"]
    t = ligne["temps_s"]
    v = ligne["valeur"]

    if pd.isna(inst_name) or pd.isna(methode):
        continue

    # si doublon je garde le premier
    if methode not in dico_temps[inst_name]:
        dico_temps[inst_name][methode] = t
    if methode not in dico_val[inst_name]:
        dico_val[inst_name][methode] = v

colonnes = ["instance"] + methodes

lignes_temps = []
lignes_val = []
for inst_name in instances_toutes:
    lt = {"instance": inst_name}
    lv = {"instance": inst_name}
    for m in methodes:
        lt[m] = dico_temps[inst_name].get(m, float("nan"))
        lv[m] = dico_val[inst_name].get(m, float("nan"))
    lignes_temps.append(lt)
    lignes_val.append(lv)

df_temps = pd.DataFrame(lignes_temps, columns=colonnes)
df_val = pd.DataFrame(lignes_val, columns=colonnes)

df_temps.to_csv(os.path.join(DOSSIER_SORTIE, "temps_par_instance.csv"), index=False)
df_val.to_csv(os.path.join(DOSSIER_SORTIE, "valeurs_par_instance.csv"), index=False)


# plot 1 : boxplot temps
df_box = df[df["temps_s"] == df["temps_s"]].copy()  # vire NaN

plt.figure()
df_box.boxplot(column="temps_s", by="methode")
plt.title("Distribution des temps par methode")
plt.suptitle("")
plt.ylabel("temps (s)")
plt.xticks(rotation=20)
plt.tight_layout()
plt.savefig(os.path.join(DOSSIER_SORTIE, "boxplot_temps.png"))
plt.close()


# graph 2 : courbes temps par instance
plt.figure()

for methode in methodes:
    bloc = df_box[df_box["methode"] == methode].copy()
    bloc = bloc.sort_values("instance")

    plt.plot(bloc["instance"], bloc["temps_s"], marker="o", linestyle="-", label=methode)

plt.title("Temps par instance")
plt.ylabel("temps (s)")
plt.xticks(rotation=90)
plt.legend()
plt.tight_layout()
plt.savefig(os.path.join(DOSSIER_SORTIE, "temps_par_instance.png"))
plt.close()


# gap (dp vs stab) si present
if ("colgen_stab" in df_val.columns) and ("colgen_dp" in df_val.columns):
    lignes_gap = []
    for _, row in df_val.iterrows():
        inst_name = row["instance"]
        v_stab = row["colgen_stab"]
        v_dp = row["colgen_dp"]

        if (v_stab == v_stab) and (v_dp == v_dp) and abs(v_stab) > 1e-12:
            gap = (v_dp - v_stab) / v_stab
        else:
            gap = float("nan")

        lignes_gap.append({"instance": inst_name, "gap_dp_vs_stab": gap})

    pd.DataFrame(lignes_gap).to_csv(os.path.join(DOSSIER_SORTIE, "gap_dp_vs_stab.csv"), index=False)

print("fini, check resultats/analyse/")
