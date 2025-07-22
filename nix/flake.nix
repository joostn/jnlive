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
      in 
        pkgs.stdenvNoCC.mkDerivation {
          name = "jnlive dev shell";
          nativeBuildInputs = with pkgs; [
            cmake
            pkg-config
            bashInteractive # needed for bash shell in vs code
            git
            less
            which
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
            libX11
          ];
          shellHook = ''
            jnlive build shell started.
          ''  ;
        };
    
    in {
      x86_64-linux.default = makeDevShell { system = "x86_64-linux"; inherit nixpkgs; };
      aarch64-linux.default = makeDevShell { system = "aarch64-linux"; inherit nixpkgs; };
    };
  };
}
