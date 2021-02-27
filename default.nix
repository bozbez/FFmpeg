let
  ffmpeg-custom = self: super: {
    ffmpeg-full = (super.ffmpeg-full.override {
      nonfreeLicensing = true;
      avresampleLibrary = false;

      wavpack = null;
      libmfx = null;

      enableLto = true;
    }).overrideAttrs (old: {
      src = ./.;
      version = "next";

      configureFlags = super.lib.remove "--disable-libwavpack" old.configureFlags;
    });
  };

  pkgs = import <nixpkgs> { overlays = [ ffmpeg-custom ]; };

  pkgs-rpi = import <nixpkgs> {
    overlays = [ ffmpeg-custom ];
    crossSystem = pkgs.lib.systems.examples.raspberryPi;
  };
in
  pkgs.ffmpeg-full
