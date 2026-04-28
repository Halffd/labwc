{
  description = "labwc - A lightweight wlroots-based Wayland window manager";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    flake-utils.url = "github:numtide/flake-utils";
  };

  outputs = { self, nixpkgs, flake-utils }:
    flake-utils.lib.eachDefaultSystem (system:
      let
        pkgs = nixpkgs.legacyPackages.${system};
        # Get the labwc sources - in a real deployment you'd use fetchFromGitHub
        # For local development, use self
        src = if self ? src then self.src else pkgs.fetchFromGitHub {
          owner = "Halffd";
          repo = "labwc";
          rev = "v0.9.5";
          sha256 = "sha256-XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX";
        };
      in
      {
        packages = {
          labwc-unwrapped = pkgs.stdenv.mkDerivation {
            pname = "labwc-unwrapped";
            version = "0.9.5";
            inherit src;

            nativeBuildInputs = with pkgs; [
              meson
              ninja
              scdoc
              pkg-config
              wayland
              wayland-protocols
            ];

            buildInputs = with pkgs; [
              wlroots_0_20
              libinput
              xkbcommon
              libxml2
              cairo
              pango
              librsvg
              libdrm
              pixman
              freetype2
              fontconfig
              libxcb
              libxcb-ewmh
              libxcb-icccm
              glib
              gdk-pixbuf
              libpng
              hicolor-icon-theme
              dbus
              seatd
            ];

            mesonFlags = [
              "-Dicon=enabled"
              "-Dxwayland=enabled"
            ];

            buildPhase = ''
              runHook preBuild
              meson setup build --prefix=$out --buildtype=release "${builtins.concatStringsSep "\" \"" mesonFlags}"
              meson compile -C build
              runHook postBuild
            '';

            installPhase = ''
              runHook preInstall
              meson install -C build
              runHook postInstall
            '';

            dontFixMesonRunPath = true;
          };

          labwc = pkgs.symlinkJoin {
            name = "labwc-full";
            paths = [ self.packages.${system}.labwc-unwrapped ];
          };

          labwc-cli = pkgs.stdenv.mkDerivation {
            pname = "labwc-cli";
            version = "0.9.5";
            inherit src;

            nativeBuildInputs = with pkgs; [
              meson
              ninja
              pkg-config
            ];

            buildInputs = with pkgs; [
              cairo
            ];

            buildPhase = ''
              runHook preBuild
              meson setup build-cli --prefix=$out --buildtype=release
              meson compile -C build-cli labwc-cli
              runHook postBuild
            '';

            installPhase = ''
              runHook preInstall
              install -Dm755 build-cli/labwc-cli $out/bin/labwc-cli
              runHook postInstall
            '';
          };

          default = self.packages.${system}.labwc;
        };

        devShells.default = pkgs.mkShell {
          buildInputs = with pkgs; [
            self.packages.${system}.labwc-unwrapped
            meson
            ninja
            scdoc
            pkg-config
          ];

          shellHook = ''
            echo "labwc dev shell"
            echo "Build with: meson setup build && meson compile -C build"
            echo "Develop: meson compile -C build && ./build/labwc"
          '';
        };
      }
    );
}