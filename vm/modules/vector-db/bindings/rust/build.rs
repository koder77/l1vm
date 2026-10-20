// Locate libvecdb and link against it.
//
// Search order:
//   1. the VECDB_LIB_DIR environment variable
//   2. ../../ relative to this crate (the vector-db repository root)
//
// The library directory is baked into the binary as an rpath on ELF and
// Mach-O targets so `cargo test` and locally built binaries find
// libvecdb.so without LD_LIBRARY_PATH. Set VECDB_LIB_DIR at build time
// to point at a different installation.

use std::env;
use std::path::{Path, PathBuf};

fn main() {
    let dir = env::var_os("VECDB_LIB_DIR")
        .map(PathBuf::from)
        .unwrap_or_else(|| {
            PathBuf::from(env::var("CARGO_MANIFEST_DIR").unwrap())
                .join("../..")
                .canonicalize()
                .expect("cannot resolve the vector-db repository root")
        });

    let lib_names = ["libvecdb.so", "libvecdb.dylib", "vecdb.dll"];
    if !lib_names.iter().any(|name| dir.join(name).exists()) {
        panic!(
            "libvecdb was not found in {}.\n\
             Build it with `make` in the vector-db repository root, or point \
             VECDB_LIB_DIR at the directory containing it.",
            dir.display()
        );
    }

    println!("cargo:rustc-link-search=native={}", dir.display());
    println!("cargo:rustc-link-lib=dylib=vecdb");

    // Keep the just-built library discoverable at run time.
    if cfg!(target_os = "linux") || cfg!(target_os = "macos") {
        println!("cargo:rustc-link-arg=-Wl,-rpath,{}", dir.display());
    }

    println!("cargo:rerun-if-env-changed=VECDB_LIB_DIR");
    for name in lib_names {
        println!(
            "cargo:rerun-if-changed={}",
            Path::new(&dir).join(name).display()
        );
    }
}
