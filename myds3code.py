from typing import TypedDict
import time
Item = TypedDict("Item", {'name': str, 'defense': float, 'weight': float})

count = 0


def calcular(elmos: list[Item], peitorais: list[Item], luvas: list[Item], calcas: list[Item]):
    begin = time.time()
    melhor_elmo = {"name": "N/A", "defense": 1, "weight": 1}
    melhor_peitoral = {"name": "N/A", "defense": 1, "weight": 1}
    melhor_luva = {"name": "N/A", "defense": 1, "weight": 1}
    melhor_calca = {"name": "N/A", "defense": 1, "weight": 1}

    soma_def = 0
    soma_peso = 0
    capacidade = 60

    for i in range(len(elmos)):
        ii = elmos[i]["defense"]
        for n in range(len(peitorais)):
            nn = peitorais[n]["defense"]
            for c in range(len(calcas)):
                cc = calcas[c]["defense"]
                for b in range(len(luvas)):
                    bb = luvas[b]["defense"]
                    new_soma_defesa = ii + nn + cc + bb
                    peso = elmos[i]["weight"] + peitorais[n]["weight"] + \
                        calcas[c]["weight"] + luvas[b]["weight"]
                    if new_soma_defesa >= soma_def and peso <= capacidade:
                        soma_def = new_soma_defesa
                        soma_peso = peso
                        melhor_elmo = elmos[i]
                        melhor_peitoral = peitorais[n]
                        melhor_calca = calcas[c]
                        melhor_luva = luvas[b]

    # for elmo in elmos:
    #     if elmo["defense"] >= melhor_elmo["defense"]:
    #         melhor_elmo = elmo

    # for peitoral in peitorais:
    #     if peitoral["defense"] >= melhor_peitoral["defense"]:
    #         melhor_peitoral = peitoral

    # for calca in calcas:
    #     if calca["defense"] >= melhor_calca["defense"]:
    #         melhor_calca = calca

    # for luva in luvas:
    #     if luva["defense"] >= melhor_luva["defense"]:
    #         melhor_luva = luva
    end = time.time()
    duration = end - begin
    print("Python versio took: {}".format(duration))
    return (melhor_elmo, melhor_peitoral, melhor_luva, melhor_calca)
