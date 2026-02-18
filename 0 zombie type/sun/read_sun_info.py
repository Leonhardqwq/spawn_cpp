import pandas as pd
import json

file_name = 'sun_info.xlsx'

###
info_read = pd.read_excel(file_name, usecols='B', skiprows=0, nrows=21, header=None)
info_list = info_read.iloc[:,0].tolist()

tmp = info_list[0]
tmp_dict = {"D": 0, "N": 1, "P": 2, "R": 3}
scene = tmp_dict[tmp[0]]

tmp = info_list[2]
neg_value = int(tmp[2:])

tmp = info_list[3]
density_threshold = int(tmp)

tmp = info_list[5]
user_id = int(tmp)

tmp = info_list[6]
mode_id = int(tmp)

tmp = info_list[7]
level_start = int(tmp) // 2

tmp = info_list[8]
level_num = int(tmp)

tmp = info_list[9]
sun_start = float(tmp)

tmp = info_list[10]
sun_end = float(tmp)

tmp = info_list[11]
sun_cap = tmp != '是'

tmp = info_list[13]
mode = int(tmp[0])

tmp = info_list[16]
mode1_sim_num = int(tmp)

tmp = info_list[17]
mode1_output_num = int(tmp)

tmp = info_list[20]
mode2_seed = int(tmp, 16)

###
factor_name_list = [
    "路障", "撑杆", "铁桶", "读报", "铁门", 
    "橄榄", "舞王", "潜水", "冰车", "海豚", 
    "小丑", "气球", "矿工", "跳跳", "蹦极", 
    "扶梯", "投篮", "白眼", "红眼",
    "空", "密度"
]
factor_id_list = list(range(len(factor_name_list)))
factor_dict = dict(zip(factor_name_list, factor_id_list))

info_read = pd.read_excel(file_name, usecols='D', skiprows=2, header=None, na_values='')
info_read.dropna(inplace=True)
info_list = info_read.iloc[:,0].tolist()
factor1 = [ factor_dict[info] for info in info_list ]

info_read = pd.read_excel(file_name, usecols='E', skiprows=2, header=None, na_values='')
info_read.dropna(inplace=True)
info_list = info_read.iloc[:,0].tolist()
factor2 = [ factor_dict[info] for info in info_list ]

info_read = pd.read_excel(file_name, usecols='F', skiprows=2, header=None, na_values='')
info_read.dropna(inplace=True)
info_list = info_read.iloc[:,0].tolist()
coef = [ float(info) for info in info_list ]

config_dict = {
    "scene": scene,
    "neg_value": neg_value,
    "density_threshold": density_threshold,
    "user_id": user_id,
    "mode_id": mode_id,
    "level_start": level_start,
    "level_num": level_num,
    "sun_start": sun_start,
    "sun_end": sun_end,
    "sun_cap": sun_cap,
    "mode": mode,
    "mode1_sim_num": mode1_sim_num,
    "mode1_output_num": mode1_output_num,
    "mode2_seed": mode2_seed,
    "factor1": factor1,
    "factor2": factor2,
    "coef": coef
}

with open("sun_config.json", "w", encoding="utf-8") as f:
    json.dump(config_dict, f, ensure_ascii=False, indent=2)
