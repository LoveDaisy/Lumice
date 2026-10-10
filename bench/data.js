window.BENCHMARK_DATA = {
  "lastUpdate": 1791672788968,
  "repoUrl": "https://github.com/LoveDaisy/Lumice",
  "entries": {
    "Single-worker Throughput": [
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "4b269f31e994c541505f45df5390fa783fd5eb4c",
          "message": "Merge pull request #372 from LoveDaisy/feat/multi-renderer-gpu-backend\n\nfeat: device-fused multi-renderer sessions on Metal and CUDA (scrum multi-renderer-gpu-backend)",
          "timestamp": "2026-09-16T14:15:30+08:00",
          "tree_id": "2b95071bd3ce2b50817d54682a83c0363b9638fe",
          "url": "https://github.com/LoveDaisy/Lumice/commit/4b269f31e994c541505f45df5390fa783fd5eb4c"
        },
        "date": 1789540147922,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 428306.1,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 594372.9,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 424915,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 379585.4,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor                \\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "eff2ea9db23ead9488cd19184d31ecc1fce4f339",
          "message": "Merge pull request #373 from LoveDaisy/feat/axis-modal-custom-preset-memory\n\nfeat(gui): remember each crystal's last Custom axis triple in the edit modal",
          "timestamp": "2026-09-16T22:02:36+08:00",
          "tree_id": "e82f08c3e0d37f056f07a7e1e0c21781d6a0133a",
          "url": "https://github.com/LoveDaisy/Lumice/commit/eff2ea9db23ead9488cd19184d31ecc1fce4f339"
        },
        "date": 1789567999566,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 427075.1,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 593394.1,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 429031,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 378895.1,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor                \\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "f28752902bc36099d973c676533fb29e7e6e1991",
          "message": "Merge pull request #374 from LoveDaisy/feat/cuda-discrete-spectrum-wl-pool-cache\n\nfix(cuda): rebuild the wl pool every BeginSession under a discrete spectrum",
          "timestamp": "2026-09-16T22:34:39+08:00",
          "tree_id": "8e6d0b7a6f8bcb70d49405b289412534a3c5eb49",
          "url": "https://github.com/LoveDaisy/Lumice/commit/f28752902bc36099d973c676533fb29e7e6e1991"
        },
        "date": 1789569944719,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 374542.1,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 592552.3,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 509509.4,
            "unit": "rays/sec",
            "extra": "CPU: INTEL(R) XEON(R) PLATINUM 8573C\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 379167.2,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor                \\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "ab1ba3499938a637c44b1102fef825610033d17e",
          "message": "Merge pull request #375 from LoveDaisy/feat/isa-engine-dll-dispatch\n\nrelease: one shell per entry point + two internal engine libraries picked by CPUID / glibc-hwcaps (scrum-566)",
          "timestamp": "2026-09-17T09:29:18+08:00",
          "tree_id": "95064d4b64e5245da0848c6223ef22c769d2c506",
          "url": "https://github.com/LoveDaisy/Lumice/commit/ab1ba3499938a637c44b1102fef825610033d17e"
        },
        "date": 1789609465371,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 471737.6,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 594318.7,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 537728,
            "unit": "rays/sec",
            "extra": "CPU: INTEL(R) XEON(R) PLATINUM 8573C\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 380007.2,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor                \\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "695de9ed0e5a901e96e0c747c5b6235ed051c10d",
          "message": "Merge pull request #377 from LoveDaisy/chore/ci-e2e-slow-macos-rest-timeout\n\nfix(ci): widen E2E Slow (macOS rest) step timeout 15→25 min",
          "timestamp": "2026-09-17T16:56:21+08:00",
          "tree_id": "de8b1ecd633d6f3a4476f875f401bac57c4c5feb",
          "url": "https://github.com/LoveDaisy/Lumice/commit/695de9ed0e5a901e96e0c747c5b6235ed051c10d"
        },
        "date": 1789636073055,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 331473.3,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 594822.7,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 388228.8,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 9V74 80-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 444926.6,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 9V74 80-Core Processor                \\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "444cec705397fdcccafdc87329cb670fe0a4983d",
          "message": "Merge pull request #376 from LoveDaisy/feat/cuda-persistent-device-buffers\n\nperf(cuda): persist lat_lut buffers and make EnsureSessionBuffers grow-only",
          "timestamp": "2026-09-17T17:17:49+08:00",
          "tree_id": "df38477d8c88f2a853e31bcf30cab5d180449a9c",
          "url": "https://github.com/LoveDaisy/Lumice/commit/444cec705397fdcccafdc87329cb670fe0a4983d"
        },
        "date": 1789637232764,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 396954.7,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 595726.6,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 425231.2,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 380484.2,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor                \\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "fbd9e333c05a864d3df8756f7a171910bc9fd37a",
          "message": "Merge pull request #378 from LoveDaisy/feat/cpu-worker-side-projection\n\nperf(core): move legacy-CPU per-ray projection onto the simulator workers",
          "timestamp": "2026-09-18T04:05:22+08:00",
          "tree_id": "14b5ad3b0df1857b2e43b1dde29f1d4fdd79955a",
          "url": "https://github.com/LoveDaisy/Lumice/commit/fbd9e333c05a864d3df8756f7a171910bc9fd37a"
        },
        "date": 1789676296732,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 316138.2,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 509825.9,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 367533.3,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 311799.1,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor                \\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "f392ee3a91e7a6975dd11b1c7e2867ef8800efa0",
          "message": "Merge pull request #379 from LoveDaisy/feat/cli-raw-float-export\n\nfeat(cli): --format npy raw float32 XYZ export with sidecar metadata",
          "timestamp": "2026-09-19T00:39:06+08:00",
          "tree_id": "7f3a5e880b4c85c0d6f7ca6ecef4f6edd0373fc9",
          "url": "https://github.com/LoveDaisy/Lumice/commit/f392ee3a91e7a6975dd11b1c7e2867ef8800efa0"
        },
        "date": 1789750236767,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 424241.3,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 511972.7,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 360873.5,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 284794.9,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 9V74 80-Core Processor                \\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "53fa48878ac86e2a3794c6b98f02d20fc12744c0",
          "message": "Merge pull request #380 from LoveDaisy/fix/fp32-accumulator-compensation\n\nfix(server): widen long-chain fp32 accumulators to double (sub-sun hue drift with ray count)",
          "timestamp": "2026-09-19T00:59:38+08:00",
          "tree_id": "f432b8348761a5f45dc462565e3814371088db89",
          "url": "https://github.com/LoveDaisy/Lumice/commit/53fa48878ac86e2a3794c6b98f02d20fc12744c0"
        },
        "date": 1789751524749,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 305382.7,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 508957,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 359962.6,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 306914.6,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor                \\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "11cc9ff591a8d177621c226af7d970ad200ad6fb",
          "message": "Merge pull request #381 from LoveDaisy/feat/lmc-linear-xyz-texture\n\nfeat(gui): store the .lmc preview texture as unexposed linear XYZ (format v5)",
          "timestamp": "2026-09-19T02:24:05+08:00",
          "tree_id": "46941acdc2657c651126351d9db7f5e5664612d3",
          "url": "https://github.com/LoveDaisy/Lumice/commit/11cc9ff591a8d177621c226af7d970ad200ad6fb"
        },
        "date": 1789756612155,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 330374.8,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 507941.3,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 362034,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 309191.1,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor                \\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "6f38f1a7758e76516f2e71424805ca2f32020ade",
          "message": "Merge pull request #382 from LoveDaisy/feat/perf-followthrough\n\nperf: follow-through scrum — per-platform worker cap, GUI startup prewarm, CUDA hit-budget fix, PostSnapshot parallelization, measurement discipline",
          "timestamp": "2026-09-19T15:59:56+08:00",
          "tree_id": "6bd42c6a11fb1d48fb9d34ac461583994b914805",
          "url": "https://github.com/LoveDaisy/Lumice/commit/6f38f1a7758e76516f2e71424805ca2f32020ade"
        },
        "date": 1789805602900,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 345393.1,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 510482,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 360608.1,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 307056.7,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor                \\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "96c437719d62599f74bab998e9baa5a4192900a7",
          "message": "Merge pull request #384 from LoveDaisy/feat/lmc-f16-texture-dual-path\n\nfeat(gui): .lmc v6 float16 texture with the live preview quantized through the same codec",
          "timestamp": "2026-09-20T01:45:06+08:00",
          "tree_id": "6294bd9290986f14645da62d4429859ba9129d91",
          "url": "https://github.com/LoveDaisy/Lumice/commit/96c437719d62599f74bab998e9baa5a4192900a7"
        },
        "date": 1789840607256,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 333710.9,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 508452.8,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 362422.5,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 312225.9,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor                \\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "07d02b311b0c719947d1ddb96023a365f573e2c7",
          "message": "Merge pull request #385 from LoveDaisy/chore/cuda-canonical-throughput-5090\n\ndocs(perf): first RTX 5090 D canonical throughput columns (home-wsl / home-win) + B/E drain-plane cost matrix",
          "timestamp": "2026-09-20T03:29:12+08:00",
          "tree_id": "42eb1aecd5d088d6fafd75cc593704374199bd52",
          "url": "https://github.com/LoveDaisy/Lumice/commit/07d02b311b0c719947d1ddb96023a365f573e2c7"
        },
        "date": 1789846782746,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 391994.8,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 507315.6,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 357320.1,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 310642.3,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor                \\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "76be31ac78abe09f12455eb5916a081fbac528cf",
          "message": "Merge pull request #386 from LoveDaisy/feat/parallel-rows-idle-core-budget\n\nfix(server): size ParallelRows by an explicit idle-core thread budget (undo the GUI-poll worker regression from #382)",
          "timestamp": "2026-09-20T05:54:03+08:00",
          "tree_id": "622c8e9b60a4128f396e320415097e083aab2610",
          "url": "https://github.com/LoveDaisy/Lumice/commit/76be31ac78abe09f12455eb5916a081fbac528cf"
        },
        "date": 1789855503430,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "Ubuntu ARM64",
            "value": 510490.3,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 358818.9,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 354306.1,
            "unit": "rays/sec",
            "extra": "CPU: Intel(R) Xeon(R) Platinum 8370C CPU @ 2.80GHz\\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "88a667fb94867eaa45b47b789e101b9b505a99ee",
          "message": "Merge pull request #387 from LoveDaisy/docs/thread-budget-owner-ruling\n\ndocs(gui): record the owner's ruling on the capped render thread budget",
          "timestamp": "2026-09-20T07:45:28+08:00",
          "tree_id": "659f93e361c41c0c2e674a5a3bba6178131d04b4",
          "url": "https://github.com/LoveDaisy/Lumice/commit/88a667fb94867eaa45b47b789e101b9b505a99ee"
        },
        "date": 1789862100870,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "Ubuntu ARM64",
            "value": 510601.3,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 417255.1,
            "unit": "rays/sec",
            "extra": "CPU: Intel(R) Xeon(R) Platinum 8370C CPU @ 2.80GHz\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 313501.7,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor                \\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "61b01911736670be1cb86ee587fc006d12b7eefd",
          "message": "Merge pull request #383 from LoveDaisy/fix/cuda-drain-window-fp32-plane\n\nfix(cuda): fold the fp32 XYZ device plane into a double plane every 8 batches (drain-window ledger drift)",
          "timestamp": "2026-09-20T09:41:39+08:00",
          "tree_id": "7251f6578c75bb2a709a992bf46e1bc045a57fe2",
          "url": "https://github.com/LoveDaisy/Lumice/commit/61b01911736670be1cb86ee587fc006d12b7eefd"
        },
        "date": 1789869282759,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "Ubuntu ARM64",
            "value": 511239.6,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 360820.4,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 317142,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor                \\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "747b2ec2a60ffe5e75331b2c0b3753ed05411959",
          "message": "Merge pull request #388 from LoveDaisy/fix/startup-calibration-test-address-reuse\n\ntest(gui): evidence the startup-calibration server rebuild by its worker-count tracker, not by address",
          "timestamp": "2026-09-20T10:28:39+08:00",
          "tree_id": "1b37adad964db6f5ecb871687f0eeb0ee499b8f3",
          "url": "https://github.com/LoveDaisy/Lumice/commit/747b2ec2a60ffe5e75331b2c0b3753ed05411959"
        },
        "date": 1789871944685,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 404567.1,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 508467.6,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 538730.5,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 9V45 96-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 355395.1,
            "unit": "rays/sec",
            "extra": "CPU: INTEL(R) XEON(R) PLATINUM 8573C\\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "94b90ab6c623e1aa3679f7aa1718a7cd0730b4fb",
          "message": "Merge pull request #389 from LoveDaisy/task/gui-view-copy-resync-mechanism\n\nfix(gui): resync the edit modal from the pool every frame — Exclude no longer overwritten while the editor is open",
          "timestamp": "2026-09-20T16:23:59+08:00",
          "tree_id": "be5ece68186e20452d6b1ff247e8d4d66e216b7c",
          "url": "https://github.com/LoveDaisy/Lumice/commit/94b90ab6c623e1aa3679f7aa1718a7cd0730b4fb"
        },
        "date": 1789893319397,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 413660.8,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 510017.4,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 361757.5,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 315206.7,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor                \\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "337ad7c9b1160379b054d8d652114eb883ba7c36",
          "message": "Merge pull request #390 from LoveDaisy/fix/composite-preview-p99-flake-margin\n\ntest(gui): drop the composite re-run p99 ratio that never saw the defect (task-582)",
          "timestamp": "2026-09-20T19:39:18+08:00",
          "tree_id": "dc6df54e496534e5ec18465303203dbf62ed18fd",
          "url": "https://github.com/LoveDaisy/Lumice/commit/337ad7c9b1160379b054d8d652114eb883ba7c36"
        },
        "date": 1789904912436,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 377095.1,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 510595.4,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 396607.6,
            "unit": "rays/sec",
            "extra": "CPU: INTEL(R) XEON(R) PLATINUM 8573C\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 385754.5,
            "unit": "rays/sec",
            "extra": "CPU: INTEL(R) XEON(R) PLATINUM 8573C\\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "048801761115ac2403b66262d5f987c2146ca3b1",
          "message": "Merge pull request #391 from LoveDaisy/feat/show-product-version\n\nfeat: show the product version — C API, title bar, --version, startup log, .lmc (task-585)",
          "timestamp": "2026-09-20T20:10:01+08:00",
          "tree_id": "93e17fbc3606c3e3386802a0e7be910d196ab10f",
          "url": "https://github.com/LoveDaisy/Lumice/commit/048801761115ac2403b66262d5f987c2146ca3b1"
        },
        "date": 1789907135202,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 426779.7,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 511008.1,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 368973.6,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 9V74 80-Core Processor                \\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "a09b72909d5b03ab532b9f908e01ac4ef233c4ac",
          "message": "Merge pull request #392 from LoveDaisy/chore/test-hygiene-sweep\n\nchore: test hygiene sweep — shared LogCapture, static_assert, wire-table test, guard hardening (chore-584)",
          "timestamp": "2026-09-20T20:25:08+08:00",
          "tree_id": "06eb85faf160e5faed09fee676e77bd65d583e70",
          "url": "https://github.com/LoveDaisy/Lumice/commit/a09b72909d5b03ab532b9f908e01ac4ef233c4ac"
        },
        "date": 1789907758532,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 441690.2,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 510290.9,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "distinct": true,
          "id": "81e8d18a17511e66af4f6e17ce5e75a7c7996daf",
          "message": "Merge pull request #392 from LoveDaisy/chore/test-hygiene-sweep\n\nchore: test hygiene sweep — shared LogCapture, static_assert, wire-table test, guard hardening (chore-584)",
          "timestamp": "2026-09-20T20:34:42+08:00",
          "tree_id": "06eb85faf160e5faed09fee676e77bd65d583e70",
          "url": "https://github.com/LoveDaisy/Lumice/commit/81e8d18a17511e66af4f6e17ce5e75a7c7996daf"
        },
        "date": 1789908524407,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 370448.9,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 356296.2,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 310950.1,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor                \\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "3442752646d7a6d1d1e7c71045a958be19025e4f",
          "message": "Merge pull request #393 from LoveDaisy/feat/look-at-horizon-series\n\nfeat(gui): Look At — Horizon series of four sun-relative level bearings (task-586)",
          "timestamp": "2026-09-20T21:18:38+08:00",
          "tree_id": "2ea46a896126f59b8ad9e871c17836f0b16a9f3b",
          "url": "https://github.com/LoveDaisy/Lumice/commit/3442752646d7a6d1d1e7c71045a958be19025e4f"
        },
        "date": 1789910914914,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 380763.1,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 510410.5,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 359838.6,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 310158.2,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor                \\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "a9eb84bee60644a8d4838c4051be47e0b5cda60a",
          "message": "Merge pull request #394 from LoveDaisy/feat/analyze-chain-table-capacity\n\nfeat(analyze): runtime chain-record capacity — --chain-capacity / C API chain_capacity (task-583)",
          "timestamp": "2026-09-20T21:46:35+08:00",
          "tree_id": "f4e0ca5a93ff255549dadfb9d9ff5d2fb530ffac",
          "url": "https://github.com/LoveDaisy/Lumice/commit/a9eb84bee60644a8d4838c4051be47e0b5cda60a"
        },
        "date": 1789912847258,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 331960.8,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 509353.6,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 331506.9,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 9V74 80-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 311437.6,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor                \\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "15a0baf562499726ca8be28b5ac5780a16af813f",
          "message": "Merge pull request #395 from LoveDaisy/feat/axis-preset-type-override\n\nfeat(gui): preset library — zenith type is editable within each preset's accepted set (task-587)",
          "timestamp": "2026-09-20T22:53:50+08:00",
          "tree_id": "49dabf2ddaaebcc7d0fc14e6aa16353b02010817",
          "url": "https://github.com/LoveDaisy/Lumice/commit/15a0baf562499726ca8be28b5ac5780a16af813f"
        },
        "date": 1789916684257,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 386793.2,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 511241.2,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 361789.7,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 369051.9,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 9V74 80-Core Processor                \\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "1bd37a62468be8c031b66851d29f8687526ad678",
          "message": "Merge pull request #396 from LoveDaisy/feat/config-summary-window\n\nfeat(gui): read-only Summary window — one page of the current configuration for sharing (task-588)",
          "timestamp": "2026-09-21T11:06:18+08:00",
          "tree_id": "a62e92733c5372ed1d234f4bdc7e07e66441a886",
          "url": "https://github.com/LoveDaisy/Lumice/commit/1bd37a62468be8c031b66851d29f8687526ad678"
        },
        "date": 1789960733179,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 406266.8,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 510607.5,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 361665.5,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 311774.6,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor                \\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "f70cdd2b7bddb782f36f438d7742e112b90ce3fc",
          "message": "Merge pull request #397 from LoveDaisy/chore/release-4.6.1\n\nchore(release): cut 4.6.1 — changelog backfill for #368–#396",
          "timestamp": "2026-09-21T12:10:01+08:00",
          "tree_id": "7a67bfaac331a8f7aecdd57d6f9a15673d4db69b",
          "url": "https://github.com/LoveDaisy/Lumice/commit/f70cdd2b7bddb782f36f438d7742e112b90ce3fc"
        },
        "date": 1789964420575,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 334093,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 509955.2,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 464242.7,
            "unit": "rays/sec",
            "extra": "CPU: INTEL(R) XEON(R) PLATINUM 8573C\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 309964.4,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor                \\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "64706c37cd5ad912eb76cde70881bb84e9efd681",
          "message": "Merge pull request #398 from LoveDaisy/feat/ui-scale\n\nfeat(gui): ui_scale — DPI-aware layout and font, plus a user UI-scale preference",
          "timestamp": "2026-09-22T17:25:47+08:00",
          "tree_id": "282afffeb45deeca9dcf5eb0ea192084f3466648",
          "url": "https://github.com/LoveDaisy/Lumice/commit/64706c37cd5ad912eb76cde70881bb84e9efd681"
        },
        "date": 1790069877955,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 367617.1,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 509568.1,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 359837,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 482229.8,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 9V45 96-Core Processor                \\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "4d4b78fc4e2fdbd6c38693489ab74498e4d39df0",
          "message": "Merge pull request #399 from LoveDaisy/feat/gui-desktop-conventions-part1\n\nfeat(gui): desktop conventions — deletable last filter row, list search, copyable text, column-major Tab, window sizing policy",
          "timestamp": "2026-09-22T19:41:21+08:00",
          "tree_id": "adccab168ff45ce7559ae9dfb6e96e62e09b4a78",
          "url": "https://github.com/LoveDaisy/Lumice/commit/4d4b78fc4e2fdbd6c38693489ab74498e4d39df0"
        },
        "date": 1790078001520,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 363242.6,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 509039.5,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 329867.3,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 9V74 80-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 368686.1,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 9V74 80-Core Processor                \\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "d69f9167201076266c84c99adb68dcf68ac437b1",
          "message": "Merge pull request #400 from LoveDaisy/feat/analysis-exclusion-view\n\nfeat(gui): the analysis list keeps excluded raypaths as greyed rows, with Include again",
          "timestamp": "2026-09-22T20:10:32+08:00",
          "tree_id": "f5c55b74bbe2f5ac5434e9ac3850cd90089af128",
          "url": "https://github.com/LoveDaisy/Lumice/commit/d69f9167201076266c84c99adb68dcf68ac437b1"
        },
        "date": 1790079809140,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 333751.3,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 509086.1,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 365803.5,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 290632.5,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 9V74 80-Core Processor                \\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "c49ef3f32f9385ae0700b887e5f73faaedd7bea4",
          "message": "Merge pull request #401 from LoveDaisy/feat/gui-desktop-conventions\n\nrefactor(gui): the edit modal's two shapes — Compact and Expanded",
          "timestamp": "2026-09-22T22:24:18+08:00",
          "tree_id": "f1e06a712c5d8c584284204571457aa629c4d097",
          "url": "https://github.com/LoveDaisy/Lumice/commit/c49ef3f32f9385ae0700b887e5f73faaedd7bea4"
        },
        "date": 1790088023658,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 325176.3,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 510668.1,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 428144.4,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 9V74 80-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 314698.1,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor                \\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "c3d3aa1d21c41277d8cf6bc76016d7067cc2448b",
          "message": "Merge pull request #402 from LoveDaisy/feat/summary-reference-version-independence\n\ntest(gui): pin the Summary page's version under gui_test so a release stops reddening its references",
          "timestamp": "2026-09-23T00:07:08+08:00",
          "tree_id": "e22276ed331178ee1e226e97b579203a031ed8ba",
          "url": "https://github.com/LoveDaisy/Lumice/commit/c3d3aa1d21c41277d8cf6bc76016d7067cc2448b"
        },
        "date": 1790093873401,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 410531.7,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 511113.8,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 430470.6,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 9V74 80-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 315056.6,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor                \\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "b6ed96be5d6acdd7acc63a500e5c045647525ba5",
          "message": "Merge pull request #403 from LoveDaisy/feat/edit-modal-height-and-expanded-polish\n\nfeat(gui): Edit Entry — a draggable height, aligned Expanded headers, standard headings",
          "timestamp": "2026-09-23T09:50:00+08:00",
          "tree_id": "4bd7771eb2b058baf5a35532f33a8b6313a2718b",
          "url": "https://github.com/LoveDaisy/Lumice/commit/b6ed96be5d6acdd7acc63a500e5c045647525ba5"
        },
        "date": 1790128911741,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 393893.7,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 509172.3,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 400757.1,
            "unit": "rays/sec",
            "extra": "CPU: INTEL(R) XEON(R) PLATINUM 8573C\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 493166.8,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 9V45 96-Core Processor                \\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "2056f69990d8a0df13c199c795260b717c144f29",
          "message": "Merge pull request #404 from LoveDaisy/feat/crystal-projected-area-weighting\n\nfix(core): weight crystal entry by projected area on every backend",
          "timestamp": "2026-09-24T19:58:42+08:00",
          "tree_id": "b0cd16714314f8b35d9687520070dca01b0a5efd",
          "url": "https://github.com/LoveDaisy/Lumice/commit/2056f69990d8a0df13c199c795260b717c144f29"
        },
        "date": 1790251830268,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 332994.5,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 509150.3,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 358343,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 310892.7,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor                \\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "108fdd825206a218a0262cfb6533db39965fb3e0",
          "message": "Merge pull request #405 from LoveDaisy/feat/card-insert-and-reorder\n\nfeat(gui): duplicate lands below the source card; drag to reorder cards",
          "timestamp": "2026-09-25T12:52:34+08:00",
          "tree_id": "e30f40ec89cb02353708deba953b4ee18ee297fe",
          "url": "https://github.com/LoveDaisy/Lumice/commit/108fdd825206a218a0262cfb6533db39965fb3e0"
        },
        "date": 1790312908176,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 356334.1,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 509727.9,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 419440.2,
            "unit": "rays/sec",
            "extra": "CPU: Intel(R) Xeon(R) Platinum 8370C CPU @ 2.80GHz\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 310225.1,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor                \\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "65514f1a79aafa4a7d5bf1c468123ac24550aa41",
          "message": "Merge pull request #406 from LoveDaisy/feat/sim-continue\n\nfeat: Continue adds rays to a finished render (LUMICE_ContinueRender)",
          "timestamp": "2026-09-25T13:25:12+08:00",
          "tree_id": "86a13a44187986c4d27a4fea1b98d72c471e573d",
          "url": "https://github.com/LoveDaisy/Lumice/commit/65514f1a79aafa4a7d5bf1c468123ac24550aa41"
        },
        "date": 1790314916562,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 323835.1,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 509457.2,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 329117.9,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 9V74 80-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 310242.9,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor                \\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "69b1b656449030f1e93e69f7bd842037f16a8bdc",
          "message": "Merge pull request #407 from LoveDaisy/feat/globe-backside-fog-fade\n\nfeat: globe back-side fade (the far side shows through, fading like fog)",
          "timestamp": "2026-09-25T14:30:07+08:00",
          "tree_id": "e4f340177152a07e767413c1203db933158f3ac0",
          "url": "https://github.com/LoveDaisy/Lumice/commit/69b1b656449030f1e93e69f7bd842037f16a8bdc"
        },
        "date": 1790318811672,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 287119.4,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 508668.4,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 421751.1,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 9V74 80-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 305431.8,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor                \\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "df90ad63b51304ed6867f9285e263e304d346c39",
          "message": "Merge pull request #408 from LoveDaisy/fix/analysis-manual-stop-flake\n\nfix(test): wait for a cone-sized sample before the manual-stop analysis assertion",
          "timestamp": "2026-09-25T15:19:07+08:00",
          "tree_id": "72655d66e9061682075555c399d83140dc3216f5",
          "url": "https://github.com/LoveDaisy/Lumice/commit/df90ad63b51304ed6867f9285e263e304d346c39"
        },
        "date": 1790321715002,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 310354.8,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 509269.3,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 364200.8,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 304181.3,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor                \\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "5c29ec370e9e6fcae68806e5d82c40e8df5359a2",
          "message": "Merge pull request #409 from LoveDaisy/fix/continue-render-plane-total-flake\n\nfix(test): calibrate ContinueRender's plane-total band to per-batch wavelength noise",
          "timestamp": "2026-09-25T15:58:37+08:00",
          "tree_id": "616829b9a9cb568528885cd75ef28c022146191b",
          "url": "https://github.com/LoveDaisy/Lumice/commit/5c29ec370e9e6fcae68806e5d82c40e8df5359a2"
        },
        "date": 1790324473688,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "Ubuntu ARM64",
            "value": 508473.8,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 364088.3,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 309461.3,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor                \\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "a2d088f53ed6ac75eeac825ca6f5ec003084e6cd",
          "message": "Merge pull request #410 from LoveDaisy/feat/channel-math-display-mode\n\nfeat: Channel B−R display mode (is this spot bluer or redder?)",
          "timestamp": "2026-09-25T16:20:42+08:00",
          "tree_id": "9492740c7c72869d21a8148f8567070eaca19d7b",
          "url": "https://github.com/LoveDaisy/Lumice/commit/a2d088f53ed6ac75eeac825ca6f5ec003084e6cd"
        },
        "date": 1790325337778,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 422107.6,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 508005.4,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 361680.6,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 360623.3,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 9V74 80-Core Processor                \\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "49fe5e10fab9bfff09a5c101fbf651f8f283af5b",
          "message": "Merge pull request #411 from LoveDaisy/fix/globe-back-fade-fma-exact-eq\n\nfix(test): compare the globe back-fade weight within an absolute tolerance",
          "timestamp": "2026-09-25T16:38:58+08:00",
          "tree_id": "1ad240095c7e9a80ab3a0f9f3c9ec1c87f9c4465",
          "url": "https://github.com/LoveDaisy/Lumice/commit/49fe5e10fab9bfff09a5c101fbf651f8f283af5b"
        },
        "date": 1790326326838,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 291277.9,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 508003.5,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 423504.5,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 9V74 80-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 366962,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 9V74 80-Core Processor                \\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "69d8136c8f926571410a99548a5e7374bf7f0ab2",
          "message": "Merge pull request #412 from LoveDaisy/perf/cpu-worker-batch-sync-cliff\n\nperf(server): decouple the CPU queue handoff from the 128-ray physics batch",
          "timestamp": "2026-09-25T20:57:22+08:00",
          "tree_id": "09c4693994679e1f4660fbb9b8892f3e4bb629e4",
          "url": "https://github.com/LoveDaisy/Lumice/commit/69d8136c8f926571410a99548a5e7374bf7f0ab2"
        },
        "date": 1790341820114,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 291651.3,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 511686.6,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 371133.4,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 369814.2,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 9V74 80-Core Processor                \\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "3c9730817a17b3c99d8469c7cfc7f9a7ceee6e38",
          "message": "Merge pull request #413 from LoveDaisy/docs/raypath-analysis-overview\n\ndocs(raypath-analysis): feature overview and roadmap",
          "timestamp": "2026-09-25T22:51:44+08:00",
          "tree_id": "c79b7399ed96b48eaf32a502f0cc41512296376e",
          "url": "https://github.com/LoveDaisy/Lumice/commit/3c9730817a17b3c99d8469c7cfc7f9a7ceee6e38"
        },
        "date": 1790348565325,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 382147.1,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 512714.7,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 403901.2,
            "unit": "rays/sec",
            "extra": "CPU: INTEL(R) XEON(R) PLATINUM 8573C\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 313218.3,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor                \\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "6a148bbcba23d5e330a528600062b88fbe62aae5",
          "message": "Merge pull request #414 from LoveDaisy/perf/cpu-per-ray-wavelength\n\nperf(core): stratify the CPU illuminant wavelength across physics batches",
          "timestamp": "2026-09-25T23:31:46+08:00",
          "tree_id": "b0ecff07f1973fc2a59863cf12726b470566f04c",
          "url": "https://github.com/LoveDaisy/Lumice/commit/6a148bbcba23d5e330a528600062b88fbe62aae5"
        },
        "date": 1790351094595,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 312841.8,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 511112.6,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 372809.2,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 352570.9,
            "unit": "rays/sec",
            "extra": "CPU: Intel(R) Xeon(R) Platinum 8370C CPU @ 2.80GHz\\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "48679991416540658f3039cdbde51520d6375e79",
          "message": "Merge pull request #415 from LoveDaisy/feat/gui-front-effective-value\n\nfix(gui): read the effective front clip under lenses it does not apply to",
          "timestamp": "2026-09-26T01:08:03+08:00",
          "tree_id": "9f96f5fa48eafea0128b3d270b751019093977c1",
          "url": "https://github.com/LoveDaisy/Lumice/commit/48679991416540658f3039cdbde51520d6375e79"
        },
        "date": 1790356911929,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 327664.5,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 512049,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 432725.4,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 9V74 80-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 313697.9,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor                \\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "890b32d27ca52132e0f8567199aa38f079614db9",
          "message": "Merge pull request #416 from LoveDaisy/feat/globe-back-fade-expfog-and-grid\n\nfeat: globe back-side fade becomes exponential fog, and far-side lines fade with it",
          "timestamp": "2026-09-26T01:37:37+08:00",
          "tree_id": "f71d2148439000301d8f261d490aa085c8114a70",
          "url": "https://github.com/LoveDaisy/Lumice/commit/890b32d27ca52132e0f8567199aa38f079614db9"
        },
        "date": 1790358574353,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 312737.4,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 513395.1,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 369066.1,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 288074.4,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 9V74 80-Core Processor                \\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "b5a1a477c67196240aa5db0dd933b77760fa2182",
          "message": "Merge pull request #417 from LoveDaisy/feat/toolbar-button-sizing-continue-color\n\nfeat(gui): group the top bar and give Continue its own colour",
          "timestamp": "2026-09-26T02:20:15+08:00",
          "tree_id": "341993077498e5caf3df7edb8433227b484ffc25",
          "url": "https://github.com/LoveDaisy/Lumice/commit/b5a1a477c67196240aa5db0dd933b77760fa2182"
        },
        "date": 1790361078255,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 276368.9,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 512025.4,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 364141.5,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 312063.7,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor                \\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "c6d32d110ca90680080935d49bda6d00274d6ac7",
          "message": "Merge pull request #418 from LoveDaisy/feat/axis-preset-highlight-summary\n\nfeat(gui): highlight the active axis preset and summarize the crystal under the edit-modal preview",
          "timestamp": "2026-09-26T03:24:31+08:00",
          "tree_id": "0c4591c98e7a93246c6b7ac5869eb931880bb594",
          "url": "https://github.com/LoveDaisy/Lumice/commit/c6d32d110ca90680080935d49bda6d00274d6ac7"
        },
        "date": 1790365016160,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 327387.5,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 512643.5,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 368733.8,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 312859.7,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor                \\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "3493b66dd3bb005b0ece61229caadab5c3c3fd09",
          "message": "Merge pull request #419 from LoveDaisy/feat/display-mode-viewport-control\n\nfeat(gui): display mode becomes a segmented control in the preview's corner",
          "timestamp": "2026-09-26T03:55:16+08:00",
          "tree_id": "97ddaa25b9147c1d43eec4ed3cd226d46d8195bb",
          "url": "https://github.com/LoveDaisy/Lumice/commit/3493b66dd3bb005b0ece61229caadab5c3c3fd09"
        },
        "date": 1790366933216,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 363052.3,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 511419.9,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 368560.9,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 328497.4,
            "unit": "rays/sec",
            "extra": "CPU: INTEL(R) XEON(R) PLATINUM 8573C\\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "208ad64e99c9a72de9042b4c4b28db4829cca387",
          "message": "Merge pull request #420 from LoveDaisy/feat/toolbar-revert-slot-gap\n\nfeat(gui): move the top bar's hidden Revert slot to the trailing end",
          "timestamp": "2026-09-26T05:03:32+08:00",
          "tree_id": "cad7a53a552485d156dcaa7b57ac91e01b2e2f7d",
          "url": "https://github.com/LoveDaisy/Lumice/commit/208ad64e99c9a72de9042b4c4b28db4829cca387"
        },
        "date": 1790370990657,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 353600.2,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 512726.6,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 336001.4,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 9V74 80-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 315107.5,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor                \\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "e12c699d7262aa07039ba1d5aa5715e65f78bf5c",
          "message": "Merge pull request #421 from LoveDaisy/feat/angular-dist-picker\n\nfeat(gui): pick an angular-distance ring on the preview",
          "timestamp": "2026-09-26T05:43:54+08:00",
          "tree_id": "2bc14239d813e7ec2ceecf67e4adb4be66e5ba9f",
          "url": "https://github.com/LoveDaisy/Lumice/commit/e12c699d7262aa07039ba1d5aa5715e65f78bf5c"
        },
        "date": 1790373458422,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 335285.7,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 512442.9,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 400448,
            "unit": "rays/sec",
            "extra": "CPU: INTEL(R) XEON(R) PLATINUM 8573C\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 313735.2,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor                \\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "b0b090813c2e811b472eb839916a357272554a9c",
          "message": "Merge pull request #422 from LoveDaisy/feat/screenshot-export-options\n\nfeat(gui): screenshot export options popup, subtract-only per family",
          "timestamp": "2026-09-26T06:04:43+08:00",
          "tree_id": "387bf188ef1a8b8098005bded4fd012477ff7205",
          "url": "https://github.com/LoveDaisy/Lumice/commit/b0b090813c2e811b472eb839916a357272554a9c"
        },
        "date": 1790374706772,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 467555.4,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 512696,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 368744.5,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 288007.5,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 9V74 80-Core Processor                \\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "169a01b75ac4932ed78b13aefb385e4e6087b765",
          "message": "Merge pull request #423 from LoveDaisy/fix/adaptive-alloc-c1-bias-probe\n\ntest(e2e): confirm an adaptive-vs-proportional mean red on 60 more sessions per arm",
          "timestamp": "2026-09-26T08:31:46+08:00",
          "tree_id": "42c5899e6e3d28b93caca02621779566a7678c6a",
          "url": "https://github.com/LoveDaisy/Lumice/commit/169a01b75ac4932ed78b13aefb385e4e6087b765"
        },
        "date": 1790383350439,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 344095.5,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 512824.7,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 369470.9,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 469133,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 9V45 96-Core Processor                \\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "f185f3da689e6f99f72572f6b6198659524a0cfa",
          "message": "Merge pull request #424 from LoveDaisy/fix/globe-far-side-own-visibility\n\nfix: clip each side of the globe by its own direction's visibility",
          "timestamp": "2026-09-26T11:01:44+08:00",
          "tree_id": "1d3bdfa97e8e41f70adb6f2c18d2203221bfa8e4",
          "url": "https://github.com/LoveDaisy/Lumice/commit/f185f3da689e6f99f72572f6b6198659524a0cfa"
        },
        "date": 1790392423131,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 319250.7,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 514072.3,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 595675.4,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 9V45 96-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 501512.9,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 9V45 96-Core Processor                \\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "e4adc5bd4c991f32ec7d292e06eaeb6773249573",
          "message": "Merge pull request #425 from LoveDaisy/chore/release-4.7.0\n\nchore(release): cut 4.7.0",
          "timestamp": "2026-09-26T11:42:17+08:00",
          "tree_id": "31eb8c69a51d64a2fd2f696314ecf1251915e92e",
          "url": "https://github.com/LoveDaisy/Lumice/commit/e4adc5bd4c991f32ec7d292e06eaeb6773249573"
        },
        "date": 1790394800092,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 367147,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 512599.9,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 366521.4,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 377956.5,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 9V74 80-Core Processor                \\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "7b107c5623d51c5578890e40cfa4b2a1e2545b02",
          "message": "Merge pull request #426 from LoveDaisy/chore/raypath-analysis-lumice-integral-plan\n\ndocs(raypath-analysis): expand §5.1 into the post-selection three-feature plan",
          "timestamp": "2026-09-27T12:04:30+08:00",
          "tree_id": "22a4d5cac931dca24cc45e2267cdd77444575d99",
          "url": "https://github.com/LoveDaisy/Lumice/commit/7b107c5623d51c5578890e40cfa4b2a1e2545b02"
        },
        "date": 1790482578432,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 281784.1,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 511349.2,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 371006.2,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 309880,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor                \\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "bbe3d1883d9873540b5709ec8aa616be79897b36",
          "message": "Merge pull request #427 from LoveDaisy/chore/raypath-analysis-compute-timing\n\ndocs(raypath-analysis): compute location moves per module maturity",
          "timestamp": "2026-09-27T13:51:45+08:00",
          "tree_id": "f291db49905b10460d4a5110faf832aca1be33b0",
          "url": "https://github.com/LoveDaisy/Lumice/commit/bbe3d1883d9873540b5709ec8aa616be79897b36"
        },
        "date": 1790489012956,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 339511.8,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 513395.7,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 391470.3,
            "unit": "rays/sec",
            "extra": "CPU: INTEL(R) XEON(R) PLATINUM 8573C\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 504541.7,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 9V45 96-Core Processor                \\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "cd82b68f1c2ab9e12e9b4f3b56f752998e9e3f34",
          "message": "Merge pull request #428 from LoveDaisy/chore/analyze-product-form-docs\n\ndocs(raypath-analysis): Analyze as the second product core (§5.1.8)",
          "timestamp": "2026-09-27T14:38:00+08:00",
          "tree_id": "79d92ce67a0cff0ef0251d949186ab2eade8ef5c",
          "url": "https://github.com/LoveDaisy/Lumice/commit/cd82b68f1c2ab9e12e9b4f3b56f752998e9e3f34"
        },
        "date": 1790491709478,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 417663,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 509429.5,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 572107,
            "unit": "rays/sec",
            "extra": "CPU: Intel(R) Xeon(R) 6973P-C\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 291900.2,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 9V74 80-Core Processor                \\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "89c8c1cc6f2798f0c7ef0ea8a8c5f89d7bda19ec",
          "message": "Merge pull request #429 from LoveDaisy/task/raypath-reduce-period-from-geometry\n\nfix(core): reduce raypaths only under the symmetry the crystal shape admits",
          "timestamp": "2026-09-27T17:35:49+08:00",
          "tree_id": "e56285d05d6ce1cec96e104506cb4b053d292439",
          "url": "https://github.com/LoveDaisy/Lumice/commit/89c8c1cc6f2798f0c7ef0ea8a8c5f89d7bda19ec"
        },
        "date": 1790502699769,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 411717.1,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 513246.6,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 337616.2,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 9V74 80-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 312802.8,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor                \\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "7b889f79db8568517726cf7bd294d13c757a8974",
          "message": "Merge pull request #430 from LoveDaisy/task/raypath-pb-ensemble-applicability\n\nfix(core): apply P/B raypath symmetry only where the orientation ensemble admits it",
          "timestamp": "2026-09-27T20:03:03+08:00",
          "tree_id": "66c883c3d1bbf83bbd049a58b95f6e6c004751b1",
          "url": "https://github.com/LoveDaisy/Lumice/commit/7b889f79db8568517726cf7bd294d13c757a8974"
        },
        "date": 1790511576867,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 419731.5,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 513206.4,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 367971.5,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 311318.6,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor                \\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "ac0fbdf294cc91312b0c421c64b99e05db8828fb",
          "message": "Merge pull request #431 from LoveDaisy/task/gpu-reduce-follows-cpu-symmetry\n\nfix(core): device filter reduction follows the crystal's shape and ensemble symmetry",
          "timestamp": "2026-09-27T21:42:14+08:00",
          "tree_id": "900e6e02f3b3e8425e7b98cde0722045fca77d1c",
          "url": "https://github.com/LoveDaisy/Lumice/commit/ac0fbdf294cc91312b0c421c64b99e05db8828fb"
        },
        "date": 1790517269815,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 313106.8,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 514804.8,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 432585.9,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 9V74 80-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 365445,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 9V74 80-Core Processor                \\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "b845c83f0138957e21f6ba7e7bcf5c1f42a0dfd1",
          "message": "Merge pull request #432 from LoveDaisy/chore/analyze-workspace-layout-docs\n\ndocs: finalize the Analyze workspace layout (raypath-analysis §5.1.8)",
          "timestamp": "2026-09-28T00:31:26+08:00",
          "tree_id": "26b84bc5a84d9c0089605c7b002cce526807c507",
          "url": "https://github.com/LoveDaisy/Lumice/commit/b845c83f0138957e21f6ba7e7bcf5c1f42a0dfd1"
        },
        "date": 1790527386024,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 348670.1,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 512774.3,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 366907.7,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 311754.4,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor                \\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "25b83e949546f15f97e44007554cd6d5a4ddd1e3",
          "message": "Merge pull request #433 from LoveDaisy/chore/repo-roles-and-shared-lib-docs\n\ndocs: repository roles for Lumice / Lumice Integral, and the shared library's first consumer",
          "timestamp": "2026-09-28T01:28:05+08:00",
          "tree_id": "035c8e3864a2fee8909f0af50fb76aaff2835c1a",
          "url": "https://github.com/LoveDaisy/Lumice/commit/25b83e949546f15f97e44007554cd6d5a4ddd1e3"
        },
        "date": 1790530886818,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 324698.8,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 512144.4,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 368754,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 349233.6,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 9V74 80-Core Processor                \\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "26ab6a18e6455817e83e95ad76594539cffc24d6",
          "message": "Merge pull request #434 from LoveDaisy/chore/ray-allocation-settings-combo\n\ngui: sim.ray_allocation Settings editor becomes an adaptive/proportional combo",
          "timestamp": "2026-09-28T01:59:27+08:00",
          "tree_id": "f911df15b6d21e6900447e072754865902bb15a7",
          "url": "https://github.com/LoveDaisy/Lumice/commit/26ab6a18e6455817e83e95ad76594539cffc24d6"
        },
        "date": 1790532800201,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 379029.5,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 512749.1,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 369736.8,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 312707.6,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor                \\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "a7dc7af83f8475e1465a1e0705a2c6437b0e04dc",
          "message": "Merge pull request #435 from LoveDaisy/task/adopt-relative-floor-ray-allocation\n\ncore: relative floor in adaptive ray allocation stops starving a filtered arc",
          "timestamp": "2026-09-28T09:32:31+08:00",
          "tree_id": "e682da94016f1a73dff66ec1f11396547391e218",
          "url": "https://github.com/LoveDaisy/Lumice/commit/a7dc7af83f8475e1465a1e0705a2c6437b0e04dc"
        },
        "date": 1790559875518,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 306207.3,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 512177.2,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 372161.5,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 503497.8,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 9V45 96-Core Processor                \\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "1b0a0361b3465b124b87fc0680bff113b21524d4",
          "message": "Merge pull request #436 from LoveDaisy/task/restore-pb-label-equivalence\n\ncore: a filter's P/B/D is a label equivalence again; the analysis list keeps physical symmetry",
          "timestamp": "2026-09-28T10:02:45+08:00",
          "tree_id": "74c642836565436b225b79dbea5e311790408de7",
          "url": "https://github.com/LoveDaisy/Lumice/commit/1b0a0361b3465b124b87fc0680bff113b21524d4"
        },
        "date": 1790561931326,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 305757.8,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 512636.8,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 473043.9,
            "unit": "rays/sec",
            "extra": "CPU: INTEL(R) XEON(R) PLATINUM 8573C\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 312143.5,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor                \\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "1455e6aeb0201ab8b80fe2ef34ff16cf1ae2ed30",
          "message": "Merge pull request #437 from LoveDaisy/chore/symmetry-two-meanings-cross-repo-docs\n\ndocs: state which symmetry meaning each repo's computation uses",
          "timestamp": "2026-09-28T10:49:54+08:00",
          "tree_id": "07236666f17979fcb194db2933d7e276e6f5c16e",
          "url": "https://github.com/LoveDaisy/Lumice/commit/1455e6aeb0201ab8b80fe2ef34ff16cf1ae2ed30"
        },
        "date": 1790564448480,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 317816.1,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 514132.8,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 369817.4,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 312502.1,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor                \\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "5d353c63c4688fb9be43a5042ef239a4c310a93b",
          "message": "Merge pull request #439 from LoveDaisy/feat/channel-br-gain-x2\n\nChannel B-R display mode: raise gain from 0.5 to 2",
          "timestamp": "2026-09-28T14:33:59+08:00",
          "tree_id": "0b0e5066e995207a2d2bb4b5603b597496d80793",
          "url": "https://github.com/LoveDaisy/Lumice/commit/5d353c63c4688fb9be43a5042ef239a4c310a93b"
        },
        "date": 1790577941453,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 349044.9,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 512995.5,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 474197.3,
            "unit": "rays/sec",
            "extra": "CPU: INTEL(R) XEON(R) PLATINUM 8573C\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 316621.7,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor                \\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "51bc059b3fa58ffeeef7ae600d704bf242a639de",
          "message": "Merge pull request #440 from LoveDaisy/feat/config-change-preview-transition\n\nGUI: a struct-hard edit no longer blanks the preview",
          "timestamp": "2026-09-28T15:21:22+08:00",
          "tree_id": "8725666a1d1fef58c78a1647e3213d2e90e95384",
          "url": "https://github.com/LoveDaisy/Lumice/commit/51bc059b3fa58ffeeef7ae600d704bf242a639de"
        },
        "date": 1790580902800,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 341192.8,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 511406.7,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 436103.1,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 9V74 80-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 348890.6,
            "unit": "rays/sec",
            "extra": "CPU: Intel(R) Xeon(R) Platinum 8370C CPU @ 2.80GHz\\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "e6d934376e48fc8fc854f3d4f4a866d325bcd3bd",
          "message": "Merge pull request #441 from LoveDaisy/scrum/lumice-shared-lib-foundation\n\nPublish-ready shared library foundation: liblumice_analytic, per-library export lists, packaging",
          "timestamp": "2026-09-28T17:16:31+08:00",
          "tree_id": "d9f2f2216737e9645bf134e5c5a1d6d6b8628eca",
          "url": "https://github.com/LoveDaisy/Lumice/commit/e6d934376e48fc8fc854f3d4f4a866d325bcd3bd"
        },
        "date": 1790588095581,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 358079.8,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 514796.4,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 418315.4,
            "unit": "rays/sec",
            "extra": "CPU: Intel(R) Xeon(R) Platinum 8370C CPU @ 2.80GHz\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 313726.5,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor                \\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "01b9dd59561fea636f6f0a888b375d2e42e95374",
          "message": "Merge pull request #442 from LoveDaisy/chore/release-4.7.1\n\nchore(release): cut 4.7.1",
          "timestamp": "2026-09-28T19:04:03+08:00",
          "tree_id": "67bb9062b1e0b8474469bb9b06c73e3f3b84ce30",
          "url": "https://github.com/LoveDaisy/Lumice/commit/01b9dd59561fea636f6f0a888b375d2e42e95374"
        },
        "date": 1790594366170,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 320009.4,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 512783.5,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 571100.6,
            "unit": "rays/sec",
            "extra": "CPU: Intel(R) Xeon(R) 6973P-C\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 314632.9,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor                \\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "591c97d4c35ccce4e8f3f6dd1cf340e93390e6e4",
          "message": "Merge pull request #443 from LoveDaisy/chore/workflow-token-least-privilege\n\nci: add top-level permissions: contents: read to workflows",
          "timestamp": "2026-09-28T20:18:35+08:00",
          "tree_id": "200ab539ed87c0bd2915e7f5e25f6ed5dac388e4",
          "url": "https://github.com/LoveDaisy/Lumice/commit/591c97d4c35ccce4e8f3f6dd1cf340e93390e6e4"
        },
        "date": 1790598756871,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 335412,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 512159.2,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 566879.8,
            "unit": "rays/sec",
            "extra": "CPU: Intel(R) Xeon(R) 6973P-C\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 312997.2,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor                \\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "73b0f697916c89df61e26e9fb12ee6bc0cbc6afb",
          "message": "Merge pull request #444 from LoveDaisy/scrum/analytic-module-a-v0\n\nliblumice_analytic module A v0: EvaluatePath, TraceFiber, DiscoverComponents + LI parity",
          "timestamp": "2026-09-29T09:40:58+08:00",
          "tree_id": "7dbc414388ddfcacdb2e91a6a1ddb3596a584cf4",
          "url": "https://github.com/LoveDaisy/Lumice/commit/73b0f697916c89df61e26e9fb12ee6bc0cbc6afb"
        },
        "date": 1790646738633,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 352689.9,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 514762.7,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 337638.9,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 9V74 80-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 312769.5,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor                \\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "e129d76c7ac6844f6499a631f39e7eb81f169450",
          "message": "Merge pull request #445 from LoveDaisy/scrum/raypath-subcommand\n\nLumice raypath: single-path analysis module, lumice.h v4.50 entry, D4 verification",
          "timestamp": "2026-09-29T18:08:24+08:00",
          "tree_id": "940b3fbc2d846b901770e2c322d5509ac5278264",
          "url": "https://github.com/LoveDaisy/Lumice/commit/e129d76c7ac6844f6499a631f39e7eb81f169450"
        },
        "date": 1790677419052,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 363806.5,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 511947.2,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 366358.8,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 343878.4,
            "unit": "rays/sec",
            "extra": "CPU: INTEL(R) XEON(R) PLATINUM 8573C\\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "9282fe084bf6c22f1620b39162daca6413f1cd29",
          "message": "Merge pull request #446 from LoveDaisy/scrum/analytic-wave2\n\nliblumice_analytic wave 2: per-pose diagnostics (API 5), band sum module B (API 6), 93 LI parity fixtures",
          "timestamp": "2026-09-30T08:45:42+08:00",
          "tree_id": "6d73ac11e978bac05aa5833e3d6725c7fa212092",
          "url": "https://github.com/LoveDaisy/Lumice/commit/9282fe084bf6c22f1620b39162daca6413f1cd29"
        },
        "date": 1790729847073,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 413820.2,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 509917.2,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 370645.8,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 312440.8,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor                \\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "c0f0e1fe7ec5d6c1f1ebc35c76c60ee01de9c46b",
          "message": "Merge pull request #452 from LoveDaisy/chore/analytic-docs-weight-kink-critical-lines\n\ndocs(analytic): three kinds of critical lines; LI 31c682e parity fixtures (94)",
          "timestamp": "2026-09-30T19:42:02+08:00",
          "tree_id": "352b8d146251654b54e578eea86b4e7382b26ce7",
          "url": "https://github.com/LoveDaisy/Lumice/commit/c0f0e1fe7ec5d6c1f1ebc35c76c60ee01de9c46b"
        },
        "date": 1790769318428,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 320247.2,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 513599.2,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 429520.9,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 9V74 80-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 313580,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor                \\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "5a6e0df8340efd9cf0b86e005e69a70cd1d7bf5c",
          "message": "Merge pull request #453 from LoveDaisy/scrum/capability-libs\n\nOrganize the C API and engine by capability",
          "timestamp": "2026-09-30T22:20:49+08:00",
          "tree_id": "e33e93629598cfbac336b950cd72445b26fb5b14",
          "url": "https://github.com/LoveDaisy/Lumice/commit/5a6e0df8340efd9cf0b86e005e69a70cd1d7bf5c"
        },
        "date": 1790779822521,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 328879.6,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 511989.6,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 417924,
            "unit": "rays/sec",
            "extra": "CPU: Intel(R) Xeon(R) Platinum 8370C CPU @ 2.80GHz\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 288862.8,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 9V74 80-Core Processor                \\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "6311b6bd9421e45d367cc351d08ae0cc257a3148",
          "message": "Merge pull request #455 from LoveDaisy/refactor/capi-scene-codec-split\n\nSplit c_api_scene.cpp into bridge, encoder and decoder",
          "timestamp": "2026-09-30T23:35:26+08:00",
          "tree_id": "e2bf504be9af427f91adf8d0e55d5f369cf2e35b",
          "url": "https://github.com/LoveDaisy/Lumice/commit/6311b6bd9421e45d367cc351d08ae0cc257a3148"
        },
        "date": 1790783635336,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 355626.4,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 511332.1,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 370355.7,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 312065.2,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor                \\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "1d6bb6f143908c461fabfc30bd29f4f94e31dbc0",
          "message": "Merge pull request #451 from LoveDaisy/scrum/ci-time-governance\n\nscrum ci-time-governance (WIP): build-time landing",
          "timestamp": "2026-10-01T06:48:43+08:00",
          "tree_id": "14a7af4467d0122e428a63d9dfec71858a8ec59a",
          "url": "https://github.com/LoveDaisy/Lumice/commit/1d6bb6f143908c461fabfc30bd29f4f94e31dbc0"
        },
        "date": 1790808681376,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 403297.8,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 513919.4,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 336233.5,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 9V74 80-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 307993.9,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor                \\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "7f16d97d16354d0b0e38424d0ee0c8cd2e53acf4",
          "message": "Merge pull request #462 from LoveDaisy/scrum/ci-time-governance\n\nci: continue time-governance calibration",
          "timestamp": "2026-10-01T17:12:50+08:00",
          "tree_id": "e0ea583d7775397055133bfe6213ee233562a204",
          "url": "https://github.com/LoveDaisy/Lumice/commit/7f16d97d16354d0b0e38424d0ee0c8cd2e53acf4"
        },
        "date": 1790846125503,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 411509.6,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 512618.3,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 364887.9,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 9V74 80-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 330234,
            "unit": "rays/sec",
            "extra": "CPU: INTEL(R) XEON(R) PLATINUM 8573C\\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "0abb8e9bdc05a0db2723e33dd75904da10092d52",
          "message": "Merge pull request #463 from LoveDaisy/scrum/ci-time-governance-acceptance\n\ndocs(ci): close time-governance acceptance",
          "timestamp": "2026-10-01T18:29:14+08:00",
          "tree_id": "ad1f485ce54ff636e4d44ade48dbe43ca58fc4ff",
          "url": "https://github.com/LoveDaisy/Lumice/commit/0abb8e9bdc05a0db2723e33dd75904da10092d52"
        },
        "date": 1790850722428,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 380247.7,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 513469.7,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 372118.4,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 312972.9,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor                \\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "4f133865f5df1f6ebbca26757196056d78e762bc",
          "message": "Merge pull request #464 from LoveDaisy/scrum/raypath-feature-diagnostics\n\nAdd target-free raypath feature diagnostics and scientific acceptance",
          "timestamp": "2026-10-02T13:12:28+08:00",
          "tree_id": "e5a02d2855e8ff1e8e73ae6d2a5f6ad04f028841",
          "url": "https://github.com/LoveDaisy/Lumice/commit/4f133865f5df1f6ebbca26757196056d78e762bc"
        },
        "date": 1790918101300,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 362861.2,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 512124.2,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 366283.4,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 368641.6,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 9V74 80-Core Processor                \\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "5fd26ee965155e74cedf143214fd9a1c3a9a0314",
          "message": "Merge pull request #465 from LoveDaisy/fix/ui-scale-window-geometry\n\nfix(gui): keep background aspect ratios consistent across UI scales",
          "timestamp": "2026-10-03T10:47:02+08:00",
          "tree_id": "2223b1e6369a418cca96f536688a1de685794a1c",
          "url": "https://github.com/LoveDaisy/Lumice/commit/5fd26ee965155e74cedf143214fd9a1c3a9a0314"
        },
        "date": 1790995795581,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 329737.6,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 510765.7,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 364740.3,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 506276.2,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 9V45 96-Core Processor                \\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "008837c9802a7b770cb84375e081eaa995c9a7ea",
          "message": "Merge pull request #466 from LoveDaisy/fix/channel-br-background-free\n\nfix(render): exclude sky background from B-R diagnostic",
          "timestamp": "2026-10-03T20:14:42+08:00",
          "tree_id": "c2814c53d58b59a0419d2d23b677c39bebde293c",
          "url": "https://github.com/LoveDaisy/Lumice/commit/008837c9802a7b770cb84375e081eaa995c9a7ea"
        },
        "date": 1791029849585,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 347342.4,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 513674.4,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 370069.3,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 312958.6,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor                \\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "b6107e0c78b94e37fde81d77d8b596e16a2d3452",
          "message": "Merge pull request #467 from LoveDaisy/fix/pytest-performance-pool-isolation\n\nfix(testing): isolate local slow pytest performance pool",
          "timestamp": "2026-10-03T22:58:56+08:00",
          "tree_id": "2bb7504b7cfd61fc451738fe5ed1567077d8d80b",
          "url": "https://github.com/LoveDaisy/Lumice/commit/b6107e0c78b94e37fde81d77d8b596e16a2d3452"
        },
        "date": 1791039674613,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 429872.4,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 512711.8,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 333189.8,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 9V74 80-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 507811.7,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 9V45 96-Core Processor                \\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "928b8accd21aa950eeb249c13ad4bd8b4028b1b9",
          "message": "Merge pull request #468 from LoveDaisy/scrum/raypath-general-diagnostics-v2\n\nfeat(raypath): phase-1 raypath feature diagnostics (scrum-649)",
          "timestamp": "2026-10-05T12:17:39+08:00",
          "tree_id": "b9531b058ecf0cc3746ac4253f6cbb3fd2ee8930",
          "url": "https://github.com/LoveDaisy/Lumice/commit/928b8accd21aa950eeb249c13ad4bd8b4028b1b9"
        },
        "date": 1791174019041,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 305978.8,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 509050.9,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 581681,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 9V45 96-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 356793,
            "unit": "rays/sec",
            "extra": "CPU: Intel(R) Xeon(R) Platinum 8370C CPU @ 2.80GHz\\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "420585b02a80634fd511794a5f91d5a046d31cbf",
          "message": "Merge pull request #469 from LoveDaisy/refactor/raypath-module-design-cleanup\n\nrefactor(raypath): module design cleanup — role-based naming, detail/ demote, JSON single-owner",
          "timestamp": "2026-10-06T00:39:09+08:00",
          "tree_id": "752b69ce57a148874846e24ba5b179f26440e1b0",
          "url": "https://github.com/LoveDaisy/Lumice/commit/420585b02a80634fd511794a5f91d5a046d31cbf"
        },
        "date": 1791218526195,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 283910.3,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 511413.7,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 334025.9,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 9V74 80-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 313601.9,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor                \\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "6abcd36409158fd01a12b110cb2c89ba688dc8a1",
          "message": "Merge pull request #470 from LoveDaisy/fix/sky-direction-convention-mismatch\n\nfix(sky-direction): resolve convention-mismatch investigation — call sites proven correct, defense lines landed",
          "timestamp": "2026-10-06T05:26:14+08:00",
          "tree_id": "bc8d78bd7c6d3609d7eccaeeaf39dcb4e38e96f7",
          "url": "https://github.com/LoveDaisy/Lumice/commit/6abcd36409158fd01a12b110cb2c89ba688dc8a1"
        },
        "date": 1791235729619,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 382194.6,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 510571,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 576493.8,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 9V45 96-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 368658.8,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 9V74 80-Core Processor                \\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "236483fc270669e06b5b472f997c6e03974f82f5",
          "message": "Merge pull request #471 from LoveDaisy/docs/raypath-case-corpus\n\ndocs(cases): add raypath analysis scenario-case corpus index",
          "timestamp": "2026-10-06T11:07:25+08:00",
          "tree_id": "05675d1c473a536b58ff6c00a95dbda93a7db23c",
          "url": "https://github.com/LoveDaisy/Lumice/commit/236483fc270669e06b5b472f997c6e03974f82f5"
        },
        "date": 1791256185939,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 427868.2,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 510129,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 369590,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 510298.6,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 9V45 96-Core Processor                \\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "0c45b96ea97e477e6973097a68acbf4e6e7e3825",
          "message": "Merge pull request #472 from LoveDaisy/fix/outward-child-selfhit-leak\n\nfix(core): classify outward children at birth in CPU/Metal slab traversal",
          "timestamp": "2026-10-06T15:02:49+08:00",
          "tree_id": "5a3cb903bdbd5960d3ed2fddfb7201cfe72a62d2",
          "url": "https://github.com/LoveDaisy/Lumice/commit/0c45b96ea97e477e6973097a68acbf4e6e7e3825"
        },
        "date": 1791270317976,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 439785.5,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 504958,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 369777.5,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 392026.7,
            "unit": "rays/sec",
            "extra": "CPU: INTEL(R) XEON(R) PLATINUM 8573C\\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "148248ccbf3b62682cd8191b787af84268d788d7",
          "message": "Merge pull request #473 from LoveDaisy/docs/corpus-c12-tint-caliber\n\ndocs(corpus): C12 tint multi-caliber + C06 LI closed-form anchor",
          "timestamp": "2026-10-07T17:57:54+08:00",
          "tree_id": "87b8eff64500a3715263acfc8706ee01d3846e00",
          "url": "https://github.com/LoveDaisy/Lumice/commit/148248ccbf3b62682cd8191b787af84268d788d7"
        },
        "date": 1791367464333,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 405968.9,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 505989.1,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 369150.3,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 308295.4,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor                \\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "215a850397ac9dd42e2eb7b30ebcb657294c11f2",
          "message": "Merge pull request #474 from LoveDaisy/chore/release-4.7.2\n\nchore(release): cut 4.7.2",
          "timestamp": "2026-10-07T18:13:09+08:00",
          "tree_id": "b9ed9fbd7711749ea472f0e75e9c4ad8a572d253",
          "url": "https://github.com/LoveDaisy/Lumice/commit/215a850397ac9dd42e2eb7b30ebcb657294c11f2"
        },
        "date": 1791368213590,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 321148.3,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 506116.1,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 369357.2,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 311091.5,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor                \\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "e6ca36f9b55e42a684caa58bff4213abf37f4cd0",
          "message": "Merge pull request #475 from LoveDaisy/feat/schema3-measure-layer\n\nfeat(raypath): schema3 measure layer (task-661)",
          "timestamp": "2026-10-08T01:15:42+08:00",
          "tree_id": "eecb54e33af022f4804aa681db1b25ae0dc6cdf4",
          "url": "https://github.com/LoveDaisy/Lumice/commit/e6ca36f9b55e42a684caa58bff4213abf37f4cd0"
        },
        "date": 1791393515242,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 281847.9,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 506725.5,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 369723.1,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 347100,
            "unit": "rays/sec",
            "extra": "CPU: Intel(R) Xeon(R) Platinum 8370C CPU @ 2.80GHz\\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "ff81cb51f24df08becce17e67dc341e7acd0ddf7",
          "message": "Merge pull request #476 from LoveDaisy/fix/config-silent-noop-guards\n\nfix(config): warn on the two silent no-op config shapes",
          "timestamp": "2026-10-08T07:38:14+08:00",
          "tree_id": "72d56acc7970f3c253fb3fb79133f22677d0d85f",
          "url": "https://github.com/LoveDaisy/Lumice/commit/ff81cb51f24df08becce17e67dc341e7acd0ddf7"
        },
        "date": 1791416510607,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 411294.3,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 507563.5,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 367807.3,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 309804.8,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor                \\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "1e88e8c405777eac6aa3f56227a2a0ccd28e82b6",
          "message": "Merge pull request #477 from LoveDaisy/feat/schema3-geometry-port\n\nfeat(analytic): schema3 geometry layer port (u-S2 field, partition, walks, focusing/chromatic, contour, API v8)",
          "timestamp": "2026-10-09T03:56:32+08:00",
          "tree_id": "6d823a2cb115de1d1cc95c3511bd06308e497425",
          "url": "https://github.com/LoveDaisy/Lumice/commit/1e88e8c405777eac6aa3f56227a2a0ccd28e82b6"
        },
        "date": 1791489562734,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 346230.4,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 504794.7,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 368043.1,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 314827.3,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor                \\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "826acd4bd8a4c3757162564f22e74f56e8c39510",
          "message": "Merge pull request #478 from LoveDaisy/feat/schema3-report\n\nfeat(raypath): schema3 report - state machines, support block, unattributed, mc_evidence, schema_version=3",
          "timestamp": "2026-10-10T10:31:01+08:00",
          "tree_id": "6cbc9655451153e80826f2030b8cca8d1547e6bc",
          "url": "https://github.com/LoveDaisy/Lumice/commit/826acd4bd8a4c3757162564f22e74f56e8c39510"
        },
        "date": 1791599684044,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 420467.5,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 506762.4,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 363589.6,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 308295.6,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor                \\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "a2c5d90a7a3f0918d628d6b49921aad3a0a7c8ed",
          "message": "Merge pull request #479 from LoveDaisy/fix/analyze-render-share-calibration\n\nShare-domain calibration: lens landing semantics + dual fold-boundary defect + cone cross-check tool (task-669)",
          "timestamp": "2026-10-10T18:45:27+08:00",
          "tree_id": "45ab14128501225882d5f360859fce217f5b3a9f",
          "url": "https://github.com/LoveDaisy/Lumice/commit/a2c5d90a7a3f0918d628d6b49921aad3a0a7c8ed"
        },
        "date": 1791629296795,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 333008.8,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 505643.3,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 430105.6,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 9V74 80-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 344382.9,
            "unit": "rays/sec",
            "extra": "CPU: INTEL(R) XEON(R) PLATINUM 8573C\\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "6e0a5b01d67218589710fdc63aaa96e5c9bab05d",
          "message": "Merge pull request #480 from LoveDaisy/fix/dual-fisheye-fold-boundary-loss\n\nfix(raypath): clamp dual-fisheye fold-boundary pixels into canvas (task-670)",
          "timestamp": "2026-10-10T22:52:34+08:00",
          "tree_id": "9b991e7eb24effad677fcd4328ccfd41633c9d4f",
          "url": "https://github.com/LoveDaisy/Lumice/commit/6e0a5b01d67218589710fdc63aaa96e5c9bab05d"
        },
        "date": 1791644140574,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 385508.5,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 504767.8,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 366579.9,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 313054.8,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor                \\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "f50e9639b74896229064fc6f302f8fbfa2251e06",
          "message": "Merge pull request #481 from LoveDaisy/chore/mc-corner-tier-contract-sync\n\nClarify mixed-corner tolerance consumer comment",
          "timestamp": "2026-10-11T01:54:48+08:00",
          "tree_id": "485185c3b80de593fe4ee9348baf6c20b9c33bde",
          "url": "https://github.com/LoveDaisy/Lumice/commit/f50e9639b74896229064fc6f302f8fbfa2251e06"
        },
        "date": 1791655080234,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 322342.8,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 505285,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 470423.8,
            "unit": "rays/sec",
            "extra": "CPU: INTEL(R) XEON(R) PLATINUM 8573C\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 312251,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor                \\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "b8396281f9127160a6677a9d802dd3b90c6d91d5",
          "message": "Merge pull request #482 from LoveDaisy/chore/ci-raypath-test-cost-governance\n\nci: reduce repeated raypath test work and govern cumulative duration",
          "timestamp": "2026-10-11T06:49:40+08:00",
          "tree_id": "13f1062264c8f6979884be10399307145b272e07",
          "url": "https://github.com/LoveDaisy/Lumice/commit/b8396281f9127160a6677a9d802dd3b90c6d91d5"
        },
        "date": 1791672779991,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 297466.5,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 506152.6,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 426092.3,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 9V74 80-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 314002.5,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor                \\nCores: 4"
          }
        ]
      }
    ],
    "Multi-worker Throughput": [
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "f10f34cfcd966500675c24f83142ce2b7267622e",
          "message": "Merge pull request #371 from LoveDaisy/feat/adaptive-allocation-gate-statistics\n\ntest(e2e): judge adaptive allocation on row energy with a Šidák worst-row threshold; keep smoke PSNR failure samples",
          "timestamp": "2026-09-16T09:04:20+08:00",
          "tree_id": "93b698302b5b77fb9b6d221be0c96348ecf0fe95",
          "url": "https://github.com/LoveDaisy/Lumice/commit/f10f34cfcd966500675c24f83142ce2b7267622e"
        },
        "date": 1789521265490,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 1008120.9,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1184743.6,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 810061.2,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 664936.8,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor                \\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "4b269f31e994c541505f45df5390fa783fd5eb4c",
          "message": "Merge pull request #372 from LoveDaisy/feat/multi-renderer-gpu-backend\n\nfeat: device-fused multi-renderer sessions on Metal and CUDA (scrum multi-renderer-gpu-backend)",
          "timestamp": "2026-09-16T14:15:30+08:00",
          "tree_id": "2b95071bd3ce2b50817d54682a83c0363b9638fe",
          "url": "https://github.com/LoveDaisy/Lumice/commit/4b269f31e994c541505f45df5390fa783fd5eb4c"
        },
        "date": 1789540152838,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 1059542.2,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1185007.6,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 809074.4,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 672272.6,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor                \\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "eff2ea9db23ead9488cd19184d31ecc1fce4f339",
          "message": "Merge pull request #373 from LoveDaisy/feat/axis-modal-custom-preset-memory\n\nfeat(gui): remember each crystal's last Custom axis triple in the edit modal",
          "timestamp": "2026-09-16T22:02:36+08:00",
          "tree_id": "e82f08c3e0d37f056f07a7e1e0c21781d6a0133a",
          "url": "https://github.com/LoveDaisy/Lumice/commit/eff2ea9db23ead9488cd19184d31ecc1fce4f339"
        },
        "date": 1789568003616,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 1095395.7,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1184720.8,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 802456.9,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 665522.6,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor                \\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "f28752902bc36099d973c676533fb29e7e6e1991",
          "message": "Merge pull request #374 from LoveDaisy/feat/cuda-discrete-spectrum-wl-pool-cache\n\nfix(cuda): rebuild the wl pool every BeginSession under a discrete spectrum",
          "timestamp": "2026-09-16T22:34:39+08:00",
          "tree_id": "8e6d0b7a6f8bcb70d49405b289412534a3c5eb49",
          "url": "https://github.com/LoveDaisy/Lumice/commit/f28752902bc36099d973c676533fb29e7e6e1991"
        },
        "date": 1789569950247,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 829195.1,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1188996.8,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 959731.2,
            "unit": "rays/sec",
            "extra": "CPU: INTEL(R) XEON(R) PLATINUM 8573C\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 670299.9,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor                \\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "ab1ba3499938a637c44b1102fef825610033d17e",
          "message": "Merge pull request #375 from LoveDaisy/feat/isa-engine-dll-dispatch\n\nrelease: one shell per entry point + two internal engine libraries picked by CPUID / glibc-hwcaps (scrum-566)",
          "timestamp": "2026-09-17T09:29:18+08:00",
          "tree_id": "95064d4b64e5245da0848c6223ef22c769d2c506",
          "url": "https://github.com/LoveDaisy/Lumice/commit/ab1ba3499938a637c44b1102fef825610033d17e"
        },
        "date": 1789609471557,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 1221477.2,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1185638,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 1016876.6,
            "unit": "rays/sec",
            "extra": "CPU: INTEL(R) XEON(R) PLATINUM 8573C\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 672423.1,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor                \\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "695de9ed0e5a901e96e0c747c5b6235ed051c10d",
          "message": "Merge pull request #377 from LoveDaisy/chore/ci-e2e-slow-macos-rest-timeout\n\nfix(ci): widen E2E Slow (macOS rest) step timeout 15→25 min",
          "timestamp": "2026-09-17T16:56:21+08:00",
          "tree_id": "de8b1ecd633d6f3a4476f875f401bac57c4c5feb",
          "url": "https://github.com/LoveDaisy/Lumice/commit/695de9ed0e5a901e96e0c747c5b6235ed051c10d"
        },
        "date": 1789636078593,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 836630.7,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1185785.5,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 742597.6,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 9V74 80-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 800993.5,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 9V74 80-Core Processor                \\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "444cec705397fdcccafdc87329cb670fe0a4983d",
          "message": "Merge pull request #376 from LoveDaisy/feat/cuda-persistent-device-buffers\n\nperf(cuda): persist lat_lut buffers and make EnsureSessionBuffers grow-only",
          "timestamp": "2026-09-17T17:17:49+08:00",
          "tree_id": "df38477d8c88f2a853e31bcf30cab5d180449a9c",
          "url": "https://github.com/LoveDaisy/Lumice/commit/444cec705397fdcccafdc87329cb670fe0a4983d"
        },
        "date": 1789637237740,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 926016.8,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1189296.2,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 801324.9,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 664908.5,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor                \\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "fbd9e333c05a864d3df8756f7a171910bc9fd37a",
          "message": "Merge pull request #378 from LoveDaisy/feat/cpu-worker-side-projection\n\nperf(core): move legacy-CPU per-ray projection onto the simulator workers",
          "timestamp": "2026-09-18T04:05:22+08:00",
          "tree_id": "14b5ad3b0df1857b2e43b1dde29f1d4fdd79955a",
          "url": "https://github.com/LoveDaisy/Lumice/commit/fbd9e333c05a864d3df8756f7a171910bc9fd37a"
        },
        "date": 1789676302029,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 898050.1,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1016083.7,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 717472.5,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 573649.8,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor                \\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "f392ee3a91e7a6975dd11b1c7e2867ef8800efa0",
          "message": "Merge pull request #379 from LoveDaisy/feat/cli-raw-float-export\n\nfeat(cli): --format npy raw float32 XYZ export with sidecar metadata",
          "timestamp": "2026-09-19T00:39:06+08:00",
          "tree_id": "7f3a5e880b4c85c0d6f7ca6ecef4f6edd0373fc9",
          "url": "https://github.com/LoveDaisy/Lumice/commit/f392ee3a91e7a6975dd11b1c7e2867ef8800efa0"
        },
        "date": 1789750240988,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 1176429.4,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1020368.7,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 709859.9,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 534424.1,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 9V74 80-Core Processor                \\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "53fa48878ac86e2a3794c6b98f02d20fc12744c0",
          "message": "Merge pull request #380 from LoveDaisy/fix/fp32-accumulator-compensation\n\nfix(server): widen long-chain fp32 accumulators to double (sub-sun hue drift with ray count)",
          "timestamp": "2026-09-19T00:59:38+08:00",
          "tree_id": "f432b8348761a5f45dc462565e3814371088db89",
          "url": "https://github.com/LoveDaisy/Lumice/commit/53fa48878ac86e2a3794c6b98f02d20fc12744c0"
        },
        "date": 1789751528562,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 813344.6,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1017120,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 710106.2,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 573300.8,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor                \\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "11cc9ff591a8d177621c226af7d970ad200ad6fb",
          "message": "Merge pull request #381 from LoveDaisy/feat/lmc-linear-xyz-texture\n\nfeat(gui): store the .lmc preview texture as unexposed linear XYZ (format v5)",
          "timestamp": "2026-09-19T02:24:05+08:00",
          "tree_id": "46941acdc2657c651126351d9db7f5e5664612d3",
          "url": "https://github.com/LoveDaisy/Lumice/commit/11cc9ff591a8d177621c226af7d970ad200ad6fb"
        },
        "date": 1789756617732,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 919791.2,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1013670.6,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 709696.4,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 566169.2,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor                \\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "6f38f1a7758e76516f2e71424805ca2f32020ade",
          "message": "Merge pull request #382 from LoveDaisy/feat/perf-followthrough\n\nperf: follow-through scrum — per-platform worker cap, GUI startup prewarm, CUDA hit-budget fix, PostSnapshot parallelization, measurement discipline",
          "timestamp": "2026-09-19T15:59:56+08:00",
          "tree_id": "6bd42c6a11fb1d48fb9d34ac461583994b914805",
          "url": "https://github.com/LoveDaisy/Lumice/commit/6f38f1a7758e76516f2e71424805ca2f32020ade"
        },
        "date": 1789805606727,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 908904.5,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1016384.9,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 707561.2,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 577298.1,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor                \\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "96c437719d62599f74bab998e9baa5a4192900a7",
          "message": "Merge pull request #384 from LoveDaisy/feat/lmc-f16-texture-dual-path\n\nfeat(gui): .lmc v6 float16 texture with the live preview quantized through the same codec",
          "timestamp": "2026-09-20T01:45:06+08:00",
          "tree_id": "6294bd9290986f14645da62d4429859ba9129d91",
          "url": "https://github.com/LoveDaisy/Lumice/commit/96c437719d62599f74bab998e9baa5a4192900a7"
        },
        "date": 1789840612873,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 903750.3,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1015368.8,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 710456.3,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 574573.3,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor                \\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "07d02b311b0c719947d1ddb96023a365f573e2c7",
          "message": "Merge pull request #385 from LoveDaisy/chore/cuda-canonical-throughput-5090\n\ndocs(perf): first RTX 5090 D canonical throughput columns (home-wsl / home-win) + B/E drain-plane cost matrix",
          "timestamp": "2026-09-20T03:29:12+08:00",
          "tree_id": "42eb1aecd5d088d6fafd75cc593704374199bd52",
          "url": "https://github.com/LoveDaisy/Lumice/commit/07d02b311b0c719947d1ddb96023a365f573e2c7"
        },
        "date": 1789846787676,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 907560.9,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1013805.3,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 709670.4,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 569557.7,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor                \\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "76be31ac78abe09f12455eb5916a081fbac528cf",
          "message": "Merge pull request #386 from LoveDaisy/feat/parallel-rows-idle-core-budget\n\nfix(server): size ParallelRows by an explicit idle-core thread budget (undo the GUI-poll worker regression from #382)",
          "timestamp": "2026-09-20T05:54:03+08:00",
          "tree_id": "622c8e9b60a4128f396e320415097e083aab2610",
          "url": "https://github.com/LoveDaisy/Lumice/commit/76be31ac78abe09f12455eb5916a081fbac528cf"
        },
        "date": 1789855508608,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "Ubuntu ARM64",
            "value": 1018543.2,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 704687.9,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 620521.5,
            "unit": "rays/sec",
            "extra": "CPU: Intel(R) Xeon(R) Platinum 8370C CPU @ 2.80GHz\\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "88a667fb94867eaa45b47b789e101b9b505a99ee",
          "message": "Merge pull request #387 from LoveDaisy/docs/thread-budget-owner-ruling\n\ndocs(gui): record the owner's ruling on the capped render thread budget",
          "timestamp": "2026-09-20T07:45:28+08:00",
          "tree_id": "659f93e361c41c0c2e674a5a3bba6178131d04b4",
          "url": "https://github.com/LoveDaisy/Lumice/commit/88a667fb94867eaa45b47b789e101b9b505a99ee"
        },
        "date": 1789862104628,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "Ubuntu ARM64",
            "value": 1017225.1,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 804518.8,
            "unit": "rays/sec",
            "extra": "CPU: Intel(R) Xeon(R) Platinum 8370C CPU @ 2.80GHz\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 574937,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor                \\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "61b01911736670be1cb86ee587fc006d12b7eefd",
          "message": "Merge pull request #383 from LoveDaisy/fix/cuda-drain-window-fp32-plane\n\nfix(cuda): fold the fp32 XYZ device plane into a double plane every 8 batches (drain-window ledger drift)",
          "timestamp": "2026-09-20T09:41:39+08:00",
          "tree_id": "7251f6578c75bb2a709a992bf46e1bc045a57fe2",
          "url": "https://github.com/LoveDaisy/Lumice/commit/61b01911736670be1cb86ee587fc006d12b7eefd"
        },
        "date": 1789869287207,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "Ubuntu ARM64",
            "value": 1019977.3,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 711925.3,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 579884.5,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor                \\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "747b2ec2a60ffe5e75331b2c0b3753ed05411959",
          "message": "Merge pull request #388 from LoveDaisy/fix/startup-calibration-test-address-reuse\n\ntest(gui): evidence the startup-calibration server rebuild by its worker-count tracker, not by address",
          "timestamp": "2026-09-20T10:28:39+08:00",
          "tree_id": "1b37adad964db6f5ecb871687f0eeb0ee499b8f3",
          "url": "https://github.com/LoveDaisy/Lumice/commit/747b2ec2a60ffe5e75331b2c0b3753ed05411959"
        },
        "date": 1789871948673,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 1107369.4,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1018987.6,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 1092486.4,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 9V45 96-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 608063.7,
            "unit": "rays/sec",
            "extra": "CPU: INTEL(R) XEON(R) PLATINUM 8573C\\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "94b90ab6c623e1aa3679f7aa1718a7cd0730b4fb",
          "message": "Merge pull request #389 from LoveDaisy/task/gui-view-copy-resync-mechanism\n\nfix(gui): resync the edit modal from the pool every frame — Exclude no longer overwritten while the editor is open",
          "timestamp": "2026-09-20T16:23:59+08:00",
          "tree_id": "be5ece68186e20452d6b1ff247e8d4d66e216b7c",
          "url": "https://github.com/LoveDaisy/Lumice/commit/94b90ab6c623e1aa3679f7aa1718a7cd0730b4fb"
        },
        "date": 1789893324460,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 1118112.4,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1016652.5,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 706477.2,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 575523.5,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor                \\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "337ad7c9b1160379b054d8d652114eb883ba7c36",
          "message": "Merge pull request #390 from LoveDaisy/fix/composite-preview-p99-flake-margin\n\ntest(gui): drop the composite re-run p99 ratio that never saw the defect (task-582)",
          "timestamp": "2026-09-20T19:39:18+08:00",
          "tree_id": "dc6df54e496534e5ec18465303203dbf62ed18fd",
          "url": "https://github.com/LoveDaisy/Lumice/commit/337ad7c9b1160379b054d8d652114eb883ba7c36"
        },
        "date": 1789904917254,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 912454.2,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1018394,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 769427.3,
            "unit": "rays/sec",
            "extra": "CPU: INTEL(R) XEON(R) PLATINUM 8573C\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 683010,
            "unit": "rays/sec",
            "extra": "CPU: INTEL(R) XEON(R) PLATINUM 8573C\\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "048801761115ac2403b66262d5f987c2146ca3b1",
          "message": "Merge pull request #391 from LoveDaisy/feat/show-product-version\n\nfeat: show the product version — C API, title bar, --version, startup log, .lmc (task-585)",
          "timestamp": "2026-09-20T20:10:01+08:00",
          "tree_id": "93e17fbc3606c3e3386802a0e7be910d196ab10f",
          "url": "https://github.com/LoveDaisy/Lumice/commit/048801761115ac2403b66262d5f987c2146ca3b1"
        },
        "date": 1789907139579,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 1203843.9,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1018681.9,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 692927.1,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 9V74 80-Core Processor                \\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "a09b72909d5b03ab532b9f908e01ac4ef233c4ac",
          "message": "Merge pull request #392 from LoveDaisy/chore/test-hygiene-sweep\n\nchore: test hygiene sweep — shared LogCapture, static_assert, wire-table test, guard hardening (chore-584)",
          "timestamp": "2026-09-20T20:25:08+08:00",
          "tree_id": "06eb85faf160e5faed09fee676e77bd65d583e70",
          "url": "https://github.com/LoveDaisy/Lumice/commit/a09b72909d5b03ab532b9f908e01ac4ef233c4ac"
        },
        "date": 1789907763572,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 949316.6,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1018482.4,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "distinct": true,
          "id": "81e8d18a17511e66af4f6e17ce5e75a7c7996daf",
          "message": "Merge pull request #392 from LoveDaisy/chore/test-hygiene-sweep\n\nchore: test hygiene sweep — shared LogCapture, static_assert, wire-table test, guard hardening (chore-584)",
          "timestamp": "2026-09-20T20:34:42+08:00",
          "tree_id": "06eb85faf160e5faed09fee676e77bd65d583e70",
          "url": "https://github.com/LoveDaisy/Lumice/commit/81e8d18a17511e66af4f6e17ce5e75a7c7996daf"
        },
        "date": 1789908528047,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 1022358.4,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 703108.5,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 571008.5,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor                \\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "3442752646d7a6d1d1e7c71045a958be19025e4f",
          "message": "Merge pull request #393 from LoveDaisy/feat/look-at-horizon-series\n\nfeat(gui): Look At — Horizon series of four sun-relative level bearings (task-586)",
          "timestamp": "2026-09-20T21:18:38+08:00",
          "tree_id": "2ea46a896126f59b8ad9e871c17836f0b16a9f3b",
          "url": "https://github.com/LoveDaisy/Lumice/commit/3442752646d7a6d1d1e7c71045a958be19025e4f"
        },
        "date": 1789910919448,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 1153857.1,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1016466.3,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 708568.1,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 571880,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor                \\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "a9eb84bee60644a8d4838c4051be47e0b5cda60a",
          "message": "Merge pull request #394 from LoveDaisy/feat/analyze-chain-table-capacity\n\nfeat(analyze): runtime chain-record capacity — --chain-capacity / C API chain_capacity (task-583)",
          "timestamp": "2026-09-20T21:46:35+08:00",
          "tree_id": "f4e0ca5a93ff255549dadfb9d9ff5d2fb530ffac",
          "url": "https://github.com/LoveDaisy/Lumice/commit/a9eb84bee60644a8d4838c4051be47e0b5cda60a"
        },
        "date": 1789912853008,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 928421.9,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1015904.4,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 652147.1,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 9V74 80-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 568497.9,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor                \\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "15a0baf562499726ca8be28b5ac5780a16af813f",
          "message": "Merge pull request #395 from LoveDaisy/feat/axis-preset-type-override\n\nfeat(gui): preset library — zenith type is editable within each preset's accepted set (task-587)",
          "timestamp": "2026-09-20T22:53:50+08:00",
          "tree_id": "49dabf2ddaaebcc7d0fc14e6aa16353b02010817",
          "url": "https://github.com/LoveDaisy/Lumice/commit/15a0baf562499726ca8be28b5ac5780a16af813f"
        },
        "date": 1789916688951,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 1090937.6,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1017921.5,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 709252.1,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 684585.6,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 9V74 80-Core Processor                \\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "1bd37a62468be8c031b66851d29f8687526ad678",
          "message": "Merge pull request #396 from LoveDaisy/feat/config-summary-window\n\nfeat(gui): read-only Summary window — one page of the current configuration for sharing (task-588)",
          "timestamp": "2026-09-21T11:06:18+08:00",
          "tree_id": "a62e92733c5372ed1d234f4bdc7e07e66441a886",
          "url": "https://github.com/LoveDaisy/Lumice/commit/1bd37a62468be8c031b66851d29f8687526ad678"
        },
        "date": 1789960737540,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 861775.9,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1016526.4,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 708358.6,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 578469.8,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor                \\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "f70cdd2b7bddb782f36f438d7742e112b90ce3fc",
          "message": "Merge pull request #397 from LoveDaisy/chore/release-4.6.1\n\nchore(release): cut 4.6.1 — changelog backfill for #368–#396",
          "timestamp": "2026-09-21T12:10:01+08:00",
          "tree_id": "7a67bfaac331a8f7aecdd57d6f9a15673d4db69b",
          "url": "https://github.com/LoveDaisy/Lumice/commit/f70cdd2b7bddb782f36f438d7742e112b90ce3fc"
        },
        "date": 1789964424860,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 995246.5,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1014765.4,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 894891.2,
            "unit": "rays/sec",
            "extra": "CPU: INTEL(R) XEON(R) PLATINUM 8573C\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 564137.2,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor                \\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "64706c37cd5ad912eb76cde70881bb84e9efd681",
          "message": "Merge pull request #398 from LoveDaisy/feat/ui-scale\n\nfeat(gui): ui_scale — DPI-aware layout and font, plus a user UI-scale preference",
          "timestamp": "2026-09-22T17:25:47+08:00",
          "tree_id": "282afffeb45deeca9dcf5eb0ea192084f3466648",
          "url": "https://github.com/LoveDaisy/Lumice/commit/64706c37cd5ad912eb76cde70881bb84e9efd681"
        },
        "date": 1790069883042,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 1032391.7,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1018409.5,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 708775.4,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 886551.5,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 9V45 96-Core Processor                \\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "4d4b78fc4e2fdbd6c38693489ab74498e4d39df0",
          "message": "Merge pull request #399 from LoveDaisy/feat/gui-desktop-conventions-part1\n\nfeat(gui): desktop conventions — deletable last filter row, list search, copyable text, column-major Tab, window sizing policy",
          "timestamp": "2026-09-22T19:41:21+08:00",
          "tree_id": "adccab168ff45ce7559ae9dfb6e96e62e09b4a78",
          "url": "https://github.com/LoveDaisy/Lumice/commit/4d4b78fc4e2fdbd6c38693489ab74498e4d39df0"
        },
        "date": 1790078006538,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 1119913,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1017184.6,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 653151.7,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 9V74 80-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 700617.9,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 9V74 80-Core Processor                \\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "d69f9167201076266c84c99adb68dcf68ac437b1",
          "message": "Merge pull request #400 from LoveDaisy/feat/analysis-exclusion-view\n\nfeat(gui): the analysis list keeps excluded raypaths as greyed rows, with Include again",
          "timestamp": "2026-09-22T20:10:32+08:00",
          "tree_id": "f5c55b74bbe2f5ac5434e9ac3850cd90089af128",
          "url": "https://github.com/LoveDaisy/Lumice/commit/d69f9167201076266c84c99adb68dcf68ac437b1"
        },
        "date": 1790079815063,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 890090,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1015364.1,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 717719.9,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 534909.1,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 9V74 80-Core Processor                \\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "c49ef3f32f9385ae0700b887e5f73faaedd7bea4",
          "message": "Merge pull request #401 from LoveDaisy/feat/gui-desktop-conventions\n\nrefactor(gui): the edit modal's two shapes — Compact and Expanded",
          "timestamp": "2026-09-22T22:24:18+08:00",
          "tree_id": "f1e06a712c5d8c584284204571457aa629c4d097",
          "url": "https://github.com/LoveDaisy/Lumice/commit/c49ef3f32f9385ae0700b887e5f73faaedd7bea4"
        },
        "date": 1790088027925,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 901132.2,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1017916.9,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 844321.2,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 9V74 80-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 571122.8,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor                \\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "c3d3aa1d21c41277d8cf6bc76016d7067cc2448b",
          "message": "Merge pull request #402 from LoveDaisy/feat/summary-reference-version-independence\n\ntest(gui): pin the Summary page's version under gui_test so a release stops reddening its references",
          "timestamp": "2026-09-23T00:07:08+08:00",
          "tree_id": "e22276ed331178ee1e226e97b579203a031ed8ba",
          "url": "https://github.com/LoveDaisy/Lumice/commit/c3d3aa1d21c41277d8cf6bc76016d7067cc2448b"
        },
        "date": 1790093877602,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 1124630.5,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1018437.1,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 841371.5,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 9V74 80-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 576388.4,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor                \\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "b6ed96be5d6acdd7acc63a500e5c045647525ba5",
          "message": "Merge pull request #403 from LoveDaisy/feat/edit-modal-height-and-expanded-polish\n\nfeat(gui): Edit Entry — a draggable height, aligned Expanded headers, standard headings",
          "timestamp": "2026-09-23T09:50:00+08:00",
          "tree_id": "4bd7771eb2b058baf5a35532f33a8b6313a2718b",
          "url": "https://github.com/LoveDaisy/Lumice/commit/b6ed96be5d6acdd7acc63a500e5c045647525ba5"
        },
        "date": 1790128917444,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 1055568.2,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1016430.3,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 779661.4,
            "unit": "rays/sec",
            "extra": "CPU: INTEL(R) XEON(R) PLATINUM 8573C\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 879846.6,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 9V45 96-Core Processor                \\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "2056f69990d8a0df13c199c795260b717c144f29",
          "message": "Merge pull request #404 from LoveDaisy/feat/crystal-projected-area-weighting\n\nfix(core): weight crystal entry by projected area on every backend",
          "timestamp": "2026-09-24T19:58:42+08:00",
          "tree_id": "b0cd16714314f8b35d9687520070dca01b0a5efd",
          "url": "https://github.com/LoveDaisy/Lumice/commit/2056f69990d8a0df13c199c795260b717c144f29"
        },
        "date": 1790251834441,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 906018.2,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1020810.8,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 709803.2,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 574295.8,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor                \\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "108fdd825206a218a0262cfb6533db39965fb3e0",
          "message": "Merge pull request #405 from LoveDaisy/feat/card-insert-and-reorder\n\nfeat(gui): duplicate lands below the source card; drag to reorder cards",
          "timestamp": "2026-09-25T12:52:34+08:00",
          "tree_id": "e30f40ec89cb02353708deba953b4ee18ee297fe",
          "url": "https://github.com/LoveDaisy/Lumice/commit/108fdd825206a218a0262cfb6533db39965fb3e0"
        },
        "date": 1790312914167,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 1028163,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1017919.6,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 807198.2,
            "unit": "rays/sec",
            "extra": "CPU: Intel(R) Xeon(R) Platinum 8370C CPU @ 2.80GHz\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 580271.5,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor                \\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "65514f1a79aafa4a7d5bf1c468123ac24550aa41",
          "message": "Merge pull request #406 from LoveDaisy/feat/sim-continue\n\nfeat: Continue adds rays to a finished render (LUMICE_ContinueRender)",
          "timestamp": "2026-09-25T13:25:12+08:00",
          "tree_id": "86a13a44187986c4d27a4fea1b98d72c471e573d",
          "url": "https://github.com/LoveDaisy/Lumice/commit/65514f1a79aafa4a7d5bf1c468123ac24550aa41"
        },
        "date": 1790314921396,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 888346.8,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1015135.4,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 650681.2,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 9V74 80-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 574226.3,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor                \\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "69b1b656449030f1e93e69f7bd842037f16a8bdc",
          "message": "Merge pull request #407 from LoveDaisy/feat/globe-backside-fog-fade\n\nfeat: globe back-side fade (the far side shows through, fading like fog)",
          "timestamp": "2026-09-25T14:30:07+08:00",
          "tree_id": "e4f340177152a07e767413c1203db933158f3ac0",
          "url": "https://github.com/LoveDaisy/Lumice/commit/69b1b656449030f1e93e69f7bd842037f16a8bdc"
        },
        "date": 1790318815680,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 992191.6,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1013332.6,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 836351.1,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 9V74 80-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 566069.4,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor                \\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "df90ad63b51304ed6867f9285e263e304d346c39",
          "message": "Merge pull request #408 from LoveDaisy/fix/analysis-manual-stop-flake\n\nfix(test): wait for a cone-sized sample before the manual-stop analysis assertion",
          "timestamp": "2026-09-25T15:19:07+08:00",
          "tree_id": "72655d66e9061682075555c399d83140dc3216f5",
          "url": "https://github.com/LoveDaisy/Lumice/commit/df90ad63b51304ed6867f9285e263e304d346c39"
        },
        "date": 1790321719093,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 921574,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1017841.3,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 714671.2,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 568688.5,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor                \\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "5c29ec370e9e6fcae68806e5d82c40e8df5359a2",
          "message": "Merge pull request #409 from LoveDaisy/fix/continue-render-plane-total-flake\n\nfix(test): calibrate ContinueRender's plane-total band to per-batch wavelength noise",
          "timestamp": "2026-09-25T15:58:37+08:00",
          "tree_id": "616829b9a9cb568528885cd75ef28c022146191b",
          "url": "https://github.com/LoveDaisy/Lumice/commit/5c29ec370e9e6fcae68806e5d82c40e8df5359a2"
        },
        "date": 1790324478064,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "Ubuntu ARM64",
            "value": 1012741,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 714562.7,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 569525.3,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor                \\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "a2d088f53ed6ac75eeac825ca6f5ec003084e6cd",
          "message": "Merge pull request #410 from LoveDaisy/feat/channel-math-display-mode\n\nfeat: Channel B−R display mode (is this spot bluer or redder?)",
          "timestamp": "2026-09-25T16:20:42+08:00",
          "tree_id": "9492740c7c72869d21a8148f8567070eaca19d7b",
          "url": "https://github.com/LoveDaisy/Lumice/commit/a2d088f53ed6ac75eeac825ca6f5ec003084e6cd"
        },
        "date": 1790325342783,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 1186194.6,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1013804,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 718257.6,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 678197.8,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 9V74 80-Core Processor                \\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "49fe5e10fab9bfff09a5c101fbf651f8f283af5b",
          "message": "Merge pull request #411 from LoveDaisy/fix/globe-back-fade-fma-exact-eq\n\nfix(test): compare the globe back-fade weight within an absolute tolerance",
          "timestamp": "2026-09-25T16:38:58+08:00",
          "tree_id": "1ad240095c7e9a80ab3a0f9f3c9ec1c87f9c4465",
          "url": "https://github.com/LoveDaisy/Lumice/commit/49fe5e10fab9bfff09a5c101fbf651f8f283af5b"
        },
        "date": 1790326331566,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 754361,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1013580,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 832554.9,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 9V74 80-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 681198.2,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 9V74 80-Core Processor                \\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "69d8136c8f926571410a99548a5e7374bf7f0ab2",
          "message": "Merge pull request #412 from LoveDaisy/perf/cpu-worker-batch-sync-cliff\n\nperf(server): decouple the CPU queue handoff from the 128-ray physics batch",
          "timestamp": "2026-09-25T20:57:22+08:00",
          "tree_id": "09c4693994679e1f4660fbb9b8892f3e4bb629e4",
          "url": "https://github.com/LoveDaisy/Lumice/commit/69d8136c8f926571410a99548a5e7374bf7f0ab2"
        },
        "date": 1790341824493,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 805160.3,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1024643.4,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 731445.7,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 705311.2,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 9V74 80-Core Processor                \\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "3c9730817a17b3c99d8469c7cfc7f9a7ceee6e38",
          "message": "Merge pull request #413 from LoveDaisy/docs/raypath-analysis-overview\n\ndocs(raypath-analysis): feature overview and roadmap",
          "timestamp": "2026-09-25T22:51:44+08:00",
          "tree_id": "c79b7399ed96b48eaf32a502f0cc41512296376e",
          "url": "https://github.com/LoveDaisy/Lumice/commit/3c9730817a17b3c99d8469c7cfc7f9a7ceee6e38"
        },
        "date": 1790348570955,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 1099502.6,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1021807.2,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 797192.2,
            "unit": "rays/sec",
            "extra": "CPU: INTEL(R) XEON(R) PLATINUM 8573C\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 586912.6,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor                \\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "6a148bbcba23d5e330a528600062b88fbe62aae5",
          "message": "Merge pull request #414 from LoveDaisy/perf/cpu-per-ray-wavelength\n\nperf(core): stratify the CPU illuminant wavelength across physics batches",
          "timestamp": "2026-09-25T23:31:46+08:00",
          "tree_id": "b0ecff07f1973fc2a59863cf12726b470566f04c",
          "url": "https://github.com/LoveDaisy/Lumice/commit/6a148bbcba23d5e330a528600062b88fbe62aae5"
        },
        "date": 1790351100247,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 797618.9,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1023832.5,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 736770.4,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 648817.4,
            "unit": "rays/sec",
            "extra": "CPU: Intel(R) Xeon(R) Platinum 8370C CPU @ 2.80GHz\\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "48679991416540658f3039cdbde51520d6375e79",
          "message": "Merge pull request #415 from LoveDaisy/feat/gui-front-effective-value\n\nfix(gui): read the effective front clip under lenses it does not apply to",
          "timestamp": "2026-09-26T01:08:03+08:00",
          "tree_id": "9f96f5fa48eafea0128b3d270b751019093977c1",
          "url": "https://github.com/LoveDaisy/Lumice/commit/48679991416540658f3039cdbde51520d6375e79"
        },
        "date": 1790356917274,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 879266.8,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1023783.6,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 856580.8,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 9V74 80-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 596256.1,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor                \\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "890b32d27ca52132e0f8567199aa38f079614db9",
          "message": "Merge pull request #416 from LoveDaisy/feat/globe-back-fade-expfog-and-grid\n\nfeat: globe back-side fade becomes exponential fog, and far-side lines fade with it",
          "timestamp": "2026-09-26T01:37:37+08:00",
          "tree_id": "f71d2148439000301d8f261d490aa085c8114a70",
          "url": "https://github.com/LoveDaisy/Lumice/commit/890b32d27ca52132e0f8567199aa38f079614db9"
        },
        "date": 1790358578550,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 969551.9,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1023340.1,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 728241.8,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 545644.3,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 9V74 80-Core Processor                \\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "b5a1a477c67196240aa5db0dd933b77760fa2182",
          "message": "Merge pull request #417 from LoveDaisy/feat/toolbar-button-sizing-continue-color\n\nfeat(gui): group the top bar and give Continue its own colour",
          "timestamp": "2026-09-26T02:20:15+08:00",
          "tree_id": "341993077498e5caf3df7edb8433227b484ffc25",
          "url": "https://github.com/LoveDaisy/Lumice/commit/b5a1a477c67196240aa5db0dd933b77760fa2182"
        },
        "date": 1790361084028,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 920114.7,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1026089.3,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 724542.3,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 590354.9,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor                \\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "c6d32d110ca90680080935d49bda6d00274d6ac7",
          "message": "Merge pull request #418 from LoveDaisy/feat/axis-preset-highlight-summary\n\nfeat(gui): highlight the active axis preset and summarize the crystal under the edit-modal preview",
          "timestamp": "2026-09-26T03:24:31+08:00",
          "tree_id": "0c4591c98e7a93246c6b7ac5869eb931880bb594",
          "url": "https://github.com/LoveDaisy/Lumice/commit/c6d32d110ca90680080935d49bda6d00274d6ac7"
        },
        "date": 1790365020253,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 869322,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1024453.4,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 729075.2,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 594661,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor                \\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "3493b66dd3bb005b0ece61229caadab5c3c3fd09",
          "message": "Merge pull request #419 from LoveDaisy/feat/display-mode-viewport-control\n\nfeat(gui): display mode becomes a segmented control in the preview's corner",
          "timestamp": "2026-09-26T03:55:16+08:00",
          "tree_id": "97ddaa25b9147c1d43eec4ed3cd226d46d8195bb",
          "url": "https://github.com/LoveDaisy/Lumice/commit/3493b66dd3bb005b0ece61229caadab5c3c3fd09"
        },
        "date": 1790366938227,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 1029633.8,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1025248.3,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 730291.8,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 610478.7,
            "unit": "rays/sec",
            "extra": "CPU: INTEL(R) XEON(R) PLATINUM 8573C\\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "208ad64e99c9a72de9042b4c4b28db4829cca387",
          "message": "Merge pull request #420 from LoveDaisy/feat/toolbar-revert-slot-gap\n\nfeat(gui): move the top bar's hidden Revert slot to the trailing end",
          "timestamp": "2026-09-26T05:03:32+08:00",
          "tree_id": "cad7a53a552485d156dcaa7b57ac91e01b2e2f7d",
          "url": "https://github.com/LoveDaisy/Lumice/commit/208ad64e99c9a72de9042b4c4b28db4829cca387"
        },
        "date": 1790370994862,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 956603.7,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1024193.3,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 664401.6,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 9V74 80-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 598520.3,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor                \\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "e12c699d7262aa07039ba1d5aa5715e65f78bf5c",
          "message": "Merge pull request #421 from LoveDaisy/feat/angular-dist-picker\n\nfeat(gui): pick an angular-distance ring on the preview",
          "timestamp": "2026-09-26T05:43:54+08:00",
          "tree_id": "2bc14239d813e7ec2ceecf67e4adb4be66e5ba9f",
          "url": "https://github.com/LoveDaisy/Lumice/commit/e12c699d7262aa07039ba1d5aa5715e65f78bf5c"
        },
        "date": 1790373463867,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 775611,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1028224.1,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 784148.1,
            "unit": "rays/sec",
            "extra": "CPU: INTEL(R) XEON(R) PLATINUM 8573C\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 593617.2,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor                \\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "b0b090813c2e811b472eb839916a357272554a9c",
          "message": "Merge pull request #422 from LoveDaisy/feat/screenshot-export-options\n\nfeat(gui): screenshot export options popup, subtract-only per family",
          "timestamp": "2026-09-26T06:04:43+08:00",
          "tree_id": "387bf188ef1a8b8098005bded4fd012477ff7205",
          "url": "https://github.com/LoveDaisy/Lumice/commit/b0b090813c2e811b472eb839916a357272554a9c"
        },
        "date": 1790374711404,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 1149590,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1023028.5,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 731895,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 547859.4,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 9V74 80-Core Processor                \\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "169a01b75ac4932ed78b13aefb385e4e6087b765",
          "message": "Merge pull request #423 from LoveDaisy/fix/adaptive-alloc-c1-bias-probe\n\ntest(e2e): confirm an adaptive-vs-proportional mean red on 60 more sessions per arm",
          "timestamp": "2026-09-26T08:31:46+08:00",
          "tree_id": "42c5899e6e3d28b93caca02621779566a7678c6a",
          "url": "https://github.com/LoveDaisy/Lumice/commit/169a01b75ac4932ed78b13aefb385e4e6087b765"
        },
        "date": 1790383354676,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 983252.3,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1026565.9,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 731698.8,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 877309.8,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 9V45 96-Core Processor                \\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "f185f3da689e6f99f72572f6b6198659524a0cfa",
          "message": "Merge pull request #424 from LoveDaisy/fix/globe-far-side-own-visibility\n\nfix: clip each side of the globe by its own direction's visibility",
          "timestamp": "2026-09-26T11:01:44+08:00",
          "tree_id": "1d3bdfa97e8e41f70adb6f2c18d2203221bfa8e4",
          "url": "https://github.com/LoveDaisy/Lumice/commit/f185f3da689e6f99f72572f6b6198659524a0cfa"
        },
        "date": 1790392428497,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 911781.2,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1026172.4,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 1170390.2,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 9V45 96-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 921938.2,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 9V45 96-Core Processor                \\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "e4adc5bd4c991f32ec7d292e06eaeb6773249573",
          "message": "Merge pull request #425 from LoveDaisy/chore/release-4.7.0\n\nchore(release): cut 4.7.0",
          "timestamp": "2026-09-26T11:42:17+08:00",
          "tree_id": "31eb8c69a51d64a2fd2f696314ecf1251915e92e",
          "url": "https://github.com/LoveDaisy/Lumice/commit/e4adc5bd4c991f32ec7d292e06eaeb6773249573"
        },
        "date": 1790394804323,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 1047765.7,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1025040.7,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 733371.8,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 704144.8,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 9V74 80-Core Processor                \\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "7b107c5623d51c5578890e40cfa4b2a1e2545b02",
          "message": "Merge pull request #426 from LoveDaisy/chore/raypath-analysis-lumice-integral-plan\n\ndocs(raypath-analysis): expand §5.1 into the post-selection three-feature plan",
          "timestamp": "2026-09-27T12:04:30+08:00",
          "tree_id": "22a4d5cac931dca24cc45e2267cdd77444575d99",
          "url": "https://github.com/LoveDaisy/Lumice/commit/7b107c5623d51c5578890e40cfa4b2a1e2545b02"
        },
        "date": 1790482583330,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 787220.2,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1024176.2,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 731723.9,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 590784.4,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor                \\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "bbe3d1883d9873540b5709ec8aa616be79897b36",
          "message": "Merge pull request #427 from LoveDaisy/chore/raypath-analysis-compute-timing\n\ndocs(raypath-analysis): compute location moves per module maturity",
          "timestamp": "2026-09-27T13:51:45+08:00",
          "tree_id": "f291db49905b10460d4a5110faf832aca1be33b0",
          "url": "https://github.com/LoveDaisy/Lumice/commit/bbe3d1883d9873540b5709ec8aa616be79897b36"
        },
        "date": 1790489018162,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 939759.8,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1026820.8,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 767976.7,
            "unit": "rays/sec",
            "extra": "CPU: INTEL(R) XEON(R) PLATINUM 8573C\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 908829.4,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 9V45 96-Core Processor                \\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "cd82b68f1c2ab9e12e9b4f3b56f752998e9e3f34",
          "message": "Merge pull request #428 from LoveDaisy/chore/analyze-product-form-docs\n\ndocs(raypath-analysis): Analyze as the second product core (§5.1.8)",
          "timestamp": "2026-09-27T14:38:00+08:00",
          "tree_id": "79d92ce67a0cff0ef0251d949186ab2eade8ef5c",
          "url": "https://github.com/LoveDaisy/Lumice/commit/cd82b68f1c2ab9e12e9b4f3b56f752998e9e3f34"
        },
        "date": 1790491714588,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 1184019.5,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1020633.2,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 1122531.1,
            "unit": "rays/sec",
            "extra": "CPU: Intel(R) Xeon(R) 6973P-C\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 560312.9,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 9V74 80-Core Processor                \\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "89c8c1cc6f2798f0c7ef0ea8a8c5f89d7bda19ec",
          "message": "Merge pull request #429 from LoveDaisy/task/raypath-reduce-period-from-geometry\n\nfix(core): reduce raypaths only under the symmetry the crystal shape admits",
          "timestamp": "2026-09-27T17:35:49+08:00",
          "tree_id": "e56285d05d6ce1cec96e104506cb4b053d292439",
          "url": "https://github.com/LoveDaisy/Lumice/commit/89c8c1cc6f2798f0c7ef0ea8a8c5f89d7bda19ec"
        },
        "date": 1790502704581,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 1139735.4,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1025967.7,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 669057.9,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 9V74 80-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 588014.1,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor                \\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "7b889f79db8568517726cf7bd294d13c757a8974",
          "message": "Merge pull request #430 from LoveDaisy/task/raypath-pb-ensemble-applicability\n\nfix(core): apply P/B raypath symmetry only where the orientation ensemble admits it",
          "timestamp": "2026-09-27T20:03:03+08:00",
          "tree_id": "66c883c3d1bbf83bbd049a58b95f6e6c004751b1",
          "url": "https://github.com/LoveDaisy/Lumice/commit/7b889f79db8568517726cf7bd294d13c757a8974"
        },
        "date": 1790511581661,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 1033460.8,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1027127.1,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 727055.1,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 583254.8,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor                \\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "ac0fbdf294cc91312b0c421c64b99e05db8828fb",
          "message": "Merge pull request #431 from LoveDaisy/task/gpu-reduce-follows-cpu-symmetry\n\nfix(core): device filter reduction follows the crystal's shape and ensemble symmetry",
          "timestamp": "2026-09-27T21:42:14+08:00",
          "tree_id": "900e6e02f3b3e8425e7b98cde0722045fca77d1c",
          "url": "https://github.com/LoveDaisy/Lumice/commit/ac0fbdf294cc91312b0c421c64b99e05db8828fb"
        },
        "date": 1790517274397,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 916091.9,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1025863.5,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 859514.5,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 9V74 80-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 700775.4,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 9V74 80-Core Processor                \\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "b845c83f0138957e21f6ba7e7bcf5c1f42a0dfd1",
          "message": "Merge pull request #432 from LoveDaisy/chore/analyze-workspace-layout-docs\n\ndocs: finalize the Analyze workspace layout (raypath-analysis §5.1.8)",
          "timestamp": "2026-09-28T00:31:26+08:00",
          "tree_id": "26b84bc5a84d9c0089605c7b002cce526807c507",
          "url": "https://github.com/LoveDaisy/Lumice/commit/b845c83f0138957e21f6ba7e7bcf5c1f42a0dfd1"
        },
        "date": 1790527391044,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 899237.5,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1025628.7,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 728915,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 585335.7,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor                \\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "25b83e949546f15f97e44007554cd6d5a4ddd1e3",
          "message": "Merge pull request #433 from LoveDaisy/chore/repo-roles-and-shared-lib-docs\n\ndocs: repository roles for Lumice / Lumice Integral, and the shared library's first consumer",
          "timestamp": "2026-09-28T01:28:05+08:00",
          "tree_id": "035c8e3864a2fee8909f0af50fb76aaff2835c1a",
          "url": "https://github.com/LoveDaisy/Lumice/commit/25b83e949546f15f97e44007554cd6d5a4ddd1e3"
        },
        "date": 1790530891167,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 833015.1,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1024976.5,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 727625.2,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 700986.3,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 9V74 80-Core Processor                \\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "26ab6a18e6455817e83e95ad76594539cffc24d6",
          "message": "Merge pull request #434 from LoveDaisy/chore/ray-allocation-settings-combo\n\ngui: sim.ray_allocation Settings editor becomes an adaptive/proportional combo",
          "timestamp": "2026-09-28T01:59:27+08:00",
          "tree_id": "f911df15b6d21e6900447e072754865902bb15a7",
          "url": "https://github.com/LoveDaisy/Lumice/commit/26ab6a18e6455817e83e95ad76594539cffc24d6"
        },
        "date": 1790532803992,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 980466,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1027971.8,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 728829.6,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 583220.9,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor                \\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "a7dc7af83f8475e1465a1e0705a2c6437b0e04dc",
          "message": "Merge pull request #435 from LoveDaisy/task/adopt-relative-floor-ray-allocation\n\ncore: relative floor in adaptive ray allocation stops starving a filtered arc",
          "timestamp": "2026-09-28T09:32:31+08:00",
          "tree_id": "e682da94016f1a73dff66ec1f11396547391e218",
          "url": "https://github.com/LoveDaisy/Lumice/commit/a7dc7af83f8475e1465a1e0705a2c6437b0e04dc"
        },
        "date": 1790559880231,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 897619.4,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1026009.7,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 734400.7,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 926714.4,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 9V45 96-Core Processor                \\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "1b0a0361b3465b124b87fc0680bff113b21524d4",
          "message": "Merge pull request #436 from LoveDaisy/task/restore-pb-label-equivalence\n\ncore: a filter's P/B/D is a label equivalence again; the analysis list keeps physical symmetry",
          "timestamp": "2026-09-28T10:02:45+08:00",
          "tree_id": "74c642836565436b225b79dbea5e311790408de7",
          "url": "https://github.com/LoveDaisy/Lumice/commit/1b0a0361b3465b124b87fc0680bff113b21524d4"
        },
        "date": 1790561936853,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 839981,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1025510.8,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 929337.7,
            "unit": "rays/sec",
            "extra": "CPU: INTEL(R) XEON(R) PLATINUM 8573C\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 590540.2,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor                \\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "1455e6aeb0201ab8b80fe2ef34ff16cf1ae2ed30",
          "message": "Merge pull request #437 from LoveDaisy/chore/symmetry-two-meanings-cross-repo-docs\n\ndocs: state which symmetry meaning each repo's computation uses",
          "timestamp": "2026-09-28T10:49:54+08:00",
          "tree_id": "07236666f17979fcb194db2933d7e276e6f5c16e",
          "url": "https://github.com/LoveDaisy/Lumice/commit/1455e6aeb0201ab8b80fe2ef34ff16cf1ae2ed30"
        },
        "date": 1790564453416,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 907934.3,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1027019.2,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 730384.5,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 586433,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor                \\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "5d353c63c4688fb9be43a5042ef239a4c310a93b",
          "message": "Merge pull request #439 from LoveDaisy/feat/channel-br-gain-x2\n\nChannel B-R display mode: raise gain from 0.5 to 2",
          "timestamp": "2026-09-28T14:33:59+08:00",
          "tree_id": "0b0e5066e995207a2d2bb4b5603b597496d80793",
          "url": "https://github.com/LoveDaisy/Lumice/commit/5d353c63c4688fb9be43a5042ef239a4c310a93b"
        },
        "date": 1790577946053,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 947487.2,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1024767.7,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 938344.4,
            "unit": "rays/sec",
            "extra": "CPU: INTEL(R) XEON(R) PLATINUM 8573C\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 593124.7,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor                \\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "51bc059b3fa58ffeeef7ae600d704bf242a639de",
          "message": "Merge pull request #440 from LoveDaisy/feat/config-change-preview-transition\n\nGUI: a struct-hard edit no longer blanks the preview",
          "timestamp": "2026-09-28T15:21:22+08:00",
          "tree_id": "8725666a1d1fef58c78a1647e3213d2e90e95384",
          "url": "https://github.com/LoveDaisy/Lumice/commit/51bc059b3fa58ffeeef7ae600d704bf242a639de"
        },
        "date": 1790580907707,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 983996.3,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1022367.6,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 859778.1,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 9V74 80-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 641188.9,
            "unit": "rays/sec",
            "extra": "CPU: Intel(R) Xeon(R) Platinum 8370C CPU @ 2.80GHz\\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "e6d934376e48fc8fc854f3d4f4a866d325bcd3bd",
          "message": "Merge pull request #441 from LoveDaisy/scrum/lumice-shared-lib-foundation\n\nPublish-ready shared library foundation: liblumice_analytic, per-library export lists, packaging",
          "timestamp": "2026-09-28T17:16:31+08:00",
          "tree_id": "d9f2f2216737e9645bf134e5c5a1d6d6b8628eca",
          "url": "https://github.com/LoveDaisy/Lumice/commit/e6d934376e48fc8fc854f3d4f4a866d325bcd3bd"
        },
        "date": 1790588099760,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 936619.1,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1026651.6,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 814825.2,
            "unit": "rays/sec",
            "extra": "CPU: Intel(R) Xeon(R) Platinum 8370C CPU @ 2.80GHz\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 589799.2,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor                \\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "01b9dd59561fea636f6f0a888b375d2e42e95374",
          "message": "Merge pull request #442 from LoveDaisy/chore/release-4.7.1\n\nchore(release): cut 4.7.1",
          "timestamp": "2026-09-28T19:04:03+08:00",
          "tree_id": "67bb9062b1e0b8474469bb9b06c73e3f3b84ce30",
          "url": "https://github.com/LoveDaisy/Lumice/commit/01b9dd59561fea636f6f0a888b375d2e42e95374"
        },
        "date": 1790594370448,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 909272.4,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1021104.3,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 1117288,
            "unit": "rays/sec",
            "extra": "CPU: Intel(R) Xeon(R) 6973P-C\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 592032.8,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor                \\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "591c97d4c35ccce4e8f3f6dd1cf340e93390e6e4",
          "message": "Merge pull request #443 from LoveDaisy/chore/workflow-token-least-privilege\n\nci: add top-level permissions: contents: read to workflows",
          "timestamp": "2026-09-28T20:18:35+08:00",
          "tree_id": "200ab539ed87c0bd2915e7f5e25f6ed5dac388e4",
          "url": "https://github.com/LoveDaisy/Lumice/commit/591c97d4c35ccce4e8f3f6dd1cf340e93390e6e4"
        },
        "date": 1790598762889,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 903281.6,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1028729.9,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 1089354.5,
            "unit": "rays/sec",
            "extra": "CPU: Intel(R) Xeon(R) 6973P-C\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 593747.5,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor                \\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "73b0f697916c89df61e26e9fb12ee6bc0cbc6afb",
          "message": "Merge pull request #444 from LoveDaisy/scrum/analytic-module-a-v0\n\nliblumice_analytic module A v0: EvaluatePath, TraceFiber, DiscoverComponents + LI parity",
          "timestamp": "2026-09-29T09:40:58+08:00",
          "tree_id": "7dbc414388ddfcacdb2e91a6a1ddb3596a584cf4",
          "url": "https://github.com/LoveDaisy/Lumice/commit/73b0f697916c89df61e26e9fb12ee6bc0cbc6afb"
        },
        "date": 1790646742910,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 965494.3,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1030160.3,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 668902.7,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 9V74 80-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 593550.3,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor                \\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "e129d76c7ac6844f6499a631f39e7eb81f169450",
          "message": "Merge pull request #445 from LoveDaisy/scrum/raypath-subcommand\n\nLumice raypath: single-path analysis module, lumice.h v4.50 entry, D4 verification",
          "timestamp": "2026-09-29T18:08:24+08:00",
          "tree_id": "940b3fbc2d846b901770e2c322d5509ac5278264",
          "url": "https://github.com/LoveDaisy/Lumice/commit/e129d76c7ac6844f6499a631f39e7eb81f169450"
        },
        "date": 1790677424471,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 1007140.8,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1025512,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 727762.3,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 626955.7,
            "unit": "rays/sec",
            "extra": "CPU: INTEL(R) XEON(R) PLATINUM 8573C\\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "9282fe084bf6c22f1620b39162daca6413f1cd29",
          "message": "Merge pull request #446 from LoveDaisy/scrum/analytic-wave2\n\nliblumice_analytic wave 2: per-pose diagnostics (API 5), band sum module B (API 6), 93 LI parity fixtures",
          "timestamp": "2026-09-30T08:45:42+08:00",
          "tree_id": "6d73ac11e978bac05aa5833e3d6725c7fa212092",
          "url": "https://github.com/LoveDaisy/Lumice/commit/9282fe084bf6c22f1620b39162daca6413f1cd29"
        },
        "date": 1790729851806,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 1044620.4,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1021256.2,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 730103.7,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 588787.3,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor                \\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "c0f0e1fe7ec5d6c1f1ebc35c76c60ee01de9c46b",
          "message": "Merge pull request #452 from LoveDaisy/chore/analytic-docs-weight-kink-critical-lines\n\ndocs(analytic): three kinds of critical lines; LI 31c682e parity fixtures (94)",
          "timestamp": "2026-09-30T19:42:02+08:00",
          "tree_id": "352b8d146251654b54e578eea86b4e7382b26ce7",
          "url": "https://github.com/LoveDaisy/Lumice/commit/c0f0e1fe7ec5d6c1f1ebc35c76c60ee01de9c46b"
        },
        "date": 1790769323373,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 1007004.5,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1026046.1,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 854748.5,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 9V74 80-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 585771.9,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor                \\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "5a6e0df8340efd9cf0b86e005e69a70cd1d7bf5c",
          "message": "Merge pull request #453 from LoveDaisy/scrum/capability-libs\n\nOrganize the C API and engine by capability",
          "timestamp": "2026-09-30T22:20:49+08:00",
          "tree_id": "e33e93629598cfbac336b950cd72445b26fb5b14",
          "url": "https://github.com/LoveDaisy/Lumice/commit/5a6e0df8340efd9cf0b86e005e69a70cd1d7bf5c"
        },
        "date": 1790779828312,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 827051.4,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1029368.5,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 819360.8,
            "unit": "rays/sec",
            "extra": "CPU: Intel(R) Xeon(R) Platinum 8370C CPU @ 2.80GHz\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 551134.3,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 9V74 80-Core Processor                \\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "6311b6bd9421e45d367cc351d08ae0cc257a3148",
          "message": "Merge pull request #455 from LoveDaisy/refactor/capi-scene-codec-split\n\nSplit c_api_scene.cpp into bridge, encoder and decoder",
          "timestamp": "2026-09-30T23:35:26+08:00",
          "tree_id": "e2bf504be9af427f91adf8d0e55d5f369cf2e35b",
          "url": "https://github.com/LoveDaisy/Lumice/commit/6311b6bd9421e45d367cc351d08ae0cc257a3148"
        },
        "date": 1790783639636,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 1031682.7,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1024088,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 727008.3,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 588746.3,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor                \\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "1d6bb6f143908c461fabfc30bd29f4f94e31dbc0",
          "message": "Merge pull request #451 from LoveDaisy/scrum/ci-time-governance\n\nscrum ci-time-governance (WIP): build-time landing",
          "timestamp": "2026-10-01T06:48:43+08:00",
          "tree_id": "14a7af4467d0122e428a63d9dfec71858a8ec59a",
          "url": "https://github.com/LoveDaisy/Lumice/commit/1d6bb6f143908c461fabfc30bd29f4f94e31dbc0"
        },
        "date": 1790808685269,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 1189691.6,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1029301.2,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 665906.1,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 9V74 80-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 589112.5,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor                \\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "7f16d97d16354d0b0e38424d0ee0c8cd2e53acf4",
          "message": "Merge pull request #462 from LoveDaisy/scrum/ci-time-governance\n\nci: continue time-governance calibration",
          "timestamp": "2026-10-01T17:12:50+08:00",
          "tree_id": "e0ea583d7775397055133bfe6213ee233562a204",
          "url": "https://github.com/LoveDaisy/Lumice/commit/7f16d97d16354d0b0e38424d0ee0c8cd2e53acf4"
        },
        "date": 1790846131376,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 1221266.8,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1024310.1,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 702594.5,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 9V74 80-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 598658.4,
            "unit": "rays/sec",
            "extra": "CPU: INTEL(R) XEON(R) PLATINUM 8573C\\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "0abb8e9bdc05a0db2723e33dd75904da10092d52",
          "message": "Merge pull request #463 from LoveDaisy/scrum/ci-time-governance-acceptance\n\ndocs(ci): close time-governance acceptance",
          "timestamp": "2026-10-01T18:29:14+08:00",
          "tree_id": "ad1f485ce54ff636e4d44ade48dbe43ca58fc4ff",
          "url": "https://github.com/LoveDaisy/Lumice/commit/0abb8e9bdc05a0db2723e33dd75904da10092d52"
        },
        "date": 1790850729021,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 1005199.5,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1024676.2,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 734686.8,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 592120.6,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor                \\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "4f133865f5df1f6ebbca26757196056d78e762bc",
          "message": "Merge pull request #464 from LoveDaisy/scrum/raypath-feature-diagnostics\n\nAdd target-free raypath feature diagnostics and scientific acceptance",
          "timestamp": "2026-10-02T13:12:28+08:00",
          "tree_id": "e5a02d2855e8ff1e8e73ae6d2a5f6ad04f028841",
          "url": "https://github.com/LoveDaisy/Lumice/commit/4f133865f5df1f6ebbca26757196056d78e762bc"
        },
        "date": 1790918105581,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 953082.3,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1026567.6,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 724835.1,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 708621.3,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 9V74 80-Core Processor                \\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "5fd26ee965155e74cedf143214fd9a1c3a9a0314",
          "message": "Merge pull request #465 from LoveDaisy/fix/ui-scale-window-geometry\n\nfix(gui): keep background aspect ratios consistent across UI scales",
          "timestamp": "2026-10-03T10:47:02+08:00",
          "tree_id": "2223b1e6369a418cca96f536688a1de685794a1c",
          "url": "https://github.com/LoveDaisy/Lumice/commit/5fd26ee965155e74cedf143214fd9a1c3a9a0314"
        },
        "date": 1790995801206,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 819798.6,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1023683.2,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 725381.1,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 946820.7,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 9V45 96-Core Processor                \\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "008837c9802a7b770cb84375e081eaa995c9a7ea",
          "message": "Merge pull request #466 from LoveDaisy/fix/channel-br-background-free\n\nfix(render): exclude sky background from B-R diagnostic",
          "timestamp": "2026-10-03T20:14:42+08:00",
          "tree_id": "c2814c53d58b59a0419d2d23b677c39bebde293c",
          "url": "https://github.com/LoveDaisy/Lumice/commit/008837c9802a7b770cb84375e081eaa995c9a7ea"
        },
        "date": 1791029854861,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 943704.4,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1026228.3,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 726510.1,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 594756.9,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor                \\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "b6107e0c78b94e37fde81d77d8b596e16a2d3452",
          "message": "Merge pull request #467 from LoveDaisy/fix/pytest-performance-pool-isolation\n\nfix(testing): isolate local slow pytest performance pool",
          "timestamp": "2026-10-03T22:58:56+08:00",
          "tree_id": "2bb7504b7cfd61fc451738fe5ed1567077d8d80b",
          "url": "https://github.com/LoveDaisy/Lumice/commit/b6107e0c78b94e37fde81d77d8b596e16a2d3452"
        },
        "date": 1791039679734,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 1241635.9,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1027606,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 652597.8,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 9V74 80-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 938731.5,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 9V45 96-Core Processor                \\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "928b8accd21aa950eeb249c13ad4bd8b4028b1b9",
          "message": "Merge pull request #468 from LoveDaisy/scrum/raypath-general-diagnostics-v2\n\nfeat(raypath): phase-1 raypath feature diagnostics (scrum-649)",
          "timestamp": "2026-10-05T12:17:39+08:00",
          "tree_id": "b9531b058ecf0cc3746ac4253f6cbb3fd2ee8930",
          "url": "https://github.com/LoveDaisy/Lumice/commit/928b8accd21aa950eeb249c13ad4bd8b4028b1b9"
        },
        "date": 1791174023235,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 864661.1,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1017698.3,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 1124667.2,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 9V45 96-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 650506.3,
            "unit": "rays/sec",
            "extra": "CPU: Intel(R) Xeon(R) Platinum 8370C CPU @ 2.80GHz\\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "6abcd36409158fd01a12b110cb2c89ba688dc8a1",
          "message": "Merge pull request #470 from LoveDaisy/fix/sky-direction-convention-mismatch\n\nfix(sky-direction): resolve convention-mismatch investigation — call sites proven correct, defense lines landed",
          "timestamp": "2026-10-06T05:26:14+08:00",
          "tree_id": "bc8d78bd7c6d3609d7eccaeeaf39dcb4e38e96f7",
          "url": "https://github.com/LoveDaisy/Lumice/commit/6abcd36409158fd01a12b110cb2c89ba688dc8a1"
        },
        "date": 1791235735169,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 1068484.1,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1020923.2,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 1094640.7,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 9V45 96-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 699988.6,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 9V74 80-Core Processor                \\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "236483fc270669e06b5b472f997c6e03974f82f5",
          "message": "Merge pull request #471 from LoveDaisy/docs/raypath-case-corpus\n\ndocs(cases): add raypath analysis scenario-case corpus index",
          "timestamp": "2026-10-06T11:07:25+08:00",
          "tree_id": "05675d1c473a536b58ff6c00a95dbda93a7db23c",
          "url": "https://github.com/LoveDaisy/Lumice/commit/236483fc270669e06b5b472f997c6e03974f82f5"
        },
        "date": 1791256190097,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 1132439.9,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1019223.4,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 731512.8,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 960772.5,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 9V45 96-Core Processor                \\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "0c45b96ea97e477e6973097a68acbf4e6e7e3825",
          "message": "Merge pull request #472 from LoveDaisy/fix/outward-child-selfhit-leak\n\nfix(core): classify outward children at birth in CPU/Metal slab traversal",
          "timestamp": "2026-10-06T15:02:49+08:00",
          "tree_id": "5a3cb903bdbd5960d3ed2fddfb7201cfe72a62d2",
          "url": "https://github.com/LoveDaisy/Lumice/commit/0c45b96ea97e477e6973097a68acbf4e6e7e3825"
        },
        "date": 1791270323257,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 1108355.1,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1011522.3,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 729455.2,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 732707.4,
            "unit": "rays/sec",
            "extra": "CPU: INTEL(R) XEON(R) PLATINUM 8573C\\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "148248ccbf3b62682cd8191b787af84268d788d7",
          "message": "Merge pull request #473 from LoveDaisy/docs/corpus-c12-tint-caliber\n\ndocs(corpus): C12 tint multi-caliber + C06 LI closed-form anchor",
          "timestamp": "2026-10-07T17:57:54+08:00",
          "tree_id": "87b8eff64500a3715263acfc8706ee01d3846e00",
          "url": "https://github.com/LoveDaisy/Lumice/commit/148248ccbf3b62682cd8191b787af84268d788d7"
        },
        "date": 1791367469301,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 1043988.6,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1012748.7,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 726449.3,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 578976.3,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor                \\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "215a850397ac9dd42e2eb7b30ebcb657294c11f2",
          "message": "Merge pull request #474 from LoveDaisy/chore/release-4.7.2\n\nchore(release): cut 4.7.2",
          "timestamp": "2026-10-07T18:13:09+08:00",
          "tree_id": "b9ed9fbd7711749ea472f0e75e9c4ad8a572d253",
          "url": "https://github.com/LoveDaisy/Lumice/commit/215a850397ac9dd42e2eb7b30ebcb657294c11f2"
        },
        "date": 1791368219106,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 841842.2,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1012411.4,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 730612.7,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 581714.4,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor                \\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "e6ca36f9b55e42a684caa58bff4213abf37f4cd0",
          "message": "Merge pull request #475 from LoveDaisy/feat/schema3-measure-layer\n\nfeat(raypath): schema3 measure layer (task-661)",
          "timestamp": "2026-10-08T01:15:42+08:00",
          "tree_id": "eecb54e33af022f4804aa681db1b25ae0dc6cdf4",
          "url": "https://github.com/LoveDaisy/Lumice/commit/e6ca36f9b55e42a684caa58bff4213abf37f4cd0"
        },
        "date": 1791393521595,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 882638.3,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1013516.9,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 728975.7,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 636125.4,
            "unit": "rays/sec",
            "extra": "CPU: Intel(R) Xeon(R) Platinum 8370C CPU @ 2.80GHz\\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "ff81cb51f24df08becce17e67dc341e7acd0ddf7",
          "message": "Merge pull request #476 from LoveDaisy/fix/config-silent-noop-guards\n\nfix(config): warn on the two silent no-op config shapes",
          "timestamp": "2026-10-08T07:38:14+08:00",
          "tree_id": "72d56acc7970f3c253fb3fb79133f22677d0d85f",
          "url": "https://github.com/LoveDaisy/Lumice/commit/ff81cb51f24df08becce17e67dc341e7acd0ddf7"
        },
        "date": 1791416515317,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 1142202.9,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1014443.6,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 728944.8,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 582128.8,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor                \\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "1e88e8c405777eac6aa3f56227a2a0ccd28e82b6",
          "message": "Merge pull request #477 from LoveDaisy/feat/schema3-geometry-port\n\nfeat(analytic): schema3 geometry layer port (u-S2 field, partition, walks, focusing/chromatic, contour, API v8)",
          "timestamp": "2026-10-09T03:56:32+08:00",
          "tree_id": "6d823a2cb115de1d1cc95c3511bd06308e497425",
          "url": "https://github.com/LoveDaisy/Lumice/commit/1e88e8c405777eac6aa3f56227a2a0ccd28e82b6"
        },
        "date": 1791489567318,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 916955.3,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1007920,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 729697.1,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 596846.9,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor                \\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "826acd4bd8a4c3757162564f22e74f56e8c39510",
          "message": "Merge pull request #478 from LoveDaisy/feat/schema3-report\n\nfeat(raypath): schema3 report - state machines, support block, unattributed, mc_evidence, schema_version=3",
          "timestamp": "2026-10-10T10:31:01+08:00",
          "tree_id": "6cbc9655451153e80826f2030b8cca8d1547e6bc",
          "url": "https://github.com/LoveDaisy/Lumice/commit/826acd4bd8a4c3757162564f22e74f56e8c39510"
        },
        "date": 1791599688511,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 1111267.8,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1012917.4,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 728964.5,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 584905.8,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor                \\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "a2c5d90a7a3f0918d628d6b49921aad3a0a7c8ed",
          "message": "Merge pull request #479 from LoveDaisy/fix/analyze-render-share-calibration\n\nShare-domain calibration: lens landing semantics + dual fold-boundary defect + cone cross-check tool (task-669)",
          "timestamp": "2026-10-10T18:45:27+08:00",
          "tree_id": "45ab14128501225882d5f360859fce217f5b3a9f",
          "url": "https://github.com/LoveDaisy/Lumice/commit/a2c5d90a7a3f0918d628d6b49921aad3a0a7c8ed"
        },
        "date": 1791629300906,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 1065182.1,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1007331.9,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 850693.8,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 9V74 80-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 630573.3,
            "unit": "rays/sec",
            "extra": "CPU: INTEL(R) XEON(R) PLATINUM 8573C\\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "6e0a5b01d67218589710fdc63aaa96e5c9bab05d",
          "message": "Merge pull request #480 from LoveDaisy/fix/dual-fisheye-fold-boundary-loss\n\nfix(raypath): clamp dual-fisheye fold-boundary pixels into canvas (task-670)",
          "timestamp": "2026-10-10T22:52:34+08:00",
          "tree_id": "9b991e7eb24effad677fcd4328ccfd41633c9d4f",
          "url": "https://github.com/LoveDaisy/Lumice/commit/6e0a5b01d67218589710fdc63aaa96e5c9bab05d"
        },
        "date": 1791644145644,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 1044636.9,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1009527.5,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 726529.6,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 547491.5,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor                \\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "f50e9639b74896229064fc6f302f8fbfa2251e06",
          "message": "Merge pull request #481 from LoveDaisy/chore/mc-corner-tier-contract-sync\n\nClarify mixed-corner tolerance consumer comment",
          "timestamp": "2026-10-11T01:54:48+08:00",
          "tree_id": "485185c3b80de593fe4ee9348baf6c20b9c33bde",
          "url": "https://github.com/LoveDaisy/Lumice/commit/f50e9639b74896229064fc6f302f8fbfa2251e06"
        },
        "date": 1791655084884,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 929868.6,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1009077,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 925137.6,
            "unit": "rays/sec",
            "extra": "CPU: INTEL(R) XEON(R) PLATINUM 8573C\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 582327.9,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor                \\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "b8396281f9127160a6677a9d802dd3b90c6d91d5",
          "message": "Merge pull request #482 from LoveDaisy/chore/ci-raypath-test-cost-governance\n\nci: reduce repeated raypath test work and govern cumulative duration",
          "timestamp": "2026-10-11T06:49:40+08:00",
          "tree_id": "13f1062264c8f6979884be10399307145b272e07",
          "url": "https://github.com/LoveDaisy/Lumice/commit/b8396281f9127160a6677a9d802dd3b90c6d91d5"
        },
        "date": 1791672785940,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 848542.3,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1012020.9,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 839553.5,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 9V74 80-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 579537.4,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor                \\nCores: 4"
          }
        ]
      }
    ],
    "Parallel Efficiency": [
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "f10f34cfcd966500675c24f83142ce2b7267622e",
          "message": "Merge pull request #371 from LoveDaisy/feat/adaptive-allocation-gate-statistics\n\ntest(e2e): judge adaptive allocation on row energy with a Šidák worst-row threshold; keep smoke PSNR failure samples",
          "timestamp": "2026-09-16T09:04:20+08:00",
          "tree_id": "93b698302b5b77fb9b6d221be0c96348ecf0fe95",
          "url": "https://github.com/LoveDaisy/Lumice/commit/f10f34cfcd966500675c24f83142ce2b7267622e"
        },
        "date": 1789521267378,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 71.4,
            "unit": "%"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 100,
            "unit": "%"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 93.6,
            "unit": "%"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 89.3,
            "unit": "%"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "4b269f31e994c541505f45df5390fa783fd5eb4c",
          "message": "Merge pull request #372 from LoveDaisy/feat/multi-renderer-gpu-backend\n\nfeat: device-fused multi-renderer sessions on Metal and CUDA (scrum multi-renderer-gpu-backend)",
          "timestamp": "2026-09-16T14:15:30+08:00",
          "tree_id": "2b95071bd3ce2b50817d54682a83c0363b9638fe",
          "url": "https://github.com/LoveDaisy/Lumice/commit/4b269f31e994c541505f45df5390fa783fd5eb4c"
        },
        "date": 1789540155158,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 82.5,
            "unit": "%"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 99.7,
            "unit": "%"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 95.2,
            "unit": "%"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 88.6,
            "unit": "%"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "eff2ea9db23ead9488cd19184d31ecc1fce4f339",
          "message": "Merge pull request #373 from LoveDaisy/feat/axis-modal-custom-preset-memory\n\nfeat(gui): remember each crystal's last Custom axis triple in the edit modal",
          "timestamp": "2026-09-16T22:02:36+08:00",
          "tree_id": "e82f08c3e0d37f056f07a7e1e0c21781d6a0133a",
          "url": "https://github.com/LoveDaisy/Lumice/commit/eff2ea9db23ead9488cd19184d31ecc1fce4f339"
        },
        "date": 1789568005628,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 85.5,
            "unit": "%"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 99.8,
            "unit": "%"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 93.5,
            "unit": "%"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 87.8,
            "unit": "%"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "f28752902bc36099d973c676533fb29e7e6e1991",
          "message": "Merge pull request #374 from LoveDaisy/feat/cuda-discrete-spectrum-wl-pool-cache\n\nfix(cuda): rebuild the wl pool every BeginSession under a discrete spectrum",
          "timestamp": "2026-09-16T22:34:39+08:00",
          "tree_id": "8e6d0b7a6f8bcb70d49405b289412534a3c5eb49",
          "url": "https://github.com/LoveDaisy/Lumice/commit/f28752902bc36099d973c676533fb29e7e6e1991"
        },
        "date": 1789569952480,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 73.8,
            "unit": "%"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 100.3,
            "unit": "%"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 94.2,
            "unit": "%"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 88.4,
            "unit": "%"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "ab1ba3499938a637c44b1102fef825610033d17e",
          "message": "Merge pull request #375 from LoveDaisy/feat/isa-engine-dll-dispatch\n\nrelease: one shell per entry point + two internal engine libraries picked by CPUID / glibc-hwcaps (scrum-566)",
          "timestamp": "2026-09-17T09:29:18+08:00",
          "tree_id": "95064d4b64e5245da0848c6223ef22c769d2c506",
          "url": "https://github.com/LoveDaisy/Lumice/commit/ab1ba3499938a637c44b1102fef825610033d17e"
        },
        "date": 1789609474415,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 86.3,
            "unit": "%"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 99.7,
            "unit": "%"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 94.6,
            "unit": "%"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 88.5,
            "unit": "%"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "695de9ed0e5a901e96e0c747c5b6235ed051c10d",
          "message": "Merge pull request #377 from LoveDaisy/chore/ci-e2e-slow-macos-rest-timeout\n\nfix(ci): widen E2E Slow (macOS rest) step timeout 15→25 min",
          "timestamp": "2026-09-17T16:56:21+08:00",
          "tree_id": "de8b1ecd633d6f3a4476f875f401bac57c4c5feb",
          "url": "https://github.com/LoveDaisy/Lumice/commit/695de9ed0e5a901e96e0c747c5b6235ed051c10d"
        },
        "date": 1789636081065,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 84.1,
            "unit": "%"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 99.7,
            "unit": "%"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 95.6,
            "unit": "%"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 90,
            "unit": "%"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "444cec705397fdcccafdc87329cb670fe0a4983d",
          "message": "Merge pull request #376 from LoveDaisy/feat/cuda-persistent-device-buffers\n\nperf(cuda): persist lat_lut buffers and make EnsureSessionBuffers grow-only",
          "timestamp": "2026-09-17T17:17:49+08:00",
          "tree_id": "df38477d8c88f2a853e31bcf30cab5d180449a9c",
          "url": "https://github.com/LoveDaisy/Lumice/commit/444cec705397fdcccafdc87329cb670fe0a4983d"
        },
        "date": 1789637240386,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 77.8,
            "unit": "%"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 99.8,
            "unit": "%"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 94.2,
            "unit": "%"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 87.4,
            "unit": "%"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "fbd9e333c05a864d3df8756f7a171910bc9fd37a",
          "message": "Merge pull request #378 from LoveDaisy/feat/cpu-worker-side-projection\n\nperf(core): move legacy-CPU per-ray projection onto the simulator workers",
          "timestamp": "2026-09-18T04:05:22+08:00",
          "tree_id": "14b5ad3b0df1857b2e43b1dde29f1d4fdd79955a",
          "url": "https://github.com/LoveDaisy/Lumice/commit/fbd9e333c05a864d3df8756f7a171910bc9fd37a"
        },
        "date": 1789676304671,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 94.7,
            "unit": "%"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 99.7,
            "unit": "%"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 97.6,
            "unit": "%"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 92,
            "unit": "%"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "f392ee3a91e7a6975dd11b1c7e2867ef8800efa0",
          "message": "Merge pull request #379 from LoveDaisy/feat/cli-raw-float-export\n\nfeat(cli): --format npy raw float32 XYZ export with sidecar metadata",
          "timestamp": "2026-09-19T00:39:06+08:00",
          "tree_id": "7f3a5e880b4c85c0d6f7ca6ecef4f6edd0373fc9",
          "url": "https://github.com/LoveDaisy/Lumice/commit/f392ee3a91e7a6975dd11b1c7e2867ef8800efa0"
        },
        "date": 1789750243085,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 92.4,
            "unit": "%"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 99.7,
            "unit": "%"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 98.4,
            "unit": "%"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 93.8,
            "unit": "%"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "53fa48878ac86e2a3794c6b98f02d20fc12744c0",
          "message": "Merge pull request #380 from LoveDaisy/fix/fp32-accumulator-compensation\n\nfix(server): widen long-chain fp32 accumulators to double (sub-sun hue drift with ray count)",
          "timestamp": "2026-09-19T00:59:38+08:00",
          "tree_id": "f432b8348761a5f45dc462565e3814371088db89",
          "url": "https://github.com/LoveDaisy/Lumice/commit/53fa48878ac86e2a3794c6b98f02d20fc12744c0"
        },
        "date": 1789751530587,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 88.8,
            "unit": "%"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 99.9,
            "unit": "%"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 98.6,
            "unit": "%"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 93.4,
            "unit": "%"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "11cc9ff591a8d177621c226af7d970ad200ad6fb",
          "message": "Merge pull request #381 from LoveDaisy/feat/lmc-linear-xyz-texture\n\nfeat(gui): store the .lmc preview texture as unexposed linear XYZ (format v5)",
          "timestamp": "2026-09-19T02:24:05+08:00",
          "tree_id": "46941acdc2657c651126351d9db7f5e5664612d3",
          "url": "https://github.com/LoveDaisy/Lumice/commit/11cc9ff591a8d177621c226af7d970ad200ad6fb"
        },
        "date": 1789756620521,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 92.8,
            "unit": "%"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 99.8,
            "unit": "%"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 98,
            "unit": "%"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 91.6,
            "unit": "%"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "6f38f1a7758e76516f2e71424805ca2f32020ade",
          "message": "Merge pull request #382 from LoveDaisy/feat/perf-followthrough\n\nperf: follow-through scrum — per-platform worker cap, GUI startup prewarm, CUDA hit-budget fix, PostSnapshot parallelization, measurement discipline",
          "timestamp": "2026-09-19T15:59:56+08:00",
          "tree_id": "6bd42c6a11fb1d48fb9d34ac461583994b914805",
          "url": "https://github.com/LoveDaisy/Lumice/commit/6f38f1a7758e76516f2e71424805ca2f32020ade"
        },
        "date": 1789805608878,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 87.7,
            "unit": "%"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 99.6,
            "unit": "%"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 98.1,
            "unit": "%"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 94,
            "unit": "%"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "96c437719d62599f74bab998e9baa5a4192900a7",
          "message": "Merge pull request #384 from LoveDaisy/feat/lmc-f16-texture-dual-path\n\nfeat(gui): .lmc v6 float16 texture with the live preview quantized through the same codec",
          "timestamp": "2026-09-20T01:45:06+08:00",
          "tree_id": "6294bd9290986f14645da62d4429859ba9129d91",
          "url": "https://github.com/LoveDaisy/Lumice/commit/96c437719d62599f74bab998e9baa5a4192900a7"
        },
        "date": 1789840615338,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 90.3,
            "unit": "%"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 99.8,
            "unit": "%"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 98,
            "unit": "%"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 92,
            "unit": "%"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "07d02b311b0c719947d1ddb96023a365f573e2c7",
          "message": "Merge pull request #385 from LoveDaisy/chore/cuda-canonical-throughput-5090\n\ndocs(perf): first RTX 5090 D canonical throughput columns (home-wsl / home-win) + B/E drain-plane cost matrix",
          "timestamp": "2026-09-20T03:29:12+08:00",
          "tree_id": "42eb1aecd5d088d6fafd75cc593704374199bd52",
          "url": "https://github.com/LoveDaisy/Lumice/commit/07d02b311b0c719947d1ddb96023a365f573e2c7"
        },
        "date": 1789846790064,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 77.2,
            "unit": "%"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 99.9,
            "unit": "%"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 99.3,
            "unit": "%"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 91.7,
            "unit": "%"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "76be31ac78abe09f12455eb5916a081fbac528cf",
          "message": "Merge pull request #386 from LoveDaisy/feat/parallel-rows-idle-core-budget\n\nfix(server): size ParallelRows by an explicit idle-core thread budget (undo the GUI-poll worker regression from #382)",
          "timestamp": "2026-09-20T05:54:03+08:00",
          "tree_id": "622c8e9b60a4128f396e320415097e083aab2610",
          "url": "https://github.com/LoveDaisy/Lumice/commit/76be31ac78abe09f12455eb5916a081fbac528cf"
        },
        "date": 1789855511483,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "Ubuntu ARM64",
            "value": 99.8,
            "unit": "%"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 98.2,
            "unit": "%"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 87.6,
            "unit": "%"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "88a667fb94867eaa45b47b789e101b9b505a99ee",
          "message": "Merge pull request #387 from LoveDaisy/docs/thread-budget-owner-ruling\n\ndocs(gui): record the owner's ruling on the capped render thread budget",
          "timestamp": "2026-09-20T07:45:28+08:00",
          "tree_id": "659f93e361c41c0c2e674a5a3bba6178131d04b4",
          "url": "https://github.com/LoveDaisy/Lumice/commit/88a667fb94867eaa45b47b789e101b9b505a99ee"
        },
        "date": 1789862106601,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "Ubuntu ARM64",
            "value": 99.6,
            "unit": "%"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 96.4,
            "unit": "%"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 91.7,
            "unit": "%"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "61b01911736670be1cb86ee587fc006d12b7eefd",
          "message": "Merge pull request #383 from LoveDaisy/fix/cuda-drain-window-fp32-plane\n\nfix(cuda): fold the fp32 XYZ device plane into a double plane every 8 batches (drain-window ledger drift)",
          "timestamp": "2026-09-20T09:41:39+08:00",
          "tree_id": "7251f6578c75bb2a709a992bf46e1bc045a57fe2",
          "url": "https://github.com/LoveDaisy/Lumice/commit/61b01911736670be1cb86ee587fc006d12b7eefd"
        },
        "date": 1789869289289,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "Ubuntu ARM64",
            "value": 99.8,
            "unit": "%"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 98.7,
            "unit": "%"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 91.4,
            "unit": "%"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "747b2ec2a60ffe5e75331b2c0b3753ed05411959",
          "message": "Merge pull request #388 from LoveDaisy/fix/startup-calibration-test-address-reuse\n\ntest(gui): evidence the startup-calibration server rebuild by its worker-count tracker, not by address",
          "timestamp": "2026-09-20T10:28:39+08:00",
          "tree_id": "1b37adad964db6f5ecb871687f0eeb0ee499b8f3",
          "url": "https://github.com/LoveDaisy/Lumice/commit/747b2ec2a60ffe5e75331b2c0b3753ed05411959"
        },
        "date": 1789871950707,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 91.2,
            "unit": "%"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 100.2,
            "unit": "%"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 101.4,
            "unit": "%"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 85.5,
            "unit": "%"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "94b90ab6c623e1aa3679f7aa1718a7cd0730b4fb",
          "message": "Merge pull request #389 from LoveDaisy/task/gui-view-copy-resync-mechanism\n\nfix(gui): resync the edit modal from the pool every frame — Exclude no longer overwritten while the editor is open",
          "timestamp": "2026-09-20T16:23:59+08:00",
          "tree_id": "be5ece68186e20452d6b1ff247e8d4d66e216b7c",
          "url": "https://github.com/LoveDaisy/Lumice/commit/94b90ab6c623e1aa3679f7aa1718a7cd0730b4fb"
        },
        "date": 1789893326824,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 90.1,
            "unit": "%"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 99.7,
            "unit": "%"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 97.6,
            "unit": "%"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 91.3,
            "unit": "%"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "337ad7c9b1160379b054d8d652114eb883ba7c36",
          "message": "Merge pull request #390 from LoveDaisy/fix/composite-preview-p99-flake-margin\n\ntest(gui): drop the composite re-run p99 ratio that never saw the defect (task-582)",
          "timestamp": "2026-09-20T19:39:18+08:00",
          "tree_id": "dc6df54e496534e5ec18465303203dbf62ed18fd",
          "url": "https://github.com/LoveDaisy/Lumice/commit/337ad7c9b1160379b054d8d652114eb883ba7c36"
        },
        "date": 1789904919462,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 80.7,
            "unit": "%"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 99.7,
            "unit": "%"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 97,
            "unit": "%"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 88.5,
            "unit": "%"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "048801761115ac2403b66262d5f987c2146ca3b1",
          "message": "Merge pull request #391 from LoveDaisy/feat/show-product-version\n\nfeat: show the product version — C API, title bar, --version, startup log, .lmc (task-585)",
          "timestamp": "2026-09-20T20:10:01+08:00",
          "tree_id": "93e17fbc3606c3e3386802a0e7be910d196ab10f",
          "url": "https://github.com/LoveDaisy/Lumice/commit/048801761115ac2403b66262d5f987c2146ca3b1"
        },
        "date": 1789907141668,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 94,
            "unit": "%"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 99.7,
            "unit": "%"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 93.9,
            "unit": "%"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "a09b72909d5b03ab532b9f908e01ac4ef233c4ac",
          "message": "Merge pull request #392 from LoveDaisy/chore/test-hygiene-sweep\n\nchore: test hygiene sweep — shared LogCapture, static_assert, wire-table test, guard hardening (chore-584)",
          "timestamp": "2026-09-20T20:25:08+08:00",
          "tree_id": "06eb85faf160e5faed09fee676e77bd65d583e70",
          "url": "https://github.com/LoveDaisy/Lumice/commit/a09b72909d5b03ab532b9f908e01ac4ef233c4ac"
        },
        "date": 1789907765942,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 71.6,
            "unit": "%"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 99.8,
            "unit": "%"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "distinct": true,
          "id": "81e8d18a17511e66af4f6e17ce5e75a7c7996daf",
          "message": "Merge pull request #392 from LoveDaisy/chore/test-hygiene-sweep\n\nchore: test hygiene sweep — shared LogCapture, static_assert, wire-table test, guard hardening (chore-584)",
          "timestamp": "2026-09-20T20:34:42+08:00",
          "tree_id": "06eb85faf160e5faed09fee676e77bd65d583e70",
          "url": "https://github.com/LoveDaisy/Lumice/commit/81e8d18a17511e66af4f6e17ce5e75a7c7996daf"
        },
        "date": 1789908529926,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 92,
            "unit": "%"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 98.7,
            "unit": "%"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 91.8,
            "unit": "%"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "3442752646d7a6d1d1e7c71045a958be19025e4f",
          "message": "Merge pull request #393 from LoveDaisy/feat/look-at-horizon-series\n\nfeat(gui): Look At — Horizon series of four sun-relative level bearings (task-586)",
          "timestamp": "2026-09-20T21:18:38+08:00",
          "tree_id": "2ea46a896126f59b8ad9e871c17836f0b16a9f3b",
          "url": "https://github.com/LoveDaisy/Lumice/commit/3442752646d7a6d1d1e7c71045a958be19025e4f"
        },
        "date": 1789910921597,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 101,
            "unit": "%"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 99.6,
            "unit": "%"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 98.5,
            "unit": "%"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 92.2,
            "unit": "%"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "a9eb84bee60644a8d4838c4051be47e0b5cda60a",
          "message": "Merge pull request #394 from LoveDaisy/feat/analyze-chain-table-capacity\n\nfeat(analyze): runtime chain-record capacity — --chain-capacity / C API chain_capacity (task-583)",
          "timestamp": "2026-09-20T21:46:35+08:00",
          "tree_id": "f4e0ca5a93ff255549dadfb9d9ff5d2fb530ffac",
          "url": "https://github.com/LoveDaisy/Lumice/commit/a9eb84bee60644a8d4838c4051be47e0b5cda60a"
        },
        "date": 1789912855362,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 93.2,
            "unit": "%"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 99.7,
            "unit": "%"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 98.4,
            "unit": "%"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 91.3,
            "unit": "%"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "15a0baf562499726ca8be28b5ac5780a16af813f",
          "message": "Merge pull request #395 from LoveDaisy/feat/axis-preset-type-override\n\nfeat(gui): preset library — zenith type is editable within each preset's accepted set (task-587)",
          "timestamp": "2026-09-20T22:53:50+08:00",
          "tree_id": "49dabf2ddaaebcc7d0fc14e6aa16353b02010817",
          "url": "https://github.com/LoveDaisy/Lumice/commit/15a0baf562499726ca8be28b5ac5780a16af813f"
        },
        "date": 1789916691254,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 94,
            "unit": "%"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 99.6,
            "unit": "%"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 98,
            "unit": "%"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 92.7,
            "unit": "%"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "1bd37a62468be8c031b66851d29f8687526ad678",
          "message": "Merge pull request #396 from LoveDaisy/feat/config-summary-window\n\nfeat(gui): read-only Summary window — one page of the current configuration for sharing (task-588)",
          "timestamp": "2026-09-21T11:06:18+08:00",
          "tree_id": "a62e92733c5372ed1d234f4bdc7e07e66441a886",
          "url": "https://github.com/LoveDaisy/Lumice/commit/1bd37a62468be8c031b66851d29f8687526ad678"
        },
        "date": 1789960739607,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 70.7,
            "unit": "%"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 99.5,
            "unit": "%"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 97.9,
            "unit": "%"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 92.8,
            "unit": "%"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "f70cdd2b7bddb782f36f438d7742e112b90ce3fc",
          "message": "Merge pull request #397 from LoveDaisy/chore/release-4.6.1\n\nchore(release): cut 4.6.1 — changelog backfill for #368–#396",
          "timestamp": "2026-09-21T12:10:01+08:00",
          "tree_id": "7a67bfaac331a8f7aecdd57d6f9a15673d4db69b",
          "url": "https://github.com/LoveDaisy/Lumice/commit/f70cdd2b7bddb782f36f438d7742e112b90ce3fc"
        },
        "date": 1789964427038,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 99.3,
            "unit": "%"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 99.5,
            "unit": "%"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 96.4,
            "unit": "%"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 91,
            "unit": "%"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "64706c37cd5ad912eb76cde70881bb84e9efd681",
          "message": "Merge pull request #398 from LoveDaisy/feat/ui-scale\n\nfeat(gui): ui_scale — DPI-aware layout and font, plus a user UI-scale preference",
          "timestamp": "2026-09-22T17:25:47+08:00",
          "tree_id": "282afffeb45deeca9dcf5eb0ea192084f3466648",
          "url": "https://github.com/LoveDaisy/Lumice/commit/64706c37cd5ad912eb76cde70881bb84e9efd681"
        },
        "date": 1790069885550,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 93.6,
            "unit": "%"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 99.9,
            "unit": "%"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 98.5,
            "unit": "%"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 91.9,
            "unit": "%"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "4d4b78fc4e2fdbd6c38693489ab74498e4d39df0",
          "message": "Merge pull request #399 from LoveDaisy/feat/gui-desktop-conventions-part1\n\nfeat(gui): desktop conventions — deletable last filter row, list search, copyable text, column-major Tab, window sizing policy",
          "timestamp": "2026-09-22T19:41:21+08:00",
          "tree_id": "adccab168ff45ce7559ae9dfb6e96e62e09b4a78",
          "url": "https://github.com/LoveDaisy/Lumice/commit/4d4b78fc4e2fdbd6c38693489ab74498e4d39df0"
        },
        "date": 1790078008990,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 102.8,
            "unit": "%"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 99.9,
            "unit": "%"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 99,
            "unit": "%"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 95,
            "unit": "%"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "d69f9167201076266c84c99adb68dcf68ac437b1",
          "message": "Merge pull request #400 from LoveDaisy/feat/analysis-exclusion-view\n\nfeat(gui): the analysis list keeps excluded raypaths as greyed rows, with Include again",
          "timestamp": "2026-09-22T20:10:32+08:00",
          "tree_id": "f5c55b74bbe2f5ac5434e9ac3850cd90089af128",
          "url": "https://github.com/LoveDaisy/Lumice/commit/d69f9167201076266c84c99adb68dcf68ac437b1"
        },
        "date": 1790079817612,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 88.9,
            "unit": "%"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 99.7,
            "unit": "%"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 98.1,
            "unit": "%"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 92,
            "unit": "%"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "c49ef3f32f9385ae0700b887e5f73faaedd7bea4",
          "message": "Merge pull request #401 from LoveDaisy/feat/gui-desktop-conventions\n\nrefactor(gui): the edit modal's two shapes — Compact and Expanded",
          "timestamp": "2026-09-22T22:24:18+08:00",
          "tree_id": "f1e06a712c5d8c584284204571457aa629c4d097",
          "url": "https://github.com/LoveDaisy/Lumice/commit/c49ef3f32f9385ae0700b887e5f73faaedd7bea4"
        },
        "date": 1790088029954,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 92.4,
            "unit": "%"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 99.7,
            "unit": "%"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 98.6,
            "unit": "%"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 90.7,
            "unit": "%"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "c3d3aa1d21c41277d8cf6bc76016d7067cc2448b",
          "message": "Merge pull request #402 from LoveDaisy/feat/summary-reference-version-independence\n\ntest(gui): pin the Summary page's version under gui_test so a release stops reddening its references",
          "timestamp": "2026-09-23T00:07:08+08:00",
          "tree_id": "e22276ed331178ee1e226e97b579203a031ed8ba",
          "url": "https://github.com/LoveDaisy/Lumice/commit/c3d3aa1d21c41277d8cf6bc76016d7067cc2448b"
        },
        "date": 1790093879752,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 91.3,
            "unit": "%"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 99.6,
            "unit": "%"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 97.7,
            "unit": "%"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 91.5,
            "unit": "%"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "b6ed96be5d6acdd7acc63a500e5c045647525ba5",
          "message": "Merge pull request #403 from LoveDaisy/feat/edit-modal-height-and-expanded-polish\n\nfeat(gui): Edit Entry — a draggable height, aligned Expanded headers, standard headings",
          "timestamp": "2026-09-23T09:50:00+08:00",
          "tree_id": "4bd7771eb2b058baf5a35532f33a8b6313a2718b",
          "url": "https://github.com/LoveDaisy/Lumice/commit/b6ed96be5d6acdd7acc63a500e5c045647525ba5"
        },
        "date": 1790128919804,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 89.3,
            "unit": "%"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 99.8,
            "unit": "%"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 97.3,
            "unit": "%"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 89.2,
            "unit": "%"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "2056f69990d8a0df13c199c795260b717c144f29",
          "message": "Merge pull request #404 from LoveDaisy/feat/crystal-projected-area-weighting\n\nfix(core): weight crystal entry by projected area on every backend",
          "timestamp": "2026-09-24T19:58:42+08:00",
          "tree_id": "b0cd16714314f8b35d9687520070dca01b0a5efd",
          "url": "https://github.com/LoveDaisy/Lumice/commit/2056f69990d8a0df13c199c795260b717c144f29"
        },
        "date": 1790251836558,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 90.7,
            "unit": "%"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 100.2,
            "unit": "%"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 99,
            "unit": "%"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 92.4,
            "unit": "%"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "108fdd825206a218a0262cfb6533db39965fb3e0",
          "message": "Merge pull request #405 from LoveDaisy/feat/card-insert-and-reorder\n\nfeat(gui): duplicate lands below the source card; drag to reorder cards",
          "timestamp": "2026-09-25T12:52:34+08:00",
          "tree_id": "e30f40ec89cb02353708deba953b4ee18ee297fe",
          "url": "https://github.com/LoveDaisy/Lumice/commit/108fdd825206a218a0262cfb6533db39965fb3e0"
        },
        "date": 1790312916906,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 96.2,
            "unit": "%"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 99.8,
            "unit": "%"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 96.2,
            "unit": "%"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 93.5,
            "unit": "%"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "65514f1a79aafa4a7d5bf1c468123ac24550aa41",
          "message": "Merge pull request #406 from LoveDaisy/feat/sim-continue\n\nfeat: Continue adds rays to a finished render (LUMICE_ContinueRender)",
          "timestamp": "2026-09-25T13:25:12+08:00",
          "tree_id": "86a13a44187986c4d27a4fea1b98d72c471e573d",
          "url": "https://github.com/LoveDaisy/Lumice/commit/65514f1a79aafa4a7d5bf1c468123ac24550aa41"
        },
        "date": 1790314923420,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 91.4,
            "unit": "%"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 99.6,
            "unit": "%"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 98.9,
            "unit": "%"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 92.5,
            "unit": "%"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "69b1b656449030f1e93e69f7bd842037f16a8bdc",
          "message": "Merge pull request #407 from LoveDaisy/feat/globe-backside-fog-fade\n\nfeat: globe back-side fade (the far side shows through, fading like fog)",
          "timestamp": "2026-09-25T14:30:07+08:00",
          "tree_id": "e4f340177152a07e767413c1203db933158f3ac0",
          "url": "https://github.com/LoveDaisy/Lumice/commit/69b1b656449030f1e93e69f7bd842037f16a8bdc"
        },
        "date": 1790318817628,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 115.2,
            "unit": "%"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 99.6,
            "unit": "%"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 99.2,
            "unit": "%"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 92.7,
            "unit": "%"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "df90ad63b51304ed6867f9285e263e304d346c39",
          "message": "Merge pull request #408 from LoveDaisy/fix/analysis-manual-stop-flake\n\nfix(test): wait for a cone-sized sample before the manual-stop analysis assertion",
          "timestamp": "2026-09-25T15:19:07+08:00",
          "tree_id": "72655d66e9061682075555c399d83140dc3216f5",
          "url": "https://github.com/LoveDaisy/Lumice/commit/df90ad63b51304ed6867f9285e263e304d346c39"
        },
        "date": 1790321720884,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 99,
            "unit": "%"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 99.9,
            "unit": "%"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 98.1,
            "unit": "%"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 93.5,
            "unit": "%"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "5c29ec370e9e6fcae68806e5d82c40e8df5359a2",
          "message": "Merge pull request #409 from LoveDaisy/fix/continue-render-plane-total-flake\n\nfix(test): calibrate ContinueRender's plane-total band to per-batch wavelength noise",
          "timestamp": "2026-09-25T15:58:37+08:00",
          "tree_id": "616829b9a9cb568528885cd75ef28c022146191b",
          "url": "https://github.com/LoveDaisy/Lumice/commit/5c29ec370e9e6fcae68806e5d82c40e8df5359a2"
        },
        "date": 1790324479879,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "Ubuntu ARM64",
            "value": 99.6,
            "unit": "%"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 98.1,
            "unit": "%"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 92,
            "unit": "%"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "a2d088f53ed6ac75eeac825ca6f5ec003084e6cd",
          "message": "Merge pull request #410 from LoveDaisy/feat/channel-math-display-mode\n\nfeat: Channel B−R display mode (is this spot bluer or redder?)",
          "timestamp": "2026-09-25T16:20:42+08:00",
          "tree_id": "9492740c7c72869d21a8148f8567070eaca19d7b",
          "url": "https://github.com/LoveDaisy/Lumice/commit/a2d088f53ed6ac75eeac825ca6f5ec003084e6cd"
        },
        "date": 1790325344907,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 93.7,
            "unit": "%"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 99.8,
            "unit": "%"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 99.3,
            "unit": "%"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 94,
            "unit": "%"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "49fe5e10fab9bfff09a5c101fbf651f8f283af5b",
          "message": "Merge pull request #411 from LoveDaisy/fix/globe-back-fade-fma-exact-eq\n\nfix(test): compare the globe back-fade weight within an absolute tolerance",
          "timestamp": "2026-09-25T16:38:58+08:00",
          "tree_id": "1ad240095c7e9a80ab3a0f9f3c9ec1c87f9c4465",
          "url": "https://github.com/LoveDaisy/Lumice/commit/49fe5e10fab9bfff09a5c101fbf651f8f283af5b"
        },
        "date": 1790326333719,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 86.3,
            "unit": "%"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 99.8,
            "unit": "%"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 98.3,
            "unit": "%"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 92.8,
            "unit": "%"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "69d8136c8f926571410a99548a5e7374bf7f0ab2",
          "message": "Merge pull request #412 from LoveDaisy/perf/cpu-worker-batch-sync-cliff\n\nperf(server): decouple the CPU queue handoff from the 128-ray physics batch",
          "timestamp": "2026-09-25T20:57:22+08:00",
          "tree_id": "09c4693994679e1f4660fbb9b8892f3e4bb629e4",
          "url": "https://github.com/LoveDaisy/Lumice/commit/69d8136c8f926571410a99548a5e7374bf7f0ab2"
        },
        "date": 1790341826720,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 92,
            "unit": "%"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 100.1,
            "unit": "%"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 98.5,
            "unit": "%"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 95.4,
            "unit": "%"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "3c9730817a17b3c99d8469c7cfc7f9a7ceee6e38",
          "message": "Merge pull request #413 from LoveDaisy/docs/raypath-analysis-overview\n\ndocs(raypath-analysis): feature overview and roadmap",
          "timestamp": "2026-09-25T22:51:44+08:00",
          "tree_id": "c79b7399ed96b48eaf32a502f0cc41512296376e",
          "url": "https://github.com/LoveDaisy/Lumice/commit/3c9730817a17b3c99d8469c7cfc7f9a7ceee6e38"
        },
        "date": 1790348573464,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 95.9,
            "unit": "%"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 99.6,
            "unit": "%"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 98.7,
            "unit": "%"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 93.7,
            "unit": "%"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "6a148bbcba23d5e330a528600062b88fbe62aae5",
          "message": "Merge pull request #414 from LoveDaisy/perf/cpu-per-ray-wavelength\n\nperf(core): stratify the CPU illuminant wavelength across physics batches",
          "timestamp": "2026-09-25T23:31:46+08:00",
          "tree_id": "b0ecff07f1973fc2a59863cf12726b470566f04c",
          "url": "https://github.com/LoveDaisy/Lumice/commit/6a148bbcba23d5e330a528600062b88fbe62aae5"
        },
        "date": 1790351103080,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 85,
            "unit": "%"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 100.2,
            "unit": "%"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 98.8,
            "unit": "%"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 92,
            "unit": "%"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "48679991416540658f3039cdbde51520d6375e79",
          "message": "Merge pull request #415 from LoveDaisy/feat/gui-front-effective-value\n\nfix(gui): read the effective front clip under lenses it does not apply to",
          "timestamp": "2026-09-26T01:08:03+08:00",
          "tree_id": "9f96f5fa48eafea0128b3d270b751019093977c1",
          "url": "https://github.com/LoveDaisy/Lumice/commit/48679991416540658f3039cdbde51520d6375e79"
        },
        "date": 1790356920739,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 89.4,
            "unit": "%"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 100,
            "unit": "%"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 99,
            "unit": "%"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 95,
            "unit": "%"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "890b32d27ca52132e0f8567199aa38f079614db9",
          "message": "Merge pull request #416 from LoveDaisy/feat/globe-back-fade-expfog-and-grid\n\nfeat: globe back-side fade becomes exponential fog, and far-side lines fade with it",
          "timestamp": "2026-09-26T01:37:37+08:00",
          "tree_id": "f71d2148439000301d8f261d490aa085c8114a70",
          "url": "https://github.com/LoveDaisy/Lumice/commit/890b32d27ca52132e0f8567199aa38f079614db9"
        },
        "date": 1790358581273,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 103.3,
            "unit": "%"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 99.7,
            "unit": "%"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 98.7,
            "unit": "%"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 94.7,
            "unit": "%"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "b5a1a477c67196240aa5db0dd933b77760fa2182",
          "message": "Merge pull request #417 from LoveDaisy/feat/toolbar-button-sizing-continue-color\n\nfeat(gui): group the top bar and give Continue its own colour",
          "timestamp": "2026-09-26T02:20:15+08:00",
          "tree_id": "341993077498e5caf3df7edb8433227b484ffc25",
          "url": "https://github.com/LoveDaisy/Lumice/commit/b5a1a477c67196240aa5db0dd933b77760fa2182"
        },
        "date": 1790361086700,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 111,
            "unit": "%"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 100.2,
            "unit": "%"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 99.5,
            "unit": "%"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 94.6,
            "unit": "%"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "c6d32d110ca90680080935d49bda6d00274d6ac7",
          "message": "Merge pull request #418 from LoveDaisy/feat/axis-preset-highlight-summary\n\nfeat(gui): highlight the active axis preset and summarize the crystal under the edit-modal preview",
          "timestamp": "2026-09-26T03:24:31+08:00",
          "tree_id": "0c4591c98e7a93246c6b7ac5869eb931880bb594",
          "url": "https://github.com/LoveDaisy/Lumice/commit/c6d32d110ca90680080935d49bda6d00274d6ac7"
        },
        "date": 1790365022412,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 88.5,
            "unit": "%"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 99.9,
            "unit": "%"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 98.9,
            "unit": "%"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 95,
            "unit": "%"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "3493b66dd3bb005b0ece61229caadab5c3c3fd09",
          "message": "Merge pull request #419 from LoveDaisy/feat/display-mode-viewport-control\n\nfeat(gui): display mode becomes a segmented control in the preview's corner",
          "timestamp": "2026-09-26T03:55:16+08:00",
          "tree_id": "97ddaa25b9147c1d43eec4ed3cd226d46d8195bb",
          "url": "https://github.com/LoveDaisy/Lumice/commit/3493b66dd3bb005b0ece61229caadab5c3c3fd09"
        },
        "date": 1790366940585,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 94.5,
            "unit": "%"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 100.2,
            "unit": "%"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 99.1,
            "unit": "%"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 92.9,
            "unit": "%"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "208ad64e99c9a72de9042b4c4b28db4829cca387",
          "message": "Merge pull request #420 from LoveDaisy/feat/toolbar-revert-slot-gap\n\nfeat(gui): move the top bar's hidden Revert slot to the trailing end",
          "timestamp": "2026-09-26T05:03:32+08:00",
          "tree_id": "cad7a53a552485d156dcaa7b57ac91e01b2e2f7d",
          "url": "https://github.com/LoveDaisy/Lumice/commit/208ad64e99c9a72de9042b4c4b28db4829cca387"
        },
        "date": 1790370996805,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 90.2,
            "unit": "%"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 99.9,
            "unit": "%"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 98.9,
            "unit": "%"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 95,
            "unit": "%"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "e12c699d7262aa07039ba1d5aa5715e65f78bf5c",
          "message": "Merge pull request #421 from LoveDaisy/feat/angular-dist-picker\n\nfeat(gui): pick an angular-distance ring on the preview",
          "timestamp": "2026-09-26T05:43:54+08:00",
          "tree_id": "2bc14239d813e7ec2ceecf67e4adb4be66e5ba9f",
          "url": "https://github.com/LoveDaisy/Lumice/commit/e12c699d7262aa07039ba1d5aa5715e65f78bf5c"
        },
        "date": 1790373466375,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 77.1,
            "unit": "%"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 100.3,
            "unit": "%"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 97.9,
            "unit": "%"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 94.6,
            "unit": "%"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "b0b090813c2e811b472eb839916a357272554a9c",
          "message": "Merge pull request #422 from LoveDaisy/feat/screenshot-export-options\n\nfeat(gui): screenshot export options popup, subtract-only per family",
          "timestamp": "2026-09-26T06:04:43+08:00",
          "tree_id": "387bf188ef1a8b8098005bded4fd012477ff7205",
          "url": "https://github.com/LoveDaisy/Lumice/commit/b0b090813c2e811b472eb839916a357272554a9c"
        },
        "date": 1790374714142,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 82,
            "unit": "%"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 99.8,
            "unit": "%"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 99.2,
            "unit": "%"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 95.1,
            "unit": "%"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "169a01b75ac4932ed78b13aefb385e4e6087b765",
          "message": "Merge pull request #423 from LoveDaisy/fix/adaptive-alloc-c1-bias-probe\n\ntest(e2e): confirm an adaptive-vs-proportional mean red on 60 more sessions per arm",
          "timestamp": "2026-09-26T08:31:46+08:00",
          "tree_id": "42c5899e6e3d28b93caca02621779566a7678c6a",
          "url": "https://github.com/LoveDaisy/Lumice/commit/169a01b75ac4932ed78b13aefb385e4e6087b765"
        },
        "date": 1790383356774,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 95.2,
            "unit": "%"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 100.1,
            "unit": "%"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 99,
            "unit": "%"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 93.5,
            "unit": "%"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "f185f3da689e6f99f72572f6b6198659524a0cfa",
          "message": "Merge pull request #424 from LoveDaisy/fix/globe-far-side-own-visibility\n\nfix: clip each side of the globe by its own direction's visibility",
          "timestamp": "2026-09-26T11:01:44+08:00",
          "tree_id": "1d3bdfa97e8e41f70adb6f2c18d2203221bfa8e4",
          "url": "https://github.com/LoveDaisy/Lumice/commit/f185f3da689e6f99f72572f6b6198659524a0cfa"
        },
        "date": 1790392431725,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 95.2,
            "unit": "%"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 99.8,
            "unit": "%"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 98.2,
            "unit": "%"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 91.9,
            "unit": "%"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "e4adc5bd4c991f32ec7d292e06eaeb6773249573",
          "message": "Merge pull request #425 from LoveDaisy/chore/release-4.7.0\n\nchore(release): cut 4.7.0",
          "timestamp": "2026-09-26T11:42:17+08:00",
          "tree_id": "31eb8c69a51d64a2fd2f696314ecf1251915e92e",
          "url": "https://github.com/LoveDaisy/Lumice/commit/e4adc5bd4c991f32ec7d292e06eaeb6773249573"
        },
        "date": 1790394806245,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 95.1,
            "unit": "%"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 100,
            "unit": "%"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 100,
            "unit": "%"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 93.2,
            "unit": "%"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "7b107c5623d51c5578890e40cfa4b2a1e2545b02",
          "message": "Merge pull request #426 from LoveDaisy/chore/raypath-analysis-lumice-integral-plan\n\ndocs(raypath-analysis): expand §5.1 into the post-selection three-feature plan",
          "timestamp": "2026-09-27T12:04:30+08:00",
          "tree_id": "22a4d5cac931dca24cc45e2267cdd77444575d99",
          "url": "https://github.com/LoveDaisy/Lumice/commit/7b107c5623d51c5578890e40cfa4b2a1e2545b02"
        },
        "date": 1790482585622,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 93.1,
            "unit": "%"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 100.1,
            "unit": "%"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 98.6,
            "unit": "%"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 95.3,
            "unit": "%"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "bbe3d1883d9873540b5709ec8aa616be79897b36",
          "message": "Merge pull request #427 from LoveDaisy/chore/raypath-analysis-compute-timing\n\ndocs(raypath-analysis): compute location moves per module maturity",
          "timestamp": "2026-09-27T13:51:45+08:00",
          "tree_id": "f291db49905b10460d4a5110faf832aca1be33b0",
          "url": "https://github.com/LoveDaisy/Lumice/commit/bbe3d1883d9873540b5709ec8aa616be79897b36"
        },
        "date": 1790489020581,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 92.3,
            "unit": "%"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 100,
            "unit": "%"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 98.1,
            "unit": "%"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 90.1,
            "unit": "%"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "cd82b68f1c2ab9e12e9b4f3b56f752998e9e3f34",
          "message": "Merge pull request #428 from LoveDaisy/chore/analyze-product-form-docs\n\ndocs(raypath-analysis): Analyze as the second product core (§5.1.8)",
          "timestamp": "2026-09-27T14:38:00+08:00",
          "tree_id": "79d92ce67a0cff0ef0251d949186ab2eade8ef5c",
          "url": "https://github.com/LoveDaisy/Lumice/commit/cd82b68f1c2ab9e12e9b4f3b56f752998e9e3f34"
        },
        "date": 1790491717078,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 94.5,
            "unit": "%"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 100.2,
            "unit": "%"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 98.1,
            "unit": "%"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 96,
            "unit": "%"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "89c8c1cc6f2798f0c7ef0ea8a8c5f89d7bda19ec",
          "message": "Merge pull request #429 from LoveDaisy/task/raypath-reduce-period-from-geometry\n\nfix(core): reduce raypaths only under the symmetry the crystal shape admits",
          "timestamp": "2026-09-27T17:35:49+08:00",
          "tree_id": "e56285d05d6ce1cec96e104506cb4b053d292439",
          "url": "https://github.com/LoveDaisy/Lumice/commit/89c8c1cc6f2798f0c7ef0ea8a8c5f89d7bda19ec"
        },
        "date": 1790502706783,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 92.3,
            "unit": "%"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 99.9,
            "unit": "%"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 99.1,
            "unit": "%"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 94,
            "unit": "%"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "7b889f79db8568517726cf7bd294d13c757a8974",
          "message": "Merge pull request #430 from LoveDaisy/task/raypath-pb-ensemble-applicability\n\nfix(core): apply P/B raypath symmetry only where the orientation ensemble admits it",
          "timestamp": "2026-09-27T20:03:03+08:00",
          "tree_id": "66c883c3d1bbf83bbd049a58b95f6e6c004751b1",
          "url": "https://github.com/LoveDaisy/Lumice/commit/7b889f79db8568517726cf7bd294d13c757a8974"
        },
        "date": 1790511583737,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 82.1,
            "unit": "%"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 100.1,
            "unit": "%"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 98.8,
            "unit": "%"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 93.7,
            "unit": "%"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "ac0fbdf294cc91312b0c421c64b99e05db8828fb",
          "message": "Merge pull request #431 from LoveDaisy/task/gpu-reduce-follows-cpu-symmetry\n\nfix(core): device filter reduction follows the crystal's shape and ensemble symmetry",
          "timestamp": "2026-09-27T21:42:14+08:00",
          "tree_id": "900e6e02f3b3e8425e7b98cde0722045fca77d1c",
          "url": "https://github.com/LoveDaisy/Lumice/commit/ac0fbdf294cc91312b0c421c64b99e05db8828fb"
        },
        "date": 1790517276860,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 97.5,
            "unit": "%"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 99.6,
            "unit": "%"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 99.3,
            "unit": "%"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 95.9,
            "unit": "%"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "b845c83f0138957e21f6ba7e7bcf5c1f42a0dfd1",
          "message": "Merge pull request #432 from LoveDaisy/chore/analyze-workspace-layout-docs\n\ndocs: finalize the Analyze workspace layout (raypath-analysis §5.1.8)",
          "timestamp": "2026-09-28T00:31:26+08:00",
          "tree_id": "26b84bc5a84d9c0089605c7b002cce526807c507",
          "url": "https://github.com/LoveDaisy/Lumice/commit/b845c83f0138957e21f6ba7e7bcf5c1f42a0dfd1"
        },
        "date": 1790527393261,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 86,
            "unit": "%"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 100,
            "unit": "%"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 99.3,
            "unit": "%"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 93.9,
            "unit": "%"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "25b83e949546f15f97e44007554cd6d5a4ddd1e3",
          "message": "Merge pull request #433 from LoveDaisy/chore/repo-roles-and-shared-lib-docs\n\ndocs: repository roles for Lumice / Lumice Integral, and the shared library's first consumer",
          "timestamp": "2026-09-28T01:28:05+08:00",
          "tree_id": "035c8e3864a2fee8909f0af50fb76aaff2835c1a",
          "url": "https://github.com/LoveDaisy/Lumice/commit/25b83e949546f15f97e44007554cd6d5a4ddd1e3"
        },
        "date": 1790530893275,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 85.5,
            "unit": "%"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 100.1,
            "unit": "%"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 98.7,
            "unit": "%"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 100.4,
            "unit": "%"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "26ab6a18e6455817e83e95ad76594539cffc24d6",
          "message": "Merge pull request #434 from LoveDaisy/chore/ray-allocation-settings-combo\n\ngui: sim.ray_allocation Settings editor becomes an adaptive/proportional combo",
          "timestamp": "2026-09-28T01:59:27+08:00",
          "tree_id": "f911df15b6d21e6900447e072754865902bb15a7",
          "url": "https://github.com/LoveDaisy/Lumice/commit/26ab6a18e6455817e83e95ad76594539cffc24d6"
        },
        "date": 1790532805943,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 86.2,
            "unit": "%"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 100.2,
            "unit": "%"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 98.6,
            "unit": "%"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 93.3,
            "unit": "%"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "a7dc7af83f8475e1465a1e0705a2c6437b0e04dc",
          "message": "Merge pull request #435 from LoveDaisy/task/adopt-relative-floor-ray-allocation\n\ncore: relative floor in adaptive ray allocation stops starving a filtered arc",
          "timestamp": "2026-09-28T09:32:31+08:00",
          "tree_id": "e682da94016f1a73dff66ec1f11396547391e218",
          "url": "https://github.com/LoveDaisy/Lumice/commit/a7dc7af83f8475e1465a1e0705a2c6437b0e04dc"
        },
        "date": 1790559882351,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 97.7,
            "unit": "%"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 100.2,
            "unit": "%"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 98.7,
            "unit": "%"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 92,
            "unit": "%"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "1b0a0361b3465b124b87fc0680bff113b21524d4",
          "message": "Merge pull request #436 from LoveDaisy/task/restore-pb-label-equivalence\n\ncore: a filter's P/B/D is a label equivalence again; the analysis list keeps physical symmetry",
          "timestamp": "2026-09-28T10:02:45+08:00",
          "tree_id": "74c642836565436b225b79dbea5e311790408de7",
          "url": "https://github.com/LoveDaisy/Lumice/commit/1b0a0361b3465b124b87fc0680bff113b21524d4"
        },
        "date": 1790561939212,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 91.6,
            "unit": "%"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 100,
            "unit": "%"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 98.2,
            "unit": "%"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 94.6,
            "unit": "%"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "1455e6aeb0201ab8b80fe2ef34ff16cf1ae2ed30",
          "message": "Merge pull request #437 from LoveDaisy/chore/symmetry-two-meanings-cross-repo-docs\n\ndocs: state which symmetry meaning each repo's computation uses",
          "timestamp": "2026-09-28T10:49:54+08:00",
          "tree_id": "07236666f17979fcb194db2933d7e276e6f5c16e",
          "url": "https://github.com/LoveDaisy/Lumice/commit/1455e6aeb0201ab8b80fe2ef34ff16cf1ae2ed30"
        },
        "date": 1790564455658,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 95.2,
            "unit": "%"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 99.9,
            "unit": "%"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 98.7,
            "unit": "%"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 93.8,
            "unit": "%"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "5d353c63c4688fb9be43a5042ef239a4c310a93b",
          "message": "Merge pull request #439 from LoveDaisy/feat/channel-br-gain-x2\n\nChannel B-R display mode: raise gain from 0.5 to 2",
          "timestamp": "2026-09-28T14:33:59+08:00",
          "tree_id": "0b0e5066e995207a2d2bb4b5603b597496d80793",
          "url": "https://github.com/LoveDaisy/Lumice/commit/5d353c63c4688fb9be43a5042ef239a4c310a93b"
        },
        "date": 1790577948299,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 90.5,
            "unit": "%"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 99.9,
            "unit": "%"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 98.9,
            "unit": "%"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 93.7,
            "unit": "%"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "51bc059b3fa58ffeeef7ae600d704bf242a639de",
          "message": "Merge pull request #440 from LoveDaisy/feat/config-change-preview-transition\n\nGUI: a struct-hard edit no longer blanks the preview",
          "timestamp": "2026-09-28T15:21:22+08:00",
          "tree_id": "8725666a1d1fef58c78a1647e3213d2e90e95384",
          "url": "https://github.com/LoveDaisy/Lumice/commit/51bc059b3fa58ffeeef7ae600d704bf242a639de"
        },
        "date": 1790580909961,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 96.1,
            "unit": "%"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 100,
            "unit": "%"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 98.6,
            "unit": "%"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 91.9,
            "unit": "%"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "e6d934376e48fc8fc854f3d4f4a866d325bcd3bd",
          "message": "Merge pull request #441 from LoveDaisy/scrum/lumice-shared-lib-foundation\n\nPublish-ready shared library foundation: liblumice_analytic, per-library export lists, packaging",
          "timestamp": "2026-09-28T17:16:31+08:00",
          "tree_id": "d9f2f2216737e9645bf134e5c5a1d6d6b8628eca",
          "url": "https://github.com/LoveDaisy/Lumice/commit/e6d934376e48fc8fc854f3d4f4a866d325bcd3bd"
        },
        "date": 1790588101689,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 87.2,
            "unit": "%"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 99.7,
            "unit": "%"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 97.4,
            "unit": "%"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 94,
            "unit": "%"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "01b9dd59561fea636f6f0a888b375d2e42e95374",
          "message": "Merge pull request #442 from LoveDaisy/chore/release-4.7.1\n\nchore(release): cut 4.7.1",
          "timestamp": "2026-09-28T19:04:03+08:00",
          "tree_id": "67bb9062b1e0b8474469bb9b06c73e3f3b84ce30",
          "url": "https://github.com/LoveDaisy/Lumice/commit/01b9dd59561fea636f6f0a888b375d2e42e95374"
        },
        "date": 1790594372494,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 94.7,
            "unit": "%"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 99.6,
            "unit": "%"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 97.8,
            "unit": "%"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 94.1,
            "unit": "%"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "591c97d4c35ccce4e8f3f6dd1cf340e93390e6e4",
          "message": "Merge pull request #443 from LoveDaisy/chore/workflow-token-least-privilege\n\nci: add top-level permissions: contents: read to workflows",
          "timestamp": "2026-09-28T20:18:35+08:00",
          "tree_id": "200ab539ed87c0bd2915e7f5e25f6ed5dac388e4",
          "url": "https://github.com/LoveDaisy/Lumice/commit/591c97d4c35ccce4e8f3f6dd1cf340e93390e6e4"
        },
        "date": 1790598765523,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 89.8,
            "unit": "%"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 100.4,
            "unit": "%"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 96.1,
            "unit": "%"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 94.8,
            "unit": "%"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "73b0f697916c89df61e26e9fb12ee6bc0cbc6afb",
          "message": "Merge pull request #444 from LoveDaisy/scrum/analytic-module-a-v0\n\nliblumice_analytic module A v0: EvaluatePath, TraceFiber, DiscoverComponents + LI parity",
          "timestamp": "2026-09-29T09:40:58+08:00",
          "tree_id": "7dbc414388ddfcacdb2e91a6a1ddb3596a584cf4",
          "url": "https://github.com/LoveDaisy/Lumice/commit/73b0f697916c89df61e26e9fb12ee6bc0cbc6afb"
        },
        "date": 1790646744991,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 91.3,
            "unit": "%"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 100.1,
            "unit": "%"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 99.1,
            "unit": "%"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 94.9,
            "unit": "%"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "e129d76c7ac6844f6499a631f39e7eb81f169450",
          "message": "Merge pull request #445 from LoveDaisy/scrum/raypath-subcommand\n\nLumice raypath: single-path analysis module, lumice.h v4.50 entry, D4 verification",
          "timestamp": "2026-09-29T18:08:24+08:00",
          "tree_id": "940b3fbc2d846b901770e2c322d5509ac5278264",
          "url": "https://github.com/LoveDaisy/Lumice/commit/e129d76c7ac6844f6499a631f39e7eb81f169450"
        },
        "date": 1790677426951,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 92.3,
            "unit": "%"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 100.2,
            "unit": "%"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 99.3,
            "unit": "%"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 91.2,
            "unit": "%"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "9282fe084bf6c22f1620b39162daca6413f1cd29",
          "message": "Merge pull request #446 from LoveDaisy/scrum/analytic-wave2\n\nliblumice_analytic wave 2: per-pose diagnostics (API 5), band sum module B (API 6), 93 LI parity fixtures",
          "timestamp": "2026-09-30T08:45:42+08:00",
          "tree_id": "6d73ac11e978bac05aa5833e3d6725c7fa212092",
          "url": "https://github.com/LoveDaisy/Lumice/commit/9282fe084bf6c22f1620b39162daca6413f1cd29"
        },
        "date": 1790729854095,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 84.1,
            "unit": "%"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 100.1,
            "unit": "%"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 98.5,
            "unit": "%"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 94.2,
            "unit": "%"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "c0f0e1fe7ec5d6c1f1ebc35c76c60ee01de9c46b",
          "message": "Merge pull request #452 from LoveDaisy/chore/analytic-docs-weight-kink-critical-lines\n\ndocs(analytic): three kinds of critical lines; LI 31c682e parity fixtures (94)",
          "timestamp": "2026-09-30T19:42:02+08:00",
          "tree_id": "352b8d146251654b54e578eea86b4e7382b26ce7",
          "url": "https://github.com/LoveDaisy/Lumice/commit/c0f0e1fe7ec5d6c1f1ebc35c76c60ee01de9c46b"
        },
        "date": 1790769325581,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 104.8,
            "unit": "%"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 99.9,
            "unit": "%"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 99.5,
            "unit": "%"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 93.4,
            "unit": "%"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "5a6e0df8340efd9cf0b86e005e69a70cd1d7bf5c",
          "message": "Merge pull request #453 from LoveDaisy/scrum/capability-libs\n\nOrganize the C API and engine by capability",
          "timestamp": "2026-09-30T22:20:49+08:00",
          "tree_id": "e33e93629598cfbac336b950cd72445b26fb5b14",
          "url": "https://github.com/LoveDaisy/Lumice/commit/5a6e0df8340efd9cf0b86e005e69a70cd1d7bf5c"
        },
        "date": 1790779830503,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 83.8,
            "unit": "%"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 100.5,
            "unit": "%"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 98,
            "unit": "%"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 95.4,
            "unit": "%"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "6311b6bd9421e45d367cc351d08ae0cc257a3148",
          "message": "Merge pull request #455 from LoveDaisy/refactor/capi-scene-codec-split\n\nSplit c_api_scene.cpp into bridge, encoder and decoder",
          "timestamp": "2026-09-30T23:35:26+08:00",
          "tree_id": "e2bf504be9af427f91adf8d0e55d5f369cf2e35b",
          "url": "https://github.com/LoveDaisy/Lumice/commit/6311b6bd9421e45d367cc351d08ae0cc257a3148"
        },
        "date": 1790783641814,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 96.7,
            "unit": "%"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 100.1,
            "unit": "%"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 98.2,
            "unit": "%"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 94.3,
            "unit": "%"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "1d6bb6f143908c461fabfc30bd29f4f94e31dbc0",
          "message": "Merge pull request #451 from LoveDaisy/scrum/ci-time-governance\n\nscrum ci-time-governance (WIP): build-time landing",
          "timestamp": "2026-10-01T06:48:43+08:00",
          "tree_id": "14a7af4467d0122e428a63d9dfec71858a8ec59a",
          "url": "https://github.com/LoveDaisy/Lumice/commit/1d6bb6f143908c461fabfc30bd29f4f94e31dbc0"
        },
        "date": 1790808687101,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 98.3,
            "unit": "%"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 100.1,
            "unit": "%"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 99,
            "unit": "%"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 95.6,
            "unit": "%"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "7f16d97d16354d0b0e38424d0ee0c8cd2e53acf4",
          "message": "Merge pull request #462 from LoveDaisy/scrum/ci-time-governance\n\nci: continue time-governance calibration",
          "timestamp": "2026-10-01T17:12:50+08:00",
          "tree_id": "e0ea583d7775397055133bfe6213ee233562a204",
          "url": "https://github.com/LoveDaisy/Lumice/commit/7f16d97d16354d0b0e38424d0ee0c8cd2e53acf4"
        },
        "date": 1790846133917,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 98.9,
            "unit": "%"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 99.9,
            "unit": "%"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 96.3,
            "unit": "%"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 90.6,
            "unit": "%"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "0abb8e9bdc05a0db2723e33dd75904da10092d52",
          "message": "Merge pull request #463 from LoveDaisy/scrum/ci-time-governance-acceptance\n\ndocs(ci): close time-governance acceptance",
          "timestamp": "2026-10-01T18:29:14+08:00",
          "tree_id": "ad1f485ce54ff636e4d44ade48dbe43ca58fc4ff",
          "url": "https://github.com/LoveDaisy/Lumice/commit/0abb8e9bdc05a0db2723e33dd75904da10092d52"
        },
        "date": 1790850731448,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 88.1,
            "unit": "%"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 99.8,
            "unit": "%"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 98.7,
            "unit": "%"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 94.6,
            "unit": "%"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "4f133865f5df1f6ebbca26757196056d78e762bc",
          "message": "Merge pull request #464 from LoveDaisy/scrum/raypath-feature-diagnostics\n\nAdd target-free raypath feature diagnostics and scientific acceptance",
          "timestamp": "2026-10-02T13:12:28+08:00",
          "tree_id": "e5a02d2855e8ff1e8e73ae6d2a5f6ad04f028841",
          "url": "https://github.com/LoveDaisy/Lumice/commit/4f133865f5df1f6ebbca26757196056d78e762bc"
        },
        "date": 1790918107588,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 87.6,
            "unit": "%"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 100.2,
            "unit": "%"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 98.9,
            "unit": "%"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 96.1,
            "unit": "%"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "5fd26ee965155e74cedf143214fd9a1c3a9a0314",
          "message": "Merge pull request #465 from LoveDaisy/fix/ui-scale-window-geometry\n\nfix(gui): keep background aspect ratios consistent across UI scales",
          "timestamp": "2026-10-03T10:47:02+08:00",
          "tree_id": "2223b1e6369a418cca96f536688a1de685794a1c",
          "url": "https://github.com/LoveDaisy/Lumice/commit/5fd26ee965155e74cedf143214fd9a1c3a9a0314"
        },
        "date": 1790995803646,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 82.9,
            "unit": "%"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 100.2,
            "unit": "%"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 99.4,
            "unit": "%"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 93.5,
            "unit": "%"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "008837c9802a7b770cb84375e081eaa995c9a7ea",
          "message": "Merge pull request #466 from LoveDaisy/fix/channel-br-background-free\n\nfix(render): exclude sky background from B-R diagnostic",
          "timestamp": "2026-10-03T20:14:42+08:00",
          "tree_id": "c2814c53d58b59a0419d2d23b677c39bebde293c",
          "url": "https://github.com/LoveDaisy/Lumice/commit/008837c9802a7b770cb84375e081eaa995c9a7ea"
        },
        "date": 1791029857160,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 90.6,
            "unit": "%"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 99.9,
            "unit": "%"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 98.2,
            "unit": "%"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 95,
            "unit": "%"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "b6107e0c78b94e37fde81d77d8b596e16a2d3452",
          "message": "Merge pull request #467 from LoveDaisy/fix/pytest-performance-pool-isolation\n\nfix(testing): isolate local slow pytest performance pool",
          "timestamp": "2026-10-03T22:58:56+08:00",
          "tree_id": "2bb7504b7cfd61fc451738fe5ed1567077d8d80b",
          "url": "https://github.com/LoveDaisy/Lumice/commit/b6107e0c78b94e37fde81d77d8b596e16a2d3452"
        },
        "date": 1791039682147,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 96.3,
            "unit": "%"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 100.2,
            "unit": "%"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 97.9,
            "unit": "%"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 92.4,
            "unit": "%"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "928b8accd21aa950eeb249c13ad4bd8b4028b1b9",
          "message": "Merge pull request #468 from LoveDaisy/scrum/raypath-general-diagnostics-v2\n\nfeat(raypath): phase-1 raypath feature diagnostics (scrum-649)",
          "timestamp": "2026-10-05T12:17:39+08:00",
          "tree_id": "b9531b058ecf0cc3746ac4253f6cbb3fd2ee8930",
          "url": "https://github.com/LoveDaisy/Lumice/commit/928b8accd21aa950eeb249c13ad4bd8b4028b1b9"
        },
        "date": 1791174025668,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 94.2,
            "unit": "%"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 100,
            "unit": "%"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 96.7,
            "unit": "%"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 91.2,
            "unit": "%"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "6abcd36409158fd01a12b110cb2c89ba688dc8a1",
          "message": "Merge pull request #470 from LoveDaisy/fix/sky-direction-convention-mismatch\n\nfix(sky-direction): resolve convention-mismatch investigation — call sites proven correct, defense lines landed",
          "timestamp": "2026-10-06T05:26:14+08:00",
          "tree_id": "bc8d78bd7c6d3609d7eccaeeaf39dcb4e38e96f7",
          "url": "https://github.com/LoveDaisy/Lumice/commit/6abcd36409158fd01a12b110cb2c89ba688dc8a1"
        },
        "date": 1791235738621,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 93.2,
            "unit": "%"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 100,
            "unit": "%"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 94.9,
            "unit": "%"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 94.9,
            "unit": "%"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "236483fc270669e06b5b472f997c6e03974f82f5",
          "message": "Merge pull request #471 from LoveDaisy/docs/raypath-case-corpus\n\ndocs(cases): add raypath analysis scenario-case corpus index",
          "timestamp": "2026-10-06T11:07:25+08:00",
          "tree_id": "05675d1c473a536b58ff6c00a95dbda93a7db23c",
          "url": "https://github.com/LoveDaisy/Lumice/commit/236483fc270669e06b5b472f997c6e03974f82f5"
        },
        "date": 1791256192193,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 88.2,
            "unit": "%"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 99.9,
            "unit": "%"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 99,
            "unit": "%"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 94.1,
            "unit": "%"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "0c45b96ea97e477e6973097a68acbf4e6e7e3825",
          "message": "Merge pull request #472 from LoveDaisy/fix/outward-child-selfhit-leak\n\nfix(core): classify outward children at birth in CPU/Metal slab traversal",
          "timestamp": "2026-10-06T15:02:49+08:00",
          "tree_id": "5a3cb903bdbd5960d3ed2fddfb7201cfe72a62d2",
          "url": "https://github.com/LoveDaisy/Lumice/commit/0c45b96ea97e477e6973097a68acbf4e6e7e3825"
        },
        "date": 1791270325652,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 84,
            "unit": "%"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 100.2,
            "unit": "%"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 98.6,
            "unit": "%"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 93.5,
            "unit": "%"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "148248ccbf3b62682cd8191b787af84268d788d7",
          "message": "Merge pull request #473 from LoveDaisy/docs/corpus-c12-tint-caliber\n\ndocs(corpus): C12 tint multi-caliber + C06 LI closed-form anchor",
          "timestamp": "2026-10-07T17:57:54+08:00",
          "tree_id": "87b8eff64500a3715263acfc8706ee01d3846e00",
          "url": "https://github.com/LoveDaisy/Lumice/commit/148248ccbf3b62682cd8191b787af84268d788d7"
        },
        "date": 1791367471375,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 85.7,
            "unit": "%"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 100.1,
            "unit": "%"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 98.4,
            "unit": "%"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 93.9,
            "unit": "%"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "215a850397ac9dd42e2eb7b30ebcb657294c11f2",
          "message": "Merge pull request #474 from LoveDaisy/chore/release-4.7.2\n\nchore(release): cut 4.7.2",
          "timestamp": "2026-10-07T18:13:09+08:00",
          "tree_id": "b9ed9fbd7711749ea472f0e75e9c4ad8a572d253",
          "url": "https://github.com/LoveDaisy/Lumice/commit/215a850397ac9dd42e2eb7b30ebcb657294c11f2"
        },
        "date": 1791368221675,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 87.4,
            "unit": "%"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 100,
            "unit": "%"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 98.9,
            "unit": "%"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 93.5,
            "unit": "%"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "e6ca36f9b55e42a684caa58bff4213abf37f4cd0",
          "message": "Merge pull request #475 from LoveDaisy/feat/schema3-measure-layer\n\nfeat(raypath): schema3 measure layer (task-661)",
          "timestamp": "2026-10-08T01:15:42+08:00",
          "tree_id": "eecb54e33af022f4804aa681db1b25ae0dc6cdf4",
          "url": "https://github.com/LoveDaisy/Lumice/commit/e6ca36f9b55e42a684caa58bff4213abf37f4cd0"
        },
        "date": 1791393523964,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 104.4,
            "unit": "%"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 100,
            "unit": "%"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 98.6,
            "unit": "%"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 91.6,
            "unit": "%"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "ff81cb51f24df08becce17e67dc341e7acd0ddf7",
          "message": "Merge pull request #476 from LoveDaisy/fix/config-silent-noop-guards\n\nfix(config): warn on the two silent no-op config shapes",
          "timestamp": "2026-10-08T07:38:14+08:00",
          "tree_id": "72d56acc7970f3c253fb3fb79133f22677d0d85f",
          "url": "https://github.com/LoveDaisy/Lumice/commit/ff81cb51f24df08becce17e67dc341e7acd0ddf7"
        },
        "date": 1791416517448,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 92.6,
            "unit": "%"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 99.9,
            "unit": "%"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 99.1,
            "unit": "%"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 94,
            "unit": "%"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "1e88e8c405777eac6aa3f56227a2a0ccd28e82b6",
          "message": "Merge pull request #477 from LoveDaisy/feat/schema3-geometry-port\n\nfeat(analytic): schema3 geometry layer port (u-S2 field, partition, walks, focusing/chromatic, contour, API v8)",
          "timestamp": "2026-10-09T03:56:32+08:00",
          "tree_id": "6d823a2cb115de1d1cc95c3511bd06308e497425",
          "url": "https://github.com/LoveDaisy/Lumice/commit/1e88e8c405777eac6aa3f56227a2a0ccd28e82b6"
        },
        "date": 1791489569285,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 88.3,
            "unit": "%"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 99.8,
            "unit": "%"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 99.1,
            "unit": "%"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 94.8,
            "unit": "%"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "826acd4bd8a4c3757162564f22e74f56e8c39510",
          "message": "Merge pull request #478 from LoveDaisy/feat/schema3-report\n\nfeat(raypath): schema3 report - state machines, support block, unattributed, mc_evidence, schema_version=3",
          "timestamp": "2026-10-10T10:31:01+08:00",
          "tree_id": "6cbc9655451153e80826f2030b8cca8d1547e6bc",
          "url": "https://github.com/LoveDaisy/Lumice/commit/826acd4bd8a4c3757162564f22e74f56e8c39510"
        },
        "date": 1791599691228,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 88.1,
            "unit": "%"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 99.9,
            "unit": "%"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 100.2,
            "unit": "%"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 94.9,
            "unit": "%"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "a2c5d90a7a3f0918d628d6b49921aad3a0a7c8ed",
          "message": "Merge pull request #479 from LoveDaisy/fix/analyze-render-share-calibration\n\nShare-domain calibration: lens landing semantics + dual fold-boundary defect + cone cross-check tool (task-669)",
          "timestamp": "2026-10-10T18:45:27+08:00",
          "tree_id": "45ab14128501225882d5f360859fce217f5b3a9f",
          "url": "https://github.com/LoveDaisy/Lumice/commit/a2c5d90a7a3f0918d628d6b49921aad3a0a7c8ed"
        },
        "date": 1791629302856,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 106.6,
            "unit": "%"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 99.6,
            "unit": "%"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 98.9,
            "unit": "%"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 91.6,
            "unit": "%"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "6e0a5b01d67218589710fdc63aaa96e5c9bab05d",
          "message": "Merge pull request #480 from LoveDaisy/fix/dual-fisheye-fold-boundary-loss\n\nfix(raypath): clamp dual-fisheye fold-boundary pixels into canvas (task-670)",
          "timestamp": "2026-10-10T22:52:34+08:00",
          "tree_id": "9b991e7eb24effad677fcd4328ccfd41633c9d4f",
          "url": "https://github.com/LoveDaisy/Lumice/commit/6e0a5b01d67218589710fdc63aaa96e5c9bab05d"
        },
        "date": 1791644147835,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 90.3,
            "unit": "%"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 100,
            "unit": "%"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 99.1,
            "unit": "%"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 87.4,
            "unit": "%"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "f50e9639b74896229064fc6f302f8fbfa2251e06",
          "message": "Merge pull request #481 from LoveDaisy/chore/mc-corner-tier-contract-sync\n\nClarify mixed-corner tolerance consumer comment",
          "timestamp": "2026-10-11T01:54:48+08:00",
          "tree_id": "485185c3b80de593fe4ee9348baf6c20b9c33bde",
          "url": "https://github.com/LoveDaisy/Lumice/commit/f50e9639b74896229064fc6f302f8fbfa2251e06"
        },
        "date": 1791655086873,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 96.2,
            "unit": "%"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 99.9,
            "unit": "%"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 98.3,
            "unit": "%"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 93.2,
            "unit": "%"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "b8396281f9127160a6677a9d802dd3b90c6d91d5",
          "message": "Merge pull request #482 from LoveDaisy/chore/ci-raypath-test-cost-governance\n\nci: reduce repeated raypath test work and govern cumulative duration",
          "timestamp": "2026-10-11T06:49:40+08:00",
          "tree_id": "13f1062264c8f6979884be10399307145b272e07",
          "url": "https://github.com/LoveDaisy/Lumice/commit/b8396281f9127160a6677a9d802dd3b90c6d91d5"
        },
        "date": 1791672788251,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 95.1,
            "unit": "%"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 100,
            "unit": "%"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 98.5,
            "unit": "%"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 92.3,
            "unit": "%"
          }
        ]
      }
    ]
  }
}