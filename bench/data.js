window.BENCHMARK_DATA = {
  "lastUpdate": 1795850390191,
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
          "id": "725018d33ad9a3d08ded65180c4f2d3272581cd7",
          "message": "Merge pull request #531 from LoveDaisy/feat/free-dims-enum-cleanup\n\nrefactor(raypath): free_dims carries the dim enum",
          "timestamp": "2026-10-21T00:02:56+08:00",
          "tree_id": "280b8049e2f76e79f05bd54c0b9bc03c21e48818",
          "url": "https://github.com/LoveDaisy/Lumice/commit/725018d33ad9a3d08ded65180c4f2d3272581cd7"
        },
        "date": 1792512388112,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 352190.4,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 506043.7,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 366963.7,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 284690.9,
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
          "id": "a7f8b4eaab29ae7dd4fb9f70c7b10040a958550d",
          "message": "Merge pull request #533 from LoveDaisy/feat/ci-ctest-budget-rebalance\n\ntest(ci): zero-sum ctest budget rebalance + the four survey structure cleanups",
          "timestamp": "2026-10-21T20:09:47+08:00",
          "tree_id": "d1414e4f2c68d617f42b63835e7c89df495db972",
          "url": "https://github.com/LoveDaisy/Lumice/commit/a7f8b4eaab29ae7dd4fb9f70c7b10040a958550d"
        },
        "date": 1792584802929,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 351185.3,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 505496.4,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 415203,
            "unit": "rays/sec",
            "extra": "CPU: Intel(R) Xeon(R) Platinum 8370C CPU @ 2.80GHz\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 360168.3,
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
          "id": "e3acdfcb46e5115e09897dc2a33218a679a73714",
          "message": "Merge pull request #534 from LoveDaisy/feat/c10-ladder-gate-order\n\nfix(raypath): the level-0 partition refusal scopes to its own card",
          "timestamp": "2026-10-21T21:59:06+08:00",
          "tree_id": "0dd10a9ed7c0e3fba17f416a6d2095a87d6673a6",
          "url": "https://github.com/LoveDaisy/Lumice/commit/e3acdfcb46e5115e09897dc2a33218a679a73714"
        },
        "date": 1792591356739,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 307518.8,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 504144,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 367790.9,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 307347.7,
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
          "id": "1792d962467839be845bd5577eabc039164cf440",
          "message": "Merge pull request #536 from LoveDaisy/feat/cuda-continue-legs\n\ntest(server): land the four CUDA continuation legs from the reference-box probe",
          "timestamp": "2026-10-21T22:52:57+08:00",
          "tree_id": "d08f791614a40b3b1163ff40f6dea1e31ff11104",
          "url": "https://github.com/LoveDaisy/Lumice/commit/1792d962467839be845bd5577eabc039164cf440"
        },
        "date": 1792594567852,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 397252.4,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 505032.4,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 396024,
            "unit": "rays/sec",
            "extra": "CPU: INTEL(R) XEON(R) PLATINUM 8573C\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 470265.3,
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
          "id": "3a789961cdb423ef5ea1b5b7ec0d5899e3187166",
          "message": "Merge pull request #535 from LoveDaisy/feat/threshold-single-source\n\nrefactor(gui-test): thresholds resolve from _thresholds.json at run time",
          "timestamp": "2026-10-21T23:46:17+08:00",
          "tree_id": "57b1ca93a4d41a1cfb107f290788ddda5f971e59",
          "url": "https://github.com/LoveDaisy/Lumice/commit/3a789961cdb423ef5ea1b5b7ec0d5899e3187166"
        },
        "date": 1792597776423,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 427310.8,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 504211.9,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 366112.6,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 364396,
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
          "id": "49620f6e2374ecef84e0cec7c546028623c60493",
          "message": "Merge pull request #537 from LoveDaisy/feat/gap3-tir-ess-probes-c18\n\nfeat(raypath): TIR share + opt-in MC region ESS evidence legs (gap ③)",
          "timestamp": "2026-10-22T14:58:23+08:00",
          "tree_id": "62674616b336864170d87cd4568ecea21be722b6",
          "url": "https://github.com/LoveDaisy/Lumice/commit/49620f6e2374ecef84e0cec7c546028623c60493"
        },
        "date": 1792652510752,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 350675.5,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 503321.7,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 415039.5,
            "unit": "rays/sec",
            "extra": "CPU: Intel(R) Xeon(R) Platinum 8370C CPU @ 2.80GHz\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 311194.9,
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
          "id": "b7910bd93417b149f483132e534f1f50f44153c2",
          "message": "Merge pull request #538 from LoveDaisy/feat/raypath-point-l2-warm\n\nfeat(raypath): the L2 point command — factor decomposition + the --warm index contract",
          "timestamp": "2026-10-22T23:20:21+08:00",
          "tree_id": "4041bf027823738c4e184bac29559cb4397a5908",
          "url": "https://github.com/LoveDaisy/Lumice/commit/b7910bd93417b149f483132e534f1f50f44153c2"
        },
        "date": 1792682627946,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 312344.6,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 506303,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 549678.7,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 9V45 96-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 473175.2,
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
          "id": "272c3e0749d0ff9744a8ebc02f71b4ce04dbf205",
          "message": "Merge pull request #539 from LoveDaisy/feat/raypath-probe-profiles\n\nfeat(raypath): the L2 probe command — four --along forms on the shared line core",
          "timestamp": "2026-10-23T06:59:16+08:00",
          "tree_id": "2dcbad7a3a36b56e1c3c0d192a8839bba8dd9b45",
          "url": "https://github.com/LoveDaisy/Lumice/commit/272c3e0749d0ff9744a8ebc02f71b4ce04dbf205"
        },
        "date": 1792710173304,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 421126.3,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 506316.3,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 367468.4,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 306509.1,
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
          "id": "3a00c4a7ac2656f50f20eb2f80c60fd77fda7205",
          "message": "Merge pull request #541 from LoveDaisy/feat/ctest-machine-normalized-budget\n\nfeat(test-gate): machine-speed normalization for ctest phase budgets (registry v4)",
          "timestamp": "2026-10-23T13:27:12+08:00",
          "tree_id": "3fb16cb4a4390a3797a2d66b5a9aa36add0388b9",
          "url": "https://github.com/LoveDaisy/Lumice/commit/3a00c4a7ac2656f50f20eb2f80c60fd77fda7205"
        },
        "date": 1792733474129,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 410929,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 506985.1,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 362741.8,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 482012.2,
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
          "id": "76fc7455b0fe29c44ca2c36c777f7cf3d88bbf16",
          "message": "Merge pull request #540 from LoveDaisy/feat/gap2-subgroup-gating\n\nfeat(core): physical P gating as a per-element retained-rotation set (gap ②)",
          "timestamp": "2026-10-23T13:58:39+08:00",
          "tree_id": "344d73644fead342651a339dcc81d442de596644",
          "url": "https://github.com/LoveDaisy/Lumice/commit/76fc7455b0fe29c44ca2c36c777f7cf3d88bbf16"
        },
        "date": 1792735753713,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 336078.9,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 506961.6,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 523426.6,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 9V45 96-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 310316.8,
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
          "id": "4ebb977073157d94978c2dd51509ed2006597e23",
          "message": "Merge pull request #542 from LoveDaisy/feat/testing-inner-loop\n\nfeat(test): the inner scope — diff-derived layers for the iteration loop",
          "timestamp": "2026-10-23T14:32:36+08:00",
          "tree_id": "08016163a6e1869c1c3db2730a08078ddbe1e0dc",
          "url": "https://github.com/LoveDaisy/Lumice/commit/4ebb977073157d94978c2dd51509ed2006597e23"
        },
        "date": 1792737376075,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 389639.7,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 506323.1,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 552908.9,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 9V45 96-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 308681.1,
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
          "id": "44446dcf5082221f38943bb7ef5f2a4bb89c7240",
          "message": "Merge pull request #543 from LoveDaisy/feat/survey-wave-review-sweep\n\nfix(core): zero-width pole-centred gluing excluded; the wave's review sweep (task-735)",
          "timestamp": "2026-10-23T20:19:32+08:00",
          "tree_id": "ea55d5da85b9ea01bace2af47e90705d3526c028",
          "url": "https://github.com/LoveDaisy/Lumice/commit/44446dcf5082221f38943bb7ef5f2a4bb89c7240"
        },
        "date": 1792758207357,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 380058.5,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 505003.8,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 367955.9,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 486851.3,
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
          "id": "61686ea7e253835425093a7964672997f4333dd5",
          "message": "Merge pull request #544 from LoveDaisy/chore/release-4.7.5\n\nchore(release): cut 4.7.5",
          "timestamp": "2026-10-23T21:28:21+08:00",
          "tree_id": "db9017bd4b326dbf208009eade13622c90e7b9b6",
          "url": "https://github.com/LoveDaisy/Lumice/commit/61686ea7e253835425093a7964672997f4333dd5"
        },
        "date": 1792762288456,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 375406.8,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 505055.6,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 431270.8,
            "unit": "rays/sec",
            "extra": "CPU: INTEL(R) XEON(R) PLATINUM 8573C\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 480196.4,
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
          "id": "f6b77fdab11fb6bc88f63fcb794f68de73f690ea",
          "message": "Merge pull request #545 from LoveDaisy/feat/raypath-warm-structure-scope\n\nfix(raypath): scope warm numeric facts to their producing wavelength",
          "timestamp": "2026-10-24T15:57:44+08:00",
          "tree_id": "4f5fec9f158d5d80e6cdc8ff173a351e2c42d8c7",
          "url": "https://github.com/LoveDaisy/Lumice/commit/f6b77fdab11fb6bc88f63fcb794f68de73f690ea"
        },
        "date": 1792828974782,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 253109.7,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 505995.2,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 368609.3,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 313914.5,
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
          "id": "204775d4b7b287915b6fe3941773c1eb8df34d19",
          "message": "Merge pull request #547 from LoveDaisy/feat/raypath-typed-architecture\n\nrefactor(raypath): complete typed-first query and C API architecture",
          "timestamp": "2026-10-27T02:50:35+08:00",
          "tree_id": "3a08be3734802675a46cfc191fef7514cdae32c2",
          "url": "https://github.com/LoveDaisy/Lumice/commit/204775d4b7b287915b6fe3941773c1eb8df34d19"
        },
        "date": 1793040869175,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 405829.2,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 504803.3,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 367686.5,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 310180.8,
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
          "id": "696685bc25d5ef49575143b12f37744b2519944d",
          "message": "Merge pull request #548 from LoveDaisy/feat/crystal-card-text-hit-overlap\n\nFix crystal card text probes stealing button clicks",
          "timestamp": "2026-10-27T22:36:07+08:00",
          "tree_id": "e128f7f0b209745c23eb487980fd9d05b97eff32",
          "url": "https://github.com/LoveDaisy/Lumice/commit/696685bc25d5ef49575143b12f37744b2519944d"
        },
        "date": 1793112017290,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 371395,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 502336.5,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 367306.8,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 309396.5,
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
          "id": "4457f2a77b4c98815d730e5d030e079f192487d0",
          "message": "Merge pull request #549 from LoveDaisy/feat/test-iteration-speed\n\nperf(test): shorten iteration feedback with dedup and sharding",
          "timestamp": "2026-10-28T19:24:30+08:00",
          "tree_id": "65aaff66a0a607a3199b30ac098be2cec79ab778",
          "url": "https://github.com/LoveDaisy/Lumice/commit/4457f2a77b4c98815d730e5d030e079f192487d0"
        },
        "date": 1793186899773,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 312986.8,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 503617.2,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 366343.6,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 308729.5,
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
          "id": "82acb1c7506066de7efea14f2991f45b483e0e0a",
          "message": "Merge pull request #550 from LoveDaisy/feat/raypath-docs-promotion\n\ndocs(raypath): promote diagnostic pipeline design and refresh the roadmap",
          "timestamp": "2026-10-30T01:21:16+08:00",
          "tree_id": "f2649338d9f7df1cc633beb6c3fe07372356de44",
          "url": "https://github.com/LoveDaisy/Lumice/commit/82acb1c7506066de7efea14f2991f45b483e0e0a"
        },
        "date": 1793294728324,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 327640.4,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 504637.2,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 589624.6,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 9V45 96-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 305678.4,
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
          "id": "b370f17aca80a388068584dd0e209daaf92dc0ba",
          "message": "Merge pull request #551 from LoveDaisy/feat/analyze-gui-v1\n\nfeat: Analyze GUI v1 — class reference maps, representative rays, layered overview, dispersion, map & point inspection",
          "timestamp": "2026-10-31T08:49:46+08:00",
          "tree_id": "23dabe19bd569422f87f8a4d312ff63f7fd70664",
          "url": "https://github.com/LoveDaisy/Lumice/commit/b370f17aca80a388068584dd0e209daaf92dc0ba"
        },
        "date": 1793408042184,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 340501,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 504763.3,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 424030.4,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 9V74 80-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 314711.8,
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
          "id": "a92d037c5dc3459b9668540e36cc255c65b8c7b2",
          "message": "Merge pull request #552 from LoveDaisy/feat/realtime-legs-placement\n\ntest: reclaim the one analyze CI row that actually pays wall-clock — real-timing legs never ran in PR ctest",
          "timestamp": "2026-10-31T13:23:32+08:00",
          "tree_id": "ad4451ec4d84a542a8ea46928ecb4229d88297e5",
          "url": "https://github.com/LoveDaisy/Lumice/commit/a92d037c5dc3459b9668540e36cc255c65b8c7b2"
        },
        "date": 1793424419021,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 426149.2,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 506342.5,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 576655.2,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 9V45 96-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 335601.9,
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
          "id": "71ee8904b6b1a70d8ea0b90f1c3ae44e0108792f",
          "message": "Merge pull request #553 from LoveDaisy/feat/panorama-drift-fix\n\nfix(gui): the analyze panorama published OS-screen coords as framebuffer coords — texture drifted with the window",
          "timestamp": "2026-10-31T15:01:13+08:00",
          "tree_id": "007cf6a260e6236bc6eec33270bf4f1ad7915f42",
          "url": "https://github.com/LoveDaisy/Lumice/commit/71ee8904b6b1a70d8ea0b90f1c3ae44e0108792f"
        },
        "date": 1793430304188,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 338148.9,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 505357.2,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 362987.4,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 367297.1,
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
          "id": "f64a77d8de746d0479d5ec85665e23b629b57fea",
          "message": "Merge pull request #554 from LoveDaisy/feat/analyze-panel-clip-fix\n\nfix(analyze): three scroll regions clipped their headers — measured reserves, a table that absorbs, and an 8pt strip ImGui secretly sized to 32pt",
          "timestamp": "2026-10-31T18:04:58+08:00",
          "tree_id": "440d8445970baae5ba4c237d591750c937060670",
          "url": "https://github.com/LoveDaisy/Lumice/commit/f64a77d8de746d0479d5ec85665e23b629b57fea"
        },
        "date": 1793441338413,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 346407.1,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 505513.1,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 573513.5,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 9V45 96-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 315404.3,
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
          "id": "0cda0c24a2505274ce88b5c332db83b11bf9e08b",
          "message": "Merge pull request #555 from LoveDaisy/feat/ray3d-orbit-grab-fix\n\nfix(gui): the ray3d orbit basis had pitch and yaw slots transposed — axes swapped and the drag metaphor scrambled",
          "timestamp": "2026-10-31T18:30:37+08:00",
          "tree_id": "9a32ca08414fc7310da15fb49a95dcd6668e8f8e",
          "url": "https://github.com/LoveDaisy/Lumice/commit/0cda0c24a2505274ce88b5c332db83b11bf9e08b"
        },
        "date": 1793442961822,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 295027.1,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 505568.8,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 362369.5,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 315113.2,
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
          "id": "284c9455d369800eb8b70405d488abe377dbf75c",
          "message": "Merge pull request #556 from LoveDaisy/feat/analyze-wait-feedback-a\n\nfeat(analyze): the wait says how long it has waited — per-face elapsed lines, a status-bar Analyzing segment, row activity dots",
          "timestamp": "2026-10-31T19:08:56+08:00",
          "tree_id": "7d66f431a427bc186d7e1e8075afa021ede499f2",
          "url": "https://github.com/LoveDaisy/Lumice/commit/284c9455d369800eb8b70405d488abe377dbf75c"
        },
        "date": 1793445160725,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 403671.6,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 505463.6,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 363609.4,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 307918,
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
          "id": "ed0062b7978aff4024c97d61b3eaf193332a0ce7",
          "message": "Merge pull request #557 from LoveDaisy/feat/witness-tir-deadline\n\nfix(survey): the 15s deadline now bites inside the witness and TIR legs — cooperative polls at batch boundaries",
          "timestamp": "2026-10-31T22:25:04+08:00",
          "tree_id": "7a451064b6104c5005007f3631ff8bf19bfb1077",
          "url": "https://github.com/LoveDaisy/Lumice/commit/ed0062b7978aff4024c97d61b3eaf193332a0ce7"
        },
        "date": 1793456940976,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 300169.6,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 505702.3,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 421462.5,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 9V74 80-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 310305.3,
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
          "id": "33934bd7de38458727db064be595da31a2314160",
          "message": "Merge pull request #558 from LoveDaisy/feat/topbar-toggle-regroup\n\nfeat(gui): the workspace switch becomes its own segmented group — and the top bar's groups get room to breathe",
          "timestamp": "2026-10-31T22:51:20+08:00",
          "tree_id": "43b7e4bb1b41920e5806a9a565ed409eb8669ac6",
          "url": "https://github.com/LoveDaisy/Lumice/commit/33934bd7de38458727db064be595da31a2314160"
        },
        "date": 1793458530856,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 315990.8,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 505584.8,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 362475.1,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 503030.1,
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
          "id": "a6c9ed30e3b546ecded6de86bb35045ee50ca793",
          "message": "Merge pull request #559 from LoveDaisy/feat/query-stage-heartbeat-abi\n\nfeat: poll-stage heartbeat over the cancel token (C ABI v4.61)",
          "timestamp": "2026-11-01T03:11:41+08:00",
          "tree_id": "eef80cb7afd5f8eff4f733b0af4b7b1569895512",
          "url": "https://github.com/LoveDaisy/Lumice/commit/a6c9ed30e3b546ecded6de86bb35045ee50ca793"
        },
        "date": 1793474150295,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 353703.7,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 504305.4,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 366741.6,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 313003,
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
          "id": "dabd3f70e5c64ffd45b2e93832dcdd02e1fe19fd",
          "message": "Merge pull request #560 from LoveDaisy/fix/analyze-reanalyze-survey-validation-blocker\n\nfix(gui): synchronize analysis reselection and quiesce teardown",
          "timestamp": "2026-11-03T03:49:26+08:00",
          "tree_id": "e1322da05e898511f4110603b9c181c936522363",
          "url": "https://github.com/LoveDaisy/Lumice/commit/dabd3f70e5c64ffd45b2e93832dcdd02e1fe19fd"
        },
        "date": 1793649323910,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 359530.2,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 504907.7,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 366669.3,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 310530.9,
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
          "id": "9930c9a89fd804e79320f1faa457c33a994c3839",
          "message": "Merge pull request #562 from LoveDaisy/feat/msvc-math-macro-gate\n\nfix(policy): reject MSVC-unsafe math constant macros",
          "timestamp": "2026-11-03T04:13:23+08:00",
          "tree_id": "84c5c41207c1680acdcccc3a71c06f77fc6b7652",
          "url": "https://github.com/LoveDaisy/Lumice/commit/9930c9a89fd804e79320f1faa457c33a994c3839"
        },
        "date": 1793650642483,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 423324.8,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 504749,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 366590.6,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 305955.3,
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
          "id": "3aadae00161f1b38193db5fd3c7ac5f4535f59b3",
          "message": "Merge pull request #563 from LoveDaisy/feat/fiber-arclength-payload-probe\n\ndocs(raypath): fiber arclength payload audit for contribution curves",
          "timestamp": "2026-11-04T04:59:13+08:00",
          "tree_id": "e5adef887341388bc5d87629182c19bfa3d0f278",
          "url": "https://github.com/LoveDaisy/Lumice/commit/3aadae00161f1b38193db5fd3c7ac5f4535f59b3"
        },
        "date": 1793739830914,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 342971.5,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 506313.4,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 330180.1,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 9V74 80-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 310894.7,
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
          "id": "5f0968259b932181425988ebdb95fc4a422442bb",
          "message": "Merge pull request #564 from LoveDaisy/feat/analyze-gui-rework\n\nAnalyze GUI rework: task-driven main view, resizable regions, independent windows",
          "timestamp": "2026-11-06T04:38:13+08:00",
          "tree_id": "6d70ce4fe8476d0d71d3a15141a3c4e9c8693a71",
          "url": "https://github.com/LoveDaisy/Lumice/commit/5f0968259b932181425988ebdb95fc4a422442bb"
        },
        "date": 1793911362828,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 346463,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 504968.8,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 363347.5,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 288593.2,
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
          "id": "3fa8e2cd61fcb3352f6adf6311731e91e2b30109",
          "message": "Merge pull request #565 from LoveDaisy/feat/executor-ordering-fixture\n\ntest(gui): executor ordering contract arms — supersede refusal, shutdown join, newest-wins",
          "timestamp": "2026-11-06T06:41:41+08:00",
          "tree_id": "ae2aea9c76fbf464eefce3a7536d023941059f41",
          "url": "https://github.com/LoveDaisy/Lumice/commit/3fa8e2cd61fcb3352f6adf6311731e91e2b30109"
        },
        "date": 1793918788165,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 385115.3,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 506891.7,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 366905.7,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 339817.6,
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
          "id": "5b630fe90d2117f664e2d37c0c201b6409149997",
          "message": "Merge pull request #566 from LoveDaisy/feat/analyze-fiber-contribution-curve\n\nfeat(gui): contribution curve — named factors and product over true SO(3) arclength",
          "timestamp": "2026-11-06T12:10:06+08:00",
          "tree_id": "58873d41ff72f12c505e1cf4e4be4662d61ef2c7",
          "url": "https://github.com/LoveDaisy/Lumice/commit/5b630fe90d2117f664e2d37c0c201b6409149997"
        },
        "date": 1793938432576,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 374632,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 504882.2,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 421220.3,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 9V74 80-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 341076,
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
          "id": "9e6d278e623a50ba610259478331d5ee90c34408",
          "message": "Merge pull request #567 from LoveDaisy/feat/analysis-pump-sky-intent-pickup\n\ntest(gui): the map wait streaks, the Set input asserts, and broken preconditions fail fast",
          "timestamp": "2026-11-06T13:55:02+08:00",
          "tree_id": "8aa0628662045bfe715965f94e9b5fa16ad7c406",
          "url": "https://github.com/LoveDaisy/Lumice/commit/9e6d278e623a50ba610259478331d5ee90c34408"
        },
        "date": 1793944739663,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 421758.4,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 504955.8,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 366637.1,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 315263.6,
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
          "id": "c1149fe20a40b4a978bc47b42a60b971ac3f9f5d",
          "message": "Merge pull request #568 from LoveDaisy/feat/scopes-gate-load-budget\n\ntest(gui): scopes gates judge recorded edges, hold in-flight actions, and drain the whole board",
          "timestamp": "2026-11-06T14:25:57+08:00",
          "tree_id": "f625921baa7c96bf5f98fc7163267e9f1701e8ac",
          "url": "https://github.com/LoveDaisy/Lumice/commit/c1149fe20a40b4a978bc47b42a60b971ac3f9f5d"
        },
        "date": 1793946575691,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 349156.1,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 504883.3,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 368053.6,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 367224.8,
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
          "id": "da8dbd629fdc05ea5ef7dc6ed218130409569f00",
          "message": "Merge pull request #569 from LoveDaisy/feat/analyze-main-direct-gestures\n\nfix(gui): direct point gestures and shared main-view zoom",
          "timestamp": "2026-11-06T20:51:42+08:00",
          "tree_id": "1034a6db0ddbe12f966cb71f61d31b197e9a04f3",
          "url": "https://github.com/LoveDaisy/Lumice/commit/da8dbd629fdc05ea5ef7dc6ed218130409569f00"
        },
        "date": 1793969706060,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 372037.6,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 503648.3,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 390330.2,
            "unit": "rays/sec",
            "extra": "CPU: INTEL(R) XEON(R) PLATINUM 8573C\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 365371.3,
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
          "id": "cf9972947e96e67045d69b96f68799a1d2823ef7",
          "message": "Merge pull request #570 from LoveDaisy/fix/cancel-survey-observation-scope\n\nfix(test): observe survey cancellation without global idle",
          "timestamp": "2026-11-06T23:31:50+08:00",
          "tree_id": "c2ea747b7eccb4fe178fdbcf931fe9d696c2bf9e",
          "url": "https://github.com/LoveDaisy/Lumice/commit/cf9972947e96e67045d69b96f68799a1d2823ef7"
        },
        "date": 1793979336922,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 275602.9,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 504419.4,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 409277.3,
            "unit": "rays/sec",
            "extra": "CPU: Intel(R) Xeon(R) Platinum 8370C CPU @ 2.80GHz\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 366175.3,
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
          "id": "54df3277d002f0ba3e76f30952c881890c7d37e2",
          "message": "Merge pull request #571 from LoveDaisy/feat/font-glyph-lambda-range\n\nfix(gui): load missing delta and lambda glyphs",
          "timestamp": "2026-11-07T01:09:00+08:00",
          "tree_id": "ac9a9b1bfa0453c3aa3be2ae737295fa67cd70bb",
          "url": "https://github.com/LoveDaisy/Lumice/commit/54df3277d002f0ba3e76f30952c881890c7d37e2"
        },
        "date": 1793985163673,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 373384.4,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 504143.8,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 365703.2,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 468610.8,
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
          "id": "347a4e5c5821ff1a77061706e0e80a16f985bafc",
          "message": "Merge pull request #572 from LoveDaisy/test/map-pump-fixture-query-isolation\n\ntest(gui): remove unrelated survey work from map fixtures",
          "timestamp": "2026-11-07T02:39:25+08:00",
          "tree_id": "17372e67756c5e4a76ebad72e1ecba46bbf68e83",
          "url": "https://github.com/LoveDaisy/Lumice/commit/347a4e5c5821ff1a77061706e0e80a16f985bafc"
        },
        "date": 1793990597817,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 355329.5,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 504766.1,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 425387.3,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 9V74 80-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 310874.6,
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
          "id": "3d2c4f05d9f464d164acf08e609a75a356daeb49",
          "message": "Merge pull request #573 from LoveDaisy/feat/analyze-layout-stability\n\nfix(gui): stabilize Analyze canvases and transient status layout",
          "timestamp": "2026-11-07T04:37:00+08:00",
          "tree_id": "2400774c1f61f029a64f97fb2880a3281f427dc2",
          "url": "https://github.com/LoveDaisy/Lumice/commit/3d2c4f05d9f464d164acf08e609a75a356daeb49"
        },
        "date": 1793997661128,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 329855.3,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 505541.3,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 557841.1,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 9V45 96-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 310150.1,
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
          "id": "824e978f0926f336217b4856d55086907fca8aa5",
          "message": "Merge pull request #574 from LoveDaisy/feat/doc-switch-keeps-workspace-mode\n\nfix(gui): preserve workspace mode across document switches",
          "timestamp": "2026-11-07T07:30:56+08:00",
          "tree_id": "932431bb0a545f9c1c9ae5f6ed910a71b8e28706",
          "url": "https://github.com/LoveDaisy/Lumice/commit/824e978f0926f336217b4856d55086907fca8aa5"
        },
        "date": 1794008095194,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 329585.8,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 504639,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 364163.8,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 367151.5,
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
          "id": "a4719d5178481cc9a506c71cbab37271f585e3f2",
          "message": "Merge pull request #575 from LoveDaisy/feat/raypath-pose-entry\n\nfeat(gui): open Pose & trace beside sky-point controls",
          "timestamp": "2026-11-07T08:10:47+08:00",
          "tree_id": "2b3fa8c58803976a6f6537a96676351f3eb5cb61",
          "url": "https://github.com/LoveDaisy/Lumice/commit/a4719d5178481cc9a506c71cbab37271f585e3f2"
        },
        "date": 1794010476252,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 318066.8,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 505622.5,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 364799.2,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 489244.8,
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
          "id": "0dc0fc9d40c4357238282b96410646dbeab7def7",
          "message": "Merge pull request #576 from LoveDaisy/feat/toolbar-adaptive-labels\n\nfeat(gui): adapt toolbar labels consistently to available width",
          "timestamp": "2026-11-07T09:27:47+08:00",
          "tree_id": "e7f3582dec340d9a9f0787308d3de5d68c9f2051",
          "url": "https://github.com/LoveDaisy/Lumice/commit/0dc0fc9d40c4357238282b96410646dbeab7def7"
        },
        "date": 1794015107796,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 401086.1,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 504896.7,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 366839.9,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 308879,
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
          "id": "b3c067009d90d6d135d8cc1147f1ff371ecac4dd",
          "message": "Merge pull request #577 from LoveDaisy/feat/raypath-view-camera-unify-design\n\ndocs(gui): define shared camera and explicit map refinement contract",
          "timestamp": "2026-11-07T10:32:16+08:00",
          "tree_id": "86cb59a0680c1e62ab39c628b9b72f127da479e9",
          "url": "https://github.com/LoveDaisy/Lumice/commit/b3c067009d90d6d135d8cc1147f1ff371ecac4dd"
        },
        "date": 1794019019736,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 337382.6,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 506272.1,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 426293.8,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 9V74 80-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 341615.3,
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
          "id": "1e1a1cfbb394c00fcaf0d05c19e737facd8edab6",
          "message": "Merge pull request #578 from LoveDaisy/feat/raypath-view-camera-unify\n\nfeat(gui): share raypath camera and make map refinement explicit",
          "timestamp": "2026-11-08T01:39:29+08:00",
          "tree_id": "fd7e92e0a2fc8277d0b09a6dbc81e07e36b2cc63",
          "url": "https://github.com/LoveDaisy/Lumice/commit/1e1a1cfbb394c00fcaf0d05c19e737facd8edab6"
        },
        "date": 1794073404783,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 301055.2,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 505623.1,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 590386.7,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 9V45 96-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 454461,
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
          "id": "7b978e0d8758f5a30b6523d75426d9951a0277a6",
          "message": "Merge pull request #579 from LoveDaisy/feat/point-view-quick-fixes\n\nAnalyze GUI feedback wave: point-view fixes, GL 3D view, curve axis policy, refine removal, IA draft",
          "timestamp": "2026-11-09T11:55:28+08:00",
          "tree_id": "fc149583067480fda08cea0315fab79846de9db4",
          "url": "https://github.com/LoveDaisy/Lumice/commit/7b978e0d8758f5a30b6523d75426d9951a0277a6"
        },
        "date": 1794196751903,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 367234.1,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 505050.6,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 366462.1,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 370460.5,
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
          "id": "98838d24edddcd959cc61999ff6e4cefbc30f53d",
          "message": "Merge pull request #580 from LoveDaisy/feat/msvc-near-far-gate\n\nPolicy gate: reject near/far macro identifiers (MSVC C2513 class)",
          "timestamp": "2026-11-09T14:12:29+08:00",
          "tree_id": "4d670f320f1fd4f216d4f4321f3da209507ca563",
          "url": "https://github.com/LoveDaisy/Lumice/commit/98838d24edddcd959cc61999ff6e4cefbc30f53d"
        },
        "date": 1794205013117,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 348977.7,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 504247,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 360998.9,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 341885.8,
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
          "id": "6900ae9c918346564c9aa6d5c8a9ea0273e0e881",
          "message": "Merge pull request #581 from LoveDaisy/feat/home-win-closing-leg\n\nClosing gate: optional Windows build+ctest leg on home-win (test.sh quick)",
          "timestamp": "2026-11-09T15:56:10+08:00",
          "tree_id": "62b62ac09322caec461b5c0c165053b7a8bd1489",
          "url": "https://github.com/LoveDaisy/Lumice/commit/6900ae9c918346564c9aa6d5c8a9ea0273e0e881"
        },
        "date": 1794211166209,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 342060.2,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 505555.3,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 562491.8,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 9V45 96-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 368485.8,
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
          "id": "e0df6562454ca8358e50acf0a8b95517ddb84774",
          "message": "Merge pull request #582 from LoveDaisy/feat/analyze-ia-implementation\n\nAnalyze IA implementation: dual entries, L1-L5 layering, verdict anchors, Pose relayout (scrum 778)",
          "timestamp": "2026-11-11T02:18:48+08:00",
          "tree_id": "a98c639e8604a4f19de9b25f463f24311689ebfd",
          "url": "https://github.com/LoveDaisy/Lumice/commit/e0df6562454ca8358e50acf0a8b95517ddb84774"
        },
        "date": 1794334954761,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 301304.1,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 506186.6,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 367741.7,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 338580.4,
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
          "id": "c207e85cf26ec4a62529bac078bef0ffd2964f2d",
          "message": "Merge pull request #583 from LoveDaisy/feat/gate-metal-env-resilience\n\nfix(gate): metal leg env resilience - tolerant decode + build-tools pre-check",
          "timestamp": "2026-11-13T00:49:24+08:00",
          "tree_id": "a6b283103069e94aa0ec31d7665b93f3dd6ca6bd",
          "url": "https://github.com/LoveDaisy/Lumice/commit/c207e85cf26ec4a62529bac078bef0ffd2964f2d"
        },
        "date": 1794502413144,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 369815.1,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 505509.4,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 366172.5,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 339097.3,
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
          "id": "911161091416fcf33a80071ff156acac7f39d184",
          "message": "Merge pull request #585 from LoveDaisy/feat/analyze-gui-feedback-1112\n\nAnalyze GUI feedback wave 2026-11-12: five interaction/display fixes",
          "timestamp": "2026-11-13T20:09:47+08:00",
          "tree_id": "43ed226410e11ba6e79bffd7881dd2371767fe03",
          "url": "https://github.com/LoveDaisy/Lumice/commit/911161091416fcf33a80071ff156acac7f39d184"
        },
        "date": 1794572037390,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 289083.6,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 503290,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 526622.4,
            "unit": "rays/sec",
            "extra": "CPU: Intel(R) Xeon(R) 6975P-C\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 309276.1,
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
          "id": "6eeddf2babad4420037abe23b6cfb294c48fda62",
          "message": "Merge pull request #586 from LoveDaisy/feat/analyze-gui-feedback-1113\n\nAnalyze GUI fourth hands-on feedback wave: five fixes (display consistency, operation bar, camera presets, staleness unification, mechanism consolidation)",
          "timestamp": "2026-11-14T13:17:34+08:00",
          "tree_id": "9ed1355ded1c691c40e9d780f80509724780b42b",
          "url": "https://github.com/LoveDaisy/Lumice/commit/6eeddf2babad4420037abe23b6cfb294c48fda62"
        },
        "date": 1794633784177,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 316794.4,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 505479.9,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 529447.1,
            "unit": "rays/sec",
            "extra": "CPU: Intel(R) Xeon(R) 6975P-C\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 310091.5,
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
          "id": "3272248ebc43f184cbd0dcf977b98d987207c48b",
          "message": "Merge pull request #587 from LoveDaisy/feat/hover-readout-type-ownership\n\nrefactor(gui): return row hover types to their semantic owner (imgui-free header)",
          "timestamp": "2026-11-14T17:16:28+08:00",
          "tree_id": "a08d19bd6f3dace2676ed9b6204fbb778c85d09c",
          "url": "https://github.com/LoveDaisy/Lumice/commit/3272248ebc43f184cbd0dcf977b98d987207c48b"
        },
        "date": 1794648024570,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 336040.1,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 506932.3,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 366231.5,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 309234.3,
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
          "id": "f75491cc7628a703b19dbad2f0587bb74d0fd60a",
          "message": "Merge pull request #588 from LoveDaisy/feat/fiber-payload-refinement\n\nfeat(raypath): fiber payload assembly-layer adaptive refinement",
          "timestamp": "2026-11-14T22:59:32+08:00",
          "tree_id": "bdd20fcebfe0d0ac85c26ed74c2d564f18c0c496",
          "url": "https://github.com/LoveDaisy/Lumice/commit/f75491cc7628a703b19dbad2f0587bb74d0fd60a"
        },
        "date": 1794668857346,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 317079.5,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 505587.8,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 524757.4,
            "unit": "rays/sec",
            "extra": "CPU: Intel(R) Xeon(R) 6975P-C\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 306303.4,
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
          "id": "22c8b8d581dac91ed8fdad5009d0dc58e0726d99",
          "message": "Merge pull request #589 from LoveDaisy/feat/gui-full-suite-env-flake\n\nFix build.sh install starvation and fast-e2e false reds; record full-suite SIGILL evidence",
          "timestamp": "2026-11-15T09:02:37+08:00",
          "tree_id": "6d8404db8b58285fb9642b543d05b44054386bb3",
          "url": "https://github.com/LoveDaisy/Lumice/commit/22c8b8d581dac91ed8fdad5009d0dc58e0726d99"
        },
        "date": 1794704776018,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 364595,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 504833.9,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 523795.7,
            "unit": "rays/sec",
            "extra": "CPU: Intel(R) Xeon(R) 6975P-C\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 329056.6,
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
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "distinct": true,
          "id": "52d4760db8da0eab613a4e8821c28c9fbf22a470",
          "message": "Merge pull request #590 from LoveDaisy/chore/release-branch-policy",
          "timestamp": "2026-11-15T10:14:25+08:00",
          "tree_id": "9f1e3ddf0ccf449ee2e2af88e676f95a914da999",
          "url": "https://github.com/LoveDaisy/Lumice/commit/52d4760db8da0eab613a4e8821c28c9fbf22a470"
        },
        "date": 1794709104107,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 376043.5,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 504217.2,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 368227.5,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 325825,
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
          "id": "0be2a1d34f5f22b099bd62e5cff395eee4233514",
          "message": "Merge pull request #591 from LoveDaisy/feat/gui-feedback-1114\n\nfeat(gui-feedback-1114): overlay projection convergence, status bar, curve region rework, 3D reset, globe colormap",
          "timestamp": "2026-11-15T20:37:40+08:00",
          "tree_id": "ee20e2bcb9393d8730a0ad997abb40f1350ecc02",
          "url": "https://github.com/LoveDaisy/Lumice/commit/0be2a1d34f5f22b099bd62e5cff395eee4233514"
        },
        "date": 1794746563488,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 251527.1,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 505787.4,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 366625.8,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 511765.4,
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
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "distinct": true,
          "id": "ecca2c50a981551d3f7ace5b811ae02be3440f90",
          "message": "Merge pull request #591 from LoveDaisy/feat/gui-feedback-1114\n\nfeat(gui-feedback-1114): overlay projection convergence, status bar, curve region rework, 3D reset, globe colormap",
          "timestamp": "2026-11-15T22:10:19+08:00",
          "tree_id": "ee20e2bcb9393d8730a0ad997abb40f1350ecc02",
          "url": "https://github.com/LoveDaisy/Lumice/commit/ecca2c50a981551d3f7ace5b811ae02be3440f90"
        },
        "date": 1794752214601,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 328564.5,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 505044.9,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 531835.9,
            "unit": "rays/sec",
            "extra": "CPU: Intel(R) Xeon(R) 6975P-C\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 287215.8,
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
          "id": "a51a333e8551b7b442d82eafe5b20ffc655ac8b4",
          "message": "Merge pull request #593 from LoveDaisy/feat/sigill-ubsan-boundary-probe\n\ndocs(test): SIGILL sanitizer campaign evidence — blind samples, exit D maintained",
          "timestamp": "2026-11-16T02:36:27+08:00",
          "tree_id": "91940051fe9ac76f7b9fc26b9defb37cb0df3ad5",
          "url": "https://github.com/LoveDaisy/Lumice/commit/a51a333e8551b7b442d82eafe5b20ffc655ac8b4"
        },
        "date": 1794768019526,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 300219.9,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 505622.9,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 529498.1,
            "unit": "rays/sec",
            "extra": "CPU: Intel(R) Xeon(R) 6975P-C\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 364604.6,
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
          "id": "46f487f792dd5402b43549dd36dba9e3d1805a08",
          "message": "Merge pull request #595 from LoveDaisy/feat/gate-gui-real-timing-leg\n\nfeat(gate): add gui-real-timing leg to the local throughput gate",
          "timestamp": "2026-11-16T04:27:19+08:00",
          "tree_id": "c7cef6104878196142c2a0ae33a82a596024d99c",
          "url": "https://github.com/LoveDaisy/Lumice/commit/46f487f792dd5402b43549dd36dba9e3d1805a08"
        },
        "date": 1794774678832,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 326455.7,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 505416.8,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 581716.9,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 9V45 96-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 330131.9,
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
          "id": "5ecd6e9eb61b3eb52edd1c4c9527849b0a6316b9",
          "message": "Merge pull request #596 from LoveDaisy/feat/crystal-param-hash-signed-overflow\n\nfix(gui): move CrystalParamHash to uint32_t domain",
          "timestamp": "2026-11-16T05:53:54+08:00",
          "tree_id": "9e62e445f596c20f1ab5d717985d2d86536ae86e",
          "url": "https://github.com/LoveDaisy/Lumice/commit/5ecd6e9eb61b3eb52edd1c4c9527849b0a6316b9"
        },
        "date": 1794779921814,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 383901.5,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 505723.3,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 532847,
            "unit": "rays/sec",
            "extra": "CPU: Intel(R) Xeon(R) 6975P-C\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 304423,
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
          "id": "2cd2f085a930d16a2d25275a43cb4ede75694cbe",
          "message": "Merge pull request #597 from LoveDaisy/feat/lens-bump-projection-cast-ub\n\nfix(core): clamp single-lens bump cast inputs into int domain",
          "timestamp": "2026-11-16T07:33:03+08:00",
          "tree_id": "a6aef670adbec2df787761450a1d13f42ccfcd1d",
          "url": "https://github.com/LoveDaisy/Lumice/commit/2cd2f085a930d16a2d25275a43cb4ede75694cbe"
        },
        "date": 1794785801752,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 427362.6,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 504560.5,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 531648,
            "unit": "rays/sec",
            "extra": "CPU: Intel(R) Xeon(R) 6975P-C\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 335919.2,
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
          "id": "70305fd23cf25cb24d9987c106de61316b403a4f",
          "message": "Merge pull request #598 from LoveDaisy/feat/realtiming-pool-expectation-drift\n\ntest(gui): re-pin four real-timing pool expectations drifted by the 778/779/785 waves",
          "timestamp": "2026-11-16T09:40:03+08:00",
          "tree_id": "137c9cb2e06f9ff7ec70a69102e1af2531cdd335",
          "url": "https://github.com/LoveDaisy/Lumice/commit/70305fd23cf25cb24d9987c106de61316b403a4f"
        },
        "date": 1794793406268,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 446535.2,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 504508.8,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 423885.9,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 9V74 80-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 364260.1,
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
          "id": "29ce75c3d157f08d2b306aa99c3643c57102c5f6",
          "message": "Merge pull request #601 from LoveDaisy/feat/profile-rows-unstable-red\n\ntest(task-profile-rows-unstable-red): stabilize the profile_rows real-timing case's two flake shapes",
          "timestamp": "2026-11-16T15:35:55+08:00",
          "tree_id": "f72d4b690c2de7bacd14b8fe44ecd7950334e852",
          "url": "https://github.com/LoveDaisy/Lumice/commit/29ce75c3d157f08d2b306aa99c3643c57102c5f6"
        },
        "date": 1794814793391,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 399531.4,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 504167.6,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 546379.6,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 9V45 96-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 362028.5,
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
          "id": "f1a4ee0693e882f9731c1bb18fd73b97231f8ca6",
          "message": "Merge pull request #602 from LoveDaisy/port/476-user-fixes\n\nPort the 788 user-fix wave from release/4.7.6 to main (from #592)",
          "timestamp": "2026-11-16T19:16:24+08:00",
          "tree_id": "eccda5ef6ecd72f7e60c4a49d05573366185d167",
          "url": "https://github.com/LoveDaisy/Lumice/commit/f1a4ee0693e882f9731c1bb18fd73b97231f8ca6"
        },
        "date": 1794828022334,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 357095.3,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 503661.7,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 426739.2,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 9V74 80-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 308802.6,
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
          "id": "0d50cb0d867067018388289b90ba011d7f842853",
          "message": "Merge pull request #603 from LoveDaisy/port/476-feedback-1115\n\nfix(gui): port the 11-15 feedback wave to main — raypath baseline, row wash, tooltips, preview style default",
          "timestamp": "2026-11-16T21:27:17+08:00",
          "tree_id": "43578551cc6f0737a662e1b9d123abea33965b33",
          "url": "https://github.com/LoveDaisy/Lumice/commit/0d50cb0d867067018388289b90ba011d7f842853"
        },
        "date": 1794835965846,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 333474.3,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 503009.4,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 330081.3,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 9V74 80-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 306385.7,
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
          "id": "5150ba87256070aa13503b417e35208ec7ad7214",
          "message": "Merge pull request #604 from LoveDaisy/port/476-release-doc-amendment\n\ndocs(release): retire release branches via cherry-pick ports, not forward-merge",
          "timestamp": "2026-11-16T22:03:08+08:00",
          "tree_id": "30c74249e200d758d9dc20737846c06868c889f2",
          "url": "https://github.com/LoveDaisy/Lumice/commit/5150ba87256070aa13503b417e35208ec7ad7214"
        },
        "date": 1794838028944,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 335065.5,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 505548.5,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 413035.2,
            "unit": "rays/sec",
            "extra": "CPU: Intel(R) Xeon(R) Platinum 8370C CPU @ 2.80GHz\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 349832.4,
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
          "id": "78e6839c6629dff1eb07098252abc53f23228c3f",
          "message": "Merge pull request #614 from LoveDaisy/feat/gate-per-leg-idle-retry\n\nfeat(gate): per-leg idle retry with 06:30 nightly deadline wall",
          "timestamp": "2026-11-17T03:43:31+08:00",
          "tree_id": "a45e11ee656596171d7a83a357d1840524c9260a",
          "url": "https://github.com/LoveDaisy/Lumice/commit/78e6839c6629dff1eb07098252abc53f23228c3f"
        },
        "date": 1794858541769,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 371429.5,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 504962.7,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 590480,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 9V45 96-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 307124.3,
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
          "id": "263a21c315c2f636a6a584eb1a035d83ac189d61",
          "message": "Merge pull request #616 from LoveDaisy/feat/gate-ubsan-leg\n\nfeat(gate): add the ubsan leg - nightly UB probe gate",
          "timestamp": "2026-11-17T06:40:57+08:00",
          "tree_id": "aff4ceb10deea2418ebf7c4fc4169e692c50991c",
          "url": "https://github.com/LoveDaisy/Lumice/commit/263a21c315c2f636a6a584eb1a035d83ac189d61"
        },
        "date": 1794869062964,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 357109.5,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 504395.8,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 423265.9,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 9V74 80-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 484036.8,
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
          "id": "a8533ca7a68228cf2eed6d461ae87db2ea03f822",
          "message": "Merge pull request #617 from LoveDaisy/feat/gui-feedback-1117\n\nGUI feedback wave 2026-11-17: tooltip two-tier rule, Ray3D region rework, globe Fit/Full views",
          "timestamp": "2026-11-18T09:53:57+08:00",
          "tree_id": "8cb29f609a7727ad38280d90429697f273fe4803",
          "url": "https://github.com/LoveDaisy/Lumice/commit/a8533ca7a68228cf2eed6d461ae87db2ea03f822"
        },
        "date": 1794967138139,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 440208.8,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 505635.8,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 529395.5,
            "unit": "rays/sec",
            "extra": "CPU: Intel(R) Xeon(R) 6975P-C\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 307327.2,
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
          "id": "14c782141958dfa028fa7a884b6858d45753a67b",
          "message": "Merge pull request #618 from LoveDaisy/feat/gui-feedback-1118\n\nfix: address GUI raypath analysis feedback from November 18",
          "timestamp": "2026-11-19T19:03:16+08:00",
          "tree_id": "66ecc42f7e82780ba7e0c083dae012cfa0c90fa8",
          "url": "https://github.com/LoveDaisy/Lumice/commit/14c782141958dfa028fa7a884b6858d45753a67b"
        },
        "date": 1795086408103,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 285453,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 505742.5,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 527203.4,
            "unit": "rays/sec",
            "extra": "CPU: Intel(R) Xeon(R) 6975P-C\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 373265.4,
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
          "id": "5a0e7c13c527f94f0e179a4f54512b53016840db",
          "message": "Merge pull request #620 from LoveDaisy/feat/reference-regen-output-decoding\n\nfix(scripts): tolerate non-UTF-8 bytes in regen external output capture",
          "timestamp": "2026-11-20T13:09:18+08:00",
          "tree_id": "9a7772a7e2f1a3e6e153fee31daf6561459fa8fb",
          "url": "https://github.com/LoveDaisy/Lumice/commit/5a0e7c13c527f94f0e179a4f54512b53016840db"
        },
        "date": 1795151590726,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 333465.5,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 505642,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 367300.1,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 371430,
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
          "id": "22d80d01c1a1a170df818d7183cde33931a70edd",
          "message": "Merge pull request #621 from LoveDaisy/feat/axis-compact-scaled-chip-clip\n\nfix(gui): fit axis chip gaps to live pane width",
          "timestamp": "2026-11-20T20:31:36+08:00",
          "tree_id": "564b2fb47651ddd7a50c6e6f5982920937e0d49c",
          "url": "https://github.com/LoveDaisy/Lumice/commit/22d80d01c1a1a170df818d7183cde33931a70edd"
        },
        "date": 1795178112221,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 313274.5,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 506309,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 527818.1,
            "unit": "rays/sec",
            "extra": "CPU: Intel(R) Xeon(R) 6975P-C\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 489595.7,
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
          "id": "7febf6d10091602d355d0238a51937df978814df",
          "message": "Merge pull request #622 from LoveDaisy/feat/small-button-glyph-clip\n\nfix(gui): preserve SmallButton descender ink at scaled line height",
          "timestamp": "2026-11-21T06:50:01+08:00",
          "tree_id": "491d95044f79f070d1681aea38134ce614726abf",
          "url": "https://github.com/LoveDaisy/Lumice/commit/7febf6d10091602d355d0238a51937df978814df"
        },
        "date": 1795215392373,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 412580.8,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 504371.8,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 329370,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 9V74 80-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 509165.2,
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
          "id": "d685b0569327ac7aea6e1e534b775274ad0194e4",
          "message": "Merge pull request #623 from LoveDaisy/feat/metal-exit-wsum-main-port\n\nfix(metal): reduce exit weights per SIMD group (port from release/4.7.7)",
          "timestamp": "2026-11-21T07:08:10+08:00",
          "tree_id": "46894b9cf42864f233a143255b8be541dd2329de",
          "url": "https://github.com/LoveDaisy/Lumice/commit/d685b0569327ac7aea6e1e534b775274ad0194e4"
        },
        "date": 1795216375800,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 299205.3,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 505853.8,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 589606.8,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 9V45 96-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 313800.9,
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
          "id": "c398905ca960146043188906e46deeff6a25fc1d",
          "message": "Merge pull request #624 from LoveDaisy/feat/precommit-hook-delegator\n\nbuild(hooks): delegate pre-commit to the committing checkout",
          "timestamp": "2026-11-21T07:24:39+08:00",
          "tree_id": "445bde2ae1e12961c92c6fcc8706028c8e638ac5",
          "url": "https://github.com/LoveDaisy/Lumice/commit/c398905ca960146043188906e46deeff6a25fc1d"
        },
        "date": 1795217332502,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 356253.6,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 505012.1,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 407443.3,
            "unit": "rays/sec",
            "extra": "CPU: INTEL(R) XEON(R) PLATINUM 8573C\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 306567,
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
          "id": "02385c457d9f24d3cb2d29aa789aa52bad68f4ab",
          "message": "Merge pull request #625 from LoveDaisy/feat/expanded-column-width-fix\n\nfix(gui): scale Expanded column widths in layout units",
          "timestamp": "2026-11-21T07:45:27+08:00",
          "tree_id": "5f78a5bdffc3bb8aba0fae12b5f23b26efddb7b8",
          "url": "https://github.com/LoveDaisy/Lumice/commit/02385c457d9f24d3cb2d29aa789aa52bad68f4ab"
        },
        "date": 1795218517339,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 354433.3,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 506648.6,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 526161.6,
            "unit": "rays/sec",
            "extra": "CPU: Intel(R) Xeon(R) 6975P-C\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 434728.6,
            "unit": "rays/sec",
            "extra": "CPU: Intel(R) Xeon(R) 6973P-C\\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "a9b0149b1aa7bb53a1a0e521d99af42a3548f068",
          "message": "Merge pull request #626 from LoveDaisy/docs/gate-duration-disposition\n\ntest(budget): record the 2026-11-20/21 ctest disposition (reason-only)",
          "timestamp": "2026-11-21T08:04:03+08:00",
          "tree_id": "a142292d0432fc5976b070cfacacb6fbb7779e7c",
          "url": "https://github.com/LoveDaisy/Lumice/commit/a9b0149b1aa7bb53a1a0e521d99af42a3548f068"
        },
        "date": 1795219653017,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 328505,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 506290,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 522784.1,
            "unit": "rays/sec",
            "extra": "CPU: Intel(R) Xeon(R) 6975P-C\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 450249,
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
          "id": "2c7a6f417d3de05ec6e1adad2fb111874175abe7",
          "message": "Merge pull request #628 from LoveDaisy/feat/analyze-presentation-first-batch\n\nfeat(gui): analyze presentation first batch",
          "timestamp": "2026-11-23T09:50:08+08:00",
          "tree_id": "db72a497c926bbf8ae62b1b32b9a14f657839492",
          "url": "https://github.com/LoveDaisy/Lumice/commit/2c7a6f417d3de05ec6e1adad2fb111874175abe7"
        },
        "date": 1795398893702,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 292149.4,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 506319.1,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 525357.2,
            "unit": "rays/sec",
            "extra": "CPU: Intel(R) Xeon(R) 6975P-C\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 308595.7,
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
          "id": "0a1784dc996df84bf94c4ff774ba5f33d0560fac",
          "message": "Merge pull request #631 from LoveDaisy/feat/map-request-symmetry-bits\n\nfeat(capi,map): carry true symmetry bits in map requests",
          "timestamp": "2026-11-23T10:15:59+08:00",
          "tree_id": "662a5fd31b8a2a2fde9798f38adc9ad4e5401e8f",
          "url": "https://github.com/LoveDaisy/Lumice/commit/0a1784dc996df84bf94c4ff774ba5f33d0560fac"
        },
        "date": 1795400395998,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 335869.5,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 504974.4,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 394008.3,
            "unit": "rays/sec",
            "extra": "CPU: INTEL(R) XEON(R) PLATINUM 8573C\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 312873.8,
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
          "id": "1efc673fc1346637961b1cbf3f127abe1539fe48",
          "message": "Merge pull request #633 from LoveDaisy/chore/ci-release-push-trigger\n\nci: trigger the workflow on release/** pushes",
          "timestamp": "2026-11-23T11:52:05+08:00",
          "tree_id": "8ea5c55e318e2da81806571f958e790d6e2704c8",
          "url": "https://github.com/LoveDaisy/Lumice/commit/1efc673fc1346637961b1cbf3f127abe1539fe48"
        },
        "date": 1795406183346,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 316966.5,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 506924.1,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 524436.3,
            "unit": "rays/sec",
            "extra": "CPU: Intel(R) Xeon(R) 6975P-C\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 312669.7,
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
          "id": "349a4270504e39cc3c291c4257052477bfb36888",
          "message": "Merge pull request #636 from LoveDaisy/feat/local-class-algebra\n\nfeat(raypath,capi): local-class algebra — point-selection representatives, log density facts, interpretive chain",
          "timestamp": "2026-11-24T02:07:01+08:00",
          "tree_id": "594868057c65dc035af6aee5110e333dc189228c",
          "url": "https://github.com/LoveDaisy/Lumice/commit/349a4270504e39cc3c291c4257052477bfb36888"
        },
        "date": 1795457445543,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 384890.6,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 506877.8,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 531036.3,
            "unit": "rays/sec",
            "extra": "CPU: Intel(R) Xeon(R) 6975P-C\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 364569.3,
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
          "id": "8d161eba2c6c4dde9e29651ca26b4ba7e5bc8be4",
          "message": "Merge pull request #639 from LoveDaisy/feat/map-session-test-contract\n\ntest(gui): decouple map lifecycle cases from the full-budget compute",
          "timestamp": "2026-11-24T15:14:15+08:00",
          "tree_id": "b15b27b5a50961505b81cf5b4afd60f0895365f4",
          "url": "https://github.com/LoveDaisy/Lumice/commit/8d161eba2c6c4dde9e29651ca26b4ba7e5bc8be4"
        },
        "date": 1795504683126,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 334361.7,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 505514.1,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 360966.5,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 461275.1,
            "unit": "rays/sec",
            "extra": "CPU: Intel(R) Xeon(R) 6973P-C\\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "390c995b286acbd17618dd50ae2adb3b2498a024",
          "message": "Merge pull request #642 from LoveDaisy/feat/metal-gate-blocking-scope\n\nci(e2e-slow): Metal dual-renderer throughput ratio becomes a reported signal",
          "timestamp": "2026-11-24T16:18:36+08:00",
          "tree_id": "f4143a5d0a9e1378f579c84b5b9e1a36183c4021",
          "url": "https://github.com/LoveDaisy/Lumice/commit/390c995b286acbd17618dd50ae2adb3b2498a024"
        },
        "date": 1795508652564,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 301294.9,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 504168.2,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 525226.1,
            "unit": "rays/sec",
            "extra": "CPU: Intel(R) Xeon(R) 6975P-C\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 310720.1,
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
          "id": "73891fd38b1448ff47bc0a3976104785c9a48413",
          "message": "Merge pull request #646 from LoveDaisy/feat/ctest-headroom-tracker\n\nfeat(scripts): ctest headroom tracker — implied-C/headroom observation with streak signals",
          "timestamp": "2026-11-24T21:22:44+08:00",
          "tree_id": "3abfec62a61fd7542d9d5f18868b4bebfbea82c6",
          "url": "https://github.com/LoveDaisy/Lumice/commit/73891fd38b1448ff47bc0a3976104785c9a48413"
        },
        "date": 1795527801994,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 349550.8,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 505590.4,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 525173.6,
            "unit": "rays/sec",
            "extra": "CPU: Intel(R) Xeon(R) 6975P-C\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 350433.9,
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
          "id": "c8b8f111be3f7aa884289413e955ee9e54a3e1a4",
          "message": "Merge pull request #647 from LoveDaisy/port/477-infra\n\nPort release/4.7.7 infra cluster to main (#619 #627 #630 #632)",
          "timestamp": "2026-11-25T00:32:29+08:00",
          "tree_id": "907721e98226835ad1da12a6903ca476eb47deb4",
          "url": "https://github.com/LoveDaisy/Lumice/commit/c8b8f111be3f7aa884289413e955ee9e54a3e1a4"
        },
        "date": 1795539678478,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 419008.4,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 506233,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 525587.2,
            "unit": "rays/sec",
            "extra": "CPU: Intel(R) Xeon(R) 6975P-C\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 310977.3,
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
          "id": "af73a164c48b2e48472d2cbd4cb07bc215f6fa5d",
          "message": "Merge pull request #648 from LoveDaisy/port/477-interaction\n\nPort the 4.7.7 interaction/UI cluster to main (#635 #638 #641 #644)",
          "timestamp": "2026-11-25T05:25:15+08:00",
          "tree_id": "a92be6ef32b900df06a2ed42efdf111ed38b4a31",
          "url": "https://github.com/LoveDaisy/Lumice/commit/af73a164c48b2e48472d2cbd4cb07bc215f6fa5d"
        },
        "date": 1795556970810,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 387104.9,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 506737.1,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 557965.7,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 9V45 96-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 311461.2,
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
          "id": "183da9daba02fa2ee60fc32b50f0bf60412e6e5a",
          "message": "Merge pull request #649 from LoveDaisy/chore/release-477-main-backfill\n\ndocs(changelog): backfill 4.7.7 section",
          "timestamp": "2026-11-25T05:54:37+08:00",
          "tree_id": "9403fad84a0837cffad25ce35cb2ec8e7d0b86f7",
          "url": "https://github.com/LoveDaisy/Lumice/commit/183da9daba02fa2ee60fc32b50f0bf60412e6e5a"
        },
        "date": 1795558212844,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 402798.7,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 506194.7,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 527894.1,
            "unit": "rays/sec",
            "extra": "CPU: Intel(R) Xeon(R) 6975P-C\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 310334.8,
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
          "id": "ab73dccde5f9f36162d9599e10b674ec95e2a17a",
          "message": "Merge pull request #653 from LoveDaisy/feat/policy-gate-timebomb-fixture\n\ntest(scripts): decouple tracker policy-gate fixtures from the wall clock",
          "timestamp": "2026-11-26T03:04:35+08:00",
          "tree_id": "4a8e9b71faed5f6dd968776cdd6079439ec02f62",
          "url": "https://github.com/LoveDaisy/Lumice/commit/ab73dccde5f9f36162d9599e10b674ec95e2a17a"
        },
        "date": 1795634550395,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 336835.1,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 504959.6,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 426341.4,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 9V74 80-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 301324.3,
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
          "id": "846c30008635ac506d0664114c44b96142b87da4",
          "message": "Merge pull request #651 from LoveDaisy/feat/caustic-marker-declutter\n\nfix(analyze-map): declutter caustic markers with own layer group and pile aggregation",
          "timestamp": "2026-11-26T03:35:17+08:00",
          "tree_id": "d7afd09f3cd1e70dcff66b286135f5160cf3c1c4",
          "url": "https://github.com/LoveDaisy/Lumice/commit/846c30008635ac506d0664114c44b96142b87da4"
        },
        "date": 1795636851550,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 421005,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 505635.7,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 389696.8,
            "unit": "rays/sec",
            "extra": "CPU: INTEL(R) XEON(R) PLATINUM 8573C\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 465806.5,
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
          "id": "2075f95b8a98f5716d7e6792cb070ecfae282307",
          "message": "Merge pull request #655 from LoveDaisy/feat/raypath-layers-ui\n\nfeat(analyze-map): themed table replaces the bare layer switches",
          "timestamp": "2026-11-26T07:44:03+08:00",
          "tree_id": "c35f36bd1be756dc2a2d06b06323bb86c8b3c330",
          "url": "https://github.com/LoveDaisy/Lumice/commit/2075f95b8a98f5716d7e6792cb070ecfae282307"
        },
        "date": 1795650915493,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 406375.7,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 506946.3,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 457510,
            "unit": "rays/sec",
            "extra": "CPU: INTEL(R) XEON(R) PLATINUM 8573C\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 363813.2,
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
          "id": "b8fe6c8550932e7aff53ee851f4ba48b9ed93f40",
          "message": "Merge pull request #652 from LoveDaisy/feat/rho-deep-tail-curve\n\nfix(gui): rho curve reads log densities on an independent axis; adopt the interpretive branch",
          "timestamp": "2026-11-26T09:02:03+08:00",
          "tree_id": "22ed15e99f07f7620cb8b9a1a189970bf09e1edf",
          "url": "https://github.com/LoveDaisy/Lumice/commit/b8fe6c8550932e7aff53ee851f4ba48b9ed93f40"
        },
        "date": 1795656440597,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 287968.9,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 506414.6,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 525010.4,
            "unit": "rays/sec",
            "extra": "CPU: Intel(R) Xeon(R) 6975P-C\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 513020.3,
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
          "id": "6f2d696ad1743bef647f6647b1b87f3bdb7e276d",
          "message": "Merge pull request #658 from LoveDaisy/feat/parry-survey-nonterminal-hang\n\ntest(gui): single-source the survey partial-budget note (Parry survey hang verdict)",
          "timestamp": "2026-11-26T17:24:07+08:00",
          "tree_id": "5db2995bb95caa5a58cf49210337423e630afc4d",
          "url": "https://github.com/LoveDaisy/Lumice/commit/6f2d696ad1743bef647f6647b1b87f3bdb7e276d"
        },
        "date": 1795686113167,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 273754.4,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 505796.9,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 411700.6,
            "unit": "rays/sec",
            "extra": "CPU: Intel(R) Xeon(R) Platinum 8370C CPU @ 2.80GHz\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 360337.5,
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
          "id": "ade1b7284b5b65885cc0128475b9945a45d1cf42",
          "message": "Merge pull request #659 from LoveDaisy/feat/arc-recovery-engine\n\nfeat: ordered fold-arc recovery — the caustic cards become drawable, pickable arcs",
          "timestamp": "2026-11-26T23:28:25+08:00",
          "tree_id": "256b9d8575ae467edf24be35717238970dfd46e2",
          "url": "https://github.com/LoveDaisy/Lumice/commit/ade1b7284b5b65885cc0128475b9945a45d1cf42"
        },
        "date": 1795708848281,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 351922.2,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 504262.5,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 530511.3,
            "unit": "rays/sec",
            "extra": "CPU: Intel(R) Xeon(R) 6975P-C\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 365946.1,
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
          "id": "69d5e0e8d16b6a7134876bbf120530716abe9acc",
          "message": "Merge pull request #660 from LoveDaisy/goal/add-2d-j-0-caustic-continuation-and-posecurve-in\n\nfeat(analytic): add 2D J=0 caustic continuation and unified PoseCurve",
          "timestamp": "2026-11-27T20:18:13+08:00",
          "tree_id": "92cce1de4d6d2d6168871034b0d63f748257f900",
          "url": "https://github.com/LoveDaisy/Lumice/commit/69d5e0e8d16b6a7134876bbf120530716abe9acc"
        },
        "date": 1795782963928,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 338719.6,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 502350.4,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 516676.1,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 9V45 96-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 310139.6,
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
          "id": "db80d09847e73993c42c92dd86a70188c8154bf8",
          "message": "Merge pull request #661 from LoveDaisy/goal/refactor-caustic-continuation-to-o-1-residual-co\n\nrefactor(analytic): switch caustic continuation to O(1) J_reg contract and split modules",
          "timestamp": "2026-11-28T02:30:54+08:00",
          "tree_id": "7d2b9b61cdb956630a349cb546570bdbeeea9683",
          "url": "https://github.com/LoveDaisy/Lumice/commit/db80d09847e73993c42c92dd86a70188c8154bf8"
        },
        "date": 1795805357551,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 303723.7,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 504326.3,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 531842.2,
            "unit": "rays/sec",
            "extra": "CPU: Intel(R) Xeon(R) 6975P-C\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 311891,
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
          "id": "6d2a8a79915f62a319ee93debc11f4f69a027dc5",
          "message": "Merge pull request #663 from LoveDaisy/chore/duration-budget-unblock\n\nRaise ctest phase budgets: macOS 560->660, Ubuntu x86_64 360->420 (transitional)",
          "timestamp": "2026-11-28T12:43:40+08:00",
          "tree_id": "0b672b4dc85982dc426c9700348a861ed3cb42c9",
          "url": "https://github.com/LoveDaisy/Lumice/commit/6d2a8a79915f62a319ee93debc11f4f69a027dc5"
        },
        "date": 1795842337135,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 410656.3,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 503933.2,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 525473.2,
            "unit": "rays/sec",
            "extra": "CPU: Intel(R) Xeon(R) 6975P-C\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 329193.8,
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
          "id": "f6818f15b3c4692f2442f1cc67388528deb0556a",
          "message": "Merge pull request #662 from LoveDaisy/goal/add-pathintrinsic-cached-per-path-intrinsic-obje\n\nAdd PathIntrinsic: cached per-path intrinsic object over verified analytic assets",
          "timestamp": "2026-11-28T13:10:01+08:00",
          "tree_id": "3f13d52cd70cf6532691fe2533e37147bd73f5bb",
          "url": "https://github.com/LoveDaisy/Lumice/commit/f6818f15b3c4692f2442f1cc67388528deb0556a"
        },
        "date": 1795843708963,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 329446.5,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 506139.4,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 422982.4,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 9V74 80-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 311135.6,
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
          "id": "45ae9033274b82965bdb6ce727f413a1ee80ac2d",
          "message": "Merge pull request #664 from LoveDaisy/goal/add-axisposedensity-exact-pose-density-for-all-a\n\nAdd AxisPoseDensity: exact pose density for all AxisDistribution types",
          "timestamp": "2026-11-28T13:44:48+08:00",
          "tree_id": "87de178a912fdc0dea12eaf3ddc139b17c8b8606",
          "url": "https://github.com/LoveDaisy/Lumice/commit/45ae9033274b82965bdb6ce727f413a1ee80ac2d"
        },
        "date": 1795845378235,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 287598.9,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 505617.2,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 364297.1,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 371250.8,
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
          "id": "7522f1f29792ff614c719977648b7540d06fe56d",
          "message": "Merge pull request #665 from LoveDaisy/goal/add-skyblurfield-pose-covariance-pushforward-bri\n\nAdd SkyBlurField: pose-covariance pushforward bridging distribution and skeleton",
          "timestamp": "2026-11-28T14:59:02+08:00",
          "tree_id": "e0da2beaa3fd78cd6344e699f682e3c8f854163f",
          "url": "https://github.com/LoveDaisy/Lumice/commit/7522f1f29792ff614c719977648b7540d06fe56d"
        },
        "date": 1795850385284,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 318795.5,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 506929,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 529632.9,
            "unit": "rays/sec",
            "extra": "CPU: Intel(R) Xeon(R) 6975P-C\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 312563.3,
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
          "id": "725018d33ad9a3d08ded65180c4f2d3272581cd7",
          "message": "Merge pull request #531 from LoveDaisy/feat/free-dims-enum-cleanup\n\nrefactor(raypath): free_dims carries the dim enum",
          "timestamp": "2026-10-21T00:02:56+08:00",
          "tree_id": "280b8049e2f76e79f05bd54c0b9bc03c21e48818",
          "url": "https://github.com/LoveDaisy/Lumice/commit/725018d33ad9a3d08ded65180c4f2d3272581cd7"
        },
        "date": 1792512394034,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 874827.7,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1012568,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 720641.7,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 537662.6,
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
          "id": "a7f8b4eaab29ae7dd4fb9f70c7b10040a958550d",
          "message": "Merge pull request #533 from LoveDaisy/feat/ci-ctest-budget-rebalance\n\ntest(ci): zero-sum ctest budget rebalance + the four survey structure cleanups",
          "timestamp": "2026-10-21T20:09:47+08:00",
          "tree_id": "d1414e4f2c68d617f42b63835e7c89df495db972",
          "url": "https://github.com/LoveDaisy/Lumice/commit/a7f8b4eaab29ae7dd4fb9f70c7b10040a958550d"
        },
        "date": 1792584809404,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 838687.5,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1007720.1,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 808698.5,
            "unit": "rays/sec",
            "extra": "CPU: Intel(R) Xeon(R) Platinum 8370C CPU @ 2.80GHz\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 682289.3,
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
          "id": "e3acdfcb46e5115e09897dc2a33218a679a73714",
          "message": "Merge pull request #534 from LoveDaisy/feat/c10-ladder-gate-order\n\nfix(raypath): the level-0 partition refusal scopes to its own card",
          "timestamp": "2026-10-21T21:59:06+08:00",
          "tree_id": "0dd10a9ed7c0e3fba17f416a6d2095a87d6673a6",
          "url": "https://github.com/LoveDaisy/Lumice/commit/e3acdfcb46e5115e09897dc2a33218a679a73714"
        },
        "date": 1792591365186,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 772325,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1008661,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 723334.7,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 574320.1,
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
          "id": "1792d962467839be845bd5577eabc039164cf440",
          "message": "Merge pull request #536 from LoveDaisy/feat/cuda-continue-legs\n\ntest(server): land the four CUDA continuation legs from the reference-box probe",
          "timestamp": "2026-10-21T22:52:57+08:00",
          "tree_id": "d08f791614a40b3b1163ff40f6dea1e31ff11104",
          "url": "https://github.com/LoveDaisy/Lumice/commit/1792d962467839be845bd5577eabc039164cf440"
        },
        "date": 1792594576083,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 1131263.8,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1009794.2,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 769407.9,
            "unit": "rays/sec",
            "extra": "CPU: INTEL(R) XEON(R) PLATINUM 8573C\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 887406.1,
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
          "id": "3a789961cdb423ef5ea1b5b7ec0d5899e3187166",
          "message": "Merge pull request #535 from LoveDaisy/feat/threshold-single-source\n\nrefactor(gui-test): thresholds resolve from _thresholds.json at run time",
          "timestamp": "2026-10-21T23:46:17+08:00",
          "tree_id": "57b1ca93a4d41a1cfb107f290788ddda5f971e59",
          "url": "https://github.com/LoveDaisy/Lumice/commit/3a789961cdb423ef5ea1b5b7ec0d5899e3187166"
        },
        "date": 1792597782200,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 1231242.5,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1010648,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 721537.4,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 699330.6,
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
          "id": "49620f6e2374ecef84e0cec7c546028623c60493",
          "message": "Merge pull request #537 from LoveDaisy/feat/gap3-tir-ess-probes-c18\n\nfeat(raypath): TIR share + opt-in MC region ESS evidence legs (gap ③)",
          "timestamp": "2026-10-22T14:58:23+08:00",
          "tree_id": "62674616b336864170d87cd4568ecea21be722b6",
          "url": "https://github.com/LoveDaisy/Lumice/commit/49620f6e2374ecef84e0cec7c546028623c60493"
        },
        "date": 1792652515660,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 1030527.9,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1007753.2,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 798903.8,
            "unit": "rays/sec",
            "extra": "CPU: Intel(R) Xeon(R) Platinum 8370C CPU @ 2.80GHz\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 582108.7,
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
          "id": "b7910bd93417b149f483132e534f1f50f44153c2",
          "message": "Merge pull request #538 from LoveDaisy/feat/raypath-point-l2-warm\n\nfeat(raypath): the L2 point command — factor decomposition + the --warm index contract",
          "timestamp": "2026-10-22T23:20:21+08:00",
          "tree_id": "4041bf027823738c4e184bac29559cb4397a5908",
          "url": "https://github.com/LoveDaisy/Lumice/commit/b7910bd93417b149f483132e534f1f50f44153c2"
        },
        "date": 1792682635340,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 846592.7,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1013352.8,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 1079912.5,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 9V45 96-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 897133.4,
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
          "id": "272c3e0749d0ff9744a8ebc02f71b4ce04dbf205",
          "message": "Merge pull request #539 from LoveDaisy/feat/raypath-probe-profiles\n\nfeat(raypath): the L2 probe command — four --along forms on the shared line core",
          "timestamp": "2026-10-23T06:59:16+08:00",
          "tree_id": "2dcbad7a3a36b56e1c3c0d192a8839bba8dd9b45",
          "url": "https://github.com/LoveDaisy/Lumice/commit/272c3e0749d0ff9744a8ebc02f71b4ce04dbf205"
        },
        "date": 1792710178194,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 1073857.5,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1014600.2,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 725903.8,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 579669,
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
          "id": "3a00c4a7ac2656f50f20eb2f80c60fd77fda7205",
          "message": "Merge pull request #541 from LoveDaisy/feat/ctest-machine-normalized-budget\n\nfeat(test-gate): machine-speed normalization for ctest phase budgets (registry v4)",
          "timestamp": "2026-10-23T13:27:12+08:00",
          "tree_id": "3fb16cb4a4390a3797a2d66b5a9aa36add0388b9",
          "url": "https://github.com/LoveDaisy/Lumice/commit/3a00c4a7ac2656f50f20eb2f80c60fd77fda7205"
        },
        "date": 1792733481470,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 1111452,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1014565.9,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 725544.9,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 915131.6,
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
          "id": "76fc7455b0fe29c44ca2c36c777f7cf3d88bbf16",
          "message": "Merge pull request #540 from LoveDaisy/feat/gap2-subgroup-gating\n\nfeat(core): physical P gating as a per-element retained-rotation set (gap ②)",
          "timestamp": "2026-10-23T13:58:39+08:00",
          "tree_id": "344d73644fead342651a339dcc81d442de596644",
          "url": "https://github.com/LoveDaisy/Lumice/commit/76fc7455b0fe29c44ca2c36c777f7cf3d88bbf16"
        },
        "date": 1792735760965,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 859736.9,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1012714.5,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 1038611.6,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 9V45 96-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 581681.1,
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
          "id": "4ebb977073157d94978c2dd51509ed2006597e23",
          "message": "Merge pull request #542 from LoveDaisy/feat/testing-inner-loop\n\nfeat(test): the inner scope — diff-derived layers for the iteration loop",
          "timestamp": "2026-10-23T14:32:36+08:00",
          "tree_id": "08016163a6e1869c1c3db2730a08078ddbe1e0dc",
          "url": "https://github.com/LoveDaisy/Lumice/commit/4ebb977073157d94978c2dd51509ed2006597e23"
        },
        "date": 1792737383624,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 1083236.3,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1011606.9,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 1076735.5,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 9V45 96-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 581445.9,
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
          "id": "44446dcf5082221f38943bb7ef5f2a4bb89c7240",
          "message": "Merge pull request #543 from LoveDaisy/feat/survey-wave-review-sweep\n\nfix(core): zero-width pole-centred gluing excluded; the wave's review sweep (task-735)",
          "timestamp": "2026-10-23T20:19:32+08:00",
          "tree_id": "ea55d5da85b9ea01bace2af47e90705d3526c028",
          "url": "https://github.com/LoveDaisy/Lumice/commit/44446dcf5082221f38943bb7ef5f2a4bb89c7240"
        },
        "date": 1792758214274,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 1095782.4,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1010775.9,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 725107,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 928308.6,
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
          "id": "61686ea7e253835425093a7964672997f4333dd5",
          "message": "Merge pull request #544 from LoveDaisy/chore/release-4.7.5\n\nchore(release): cut 4.7.5",
          "timestamp": "2026-10-23T21:28:21+08:00",
          "tree_id": "db9017bd4b326dbf208009eade13622c90e7b9b6",
          "url": "https://github.com/LoveDaisy/Lumice/commit/61686ea7e253835425093a7964672997f4333dd5"
        },
        "date": 1792762294912,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 877904.2,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1007012.2,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 855790.9,
            "unit": "rays/sec",
            "extra": "CPU: INTEL(R) XEON(R) PLATINUM 8573C\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 900146,
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
          "id": "f6b77fdab11fb6bc88f63fcb794f68de73f690ea",
          "message": "Merge pull request #545 from LoveDaisy/feat/raypath-warm-structure-scope\n\nfix(raypath): scope warm numeric facts to their producing wavelength",
          "timestamp": "2026-10-24T15:57:44+08:00",
          "tree_id": "4f5fec9f158d5d80e6cdc8ff173a351e2c42d8c7",
          "url": "https://github.com/LoveDaisy/Lumice/commit/f6b77fdab11fb6bc88f63fcb794f68de73f690ea"
        },
        "date": 1792828982926,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 660083.8,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1011916.2,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 728429.8,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 588041.5,
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
          "id": "204775d4b7b287915b6fe3941773c1eb8df34d19",
          "message": "Merge pull request #547 from LoveDaisy/feat/raypath-typed-architecture\n\nrefactor(raypath): complete typed-first query and C API architecture",
          "timestamp": "2026-10-27T02:50:35+08:00",
          "tree_id": "3a08be3734802675a46cfc191fef7514cdae32c2",
          "url": "https://github.com/LoveDaisy/Lumice/commit/204775d4b7b287915b6fe3941773c1eb8df34d19"
        },
        "date": 1793040876237,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 1112906.8,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1010265.4,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 722069,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 581417.7,
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
          "id": "696685bc25d5ef49575143b12f37744b2519944d",
          "message": "Merge pull request #548 from LoveDaisy/feat/crystal-card-text-hit-overlap\n\nFix crystal card text probes stealing button clicks",
          "timestamp": "2026-10-27T22:36:07+08:00",
          "tree_id": "e128f7f0b209745c23eb487980fd9d05b97eff32",
          "url": "https://github.com/LoveDaisy/Lumice/commit/696685bc25d5ef49575143b12f37744b2519944d"
        },
        "date": 1793112025721,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 845490.2,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1005593.1,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 723906.8,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 583131.5,
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
          "id": "4457f2a77b4c98815d730e5d030e079f192487d0",
          "message": "Merge pull request #549 from LoveDaisy/feat/test-iteration-speed\n\nperf(test): shorten iteration feedback with dedup and sharding",
          "timestamp": "2026-10-28T19:24:30+08:00",
          "tree_id": "65aaff66a0a607a3199b30ac098be2cec79ab778",
          "url": "https://github.com/LoveDaisy/Lumice/commit/4457f2a77b4c98815d730e5d030e079f192487d0"
        },
        "date": 1793186904789,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 923084.1,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1010161,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 723101,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 580904.4,
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
          "id": "82acb1c7506066de7efea14f2991f45b483e0e0a",
          "message": "Merge pull request #550 from LoveDaisy/feat/raypath-docs-promotion\n\ndocs(raypath): promote diagnostic pipeline design and refresh the roadmap",
          "timestamp": "2026-10-30T01:21:16+08:00",
          "tree_id": "f2649338d9f7df1cc633beb6c3fe07372356de44",
          "url": "https://github.com/LoveDaisy/Lumice/commit/82acb1c7506066de7efea14f2991f45b483e0e0a"
        },
        "date": 1793294734407,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 887887.1,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1005530.7,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 1142912.4,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 9V45 96-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 580794.7,
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
          "id": "b370f17aca80a388068584dd0e209daaf92dc0ba",
          "message": "Merge pull request #551 from LoveDaisy/feat/analyze-gui-v1\n\nfeat: Analyze GUI v1 — class reference maps, representative rays, layered overview, dispersion, map & point inspection",
          "timestamp": "2026-10-31T08:49:46+08:00",
          "tree_id": "23dabe19bd569422f87f8a4d312ff63f7fd70664",
          "url": "https://github.com/LoveDaisy/Lumice/commit/b370f17aca80a388068584dd0e209daaf92dc0ba"
        },
        "date": 1793408047056,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 1102352.9,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1006588.3,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 839518.6,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 9V74 80-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 574935.6,
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
          "id": "a92d037c5dc3459b9668540e36cc255c65b8c7b2",
          "message": "Merge pull request #552 from LoveDaisy/feat/realtime-legs-placement\n\ntest: reclaim the one analyze CI row that actually pays wall-clock — real-timing legs never ran in PR ctest",
          "timestamp": "2026-10-31T13:23:32+08:00",
          "tree_id": "ad4451ec4d84a542a8ea46928ecb4229d88297e5",
          "url": "https://github.com/LoveDaisy/Lumice/commit/a92d037c5dc3459b9668540e36cc255c65b8c7b2"
        },
        "date": 1793424424497,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 1111655,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1014509.9,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 1140867.6,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 9V45 96-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 612729.3,
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
          "id": "71ee8904b6b1a70d8ea0b90f1c3ae44e0108792f",
          "message": "Merge pull request #553 from LoveDaisy/feat/panorama-drift-fix\n\nfix(gui): the analyze panorama published OS-screen coords as framebuffer coords — texture drifted with the window",
          "timestamp": "2026-10-31T15:01:13+08:00",
          "tree_id": "007cf6a260e6236bc6eec33270bf4f1ad7915f42",
          "url": "https://github.com/LoveDaisy/Lumice/commit/71ee8904b6b1a70d8ea0b90f1c3ae44e0108792f"
        },
        "date": 1793430311555,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 944296,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1011945.4,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 718480.6,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 670363.4,
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
          "id": "f64a77d8de746d0479d5ec85665e23b629b57fea",
          "message": "Merge pull request #554 from LoveDaisy/feat/analyze-panel-clip-fix\n\nfix(analyze): three scroll regions clipped their headers — measured reserves, a table that absorbs, and an 8pt strip ImGui secretly sized to 32pt",
          "timestamp": "2026-10-31T18:04:58+08:00",
          "tree_id": "440d8445970baae5ba4c237d591750c937060670",
          "url": "https://github.com/LoveDaisy/Lumice/commit/f64a77d8de746d0479d5ec85665e23b629b57fea"
        },
        "date": 1793441345710,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 1129413.3,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1012367.7,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 1139574.3,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 9V45 96-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 591582.2,
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
          "id": "0cda0c24a2505274ce88b5c332db83b11bf9e08b",
          "message": "Merge pull request #555 from LoveDaisy/feat/ray3d-orbit-grab-fix\n\nfix(gui): the ray3d orbit basis had pitch and yaw slots transposed — axes swapped and the drag metaphor scrambled",
          "timestamp": "2026-10-31T18:30:37+08:00",
          "tree_id": "9a32ca08414fc7310da15fb49a95dcd6668e8f8e",
          "url": "https://github.com/LoveDaisy/Lumice/commit/0cda0c24a2505274ce88b5c332db83b11bf9e08b"
        },
        "date": 1793442967585,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 923768.9,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1011355.4,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 720060.9,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 588981,
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
          "id": "284c9455d369800eb8b70405d488abe377dbf75c",
          "message": "Merge pull request #556 from LoveDaisy/feat/analyze-wait-feedback-a\n\nfeat(analyze): the wait says how long it has waited — per-face elapsed lines, a status-bar Analyzing segment, row activity dots",
          "timestamp": "2026-10-31T19:08:56+08:00",
          "tree_id": "7d66f431a427bc186d7e1e8075afa021ede499f2",
          "url": "https://github.com/LoveDaisy/Lumice/commit/284c9455d369800eb8b70405d488abe377dbf75c"
        },
        "date": 1793445166281,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 1075723.8,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1007156,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 712251.8,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 562647.9,
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
          "id": "ed0062b7978aff4024c97d61b3eaf193332a0ce7",
          "message": "Merge pull request #557 from LoveDaisy/feat/witness-tir-deadline\n\nfix(survey): the 15s deadline now bites inside the witness and TIR legs — cooperative polls at batch boundaries",
          "timestamp": "2026-10-31T22:25:04+08:00",
          "tree_id": "7a451064b6104c5005007f3631ff8bf19bfb1077",
          "url": "https://github.com/LoveDaisy/Lumice/commit/ed0062b7978aff4024c97d61b3eaf193332a0ce7"
        },
        "date": 1793456947966,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 918626.7,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1012235.3,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 841193,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 9V74 80-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 582232.4,
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
          "id": "33934bd7de38458727db064be595da31a2314160",
          "message": "Merge pull request #558 from LoveDaisy/feat/topbar-toggle-regroup\n\nfeat(gui): the workspace switch becomes its own segmented group — and the top bar's groups get room to breathe",
          "timestamp": "2026-10-31T22:51:20+08:00",
          "tree_id": "43b7e4bb1b41920e5806a9a565ed409eb8669ac6",
          "url": "https://github.com/LoveDaisy/Lumice/commit/33934bd7de38458727db064be595da31a2314160"
        },
        "date": 1793458535648,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 818783.7,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1010097.8,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 712650.6,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 949375.3,
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
          "id": "a6c9ed30e3b546ecded6de86bb35045ee50ca793",
          "message": "Merge pull request #559 from LoveDaisy/feat/query-stage-heartbeat-abi\n\nfeat: poll-stage heartbeat over the cancel token (C ABI v4.61)",
          "timestamp": "2026-11-01T03:11:41+08:00",
          "tree_id": "eef80cb7afd5f8eff4f733b0af4b7b1569895512",
          "url": "https://github.com/LoveDaisy/Lumice/commit/a6c9ed30e3b546ecded6de86bb35045ee50ca793"
        },
        "date": 1793474155184,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 943173.1,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1009446.8,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 723081.7,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 582899.1,
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
          "id": "dabd3f70e5c64ffd45b2e93832dcdd02e1fe19fd",
          "message": "Merge pull request #560 from LoveDaisy/fix/analyze-reanalyze-survey-validation-blocker\n\nfix(gui): synchronize analysis reselection and quiesce teardown",
          "timestamp": "2026-11-03T03:49:26+08:00",
          "tree_id": "e1322da05e898511f4110603b9c181c936522363",
          "url": "https://github.com/LoveDaisy/Lumice/commit/dabd3f70e5c64ffd45b2e93832dcdd02e1fe19fd"
        },
        "date": 1793649329100,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 1043443.9,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1008083.9,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 719651.4,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 578883.3,
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
          "id": "9930c9a89fd804e79320f1faa457c33a994c3839",
          "message": "Merge pull request #562 from LoveDaisy/feat/msvc-math-macro-gate\n\nfix(policy): reject MSVC-unsafe math constant macros",
          "timestamp": "2026-11-03T04:13:23+08:00",
          "tree_id": "84c5c41207c1680acdcccc3a71c06f77fc6b7652",
          "url": "https://github.com/LoveDaisy/Lumice/commit/9930c9a89fd804e79320f1faa457c33a994c3839"
        },
        "date": 1793650652716,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 1169123.6,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1009541.9,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 726050.1,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 549892.4,
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
          "id": "3aadae00161f1b38193db5fd3c7ac5f4535f59b3",
          "message": "Merge pull request #563 from LoveDaisy/feat/fiber-arclength-payload-probe\n\ndocs(raypath): fiber arclength payload audit for contribution curves",
          "timestamp": "2026-11-04T04:59:13+08:00",
          "tree_id": "e5adef887341388bc5d87629182c19bfa3d0f278",
          "url": "https://github.com/LoveDaisy/Lumice/commit/3aadae00161f1b38193db5fd3c7ac5f4535f59b3"
        },
        "date": 1793739840589,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 925757.1,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1010061.7,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 651915.8,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 9V74 80-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 576931.2,
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
          "id": "5f0968259b932181425988ebdb95fc4a422442bb",
          "message": "Merge pull request #564 from LoveDaisy/feat/analyze-gui-rework\n\nAnalyze GUI rework: task-driven main view, resizable regions, independent windows",
          "timestamp": "2026-11-06T04:38:13+08:00",
          "tree_id": "6d70ce4fe8476d0d71d3a15141a3c4e9c8693a71",
          "url": "https://github.com/LoveDaisy/Lumice/commit/5f0968259b932181425988ebdb95fc4a422442bb"
        },
        "date": 1793911370218,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 935210.1,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1010225.3,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 720751.5,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 534396.5,
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
          "id": "3fa8e2cd61fcb3352f6adf6311731e91e2b30109",
          "message": "Merge pull request #565 from LoveDaisy/feat/executor-ordering-fixture\n\ntest(gui): executor ordering contract arms — supersede refusal, shutdown join, newest-wins",
          "timestamp": "2026-11-06T06:41:41+08:00",
          "tree_id": "ae2aea9c76fbf464eefce3a7536d023941059f41",
          "url": "https://github.com/LoveDaisy/Lumice/commit/3fa8e2cd61fcb3352f6adf6311731e91e2b30109"
        },
        "date": 1793918794190,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 1066488.4,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1013738.9,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 721119.6,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 625992.1,
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
          "id": "5b630fe90d2117f664e2d37c0c201b6409149997",
          "message": "Merge pull request #566 from LoveDaisy/feat/analyze-fiber-contribution-curve\n\nfeat(gui): contribution curve — named factors and product over true SO(3) arclength",
          "timestamp": "2026-11-06T12:10:06+08:00",
          "tree_id": "58873d41ff72f12c505e1cf4e4be4662d61ef2c7",
          "url": "https://github.com/LoveDaisy/Lumice/commit/5b630fe90d2117f664e2d37c0c201b6409149997"
        },
        "date": 1793938438646,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 1127765.5,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1005408.4,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 845292.6,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 9V74 80-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 610629.2,
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
          "id": "9e6d278e623a50ba610259478331d5ee90c34408",
          "message": "Merge pull request #567 from LoveDaisy/feat/analysis-pump-sky-intent-pickup\n\ntest(gui): the map wait streaks, the Set input asserts, and broken preconditions fail fast",
          "timestamp": "2026-11-06T13:55:02+08:00",
          "tree_id": "8aa0628662045bfe715965f94e9b5fa16ad7c406",
          "url": "https://github.com/LoveDaisy/Lumice/commit/9e6d278e623a50ba610259478331d5ee90c34408"
        },
        "date": 1793944745460,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 1175191.4,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1011516.1,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 722800.7,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 590581.9,
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
          "id": "c1149fe20a40b4a978bc47b42a60b971ac3f9f5d",
          "message": "Merge pull request #568 from LoveDaisy/feat/scopes-gate-load-budget\n\ntest(gui): scopes gates judge recorded edges, hold in-flight actions, and drain the whole board",
          "timestamp": "2026-11-06T14:25:57+08:00",
          "tree_id": "f625921baa7c96bf5f98fc7163267e9f1701e8ac",
          "url": "https://github.com/LoveDaisy/Lumice/commit/c1149fe20a40b4a978bc47b42a60b971ac3f9f5d"
        },
        "date": 1793946581938,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 850199.8,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1010595.4,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 712777.5,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 704956.7,
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
          "id": "da8dbd629fdc05ea5ef7dc6ed218130409569f00",
          "message": "Merge pull request #569 from LoveDaisy/feat/analyze-main-direct-gestures\n\nfix(gui): direct point gestures and shared main-view zoom",
          "timestamp": "2026-11-06T20:51:42+08:00",
          "tree_id": "1034a6db0ddbe12f966cb71f61d31b197e9a04f3",
          "url": "https://github.com/LoveDaisy/Lumice/commit/da8dbd629fdc05ea5ef7dc6ed218130409569f00"
        },
        "date": 1793969713348,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 915487.1,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1009743.3,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 767141.9,
            "unit": "rays/sec",
            "extra": "CPU: INTEL(R) XEON(R) PLATINUM 8573C\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 702449.4,
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
          "id": "cf9972947e96e67045d69b96f68799a1d2823ef7",
          "message": "Merge pull request #570 from LoveDaisy/fix/cancel-survey-observation-scope\n\nfix(test): observe survey cancellation without global idle",
          "timestamp": "2026-11-06T23:31:50+08:00",
          "tree_id": "c2ea747b7eccb4fe178fdbcf931fe9d696c2bf9e",
          "url": "https://github.com/LoveDaisy/Lumice/commit/cf9972947e96e67045d69b96f68799a1d2823ef7"
        },
        "date": 1793979342194,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 805782.3,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1009236.6,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 801753.9,
            "unit": "rays/sec",
            "extra": "CPU: Intel(R) Xeon(R) Platinum 8370C CPU @ 2.80GHz\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 661321.6,
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
          "id": "54df3277d002f0ba3e76f30952c881890c7d37e2",
          "message": "Merge pull request #571 from LoveDaisy/feat/font-glyph-lambda-range\n\nfix(gui): load missing delta and lambda glyphs",
          "timestamp": "2026-11-07T01:09:00+08:00",
          "tree_id": "ac9a9b1bfa0453c3aa3be2ae737295fa67cd70bb",
          "url": "https://github.com/LoveDaisy/Lumice/commit/54df3277d002f0ba3e76f30952c881890c7d37e2"
        },
        "date": 1793985169945,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 944343.3,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1007794.5,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 721610.6,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 891552.3,
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
          "id": "347a4e5c5821ff1a77061706e0e80a16f985bafc",
          "message": "Merge pull request #572 from LoveDaisy/test/map-pump-fixture-query-isolation\n\ntest(gui): remove unrelated survey work from map fixtures",
          "timestamp": "2026-11-07T02:39:25+08:00",
          "tree_id": "17372e67756c5e4a76ebad72e1ecba46bbf68e83",
          "url": "https://github.com/LoveDaisy/Lumice/commit/347a4e5c5821ff1a77061706e0e80a16f985bafc"
        },
        "date": 1793990604896,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 1075809.5,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1010253.2,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 843638.4,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 9V74 80-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 585623.3,
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
          "id": "3d2c4f05d9f464d164acf08e609a75a356daeb49",
          "message": "Merge pull request #573 from LoveDaisy/feat/analyze-layout-stability\n\nfix(gui): stabilize Analyze canvases and transient status layout",
          "timestamp": "2026-11-07T04:37:00+08:00",
          "tree_id": "2400774c1f61f029a64f97fb2880a3281f427dc2",
          "url": "https://github.com/LoveDaisy/Lumice/commit/3d2c4f05d9f464d164acf08e609a75a356daeb49"
        },
        "date": 1793997666869,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 913958,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1013091,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 1087865.4,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 9V45 96-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 583737.2,
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
          "id": "824e978f0926f336217b4856d55086907fca8aa5",
          "message": "Merge pull request #574 from LoveDaisy/feat/doc-switch-keeps-workspace-mode\n\nfix(gui): preserve workspace mode across document switches",
          "timestamp": "2026-11-07T07:30:56+08:00",
          "tree_id": "932431bb0a545f9c1c9ae5f6ed910a71b8e28706",
          "url": "https://github.com/LoveDaisy/Lumice/commit/824e978f0926f336217b4856d55086907fca8aa5"
        },
        "date": 1794008102117,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 869622.2,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1009305.3,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 721783.9,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 702668.2,
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
          "id": "a4719d5178481cc9a506c71cbab37271f585e3f2",
          "message": "Merge pull request #575 from LoveDaisy/feat/raypath-pose-entry\n\nfeat(gui): open Pose & trace beside sky-point controls",
          "timestamp": "2026-11-07T08:10:47+08:00",
          "tree_id": "2b3fa8c58803976a6f6537a96676351f3eb5cb61",
          "url": "https://github.com/LoveDaisy/Lumice/commit/a4719d5178481cc9a506c71cbab37271f585e3f2"
        },
        "date": 1794010483900,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 855389.1,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1011796,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 721920.8,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 927841.6,
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
          "id": "0dc0fc9d40c4357238282b96410646dbeab7def7",
          "message": "Merge pull request #576 from LoveDaisy/feat/toolbar-adaptive-labels\n\nfeat(gui): adapt toolbar labels consistently to available width",
          "timestamp": "2026-11-07T09:27:47+08:00",
          "tree_id": "e7f3582dec340d9a9f0787308d3de5d68c9f2051",
          "url": "https://github.com/LoveDaisy/Lumice/commit/0dc0fc9d40c4357238282b96410646dbeab7def7"
        },
        "date": 1794015114881,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 1147774.1,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1013132.5,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 724614,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 581456.8,
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
          "id": "b3c067009d90d6d135d8cc1147f1ff371ecac4dd",
          "message": "Merge pull request #577 from LoveDaisy/feat/raypath-view-camera-unify-design\n\ndocs(gui): define shared camera and explicit map refinement contract",
          "timestamp": "2026-11-07T10:32:16+08:00",
          "tree_id": "86cb59a0680c1e62ab39c628b9b72f127da479e9",
          "url": "https://github.com/LoveDaisy/Lumice/commit/b3c067009d90d6d135d8cc1147f1ff371ecac4dd"
        },
        "date": 1794019026972,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 886750.2,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1011263.9,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 845837.1,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 9V74 80-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 640373.7,
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
          "id": "1e1a1cfbb394c00fcaf0d05c19e737facd8edab6",
          "message": "Merge pull request #578 from LoveDaisy/feat/raypath-view-camera-unify\n\nfeat(gui): share raypath camera and make map refinement explicit",
          "timestamp": "2026-11-08T01:39:29+08:00",
          "tree_id": "fd7e92e0a2fc8277d0b09a6dbc81e07e36b2cc63",
          "url": "https://github.com/LoveDaisy/Lumice/commit/1e1a1cfbb394c00fcaf0d05c19e737facd8edab6"
        },
        "date": 1794073412037,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 749877.3,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1013154.3,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 1147742.9,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 9V45 96-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 875041.2,
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
          "id": "7b978e0d8758f5a30b6523d75426d9951a0277a6",
          "message": "Merge pull request #579 from LoveDaisy/feat/point-view-quick-fixes\n\nAnalyze GUI feedback wave: point-view fixes, GL 3D view, curve axis policy, refine removal, IA draft",
          "timestamp": "2026-11-09T11:55:28+08:00",
          "tree_id": "fc149583067480fda08cea0315fab79846de9db4",
          "url": "https://github.com/LoveDaisy/Lumice/commit/7b978e0d8758f5a30b6523d75426d9951a0277a6"
        },
        "date": 1794196756884,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 981360.3,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1010675.2,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 718725.4,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 700349.2,
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
          "id": "98838d24edddcd959cc61999ff6e4cefbc30f53d",
          "message": "Merge pull request #580 from LoveDaisy/feat/msvc-near-far-gate\n\nPolicy gate: reject near/far macro identifiers (MSVC C2513 class)",
          "timestamp": "2026-11-09T14:12:29+08:00",
          "tree_id": "4d670f320f1fd4f216d4f4321f3da209507ca563",
          "url": "https://github.com/LoveDaisy/Lumice/commit/98838d24edddcd959cc61999ff6e4cefbc30f53d"
        },
        "date": 1794205019428,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 930343.7,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1009785.3,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 725388.7,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 639820.9,
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
          "id": "6900ae9c918346564c9aa6d5c8a9ea0273e0e881",
          "message": "Merge pull request #581 from LoveDaisy/feat/home-win-closing-leg\n\nClosing gate: optional Windows build+ctest leg on home-win (test.sh quick)",
          "timestamp": "2026-11-09T15:56:10+08:00",
          "tree_id": "62b62ac09322caec461b5c0c165053b7a8bd1489",
          "url": "https://github.com/LoveDaisy/Lumice/commit/6900ae9c918346564c9aa6d5c8a9ea0273e0e881"
        },
        "date": 1794211172985,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 936172.3,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1013939.6,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 1104319.6,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 9V45 96-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 696889.8,
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
          "id": "e0df6562454ca8358e50acf0a8b95517ddb84774",
          "message": "Merge pull request #582 from LoveDaisy/feat/analyze-ia-implementation\n\nAnalyze IA implementation: dual entries, L1-L5 layering, verdict anchors, Pose relayout (scrum 778)",
          "timestamp": "2026-11-11T02:18:48+08:00",
          "tree_id": "a98c639e8604a4f19de9b25f463f24311689ebfd",
          "url": "https://github.com/LoveDaisy/Lumice/commit/e0df6562454ca8358e50acf0a8b95517ddb84774"
        },
        "date": 1794334960176,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 837052.9,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1011208.6,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 727885.4,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 632719.7,
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
          "id": "c207e85cf26ec4a62529bac078bef0ffd2964f2d",
          "message": "Merge pull request #583 from LoveDaisy/feat/gate-metal-env-resilience\n\nfix(gate): metal leg env resilience - tolerant decode + build-tools pre-check",
          "timestamp": "2026-11-13T00:49:24+08:00",
          "tree_id": "a6b283103069e94aa0ec31d7665b93f3dd6ca6bd",
          "url": "https://github.com/LoveDaisy/Lumice/commit/c207e85cf26ec4a62529bac078bef0ffd2964f2d"
        },
        "date": 1794502418782,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 923914.2,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1010224.9,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 722504.7,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 634298,
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
          "id": "911161091416fcf33a80071ff156acac7f39d184",
          "message": "Merge pull request #585 from LoveDaisy/feat/analyze-gui-feedback-1112\n\nAnalyze GUI feedback wave 2026-11-12: five interaction/display fixes",
          "timestamp": "2026-11-13T20:09:47+08:00",
          "tree_id": "43ed226410e11ba6e79bffd7881dd2371767fe03",
          "url": "https://github.com/LoveDaisy/Lumice/commit/911161091416fcf33a80071ff156acac7f39d184"
        },
        "date": 1794572042481,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 758827.9,
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
            "value": 1034096.4,
            "unit": "rays/sec",
            "extra": "CPU: Intel(R) Xeon(R) 6975P-C\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 574863.9,
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
          "id": "6eeddf2babad4420037abe23b6cfb294c48fda62",
          "message": "Merge pull request #586 from LoveDaisy/feat/analyze-gui-feedback-1113\n\nAnalyze GUI fourth hands-on feedback wave: five fixes (display consistency, operation bar, camera presets, staleness unification, mechanism consolidation)",
          "timestamp": "2026-11-14T13:17:34+08:00",
          "tree_id": "9ed1355ded1c691c40e9d780f80509724780b42b",
          "url": "https://github.com/LoveDaisy/Lumice/commit/6eeddf2babad4420037abe23b6cfb294c48fda62"
        },
        "date": 1794633790315,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 846685.8,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1010250.7,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 1044780.7,
            "unit": "rays/sec",
            "extra": "CPU: Intel(R) Xeon(R) 6975P-C\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 577891,
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
          "id": "3272248ebc43f184cbd0dcf977b98d987207c48b",
          "message": "Merge pull request #587 from LoveDaisy/feat/hover-readout-type-ownership\n\nrefactor(gui): return row hover types to their semantic owner (imgui-free header)",
          "timestamp": "2026-11-14T17:16:28+08:00",
          "tree_id": "a08d19bd6f3dace2676ed9b6204fbb778c85d09c",
          "url": "https://github.com/LoveDaisy/Lumice/commit/3272248ebc43f184cbd0dcf977b98d987207c48b"
        },
        "date": 1794648028513,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 921352.2,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1011666.7,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 715355.6,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 581499.5,
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
          "id": "f75491cc7628a703b19dbad2f0587bb74d0fd60a",
          "message": "Merge pull request #588 from LoveDaisy/feat/fiber-payload-refinement\n\nfeat(raypath): fiber payload assembly-layer adaptive refinement",
          "timestamp": "2026-11-14T22:59:32+08:00",
          "tree_id": "bdd20fcebfe0d0ac85c26ed74c2d564f18c0c496",
          "url": "https://github.com/LoveDaisy/Lumice/commit/f75491cc7628a703b19dbad2f0587bb74d0fd60a"
        },
        "date": 1794668864417,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 1014976.5,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1010617.3,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 1028935.7,
            "unit": "rays/sec",
            "extra": "CPU: Intel(R) Xeon(R) 6975P-C\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 578347.5,
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
          "id": "22c8b8d581dac91ed8fdad5009d0dc58e0726d99",
          "message": "Merge pull request #589 from LoveDaisy/feat/gui-full-suite-env-flake\n\nFix build.sh install starvation and fast-e2e false reds; record full-suite SIGILL evidence",
          "timestamp": "2026-11-15T09:02:37+08:00",
          "tree_id": "6d8404db8b58285fb9642b543d05b44054386bb3",
          "url": "https://github.com/LoveDaisy/Lumice/commit/22c8b8d581dac91ed8fdad5009d0dc58e0726d99"
        },
        "date": 1794704783947,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 868740.1,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1009838.9,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 1038647.1,
            "unit": "rays/sec",
            "extra": "CPU: Intel(R) Xeon(R) 6975P-C\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 623173,
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
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "distinct": true,
          "id": "52d4760db8da0eab613a4e8821c28c9fbf22a470",
          "message": "Merge pull request #590 from LoveDaisy/chore/release-branch-policy",
          "timestamp": "2026-11-15T10:14:25+08:00",
          "tree_id": "9f1e3ddf0ccf449ee2e2af88e676f95a914da999",
          "url": "https://github.com/LoveDaisy/Lumice/commit/52d4760db8da0eab613a4e8821c28c9fbf22a470"
        },
        "date": 1794709110657,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 1096289.4,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1009381.3,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 721455.4,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 598702.4,
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
          "id": "0be2a1d34f5f22b099bd62e5cff395eee4233514",
          "message": "Merge pull request #591 from LoveDaisy/feat/gui-feedback-1114\n\nfeat(gui-feedback-1114): overlay projection convergence, status bar, curve region rework, 3D reset, globe colormap",
          "timestamp": "2026-11-15T20:37:40+08:00",
          "tree_id": "ee20e2bcb9393d8730a0ad997abb40f1350ecc02",
          "url": "https://github.com/LoveDaisy/Lumice/commit/0be2a1d34f5f22b099bd62e5cff395eee4233514"
        },
        "date": 1794746567643,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 802321.3,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1011159,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 722361.7,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 950592,
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
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "distinct": true,
          "id": "ecca2c50a981551d3f7ace5b811ae02be3440f90",
          "message": "Merge pull request #591 from LoveDaisy/feat/gui-feedback-1114\n\nfeat(gui-feedback-1114): overlay projection convergence, status bar, curve region rework, 3D reset, globe colormap",
          "timestamp": "2026-11-15T22:10:19+08:00",
          "tree_id": "ee20e2bcb9393d8730a0ad997abb40f1350ecc02",
          "url": "https://github.com/LoveDaisy/Lumice/commit/ecca2c50a981551d3f7ace5b811ae02be3440f90"
        },
        "date": 1794752221513,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 859948.9,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1008284.9,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 1046806.5,
            "unit": "rays/sec",
            "extra": "CPU: Intel(R) Xeon(R) 6975P-C\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 539276.3,
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
          "id": "a51a333e8551b7b442d82eafe5b20ffc655ac8b4",
          "message": "Merge pull request #593 from LoveDaisy/feat/sigill-ubsan-boundary-probe\n\ndocs(test): SIGILL sanitizer campaign evidence — blind samples, exit D maintained",
          "timestamp": "2026-11-16T02:36:27+08:00",
          "tree_id": "91940051fe9ac76f7b9fc26b9defb37cb0df3ad5",
          "url": "https://github.com/LoveDaisy/Lumice/commit/a51a333e8551b7b442d82eafe5b20ffc655ac8b4"
        },
        "date": 1794768026333,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 802364,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1011840.7,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 1038065.1,
            "unit": "rays/sec",
            "extra": "CPU: Intel(R) Xeon(R) 6975P-C\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 696756.3,
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
          "id": "46f487f792dd5402b43549dd36dba9e3d1805a08",
          "message": "Merge pull request #595 from LoveDaisy/feat/gate-gui-real-timing-leg\n\nfeat(gate): add gui-real-timing leg to the local throughput gate",
          "timestamp": "2026-11-16T04:27:19+08:00",
          "tree_id": "c7cef6104878196142c2a0ae33a82a596024d99c",
          "url": "https://github.com/LoveDaisy/Lumice/commit/46f487f792dd5402b43549dd36dba9e3d1805a08"
        },
        "date": 1794774685629,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 824513.3,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1011062.6,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 1109191.9,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 9V45 96-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 618268.3,
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
          "id": "5ecd6e9eb61b3eb52edd1c4c9527849b0a6316b9",
          "message": "Merge pull request #596 from LoveDaisy/feat/crystal-param-hash-signed-overflow\n\nfix(gui): move CrystalParamHash to uint32_t domain",
          "timestamp": "2026-11-16T05:53:54+08:00",
          "tree_id": "9e62e445f596c20f1ab5d717985d2d86536ae86e",
          "url": "https://github.com/LoveDaisy/Lumice/commit/5ecd6e9eb61b3eb52edd1c4c9527849b0a6316b9"
        },
        "date": 1794779928748,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 933680,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1009010.1,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 1053903.4,
            "unit": "rays/sec",
            "extra": "CPU: Intel(R) Xeon(R) 6975P-C\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 582078.9,
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
          "id": "2cd2f085a930d16a2d25275a43cb4ede75694cbe",
          "message": "Merge pull request #597 from LoveDaisy/feat/lens-bump-projection-cast-ub\n\nfix(core): clamp single-lens bump cast inputs into int domain",
          "timestamp": "2026-11-16T07:33:03+08:00",
          "tree_id": "a6aef670adbec2df787761450a1d13f42ccfcd1d",
          "url": "https://github.com/LoveDaisy/Lumice/commit/2cd2f085a930d16a2d25275a43cb4ede75694cbe"
        },
        "date": 1794785807640,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 1189616,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1009326.7,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 1046466.9,
            "unit": "rays/sec",
            "extra": "CPU: Intel(R) Xeon(R) 6975P-C\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 618739.9,
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
          "id": "70305fd23cf25cb24d9987c106de61316b403a4f",
          "message": "Merge pull request #598 from LoveDaisy/feat/realtiming-pool-expectation-drift\n\ntest(gui): re-pin four real-timing pool expectations drifted by the 778/779/785 waves",
          "timestamp": "2026-11-16T09:40:03+08:00",
          "tree_id": "137c9cb2e06f9ff7ec70a69102e1af2531cdd335",
          "url": "https://github.com/LoveDaisy/Lumice/commit/70305fd23cf25cb24d9987c106de61316b403a4f"
        },
        "date": 1794793413255,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 1217358.9,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1008537.9,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 841002.4,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 9V74 80-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 686314.3,
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
          "id": "29ce75c3d157f08d2b306aa99c3643c57102c5f6",
          "message": "Merge pull request #601 from LoveDaisy/feat/profile-rows-unstable-red\n\ntest(task-profile-rows-unstable-red): stabilize the profile_rows real-timing case's two flake shapes",
          "timestamp": "2026-11-16T15:35:55+08:00",
          "tree_id": "f72d4b690c2de7bacd14b8fe44ecd7950334e852",
          "url": "https://github.com/LoveDaisy/Lumice/commit/29ce75c3d157f08d2b306aa99c3643c57102c5f6"
        },
        "date": 1794814800130,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 1135439.8,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1007882.8,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 1126981.6,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 9V45 96-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 694379,
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
          "id": "f1a4ee0693e882f9731c1bb18fd73b97231f8ca6",
          "message": "Merge pull request #602 from LoveDaisy/port/476-user-fixes\n\nPort the 788 user-fix wave from release/4.7.6 to main (from #592)",
          "timestamp": "2026-11-16T19:16:24+08:00",
          "tree_id": "eccda5ef6ecd72f7e60c4a49d05573366185d167",
          "url": "https://github.com/LoveDaisy/Lumice/commit/f1a4ee0693e882f9731c1bb18fd73b97231f8ca6"
        },
        "date": 1794828028964,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 948104.1,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1006274.7,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 841928.6,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 9V74 80-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 581686,
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
          "id": "0d50cb0d867067018388289b90ba011d7f842853",
          "message": "Merge pull request #603 from LoveDaisy/port/476-feedback-1115\n\nfix(gui): port the 11-15 feedback wave to main — raypath baseline, row wash, tooltips, preview style default",
          "timestamp": "2026-11-16T21:27:17+08:00",
          "tree_id": "43578551cc6f0737a662e1b9d123abea33965b33",
          "url": "https://github.com/LoveDaisy/Lumice/commit/0d50cb0d867067018388289b90ba011d7f842853"
        },
        "date": 1794835972494,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 956640.1,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1010048,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 653851,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 9V74 80-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 582492.1,
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
          "id": "5150ba87256070aa13503b417e35208ec7ad7214",
          "message": "Merge pull request #604 from LoveDaisy/port/476-release-doc-amendment\n\ndocs(release): retire release branches via cherry-pick ports, not forward-merge",
          "timestamp": "2026-11-16T22:03:08+08:00",
          "tree_id": "30c74249e200d758d9dc20737846c06868c889f2",
          "url": "https://github.com/LoveDaisy/Lumice/commit/5150ba87256070aa13503b417e35208ec7ad7214"
        },
        "date": 1794838034215,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 865342.2,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1010593.2,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 804446.1,
            "unit": "rays/sec",
            "extra": "CPU: Intel(R) Xeon(R) Platinum 8370C CPU @ 2.80GHz\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 629513.9,
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
          "id": "78e6839c6629dff1eb07098252abc53f23228c3f",
          "message": "Merge pull request #614 from LoveDaisy/feat/gate-per-leg-idle-retry\n\nfeat(gate): per-leg idle retry with 06:30 nightly deadline wall",
          "timestamp": "2026-11-17T03:43:31+08:00",
          "tree_id": "a45e11ee656596171d7a83a357d1840524c9260a",
          "url": "https://github.com/LoveDaisy/Lumice/commit/78e6839c6629dff1eb07098252abc53f23228c3f"
        },
        "date": 1794858546482,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 882918.7,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1008518.2,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 1167098.1,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 9V45 96-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 583287,
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
          "id": "263a21c315c2f636a6a584eb1a035d83ac189d61",
          "message": "Merge pull request #616 from LoveDaisy/feat/gate-ubsan-leg\n\nfeat(gate): add the ubsan leg - nightly UB probe gate",
          "timestamp": "2026-11-17T06:40:57+08:00",
          "tree_id": "aff4ceb10deea2418ebf7c4fc4169e692c50991c",
          "url": "https://github.com/LoveDaisy/Lumice/commit/263a21c315c2f636a6a584eb1a035d83ac189d61"
        },
        "date": 1794869067634,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 880425.5,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1009426.3,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 838273.4,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 9V74 80-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 910329.2,
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
          "id": "a8533ca7a68228cf2eed6d461ae87db2ea03f822",
          "message": "Merge pull request #617 from LoveDaisy/feat/gui-feedback-1117\n\nGUI feedback wave 2026-11-17: tooltip two-tier rule, Ray3D region rework, globe Fit/Full views",
          "timestamp": "2026-11-18T09:53:57+08:00",
          "tree_id": "8cb29f609a7727ad38280d90429697f273fe4803",
          "url": "https://github.com/LoveDaisy/Lumice/commit/a8533ca7a68228cf2eed6d461ae87db2ea03f822"
        },
        "date": 1794967143716,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 1180739.4,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1010318.3,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 1037174.6,
            "unit": "rays/sec",
            "extra": "CPU: Intel(R) Xeon(R) 6975P-C\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 586077.2,
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
          "id": "14c782141958dfa028fa7a884b6858d45753a67b",
          "message": "Merge pull request #618 from LoveDaisy/feat/gui-feedback-1118\n\nfix: address GUI raypath analysis feedback from November 18",
          "timestamp": "2026-11-19T19:03:16+08:00",
          "tree_id": "66ecc42f7e82780ba7e0c083dae012cfa0c90fa8",
          "url": "https://github.com/LoveDaisy/Lumice/commit/14c782141958dfa028fa7a884b6858d45753a67b"
        },
        "date": 1795086412270,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 832729.2,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1009956.6,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 1032099.2,
            "unit": "rays/sec",
            "extra": "CPU: Intel(R) Xeon(R) 6975P-C\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 697367.4,
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
          "id": "5a0e7c13c527f94f0e179a4f54512b53016840db",
          "message": "Merge pull request #620 from LoveDaisy/feat/reference-regen-output-decoding\n\nfix(scripts): tolerate non-UTF-8 bytes in regen external output capture",
          "timestamp": "2026-11-20T13:09:18+08:00",
          "tree_id": "9a7772a7e2f1a3e6e153fee31daf6561459fa8fb",
          "url": "https://github.com/LoveDaisy/Lumice/commit/5a0e7c13c527f94f0e179a4f54512b53016840db"
        },
        "date": 1795151597554,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 809899.6,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1010452.3,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 722862.8,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 702292.6,
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
          "id": "22d80d01c1a1a170df818d7183cde33931a70edd",
          "message": "Merge pull request #621 from LoveDaisy/feat/axis-compact-scaled-chip-clip\n\nfix(gui): fit axis chip gaps to live pane width",
          "timestamp": "2026-11-20T20:31:36+08:00",
          "tree_id": "564b2fb47651ddd7a50c6e6f5982920937e0d49c",
          "url": "https://github.com/LoveDaisy/Lumice/commit/22d80d01c1a1a170df818d7183cde33931a70edd"
        },
        "date": 1795178117672,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 878943.7,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1014568.3,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 1038264.7,
            "unit": "rays/sec",
            "extra": "CPU: Intel(R) Xeon(R) 6975P-C\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 913061.6,
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
          "id": "7febf6d10091602d355d0238a51937df978814df",
          "message": "Merge pull request #622 from LoveDaisy/feat/small-button-glyph-clip\n\nfix(gui): preserve SmallButton descender ink at scaled line height",
          "timestamp": "2026-11-21T06:50:01+08:00",
          "tree_id": "491d95044f79f070d1681aea38134ce614726abf",
          "url": "https://github.com/LoveDaisy/Lumice/commit/7febf6d10091602d355d0238a51937df978814df"
        },
        "date": 1795215399468,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 1053491.4,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1011468.8,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 648094.5,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 9V74 80-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 961473.1,
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
          "id": "d685b0569327ac7aea6e1e534b775274ad0194e4",
          "message": "Merge pull request #623 from LoveDaisy/feat/metal-exit-wsum-main-port\n\nfix(metal): reduce exit weights per SIMD group (port from release/4.7.7)",
          "timestamp": "2026-11-21T07:08:10+08:00",
          "tree_id": "46894b9cf42864f233a143255b8be541dd2329de",
          "url": "https://github.com/LoveDaisy/Lumice/commit/d685b0569327ac7aea6e1e534b775274ad0194e4"
        },
        "date": 1795216383073,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 849924.1,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1012591.3,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 1168646.9,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 9V45 96-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 593782.9,
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
          "id": "c398905ca960146043188906e46deeff6a25fc1d",
          "message": "Merge pull request #624 from LoveDaisy/feat/precommit-hook-delegator\n\nbuild(hooks): delegate pre-commit to the committing checkout",
          "timestamp": "2026-11-21T07:24:39+08:00",
          "tree_id": "445bde2ae1e12961c92c6fcc8706028c8e638ac5",
          "url": "https://github.com/LoveDaisy/Lumice/commit/c398905ca960146043188906e46deeff6a25fc1d"
        },
        "date": 1795217339355,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 986092,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1010520.3,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 791868,
            "unit": "rays/sec",
            "extra": "CPU: INTEL(R) XEON(R) PLATINUM 8573C\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 578287.3,
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
          "id": "02385c457d9f24d3cb2d29aa789aa52bad68f4ab",
          "message": "Merge pull request #625 from LoveDaisy/feat/expanded-column-width-fix\n\nfix(gui): scale Expanded column widths in layout units",
          "timestamp": "2026-11-21T07:45:27+08:00",
          "tree_id": "5f78a5bdffc3bb8aba0fae12b5f23b26efddb7b8",
          "url": "https://github.com/LoveDaisy/Lumice/commit/02385c457d9f24d3cb2d29aa789aa52bad68f4ab"
        },
        "date": 1795218521540,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 1029951.9,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1014445.9,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 1036127.1,
            "unit": "rays/sec",
            "extra": "CPU: Intel(R) Xeon(R) 6975P-C\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 799227,
            "unit": "rays/sec",
            "extra": "CPU: Intel(R) Xeon(R) 6973P-C\\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "a9b0149b1aa7bb53a1a0e521d99af42a3548f068",
          "message": "Merge pull request #626 from LoveDaisy/docs/gate-duration-disposition\n\ntest(budget): record the 2026-11-20/21 ctest disposition (reason-only)",
          "timestamp": "2026-11-21T08:04:03+08:00",
          "tree_id": "a142292d0432fc5976b070cfacacb6fbb7779e7c",
          "url": "https://github.com/LoveDaisy/Lumice/commit/a9b0149b1aa7bb53a1a0e521d99af42a3548f068"
        },
        "date": 1795219658153,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 852743.6,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1011025.4,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 1029965.6,
            "unit": "rays/sec",
            "extra": "CPU: Intel(R) Xeon(R) 6975P-C\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 924819.4,
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
          "id": "2c7a6f417d3de05ec6e1adad2fb111874175abe7",
          "message": "Merge pull request #628 from LoveDaisy/feat/analyze-presentation-first-batch\n\nfeat(gui): analyze presentation first batch",
          "timestamp": "2026-11-23T09:50:08+08:00",
          "tree_id": "db72a497c926bbf8ae62b1b32b9a14f657839492",
          "url": "https://github.com/LoveDaisy/Lumice/commit/2c7a6f417d3de05ec6e1adad2fb111874175abe7"
        },
        "date": 1795398897847,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 784614.6,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1013185.5,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 1037300.6,
            "unit": "rays/sec",
            "extra": "CPU: Intel(R) Xeon(R) 6975P-C\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 583599.3,
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
          "id": "0a1784dc996df84bf94c4ff774ba5f33d0560fac",
          "message": "Merge pull request #631 from LoveDaisy/feat/map-request-symmetry-bits\n\nfeat(capi,map): carry true symmetry bits in map requests",
          "timestamp": "2026-11-23T10:15:59+08:00",
          "tree_id": "662a5fd31b8a2a2fde9798f38adc9ad4e5401e8f",
          "url": "https://github.com/LoveDaisy/Lumice/commit/0a1784dc996df84bf94c4ff774ba5f33d0560fac"
        },
        "date": 1795400400470,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 948435.8,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1010686.7,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 776045.3,
            "unit": "rays/sec",
            "extra": "CPU: INTEL(R) XEON(R) PLATINUM 8573C\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 589296.8,
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
          "id": "1efc673fc1346637961b1cbf3f127abe1539fe48",
          "message": "Merge pull request #633 from LoveDaisy/chore/ci-release-push-trigger\n\nci: trigger the workflow on release/** pushes",
          "timestamp": "2026-11-23T11:52:05+08:00",
          "tree_id": "8ea5c55e318e2da81806571f958e790d6e2704c8",
          "url": "https://github.com/LoveDaisy/Lumice/commit/1efc673fc1346637961b1cbf3f127abe1539fe48"
        },
        "date": 1795406190634,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 865262.4,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1012014.7,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 1037307,
            "unit": "rays/sec",
            "extra": "CPU: Intel(R) Xeon(R) 6975P-C\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 583131.8,
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
          "id": "349a4270504e39cc3c291c4257052477bfb36888",
          "message": "Merge pull request #636 from LoveDaisy/feat/local-class-algebra\n\nfeat(raypath,capi): local-class algebra — point-selection representatives, log density facts, interpretive chain",
          "timestamp": "2026-11-24T02:07:01+08:00",
          "tree_id": "594868057c65dc035af6aee5110e333dc189228c",
          "url": "https://github.com/LoveDaisy/Lumice/commit/349a4270504e39cc3c291c4257052477bfb36888"
        },
        "date": 1795457452816,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 936662.3,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1013364.4,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 1044838.4,
            "unit": "rays/sec",
            "extra": "CPU: Intel(R) Xeon(R) 6975P-C\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 694891.1,
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
          "id": "8d161eba2c6c4dde9e29651ca26b4ba7e5bc8be4",
          "message": "Merge pull request #639 from LoveDaisy/feat/map-session-test-contract\n\ntest(gui): decouple map lifecycle cases from the full-budget compute",
          "timestamp": "2026-11-24T15:14:15+08:00",
          "tree_id": "b15b27b5a50961505b81cf5b4afd60f0895365f4",
          "url": "https://github.com/LoveDaisy/Lumice/commit/8d161eba2c6c4dde9e29651ca26b4ba7e5bc8be4"
        },
        "date": 1795504690144,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 912379.8,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1010740.6,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 716404.5,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 819095.2,
            "unit": "rays/sec",
            "extra": "CPU: Intel(R) Xeon(R) 6973P-C\\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "390c995b286acbd17618dd50ae2adb3b2498a024",
          "message": "Merge pull request #642 from LoveDaisy/feat/metal-gate-blocking-scope\n\nci(e2e-slow): Metal dual-renderer throughput ratio becomes a reported signal",
          "timestamp": "2026-11-24T16:18:36+08:00",
          "tree_id": "f4143a5d0a9e1378f579c84b5b9e1a36183c4021",
          "url": "https://github.com/LoveDaisy/Lumice/commit/390c995b286acbd17618dd50ae2adb3b2498a024"
        },
        "date": 1795508658726,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 789758.1,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1010660.8,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 1041800.6,
            "unit": "rays/sec",
            "extra": "CPU: Intel(R) Xeon(R) 6975P-C\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 583879,
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
          "id": "73891fd38b1448ff47bc0a3976104785c9a48413",
          "message": "Merge pull request #646 from LoveDaisy/feat/ctest-headroom-tracker\n\nfeat(scripts): ctest headroom tracker — implied-C/headroom observation with streak signals",
          "timestamp": "2026-11-24T21:22:44+08:00",
          "tree_id": "3abfec62a61fd7542d9d5f18868b4bebfbea82c6",
          "url": "https://github.com/LoveDaisy/Lumice/commit/73891fd38b1448ff47bc0a3976104785c9a48413"
        },
        "date": 1795527808968,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 871688.2,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1011647.2,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 1028469,
            "unit": "rays/sec",
            "extra": "CPU: Intel(R) Xeon(R) 6975P-C\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 652963,
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
          "id": "c8b8f111be3f7aa884289413e955ee9e54a3e1a4",
          "message": "Merge pull request #647 from LoveDaisy/port/477-infra\n\nPort release/4.7.7 infra cluster to main (#619 #627 #630 #632)",
          "timestamp": "2026-11-25T00:32:29+08:00",
          "tree_id": "907721e98226835ad1da12a6903ca476eb47deb4",
          "url": "https://github.com/LoveDaisy/Lumice/commit/c8b8f111be3f7aa884289413e955ee9e54a3e1a4"
        },
        "date": 1795539682111,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 1075242.5,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1012296.1,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 1032391.1,
            "unit": "rays/sec",
            "extra": "CPU: Intel(R) Xeon(R) 6975P-C\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 583508,
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
          "id": "af73a164c48b2e48472d2cbd4cb07bc215f6fa5d",
          "message": "Merge pull request #648 from LoveDaisy/port/477-interaction\n\nPort the 4.7.7 interaction/UI cluster to main (#635 #638 #641 #644)",
          "timestamp": "2026-11-25T05:25:15+08:00",
          "tree_id": "a92be6ef32b900df06a2ed42efdf111ed38b4a31",
          "url": "https://github.com/LoveDaisy/Lumice/commit/af73a164c48b2e48472d2cbd4cb07bc215f6fa5d"
        },
        "date": 1795556974785,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 934650.9,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1014250.4,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 1067890.3,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 9V45 96-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 592616.4,
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
          "id": "183da9daba02fa2ee60fc32b50f0bf60412e6e5a",
          "message": "Merge pull request #649 from LoveDaisy/chore/release-477-main-backfill\n\ndocs(changelog): backfill 4.7.7 section",
          "timestamp": "2026-11-25T05:54:37+08:00",
          "tree_id": "9403fad84a0837cffad25ce35cb2ec8e7d0b86f7",
          "url": "https://github.com/LoveDaisy/Lumice/commit/183da9daba02fa2ee60fc32b50f0bf60412e6e5a"
        },
        "date": 1795558216464,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 1044302.6,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1011074.1,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 1030707.6,
            "unit": "rays/sec",
            "extra": "CPU: Intel(R) Xeon(R) 6975P-C\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 588268.9,
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
          "id": "ab73dccde5f9f36162d9599e10b674ec95e2a17a",
          "message": "Merge pull request #653 from LoveDaisy/feat/policy-gate-timebomb-fixture\n\ntest(scripts): decouple tracker policy-gate fixtures from the wall clock",
          "timestamp": "2026-11-26T03:04:35+08:00",
          "tree_id": "4a8e9b71faed5f6dd968776cdd6079439ec02f62",
          "url": "https://github.com/LoveDaisy/Lumice/commit/ab73dccde5f9f36162d9599e10b674ec95e2a17a"
        },
        "date": 1795634562446,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 918954.1,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1005534.9,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 838915.8,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 9V74 80-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 557161.4,
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
          "id": "846c30008635ac506d0664114c44b96142b87da4",
          "message": "Merge pull request #651 from LoveDaisy/feat/caustic-marker-declutter\n\nfix(analyze-map): declutter caustic markers with own layer group and pile aggregation",
          "timestamp": "2026-11-26T03:35:17+08:00",
          "tree_id": "d7afd09f3cd1e70dcff66b286135f5160cf3c1c4",
          "url": "https://github.com/LoveDaisy/Lumice/commit/846c30008635ac506d0664114c44b96142b87da4"
        },
        "date": 1795636855028,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 1120737.5,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1012651.4,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 763185.8,
            "unit": "rays/sec",
            "extra": "CPU: INTEL(R) XEON(R) PLATINUM 8573C\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 876669.1,
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
          "id": "2075f95b8a98f5716d7e6792cb070ecfae282307",
          "message": "Merge pull request #655 from LoveDaisy/feat/raypath-layers-ui\n\nfeat(analyze-map): themed table replaces the bare layer switches",
          "timestamp": "2026-11-26T07:44:03+08:00",
          "tree_id": "c35f36bd1be756dc2a2d06b06323bb86c8b3c330",
          "url": "https://github.com/LoveDaisy/Lumice/commit/2075f95b8a98f5716d7e6792cb070ecfae282307"
        },
        "date": 1795650918705,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 1032424.5,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1013515.8,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 895444.2,
            "unit": "rays/sec",
            "extra": "CPU: INTEL(R) XEON(R) PLATINUM 8573C\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 694297.5,
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
          "id": "b8fe6c8550932e7aff53ee851f4ba48b9ed93f40",
          "message": "Merge pull request #652 from LoveDaisy/feat/rho-deep-tail-curve\n\nfix(gui): rho curve reads log densities on an independent axis; adopt the interpretive branch",
          "timestamp": "2026-11-26T09:02:03+08:00",
          "tree_id": "22ed15e99f07f7620cb8b9a1a189970bf09e1edf",
          "url": "https://github.com/LoveDaisy/Lumice/commit/b8fe6c8550932e7aff53ee851f4ba48b9ed93f40"
        },
        "date": 1795656445798,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 864650.8,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1011989.7,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 1036353.9,
            "unit": "rays/sec",
            "extra": "CPU: Intel(R) Xeon(R) 6975P-C\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 973566.3,
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
          "id": "6f2d696ad1743bef647f6647b1b87f3bdb7e276d",
          "message": "Merge pull request #658 from LoveDaisy/feat/parry-survey-nonterminal-hang\n\ntest(gui): single-source the survey partial-budget note (Parry survey hang verdict)",
          "timestamp": "2026-11-26T17:24:07+08:00",
          "tree_id": "5db2995bb95caa5a58cf49210337423e630afc4d",
          "url": "https://github.com/LoveDaisy/Lumice/commit/6f2d696ad1743bef647f6647b1b87f3bdb7e276d"
        },
        "date": 1795686117428,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 896422.2,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1010951.8,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 803151.3,
            "unit": "rays/sec",
            "extra": "CPU: Intel(R) Xeon(R) Platinum 8370C CPU @ 2.80GHz\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 702833.3,
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
          "id": "ade1b7284b5b65885cc0128475b9945a45d1cf42",
          "message": "Merge pull request #659 from LoveDaisy/feat/arc-recovery-engine\n\nfeat: ordered fold-arc recovery — the caustic cards become drawable, pickable arcs",
          "timestamp": "2026-11-26T23:28:25+08:00",
          "tree_id": "256b9d8575ae467edf24be35717238970dfd46e2",
          "url": "https://github.com/LoveDaisy/Lumice/commit/ade1b7284b5b65885cc0128475b9945a45d1cf42"
        },
        "date": 1795708856205,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 953686.5,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1009873.6,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 1038205.7,
            "unit": "rays/sec",
            "extra": "CPU: Intel(R) Xeon(R) 6975P-C\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 689083.7,
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
          "id": "69d5e0e8d16b6a7134876bbf120530716abe9acc",
          "message": "Merge pull request #660 from LoveDaisy/goal/add-2d-j-0-caustic-continuation-and-posecurve-in\n\nfeat(analytic): add 2D J=0 caustic continuation and unified PoseCurve",
          "timestamp": "2026-11-27T20:18:13+08:00",
          "tree_id": "92cce1de4d6d2d6168871034b0d63f748257f900",
          "url": "https://github.com/LoveDaisy/Lumice/commit/69d5e0e8d16b6a7134876bbf120530716abe9acc"
        },
        "date": 1795782968750,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 779320.8,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1007526.6,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 1022869.5,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 9V45 96-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 580805.4,
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
          "id": "db80d09847e73993c42c92dd86a70188c8154bf8",
          "message": "Merge pull request #661 from LoveDaisy/goal/refactor-caustic-continuation-to-o-1-residual-co\n\nrefactor(analytic): switch caustic continuation to O(1) J_reg contract and split modules",
          "timestamp": "2026-11-28T02:30:54+08:00",
          "tree_id": "7d2b9b61cdb956630a349cb546570bdbeeea9683",
          "url": "https://github.com/LoveDaisy/Lumice/commit/db80d09847e73993c42c92dd86a70188c8154bf8"
        },
        "date": 1795805362324,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 883676,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1008714.3,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 1035862.6,
            "unit": "rays/sec",
            "extra": "CPU: Intel(R) Xeon(R) 6975P-C\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 579958.6,
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
          "id": "6d2a8a79915f62a319ee93debc11f4f69a027dc5",
          "message": "Merge pull request #663 from LoveDaisy/chore/duration-budget-unblock\n\nRaise ctest phase budgets: macOS 560->660, Ubuntu x86_64 360->420 (transitional)",
          "timestamp": "2026-11-28T12:43:40+08:00",
          "tree_id": "0b672b4dc85982dc426c9700348a861ed3cb42c9",
          "url": "https://github.com/LoveDaisy/Lumice/commit/6d2a8a79915f62a319ee93debc11f4f69a027dc5"
        },
        "date": 1795842343122,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 1213154.9,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1005251.8,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 1033468.8,
            "unit": "rays/sec",
            "extra": "CPU: Intel(R) Xeon(R) 6975P-C\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 574837.2,
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
          "id": "f6818f15b3c4692f2442f1cc67388528deb0556a",
          "message": "Merge pull request #662 from LoveDaisy/goal/add-pathintrinsic-cached-per-path-intrinsic-obje\n\nAdd PathIntrinsic: cached per-path intrinsic object over verified analytic assets",
          "timestamp": "2026-11-28T13:10:01+08:00",
          "tree_id": "3f13d52cd70cf6532691fe2533e37147bd73f5bb",
          "url": "https://github.com/LoveDaisy/Lumice/commit/f6818f15b3c4692f2442f1cc67388528deb0556a"
        },
        "date": 1795843712603,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 1010739.7,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1010781.1,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 839787.5,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 9V74 80-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 586849,
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
          "id": "45ae9033274b82965bdb6ce727f413a1ee80ac2d",
          "message": "Merge pull request #664 from LoveDaisy/goal/add-axisposedensity-exact-pose-density-for-all-a\n\nAdd AxisPoseDensity: exact pose density for all AxisDistribution types",
          "timestamp": "2026-11-28T13:44:48+08:00",
          "tree_id": "87de178a912fdc0dea12eaf3ddc139b17c8b8606",
          "url": "https://github.com/LoveDaisy/Lumice/commit/45ae9033274b82965bdb6ce727f413a1ee80ac2d"
        },
        "date": 1795845383616,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 901093.7,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1011275.5,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 717866.6,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 699285,
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
          "id": "7522f1f29792ff614c719977648b7540d06fe56d",
          "message": "Merge pull request #665 from LoveDaisy/goal/add-skyblurfield-pose-covariance-pushforward-bri\n\nAdd SkyBlurField: pose-covariance pushforward bridging distribution and skeleton",
          "timestamp": "2026-11-28T14:59:02+08:00",
          "tree_id": "e0da2beaa3fd78cd6344e699f682e3c8f854163f",
          "url": "https://github.com/LoveDaisy/Lumice/commit/7522f1f29792ff614c719977648b7540d06fe56d"
        },
        "date": 1795850389690,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 867759.1,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1013220.4,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 1035247.1,
            "unit": "rays/sec",
            "extra": "CPU: Intel(R) Xeon(R) 6975P-C\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 582759.8,
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
          "id": "a4d56f0943b8fe3e078f72c8b136692f9692b114",
          "message": "Merge pull request #530 from LoveDaisy/feat/gap1-family-routing\n\nfeat(raypath): gap-1 measure-to-family routing (first wave 9/9)",
          "timestamp": "2026-10-20T22:27:14+08:00",
          "tree_id": "77c386a99074b5e97dfb8278a6d65fcc30ed649b",
          "url": "https://github.com/LoveDaisy/Lumice/commit/a4d56f0943b8fe3e078f72c8b136692f9692b114"
        },
        "date": 1792506650358,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 94.3,
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
          "id": "725018d33ad9a3d08ded65180c4f2d3272581cd7",
          "message": "Merge pull request #531 from LoveDaisy/feat/free-dims-enum-cleanup\n\nrefactor(raypath): free_dims carries the dim enum",
          "timestamp": "2026-10-21T00:02:56+08:00",
          "tree_id": "280b8049e2f76e79f05bd54c0b9bc03c21e48818",
          "url": "https://github.com/LoveDaisy/Lumice/commit/725018d33ad9a3d08ded65180c4f2d3272581cd7"
        },
        "date": 1792512396188,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 82.8,
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
            "value": 94.4,
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
          "id": "a7f8b4eaab29ae7dd4fb9f70c7b10040a958550d",
          "message": "Merge pull request #533 from LoveDaisy/feat/ci-ctest-budget-rebalance\n\ntest(ci): zero-sum ctest budget rebalance + the four survey structure cleanups",
          "timestamp": "2026-10-21T20:09:47+08:00",
          "tree_id": "d1414e4f2c68d617f42b63835e7c89df495db972",
          "url": "https://github.com/LoveDaisy/Lumice/commit/a7f8b4eaab29ae7dd4fb9f70c7b10040a958550d"
        },
        "date": 1792584811686,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 79.6,
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
          "id": "e3acdfcb46e5115e09897dc2a33218a679a73714",
          "message": "Merge pull request #534 from LoveDaisy/feat/c10-ladder-gate-order\n\nfix(raypath): the level-0 partition refusal scopes to its own card",
          "timestamp": "2026-10-21T21:59:06+08:00",
          "tree_id": "0dd10a9ed7c0e3fba17f416a6d2095a87d6673a6",
          "url": "https://github.com/LoveDaisy/Lumice/commit/e3acdfcb46e5115e09897dc2a33218a679a73714"
        },
        "date": 1792591367700,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 83.7,
            "unit": "%"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 100,
            "unit": "%"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 98.3,
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
          "id": "1792d962467839be845bd5577eabc039164cf440",
          "message": "Merge pull request #536 from LoveDaisy/feat/cuda-continue-legs\n\ntest(server): land the four CUDA continuation legs from the reference-box probe",
          "timestamp": "2026-10-21T22:52:57+08:00",
          "tree_id": "d08f791614a40b3b1163ff40f6dea1e31ff11104",
          "url": "https://github.com/LoveDaisy/Lumice/commit/1792d962467839be845bd5577eabc039164cf440"
        },
        "date": 1792594579291,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 94.9,
            "unit": "%"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 100,
            "unit": "%"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 97.1,
            "unit": "%"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 94.4,
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
          "id": "3a789961cdb423ef5ea1b5b7ec0d5899e3187166",
          "message": "Merge pull request #535 from LoveDaisy/feat/threshold-single-source\n\nrefactor(gui-test): thresholds resolve from _thresholds.json at run time",
          "timestamp": "2026-10-21T23:46:17+08:00",
          "tree_id": "57b1ca93a4d41a1cfb107f290788ddda5f971e59",
          "url": "https://github.com/LoveDaisy/Lumice/commit/3a789961cdb423ef5ea1b5b7ec0d5899e3187166"
        },
        "date": 1792597784164,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 96,
            "unit": "%"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 100.2,
            "unit": "%"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 98.5,
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
          "id": "49620f6e2374ecef84e0cec7c546028623c60493",
          "message": "Merge pull request #537 from LoveDaisy/feat/gap3-tir-ess-probes-c18\n\nfeat(raypath): TIR share + opt-in MC region ESS evidence legs (gap ③)",
          "timestamp": "2026-10-22T14:58:23+08:00",
          "tree_id": "62674616b336864170d87cd4568ecea21be722b6",
          "url": "https://github.com/LoveDaisy/Lumice/commit/49620f6e2374ecef84e0cec7c546028623c60493"
        },
        "date": 1792652517622,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 98,
            "unit": "%"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 100.1,
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
          "id": "b7910bd93417b149f483132e534f1f50f44153c2",
          "message": "Merge pull request #538 from LoveDaisy/feat/raypath-point-l2-warm\n\nfeat(raypath): the L2 point command — factor decomposition + the --warm index contract",
          "timestamp": "2026-10-22T23:20:21+08:00",
          "tree_id": "4041bf027823738c4e184bac29559cb4397a5908",
          "url": "https://github.com/LoveDaisy/Lumice/commit/b7910bd93417b149f483132e534f1f50f44153c2"
        },
        "date": 1792682637895,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 90.3,
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
          "id": "272c3e0749d0ff9744a8ebc02f71b4ce04dbf205",
          "message": "Merge pull request #539 from LoveDaisy/feat/raypath-probe-profiles\n\nfeat(raypath): the L2 probe command — four --along forms on the shared line core",
          "timestamp": "2026-10-23T06:59:16+08:00",
          "tree_id": "2dcbad7a3a36b56e1c3c0d192a8839bba8dd9b45",
          "url": "https://github.com/LoveDaisy/Lumice/commit/272c3e0749d0ff9744a8ebc02f71b4ce04dbf205"
        },
        "date": 1792710180079,
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
          "id": "3a00c4a7ac2656f50f20eb2f80c60fd77fda7205",
          "message": "Merge pull request #541 from LoveDaisy/feat/ctest-machine-normalized-budget\n\nfeat(test-gate): machine-speed normalization for ctest phase budgets (registry v4)",
          "timestamp": "2026-10-23T13:27:12+08:00",
          "tree_id": "3fb16cb4a4390a3797a2d66b5a9aa36add0388b9",
          "url": "https://github.com/LoveDaisy/Lumice/commit/3a00c4a7ac2656f50f20eb2f80c60fd77fda7205"
        },
        "date": 1792733483779,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 90.2,
            "unit": "%"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 100.1,
            "unit": "%"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 100,
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
          "id": "76fc7455b0fe29c44ca2c36c777f7cf3d88bbf16",
          "message": "Merge pull request #540 from LoveDaisy/feat/gap2-subgroup-gating\n\nfeat(core): physical P gating as a per-element retained-rotation set (gap ②)",
          "timestamp": "2026-10-23T13:58:39+08:00",
          "tree_id": "344d73644fead342651a339dcc81d442de596644",
          "url": "https://github.com/LoveDaisy/Lumice/commit/76fc7455b0fe29c44ca2c36c777f7cf3d88bbf16"
        },
        "date": 1792735763275,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 85.3,
            "unit": "%"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 99.9,
            "unit": "%"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 99.2,
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
          "id": "4ebb977073157d94978c2dd51509ed2006597e23",
          "message": "Merge pull request #542 from LoveDaisy/feat/testing-inner-loop\n\nfeat(test): the inner scope — diff-derived layers for the iteration loop",
          "timestamp": "2026-10-23T14:32:36+08:00",
          "tree_id": "08016163a6e1869c1c3db2730a08078ddbe1e0dc",
          "url": "https://github.com/LoveDaisy/Lumice/commit/4ebb977073157d94978c2dd51509ed2006597e23"
        },
        "date": 1792737386181,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 92.7,
            "unit": "%"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 99.9,
            "unit": "%"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 97.4,
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
          "id": "44446dcf5082221f38943bb7ef5f2a4bb89c7240",
          "message": "Merge pull request #543 from LoveDaisy/feat/survey-wave-review-sweep\n\nfix(core): zero-width pole-centred gluing excluded; the wave's review sweep (task-735)",
          "timestamp": "2026-10-23T20:19:32+08:00",
          "tree_id": "ea55d5da85b9ea01bace2af47e90705d3526c028",
          "url": "https://github.com/LoveDaisy/Lumice/commit/44446dcf5082221f38943bb7ef5f2a4bb89c7240"
        },
        "date": 1792758216427,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 96.1,
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
          "id": "61686ea7e253835425093a7964672997f4333dd5",
          "message": "Merge pull request #544 from LoveDaisy/chore/release-4.7.5\n\nchore(release): cut 4.7.5",
          "timestamp": "2026-10-23T21:28:21+08:00",
          "tree_id": "db9017bd4b326dbf208009eade13622c90e7b9b6",
          "url": "https://github.com/LoveDaisy/Lumice/commit/61686ea7e253835425093a7964672997f4333dd5"
        },
        "date": 1792762297142,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 78,
            "unit": "%"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 99.7,
            "unit": "%"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 99.2,
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
          "id": "f6b77fdab11fb6bc88f63fcb794f68de73f690ea",
          "message": "Merge pull request #545 from LoveDaisy/feat/raypath-warm-structure-scope\n\nfix(raypath): scope warm numeric facts to their producing wavelength",
          "timestamp": "2026-10-24T15:57:44+08:00",
          "tree_id": "4f5fec9f158d5d80e6cdc8ff173a351e2c42d8c7",
          "url": "https://github.com/LoveDaisy/Lumice/commit/f6b77fdab11fb6bc88f63fcb794f68de73f690ea"
        },
        "date": 1792828985155,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 86.9,
            "unit": "%"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 100,
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
          "id": "204775d4b7b287915b6fe3941773c1eb8df34d19",
          "message": "Merge pull request #547 from LoveDaisy/feat/raypath-typed-architecture\n\nrefactor(raypath): complete typed-first query and C API architecture",
          "timestamp": "2026-10-27T02:50:35+08:00",
          "tree_id": "3a08be3734802675a46cfc191fef7514cdae32c2",
          "url": "https://github.com/LoveDaisy/Lumice/commit/204775d4b7b287915b6fe3941773c1eb8df34d19"
        },
        "date": 1793040878851,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 91.4,
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
          "id": "696685bc25d5ef49575143b12f37744b2519944d",
          "message": "Merge pull request #548 from LoveDaisy/feat/crystal-card-text-hit-overlap\n\nFix crystal card text probes stealing button clicks",
          "timestamp": "2026-10-27T22:36:07+08:00",
          "tree_id": "e128f7f0b209745c23eb487980fd9d05b97eff32",
          "url": "https://github.com/LoveDaisy/Lumice/commit/696685bc25d5ef49575143b12f37744b2519944d"
        },
        "date": 1793112028061,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 75.9,
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
          "id": "4457f2a77b4c98815d730e5d030e079f192487d0",
          "message": "Merge pull request #549 from LoveDaisy/feat/test-iteration-speed\n\nperf(test): shorten iteration feedback with dedup and sharding",
          "timestamp": "2026-10-28T19:24:30+08:00",
          "tree_id": "65aaff66a0a607a3199b30ac098be2cec79ab778",
          "url": "https://github.com/LoveDaisy/Lumice/commit/4457f2a77b4c98815d730e5d030e079f192487d0"
        },
        "date": 1793186906777,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 98.3,
            "unit": "%"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 100.3,
            "unit": "%"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 98.7,
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
          "id": "82acb1c7506066de7efea14f2991f45b483e0e0a",
          "message": "Merge pull request #550 from LoveDaisy/feat/raypath-docs-promotion\n\ndocs(raypath): promote diagnostic pipeline design and refresh the roadmap",
          "timestamp": "2026-10-30T01:21:16+08:00",
          "tree_id": "f2649338d9f7df1cc633beb6c3fe07372356de44",
          "url": "https://github.com/LoveDaisy/Lumice/commit/82acb1c7506066de7efea14f2991f45b483e0e0a"
        },
        "date": 1793294739615,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 90.3,
            "unit": "%"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 99.6,
            "unit": "%"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 96.9,
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
          "id": "b370f17aca80a388068584dd0e209daaf92dc0ba",
          "message": "Merge pull request #551 from LoveDaisy/feat/analyze-gui-v1\n\nfeat: Analyze GUI v1 — class reference maps, representative rays, layered overview, dispersion, map & point inspection",
          "timestamp": "2026-10-31T08:49:46+08:00",
          "tree_id": "23dabe19bd569422f87f8a4d312ff63f7fd70664",
          "url": "https://github.com/LoveDaisy/Lumice/commit/b370f17aca80a388068584dd0e209daaf92dc0ba"
        },
        "date": 1793408049035,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 107.9,
            "unit": "%"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 99.7,
            "unit": "%"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 99,
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
          "id": "a92d037c5dc3459b9668540e36cc255c65b8c7b2",
          "message": "Merge pull request #552 from LoveDaisy/feat/realtime-legs-placement\n\ntest: reclaim the one analyze CI row that actually pays wall-clock — real-timing legs never ran in PR ctest",
          "timestamp": "2026-10-31T13:23:32+08:00",
          "tree_id": "ad4451ec4d84a542a8ea46928ecb4229d88297e5",
          "url": "https://github.com/LoveDaisy/Lumice/commit/a92d037c5dc3459b9668540e36cc255c65b8c7b2"
        },
        "date": 1793424426407,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 87,
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
          "id": "71ee8904b6b1a70d8ea0b90f1c3ae44e0108792f",
          "message": "Merge pull request #553 from LoveDaisy/feat/panorama-drift-fix\n\nfix(gui): the analyze panorama published OS-screen coords as framebuffer coords — texture drifted with the window",
          "timestamp": "2026-10-31T15:01:13+08:00",
          "tree_id": "007cf6a260e6236bc6eec33270bf4f1ad7915f42",
          "url": "https://github.com/LoveDaisy/Lumice/commit/71ee8904b6b1a70d8ea0b90f1c3ae44e0108792f"
        },
        "date": 1793430314116,
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
            "value": 99,
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
          "id": "f64a77d8de746d0479d5ec85665e23b629b57fea",
          "message": "Merge pull request #554 from LoveDaisy/feat/analyze-panel-clip-fix\n\nfix(analyze): three scroll regions clipped their headers — measured reserves, a table that absorbs, and an 8pt strip ImGui secretly sized to 32pt",
          "timestamp": "2026-10-31T18:04:58+08:00",
          "tree_id": "440d8445970baae5ba4c237d591750c937060670",
          "url": "https://github.com/LoveDaisy/Lumice/commit/f64a77d8de746d0479d5ec85665e23b629b57fea"
        },
        "date": 1793441348151,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 108.7,
            "unit": "%"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 100.1,
            "unit": "%"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 99.4,
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
          "id": "0cda0c24a2505274ce88b5c332db83b11bf9e08b",
          "message": "Merge pull request #555 from LoveDaisy/feat/ray3d-orbit-grab-fix\n\nfix(gui): the ray3d orbit basis had pitch and yaw slots transposed — axes swapped and the drag metaphor scrambled",
          "timestamp": "2026-10-31T18:30:37+08:00",
          "tree_id": "9a32ca08414fc7310da15fb49a95dcd6668e8f8e",
          "url": "https://github.com/LoveDaisy/Lumice/commit/0cda0c24a2505274ce88b5c332db83b11bf9e08b"
        },
        "date": 1793442969466,
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
          "id": "284c9455d369800eb8b70405d488abe377dbf75c",
          "message": "Merge pull request #556 from LoveDaisy/feat/analyze-wait-feedback-a\n\nfeat(analyze): the wait says how long it has waited — per-face elapsed lines, a status-bar Analyzing segment, row activity dots",
          "timestamp": "2026-10-31T19:08:56+08:00",
          "tree_id": "7d66f431a427bc186d7e1e8075afa021ede499f2",
          "url": "https://github.com/LoveDaisy/Lumice/commit/284c9455d369800eb8b70405d488abe377dbf75c"
        },
        "date": 1793445168319,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 88.8,
            "unit": "%"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 99.6,
            "unit": "%"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 97.9,
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
          "id": "ed0062b7978aff4024c97d61b3eaf193332a0ce7",
          "message": "Merge pull request #557 from LoveDaisy/feat/witness-tir-deadline\n\nfix(survey): the 15s deadline now bites inside the witness and TIR legs — cooperative polls at batch boundaries",
          "timestamp": "2026-10-31T22:25:04+08:00",
          "tree_id": "7a451064b6104c5005007f3631ff8bf19bfb1077",
          "url": "https://github.com/LoveDaisy/Lumice/commit/ed0062b7978aff4024c97d61b3eaf193332a0ce7"
        },
        "date": 1793456950361,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 102,
            "unit": "%"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 100.1,
            "unit": "%"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 99.8,
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
          "id": "33934bd7de38458727db064be595da31a2314160",
          "message": "Merge pull request #558 from LoveDaisy/feat/topbar-toggle-regroup\n\nfeat(gui): the workspace switch becomes its own segmented group — and the top bar's groups get room to breathe",
          "timestamp": "2026-10-31T22:51:20+08:00",
          "tree_id": "43b7e4bb1b41920e5806a9a565ed409eb8669ac6",
          "url": "https://github.com/LoveDaisy/Lumice/commit/33934bd7de38458727db064be595da31a2314160"
        },
        "date": 1793458537472,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 86.4,
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
            "value": 94.4,
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
          "id": "a6c9ed30e3b546ecded6de86bb35045ee50ca793",
          "message": "Merge pull request #559 from LoveDaisy/feat/query-stage-heartbeat-abi\n\nfeat: poll-stage heartbeat over the cancel token (C ABI v4.61)",
          "timestamp": "2026-11-01T03:11:41+08:00",
          "tree_id": "eef80cb7afd5f8eff4f733b0af4b7b1569895512",
          "url": "https://github.com/LoveDaisy/Lumice/commit/a6c9ed30e3b546ecded6de86bb35045ee50ca793"
        },
        "date": 1793474157130,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 88.9,
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
            "value": 93.1,
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
          "id": "dabd3f70e5c64ffd45b2e93832dcdd02e1fe19fd",
          "message": "Merge pull request #560 from LoveDaisy/fix/analyze-reanalyze-survey-validation-blocker\n\nfix(gui): synchronize analysis reselection and quiesce teardown",
          "timestamp": "2026-11-03T03:49:26+08:00",
          "tree_id": "e1322da05e898511f4110603b9c181c936522363",
          "url": "https://github.com/LoveDaisy/Lumice/commit/dabd3f70e5c64ffd45b2e93832dcdd02e1fe19fd"
        },
        "date": 1793649331353,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 96.7,
            "unit": "%"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 99.8,
            "unit": "%"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 98.1,
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
          "id": "9930c9a89fd804e79320f1faa457c33a994c3839",
          "message": "Merge pull request #562 from LoveDaisy/feat/msvc-math-macro-gate\n\nfix(policy): reject MSVC-unsafe math constant macros",
          "timestamp": "2026-11-03T04:13:23+08:00",
          "tree_id": "84c5c41207c1680acdcccc3a71c06f77fc6b7652",
          "url": "https://github.com/LoveDaisy/Lumice/commit/9930c9a89fd804e79320f1faa457c33a994c3839"
        },
        "date": 1793650655634,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 92.1,
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
            "value": 89.9,
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
          "id": "3aadae00161f1b38193db5fd3c7ac5f4535f59b3",
          "message": "Merge pull request #563 from LoveDaisy/feat/fiber-arclength-payload-probe\n\ndocs(raypath): fiber arclength payload audit for contribution curves",
          "timestamp": "2026-11-04T04:59:13+08:00",
          "tree_id": "e5adef887341388bc5d87629182c19bfa3d0f278",
          "url": "https://github.com/LoveDaisy/Lumice/commit/3aadae00161f1b38193db5fd3c7ac5f4535f59b3"
        },
        "date": 1793739843873,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 90,
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
          "id": "5f0968259b932181425988ebdb95fc4a422442bb",
          "message": "Merge pull request #564 from LoveDaisy/feat/analyze-gui-rework\n\nAnalyze GUI rework: task-driven main view, resizable regions, independent windows",
          "timestamp": "2026-11-06T04:38:13+08:00",
          "tree_id": "6d70ce4fe8476d0d71d3a15141a3c4e9c8693a71",
          "url": "https://github.com/LoveDaisy/Lumice/commit/5f0968259b932181425988ebdb95fc4a422442bb"
        },
        "date": 1793911372561,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 90,
            "unit": "%"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 100,
            "unit": "%"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 99.2,
            "unit": "%"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 92.6,
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
          "id": "3fa8e2cd61fcb3352f6adf6311731e91e2b30109",
          "message": "Merge pull request #565 from LoveDaisy/feat/executor-ordering-fixture\n\ntest(gui): executor ordering contract arms — supersede refusal, shutdown join, newest-wins",
          "timestamp": "2026-11-06T06:41:41+08:00",
          "tree_id": "ae2aea9c76fbf464eefce3a7536d023941059f41",
          "url": "https://github.com/LoveDaisy/Lumice/commit/3fa8e2cd61fcb3352f6adf6311731e91e2b30109"
        },
        "date": 1793918796236,
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
            "value": 98.3,
            "unit": "%"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 92.1,
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
          "id": "5b630fe90d2117f664e2d37c0c201b6409149997",
          "message": "Merge pull request #566 from LoveDaisy/feat/analyze-fiber-contribution-curve\n\nfeat(gui): contribution curve — named factors and product over true SO(3) arclength",
          "timestamp": "2026-11-06T12:10:06+08:00",
          "tree_id": "58873d41ff72f12c505e1cf4e4be4662d61ef2c7",
          "url": "https://github.com/LoveDaisy/Lumice/commit/5b630fe90d2117f664e2d37c0c201b6409149997"
        },
        "date": 1793938440631,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 100.3,
            "unit": "%"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 99.6,
            "unit": "%"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 100.3,
            "unit": "%"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 89.5,
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
          "id": "9e6d278e623a50ba610259478331d5ee90c34408",
          "message": "Merge pull request #567 from LoveDaisy/feat/analysis-pump-sky-intent-pickup\n\ntest(gui): the map wait streaks, the Set input asserts, and broken preconditions fail fast",
          "timestamp": "2026-11-06T13:55:02+08:00",
          "tree_id": "8aa0628662045bfe715965f94e9b5fa16ad7c406",
          "url": "https://github.com/LoveDaisy/Lumice/commit/9e6d278e623a50ba610259478331d5ee90c34408"
        },
        "date": 1793944747493,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 92.9,
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
          "id": "c1149fe20a40b4a978bc47b42a60b971ac3f9f5d",
          "message": "Merge pull request #568 from LoveDaisy/feat/scopes-gate-load-budget\n\ntest(gui): scopes gates judge recorded edges, hold in-flight actions, and drain the whole board",
          "timestamp": "2026-11-06T14:25:57+08:00",
          "tree_id": "f625921baa7c96bf5f98fc7163267e9f1701e8ac",
          "url": "https://github.com/LoveDaisy/Lumice/commit/c1149fe20a40b4a978bc47b42a60b971ac3f9f5d"
        },
        "date": 1793946584081,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 81.2,
            "unit": "%"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 100.1,
            "unit": "%"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 96.8,
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
          "id": "da8dbd629fdc05ea5ef7dc6ed218130409569f00",
          "message": "Merge pull request #569 from LoveDaisy/feat/analyze-main-direct-gestures\n\nfix(gui): direct point gestures and shared main-view zoom",
          "timestamp": "2026-11-06T20:51:42+08:00",
          "tree_id": "1034a6db0ddbe12f966cb71f61d31b197e9a04f3",
          "url": "https://github.com/LoveDaisy/Lumice/commit/da8dbd629fdc05ea5ef7dc6ed218130409569f00"
        },
        "date": 1793969715774,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 82,
            "unit": "%"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 100.2,
            "unit": "%"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 98.3,
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
          "id": "cf9972947e96e67045d69b96f68799a1d2823ef7",
          "message": "Merge pull request #570 from LoveDaisy/fix/cancel-survey-observation-scope\n\nfix(test): observe survey cancellation without global idle",
          "timestamp": "2026-11-06T23:31:50+08:00",
          "tree_id": "c2ea747b7eccb4fe178fdbcf931fe9d696c2bf9e",
          "url": "https://github.com/LoveDaisy/Lumice/commit/cf9972947e96e67045d69b96f68799a1d2823ef7"
        },
        "date": 1793979344168,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 97.5,
            "unit": "%"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 100,
            "unit": "%"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 97.9,
            "unit": "%"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 90.3,
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
          "id": "54df3277d002f0ba3e76f30952c881890c7d37e2",
          "message": "Merge pull request #571 from LoveDaisy/feat/font-glyph-lambda-range\n\nfix(gui): load missing delta and lambda glyphs",
          "timestamp": "2026-11-07T01:09:00+08:00",
          "tree_id": "ac9a9b1bfa0453c3aa3be2ae737295fa67cd70bb",
          "url": "https://github.com/LoveDaisy/Lumice/commit/54df3277d002f0ba3e76f30952c881890c7d37e2"
        },
        "date": 1793985171998,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 84.3,
            "unit": "%"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 100,
            "unit": "%"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 98.7,
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
          "id": "347a4e5c5821ff1a77061706e0e80a16f985bafc",
          "message": "Merge pull request #572 from LoveDaisy/test/map-pump-fixture-query-isolation\n\ntest(gui): remove unrelated survey work from map fixtures",
          "timestamp": "2026-11-07T02:39:25+08:00",
          "tree_id": "17372e67756c5e4a76ebad72e1ecba46bbf68e83",
          "url": "https://github.com/LoveDaisy/Lumice/commit/347a4e5c5821ff1a77061706e0e80a16f985bafc"
        },
        "date": 1793990607107,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 100.9,
            "unit": "%"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 100.1,
            "unit": "%"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 99.2,
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
          "id": "3d2c4f05d9f464d164acf08e609a75a356daeb49",
          "message": "Merge pull request #573 from LoveDaisy/feat/analyze-layout-stability\n\nfix(gui): stabilize Analyze canvases and transient status layout",
          "timestamp": "2026-11-07T04:37:00+08:00",
          "tree_id": "2400774c1f61f029a64f97fb2880a3281f427dc2",
          "url": "https://github.com/LoveDaisy/Lumice/commit/3d2c4f05d9f464d164acf08e609a75a356daeb49"
        },
        "date": 1793997668926,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 92.4,
            "unit": "%"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 100.2,
            "unit": "%"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 97.5,
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
          "id": "824e978f0926f336217b4856d55086907fca8aa5",
          "message": "Merge pull request #574 from LoveDaisy/feat/doc-switch-keeps-workspace-mode\n\nfix(gui): preserve workspace mode across document switches",
          "timestamp": "2026-11-07T07:30:56+08:00",
          "tree_id": "932431bb0a545f9c1c9ae5f6ed910a71b8e28706",
          "url": "https://github.com/LoveDaisy/Lumice/commit/824e978f0926f336217b4856d55086907fca8aa5"
        },
        "date": 1794008104365,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 88,
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
            "value": 95.7,
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
          "id": "a4719d5178481cc9a506c71cbab37271f585e3f2",
          "message": "Merge pull request #575 from LoveDaisy/feat/raypath-pose-entry\n\nfeat(gui): open Pose & trace beside sky-point controls",
          "timestamp": "2026-11-07T08:10:47+08:00",
          "tree_id": "2b3fa8c58803976a6f6537a96676351f3eb5cb61",
          "url": "https://github.com/LoveDaisy/Lumice/commit/a4719d5178481cc9a506c71cbab37271f585e3f2"
        },
        "date": 1794010486139,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 89.6,
            "unit": "%"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 100.1,
            "unit": "%"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 98.9,
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
          "id": "0dc0fc9d40c4357238282b96410646dbeab7def7",
          "message": "Merge pull request #576 from LoveDaisy/feat/toolbar-adaptive-labels\n\nfeat(gui): adapt toolbar labels consistently to available width",
          "timestamp": "2026-11-07T09:27:47+08:00",
          "tree_id": "e7f3582dec340d9a9f0787308d3de5d68c9f2051",
          "url": "https://github.com/LoveDaisy/Lumice/commit/0dc0fc9d40c4357238282b96410646dbeab7def7"
        },
        "date": 1794015117392,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 95.4,
            "unit": "%"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 100.3,
            "unit": "%"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 98.8,
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
          "id": "b3c067009d90d6d135d8cc1147f1ff371ecac4dd",
          "message": "Merge pull request #577 from LoveDaisy/feat/raypath-view-camera-unify-design\n\ndocs(gui): define shared camera and explicit map refinement contract",
          "timestamp": "2026-11-07T10:32:16+08:00",
          "tree_id": "86cb59a0680c1e62ab39c628b9b72f127da479e9",
          "url": "https://github.com/LoveDaisy/Lumice/commit/b3c067009d90d6d135d8cc1147f1ff371ecac4dd"
        },
        "date": 1794019029319,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 87.6,
            "unit": "%"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 99.9,
            "unit": "%"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 99.2,
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
          "id": "1e1a1cfbb394c00fcaf0d05c19e737facd8edab6",
          "message": "Merge pull request #578 from LoveDaisy/feat/raypath-view-camera-unify\n\nfeat(gui): share raypath camera and make map refinement explicit",
          "timestamp": "2026-11-08T01:39:29+08:00",
          "tree_id": "fd7e92e0a2fc8277d0b09a6dbc81e07e36b2cc63",
          "url": "https://github.com/LoveDaisy/Lumice/commit/1e1a1cfbb394c00fcaf0d05c19e737facd8edab6"
        },
        "date": 1794073414387,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 83,
            "unit": "%"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 100.2,
            "unit": "%"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 97.2,
            "unit": "%"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 96.3,
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
          "id": "7b978e0d8758f5a30b6523d75426d9951a0277a6",
          "message": "Merge pull request #579 from LoveDaisy/feat/point-view-quick-fixes\n\nAnalyze GUI feedback wave: point-view fixes, GL 3D view, curve axis policy, refine removal, IA draft",
          "timestamp": "2026-11-09T11:55:28+08:00",
          "tree_id": "fc149583067480fda08cea0315fab79846de9db4",
          "url": "https://github.com/LoveDaisy/Lumice/commit/7b978e0d8758f5a30b6523d75426d9951a0277a6"
        },
        "date": 1794196758801,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 89.1,
            "unit": "%"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 100.1,
            "unit": "%"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 98.1,
            "unit": "%"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 94.5,
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
          "id": "98838d24edddcd959cc61999ff6e4cefbc30f53d",
          "message": "Merge pull request #580 from LoveDaisy/feat/msvc-near-far-gate\n\nPolicy gate: reject near/far macro identifiers (MSVC C2513 class)",
          "timestamp": "2026-11-09T14:12:29+08:00",
          "tree_id": "4d670f320f1fd4f216d4f4321f3da209507ca563",
          "url": "https://github.com/LoveDaisy/Lumice/commit/98838d24edddcd959cc61999ff6e4cefbc30f53d"
        },
        "date": 1794205021449,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 88.9,
            "unit": "%"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 100.1,
            "unit": "%"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 100.5,
            "unit": "%"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 93.6,
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
          "id": "6900ae9c918346564c9aa6d5c8a9ea0273e0e881",
          "message": "Merge pull request #581 from LoveDaisy/feat/home-win-closing-leg\n\nClosing gate: optional Windows build+ctest leg on home-win (test.sh quick)",
          "timestamp": "2026-11-09T15:56:10+08:00",
          "tree_id": "62b62ac09322caec461b5c0c165053b7a8bd1489",
          "url": "https://github.com/LoveDaisy/Lumice/commit/6900ae9c918346564c9aa6d5c8a9ea0273e0e881"
        },
        "date": 1794211175363,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 91.2,
            "unit": "%"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 100.3,
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
          "id": "e0df6562454ca8358e50acf0a8b95517ddb84774",
          "message": "Merge pull request #582 from LoveDaisy/feat/analyze-ia-implementation\n\nAnalyze IA implementation: dual entries, L1-L5 layering, verdict anchors, Pose relayout (scrum 778)",
          "timestamp": "2026-11-11T02:18:48+08:00",
          "tree_id": "a98c639e8604a4f19de9b25f463f24311689ebfd",
          "url": "https://github.com/LoveDaisy/Lumice/commit/e0df6562454ca8358e50acf0a8b95517ddb84774"
        },
        "date": 1794334962312,
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
            "value": 99,
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
          "id": "c207e85cf26ec4a62529bac078bef0ffd2964f2d",
          "message": "Merge pull request #583 from LoveDaisy/feat/gate-metal-env-resilience\n\nfix(gate): metal leg env resilience - tolerant decode + build-tools pre-check",
          "timestamp": "2026-11-13T00:49:24+08:00",
          "tree_id": "a6b283103069e94aa0ec31d7665b93f3dd6ca6bd",
          "url": "https://github.com/LoveDaisy/Lumice/commit/c207e85cf26ec4a62529bac078bef0ffd2964f2d"
        },
        "date": 1794502420664,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 83.3,
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
          "id": "911161091416fcf33a80071ff156acac7f39d184",
          "message": "Merge pull request #585 from LoveDaisy/feat/analyze-gui-feedback-1112\n\nAnalyze GUI feedback wave 2026-11-12: five interaction/display fixes",
          "timestamp": "2026-11-13T20:09:47+08:00",
          "tree_id": "43ed226410e11ba6e79bffd7881dd2371767fe03",
          "url": "https://github.com/LoveDaisy/Lumice/commit/911161091416fcf33a80071ff156acac7f39d184"
        },
        "date": 1794572044507,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 87.5,
            "unit": "%"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 100.2,
            "unit": "%"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 98.2,
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
          "id": "6eeddf2babad4420037abe23b6cfb294c48fda62",
          "message": "Merge pull request #586 from LoveDaisy/feat/analyze-gui-feedback-1113\n\nAnalyze GUI fourth hands-on feedback wave: five fixes (display consistency, operation bar, camera presets, staleness unification, mechanism consolidation)",
          "timestamp": "2026-11-14T13:17:34+08:00",
          "tree_id": "9ed1355ded1c691c40e9d780f80509724780b42b",
          "url": "https://github.com/LoveDaisy/Lumice/commit/6eeddf2babad4420037abe23b6cfb294c48fda62"
        },
        "date": 1794633792284,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 89.1,
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
          "id": "3272248ebc43f184cbd0dcf977b98d987207c48b",
          "message": "Merge pull request #587 from LoveDaisy/feat/hover-readout-type-ownership\n\nrefactor(gui): return row hover types to their semantic owner (imgui-free header)",
          "timestamp": "2026-11-14T17:16:28+08:00",
          "tree_id": "a08d19bd6f3dace2676ed9b6204fbb778c85d09c",
          "url": "https://github.com/LoveDaisy/Lumice/commit/3272248ebc43f184cbd0dcf977b98d987207c48b"
        },
        "date": 1794648030126,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 91.4,
            "unit": "%"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 99.8,
            "unit": "%"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 97.7,
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
          "id": "f75491cc7628a703b19dbad2f0587bb74d0fd60a",
          "message": "Merge pull request #588 from LoveDaisy/feat/fiber-payload-refinement\n\nfeat(raypath): fiber payload assembly-layer adaptive refinement",
          "timestamp": "2026-11-14T22:59:32+08:00",
          "tree_id": "bdd20fcebfe0d0ac85c26ed74c2d564f18c0c496",
          "url": "https://github.com/LoveDaisy/Lumice/commit/f75491cc7628a703b19dbad2f0587bb74d0fd60a"
        },
        "date": 1794668866936,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 106.7,
            "unit": "%"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 99.9,
            "unit": "%"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 98,
            "unit": "%"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 94.4,
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
          "id": "22c8b8d581dac91ed8fdad5009d0dc58e0726d99",
          "message": "Merge pull request #589 from LoveDaisy/feat/gui-full-suite-env-flake\n\nFix build.sh install starvation and fast-e2e false reds; record full-suite SIGILL evidence",
          "timestamp": "2026-11-15T09:02:37+08:00",
          "tree_id": "6d8404db8b58285fb9642b543d05b44054386bb3",
          "url": "https://github.com/LoveDaisy/Lumice/commit/22c8b8d581dac91ed8fdad5009d0dc58e0726d99"
        },
        "date": 1794704786450,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 79.4,
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
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "distinct": true,
          "id": "52d4760db8da0eab613a4e8821c28c9fbf22a470",
          "message": "Merge pull request #590 from LoveDaisy/chore/release-branch-policy",
          "timestamp": "2026-11-15T10:14:25+08:00",
          "tree_id": "9f1e3ddf0ccf449ee2e2af88e676f95a914da999",
          "url": "https://github.com/LoveDaisy/Lumice/commit/52d4760db8da0eab613a4e8821c28c9fbf22a470"
        },
        "date": 1794709112835,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 97.2,
            "unit": "%"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 100.1,
            "unit": "%"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 98,
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
          "id": "0be2a1d34f5f22b099bd62e5cff395eee4233514",
          "message": "Merge pull request #591 from LoveDaisy/feat/gui-feedback-1114\n\nfeat(gui-feedback-1114): overlay projection convergence, status bar, curve region rework, 3D reset, globe colormap",
          "timestamp": "2026-11-15T20:37:40+08:00",
          "tree_id": "ee20e2bcb9393d8730a0ad997abb40f1350ecc02",
          "url": "https://github.com/LoveDaisy/Lumice/commit/0be2a1d34f5f22b099bd62e5cff395eee4233514"
        },
        "date": 1794746569321,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 106.3,
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
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "distinct": true,
          "id": "ecca2c50a981551d3f7ace5b811ae02be3440f90",
          "message": "Merge pull request #591 from LoveDaisy/feat/gui-feedback-1114\n\nfeat(gui-feedback-1114): overlay projection convergence, status bar, curve region rework, 3D reset, globe colormap",
          "timestamp": "2026-11-15T22:10:19+08:00",
          "tree_id": "ee20e2bcb9393d8730a0ad997abb40f1350ecc02",
          "url": "https://github.com/LoveDaisy/Lumice/commit/ecca2c50a981551d3f7ace5b811ae02be3440f90"
        },
        "date": 1794752223672,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 87.2,
            "unit": "%"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 99.8,
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
          "id": "a51a333e8551b7b442d82eafe5b20ffc655ac8b4",
          "message": "Merge pull request #593 from LoveDaisy/feat/sigill-ubsan-boundary-probe\n\ndocs(test): SIGILL sanitizer campaign evidence — blind samples, exit D maintained",
          "timestamp": "2026-11-16T02:36:27+08:00",
          "tree_id": "91940051fe9ac76f7b9fc26b9defb37cb0df3ad5",
          "url": "https://github.com/LoveDaisy/Lumice/commit/a51a333e8551b7b442d82eafe5b20ffc655ac8b4"
        },
        "date": 1794768028462,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 89.1,
            "unit": "%"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 100.1,
            "unit": "%"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 98,
            "unit": "%"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 95.5,
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
          "id": "46f487f792dd5402b43549dd36dba9e3d1805a08",
          "message": "Merge pull request #595 from LoveDaisy/feat/gate-gui-real-timing-leg\n\nfeat(gate): add gui-real-timing leg to the local throughput gate",
          "timestamp": "2026-11-16T04:27:19+08:00",
          "tree_id": "c7cef6104878196142c2a0ae33a82a596024d99c",
          "url": "https://github.com/LoveDaisy/Lumice/commit/46f487f792dd5402b43549dd36dba9e3d1805a08"
        },
        "date": 1794774687960,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 84.2,
            "unit": "%"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 100,
            "unit": "%"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 95.3,
            "unit": "%"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 93.6,
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
          "id": "5ecd6e9eb61b3eb52edd1c4c9527849b0a6316b9",
          "message": "Merge pull request #596 from LoveDaisy/feat/crystal-param-hash-signed-overflow\n\nfix(gui): move CrystalParamHash to uint32_t domain",
          "timestamp": "2026-11-16T05:53:54+08:00",
          "tree_id": "9e62e445f596c20f1ab5d717985d2d86536ae86e",
          "url": "https://github.com/LoveDaisy/Lumice/commit/5ecd6e9eb61b3eb52edd1c4c9527849b0a6316b9"
        },
        "date": 1794779930822,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 81.1,
            "unit": "%"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 99.8,
            "unit": "%"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 98.9,
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
          "id": "2cd2f085a930d16a2d25275a43cb4ede75694cbe",
          "message": "Merge pull request #597 from LoveDaisy/feat/lens-bump-projection-cast-ub\n\nfix(core): clamp single-lens bump cast inputs into int domain",
          "timestamp": "2026-11-16T07:33:03+08:00",
          "tree_id": "a6aef670adbec2df787761450a1d13f42ccfcd1d",
          "url": "https://github.com/LoveDaisy/Lumice/commit/2cd2f085a930d16a2d25275a43cb4ede75694cbe"
        },
        "date": 1794785809632,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 92.8,
            "unit": "%"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 100,
            "unit": "%"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 98.4,
            "unit": "%"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 92.1,
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
          "id": "70305fd23cf25cb24d9987c106de61316b403a4f",
          "message": "Merge pull request #598 from LoveDaisy/feat/realtiming-pool-expectation-drift\n\ntest(gui): re-pin four real-timing pool expectations drifted by the 778/779/785 waves",
          "timestamp": "2026-11-16T09:40:03+08:00",
          "tree_id": "137c9cb2e06f9ff7ec70a69102e1af2531cdd335",
          "url": "https://github.com/LoveDaisy/Lumice/commit/70305fd23cf25cb24d9987c106de61316b403a4f"
        },
        "date": 1794793415365,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 90.9,
            "unit": "%"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 100,
            "unit": "%"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 99.2,
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
          "id": "29ce75c3d157f08d2b306aa99c3643c57102c5f6",
          "message": "Merge pull request #601 from LoveDaisy/feat/profile-rows-unstable-red\n\ntest(task-profile-rows-unstable-red): stabilize the profile_rows real-timing case's two flake shapes",
          "timestamp": "2026-11-16T15:35:55+08:00",
          "tree_id": "f72d4b690c2de7bacd14b8fe44ecd7950334e852",
          "url": "https://github.com/LoveDaisy/Lumice/commit/29ce75c3d157f08d2b306aa99c3643c57102c5f6"
        },
        "date": 1794814802243,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 94.7,
            "unit": "%"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 100,
            "unit": "%"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 103.1,
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
          "id": "f1a4ee0693e882f9731c1bb18fd73b97231f8ca6",
          "message": "Merge pull request #602 from LoveDaisy/port/476-user-fixes\n\nPort the 788 user-fix wave from release/4.7.6 to main (from #592)",
          "timestamp": "2026-11-16T19:16:24+08:00",
          "tree_id": "eccda5ef6ecd72f7e60c4a49d05573366185d167",
          "url": "https://github.com/LoveDaisy/Lumice/commit/f1a4ee0693e882f9731c1bb18fd73b97231f8ca6"
        },
        "date": 1794828030983,
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
            "value": 98.6,
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
          "id": "0d50cb0d867067018388289b90ba011d7f842853",
          "message": "Merge pull request #603 from LoveDaisy/port/476-feedback-1115\n\nfix(gui): port the 11-15 feedback wave to main — raypath baseline, row wash, tooltips, preview style default",
          "timestamp": "2026-11-16T21:27:17+08:00",
          "tree_id": "43578551cc6f0737a662e1b9d123abea33965b33",
          "url": "https://github.com/LoveDaisy/Lumice/commit/0d50cb0d867067018388289b90ba011d7f842853"
        },
        "date": 1794835974674,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 95.6,
            "unit": "%"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 100.4,
            "unit": "%"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 99,
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
          "id": "5150ba87256070aa13503b417e35208ec7ad7214",
          "message": "Merge pull request #604 from LoveDaisy/port/476-release-doc-amendment\n\ndocs(release): retire release branches via cherry-pick ports, not forward-merge",
          "timestamp": "2026-11-16T22:03:08+08:00",
          "tree_id": "30c74249e200d758d9dc20737846c06868c889f2",
          "url": "https://github.com/LoveDaisy/Lumice/commit/5150ba87256070aa13503b417e35208ec7ad7214"
        },
        "date": 1794838036226,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 86.1,
            "unit": "%"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 100,
            "unit": "%"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 97.4,
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
          "id": "78e6839c6629dff1eb07098252abc53f23228c3f",
          "message": "Merge pull request #614 from LoveDaisy/feat/gate-per-leg-idle-retry\n\nfeat(gate): per-leg idle retry with 06:30 nightly deadline wall",
          "timestamp": "2026-11-17T03:43:31+08:00",
          "tree_id": "a45e11ee656596171d7a83a357d1840524c9260a",
          "url": "https://github.com/LoveDaisy/Lumice/commit/78e6839c6629dff1eb07098252abc53f23228c3f"
        },
        "date": 1794858548484,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 79.2,
            "unit": "%"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 99.9,
            "unit": "%"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 98.8,
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
          "id": "263a21c315c2f636a6a584eb1a035d83ac189d61",
          "message": "Merge pull request #616 from LoveDaisy/feat/gate-ubsan-leg\n\nfeat(gate): add the ubsan leg - nightly UB probe gate",
          "timestamp": "2026-11-17T06:40:57+08:00",
          "tree_id": "aff4ceb10deea2418ebf7c4fc4169e692c50991c",
          "url": "https://github.com/LoveDaisy/Lumice/commit/263a21c315c2f636a6a584eb1a035d83ac189d61"
        },
        "date": 1794869069440,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 82.2,
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
          "id": "a8533ca7a68228cf2eed6d461ae87db2ea03f822",
          "message": "Merge pull request #617 from LoveDaisy/feat/gui-feedback-1117\n\nGUI feedback wave 2026-11-17: tooltip two-tier rule, Ray3D region rework, globe Fit/Full views",
          "timestamp": "2026-11-18T09:53:57+08:00",
          "tree_id": "8cb29f609a7727ad38280d90429697f273fe4803",
          "url": "https://github.com/LoveDaisy/Lumice/commit/a8533ca7a68228cf2eed6d461ae87db2ea03f822"
        },
        "date": 1794967145412,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 89.4,
            "unit": "%"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 99.9,
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
          "id": "14c782141958dfa028fa7a884b6858d45753a67b",
          "message": "Merge pull request #618 from LoveDaisy/feat/gui-feedback-1118\n\nfix: address GUI raypath analysis feedback from November 18",
          "timestamp": "2026-11-19T19:03:16+08:00",
          "tree_id": "66ecc42f7e82780ba7e0c083dae012cfa0c90fa8",
          "url": "https://github.com/LoveDaisy/Lumice/commit/14c782141958dfa028fa7a884b6858d45753a67b"
        },
        "date": 1795086413775,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 97.2,
            "unit": "%"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 99.8,
            "unit": "%"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 97.9,
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
          "id": "5a0e7c13c527f94f0e179a4f54512b53016840db",
          "message": "Merge pull request #620 from LoveDaisy/feat/reference-regen-output-decoding\n\nfix(scripts): tolerate non-UTF-8 bytes in regen external output capture",
          "timestamp": "2026-11-20T13:09:18+08:00",
          "tree_id": "9a7772a7e2f1a3e6e153fee31daf6561459fa8fb",
          "url": "https://github.com/LoveDaisy/Lumice/commit/5a0e7c13c527f94f0e179a4f54512b53016840db"
        },
        "date": 1795151600006,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 81,
            "unit": "%"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 99.9,
            "unit": "%"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 98.4,
            "unit": "%"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 94.5,
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
          "id": "22d80d01c1a1a170df818d7183cde33931a70edd",
          "message": "Merge pull request #621 from LoveDaisy/feat/axis-compact-scaled-chip-clip\n\nfix(gui): fit axis chip gaps to live pane width",
          "timestamp": "2026-11-20T20:31:36+08:00",
          "tree_id": "564b2fb47651ddd7a50c6e6f5982920937e0d49c",
          "url": "https://github.com/LoveDaisy/Lumice/commit/22d80d01c1a1a170df818d7183cde33931a70edd"
        },
        "date": 1795178119640,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 93.5,
            "unit": "%"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 100.2,
            "unit": "%"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 98.4,
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
          "id": "7febf6d10091602d355d0238a51937df978814df",
          "message": "Merge pull request #622 from LoveDaisy/feat/small-button-glyph-clip\n\nfix(gui): preserve SmallButton descender ink at scaled line height",
          "timestamp": "2026-11-21T06:50:01+08:00",
          "tree_id": "491d95044f79f070d1681aea38134ce614726abf",
          "url": "https://github.com/LoveDaisy/Lumice/commit/7febf6d10091602d355d0238a51937df978814df"
        },
        "date": 1795215401872,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 85.1,
            "unit": "%"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 100.3,
            "unit": "%"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 98.4,
            "unit": "%"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 94.4,
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
          "id": "d685b0569327ac7aea6e1e534b775274ad0194e4",
          "message": "Merge pull request #623 from LoveDaisy/feat/metal-exit-wsum-main-port\n\nfix(metal): reduce exit weights per SIMD group (port from release/4.7.7)",
          "timestamp": "2026-11-21T07:08:10+08:00",
          "tree_id": "46894b9cf42864f233a143255b8be541dd2329de",
          "url": "https://github.com/LoveDaisy/Lumice/commit/d685b0569327ac7aea6e1e534b775274ad0194e4"
        },
        "date": 1795216385689,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 94.7,
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
          "id": "c398905ca960146043188906e46deeff6a25fc1d",
          "message": "Merge pull request #624 from LoveDaisy/feat/precommit-hook-delegator\n\nbuild(hooks): delegate pre-commit to the committing checkout",
          "timestamp": "2026-11-21T07:24:39+08:00",
          "tree_id": "445bde2ae1e12961c92c6fcc8706028c8e638ac5",
          "url": "https://github.com/LoveDaisy/Lumice/commit/c398905ca960146043188906e46deeff6a25fc1d"
        },
        "date": 1795217341951,
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
            "value": 97.2,
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
          "id": "02385c457d9f24d3cb2d29aa789aa52bad68f4ab",
          "message": "Merge pull request #625 from LoveDaisy/feat/expanded-column-width-fix\n\nfix(gui): scale Expanded column widths in layout units",
          "timestamp": "2026-11-21T07:45:27+08:00",
          "tree_id": "5f78a5bdffc3bb8aba0fae12b5f23b26efddb7b8",
          "url": "https://github.com/LoveDaisy/Lumice/commit/02385c457d9f24d3cb2d29aa789aa52bad68f4ab"
        },
        "date": 1795218523264,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 96.9,
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
          "id": "a9b0149b1aa7bb53a1a0e521d99af42a3548f068",
          "message": "Merge pull request #626 from LoveDaisy/docs/gate-duration-disposition\n\ntest(budget): record the 2026-11-20/21 ctest disposition (reason-only)",
          "timestamp": "2026-11-21T08:04:03+08:00",
          "tree_id": "a142292d0432fc5976b070cfacacb6fbb7779e7c",
          "url": "https://github.com/LoveDaisy/Lumice/commit/a9b0149b1aa7bb53a1a0e521d99af42a3548f068"
        },
        "date": 1795219660141,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 86.5,
            "unit": "%"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 99.8,
            "unit": "%"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 98.5,
            "unit": "%"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 102.7,
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
          "id": "2c7a6f417d3de05ec6e1adad2fb111874175abe7",
          "message": "Merge pull request #628 from LoveDaisy/feat/analyze-presentation-first-batch\n\nfeat(gui): analyze presentation first batch",
          "timestamp": "2026-11-23T09:50:08+08:00",
          "tree_id": "db72a497c926bbf8ae62b1b32b9a14f657839492",
          "url": "https://github.com/LoveDaisy/Lumice/commit/2c7a6f417d3de05ec6e1adad2fb111874175abe7"
        },
        "date": 1795398899452,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 89.5,
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
          "id": "0a1784dc996df84bf94c4ff774ba5f33d0560fac",
          "message": "Merge pull request #631 from LoveDaisy/feat/map-request-symmetry-bits\n\nfeat(capi,map): carry true symmetry bits in map requests",
          "timestamp": "2026-11-23T10:15:59+08:00",
          "tree_id": "662a5fd31b8a2a2fde9798f38adc9ad4e5401e8f",
          "url": "https://github.com/LoveDaisy/Lumice/commit/0a1784dc996df84bf94c4ff774ba5f33d0560fac"
        },
        "date": 1795400402069,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 94.1,
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
          "id": "1efc673fc1346637961b1cbf3f127abe1539fe48",
          "message": "Merge pull request #633 from LoveDaisy/chore/ci-release-push-trigger\n\nci: trigger the workflow on release/** pushes",
          "timestamp": "2026-11-23T11:52:05+08:00",
          "tree_id": "8ea5c55e318e2da81806571f958e790d6e2704c8",
          "url": "https://github.com/LoveDaisy/Lumice/commit/1efc673fc1346637961b1cbf3f127abe1539fe48"
        },
        "date": 1795406193217,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 91,
            "unit": "%"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 99.8,
            "unit": "%"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 98.9,
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
          "id": "349a4270504e39cc3c291c4257052477bfb36888",
          "message": "Merge pull request #636 from LoveDaisy/feat/local-class-algebra\n\nfeat(raypath,capi): local-class algebra — point-selection representatives, log density facts, interpretive chain",
          "timestamp": "2026-11-24T02:07:01+08:00",
          "tree_id": "594868057c65dc035af6aee5110e333dc189228c",
          "url": "https://github.com/LoveDaisy/Lumice/commit/349a4270504e39cc3c291c4257052477bfb36888"
        },
        "date": 1795457455365,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 81.1,
            "unit": "%"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 100,
            "unit": "%"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 98.4,
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
          "id": "8d161eba2c6c4dde9e29651ca26b4ba7e5bc8be4",
          "message": "Merge pull request #639 from LoveDaisy/feat/map-session-test-contract\n\ntest(gui): decouple map lifecycle cases from the full-budget compute",
          "timestamp": "2026-11-24T15:14:15+08:00",
          "tree_id": "b15b27b5a50961505b81cf5b4afd60f0895365f4",
          "url": "https://github.com/LoveDaisy/Lumice/commit/8d161eba2c6c4dde9e29651ca26b4ba7e5bc8be4"
        },
        "date": 1795504693000,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 91,
            "unit": "%"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 100,
            "unit": "%"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 99.2,
            "unit": "%"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 88.8,
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
          "id": "390c995b286acbd17618dd50ae2adb3b2498a024",
          "message": "Merge pull request #642 from LoveDaisy/feat/metal-gate-blocking-scope\n\nci(e2e-slow): Metal dual-renderer throughput ratio becomes a reported signal",
          "timestamp": "2026-11-24T16:18:36+08:00",
          "tree_id": "f4143a5d0a9e1378f579c84b5b9e1a36183c4021",
          "url": "https://github.com/LoveDaisy/Lumice/commit/390c995b286acbd17618dd50ae2adb3b2498a024"
        },
        "date": 1795508660801,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 87.4,
            "unit": "%"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 100.2,
            "unit": "%"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 99.2,
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
          "id": "73891fd38b1448ff47bc0a3976104785c9a48413",
          "message": "Merge pull request #646 from LoveDaisy/feat/ctest-headroom-tracker\n\nfeat(scripts): ctest headroom tracker — implied-C/headroom observation with streak signals",
          "timestamp": "2026-11-24T21:22:44+08:00",
          "tree_id": "3abfec62a61fd7542d9d5f18868b4bebfbea82c6",
          "url": "https://github.com/LoveDaisy/Lumice/commit/73891fd38b1448ff47bc0a3976104785c9a48413"
        },
        "date": 1795527811937,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 83.1,
            "unit": "%"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 100,
            "unit": "%"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 97.9,
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
          "id": "c8b8f111be3f7aa884289413e955ee9e54a3e1a4",
          "message": "Merge pull request #647 from LoveDaisy/port/477-infra\n\nPort release/4.7.7 infra cluster to main (#619 #627 #630 #632)",
          "timestamp": "2026-11-25T00:32:29+08:00",
          "tree_id": "907721e98226835ad1da12a6903ca476eb47deb4",
          "url": "https://github.com/LoveDaisy/Lumice/commit/c8b8f111be3f7aa884289413e955ee9e54a3e1a4"
        },
        "date": 1795539683917,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 85.5,
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
          "id": "af73a164c48b2e48472d2cbd4cb07bc215f6fa5d",
          "message": "Merge pull request #648 from LoveDaisy/port/477-interaction\n\nPort the 4.7.7 interaction/UI cluster to main (#635 #638 #641 #644)",
          "timestamp": "2026-11-25T05:25:15+08:00",
          "tree_id": "a92be6ef32b900df06a2ed42efdf111ed38b4a31",
          "url": "https://github.com/LoveDaisy/Lumice/commit/af73a164c48b2e48472d2cbd4cb07bc215f6fa5d"
        },
        "date": 1795556976748,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 80.5,
            "unit": "%"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 100.1,
            "unit": "%"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 95.7,
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
          "id": "183da9daba02fa2ee60fc32b50f0bf60412e6e5a",
          "message": "Merge pull request #649 from LoveDaisy/chore/release-477-main-backfill\n\ndocs(changelog): backfill 4.7.7 section",
          "timestamp": "2026-11-25T05:54:37+08:00",
          "tree_id": "9403fad84a0837cffad25ce35cb2ec8e7d0b86f7",
          "url": "https://github.com/LoveDaisy/Lumice/commit/183da9daba02fa2ee60fc32b50f0bf60412e6e5a"
        },
        "date": 1795558218091,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 86.4,
            "unit": "%"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 99.9,
            "unit": "%"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 97.6,
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
          "id": "ab73dccde5f9f36162d9599e10b674ec95e2a17a",
          "message": "Merge pull request #653 from LoveDaisy/feat/policy-gate-timebomb-fixture\n\ntest(scripts): decouple tracker policy-gate fixtures from the wall clock",
          "timestamp": "2026-11-26T03:04:35+08:00",
          "tree_id": "4a8e9b71faed5f6dd968776cdd6079439ec02f62",
          "url": "https://github.com/LoveDaisy/Lumice/commit/ab73dccde5f9f36162d9599e10b674ec95e2a17a"
        },
        "date": 1795634565151,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 90.9,
            "unit": "%"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 99.6,
            "unit": "%"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 98.4,
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
          "id": "846c30008635ac506d0664114c44b96142b87da4",
          "message": "Merge pull request #651 from LoveDaisy/feat/caustic-marker-declutter\n\nfix(analyze-map): declutter caustic markers with own layer group and pile aggregation",
          "timestamp": "2026-11-26T03:35:17+08:00",
          "tree_id": "d7afd09f3cd1e70dcff66b286135f5160cf3c1c4",
          "url": "https://github.com/LoveDaisy/Lumice/commit/846c30008635ac506d0664114c44b96142b87da4"
        },
        "date": 1795636856700,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 88.7,
            "unit": "%"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 100.1,
            "unit": "%"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 97.9,
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
          "id": "2075f95b8a98f5716d7e6792cb070ecfae282307",
          "message": "Merge pull request #655 from LoveDaisy/feat/raypath-layers-ui\n\nfeat(analyze-map): themed table replaces the bare layer switches",
          "timestamp": "2026-11-26T07:44:03+08:00",
          "tree_id": "c35f36bd1be756dc2a2d06b06323bb86c8b3c330",
          "url": "https://github.com/LoveDaisy/Lumice/commit/2075f95b8a98f5716d7e6792cb070ecfae282307"
        },
        "date": 1795650920674,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 84.7,
            "unit": "%"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 100,
            "unit": "%"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 97.9,
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
          "id": "b8fe6c8550932e7aff53ee851f4ba48b9ed93f40",
          "message": "Merge pull request #652 from LoveDaisy/feat/rho-deep-tail-curve\n\nfix(gui): rho curve reads log densities on an independent axis; adopt the interpretive branch",
          "timestamp": "2026-11-26T09:02:03+08:00",
          "tree_id": "22ed15e99f07f7620cb8b9a1a189970bf09e1edf",
          "url": "https://github.com/LoveDaisy/Lumice/commit/b8fe6c8550932e7aff53ee851f4ba48b9ed93f40"
        },
        "date": 1795656448027,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 100.1,
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
          "id": "6f2d696ad1743bef647f6647b1b87f3bdb7e276d",
          "message": "Merge pull request #658 from LoveDaisy/feat/parry-survey-nonterminal-hang\n\ntest(gui): single-source the survey partial-budget note (Parry survey hang verdict)",
          "timestamp": "2026-11-26T17:24:07+08:00",
          "tree_id": "5db2995bb95caa5a58cf49210337423e630afc4d",
          "url": "https://github.com/LoveDaisy/Lumice/commit/6f2d696ad1743bef647f6647b1b87f3bdb7e276d"
        },
        "date": 1795686119487,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 109.2,
            "unit": "%"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 99.9,
            "unit": "%"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 97.5,
            "unit": "%"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 97.5,
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
          "id": "ade1b7284b5b65885cc0128475b9945a45d1cf42",
          "message": "Merge pull request #659 from LoveDaisy/feat/arc-recovery-engine\n\nfeat: ordered fold-arc recovery — the caustic cards become drawable, pickable arcs",
          "timestamp": "2026-11-26T23:28:25+08:00",
          "tree_id": "256b9d8575ae467edf24be35717238970dfd46e2",
          "url": "https://github.com/LoveDaisy/Lumice/commit/ade1b7284b5b65885cc0128475b9945a45d1cf42"
        },
        "date": 1795708859020,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 90.3,
            "unit": "%"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 100.1,
            "unit": "%"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 97.8,
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
          "id": "69d5e0e8d16b6a7134876bbf120530716abe9acc",
          "message": "Merge pull request #660 from LoveDaisy/goal/add-2d-j-0-caustic-continuation-and-posecurve-in\n\nfeat(analytic): add 2D J=0 caustic continuation and unified PoseCurve",
          "timestamp": "2026-11-27T20:18:13+08:00",
          "tree_id": "92cce1de4d6d2d6168871034b0d63f748257f900",
          "url": "https://github.com/LoveDaisy/Lumice/commit/69d5e0e8d16b6a7134876bbf120530716abe9acc"
        },
        "date": 1795782970954,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 76.7,
            "unit": "%"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 100.3,
            "unit": "%"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 99,
            "unit": "%"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 93.6,
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
          "id": "db80d09847e73993c42c92dd86a70188c8154bf8",
          "message": "Merge pull request #661 from LoveDaisy/goal/refactor-caustic-continuation-to-o-1-residual-co\n\nrefactor(analytic): switch caustic continuation to O(1) J_reg contract and split modules",
          "timestamp": "2026-11-28T02:30:54+08:00",
          "tree_id": "7d2b9b61cdb956630a349cb546570bdbeeea9683",
          "url": "https://github.com/LoveDaisy/Lumice/commit/db80d09847e73993c42c92dd86a70188c8154bf8"
        },
        "date": 1795805364615,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 97,
            "unit": "%"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 100,
            "unit": "%"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 97.4,
            "unit": "%"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 93,
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
          "id": "6d2a8a79915f62a319ee93debc11f4f69a027dc5",
          "message": "Merge pull request #663 from LoveDaisy/chore/duration-budget-unblock\n\nRaise ctest phase budgets: macOS 560->660, Ubuntu x86_64 360->420 (transitional)",
          "timestamp": "2026-11-28T12:43:40+08:00",
          "tree_id": "0b672b4dc85982dc426c9700348a861ed3cb42c9",
          "url": "https://github.com/LoveDaisy/Lumice/commit/6d2a8a79915f62a319ee93debc11f4f69a027dc5"
        },
        "date": 1795842345723,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 98.5,
            "unit": "%"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 99.7,
            "unit": "%"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 98.3,
            "unit": "%"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 87.3,
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
          "id": "f6818f15b3c4692f2442f1cc67388528deb0556a",
          "message": "Merge pull request #662 from LoveDaisy/goal/add-pathintrinsic-cached-per-path-intrinsic-obje\n\nAdd PathIntrinsic: cached per-path intrinsic object over verified analytic assets",
          "timestamp": "2026-11-28T13:10:01+08:00",
          "tree_id": "3f13d52cd70cf6532691fe2533e37147bd73f5bb",
          "url": "https://github.com/LoveDaisy/Lumice/commit/f6818f15b3c4692f2442f1cc67388528deb0556a"
        },
        "date": 1795843714738,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 102.3,
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
          "id": "45ae9033274b82965bdb6ce727f413a1ee80ac2d",
          "message": "Merge pull request #664 from LoveDaisy/goal/add-axisposedensity-exact-pose-density-for-all-a\n\nAdd AxisPoseDensity: exact pose density for all AxisDistribution types",
          "timestamp": "2026-11-28T13:44:48+08:00",
          "tree_id": "87de178a912fdc0dea12eaf3ddc139b17c8b8606",
          "url": "https://github.com/LoveDaisy/Lumice/commit/45ae9033274b82965bdb6ce727f413a1ee80ac2d"
        },
        "date": 1795845385918,
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
            "value": 98.5,
            "unit": "%"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 94.2,
            "unit": "%"
          }
        ]
      }
    ]
  }
}