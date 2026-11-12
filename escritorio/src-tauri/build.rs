// Compila el núcleo de SintéticaOS (firmware/nucleo, el mismo de la placa) y
// su API en C (firmware/api) adentro de la app.
use std::path::PathBuf;

fn main() {
    let raiz = PathBuf::from(std::env::var("CARGO_MANIFEST_DIR").unwrap()).join("../../firmware");
    let nucleo = raiz.join("nucleo");
    let mut b = cc::Build::new();
    b.cpp(true).std("c++17").include(&nucleo).include(raiz.join("api")).warnings(false);
    // En Windows, clang-cl: el núcleo usa __builtin_* (como clang en el wasm y el NDK).
    if std::env::var("CARGO_CFG_TARGET_ENV").as_deref() == Ok("msvc") {
        b.compiler("clang-cl");
    }
    for e in std::fs::read_dir(&nucleo).unwrap() {
        let p = e.unwrap().path();
        if p.extension().is_some_and(|x| x == "cpp") {
            b.file(&p);
        }
    }
    b.file(raiz.join("api/sintetica_c.cpp"));
    b.compile("sinteticaos");
    println!("cargo:rerun-if-changed=../../firmware/nucleo");
    println!("cargo:rerun-if-changed=../../firmware/api");
    tauri_build::build()
}
