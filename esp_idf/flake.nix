{
  inputs = {
    esp-dev.url = "github:mirrexagon/nixpkgs-esp-dev";
    nixpkgs.follows = "esp-dev/nixpkgs";
  };

  outputs =
    {
      self,
      esp-dev,
      nixpkgs,
    }:
    let
      system = "x86_64-linux";
    in
    {
      devShells.${system}.default = esp-dev.devShells.${system}.esp-idf-full;
    };
}
