let
  ffmpeg-rpi = self: super: {
    raspberrypifw-hardfp = super.stdenv.mkDerivation rec {
      pname = "raspberrypi-firmware-hardfp";
      version = "1.20200601";

      src = super.fetchFromGitHub {
        owner = "raspberrypi";
        repo = "firmware";
        rev = version;
        sha256 = "1vm038f9digwg8gdxl2bypzlip3ycjb6bl56274gh5i9abl6wjvf";
      };

      outputs = [ "out" "lib" "dev" ];

      installPhase = ''
        mkdir -p $out $lib $dev

        cp -r hardfp/opt/vc/bin $out
        cp -r hardfp/opt/vc/lib $lib
        cp -r hardfp/opt/vc/include $dev
      '';
    };

    libgpiod = super.libgpiod.overrideAttrs (old: {
      configureFlags = old.configureFlags ++ super.lib.optionals
        (super.stdenv.hostPlatform != super.stdenv.buildPlatform) [
        "ac_cv_func_malloc_0_nonnull=yes"
        "ac_cv_func_realloc_0_nonnull=yes"
      ];
    });

    ffmpeg_4 = (super.ffmpeg_4.override {
      x265 = null;
      dav1d = null;
    }).overrideAttrs (old: {
      src = ./.;
      version = "next";

      configureFlags = (super.lib.subtractLists [
        "--enable-shared"

        "--enable-libass"
        "--enable-libx265"
        "--enable-libdav1d"
      ] old.configureFlags) ++ [
        "--extra-ldflags=-latomic"
        "--enable-static"
        "--disable-shared"

        "--enable-libsrt"
        "--enable-libgpiod"

        "--enable-nonfree"
        "--enable-mmal"
        "--enable-omx"
        "--enable-omx-rpi"
      ];

      buildInputs = (super.lib.subtractLists [
        self.libass
      ] old.buildInputs) ++ [
        self.srt
        self.libgpiod

        self.raspberrypifw-hardfp.lib
        self.raspberrypifw-hardfp.dev
      ];

      preConfigure = ''
        export CFLAGS="-isystem${self.raspberrypifw-hardfp.dev}/include/IL"
      '';
    });
  };

  pkgs = import <nixpkgs> {};
  pkgs-rpi = import <nixpkgs> {
    overlays = [ ffmpeg-rpi ];
    crossSystem = pkgs.lib.systems.examples.raspberryPi;
  };

in
  pkgs-rpi.ffmpeg_4
