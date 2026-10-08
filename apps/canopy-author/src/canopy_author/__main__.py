"""Compile a verified local accepted GrowBIM revision to a game-only semantic pack."""
import argparse
from pathlib import Path

from .pack import compile_pack, pack_sha256
from .source import read_source


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, required=True, help="GrowBIM local project store root")
    parser.add_argument("--project-id", required=True)
    parser.add_argument("--revision", required=True, help="immutable accepted design revision")
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    binary = compile_pack(read_source(args.root, args.project_id, args.revision))
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_bytes(binary)
    print(f"{pack_sha256(binary)}  {args.output}")


if __name__ == "__main__":
    main()
