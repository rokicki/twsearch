fn main() {
    cxx_build::bridge("src/rs/main.rs")
        .file("src/cpp/antipode.cpp")
        .file("src/cpp/calcsymm.cpp")
        .file("src/cpp/canon.cpp")
        .file("src/cpp/cmdlineops.cpp")
        .file("src/cpp/filtermoves.cpp")
        .file("src/cpp/findalgo.cpp")
        .file("src/cpp/generatingset.cpp")
        .file("src/cpp/god.cpp")
        .file("src/cpp/index.cpp")
        .file("src/cpp/parsemoves.cpp")
        .file("src/cpp/prunetable.cpp")
        .file("src/cpp/puzdef.cpp")
        .file("src/cpp/readksolve.cpp")
        .file("src/cpp/rustapi.cpp")
        .file("src/cpp/solve.cpp")
        .file("src/cpp/test.cpp")
        .file("src/cpp/threads.cpp")
        .file("src/cpp/twsearch.cpp")
        .file("src/cpp/util.cpp")
        .file("src/cpp/workchunks.cpp")
        .file("src/cpp/rotations.cpp")
        .file("src/cpp/orderedgs.cpp")
        .file("src/cpp/wasmapi.cpp")
        .file("src/cpp/cityhash/src/city.cc")
        .file("src/cpp/coset.cpp")
        .file("src/cpp/descsets.cpp")
        .file("src/cpp/ordertree.cpp")
        .file("src/cpp/unrotate.cpp")
        .file("src/cpp/shorten.cpp")
        .flag_if_supported("-std=c++14")
        .flag("-DWASM")
        .flag("-DASLIBRARY")
        .flag("-DTWSEARCH_VERSION=v0.4.2-7-g4a9107fa")
        // .flag("-lpthread") # Unneeded?
        .flag("-DUSE_PTHREADS")
        .flag("-DHAVE_FFSLL")
        .compile("twsearch-rs");

    // println!("cargo:rerun-if-changed=src/main.rs");
    // println!("cargo:rerun-if-changed=src/blobstore.cc");
    // println!("cargo:rerun-if-changed=include/blobstore.h");
}
