{
  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    # The Rust toolchain of the Schemify plugin: Rust 1.95.0 with rust-lld from rust-overlay at
    # the rev Schemify's flake.lock pins, so `./build.sh` gives the same bytes as Schemify's CI.
    rust-overlay = {
      url = "github:oxalica/rust-overlay";
      inputs.nixpkgs.follows = "nixpkgs";
    };
  };

  outputs = { self, nixpkgs, rust-overlay }:
    let
      system = "x86_64-linux";
      pkgs = import nixpkgs {
        inherit system;
        overlays = [ rust-overlay.overlays.default ];
      };

      volare = pkgs.python3Packages.buildPythonPackage rec {
        pname = "volare";
        version = "0.20.6";
        pyproject = true;
        src = pkgs.fetchPypi {
          inherit pname version;
          sha256 = "a2ebd9b8a81de144dbc352eab71670e7e4b3e4713272387479a75935569e076a";
        };
        build-system = with pkgs.python3Packages; [ poetry-core ];
        dependencies = with pkgs.python3Packages; [
          click
          httpx
          pcpp
          pyyaml
          rich
          zstandard
        ];
        pythonRelaxDeps = [ "rich" ];
        doCheck = false;
      };
    in
    {
      # The library only. No Xyce: characterise() and the C FFI always use ngspice,
      # which is shelled out to at runtime (never linked) and is in cache.nixos.org.
      # Xyce is only reachable through `gmid_runner --simulator xyce` (dev shell).
      packages.${system}.default = pkgs.stdenv.mkDerivation {
        pname = "gmidvisualizer";
        version = "0.1.0";
        src = ./cpp;
        nativeBuildInputs = with pkgs; [ cmake ninja ];
        # Puts ngspice on PATH for whoever installs this, which the .so needs to run.
        propagatedBuildInputs = [ pkgs.ngspice ];
        doCheck = true;   # tests/regression.cpp; the cgg test runs ngspice
        nativeCheckInputs = [ pkgs.ngspice ];
      };

      devShells.${system}.default = pkgs.mkShell {
        CC = "clang";
        CXX = "clang++";

        packages = [
          # The Schemify plugin (root crate): Rust 1.95.0 with the wasm32 std and rust-lld.
          (pkgs.rust-bin.stable."1.95.0".default.override {
            targets = [ "wasm32-unknown-unknown" ];
          })
        ] ++ (with pkgs; [
          cmake
          ninja
          clang_19
          llvmPackages_19.libcxx

          # Simulators (optional — needed to actually run sweeps)
          ngspice
          (xyce.override { enableDocs = false; enableTests = false; })

          # PDK installer (optional — needed to test with real process models)
          volare
        ]);
      };
    };
}
