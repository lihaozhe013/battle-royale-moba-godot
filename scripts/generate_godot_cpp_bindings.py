#!/usr/bin/env python3

import shutil
import sys
from pathlib import Path


def main():
    if len(sys.argv) != 9:
        raise SystemExit(
            "Usage: generate_godot_cpp_bindings.py GENERATOR_DIR API_FILE INTERFACE_FILE "
            "TEMPLATE_GET_NODE BITS PRECISION GENERATED_DIR FLAT_OUTPUT_DIR"
        )

    generator_dir = Path(sys.argv[1]).resolve()
    api_file = Path(sys.argv[2]).resolve()
    interface_file = Path(sys.argv[3]).resolve()
    use_template_get_node = sys.argv[4].lower() == "true"
    bits = sys.argv[5]
    precision = sys.argv[6]
    generated_dir = Path(sys.argv[7]).resolve()
    flat_output_dir = Path(sys.argv[8]).resolve()

    sys.path.insert(0, str(generator_dir))
    from binding_generator import generate_bindings, get_file_list

    generate_bindings(
        api_filepath=str(api_file),
        interface_filepath=str(interface_file),
        use_template_get_node=use_template_get_node,
        bits=bits,
        precision=precision,
        output_dir=str(generated_dir),
    )

    for generated_file in get_file_list(str(api_file), str(generated_dir), headers=True, sources=True):
        source = Path(generated_file)
        relative_path = source.relative_to(generated_dir)
        flat_name = "godotcpp_" + "__".join(relative_path.parts)
        destination = flat_output_dir / flat_name
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source, destination)


if __name__ == "__main__":
    main()
