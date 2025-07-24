{
  nixConfig.bash-prompt-prefix = "jnlive>";
  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
  };

  outputs = { self, nixpkgs }: {
    devShells = let
      makeDevShell = { system, nixpkgs }:
      let
        pkgs = import nixpkgs {
          inherit system;
          config = {
            allowUnfree = true;
          };
        };
        llvmPackagesToUse = pkgs.llvmPackages_20;

#   libcxxCairomm = pkgs.callPackage (pkgs.path + "/pkgs/development/libraries/cairomm") {
#     stdenv = pkgs.llvmPackages_20.libcxxStdenv;
#     libsigcxx = pkgs.libsigcxx.override { stdenv = pkgs.llvmPackages_20.libcxxStdenv; };
#     boost = pkgs.boost.override { stdenv = pkgs.llvmPackages_20.libcxxStdenv; };
#   };
#   libcxxCairomm = pkgs.callPackage (pkgs.path + "/pkgs/development/libraries/cairomm") {
#     stdenv = pkgs.llvmPackages_20.libcxxStdenv;
# ` };

      in 
        llvmPackagesToUse.stdenv.mkDerivation {
          name = "jnlive dev shell";
          nativeBuildInputs = with pkgs; [
            cmake
            pkg-config
            bashInteractive # needed for bash shell in vs code
            git
            less
            which
            ninja
            llvmPackagesToUse.bintools
            llvmPackagesToUse.clang-tools # for clangd
            #clangd-in-path
          ];

          buildInputs = with pkgs; [
            jack2
            lilv
            libusb1
            hidapi
            jsoncpp
            zix
            suil
            gtk3
            gtkmm3
            #libcxxCairomm
            xorg.libX11
          ];
          shellHook = ''
            echo "jnlive build shell started."
          ''  ;
          NIX_HARDENING_ENABLE = "";
        };
    
    in {
      x86_64-linux.default = makeDevShell { system = "x86_64-linux"; inherit nixpkgs; };
      aarch64-linux.default = makeDevShell { system = "aarch64-linux"; inherit nixpkgs; };
    };
  };
}
