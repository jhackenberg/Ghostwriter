{
  description = "Ghostwriter – a high-performance message broker";

  inputs = {
    nixpkgs.url = "github:nixos/nixpkgs?ref=nixos-unstable";
  };

  outputs = { self, nixpkgs }:
    let
      system = "x86_64-linux";
      pkgs = import nixpkgs { inherit system; };

      # Git submodules that are built as part of the CMake project.
      flatbuffers-src = pkgs.fetchFromGitHub {
        owner = "google";
        repo = "flatbuffers";
        rev = "6400c9b054912ae4761ab3534248b51ffa7a6c1e";
        hash = "sha256-7JjrPUTEuq/rXzj9FQI+en5ncY6yIB4wTMYS6VkxpQU=";
      };

      hdrhistogram-src = pkgs.fetchFromGitHub {
        owner = "HdrHistogram";
        repo = "HdrHistogram_c";
        rev = "933c5dc1f347358450c4cd678132dd93e6ac2134";
        hash = "sha256-g9Eyifmo2E4Lq7XlI9Q4wAEhCLeKYAIRc6SF8JHS2Ks=";
      };

      librdkafka-src = pkgs.fetchFromGitHub {
        owner = "edenhill";
        repo = "librdkafka";
        rev = "77a013b7a2611f7bdc091afa1e56b1a46d1c52f5";
        hash = "sha256-NLlg9S3bn5rAFyRa1ETeQGhFJYb/1y2ZiDylOy7xNbY=";
      };

      commonBuildInputs = [
        pkgs.boost
        pkgs.tbb
        pkgs.ucx
        pkgs.jemalloc
        pkgs.openssl
        pkgs.zlib
        pkgs.zstd
        pkgs.cyrus_sasl
        pkgs.curl
      ];

      cmakeFlags = [
        "-DCMAKE_BUILD_TYPE=Release"
        "-DGHOSTWRITER_BUILD_YSB_BENCHMARK=OFF"
        "-DGHOSTWRITER_BUILD_YSB_KAFKA_BENCHMARK=OFF"
        "-DGHOSTWRITER_BUILD_YSB_LOCAL_RUNNER=OFF"
        "-DGHOSTWRITER_BUILD_YSB_DATA_GENERATOR=OFF"
        "-DCMAKE_POLICY_VERSION_MINIMUM=3.5"
      ];

      ghostwriter = pkgs.stdenv.mkDerivation {
        pname = "ghostwriter";
        version = "0.1";

        src = self;

        nativeBuildInputs = [
          pkgs.cmake
          pkgs.gcc
          pkgs.pkg-config
          pkgs.autoPatchelfHook
        ];

        buildInputs = commonBuildInputs;
        inherit cmakeFlags;

        postUnpack = ''
          mkdir -p $sourceRoot/extern
          rm -rf $sourceRoot/extern/flatbuffers $sourceRoot/extern/hdrhistogram $sourceRoot/extern/librdkafka
          cp -r ${flatbuffers-src}  $sourceRoot/extern/flatbuffers
          cp -r ${hdrhistogram-src} $sourceRoot/extern/hdrhistogram
          cp -r ${librdkafka-src}   $sourceRoot/extern/librdkafka
          chmod -R u+w $sourceRoot/extern
        '';

        installPhase = ''
          runHook preInstall

          mkdir -p $out/bin $out/lib
          for bin in \
            benchmark_broker_node \
            benchmark_consumer \
            benchmark_producer \
            kafka_benchmark_consumer \
            kafka_benchmark_producer \
            test_client \
            test_server; do
            [ -f "$bin" ] && install -m755 "$bin" "$out/bin/"
          done

          # Install in-tree shared libraries.
          find lib -name '*.so*' -exec install -m644 {} "$out/lib/" \;

          # Strip /build/ references from RPATHs before fixup checks run.
          for f in $out/bin/* $out/lib/*.so*; do
            [ -f "$f" ] || continue
            local oldRpath=$(patchelf --print-rpath "$f" 2>/dev/null || true)
            if [[ "$oldRpath" == */build/* ]]; then
              local newRpath=$(echo "$oldRpath" | tr ':' '\n' | grep -v '/build/' | tr '\n' ':' | sed 's/:$//')
              newRpath="$out/lib:$newRpath"
              patchelf --set-rpath "$newRpath" "$f"
            fi
          done

          runHook postInstall
        '';
      };

      mkTarget = name: ghostwriter.overrideAttrs (old: {
        pname = name;
        installPhase = ''
          runHook preInstall

          mkdir -p $out/bin $out/lib

          install -m755 ${name} $out/bin/

          # Install in-tree shared libraries needed at runtime.
          find lib -name '*.so*' -exec install -m644 {} "$out/lib/" \;

          # Strip /build/ references from RPATHs.
          for f in $out/bin/* $out/lib/*.so*; do
            [ -f "$f" ] || continue
            local oldRpath=$(patchelf --print-rpath "$f" 2>/dev/null || true)
            if [[ "$oldRpath" == */build/* ]]; then
              local newRpath=$(echo "$oldRpath" | tr ':' '\n' | grep -v '/build/' | tr '\n' ':' | sed 's/:$//')
              newRpath="$out/lib:$newRpath"
              patchelf --set-rpath "$newRpath" "$f"
            fi
          done

          runHook postInstall
        '';
      });
    in
    {
      packages.${system} = {
        default = ghostwriter;
        ghostwriter = ghostwriter;
        benchmark-broker-node = mkTarget "benchmark_broker_node";
        benchmark-consumer = mkTarget "benchmark_consumer";
        benchmark-producer = mkTarget "benchmark_producer";
        kafka-benchmark-consumer = mkTarget "kafka_benchmark_consumer";
        kafka-benchmark-producer = mkTarget "kafka_benchmark_producer";
        test-client = mkTarget "test_client";
        test-server = mkTarget "test_server";
      };

      devShells.${system}.default = pkgs.mkShell {
        nativeBuildInputs = [
          pkgs.cmake
          pkgs.gcc
          pkgs.pkg-config
        ];

        buildInputs = commonBuildInputs;

        # Allow -march=native for performance-critical benchmarking code.
        NIX_ENFORCE_NO_NATIVE = false;

        shellHook = ''
          echo "Ghostwriter dev shell (GCC $(gcc -dumpversion), Boost ${pkgs.boost.version})"
        '';
      };
    };
}
