"""Create an isolated local ADVAI runtime from the supported core's distributed configs."""

import argparse
import os
from pathlib import Path
import re

from configure import merge


def override(text: str, changes: dict[str, str]) -> str:
    for key, value in changes.items():
        pattern = rf"^\s*{re.escape(key)}\s*=.*$"
        text, count = re.subn(pattern, lambda _: f"{key} = {value}", text, flags=re.MULTILINE)
        if count != 1:
            raise ValueError(f"Expected exactly one config entry for {key}, found {count}")
    return text


def quoted(path: Path) -> str:
    return f'"{path.resolve().as_posix()}"'


def create(core: Path, output: Path, population: int, accounts: int, mysql_client: Path) -> None:
    if output.exists():
        raise ValueError(f"Runtime output already exists: {output}")
    password = os.environ.get("ADVAI_DB_PASSWORD", "")
    if not password or any(c in password for c in ';"\n\r'):
        raise ValueError("Set ADVAI_DB_PASSWORD to a nonempty config-safe local test database password")
    root = Path(__file__).resolve().parents[2]
    core = core.resolve(strict=True)
    mysql_client = mysql_client.resolve(strict=True)
    output = output.resolve()
    data = core / "var/advai-data"
    for directory in ("dbc", "maps", "vmaps", "mmaps"):
        if not (data / directory).is_dir():
            raise ValueError(f"Required runtime data is absent: {data / directory}")
    db = lambda name: f'"127.0.0.1;13316;advai;{password};advai_{name}"'
    world = override((core / "src/server/apps/worldserver/worldserver.conf.dist").read_text(encoding="utf-8"), {
        "WorldServerPort": "18085", "BindIP": '"127.0.0.1"',
        "LoginDatabaseInfo": db("auth"), "CharacterDatabaseInfo": db("characters"),
        "WorldDatabaseInfo": db("world"), "DataDir": quoted(data),
        "SourceDirectory": quoted(core), "LogsDir": quoted(output / "logs"),
        "PlayerLimit": str(max(2100, population + 100)), "Warden.Enabled": "0",
        "Console.Enable": "0", "BeepAtStart": "0",
        "MySQLExecutable": quoted(mysql_client),
    })
    auth = override((core / "src/server/apps/authserver/authserver.conf.dist").read_text(encoding="utf-8"), {
        "RealmServerPort": "13724", "BindIP": '"127.0.0.1"', "LoginDatabaseInfo": db("auth"),
        "SourceDirectory": quoted(core), "LogsDir": quoted(output / "auth-logs"),
        "MySQLExecutable": quoted(mysql_client),
    })
    bots = merge((root / "conf/playerbots.conf.dist").read_text(encoding="utf-8"),
                 (root / "conf/playerbots.advai.preset").read_text(encoding="utf-8"))
    bots = override(bots, {"PlayerbotsDatabaseInfo": db("playerbots"),
        "Playerbots.CommandServerPort": "0",
        "Playerbots.MinRandomBots": str(population), "Playerbots.MaxRandomBots": str(population),
        "Playerbots.RandomBotAccountCount": str(accounts)})
    for directory in ("configs/modules", "logs", "auth-logs"):
        (output / directory).mkdir(parents=True, exist_ok=True)
    for relative, text in (("configs/worldserver.conf", world), ("configs/authserver.conf", auth),
                           ("configs/modules/playerbots.conf", bots)):
        (output / relative).write_text(text, encoding="utf-8", newline="\n")
    print(f"Created isolated runtime at {output}; MySQL 13316, auth 13724, world 18085; bots {population}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--core", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--population", type=int, default=2000)
    parser.add_argument("--accounts", type=int, help="Generated accounts, including the reserve for long breaks")
    parser.add_argument("--mysql-client", type=Path, required=True)
    args = parser.parse_args()
    required_accounts = (args.population * 12 + 9) // 10 + 50
    if args.accounts is None:
        args.accounts = required_accounts
    if not 1 <= args.population <= 10000 or args.accounts < required_accounts:
        parser.error(f"Population must be between 1 and 10000; lifecycle reserve requires at least {required_accounts} accounts")
    try:
        create(args.core, args.output, args.population, args.accounts, args.mysql_client)
    except ValueError as error:
        parser.exit(1, f"ADVAI RUNTIME SETUP FAILURE: {error}\n")
