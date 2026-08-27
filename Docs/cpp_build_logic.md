# Native build

Use the Makefile for the C++ GDExtension. The default target is
`template_debug`.

```bash
make build
make rebuild
make clean
```

Useful formatting and tooling targets are listed by `make help`. For build
details, inspect `Makefile`, `build.py`, and `src_cpp/meson.build`; those files
are authoritative.
