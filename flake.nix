{
  description = "";
  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    flake-utils.url = "github:numtide/flake-utils";
    pre-commit-hooks = {
      url = "github:cachix/pre-commit-hooks.nix";
      inputs = {
        nixpkgs.follows = "nixpkgs";
        flake-utils.follows = "flake-utils";
      };
    };
  };
  outputs = {
    self,
    nixpkgs,
    flake-utils,
    pre-commit-hooks,
  }:
    flake-utils.lib.eachDefaultSystem (
      system: let
        name = "game";
        src = ./.;
        pkgs = nixpkgs.legacyPackages.${system};

        nativeBuildInputs = with pkgs; [
          cmake
          gcc
        ];

        buildInputs = with pkgs; [
          clang-tools
          git
          cglm
          glfw
          glslang
          shaderc
          vulkan-extension-layer
          vulkan-headers
          vulkan-loader
          vulkan-tools
          vulkan-validation-layers
        ];

        VK_LAYER_PATH = "${pkgs.vulkan-validation-layers}/share/vulkan/explicit_layer.d";

        mkDerivation = pkgs.stdenvNoCC.mkDerivation;
      in {
        packages.default = mkDerivation {
          inherit name src nativeBuildInputs buildInputs;
        };

        devShells.default = pkgs.mkShell {
          inherit nativeBuildInputs buildInputs VK_LAYER_PATH;
          inherit (self.checks.${system}.pre-commit-check) shellHook;
        };

        checks = {
          pre-commit-check = pre-commit-hooks.lib.${system}.run {
            inherit src;
            hooks = {
              alejandra.enable = true;
              clang-format.enable = true;
            };
          };
        };

        formatter = pkgs.alejandra;
      }
    );
}
