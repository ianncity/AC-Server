"""Draft static locale catalogs offline; outputs require editorial review before publication.

This development tool is not imported by the server or catalog loader. Model files
must already exist locally. Runtime ADVAI has no machine-translation dependency.
"""

import argparse
import json
from pathlib import Path
import re
import sys

from catalog import read_catalog

LANGUAGES = {"koKR": "ko", "frFR": "fr", "deDE": "de", "zhCN": "zh", "zhTW": "zh",
             "esES": "es", "esMX": "es", "ruRU": "ru"}


def draft(model: Path, output: Path, threads: int) -> None:
    import ctranslate2
    import sentencepiece
    from opencc import OpenCC

    source = Path(__file__).parent / "catalog"
    english = read_catalog(source / "enUS.txt")
    facts = json.loads((source / "facts/enUS.json").read_text(encoding="utf-8"))
    output.mkdir(parents=True, exist_ok=True)
    processor = sentencepiece.SentencePieceProcessor(model_file=str(model / "sentencepiece.bpe.model"))
    translator = ctranslate2.Translator(str(model), device="cpu", compute_type="int8",
                                        inter_threads=1, intra_threads=threads)
    traditional = OpenCC("s2twp")
    cache_path = output / "translation-cache.json"
    cache = json.loads(cache_path.read_text(encoding="utf-8")) if cache_path.exists() else {}
    def linguistic(text: str) -> bool:
        return any(character.isalpha() for character in text)

    sources = sorted({piece.strip() for lines in english.values() for line in lines
                      for piece in line.split("%facts") if linguistic(piece)} |
                     {text for text in facts.values() if linguistic(text)})
    for language in dict.fromkeys(LANGUAGES.values()):
        missing = [text for text in sources if f"{language}:{text}" not in cache]
        for begin in range(0, len(missing), 24):
            batch = missing[begin:begin + 24]
            # "Quest" is often mistranslated as searching; "mission" retains its game-task meaning.
            prepared = [re.sub(r"\bquesting\b", "doing missions", text, flags=re.I) for text in batch]
            prepared = [re.sub(r"\bquests\b", "missions", text, flags=re.I) for text in prepared]
            prepared = [re.sub(r"\bquest\b", "mission", text, flags=re.I) for text in prepared]
            prepared = [re.sub(r"\{([0-3])\}", lambda match: str(765430010 + int(match[1])), text)
                        for text in prepared]
            tokens = [["__en__", *processor.encode(text, out_type=str), "</s>"] for text in prepared]
            results = translator.translate_batch(tokens, target_prefix=[[f"__{language}__"] for _ in batch],
                                                   beam_size=3, max_decoding_length=128, disable_unk=True)
            for original, result in zip(batch, results, strict=True):
                pieces = [token for token in result.hypotheses[0]
                          if token not in (f"__{language}__", "</s>", "<s>", "<pad>")]
                text = re.sub(r"\s+", " ", processor.decode(pieces)).strip()
                if not text or "<unk>" in pieces:
                    raise ValueError(f"Translation failed for {language}:{original}")
                if language == "fr" and re.search(r"\bquest", original, re.I):
                    text = re.sub(r"\bmissions\b", "quêtes", text, flags=re.I)
                    text = re.sub(r"\bmission\b", "quête", text, flags=re.I)
                if language == "ko" and "arena" in original.lower():
                    text = text.replace("경기장", "투기장")
                for index in range(4):
                    number = str(765430010 + index)
                    pattern = r"(?<!\d)" + r"[\s,.\u00a0]*".join(number) + r"(?!\d)"
                    text = re.sub(pattern, "{" + str(index) + "}", text)
                cache[f"{language}:{original}"] = text
            cache_path.write_text(json.dumps(cache, ensure_ascii=False, indent=2), encoding="utf-8")
            print(f"Draft {language}: {min(begin + 24, len(missing))}/{len(missing)} new phrases", flush=True)
    for locale, language in LANGUAGES.items():
        lines = ["# Static translation draft: editorial review required."]
        for outcome, variants in english.items():
            lines.extend(("", f"[{outcome}]"))
            for variant in variants:
                pieces = variant.split("%facts")
                translated = [cache[f"{language}:{piece.strip()}"] if linguistic(piece) else piece.strip()
                              for piece in pieces]
                if len(pieces) == 2:
                    before = " " if pieces[0] and pieces[0][-1].isspace() else ""
                    after = " " if pieces[1] and pieces[1][0].isspace() else ""
                    text = translated[0] + before + "%facts" + after + translated[1]
                else:
                    text = translated[0]
                if locale == "zhTW":
                    text = traditional.convert(text)
                lines.append(text)
        destination = output / f"{locale}.txt"
        destination.write_text("\n".join(lines) + "\n", encoding="utf-8", newline="\n")
        translated_facts = {key: cache[f"{language}:{text}"] if linguistic(text) else text for key, text in facts.items()}
        if locale == "zhTW":
            translated_facts = {key: traditional.convert(text) for key, text in translated_facts.items()}
        fact_destination = output / "facts" / f"{locale}.json"
        fact_destination.parent.mkdir(parents=True, exist_ok=True)
        fact_destination.write_text(json.dumps(translated_facts, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
        print(f"Wrote {destination.name}; review and catalog validation remain required", flush=True)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--model", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--threads", type=int, default=4)
    args = parser.parse_args()
    if args.output.resolve() == (Path(__file__).parent / "catalog").resolve():
        parser.error("Draft output must be separate from the reviewed source catalogs")
    try:
        draft(args.model, args.output, args.threads)
    except (ValueError, RuntimeError) as error:
        print(f"ADVAI TRANSLATION DRAFT FAILURE: {error}", file=sys.stderr)
        sys.exit(1)
