"""Small, reproducible DOE workflow built on the existing C++ seed engine."""

from __future__ import annotations

import argparse
import csv
import hashlib
import html
import itertools
import json
import math
import re
import subprocess
import sys
import tempfile
from collections import defaultdict
from pathlib import Path


ROOT = Path(__file__).resolve().parent
EXPERIMENTS = ROOT / "experiments"
UINT32_MASK = 0xFFFFFFFF
SEED_STEP = 101
SEED_INV = 2083697005
SCENES = {"DE": 0, "NE": 1, "PE": 2, "RE": 3}

# 可直接在这里增删中文名、英文名和简称；每个名称必须全局唯一。
# 第一个名称用于 CSV 表头和报告，其余名称都可以写入 experiment.json。
FACTOR_ALIASES = {
    0: ("路障", "路障僵尸", "conehead", "cone"),
    1: ("撑杆", "撑杆僵尸", "pole_vaulting", "pole"),
    2: ("铁桶", "铁桶僵尸", "buckethead", "bucket"),
    3: ("读报", "读报僵尸", "newspaper"),
    4: ("铁门", "铁门僵尸", "screen_door", "screen"),
    5: ("橄榄", "橄榄球", "橄榄球僵尸", "football"),
    6: ("舞王", "舞王僵尸", "dancing", "dancer"),
    7: ("潜水", "潜水僵尸", "snorkel"),
    8: ("冰车", "冰车僵尸", "zomboni"),
    9: ("海豚", "海豚僵尸", "dolphin"),
    10: ("小丑", "小丑僵尸", "jack_in_the_box", "jack"),
    11: ("气球", "气球僵尸", "balloon"),
    12: ("矿工", "矿工僵尸", "digger"),
    13: ("跳跳", "跳跳僵尸", "pogo"),
    14: ("蹦极", "蹦极僵尸", "bungee"),
    15: ("扶梯", "扶梯僵尸", "ladder"),
    16: ("投篮", "投篮僵尸", "catapult"),
    17: ("白眼", "白眼巨人", "gargantuar", "garg"),
    18: ("红眼", "红眼巨人", "giga_gargantuar", "giga"),
    20: ("密度", "低权重", "low_weight", "density"),
}


class WorkflowError(RuntimeError):
    pass


def _norm(value: object) -> str:
    return str(value).strip().casefold()


def _factor_index() -> dict[str, int]:
    result: dict[str, int] = {}
    for bit, names in FACTOR_ALIASES.items():
        if not names:
            raise RuntimeError(f"FACTOR_ALIASES[{bit}] 没有名称")
        for name in names:
            key = _norm(name)
            if not key or key in result:
                raise RuntimeError(f"因子名称为空或重复: {name!r}")
            result[key] = bit
    return result


FACTOR_INDEX = _factor_index()


def factor_id(value: object) -> int:
    if isinstance(value, int) and value in FACTOR_ALIASES:
        return value
    try:
        return FACTOR_INDEX[_norm(value)]
    except KeyError as exc:
        raise WorkflowError(f"未知因子 {value!r}；请检查顶部 FACTOR_ALIASES") from exc


def factor_name(bit: int) -> str:
    return FACTOR_ALIASES[bit][0]


def term_name(term: tuple[int, ...]) -> str:
    return "截距" if not term else " × ".join(map(factor_name, term))


def _atomic_text(path: Path, text: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    tmp = path.with_name(path.name + ".tmp")
    tmp.write_text(text, encoding="utf-8")
    tmp.replace(path)


def _write_json(path: Path, value: object) -> None:
    _atomic_text(path, json.dumps(value, ensure_ascii=False, indent=2) + "\n")


def _write_csv(path: Path, header: list[str], rows: list[list[object]]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    tmp = path.with_name(path.name + ".tmp")
    with tmp.open("w", newline="", encoding="utf-8-sig") as file:
        writer = csv.writer(file)
        writer.writerow(header)
        writer.writerows(rows)
    tmp.replace(path)


def _read_json(path: Path) -> dict:
    try:
        value = json.loads(path.read_text(encoding="utf-8-sig"))
    except FileNotFoundError as exc:
        raise WorkflowError(f"文件不存在: {path}") from exc
    except json.JSONDecodeError as exc:
        raise WorkflowError(f"JSON 格式错误 {path}:{exc.lineno}: {exc.msg}") from exc
    if not isinstance(value, dict):
        raise WorkflowError(f"JSON 顶层必须是对象: {path}")
    return value


def _integer(value: object) -> int:
    if isinstance(value, bool) or not isinstance(value, (int, str)):
        raise ValueError
    if isinstance(value, int):
        return value
    text = value.strip().replace("_", "")
    unsigned = text.lstrip("+-")
    base = 16 if unsigned.lower().startswith("0x") or re.search(r"[a-f]", unsigned, re.I) else 10
    return int(text, base)


def _u32(value: object, name: str) -> int:
    try:
        number = _integer(value)
    except (TypeError, ValueError) as exc:
        raise WorkflowError(f"{name} 必须是 uint32") from exc
    if not 0 <= number <= UINT32_MASK:
        raise WorkflowError(f"{name} 超出 uint32 范围")
    return number


def _hex_seed(value: object) -> str:
    return f"0x{_u32(value, 'seed'):08X}"


def _positive_u32(value: object, name: str, *, zero: bool = False) -> int:
    number = _u32(value, name)
    if not zero and number == 0:
        raise WorkflowError(f"{name} 必须是正整数")
    return number


def _finite(value: object, name: str) -> float:
    if isinstance(value, bool):
        raise WorkflowError(f"{name} 必须是有限数")
    try:
        number = float(value)
    except (TypeError, ValueError) as exc:
        raise WorkflowError(f"{name} 必须是有限数") from exc
    if not math.isfinite(number):
        raise WorkflowError(f"{name} 必须是有限数")
    return number


def _fingerprint(value: object) -> str:
    data = json.dumps(value, ensure_ascii=False, sort_keys=True, separators=(",", ":"))
    return hashlib.sha256(data.encode()).hexdigest()


def _np():
    try:
        import numpy as np
    except ImportError as exc:
        raise WorkflowError("此命令需要 NumPy：python -m pip install numpy") from exc
    return np


def _validate_config(raw: dict, path: Path) -> dict:
    cfg = dict(raw)
    if int(cfg.get("schema_version", 1)) != 1:
        raise WorkflowError(f"不支持的 schema_version: {cfg.get('schema_version')!r}")

    experiment_id = str(cfg.get("id", path.parent.name)).strip()
    if not re.fullmatch(r"[A-Za-z0-9_.-]+", experiment_id):
        raise WorkflowError("id 只能包含英文字母、数字、点、下划线和连字符")
    cfg["id"] = experiment_id

    names = cfg.get("names", {})
    if not isinstance(names, dict):
        raise WorkflowError("names 必须是对象")
    display = str(names.get("display", experiment_id)).strip()
    short = str(names.get("short", experiment_id)).strip()
    aliases = names.get("aliases", [])
    if not display or not short or not isinstance(aliases, list):
        raise WorkflowError("names.display/names.short 不能为空，names.aliases 必须是数组")
    aliases = [str(x).strip() for x in aliases]
    if any(not x for x in aliases):
        raise WorkflowError("names.aliases 不能包含空名称")
    cfg["names"] = {"display": display, "short": short, "aliases": aliases}

    scene = str(cfg.get("scene", "")).upper()
    if scene not in SCENES:
        raise WorkflowError("scene 必须是 DE、NE、PE 或 RE")
    cfg["scene"] = scene

    seed = cfg.get("seed", {})
    if not isinstance(seed, dict):
        raise WorkflowError("seed 必须是对象")
    cfg["seed"] = {
        "user_id": _u32(seed.get("user_id", 1), "seed.user_id"),
        "mode_id": _u32(seed.get("mode_id", 13), "seed.mode_id"),
        "level_start": _u32(seed.get("level_start", 1000), "seed.level_start"),
    }

    source_factors = cfg.get("factors")
    if not isinstance(source_factors, list) or not source_factors:
        raise WorkflowError("factors 必须是非空数组")
    factors = [factor_id(x) for x in source_factors]
    if len(set(factors)) != len(factors):
        raise WorkflowError("factors 中存在重复因子或同义名称")
    cfg["_factors"] = factors
    cfg["density_threshold"] = _u32(
        cfg.get("density_threshold", 12000), "density_threshold"
    )

    design = cfg.get("design", {})
    if not isinstance(design, dict):
        raise WorkflowError("design 必须是对象")
    method = str(design.get("method", "full")).lower()
    if method not in {"full", "fractional", "d_optimal", "import"}:
        raise WorkflowError("design.method 必须是 full/fractional/d_optimal/import")
    design = dict(design)
    design["method"] = method

    source_terms = design.get("terms", [[]] + [[x] for x in source_factors])
    if not isinstance(source_terms, list) or not source_terms:
        raise WorkflowError("design.terms 必须是非空数组")
    terms: list[tuple[int, ...]] = []
    for source in source_terms:
        if not isinstance(source, list) or len(source) > 2:
            raise WorkflowError("每个模型项必须是含 0～2 个因子的数组")
        term = tuple(factor_id(x) for x in source)
        if len(set(term)) != len(term) or any(x not in factors for x in term):
            raise WorkflowError(f"非法模型项: {source!r}")
        term = tuple(sorted(term))
        if term in terms:
            raise WorkflowError(f"重复模型项: {source!r}")
        terms.append(term)
    if () not in terms:
        raise WorkflowError("design.terms 必须包含空列表 [] 作为截距")
    design["_terms"] = terms

    if method == "fractional":
        base_source = design.get("base_factors")
        generators = design.get("generators")
        if not isinstance(base_source, list) or not base_source:
            raise WorkflowError("fractional 需要非空 base_factors")
        if not isinstance(generators, dict):
            raise WorkflowError("fractional 需要 generators 对象")
        base = [factor_id(x) for x in base_source]
        if len(set(base)) != len(base) or any(x not in factors for x in base):
            raise WorkflowError("base_factors 必须是 factors 的无重复子集")
        parsed_generators: dict[int, tuple[tuple[int, ...], int]] = {}
        for name, spec in generators.items():
            generated = factor_id(name)
            if generated not in factors or generated in base or generated in parsed_generators:
                raise WorkflowError(f"非法生成列: {name!r}")
            if not isinstance(spec, dict) or not isinstance(spec.get("product"), list):
                raise WorkflowError(f"生成列 {name!r} 缺少 product 数组")
            product = tuple(factor_id(x) for x in spec["product"])
            sign = spec.get("sign", 1)
            if not product or len(set(product)) != len(product) or any(x not in base for x in product):
                raise WorkflowError(f"生成列 {name!r} 的 product 必须是 base_factors 的非空子集")
            if sign not in (-1, 1):
                raise WorkflowError(f"生成列 {name!r} 的 sign 只能是 -1 或 1")
            parsed_generators[generated] = (product, int(sign))
        if set(factors) != set(base) | set(parsed_generators):
            missing = set(factors) - set(base) - set(parsed_generators)
            raise WorkflowError(f"以下因子既不是基础列也没有生成元: {', '.join(map(factor_name, missing))}")
        design["_base"] = base
        design["_generators"] = parsed_generators

    if method == "d_optimal":
        for key, default, minimum in (
            ("runs", None, 1), ("starts", 4, 1), ("max_iter", 50, 1),
        ):
            value = design.get(key, default)
            if value is None:
                raise WorkflowError("d_optimal 需要 design.runs")
            try:
                value = int(value)
            except (TypeError, ValueError) as exc:
                raise WorkflowError(f"design.{key} 必须是整数") from exc
            if value < minimum:
                raise WorkflowError(f"design.{key} 必须不小于 {minimum}")
            design[key] = value
        design["random_seed"] = _u32(
            design.get("random_seed", 20260723), "design.random_seed"
        )
        design["max_candidates"] = int(design.get("max_candidates", 65536))
        if design["max_candidates"] < 1:
            raise WorkflowError("design.max_candidates 必须为正数")

    cfg["design"] = design
    simulation = cfg.get("simulation", {})
    if not isinstance(simulation, dict):
        raise WorkflowError("simulation 必须是对象")
    if not isinstance(simulation.get("sun_cap", True), bool):
        raise WorkflowError("simulation.sun_cap 必须是布尔值")
    cfg["simulation"] = {
        **simulation,
        "sun_start": _finite(simulation.get("sun_start", 9990), "simulation.sun_start"),
        "sun_end": _finite(simulation.get("sun_end", -500), "simulation.sun_end"),
        "sun_cap": simulation.get("sun_cap", True),
        "bias": _finite(simulation.get("bias", 0), "simulation.bias"),
        "level_num": _positive_u32(simulation.get("level_num", 5000), "simulation.level_num"),
        "sim_num": _positive_u32(simulation.get("sim_num", 10000), "simulation.sim_num"),
        "output_num": _positive_u32(
            simulation.get("output_num", 20), "simulation.output_num", zero=True
        ),
        "sampler_seed": _u32(
            simulation.get("sampler_seed", 20260723), "simulation.sampler_seed"
        ),
        "mode2_seed": _u32(
            simulation.get("mode2_seed", UINT32_MASK), "simulation.mode2_seed"
        ),
    }
    cfg["_path"] = path
    return cfg


def _experiment_names(path: Path) -> list[str]:
    try:
        cfg = _read_json(path / "experiment.json")
        names = cfg.get("names", {})
        return [path.name, str(cfg.get("id", "")), str(names.get("display", "")),
                str(names.get("short", "")), *map(str, names.get("aliases", []))]
    except (WorkflowError, TypeError):
        return [path.name]


def experiment_path(name: str) -> Path:
    direct = EXPERIMENTS / name
    if (direct / "experiment.json").is_file():
        return direct
    if not EXPERIMENTS.exists():
        raise WorkflowError(f"找不到实验 {name!r}")
    key = _norm(name)
    matches = [p for p in EXPERIMENTS.iterdir() if p.is_dir() and
               key in {_norm(x) for x in _experiment_names(p) if x}]
    if not matches:
        raise WorkflowError(f"找不到实验 {name!r}")
    if len(matches) > 1:
        raise WorkflowError(f"实验名称 {name!r} 有歧义: {', '.join(p.name for p in matches)}")
    return matches[0]


def read_experiment(name: str) -> tuple[Path, dict]:
    path = experiment_path(name)
    return path, _validate_config(_read_json(path / "experiment.json"), path / "experiment.json")


def _all_rows(factors: list[int]) -> list[dict[int, int]]:
    return [dict(zip(factors, row)) for row in itertools.product((-1, 1), repeat=len(factors))]


def _matrix(rows: list[dict[int, int]], terms: list[tuple[int, ...]]):
    np = _np()
    return np.asarray([
        [math.prod(row[x] for x in term) for term in terms]
        for row in rows
    ], dtype=float)


def _check_design(rows: list[dict[int, int]], cfg: dict) -> None:
    if not rows:
        raise WorkflowError("DOE 设计为空")
    factors, terms = cfg["_factors"], cfg["design"]["_terms"]
    signatures = [tuple(row[x] for x in factors) for row in rows]
    if len(set(signatures)) != len(signatures):
        raise WorkflowError("DOE 设计包含重复行")
    if any(value not in (-1, 1) for row in rows for value in row.values()):
        raise WorkflowError("DOE 因子值只能是 -1 或 1")
    matrix = _matrix(rows, terms)
    rank = int(_np().linalg.matrix_rank(matrix))
    if rank < len(terms):
        raise WorkflowError(f"模型矩阵不满秩：rank={rank}, terms={len(terms)}")


def _fractional(cfg: dict) -> list[dict[int, int]]:
    design = cfg["design"]
    result = []
    for row in _all_rows(design["_base"]):
        for generated, (product, sign) in design["_generators"].items():
            row[generated] = sign * math.prod(row[x] for x in product)
        result.append(row)
    return result


def _d_optimal(cfg: dict) -> list[dict[int, int]]:
    np = _np()
    factors, design = cfg["_factors"], cfg["design"]
    candidate_count = 1 << len(factors)
    if candidate_count > design["max_candidates"]:
        raise WorkflowError(
            f"D-optimal 候选点有 {candidate_count} 个，超过 max_candidates="
            f"{design['max_candidates']}；请提高限制或使用 import"
        )
    candidates = _all_rows(factors)
    x = _matrix(candidates, design["_terms"])
    runs, columns = design["runs"], x.shape[1]
    if not columns <= runs <= len(candidates):
        raise WorkflowError(f"design.runs 必须在模型项数 {columns} 和候选点数 {len(candidates)} 之间")
    if np.linalg.matrix_rank(x) < columns:
        raise WorkflowError("完整候选集的模型矩阵不满秩")

    rng = np.random.default_rng(design["random_seed"])
    best_indices, best_score = None, -np.inf
    for _ in range(design["starts"]):
        selected = None
        for _ in range(1000):
            trial = rng.choice(len(candidates), runs, replace=False)
            if np.linalg.matrix_rank(x[trial]) == columns:
                selected = trial.astype(int)
                break
        if selected is None:
            raise WorkflowError("无法构造满秩的 D-optimal 初始设计")

        for _ in range(design["max_iter"]):
            xs = x[selected]
            info_inv = np.linalg.inv(xs.T @ xs)
            leverage = np.einsum("ij,jk,ik->i", x, info_inv, x)
            chosen = set(map(int, selected))
            best_gain, exchange = 1.0 + 1e-10, None
            for position, old_index in enumerate(selected):
                old = x[old_index]
                cross = x @ info_inv @ old
                gain = (1.0 - leverage[old_index]) * (1.0 + leverage) + cross * cross
                gain[list(chosen)] = -np.inf
                new_index = int(np.argmax(gain))
                if gain[new_index] > best_gain:
                    best_gain, exchange = float(gain[new_index]), (position, new_index)
            if exchange is None:
                break
            selected[exchange[0]] = exchange[1]

        selected = np.sort(selected)
        sign, score = np.linalg.slogdet(x[selected].T @ x[selected])
        signature = tuple(map(int, selected))
        if sign > 0 and (score > best_score + 1e-10 or
                         (abs(score - best_score) <= 1e-10 and
                          (best_indices is None or signature < tuple(best_indices)))):
            best_score, best_indices = float(score), signature
    if best_indices is None:
        raise WorkflowError("D-optimal 搜索没有得到有效设计")
    return [candidates[i] for i in best_indices]


def _import_design(exp: Path, cfg: dict) -> tuple[list[dict[int, int]], list[str] | None]:
    source = exp / cfg["design"].get("input", "design_input.csv")
    try:
        file = source.open(newline="", encoding="utf-8-sig")
    except FileNotFoundError as exc:
        raise WorkflowError(f"导入设计不存在: {source}") from exc
    with file:
        reader = csv.DictReader(file)
        if not reader.fieldnames:
            raise WorkflowError(f"导入设计没有表头: {source}")
        columns: dict[int, str] = {}
        id_column = next((x for x in reader.fieldnames if _norm(x) == "design_id"), None)
        for column in reader.fieldnames:
            if column == id_column:
                continue
            bit = factor_id(column)
            if bit in columns:
                raise WorkflowError(f"导入设计包含重复同义因子列: {column}")
            columns[bit] = column
        if set(columns) != set(cfg["_factors"]):
            raise WorkflowError("导入设计的因子列必须与 experiment.json 的 factors 完全一致")
        rows, ids = [], []
        for line, source_row in enumerate(reader, 2):
            row: dict[int, int] = {}
            for bit, column in columns.items():
                try:
                    value = float(source_row[column])
                except (TypeError, ValueError) as exc:
                    raise WorkflowError(f"{source}:{line} 的 {column} 不是数字") from exc
                if value not in (-1.0, 1.0):
                    raise WorkflowError(f"{source}:{line} 的 {column} 只能是 -1 或 1")
                row[bit] = int(value)
            rows.append(row)
            ids.append(str(source_row[id_column]).strip() if id_column else "")
    if id_column:
        if any(not x for x in ids) or len(set(ids)) != len(ids):
            raise WorkflowError("导入设计的 design_id 必须非空且唯一")
        return rows, ids
    return rows, None


def make_design(exp: Path, cfg: dict) -> list[dict]:
    method = cfg["design"]["method"]
    imported_ids = None
    if method == "full":
        rows = _all_rows(cfg["_factors"])
    elif method == "fractional":
        rows = _fractional(cfg)
    elif method == "d_optimal":
        rows = _d_optimal(cfg)
    else:
        rows, imported_ids = _import_design(exp, cfg)
    _check_design(rows, cfg)
    width = max(3, len(str(len(rows))))
    ids = imported_ids or [f"D{i:0{width}d}" for i in range(1, len(rows) + 1)]
    return [{"design_id": design_id, "values": row} for design_id, row in zip(ids, rows)]


def write_design(exp: Path, cfg: dict, design: list[dict], path: Path | None = None) -> Path:
    path = path or exp / "out" / "design.csv"
    factors = cfg["_factors"]
    _write_csv(path, ["design_id", *map(factor_name, factors)], [
        [row["design_id"], *(row["values"][x] for x in factors)] for row in design
    ])
    return path


def load_design(exp: Path, cfg: dict) -> list[dict]:
    source = exp / "out" / "design.csv"
    shadow = dict(cfg)
    shadow["design"] = dict(cfg["design"], input=str(source.relative_to(exp)))
    rows, ids = _import_design(exp, shadow)
    if ids is None:
        raise WorkflowError("out/design.csv 缺少 design_id")
    _check_design(rows, cfg)
    return [{"design_id": design_id, "values": row} for design_id, row in zip(ids, rows)]


def _engine_path(given: str | None) -> Path:
    candidates = [Path(given)] if given else [
        ROOT / "_build" / "pvz_doe.exe", ROOT / "pvz_doe.exe",
        ROOT / "_build" / "pvz_doe", ROOT / "pvz_doe",
    ]
    for path in candidates:
        if path.is_file():
            return path.resolve()
    raise WorkflowError(
        "找不到 pvz_doe 计算内核；请先按 README 编译，或使用 --engine 指定路径"
    )


def _engine_result(engine: Path, command: list[str]) -> dict:
    result = subprocess.run(
        [str(engine), *command], cwd=ROOT,
        text=True, encoding="utf-8", capture_output=True,
    )
    if result.returncode:
        detail = result.stderr.strip() or result.stdout.strip() or "未知错误"
        raise WorkflowError(f"计算内核失败 ({' '.join(command[:-1] or command)}): {detail}")
    try:
        value = json.loads(result.stdout.strip())
    except json.JSONDecodeError as exc:
        raise WorkflowError(f"计算内核没有返回合法 JSON: {result.stdout[-500:]!r}") from exc
    if not isinstance(value, dict):
        raise WorkflowError("计算内核 JSON 顶层必须是对象")
    return value


def _engine_json(engine: Path, command: list[str], request: dict, temp_dir: Path) -> dict:
    temp_dir.mkdir(parents=True, exist_ok=True)
    with tempfile.NamedTemporaryFile("w", suffix=".json", prefix="request-", delete=False,
                                     dir=temp_dir, encoding="utf-8") as file:
        json.dump(request, file, ensure_ascii=False)
        request_path = Path(file.name)
    try:
        return _engine_result(engine, [*command, str(request_path)])
    finally:
        request_path.unlink(missing_ok=True)


def _targets(design: list[dict], factors: list[int]) -> tuple[list[int], dict[int, str]]:
    result, lookup = [], {}
    for row in design:
        code = sum(1 << bit for bit in factors if row["values"][bit] == 1)
        if code in lookup:
            raise WorkflowError("两个 DOE 行投影为同一个目标编码")
        result.append(code)
        lookup[code] = row["design_id"]
    return result, lookup


def _schedule_key(cfg: dict, design: list[dict]) -> str:
    factors = cfg["_factors"]
    return _fingerprint({
        "scene": cfg["scene"],
        "seed": cfg["seed"],
        "factors": factors,
        "density_threshold": cfg["density_threshold"],
        "design_config": {
            key: value for key, value in cfg["design"].items()
            if not key.startswith("_") and key != "terms"
        },
        "design": [
            [row["design_id"], [row["values"][bit] for bit in factors]] for row in design
        ],
    })


def _sample_key(schedule_key: str, design_id: str, level: int, rng_seed: int,
                real_seed: int, spawn_code: int, weight: int) -> str:
    return hashlib.sha256(
        f"{schedule_key}|{design_id}|{level}|{rng_seed}|{real_seed}|{spawn_code}|{weight}".encode()
    ).hexdigest()[:16]


def _schedule(exp: Path, cfg: dict, design: list[dict], engine: Path,
              threads: int | None, path: Path | None = None) -> Path:
    factors = cfg["_factors"]
    targets, target_ids = _targets(design, factors)
    factor_mask = sum(1 << bit for bit in factors)
    request: dict[str, object] = {
        "scene": cfg["scene"],
        "factor_mask": factor_mask,
        "density_threshold": cfg["density_threshold"],
        "targets": targets,
    }
    search = cfg.get("search", {})
    if isinstance(search, dict):
        if "start_rng_seed" in search:
            request["start_rng_seed"] = _u32(
                search["start_rng_seed"], "search.start_rng_seed"
            )
        if "find_len" in search:
            try:
                find_len = _integer(search["find_len"])
            except (TypeError, ValueError) as exc:
                raise WorkflowError("search.find_len 必须是 1..2^32") from exc
            if not 1 <= find_len <= 1 << 32:
                raise WorkflowError("search.find_len 必须是 1..2^32")
            request["find_len"] = find_len
    if threads is not None:
        if threads < 1:
            raise WorkflowError("--threads 必须为正数")
        request["threads"] = threads

    result = _engine_json(engine, ["search", "set"], request, exp / "out")
    segments = result.get("segments")
    if not isinstance(segments, list) or not segments:
        raise WorkflowError("set 搜索没有返回任何片段")
    covered: list[int] = []
    clean_segments = []
    for index, segment in enumerate(segments):
        if not isinstance(segment, dict) or not isinstance(segment.get("codes"), list):
            raise WorkflowError(f"set 搜索第 {index + 1} 个片段格式错误")
        rng_seed = _u32(segment.get("rng_seed"), "segment.rng_seed")
        codes = [_u32(x, "segment.codes") for x in segment["codes"]]
        length = int(segment.get("length", len(codes)))
        if length != len(codes) or length < 1:
            raise WorkflowError("set 搜索片段 length 与 codes 不一致")
        if any(code not in target_ids for code in codes):
            raise WorkflowError("set 搜索返回了目标集合之外的编码")
        covered.extend(codes)
        clean_segments.append({"rng_seed": rng_seed, "length": length, "codes": codes})
    if len(covered) != len(set(covered)) or set(covered) != set(targets):
        raise WorkflowError("set 搜索结果没有不重不漏地覆盖 DOE")

    seed = cfg["seed"]
    expanded = _engine_json(engine, ["expand"], {
        "scene": cfg["scene"],
        "user_id": seed["user_id"],
        "mode_id": seed["mode_id"],
        "level_start": seed["level_start"],
        "factor_mask": factor_mask,
        "density_threshold": cfg["density_threshold"],
        "segments": clean_segments,
    }, exp / "out")
    samples = expanded.get("samples")
    if not isinstance(samples, list) or len(samples) != len(targets):
        raise WorkflowError("expand 返回的 samples 数量不正确")

    schedule_key = _schedule_key(cfg, design)
    rows, order = [], 0
    width = max(3, len(str(len(samples))))
    sample_at = 0
    for segment_index, segment in enumerate(clean_segments):
        segment_level = (seed["level_start"] + order) & UINT32_MASK
        expected_real = (segment["rng_seed"] - seed["user_id"] - seed["mode_id"]
                         - SEED_STEP * segment_level) & UINT32_MASK
        for offset, projected in enumerate(segment["codes"]):
            sample = samples[sample_at]
            if not isinstance(sample, dict):
                raise WorkflowError("expand sample 格式错误")
            level = (seed["level_start"] + order) & UINT32_MASK
            rng_seed = (segment["rng_seed"] + SEED_STEP * offset) & UINT32_MASK
            expected_index = (rng_seed * SEED_INV) & UINT32_MASK
            checks = {
                "segment": segment_index, "offset": offset, "level": level,
                "rng_seed": rng_seed, "real_seed": expected_real,
                "bin_index": expected_index, "projected_code": projected,
            }
            for key, expected in checks.items():
                if key in sample and _u32(sample[key], f"expand.{key}") != expected:
                    raise WorkflowError(f"expand 的 {key} 校验失败: {sample[key]} != {expected}")
            if "spawn_code" not in sample or "weight" not in sample:
                raise WorkflowError("expand sample 缺少 spawn_code 或 weight")
            sample_id = f"S{sample_at + 1:0{width}d}"
            design_id = target_ids[projected]
            spawn_code = _u32(sample["spawn_code"], "spawn_code")
            weight = _u32(sample["weight"], "weight")
            sample_key = _sample_key(
                schedule_key, design_id, level, rng_seed, expected_real, spawn_code, weight
            )
            rows.append([
                sample_id, sample_key, schedule_key, order, design_id,
                f"G{segment_index + 1:03d}", offset, level,
                _hex_seed(rng_seed), _hex_seed(expected_real),
                expected_index, spawn_code, projected,
                weight,
            ])
            sample_at += 1
            order += 1

    path = path or exp / "out" / "schedule.csv"
    _write_csv(path, [
        "sample_id", "sample_key", "schedule_key", "execution_order", "design_id",
        "segment_id", "segment_offset",
        "level", "rng_seed", "real_seed", "bin_index", "spawn_code",
        "projected_code", "weight",
    ], rows)
    return path


def _observation_template(exp: Path) -> Path:
    schedule = exp / "out" / "schedule.csv"
    with schedule.open(newline="", encoding="utf-8-sig") as file:
        samples = list(csv.DictReader(file))
    path = exp / "observations.csv"
    if path.exists():
        print(f"保留已有观测文件: {path}", file=sys.stderr)
        return path
    width = max(3, len(str(len(samples))))
    _write_csv(path, [
        "observation_id", "sample_id", "sample_key", "replicate", "raw_response", "adjustment",
        "valid", "source", "note",
    ], [[f"O{i:0{width}d}", row["sample_id"], row["sample_key"], 1, "", 0, 1, "", ""]
        for i, row in enumerate(samples, 1)])
    return path


def _read_csv(path: Path) -> list[dict[str, str]]:
    try:
        with path.open(newline="", encoding="utf-8-sig") as file:
            reader = csv.DictReader(file)
            if not reader.fieldnames:
                raise WorkflowError(f"CSV 没有表头: {path}")
            return list(reader)
    except FileNotFoundError as exc:
        raise WorkflowError(f"文件不存在: {path}") from exc


def _validate_schedule(cfg: dict, design: list[dict], rows: list[dict[str, str]]) \
        -> tuple[dict[str, tuple[str, str]], str]:
    if not rows:
        raise WorkflowError("schedule.csv 为空")
    required = {
        "sample_id", "sample_key", "schedule_key", "execution_order", "design_id",
        "segment_id", "segment_offset", "level", "rng_seed", "real_seed", "bin_index",
        "spawn_code", "projected_code", "weight",
    }
    if not required <= set(rows[0]):
        raise WorkflowError("schedule.csv 缺少必要列")

    factors = cfg["_factors"]
    factor_mask = sum(1 << bit for bit in factors)
    targets = {
        row["design_id"]: sum(
            1 << bit for bit in factors if row["values"][bit] == 1
        ) for row in design
    }
    expected_key = _schedule_key(cfg, design)
    schedule: dict[str, tuple[str, str]] = {}
    design_ids: list[str] = []
    seen_segments: set[str] = set()
    current_segment = ""
    previous_level = previous_rng = previous_index = previous_offset = previous_real = None

    for order, row in enumerate(rows):
        sample_id, design_id = row.get("sample_id", ""), row.get("design_id", "")
        segment = row.get("segment_id", "")
        if (not sample_id or sample_id in schedule or design_id not in targets or
                not segment or row.get("schedule_key") != expected_key):
            raise WorkflowError("schedule.csv 的样本、设计点、片段或指纹非法")
        try:
            execution_order = int(row["execution_order"])
            offset = int(row["segment_offset"])
        except (TypeError, ValueError) as exc:
            raise WorkflowError("schedule.csv 的 execution_order/segment_offset 不是整数") from exc
        level = _u32(row["level"], "schedule.level")
        rng_seed = _u32(row["rng_seed"], "schedule.rng_seed")
        real_seed = _u32(row["real_seed"], "schedule.real_seed")
        bin_index = _u32(row["bin_index"], "schedule.bin_index")
        spawn_code = _u32(row["spawn_code"], "schedule.spawn_code")
        projected = _u32(row["projected_code"], "schedule.projected_code")
        weight = _u32(row["weight"], "schedule.weight")

        if execution_order != order:
            raise WorkflowError("schedule.csv 的 execution_order 不连续")
        if order == 0 and level != cfg["seed"]["level_start"]:
            raise WorkflowError("schedule.csv 的首个 level 不是 seed.level_start")
        if previous_level is not None and level != ((previous_level + 1) & UINT32_MASK):
            raise WorkflowError("schedule.csv 的 level 不连续")

        if segment != current_segment:
            if segment in seen_segments or offset != 0:
                raise WorkflowError("schedule.csv 的 segment 不连续或未从 offset 0 开始")
            seen_segments.add(segment)
            current_segment = segment
        elif (offset != previous_offset + 1 or
              rng_seed != ((previous_rng + SEED_STEP) & UINT32_MASK) or
              bin_index != ((previous_index + 1) & UINT32_MASK) or
              real_seed != previous_real):
            raise WorkflowError("schedule.csv 的同段 offset/种子/下标不连续")

        expected_real = (
            rng_seed - cfg["seed"]["user_id"] - cfg["seed"]["mode_id"] - SEED_STEP * level
        ) & UINT32_MASK
        if real_seed != expected_real or bin_index != ((rng_seed * SEED_INV) & UINT32_MASK):
            raise WorkflowError("schedule.csv 的种子换算不正确")

        actual = spawn_code & factor_mask & 0x000FFFFF
        if factor_mask & (1 << 20) and weight < cfg["density_threshold"]:
            actual |= 1 << 20
        if projected != targets[design_id] or actual != projected:
            raise WorkflowError("schedule.csv 的出怪编码与 DOE 投影不一致")
        sample_key = _sample_key(
            expected_key, design_id, level, rng_seed, real_seed, spawn_code, weight
        )
        if row.get("sample_key") != sample_key:
            raise WorkflowError("schedule.csv 的 sample_key 与样本内容不匹配")

        schedule[sample_id] = (design_id, sample_key)
        design_ids.append(design_id)
        previous_level, previous_rng, previous_index = level, rng_seed, bin_index
        previous_offset, previous_real = offset, real_seed

    if len(design_ids) != len(set(design_ids)) or set(design_ids) != set(targets):
        raise WorkflowError("schedule.csv 没有不重不漏地覆盖 DOE")
    return schedule, expected_key


def _observations(exp: Path, schedule: dict[str, tuple[str, str]]) -> dict[str, list[float]]:
    grouped: dict[str, list[float]] = defaultdict(list)
    true_values, false_values = {"1", "true", "yes", "y", "是"}, {"0", "false", "no", "n", "否"}
    for line, row in enumerate(_read_csv(exp / "observations.csv"), 2):
        valid = _norm(row.get("valid", ""))
        if valid not in true_values | false_values:
            raise WorkflowError(f"observations.csv:{line} 的 valid 不是布尔值")
        if valid in false_values:
            continue
        sample_id = row.get("sample_id", "")
        if sample_id not in schedule:
            raise WorkflowError(f"observations.csv:{line} 的 sample_id 不在 schedule 中")
        if row.get("sample_key", "") != schedule[sample_id][1]:
            raise WorkflowError(f"observations.csv:{line} 与当前 schedule 不匹配；请重新导出样本")
        try:
            raw = float(row.get("raw_response", ""))
            adjustment = float(row.get("adjustment") or 0)
        except ValueError as exc:
            raise WorkflowError(f"observations.csv:{line} 的响应不是数字") from exc
        if not math.isfinite(raw + adjustment):
            raise WorkflowError(f"observations.csv:{line} 的响应不是有限数")
        grouped[sample_id].append(raw + adjustment)
    if not grouped:
        raise WorkflowError("没有有效观测；请填写 raw_response，或将未完成行的 valid 改为 0")
    return dict(grouped)


def _analysis_key(cfg: dict, schedule_key: str, grouped: dict[str, list[float]]) -> str:
    return _fingerprint({
        "schedule_key": schedule_key,
        "terms": [list(term) for term in cfg["design"]["_terms"]],
        "simulation": cfg["simulation"],
        "responses": [[sample_id, sorted(values)] for sample_id, values in sorted(grouped.items())],
    })


def _analysis_inputs(exp: Path, cfg: dict):
    design = load_design(exp, cfg)
    schedule, schedule_key = _validate_schedule(
        cfg, design, _read_csv(exp / "out" / "schedule.csv")
    )
    grouped = _observations(exp, schedule)
    return design, schedule, grouped, _analysis_key(cfg, schedule_key, grouped)


def _fit(exp: Path, cfg: dict) -> tuple[dict, list[dict]]:
    np = _np()
    design_rows, schedule, grouped, analysis_key = _analysis_inputs(exp, cfg)
    design = {row["design_id"]: row["values"] for row in design_rows}

    terms = cfg["design"]["_terms"]
    sample_ids = sorted(grouped, key=lambda x: int(x[1:]) if x[1:].isdigit() else x)
    rows = [design[schedule[x][0]] for x in sample_ids]
    x = _matrix(rows, terms)
    y = np.asarray([np.mean(grouped[x]) for x in sample_ids], dtype=float)
    rank = int(np.linalg.matrix_rank(x))
    if rank < len(terms):
        raise WorkflowError(f"有效观测的模型矩阵不满秩：rank={rank}, terms={len(terms)}")
    coef, _, _, _ = np.linalg.lstsq(x, y, rcond=None)
    predicted = x @ coef
    residual = y - predicted
    n, p = len(y), len(terms)
    sse = float(residual @ residual)
    centered = y - y.mean()
    sst = float(centered @ centered)
    r2 = None if sst == 0 else 1.0 - sse / sst
    df = n - rank
    rmse = math.sqrt(sse / n)
    adjusted = None if r2 is None or df <= 0 else 1.0 - (1.0 - r2) * (n - 1) / df
    standard_error = [None] * p
    p_value = [None] * p
    t_value = [None] * p
    if df > 0:
        covariance = (sse / df) * np.linalg.pinv(x.T @ x)
        standard_error = np.sqrt(np.maximum(np.diag(covariance), 0)).tolist()
        t_value = [None if se == 0 else float(value / se)
                   for value, se in zip(coef, standard_error)]
        try:
            from scipy.stats import t as student_t
            p_value = [None if value is None else float(2 * student_t.sf(abs(value), df))
                       for value in t_value]
        except ImportError:
            pass

    simulation = cfg["simulation"]
    seed = cfg["seed"]
    factor1, factor2 = [], []
    for term in terms:
        if not term:
            factor1.append(19); factor2.append(19)
        elif len(term) == 1:
            factor1.append(term[0]); factor2.append(19)
        else:
            factor1.append(term[0]); factor2.append(term[1])
    fit = {
        "n": n, "rank": rank, "df_resid": df, "r2": r2,
        "adjusted_r2": adjusted, "rmse": rmse, "sse": sse,
    }
    model = {
        "schema_version": 1,
        "experiment_id": cfg["id"],
        "analysis_key": analysis_key,
        "names": cfg["names"],
        "scene": SCENES[cfg["scene"]],
        "scene_name": cfg["scene"],
        "neg_value": -1,
        "density_threshold": cfg["density_threshold"],
        "user_id": seed["user_id"],
        "mode_id": seed["mode_id"],
        "level_start": seed["level_start"],
        "level_num": simulation["level_num"],
        "sun_start": simulation["sun_start"],
        "sun_end": simulation["sun_end"],
        "sun_cap": simulation["sun_cap"],
        "bias": simulation["bias"],
        "mode": 1,
        "mode1_sim_num": simulation["sim_num"],
        "mode1_output_num": simulation["output_num"],
        "mode2_seed": _hex_seed(simulation["mode2_seed"]),
        "sampler_seed": _hex_seed(simulation["sampler_seed"]),
        "factor1": factor1,
        "factor2": factor2,
        "coef": [float(x) for x in coef],
        "terms": [
            {"factors": [factor_name(x) for x in term], "coefficient": float(value),
             "std_error": standard_error[i], "t_value": t_value[i], "p_value": p_value[i]}
            for i, (term, value) in enumerate(zip(terms, coef))
        ],
        "fit": fit,
        "response": {"name": "net_sun", "positive": "gain"},
    }
    model["model_key"] = _fingerprint(model)
    detail = [{
        "sample_id": sample_id, "design_id": schedule[sample_id][0],
        "replicates": len(grouped[sample_id]), "observed": float(y[i]),
        "predicted": float(predicted[i]), "residual": float(residual[i]),
    } for i, sample_id in enumerate(sample_ids)]
    return model, detail


def _number(value: object, digits: int = 6) -> str:
    if value is None:
        return "—"
    return f"{float(value):.{digits}g}"


def _cli_integer(value: str) -> int:
    try:
        return _integer(value)
    except (TypeError, ValueError) as exc:
        raise argparse.ArgumentTypeError(f"不是整数: {value!r}") from exc


def _cli_u32(value: str) -> int:
    number = _cli_integer(value)
    if not 0 <= number <= UINT32_MASK:
        raise argparse.ArgumentTypeError("必须在 0..2^32-1 之间")
    return number


def _cli_length(value: str) -> int:
    number = _cli_integer(value)
    if not 1 <= number <= 1 << 32:
        raise argparse.ArgumentTypeError("必须在 1..2^32 之间")
    return number


def _cli_positive(value: str) -> int:
    number = _cli_integer(value)
    if number < 1:
        raise argparse.ArgumentTypeError("必须是正整数")
    return number


def _score_spec(value: str) -> tuple[int, int]:
    for separator in ("=", ":"):
        if separator in value:
            name, weight = value.rsplit(separator, 1)
            break
    else:
        raise argparse.ArgumentTypeError("评分项必须写成 因子=权重")
    try:
        bit = factor_id(name)
    except WorkflowError as exc:
        try:
            bit = _cli_integer(name)
        except argparse.ArgumentTypeError:
            raise argparse.ArgumentTypeError(str(exc)) from exc
    if not 0 <= bit <= 18:
        raise argparse.ArgumentTypeError("评分因子必须是 0..18 位的僵尸类型")
    try:
        return bit, _cli_integer(weight)
    except argparse.ArgumentTypeError as exc:
        raise argparse.ArgumentTypeError(f"非法权重: {weight!r}") from exc


def _print_json(value: object) -> None:
    print(json.dumps(value, ensure_ascii=False, indent=2))


def _report(cfg: dict, model: dict, detail: list[dict]) -> str:
    coefficient_rows = "".join(
        "<tr>" + "".join(f"<td>{html.escape(str(value))}</td>" for value in (
            " × ".join(term["factors"]) or "截距", _number(term["coefficient"]),
            _number(term["std_error"]), _number(term["t_value"]), _number(term["p_value"]),
        )) + "</tr>" for term in model["terms"]
    )
    sample_rows = "".join(
        "<tr>" + "".join(f"<td>{html.escape(str(value))}</td>" for value in (
            row["sample_id"], row["design_id"], row["replicates"],
            _number(row["observed"]), _number(row["predicted"]), _number(row["residual"]),
        )) + "</tr>" for row in detail
    )
    fit = model["fit"]
    return f"""<!doctype html>
<html lang="zh-CN"><meta charset="utf-8"><title>{html.escape(cfg['names']['display'])}</title>
<style>body{{font:15px/1.5 system-ui;margin:2rem;max-width:1100px}}table{{border-collapse:collapse}}
th,td{{border:1px solid #bbb;padding:.35rem .6rem;text-align:right}}th:first-child,td:first-child{{text-align:left}}
code{{background:#eee;padding:.1rem .25rem}}</style>
<h1>{html.escape(cfg['names']['display'])}</h1>
<p>实验 <code>{html.escape(cfg['id'])}</code>；场地 {cfg['scene']}；有效样本 {fit['n']}；
rank={fit['rank']}；R²={_number(fit['r2'])}；调整 R²={_number(fit['adjusted_r2'])}；RMSE={_number(fit['rmse'])}。</p>
<p>响应约定：<strong>正值表示净增加阳光</strong>。同一样本的重复观测先取均值再拟合。</p>
<h2>系数</h2><table><thead><tr><th>模型项</th><th>系数</th><th>标准误</th><th>t</th><th>p</th></tr></thead>
<tbody>{coefficient_rows}</tbody></table>
<h2>样本拟合</h2><table><thead><tr><th>样本</th><th>设计点</th><th>重复数</th><th>观测</th><th>预测</th><th>残差</th></tr></thead>
<tbody>{sample_rows}</tbody></table>
<p>p 值一栏在未安装 SciPy 或残差自由度为零时留空。本报告只包含 OLS；长期模拟由 C++ 计算内核读取 <code>model.json</code> 执行。</p>
</html>\n"""


def cmd_init(args: argparse.Namespace) -> None:
    if not re.fullmatch(r"[A-Za-z0-9_.-]+", args.name):
        raise WorkflowError("NAME 只能包含英文字母、数字、点、下划线和连字符")
    scene = args.scene.upper()
    path = EXPERIMENTS / args.name
    if path.exists():
        raise WorkflowError(f"实验目录已存在: {path}")
    aliases = args.alias or []
    config = {
        "schema_version": 1,
        "id": args.name,
        "names": {"display": args.display or args.name, "short": args.short or args.name,
                  "aliases": aliases},
        "scene": scene,
        "seed": {"user_id": 1, "mode_id": 13, "level_start": 1000},
        "factors": ["橄榄", "冰车", "气球", "白眼", "红眼", "密度"],
        "density_threshold": 12000,
        "design": {
            "method": "full",
            "terms": [[], ["橄榄"], ["冰车"], ["气球"], ["白眼"], ["红眼"], ["密度"]],
        },
        "simulation": {
            "sun_start": 9990, "sun_end": -500, "sun_cap": True, "bias": 0,
            "level_num": 5000, "sim_num": 10000,
            "sampler_seed": _hex_seed(20260723),
            "output_num": 20,
        },
    }
    _validate_config(config, path / "experiment.json")
    _write_json(path / "experiment.json", config)
    print(path / "experiment.json")


def cmd_list(_: argparse.Namespace) -> None:
    if not EXPERIMENTS.exists():
        return
    for path in sorted(x for x in EXPERIMENTS.iterdir() if x.is_dir()):
        try:
            cfg = _validate_config(_read_json(path / "experiment.json"), path / "experiment.json")
            aliases = ", ".join(cfg["names"]["aliases"])
            print(f"{path.name}\t{cfg['scene']}\t{cfg['names']['display']}\t{cfg['names']['short']}\t{aliases}")
        except WorkflowError as exc:
            print(f"{path.name}\tERROR\t{exc}")


def cmd_design(args: argparse.Namespace) -> None:
    exp, cfg = read_experiment(args.experiment)
    design = make_design(exp, cfg)
    path = write_design(exp, cfg, design)
    print(f"{path} ({len(design)} runs)")


def cmd_prepare(args: argparse.Namespace) -> None:
    exp, cfg = read_experiment(args.experiment)
    design = make_design(exp, cfg)
    engine = _engine_path(args.engine)
    out = exp / "out"
    design_path, schedule = out / "design.csv", out / "schedule.csv"
    design_next, schedule_next = out / "design.csv.next", out / "schedule.csv.next"
    try:
        write_design(exp, cfg, design, design_next)
        _schedule(exp, cfg, design, engine, args.threads, schedule_next)
        _validate_schedule(cfg, design, _read_csv(schedule_next))
        design_next.replace(design_path)
        schedule_next.replace(schedule)
    finally:
        design_next.unlink(missing_ok=True)
        schedule_next.unlink(missing_ok=True)
    observations = _observation_template(exp)
    print(design_path)
    print(schedule)
    print(observations)


def cmd_analyze(args: argparse.Namespace) -> None:
    exp, cfg = read_experiment(args.experiment)
    model, detail = _fit(exp, cfg)
    model_path = exp / "out" / "model.json"
    report_path = exp / "out" / "report.html"
    _write_json(model_path, model)
    _atomic_text(report_path, _report(cfg, model, detail))
    print(model_path)
    print(report_path)


def cmd_inspect(args: argparse.Namespace) -> None:
    _, cfg = read_experiment(args.experiment)
    seed = cfg["seed"]
    result = _engine_result(_engine_path(args.engine), [
        "inspect", cfg["scene"], str(args.real_seed), str(seed["user_id"]),
        str(seed["mode_id"]), str(args.level if args.level is not None else seed["level_start"]),
        str(args.count),
    ])
    _print_json(result)


def cmd_search(args: argparse.Namespace) -> None:
    request: dict[str, object] = {"scene": args.scene}
    for key in ("start_rng_seed", "find_len", "threads"):
        value = getattr(args, key, None)
        if value is not None:
            request[key] = value
    if args.search_mode == "exact":
        request["masks"] = args.masks
    elif args.search_mode == "score":
        request.update({
            "window": args.window,
            "constraints": [bit for bit, _ in args.scores],
            "weights": [weight for _, weight in args.scores],
        })
    else:
        masks, fmasks = args.masks or [], args.fmasks or []
        if not masks and not fmasks:
            raise WorkflowError("run 至少需要一个 --require 或 --exclude")
        request.update({"masks": masks, "fmasks": fmasks})
    result = _engine_json(
        _engine_path(args.engine), ["search", args.search_mode], request,
        Path(tempfile.gettempdir()) / "pvz_doe",
    )
    _print_json(result)


def _validate_model(exp: Path, cfg: dict, model: dict) -> str:
    if model.get("experiment_id") != cfg["id"]:
        raise WorkflowError("model.json 不属于当前实验")
    model_key = model.get("model_key")
    unsigned = {key: value for key, value in model.items() if key != "model_key"}
    if not isinstance(model_key, str) or model_key != _fingerprint(unsigned):
        raise WorkflowError("model.json 的 model_key 无效；请重新 analyze")
    *_, analysis_key = _analysis_inputs(exp, cfg)
    if model.get("analysis_key") != analysis_key:
        raise WorkflowError("model.json 与当前配置、日程或观测不匹配；请重新 analyze")
    return model_key


def _evaluation(exp: Path, cfg: dict, mode: str, result: dict, model_key: str) -> Path:
    path = exp / "out" / "evaluation.json"
    value = _read_json(path) if path.exists() else {}
    old_id = value.get("experiment_id")
    if old_id not in (None, cfg["id"]):
        raise WorkflowError(f"evaluation.json 属于其他实验: {old_id!r}")
    if value.get("model_key") != model_key:
        value = {}
    value.update({
        "schema_version": 1, "experiment_id": cfg["id"],
        "model_key": model_key, mode: result,
    })
    _write_json(path, value)
    return path


def _write_trajectory(exp: Path, result: dict) -> tuple[Path, dict]:
    samples = result.get("samples")
    if not isinstance(samples, list):
        raise WorkflowError("trace 模拟没有返回 samples")
    fields = [
        "real_seed", "step", "level", "rng_seed", "spawn_code",
        "predicted_response", "sun", "failed",
    ]
    real_seed = _hex_seed(result.get("real_seed"))
    rows = []
    for index, sample in enumerate(samples):
        if not isinstance(sample, dict) or any(key not in sample for key in fields[1:]):
            raise WorkflowError(f"trace 第 {index + 1} 个样本格式错误")
        rng_seed = _hex_seed(sample["rng_seed"])
        rows.append([real_seed, sample["step"], sample["level"], rng_seed,
                     *(sample[key] for key in fields[4:])])
    path = exp / "out" / "trajectories.csv"
    _write_csv(path, fields, rows)
    summary = {key: value for key, value in result.items() if key != "samples"}
    summary.update({"real_seed": real_seed, "sample_count": len(samples),
                    "trajectories": path.name})
    return path, summary


def cmd_simulate(args: argparse.Namespace) -> None:
    exp, cfg = read_experiment(args.experiment)
    source_model = _read_json(exp / "out" / "model.json")
    model_key = _validate_model(exp, cfg, source_model)
    model = dict(source_model)
    model["mode"] = {"mc": 1, "trace": 2, "worst": 3}[args.simulation_mode]
    if args.level_num is not None:
        model["level_num"] = args.level_num

    if args.simulation_mode == "mc":
        for argument, field in (
            (args.trials, "mode1_sim_num"),
            (args.output_num, "mode1_output_num"),
            (args.sampler_seed, "sampler_seed"),
        ):
            if argument is not None:
                model[field] = argument
    elif args.simulation_mode == "trace":
        model["mode2_seed"] = args.real_seed
        if args.level_start is not None:
            model["level_start"] = args.level_start
    else:
        if args.start_rng_seed is not None:
            model["start_rng_seed"] = args.start_rng_seed
        if args.find_len is not None:
            model["find_len"] = args.find_len

    result = _engine_json(
        _engine_path(args.engine), ["simulate"], model, exp / "out"
    )
    trajectory = None
    if args.simulation_mode == "trace":
        trajectory, result = _write_trajectory(exp, result)
    evaluation = _evaluation(exp, cfg, args.simulation_mode, result, model_key)
    print(evaluation)
    if trajectory:
        print(trajectory)


def cmd_check(args: argparse.Namespace) -> None:
    exp, cfg = read_experiment(args.experiment)
    design_path = exp / "out" / "design.csv"
    schedule_path = exp / "out" / "schedule.csv"
    observations_path = exp / "observations.csv"
    model_path = exp / "out" / "model.json"
    evaluation_path = exp / "out" / "evaluation.json"
    design = load_design(exp, cfg) if design_path.exists() else None
    schedule = None
    if schedule_path.exists():
        if design is None:
            raise WorkflowError("存在 schedule.csv，但缺少 out/design.csv")
        schedule, _ = _validate_schedule(cfg, design, _read_csv(schedule_path))
    elif observations_path.exists():
        raise WorkflowError("存在 observations.csv，但缺少 out/schedule.csv")
    if observations_path.exists():
        assert schedule is not None
        observation_rows = _read_csv(observations_path)
        unknown = {row.get("sample_id", "") for row in observation_rows} - set(schedule)
        if unknown:
            raise WorkflowError(f"observations.csv 包含未知 sample_id: {sorted(unknown)!r}")
        if any(row.get("sample_key", "") != schedule[row["sample_id"]][1]
               for row in observation_rows):
            raise WorkflowError("observations.csv 与当前 schedule 不匹配")
    model_key = None
    if model_path.exists():
        model_key = _validate_model(exp, cfg, _read_json(model_path))
    if evaluation_path.exists():
        if model_key is None:
            raise WorkflowError("存在 evaluation.json，但缺少 model.json")
        evaluation = _read_json(evaluation_path)
        if (evaluation.get("experiment_id") != cfg["id"] or
                evaluation.get("model_key") != model_key):
            raise WorkflowError("evaluation.json 与当前 model.json 不匹配")
    print(f"OK: {cfg['id']}")


def parser() -> argparse.ArgumentParser:
    result = argparse.ArgumentParser(description="PVZ 种子 × DOE × 阳光模型工作流")
    commands = result.add_subparsers(dest="command", required=True)

    command = commands.add_parser("init", help="创建实验配置")
    command.add_argument("name")
    command.add_argument("scene", choices=SCENES)
    command.add_argument("--display", help="报告显示名称")
    command.add_argument("--short", help="命令行简称")
    command.add_argument("--alias", action="append", help="可重复指定其他称呼")
    command.set_defaults(func=cmd_init)

    command = commands.add_parser("list", help="列出实验及其称呼")
    command.set_defaults(func=cmd_list)

    command = commands.add_parser("design", help="只生成并校验 DOE 设计")
    command.add_argument("experiment")
    command.set_defaults(func=cmd_design)

    command = commands.add_parser("prepare", help="DOE、种子片段搜索和执行表")
    command.add_argument("experiment")
    command.add_argument("--engine", help="pvz_doe 计算内核路径")
    command.add_argument("--threads", type=int)
    command.set_defaults(func=cmd_prepare)

    command = commands.add_parser("analyze", help="拟合 OLS 并生成 model.json/report.html")
    command.add_argument("experiment")
    command.set_defaults(func=cmd_analyze)

    command = commands.add_parser("inspect", help="查看实验种子在连续 level 下的出怪")
    command.add_argument("experiment")
    command.add_argument("real_seed", type=_cli_u32)
    command.add_argument("--level", type=_cli_u32, help="默认使用 seed.level_start")
    command.add_argument("--count", type=_cli_positive, default=1)
    command.add_argument("--engine", help="pvz_doe 计算内核路径")
    command.set_defaults(func=cmd_inspect)

    search = commands.add_parser("search", help="独立使用 Quick 种子搜索")
    searches = search.add_subparsers(dest="search_mode", required=True)

    def search_options(item: argparse.ArgumentParser) -> None:
        item.add_argument("scene", choices=SCENES)
        item.add_argument("--start-rng-seed", type=_cli_u32)
        item.add_argument("--find-len", type=_cli_length)
        item.add_argument("--threads", type=_cli_positive)
        item.add_argument("--engine", help="pvz_doe 计算内核路径")
        item.set_defaults(func=cmd_search)

    item = searches.add_parser("exact", help="搜索完整出怪编码序列")
    search_options(item)
    item.add_argument("masks", nargs="+", type=_cli_u32, metavar="CODE")

    item = searches.add_parser("score", help="按僵尸类型权重评分连续窗口")
    search_options(item)
    item.add_argument("window", type=_cli_positive)
    item.add_argument("scores", nargs="+", type=_score_spec, metavar="因子=权重")

    item = searches.add_parser("run", help="搜索满足包含/排除条件的连续区间")
    search_options(item)
    item.add_argument("--require", dest="masks", action="append", type=_cli_u32,
                      metavar="MASK", help="必须完全包含的 bitmask，可重复")
    item.add_argument("--exclude", dest="fmasks", action="append", type=_cli_u32,
                      metavar="MASK", help="必须完全缺失的 bitmask，可重复")

    simulate = commands.add_parser("simulate", help="使用 out/model.json 传播阳光模型")
    simulations = simulate.add_subparsers(dest="simulation_mode", required=True)

    def simulate_options(item: argparse.ArgumentParser) -> None:
        item.add_argument("experiment")
        item.add_argument("--level-num", type=_cli_positive)
        item.add_argument("--engine", help="pvz_doe 计算内核路径")
        item.set_defaults(func=cmd_simulate)

    item = simulations.add_parser("mc", help="Monte Carlo 破阵率")
    simulate_options(item)
    item.add_argument("--trials", type=_cli_positive)
    item.add_argument("--output-num", type=_cli_positive)
    item.add_argument("--sampler-seed", type=_cli_u32)

    item = simulations.add_parser("trace", help="指定 real_seed 的阳光轨迹")
    simulate_options(item)
    item.add_argument("real_seed", type=_cli_u32)
    item.add_argument("--level-start", type=_cli_u32)

    item = simulations.add_parser("worst", help="完整环上的最坏累积损失区间")
    simulate_options(item)
    item.add_argument("--start-rng-seed", type=_cli_u32)
    item.add_argument("--find-len", type=_cli_length)

    command = commands.add_parser("check", help="检查配置和已有中间文件")
    command.add_argument("experiment")
    command.set_defaults(func=cmd_check)
    return result


def main(argv: list[str] | None = None) -> int:
    try:
        args = parser().parse_args(argv)
        args.func(args)
        return 0
    except WorkflowError as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 2
    except KeyboardInterrupt:
        print("error: 已取消", file=sys.stderr)
        return 130


if __name__ == "__main__":
    raise SystemExit(main())
