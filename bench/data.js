window.BENCHMARK_DATA = {
  "lastUpdate": 1792591358804,
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
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "edebcf3766408a5da0e9750157314d7325aaa469",
          "message": "Merge pull request #483 from LoveDaisy/feat/look-at-target-lock\n\nfeat(gui): add Look At target locking with manual takeover",
          "timestamp": "2026-10-11T21:06:44+08:00",
          "tree_id": "fb7774db9dcb6b30ed6ab81c83175faa1687100b",
          "url": "https://github.com/LoveDaisy/Lumice/commit/edebcf3766408a5da0e9750157314d7325aaa469"
        },
        "date": 1791724195125,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 372773.6,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 504533.2,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 364113.7,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 315393.8,
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
          "id": "edebcf3766408a5da0e9750157314d7325aaa469",
          "message": "Merge pull request #483 from LoveDaisy/feat/look-at-target-lock\n\nfeat(gui): add Look At target locking with manual takeover",
          "timestamp": "2026-10-11T21:06:44+08:00",
          "tree_id": "fb7774db9dcb6b30ed6ab81c83175faa1687100b",
          "url": "https://github.com/LoveDaisy/Lumice/commit/edebcf3766408a5da0e9750157314d7325aaa469"
        },
        "date": 1791725227039,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 305562.4,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 506677.5,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 365603.6,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 338180,
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
          "id": "87a9d3bd2b85c0f0373e5ecf32c77bd163a39cb6",
          "message": "Merge pull request #484 from LoveDaisy/chore/release-4.7.3\n\nchore(release): cut 4.7.3",
          "timestamp": "2026-10-12T13:03:05+08:00",
          "tree_id": "dc8ccddd040a6828e7b0f0bd8e0d347da506521e",
          "url": "https://github.com/LoveDaisy/Lumice/commit/87a9d3bd2b85c0f0373e5ecf32c77bd163a39cb6"
        },
        "date": 1791781587157,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 308618.6,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 505356.2,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 363220.1,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 368595.4,
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
          "id": "46671819d8f4c160a624710d644cfca2c5892361",
          "message": "Merge pull request #485 from LoveDaisy/chore/migrate-repo-name-lumice\n\ndocs: migrate repo name ice_halo_sim to Lumice",
          "timestamp": "2026-10-12T13:34:20+08:00",
          "tree_id": "aec7c127f5820de64118e9f6a204ec6cb003c8ff",
          "url": "https://github.com/LoveDaisy/Lumice/commit/46671819d8f4c160a624710d644cfca2c5892361"
        },
        "date": 1791783420485,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 393522.5,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 505750.2,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 545899.4,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 9V45 96-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 373272.3,
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
          "id": "e1ac9076ac5ba61fb78e51027bd35f89f6be05d7",
          "message": "Merge pull request #486 from LoveDaisy/fix/sun-diameter-zero-min\n\nfix(gui): allow sun diameter 0 (point source) in the GUI domain",
          "timestamp": "2026-10-12T20:22:38+08:00",
          "tree_id": "306c8309b25fc330b0d3e620e8b4ba4b488264b3",
          "url": "https://github.com/LoveDaisy/Lumice/commit/e1ac9076ac5ba61fb78e51027bd35f89f6be05d7"
        },
        "date": 1791807947383,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 349737.1,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 506600,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 553796.7,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 9V45 96-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 311691.8,
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
          "id": "cdb4c383c1637dc4de908ea64859de306a7ec22c",
          "message": "Merge pull request #487 from LoveDaisy/fix/look-at-row-alignment\n\nfix(gui): align the View Look At row to the panel's control grid",
          "timestamp": "2026-10-12T22:17:52+08:00",
          "tree_id": "684e0d405dc952e70143ce6cf8d78cd922de7278",
          "url": "https://github.com/LoveDaisy/Lumice/commit/cdb4c383c1637dc4de908ea64859de306a7ec22c"
        },
        "date": 1791815055691,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 344357.2,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 503499.9,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 469007.2,
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
          "id": "962a92222a309a30e539b51a23aaafe5da021605",
          "message": "Merge pull request #488 from LoveDaisy/fix/ci-seed-audit-false-positive\n\nfix(ci): fix ccache seed audit false positive (one write per miss)",
          "timestamp": "2026-10-13T00:11:07+08:00",
          "tree_id": "b8ad04d108363e75b26db167440a99bf58627e1e",
          "url": "https://github.com/LoveDaisy/Lumice/commit/962a92222a309a30e539b51a23aaafe5da021605"
        },
        "date": 1791821666421,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 385903.6,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 506983.6,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 366466.9,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 311742,
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
          "id": "b45848cea4b9c09ffeec37e4f3f442ae13cd62e6",
          "message": "Merge pull request #489 from LoveDaisy/fix/look-at-row-grid-polish\n\nFix: revert right panel to 300px and end the Look At row on the Lock square",
          "timestamp": "2026-10-13T07:07:41+08:00",
          "tree_id": "5b2702f367d17c7dd47f5ddeb66a544d1cd3c8e1",
          "url": "https://github.com/LoveDaisy/Lumice/commit/b45848cea4b9c09ffeec37e4f3f442ae13cd62e6"
        },
        "date": 1791846636498,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 346695.5,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 505387.6,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 363833.5,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 498390.5,
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
          "id": "5b267889368334c2aed943d7bae310becb382cba",
          "message": "Merge pull request #491 from LoveDaisy/chore/release-4.7.3-recut\n\nchore(release): recut 4.7.3",
          "timestamp": "2026-10-13T07:28:24+08:00",
          "tree_id": "b91244d1d5185b200796d088072f6fd80ca186d1",
          "url": "https://github.com/LoveDaisy/Lumice/commit/5b267889368334c2aed943d7bae310becb382cba"
        },
        "date": 1791847899190,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 366319.4,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 506195.7,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 363690.5,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 309985.1,
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
          "id": "7ce6259807c4d988af8af1a1bee70b276ca2173e",
          "message": "Merge pull request #492 from LoveDaisy/chore/raypath-docs-current-state\n\ndocs(raypath): align analysis docs with the current product state",
          "timestamp": "2026-10-14T18:27:14+08:00",
          "tree_id": "b5ab7d4c031478152f3808435eb5a75fa97f6fbf",
          "url": "https://github.com/LoveDaisy/Lumice/commit/7ce6259807c4d988af8af1a1bee70b276ca2173e"
        },
        "date": 1791973839280,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 309974.1,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 503418.4,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 365506,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 310211.2,
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
          "id": "d732ea5937ccbe41714459898520e6607d066b12",
          "message": "Merge pull request #493 from LoveDaisy/fix/gui-test-window-size-leak\n\ntest(gui): restore shared window basis at case entry with a leak sentinel",
          "timestamp": "2026-10-14T20:16:19+08:00",
          "tree_id": "18a408014d7f5b0bdfc2b2667c2dd6fa2d21835e",
          "url": "https://github.com/LoveDaisy/Lumice/commit/d732ea5937ccbe41714459898520e6607d066b12"
        },
        "date": 1791980357201,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 349864.8,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 505380.6,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 366844.5,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 372232.6,
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
          "id": "92c54af058536b731f3f2a7bc50a444704146b7f",
          "message": "Merge pull request #494 from LoveDaisy/feat/analysis-crystal-id-mismatch\n\nfix(gui): align analysis list crystal numbers with card IDs",
          "timestamp": "2026-10-15T01:51:21+08:00",
          "tree_id": "d43c91f95f5daa0bd5c9b3c56b99635d451c6af2",
          "url": "https://github.com/LoveDaisy/Lumice/commit/92c54af058536b731f3f2a7bc50a444704146b7f"
        },
        "date": 1792000467433,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 372381.2,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 503619.7,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 425647.9,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 9V74 80-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 290582.5,
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
          "id": "e24525033315c4fa88cce6199342f7f62cee94d3",
          "message": "Merge pull request #495 from LoveDaisy/feat/axis-sticky-label\n\nfeat(gui): sticky user-chosen axis label, decoupled from preset apply",
          "timestamp": "2026-10-16T09:27:12+08:00",
          "tree_id": "839d54c4e434bf7ae6b44932a72e5fcd24c3bdac",
          "url": "https://github.com/LoveDaisy/Lumice/commit/e24525033315c4fa88cce6199342f7f62cee94d3"
        },
        "date": 1792114218616,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 341853.3,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 505849.6,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 427171.5,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 9V74 80-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 370640.5,
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
          "id": "d3b5b77505182ea5e0fff0873d106cc506c38f0c",
          "message": "Merge pull request #496 from LoveDaisy/feat/axis-apply-decouple\n\nfeat(gui): Apply preset is a pure parameter shortcut, label and view untouched",
          "timestamp": "2026-10-16T21:21:19+08:00",
          "tree_id": "4d4ba181a95d82bf92b42afdd4e29619e4d7710c",
          "url": "https://github.com/LoveDaisy/Lumice/commit/d3b5b77505182ea5e0fff0873d106cc506c38f0c"
        },
        "date": 1792157049328,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 329859.1,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 504128.4,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 425721.4,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 9V74 80-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 499058.7,
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
          "id": "c3372ae22b354c4456f0c52e19980930451b511a",
          "message": "Merge pull request #497 from LoveDaisy/feat/entry-edit-weight-toggle\n\nfeat(gui): entry bar in Edit Entry — weight and participation toggle in the modal",
          "timestamp": "2026-10-16T22:18:44+08:00",
          "tree_id": "1b15038814229189d3747ad68a70aad46c55ed88",
          "url": "https://github.com/LoveDaisy/Lumice/commit/c3372ae22b354c4456f0c52e19980930451b511a"
        },
        "date": 1792160492251,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 351547.1,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 506160.3,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 429632.5,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 9V74 80-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 481807.7,
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
          "id": "661f6814adafc3ae410096f9f67ed0cbcfe37403",
          "message": "Merge pull request #498 from LoveDaisy/chore/doc-topic-organization\n\ndocs: organize documentation into topic directories",
          "timestamp": "2026-10-17T01:50:13+08:00",
          "tree_id": "c1cf0a07d8d95b945e5279c062b26efaf6c926b0",
          "url": "https://github.com/LoveDaisy/Lumice/commit/661f6814adafc3ae410096f9f67ed0cbcfe37403"
        },
        "date": 1792173200286,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 405752.6,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 504222.8,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 367247,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 309514.1,
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
          "id": "a8a3d13f7e30f924bc710a8e6693a2e9a3cfc20c",
          "message": "Merge pull request #500 from LoveDaisy/docs/doc-code-consistency\n\ndocs: reconcile guides and architecture with current implementation",
          "timestamp": "2026-10-17T12:16:41+08:00",
          "tree_id": "0c9364b710adfa1f3ffbca4ad5afb4d536b131ab",
          "url": "https://github.com/LoveDaisy/Lumice/commit/a8a3d13f7e30f924bc710a8e6693a2e9a3cfc20c"
        },
        "date": 1792210800997,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 329530.2,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 504666.3,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 399511.1,
            "unit": "rays/sec",
            "extra": "CPU: INTEL(R) XEON(R) PLATINUM 8573C\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 309413.5,
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
          "id": "7578ffac709fd28e350dc9de11f7cd14c68c6bd0",
          "message": "Merge pull request #501 from LoveDaisy/fix/xyz-stats-current-capi\n\nFix dump_xyz_stats.py: consume current C API contract via CLI npy export",
          "timestamp": "2026-10-17T15:52:48+08:00",
          "tree_id": "b76c99c2e7759bc60276b944ee897ddb60ef96a4",
          "url": "https://github.com/LoveDaisy/Lumice/commit/7578ffac709fd28e350dc9de11f7cd14c68c6bd0"
        },
        "date": 1792223715903,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 327955.4,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 504214.7,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 481738.9,
            "unit": "rays/sec",
            "extra": "CPU: Intel(R) Xeon(R) 6973P-C\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 480727.9,
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
          "id": "2d8b602f597c2077eef981ad5ed97922620caecc",
          "message": "Merge pull request #502 from LoveDaisy/fix/cli-verbose-help-level\n\nfix(cli): describe -v as verbose level in help text",
          "timestamp": "2026-10-17T16:38:00+08:00",
          "tree_id": "ee41ffe66d42cacfc9fedf0ceb7e26dc02a0ea97",
          "url": "https://github.com/LoveDaisy/Lumice/commit/2d8b602f597c2077eef981ad5ed97922620caecc"
        },
        "date": 1792226460167,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 342140.9,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 504809.8,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 364247.8,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 484073.5,
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
          "id": "d18ef073971c4a72f49515f970489327eb2d6464",
          "message": "Merge pull request #503 from LoveDaisy/fix/cli-verbose-help-level\n\nfix(cli): describe -v as verbose level in help text, pin -v/-d level mapping",
          "timestamp": "2026-10-17T16:55:44+08:00",
          "tree_id": "721ce317bc2328c82f6a85758e3c0e11ecfee029",
          "url": "https://github.com/LoveDaisy/Lumice/commit/d18ef073971c4a72f49515f970489327eb2d6464"
        },
        "date": 1792227537318,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 334081.3,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 505967,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 366432.7,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 308001.2,
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
          "id": "35bde7ee5db9a00e9d59df65f6488a4e873fd9df",
          "message": "Merge pull request #504 from LoveDaisy/fix/lens-border-print-colour-gate\n\nfix(gui): gate lens border colour under print mode (join NotUnderPrintMode family)",
          "timestamp": "2026-10-17T17:14:39+08:00",
          "tree_id": "f863f15f651274c6567920c6d747669634cf2c78",
          "url": "https://github.com/LoveDaisy/Lumice/commit/35bde7ee5db9a00e9d59df65f6488a4e873fd9df"
        },
        "date": 1792228672142,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 247281.8,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 504809,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 365558.2,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 368547.3,
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
          "id": "43a72d655ad2a9f3ab50650b80bc0bafeddeb49a",
          "message": "Merge pull request #505 from LoveDaisy/chore/source-comment-consistency\n\nchore(source-comment-consistency): rewrite five drift groups of src/ comments",
          "timestamp": "2026-10-17T17:35:59+08:00",
          "tree_id": "d4e5b5d64f6ec519bee36522169fafe1ef1e335a",
          "url": "https://github.com/LoveDaisy/Lumice/commit/43a72d655ad2a9f3ab50650b80bc0bafeddeb49a"
        },
        "date": 1792230020749,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 306154.3,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 506185.6,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 484721.9,
            "unit": "rays/sec",
            "extra": "CPU: Intel(R) Xeon(R) 6973P-C\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 310487.5,
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
          "id": "9f6df4d7e70c2cd552b73298382577b9399e17d1",
          "message": "Merge pull request #506 from LoveDaisy/fix/verbose-witness-dead-site\n\nfeat(cli): make -v/-d observable — drop dead VERBOSE site, add parse-time level lines",
          "timestamp": "2026-10-17T18:54:32+08:00",
          "tree_id": "4ad751e4775236fa65d6490bf1d0dd4ec37524c1",
          "url": "https://github.com/LoveDaisy/Lumice/commit/9f6df4d7e70c2cd552b73298382577b9399e17d1"
        },
        "date": 1792234605854,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 459104.2,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 504915,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 486182.9,
            "unit": "rays/sec",
            "extra": "CPU: Intel(R) Xeon(R) 6973P-C\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 487028.7,
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
          "id": "e74a6fad65c33336e274a3929c5f9121f4339a44",
          "message": "Merge pull request #507 from LoveDaisy/chore/e2e-step-budget-runner-variance\n\ntest/e2e cost trims + shadow bench probe (697 PR1: restore work headroom, instrument the budget)",
          "timestamp": "2026-10-17T23:21:07+08:00",
          "tree_id": "b3064b0619f8ecd539d6992a56acf002c7bac678",
          "url": "https://github.com/LoveDaisy/Lumice/commit/e74a6fad65c33336e274a3929c5f9121f4339a44"
        },
        "date": 1792250648347,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 423306.2,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 504163.3,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 428393.7,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 9V74 80-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 314427.6,
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
          "id": "b5112a34db028a9f3ad0ae392c2a1769e42ea6f7",
          "message": "Merge pull request #509 from LoveDaisy/feat/api-version-narrow-header\n\nrefactor(capi): move LUMICE_API_VERSION to a leaf header",
          "timestamp": "2026-10-18T10:16:11+08:00",
          "tree_id": "9bdfa4c447ed393ed682a9709fdfa31a03e7e980",
          "url": "https://github.com/LoveDaisy/Lumice/commit/b5112a34db028a9f3ad0ae392c2a1769e42ea6f7"
        },
        "date": 1792289976365,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 383991.5,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 506109.4,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 367738.1,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 314462.5,
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
          "id": "b5112a34db028a9f3ad0ae392c2a1769e42ea6f7",
          "message": "Merge pull request #509 from LoveDaisy/feat/api-version-narrow-header\n\nrefactor(capi): move LUMICE_API_VERSION to a leaf header",
          "timestamp": "2026-10-18T10:16:11+08:00",
          "tree_id": "9bdfa4c447ed393ed682a9709fdfa31a03e7e980",
          "url": "https://github.com/LoveDaisy/Lumice/commit/b5112a34db028a9f3ad0ae392c2a1769e42ea6f7"
        },
        "date": 1792291248521,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 328555.9,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 504116.3,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 429181.5,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 9V74 80-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 312442.5,
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
          "id": "01ed3796d7ecf82462ec49259ea9a23a1b3a2820",
          "message": "Merge pull request #508 from LoveDaisy/feat/raypath-survey-c14-smoke\n\nfeat(raypath): survey CLI skeleton + C14 smoke slice (schema4)",
          "timestamp": "2026-10-18T11:57:21+08:00",
          "tree_id": "19d2870c77a0856c4cf48a574aea58c803bf7f4b",
          "url": "https://github.com/LoveDaisy/Lumice/commit/01ed3796d7ecf82462ec49259ea9a23a1b3a2820"
        },
        "date": 1792296080715,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 339044.1,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 504200.1,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 569714.7,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 9V45 96-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 307613.7,
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
          "id": "1ef8e6d0040a4eaf101f1b281d41bcf7cfc036a4",
          "message": "Merge pull request #510 from LoveDaisy/chore/restore-bisection-registration\n\nchore(test): restore the endpoint-bisection registry entry dropped in #507",
          "timestamp": "2026-10-18T13:00:24+08:00",
          "tree_id": "124086edda2c319809900322ef073b4ba1c6808c",
          "url": "https://github.com/LoveDaisy/Lumice/commit/1ef8e6d0040a4eaf101f1b281d41bcf7cfc036a4"
        },
        "date": 1792299785007,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 405155.5,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 503743.3,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 363476.5,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 469705.6,
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
          "id": "872baa366d78c2065a37645eb3bd9ac33ef78cdb",
          "message": "Merge pull request #511 from LoveDaisy/feat/raypath-survey-c01-delta\n\nfeat(raypath): survey C01 full-dimensional delta machine, radial probe and chromatic layer v1",
          "timestamp": "2026-10-18T16:00:38+08:00",
          "tree_id": "d06f27173c45451f8acbcd7f8ee8d892a299bbea",
          "url": "https://github.com/LoveDaisy/Lumice/commit/872baa366d78c2065a37645eb3bd9ac33ef78cdb"
        },
        "date": 1792310634600,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 409899,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 507187.7,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 361902.1,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 284231.1,
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
          "id": "be2288de2d204daa735f6abd653f3303a0dda03e",
          "message": "Merge pull request #512 from LoveDaisy/feat/raypath-survey-c15-tint\n\nfeat(raypath): survey C15 slice — weight_tint chromatic leg (smoke trilogy finale)",
          "timestamp": "2026-10-18T20:59:11+08:00",
          "tree_id": "15694e43ea5a96e16d4ddd53b45ecd9d8fdb14d6",
          "url": "https://github.com/LoveDaisy/Lumice/commit/be2288de2d204daa735f6abd653f3303a0dda03e"
        },
        "date": 1792328616686,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 325969.1,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 505239.8,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 574887.4,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 9V45 96-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 362217.4,
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
          "id": "6edfd93e4b15b31fb7103c58cbe8e08d64ea0770",
          "message": "Merge pull request #513 from LoveDaisy/feat/axis-preset-memory\n\nGUI: axis chips switch label and parameters, Reset row replaces apply combo",
          "timestamp": "2026-10-19T08:29:50+08:00",
          "tree_id": "3b226eb5638e474f8fba92b298af79680e608bb3",
          "url": "https://github.com/LoveDaisy/Lumice/commit/6edfd93e4b15b31fb7103c58cbe8e08d64ea0770"
        },
        "date": 1792369999390,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 344875.7,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 506321.4,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 368942.6,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 289380.1,
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
          "id": "a0bce739786ae19df76253a07e518ea470b5f422",
          "message": "Merge pull request #514 from LoveDaisy/chore/ci-build-budget-per-miss\n\nci: per-miss build budget — adaptive limit + absolute backstop (registry v3)",
          "timestamp": "2026-10-19T09:55:27+08:00",
          "tree_id": "44c0290ab73f18304b5552f748f3c0a49390d2dd",
          "url": "https://github.com/LoveDaisy/Lumice/commit/a0bce739786ae19df76253a07e518ea470b5f422"
        },
        "date": 1792375116353,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 327381.9,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 503958,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 426425.7,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 9V74 80-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 303547.5,
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
          "id": "31fd56b1341b4ec0624cb56ae875d26f0d037089",
          "message": "docs: slim root AGENTS.md to constitution + route map (983→396 lines) (#515)\n\n* docs(test): sink test-tree operating rules into test/AGENTS.md\n\nStep 2 (M1) of the AGENTS.md slimming: the Testing section's operational\ndetail (three-platform matrix, placement gates, isolation discipline,\nshared-library plumbing, E2E layout, the full reference-regeneration\nworkflow) moves to test/AGENTS.md; src/raypath/AGENTS.md states the module\nsurface. Root keeps the test-scope table, judgment discipline, gate\nsummaries and the duration-registry paragraph with the cold-build contract\nstrings verbatim. Pointers that named AGENTS.md sections now name the new\nhomes (checkers, CONTRIBUTING, e2e README, testing-architecture en/zh).\n\nCo-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>\n\n* docs(index): recycle AGENTS.md doc-index summaries into the docs themselves\n\nStep 3 (M2): the Documentation Index's 38 entries collapse to one/two-line\ntrigger hooks — the docs are the sole content authority. Claim-diff against\nthe doc bodies found only three real gaps, now ported: the v4.7.0\nfilter-symmetry baseline anchor (raypath-symmetry en+zh), the CUDA TU\ncompile guard script and its gtest-zero-match false-green rationale\n(gpu-remote-cuda-build-testing), and the GPU implementation-discipline\nblock with the legacy PropagateSlab reference (seam-design header; the\n#295 incident narrative stays in gpu-route-history). The index also drops\nstale claims the docs had already superseded (overlay blueprint→as-built,\nparity 10/10→30/30).\n\nCo-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>\n\n* docs(root): compress AGENTS.md to the constitution + route-map shape\n\nStep 4 (M3): root drops from 983 to 395 lines. Code Structure one-liners\nper directory (raypath/test rules point at the sub-AGENTS files); Style\nkeeps every rule statement and checker-referenced sentence, compressing\nonly narrative blocks (launcher history, env incident, naming sub-bullets);\nthe KB section keeps the retrieval discipline and the completeness-claim\nrule with the PR #261 evidence, compressing the story; the Testing\nsection's scope table and judgment discipline stay verbatim. Line count is\nthe target, not the judge: no checker-referenced or route-bearing sentence\nwas removed to hit it.\n\nCo-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>\n\n* docs(test): reconcile duration-registry summaries with registry v3 and fix the PR-261 pointer\n\nRebase onto main (a0bce739, PR #514) moved the duration registry to v3: the\nroot AGENTS.md summary still said \"v2 / two quantities\", which no checker\nwould have caught after merge. Root side now shrinks to the pytest-pinned\nstrings plus pointers (operating rules single-sourced in test/AGENTS.md,\nrewritten for v3 semantics: per-miss adaptive ruler + absolute backstop).\nThe KB completeness-rule paragraph no longer claims \"PR #261 has the full\nnarrative\" (falsified: the PR description does not carry it) and inlines the\nfour supporting measurements instead. The windows physical-desktop line moves\nout of root (test/AGENTS.md already holds it).\n\nCo-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>\n\n---------\n\nCo-authored-by: Claude Sonnet 5 <noreply@anthropic.com>",
          "timestamp": "2026-10-19T11:25:46+08:00",
          "tree_id": "8c68466a2ee7859b6f3cbfb33be0938673d9e9c4",
          "url": "https://github.com/LoveDaisy/Lumice/commit/31fd56b1341b4ec0624cb56ae875d26f0d037089"
        },
        "date": 1792380547829,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 444671.8,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 504696.7,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 368305,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 309734.3,
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
          "id": "5d28c6f252fce9b6b55e845b47eaf5cc77815f6e",
          "message": "Merge pull request #517 from LoveDaisy/chore/li-parity-fixture-lfs\n\nchore(test): track li-parity fixtures in Git LFS",
          "timestamp": "2026-10-19T16:00:23+08:00",
          "tree_id": "457c05fb6b6b02b293a736c7b48a89a1eb9f6353",
          "url": "https://github.com/LoveDaisy/Lumice/commit/5d28c6f252fce9b6b55e845b47eaf5cc77815f6e"
        },
        "date": 1792397019086,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 389862.5,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 504134.9,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 369325.6,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 308364.6,
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
          "id": "0c999555a069ca1ba6716ec874872bf1cfd785ac",
          "message": "Merge pull request #518 from LoveDaisy/chore/per-miss-all-legs\n\nci: per-miss budget on all four legs + envelope-derived outer bounds",
          "timestamp": "2026-10-19T17:39:48+08:00",
          "tree_id": "b435ae70681f441518035f9c2c7980f4c4eeef3b",
          "url": "https://github.com/LoveDaisy/Lumice/commit/0c999555a069ca1ba6716ec874872bf1cfd785ac"
        },
        "date": 1792403145482,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 322316.9,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 506602,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 367573.2,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 307581,
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
          "id": "3d1db8784fde656bdfd16735733b0763fef6523e",
          "message": "Merge pull request #516 from LoveDaisy/refactor/math-constants-consolidation\n\nrefactor: single authority for the pi/vec3 families (census ledger item 2)",
          "timestamp": "2026-10-19T18:09:03+08:00",
          "tree_id": "a99a1ce01dce24594bbd4c6e47ccb2033cfae345",
          "url": "https://github.com/LoveDaisy/Lumice/commit/3d1db8784fde656bdfd16735733b0763fef6523e"
        },
        "date": 1792404743695,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 321086.8,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 506928.9,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 397579.5,
            "unit": "rays/sec",
            "extra": "CPU: INTEL(R) XEON(R) PLATINUM 8573C\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 316184.6,
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
          "id": "4f5f873d869a4e582eaad4d8fef0dcfcf48e1efe",
          "message": "Merge pull request #519 from LoveDaisy/chore/test-tu-cost-gate\n\nfeat(ci): test-TU cost declaration gate — fifth diff-scoped checker",
          "timestamp": "2026-10-19T18:39:25+08:00",
          "tree_id": "8ae83eaccc75164a1d8908cf61bbcecc506e8da4",
          "url": "https://github.com/LoveDaisy/Lumice/commit/4f5f873d869a4e582eaad4d8fef0dcfcf48e1efe"
        },
        "date": 1792406527680,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 412639.3,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 504637.6,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 366410.1,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 487408.7,
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
          "id": "943184db19db97281bc7d892e26c1e0a4a459c76",
          "message": "Merge pull request #520 from LoveDaisy/chore/release-4.7.4\n\nchore(release): cut 4.7.4",
          "timestamp": "2026-10-19T19:23:08+08:00",
          "tree_id": "267e85e972a21e8c9dd6b409a36f43e5ef8c9b61",
          "url": "https://github.com/LoveDaisy/Lumice/commit/943184db19db97281bc7d892e26c1e0a4a459c76"
        },
        "date": 1792409182475,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 331809.6,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 505462.2,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 425496.5,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 9V74 80-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 313422.8,
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
          "id": "6a0a317eb4b27627a837076e4d21dc1dc17e3ab0",
          "message": "Merge pull request #521 from LoveDaisy/feat/axis-chip-pose-and-reset-row\n\nfeat(gui): chip switch re-seats preview pose; Reset All row (task-712)",
          "timestamp": "2026-10-20T01:55:48+08:00",
          "tree_id": "2b54e7d0492fe7227f1448ac86528fb92d531110",
          "url": "https://github.com/LoveDaisy/Lumice/commit/6a0a317eb4b27627a837076e4d21dc1dc17e3ab0"
        },
        "date": 1792432770702,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 316089.8,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 504922.3,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 403231.5,
            "unit": "rays/sec",
            "extra": "CPU: INTEL(R) XEON(R) PLATINUM 8573C\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 313449.6,
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
          "id": "4dfe2ac445b752957a1a13cbb722bb0f12a53d12",
          "message": "Merge pull request #522 from LoveDaisy/feat/gui-visual-declutter\n\nfeat(gui): right-panel visual declutter — Continue dim, Resolution de-rust, aspect combo merge (task-713)",
          "timestamp": "2026-10-20T02:27:18+08:00",
          "tree_id": "ff8a85581ce9896080bb1822c5962ddbef7910ee",
          "url": "https://github.com/LoveDaisy/Lumice/commit/4dfe2ac445b752957a1a13cbb722bb0f12a53d12"
        },
        "date": 1792434783271,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 287075.5,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 505962,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 367577.6,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 313512.3,
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
          "id": "6997a5a16a019e98c1819cb1b23b8a6c42f5fc33",
          "message": "Merge pull request #523 from LoveDaisy/feat/raypath-survey-c17-c06-c09\n\nfeat(survey): C17/C06/C09 wiring — witness, dual-mechanism chroma, support set (task-714)",
          "timestamp": "2026-10-20T06:23:22+08:00",
          "tree_id": "aa3e3ba9cc15c5e4714f8d861ef924a19ad59994",
          "url": "https://github.com/LoveDaisy/Lumice/commit/6997a5a16a019e98c1819cb1b23b8a6c42f5fc33"
        },
        "date": 1792448815936,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 376480.5,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 506171.3,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 365858.2,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 312854.6,
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
          "id": "d36f13688d6b3d03758b35e63d5a5936f8558fac",
          "message": "Merge pull request #524 from LoveDaisy/chore/fullsky-bound-comment\n\nchore(test): rewrite the full-sky identity pin's tolerance comment to the real mechanism",
          "timestamp": "2026-10-20T10:17:44+08:00",
          "tree_id": "2a6d01b61c4d30d7ae2cb4e9ef9897254515605b",
          "url": "https://github.com/LoveDaisy/Lumice/commit/d36f13688d6b3d03758b35e63d5a5936f8558fac"
        },
        "date": 1792462880272,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 330146.5,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 504750.3,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 368250.5,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 312703.5,
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
          "id": "471ce7726c6d62522bd87ff000726cbcd454d649",
          "message": "Merge pull request #525 from LoveDaisy/chore/comdat-flake-doc-note\n\ndocs(testing): record the COMDAT x -march local-red/CI-green flake mechanism (§4.6.1)",
          "timestamp": "2026-10-20T10:42:04+08:00",
          "tree_id": "3331d03171eadc032df781091ed4a7668619d608",
          "url": "https://github.com/LoveDaisy/Lumice/commit/471ce7726c6d62522bd87ff000726cbcd454d649"
        },
        "date": 1792464315258,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 401990.2,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 506166.4,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 367304.9,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 328845.3,
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
          "id": "c10466366a3f9d19d42a890a092389cebaade1b0",
          "message": "Merge pull request #526 from LoveDaisy/feat/cl-exe-shared-ipo\n\nfeat(cmake): enable IPO for cl.exe shared Release trees",
          "timestamp": "2026-10-20T12:57:03+08:00",
          "tree_id": "38f1949b834ef1d5cd955ecb0db8f7a0c5e04fb2",
          "url": "https://github.com/LoveDaisy/Lumice/commit/c10466366a3f9d19d42a890a092389cebaade1b0"
        },
        "date": 1792472442206,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 261656.2,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 504121.8,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 554088.7,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 9V45 96-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 310646.5,
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
          "id": "0133b0e9825ce175d55d5cfcb43ed65fd904b519",
          "message": "Merge pull request #527 from LoveDaisy/feat/core-json-grid-import-notice\n\nfeat(gui): notice render[].grid refusal on core JSON import",
          "timestamp": "2026-10-20T15:49:47+08:00",
          "tree_id": "d94d4836689f7f76be563603c87a73e502fbd55c",
          "url": "https://github.com/LoveDaisy/Lumice/commit/0133b0e9825ce175d55d5cfcb43ed65fd904b519"
        },
        "date": 1792482782718,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 337106.2,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 506799.5,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 571275.1,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 9V45 96-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 313700.9,
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
          "id": "cb6c6b6a30094b1868d511ae523a9a244055462f",
          "message": "Merge pull request #528 from LoveDaisy/chore/rng-cross-stdlib-contract-doc\n\ndocs: state the cross-stdlib RNG contract instead of implying bit-exact replay",
          "timestamp": "2026-10-20T16:27:35+08:00",
          "tree_id": "6402f39a5ce3737c1d821ff1782754258edaf12f",
          "url": "https://github.com/LoveDaisy/Lumice/commit/cb6c6b6a30094b1868d511ae523a9a244055462f"
        },
        "date": 1792485055541,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 305764.2,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 506504.3,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 552064.8,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 9V45 96-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 367178.2,
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
          "id": "d1b929b7a581fc70342002de9711cfb3760b2e62",
          "message": "Merge pull request #529 from LoveDaisy/feat/lens-proj-recalibration\n\ntest(gui): recalibrate the lens_proj reference group (drift +1.2~+2.5 dB)",
          "timestamp": "2026-10-20T20:41:06+08:00",
          "tree_id": "8ec07448bbb5d138099669297f4ba99267172564",
          "url": "https://github.com/LoveDaisy/Lumice/commit/d1b929b7a581fc70342002de9711cfb3760b2e62"
        },
        "date": 1792500262599,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 400231.9,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 506084.1,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 414303.8,
            "unit": "rays/sec",
            "extra": "CPU: Intel(R) Xeon(R) Platinum 8370C CPU @ 2.80GHz\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 313280.6,
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
          "id": "a4d56f0943b8fe3e078f72c8b136692f9692b114",
          "message": "Merge pull request #530 from LoveDaisy/feat/gap1-family-routing\n\nfeat(raypath): gap-1 measure-to-family routing (first wave 9/9)",
          "timestamp": "2026-10-20T22:27:14+08:00",
          "tree_id": "77c386a99074b5e97dfb8278a6d65fcc30ed649b",
          "url": "https://github.com/LoveDaisy/Lumice/commit/a4d56f0943b8fe3e078f72c8b136692f9692b114"
        },
        "date": 1792506638569,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 339708.2,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 506078.5,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 423907.6,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 9V74 80-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 313831.8,
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
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "edebcf3766408a5da0e9750157314d7325aaa469",
          "message": "Merge pull request #483 from LoveDaisy/feat/look-at-target-lock\n\nfeat(gui): add Look At target locking with manual takeover",
          "timestamp": "2026-10-11T21:06:44+08:00",
          "tree_id": "fb7774db9dcb6b30ed6ab81c83175faa1687100b",
          "url": "https://github.com/LoveDaisy/Lumice/commit/edebcf3766408a5da0e9750157314d7325aaa469"
        },
        "date": 1791724200549,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 1148299.9,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1007239.1,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 718976.1,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 583215.3,
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
          "id": "edebcf3766408a5da0e9750157314d7325aaa469",
          "message": "Merge pull request #483 from LoveDaisy/feat/look-at-target-lock\n\nfeat(gui): add Look At target locking with manual takeover",
          "timestamp": "2026-10-11T21:06:44+08:00",
          "tree_id": "fb7774db9dcb6b30ed6ab81c83175faa1687100b",
          "url": "https://github.com/LoveDaisy/Lumice/commit/edebcf3766408a5da0e9750157314d7325aaa469"
        },
        "date": 1791725230828,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 823911,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1012781.6,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 719567.9,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 636047,
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
          "id": "87a9d3bd2b85c0f0373e5ecf32c77bd163a39cb6",
          "message": "Merge pull request #484 from LoveDaisy/chore/release-4.7.3\n\nchore(release): cut 4.7.3",
          "timestamp": "2026-10-12T13:03:05+08:00",
          "tree_id": "dc8ccddd040a6828e7b0f0bd8e0d347da506521e",
          "url": "https://github.com/LoveDaisy/Lumice/commit/87a9d3bd2b85c0f0373e5ecf32c77bd163a39cb6"
        },
        "date": 1791781592901,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 850667.9,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1010594.6,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 724216,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 700011.2,
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
          "id": "46671819d8f4c160a624710d644cfca2c5892361",
          "message": "Merge pull request #485 from LoveDaisy/chore/migrate-repo-name-lumice\n\ndocs: migrate repo name ice_halo_sim to Lumice",
          "timestamp": "2026-10-12T13:34:20+08:00",
          "tree_id": "aec7c127f5820de64118e9f6a204ec6cb003c8ff",
          "url": "https://github.com/LoveDaisy/Lumice/commit/46671819d8f4c160a624710d644cfca2c5892361"
        },
        "date": 1791783425563,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 1057844.5,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1009943.6,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 1074214.9,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 9V45 96-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 708785.8,
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
          "id": "e1ac9076ac5ba61fb78e51027bd35f89f6be05d7",
          "message": "Merge pull request #486 from LoveDaisy/fix/sun-diameter-zero-min\n\nfix(gui): allow sun diameter 0 (point source) in the GUI domain",
          "timestamp": "2026-10-12T20:22:38+08:00",
          "tree_id": "306c8309b25fc330b0d3e620e8b4ba4b488264b3",
          "url": "https://github.com/LoveDaisy/Lumice/commit/e1ac9076ac5ba61fb78e51027bd35f89f6be05d7"
        },
        "date": 1791807952053,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 908827.8,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1011445.2,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 1134042.4,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 9V45 96-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 591142.8,
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
          "id": "cdb4c383c1637dc4de908ea64859de306a7ec22c",
          "message": "Merge pull request #487 from LoveDaisy/fix/look-at-row-alignment\n\nfix(gui): align the View Look At row to the panel's control grid",
          "timestamp": "2026-10-12T22:17:52+08:00",
          "tree_id": "684e0d405dc952e70143ce6cf8d78cd922de7278",
          "url": "https://github.com/LoveDaisy/Lumice/commit/cdb4c383c1637dc4de908ea64859de306a7ec22c"
        },
        "date": 1791815059841,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 1018257.4,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1007889.7,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 924417.8,
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
          "id": "962a92222a309a30e539b51a23aaafe5da021605",
          "message": "Merge pull request #488 from LoveDaisy/fix/ci-seed-audit-false-positive\n\nfix(ci): fix ccache seed audit false positive (one write per miss)",
          "timestamp": "2026-10-13T00:11:07+08:00",
          "tree_id": "b8ad04d108363e75b26db167440a99bf58627e1e",
          "url": "https://github.com/LoveDaisy/Lumice/commit/962a92222a309a30e539b51a23aaafe5da021605"
        },
        "date": 1791821672177,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 919396.5,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1010566.2,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 723457.4,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 583862.7,
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
          "id": "b45848cea4b9c09ffeec37e4f3f442ae13cd62e6",
          "message": "Merge pull request #489 from LoveDaisy/fix/look-at-row-grid-polish\n\nFix: revert right panel to 300px and end the Look At row on the Lock square",
          "timestamp": "2026-10-13T07:07:41+08:00",
          "tree_id": "5b2702f367d17c7dd47f5ddeb66a544d1cd3c8e1",
          "url": "https://github.com/LoveDaisy/Lumice/commit/b45848cea4b9c09ffeec37e4f3f442ae13cd62e6"
        },
        "date": 1791846641598,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 827602.5,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1012350.1,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 711776.5,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 949127.5,
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
          "id": "5b267889368334c2aed943d7bae310becb382cba",
          "message": "Merge pull request #491 from LoveDaisy/chore/release-4.7.3-recut\n\nchore(release): recut 4.7.3",
          "timestamp": "2026-10-13T07:28:24+08:00",
          "tree_id": "b91244d1d5185b200796d088072f6fd80ca186d1",
          "url": "https://github.com/LoveDaisy/Lumice/commit/5b267889368334c2aed943d7bae310becb382cba"
        },
        "date": 1791847903361,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 1070260,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1011844.2,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 720454.1,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 575994.7,
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
          "id": "7ce6259807c4d988af8af1a1bee70b276ca2173e",
          "message": "Merge pull request #492 from LoveDaisy/chore/raypath-docs-current-state\n\ndocs(raypath): align analysis docs with the current product state",
          "timestamp": "2026-10-14T18:27:14+08:00",
          "tree_id": "b5ab7d4c031478152f3808435eb5a75fa97f6fbf",
          "url": "https://github.com/LoveDaisy/Lumice/commit/7ce6259807c4d988af8af1a1bee70b276ca2173e"
        },
        "date": 1791973845060,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 877919.2,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1007813.2,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 725294.7,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 583812,
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
          "id": "d732ea5937ccbe41714459898520e6607d066b12",
          "message": "Merge pull request #493 from LoveDaisy/fix/gui-test-window-size-leak\n\ntest(gui): restore shared window basis at case entry with a leak sentinel",
          "timestamp": "2026-10-14T20:16:19+08:00",
          "tree_id": "18a408014d7f5b0bdfc2b2667c2dd6fa2d21835e",
          "url": "https://github.com/LoveDaisy/Lumice/commit/d732ea5937ccbe41714459898520e6607d066b12"
        },
        "date": 1791980364261,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 1004421.5,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1009465.9,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 724818.9,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 700811.1,
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
          "id": "92c54af058536b731f3f2a7bc50a444704146b7f",
          "message": "Merge pull request #494 from LoveDaisy/feat/analysis-crystal-id-mismatch\n\nfix(gui): align analysis list crystal numbers with card IDs",
          "timestamp": "2026-10-15T01:51:21+08:00",
          "tree_id": "d43c91f95f5daa0bd5c9b3c56b99635d451c6af2",
          "url": "https://github.com/LoveDaisy/Lumice/commit/92c54af058536b731f3f2a7bc50a444704146b7f"
        },
        "date": 1792000474666,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 898248.8,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1006245.2,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 845490,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 9V74 80-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 544494.6,
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
          "id": "e24525033315c4fa88cce6199342f7f62cee94d3",
          "message": "Merge pull request #495 from LoveDaisy/feat/axis-sticky-label\n\nfeat(gui): sticky user-chosen axis label, decoupled from preset apply",
          "timestamp": "2026-10-16T09:27:12+08:00",
          "tree_id": "839d54c4e434bf7ae6b44932a72e5fcd24c3bdac",
          "url": "https://github.com/LoveDaisy/Lumice/commit/e24525033315c4fa88cce6199342f7f62cee94d3"
        },
        "date": 1792114223216,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 919481.8,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1014445.6,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 845728.4,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 9V74 80-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 697867.9,
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
          "id": "d3b5b77505182ea5e0fff0873d106cc506c38f0c",
          "message": "Merge pull request #496 from LoveDaisy/feat/axis-apply-decouple\n\nfeat(gui): Apply preset is a pure parameter shortcut, label and view untouched",
          "timestamp": "2026-10-16T21:21:19+08:00",
          "tree_id": "4d4ba181a95d82bf92b42afdd4e29619e4d7710c",
          "url": "https://github.com/LoveDaisy/Lumice/commit/d3b5b77505182ea5e0fff0873d106cc506c38f0c"
        },
        "date": 1792157053880,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 943194.9,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1010883.7,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 843954.5,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 9V74 80-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 926469.5,
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
          "id": "c3372ae22b354c4456f0c52e19980930451b511a",
          "message": "Merge pull request #497 from LoveDaisy/feat/entry-edit-weight-toggle\n\nfeat(gui): entry bar in Edit Entry — weight and participation toggle in the modal",
          "timestamp": "2026-10-16T22:18:44+08:00",
          "tree_id": "1b15038814229189d3747ad68a70aad46c55ed88",
          "url": "https://github.com/LoveDaisy/Lumice/commit/c3372ae22b354c4456f0c52e19980930451b511a"
        },
        "date": 1792160496482,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 889188.7,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1012235,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 849766.4,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 9V74 80-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 913149.9,
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
          "id": "661f6814adafc3ae410096f9f67ed0cbcfe37403",
          "message": "Merge pull request #498 from LoveDaisy/chore/doc-topic-organization\n\ndocs: organize documentation into topic directories",
          "timestamp": "2026-10-17T01:50:13+08:00",
          "tree_id": "c1cf0a07d8d95b945e5279c062b26efaf6c926b0",
          "url": "https://github.com/LoveDaisy/Lumice/commit/661f6814adafc3ae410096f9f67ed0cbcfe37403"
        },
        "date": 1792173204194,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 1218681.7,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1011099.2,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 725076.5,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 582576.9,
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
          "id": "a8a3d13f7e30f924bc710a8e6693a2e9a3cfc20c",
          "message": "Merge pull request #500 from LoveDaisy/docs/doc-code-consistency\n\ndocs: reconcile guides and architecture with current implementation",
          "timestamp": "2026-10-17T12:16:41+08:00",
          "tree_id": "0c9364b710adfa1f3ffbca4ad5afb4d536b131ab",
          "url": "https://github.com/LoveDaisy/Lumice/commit/a8a3d13f7e30f924bc710a8e6693a2e9a3cfc20c"
        },
        "date": 1792210805861,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 896302.9,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1009085.2,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 773841.8,
            "unit": "rays/sec",
            "extra": "CPU: INTEL(R) XEON(R) PLATINUM 8573C\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 572629,
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
          "id": "7578ffac709fd28e350dc9de11f7cd14c68c6bd0",
          "message": "Merge pull request #501 from LoveDaisy/fix/xyz-stats-current-capi\n\nFix dump_xyz_stats.py: consume current C API contract via CLI npy export",
          "timestamp": "2026-10-17T15:52:48+08:00",
          "tree_id": "b76c99c2e7759bc60276b944ee897ddb60ef96a4",
          "url": "https://github.com/LoveDaisy/Lumice/commit/7578ffac709fd28e350dc9de11f7cd14c68c6bd0"
        },
        "date": 1792223721829,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 892240.8,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1010413.8,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 945153.9,
            "unit": "rays/sec",
            "extra": "CPU: Intel(R) Xeon(R) 6973P-C\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 901635.7,
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
          "id": "2d8b602f597c2077eef981ad5ed97922620caecc",
          "message": "Merge pull request #502 from LoveDaisy/fix/cli-verbose-help-level\n\nfix(cli): describe -v as verbose level in help text",
          "timestamp": "2026-10-17T16:38:00+08:00",
          "tree_id": "ee41ffe66d42cacfc9fedf0ceb7e26dc02a0ea97",
          "url": "https://github.com/LoveDaisy/Lumice/commit/2d8b602f597c2077eef981ad5ed97922620caecc"
        },
        "date": 1792226464273,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 1012646.6,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1010673.4,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 720907.1,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 918789.4,
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
          "id": "d18ef073971c4a72f49515f970489327eb2d6464",
          "message": "Merge pull request #503 from LoveDaisy/fix/cli-verbose-help-level\n\nfix(cli): describe -v as verbose level in help text, pin -v/-d level mapping",
          "timestamp": "2026-10-17T16:55:44+08:00",
          "tree_id": "721ce317bc2328c82f6a85758e3c0e11ecfee029",
          "url": "https://github.com/LoveDaisy/Lumice/commit/d18ef073971c4a72f49515f970489327eb2d6464"
        },
        "date": 1792227541958,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 888744.5,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1009803.5,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 725598.9,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 582301.4,
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
          "id": "35bde7ee5db9a00e9d59df65f6488a4e873fd9df",
          "message": "Merge pull request #504 from LoveDaisy/fix/lens-border-print-colour-gate\n\nfix(gui): gate lens border colour under print mode (join NotUnderPrintMode family)",
          "timestamp": "2026-10-17T17:14:39+08:00",
          "tree_id": "f863f15f651274c6567920c6d747669634cf2c78",
          "url": "https://github.com/LoveDaisy/Lumice/commit/35bde7ee5db9a00e9d59df65f6488a4e873fd9df"
        },
        "date": 1792228677230,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 665854,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1010891.7,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 725325,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 692529.7,
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
          "id": "43a72d655ad2a9f3ab50650b80bc0bafeddeb49a",
          "message": "Merge pull request #505 from LoveDaisy/chore/source-comment-consistency\n\nchore(source-comment-consistency): rewrite five drift groups of src/ comments",
          "timestamp": "2026-10-17T17:35:59+08:00",
          "tree_id": "d4e5b5d64f6ec519bee36522169fafe1ef1e335a",
          "url": "https://github.com/LoveDaisy/Lumice/commit/43a72d655ad2a9f3ab50650b80bc0bafeddeb49a"
        },
        "date": 1792230025439,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 931053.6,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1013857.5,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 1001635.2,
            "unit": "rays/sec",
            "extra": "CPU: Intel(R) Xeon(R) 6973P-C\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 585891.9,
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
          "id": "9f6df4d7e70c2cd552b73298382577b9399e17d1",
          "message": "Merge pull request #506 from LoveDaisy/fix/verbose-witness-dead-site\n\nfeat(cli): make -v/-d observable — drop dead VERBOSE site, add parse-time level lines",
          "timestamp": "2026-10-17T18:54:32+08:00",
          "tree_id": "4ad751e4775236fa65d6490bf1d0dd4ec37524c1",
          "url": "https://github.com/LoveDaisy/Lumice/commit/9f6df4d7e70c2cd552b73298382577b9399e17d1"
        },
        "date": 1792234611264,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 1184534.2,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1009784.3,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 957147,
            "unit": "rays/sec",
            "extra": "CPU: Intel(R) Xeon(R) 6973P-C\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 921974.8,
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
          "id": "e74a6fad65c33336e274a3929c5f9121f4339a44",
          "message": "Merge pull request #507 from LoveDaisy/chore/e2e-step-budget-runner-variance\n\ntest/e2e cost trims + shadow bench probe (697 PR1: restore work headroom, instrument the budget)",
          "timestamp": "2026-10-17T23:21:07+08:00",
          "tree_id": "b3064b0619f8ecd539d6992a56acf002c7bac678",
          "url": "https://github.com/LoveDaisy/Lumice/commit/e74a6fad65c33336e274a3929c5f9121f4339a44"
        },
        "date": 1792250652046,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 1229014.2,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1011444.4,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 850256.2,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 9V74 80-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 587972.2,
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
          "id": "b5112a34db028a9f3ad0ae392c2a1769e42ea6f7",
          "message": "Merge pull request #509 from LoveDaisy/feat/api-version-narrow-header\n\nrefactor(capi): move LUMICE_API_VERSION to a leaf header",
          "timestamp": "2026-10-18T10:16:11+08:00",
          "tree_id": "9bdfa4c447ed393ed682a9709fdfa31a03e7e980",
          "url": "https://github.com/LoveDaisy/Lumice/commit/b5112a34db028a9f3ad0ae392c2a1769e42ea6f7"
        },
        "date": 1792289980605,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 989381.7,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1011702.8,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 728002.7,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 588717.2,
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
          "id": "b5112a34db028a9f3ad0ae392c2a1769e42ea6f7",
          "message": "Merge pull request #509 from LoveDaisy/feat/api-version-narrow-header\n\nrefactor(capi): move LUMICE_API_VERSION to a leaf header",
          "timestamp": "2026-10-18T10:16:11+08:00",
          "tree_id": "9bdfa4c447ed393ed682a9709fdfa31a03e7e980",
          "url": "https://github.com/LoveDaisy/Lumice/commit/b5112a34db028a9f3ad0ae392c2a1769e42ea6f7"
        },
        "date": 1792291252460,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 906704.2,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1009773,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 851121.5,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 9V74 80-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 588224.4,
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
          "id": "01ed3796d7ecf82462ec49259ea9a23a1b3a2820",
          "message": "Merge pull request #508 from LoveDaisy/feat/raypath-survey-c14-smoke\n\nfeat(raypath): survey CLI skeleton + C14 smoke slice (schema4)",
          "timestamp": "2026-10-18T11:57:21+08:00",
          "tree_id": "19d2870c77a0856c4cf48a574aea58c803bf7f4b",
          "url": "https://github.com/LoveDaisy/Lumice/commit/01ed3796d7ecf82462ec49259ea9a23a1b3a2820"
        },
        "date": 1792296085124,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 950612.5,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1009344.4,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 1096747.6,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 9V45 96-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 582194.1,
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
          "id": "1ef8e6d0040a4eaf101f1b281d41bcf7cfc036a4",
          "message": "Merge pull request #510 from LoveDaisy/chore/restore-bisection-registration\n\nchore(test): restore the endpoint-bisection registry entry dropped in #507",
          "timestamp": "2026-10-18T13:00:24+08:00",
          "tree_id": "124086edda2c319809900322ef073b4ba1c6808c",
          "url": "https://github.com/LoveDaisy/Lumice/commit/1ef8e6d0040a4eaf101f1b281d41bcf7cfc036a4"
        },
        "date": 1792299789551,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 1087392.5,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1006126.5,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 725473.4,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 878499.9,
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
          "id": "872baa366d78c2065a37645eb3bd9ac33ef78cdb",
          "message": "Merge pull request #511 from LoveDaisy/feat/raypath-survey-c01-delta\n\nfeat(raypath): survey C01 full-dimensional delta machine, radial probe and chromatic layer v1",
          "timestamp": "2026-10-18T16:00:38+08:00",
          "tree_id": "d06f27173c45451f8acbcd7f8ee8d892a299bbea",
          "url": "https://github.com/LoveDaisy/Lumice/commit/872baa366d78c2065a37645eb3bd9ac33ef78cdb"
        },
        "date": 1792310639488,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 1031877.5,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1010639.7,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 727363.7,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 531639.4,
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
          "id": "be2288de2d204daa735f6abd653f3303a0dda03e",
          "message": "Merge pull request #512 from LoveDaisy/feat/raypath-survey-c15-tint\n\nfeat(raypath): survey C15 slice — weight_tint chromatic leg (smoke trilogy finale)",
          "timestamp": "2026-10-18T20:59:11+08:00",
          "tree_id": "15694e43ea5a96e16d4ddd53b45ecd9d8fdb14d6",
          "url": "https://github.com/LoveDaisy/Lumice/commit/be2288de2d204daa735f6abd653f3303a0dda03e"
        },
        "date": 1792328621934,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 921335.5,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1009248.7,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 1115990,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 9V45 96-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 685259,
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
          "id": "6edfd93e4b15b31fb7103c58cbe8e08d64ea0770",
          "message": "Merge pull request #513 from LoveDaisy/feat/axis-preset-memory\n\nGUI: axis chips switch label and parameters, Reset row replaces apply combo",
          "timestamp": "2026-10-19T08:29:50+08:00",
          "tree_id": "3b226eb5638e474f8fba92b298af79680e608bb3",
          "url": "https://github.com/LoveDaisy/Lumice/commit/6edfd93e4b15b31fb7103c58cbe8e08d64ea0770"
        },
        "date": 1792370004557,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 915407.9,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1008801.5,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 724562.5,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 533567,
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
          "id": "a0bce739786ae19df76253a07e518ea470b5f422",
          "message": "Merge pull request #514 from LoveDaisy/chore/ci-build-budget-per-miss\n\nci: per-miss build budget — adaptive limit + absolute backstop (registry v3)",
          "timestamp": "2026-10-19T09:55:27+08:00",
          "tree_id": "44c0290ab73f18304b5552f748f3c0a49390d2dd",
          "url": "https://github.com/LoveDaisy/Lumice/commit/a0bce739786ae19df76253a07e518ea470b5f422"
        },
        "date": 1792375121315,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 916967.7,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1009423.9,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 841967.3,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 9V74 80-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 580069.1,
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
          "id": "31fd56b1341b4ec0624cb56ae875d26f0d037089",
          "message": "docs: slim root AGENTS.md to constitution + route map (983→396 lines) (#515)\n\n* docs(test): sink test-tree operating rules into test/AGENTS.md\n\nStep 2 (M1) of the AGENTS.md slimming: the Testing section's operational\ndetail (three-platform matrix, placement gates, isolation discipline,\nshared-library plumbing, E2E layout, the full reference-regeneration\nworkflow) moves to test/AGENTS.md; src/raypath/AGENTS.md states the module\nsurface. Root keeps the test-scope table, judgment discipline, gate\nsummaries and the duration-registry paragraph with the cold-build contract\nstrings verbatim. Pointers that named AGENTS.md sections now name the new\nhomes (checkers, CONTRIBUTING, e2e README, testing-architecture en/zh).\n\nCo-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>\n\n* docs(index): recycle AGENTS.md doc-index summaries into the docs themselves\n\nStep 3 (M2): the Documentation Index's 38 entries collapse to one/two-line\ntrigger hooks — the docs are the sole content authority. Claim-diff against\nthe doc bodies found only three real gaps, now ported: the v4.7.0\nfilter-symmetry baseline anchor (raypath-symmetry en+zh), the CUDA TU\ncompile guard script and its gtest-zero-match false-green rationale\n(gpu-remote-cuda-build-testing), and the GPU implementation-discipline\nblock with the legacy PropagateSlab reference (seam-design header; the\n#295 incident narrative stays in gpu-route-history). The index also drops\nstale claims the docs had already superseded (overlay blueprint→as-built,\nparity 10/10→30/30).\n\nCo-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>\n\n* docs(root): compress AGENTS.md to the constitution + route-map shape\n\nStep 4 (M3): root drops from 983 to 395 lines. Code Structure one-liners\nper directory (raypath/test rules point at the sub-AGENTS files); Style\nkeeps every rule statement and checker-referenced sentence, compressing\nonly narrative blocks (launcher history, env incident, naming sub-bullets);\nthe KB section keeps the retrieval discipline and the completeness-claim\nrule with the PR #261 evidence, compressing the story; the Testing\nsection's scope table and judgment discipline stay verbatim. Line count is\nthe target, not the judge: no checker-referenced or route-bearing sentence\nwas removed to hit it.\n\nCo-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>\n\n* docs(test): reconcile duration-registry summaries with registry v3 and fix the PR-261 pointer\n\nRebase onto main (a0bce739, PR #514) moved the duration registry to v3: the\nroot AGENTS.md summary still said \"v2 / two quantities\", which no checker\nwould have caught after merge. Root side now shrinks to the pytest-pinned\nstrings plus pointers (operating rules single-sourced in test/AGENTS.md,\nrewritten for v3 semantics: per-miss adaptive ruler + absolute backstop).\nThe KB completeness-rule paragraph no longer claims \"PR #261 has the full\nnarrative\" (falsified: the PR description does not carry it) and inlines the\nfour supporting measurements instead. The windows physical-desktop line moves\nout of root (test/AGENTS.md already holds it).\n\nCo-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>\n\n---------\n\nCo-authored-by: Claude Sonnet 5 <noreply@anthropic.com>",
          "timestamp": "2026-10-19T11:25:46+08:00",
          "tree_id": "8c68466a2ee7859b6f3cbfb33be0938673d9e9c4",
          "url": "https://github.com/LoveDaisy/Lumice/commit/31fd56b1341b4ec0624cb56ae875d26f0d037089"
        },
        "date": 1792380552944,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 1197872.8,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1008062.8,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 725736.7,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 578982.5,
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
          "id": "5d28c6f252fce9b6b55e845b47eaf5cc77815f6e",
          "message": "Merge pull request #517 from LoveDaisy/chore/li-parity-fixture-lfs\n\nchore(test): track li-parity fixtures in Git LFS",
          "timestamp": "2026-10-19T16:00:23+08:00",
          "tree_id": "457c05fb6b6b02b293a736c7b48a89a1eb9f6353",
          "url": "https://github.com/LoveDaisy/Lumice/commit/5d28c6f252fce9b6b55e845b47eaf5cc77815f6e"
        },
        "date": 1792397027233,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 1067651.9,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1008479.8,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 728832.8,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 582753.7,
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
          "id": "0c999555a069ca1ba6716ec874872bf1cfd785ac",
          "message": "Merge pull request #518 from LoveDaisy/chore/per-miss-all-legs\n\nci: per-miss budget on all four legs + envelope-derived outer bounds",
          "timestamp": "2026-10-19T17:39:48+08:00",
          "tree_id": "b435ae70681f441518035f9c2c7980f4c4eeef3b",
          "url": "https://github.com/LoveDaisy/Lumice/commit/0c999555a069ca1ba6716ec874872bf1cfd785ac"
        },
        "date": 1792403152275,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 823355.1,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1010394.5,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 725075.9,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 586596.5,
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
          "id": "3d1db8784fde656bdfd16735733b0763fef6523e",
          "message": "Merge pull request #516 from LoveDaisy/refactor/math-constants-consolidation\n\nrefactor: single authority for the pi/vec3 families (census ledger item 2)",
          "timestamp": "2026-10-19T18:09:03+08:00",
          "tree_id": "a99a1ce01dce24594bbd4c6e47ccb2033cfae345",
          "url": "https://github.com/LoveDaisy/Lumice/commit/3d1db8784fde656bdfd16735733b0763fef6523e"
        },
        "date": 1792404750944,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 979607.1,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1014819.6,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 776755.3,
            "unit": "rays/sec",
            "extra": "CPU: INTEL(R) XEON(R) PLATINUM 8573C\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 583965.6,
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
          "id": "4f5f873d869a4e582eaad4d8fef0dcfcf48e1efe",
          "message": "Merge pull request #519 from LoveDaisy/chore/test-tu-cost-gate\n\nfeat(ci): test-TU cost declaration gate — fifth diff-scoped checker",
          "timestamp": "2026-10-19T18:39:25+08:00",
          "tree_id": "8ae83eaccc75164a1d8908cf61bbcecc506e8da4",
          "url": "https://github.com/LoveDaisy/Lumice/commit/4f5f873d869a4e582eaad4d8fef0dcfcf48e1efe"
        },
        "date": 1792406532320,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 1109282,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1009149.5,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 725309.5,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 923762.2,
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
          "id": "943184db19db97281bc7d892e26c1e0a4a459c76",
          "message": "Merge pull request #520 from LoveDaisy/chore/release-4.7.4\n\nchore(release): cut 4.7.4",
          "timestamp": "2026-10-19T19:23:08+08:00",
          "tree_id": "267e85e972a21e8c9dd6b409a36f43e5ef8c9b61",
          "url": "https://github.com/LoveDaisy/Lumice/commit/943184db19db97281bc7d892e26c1e0a4a459c76"
        },
        "date": 1792409189610,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 968681,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1012479,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 842505.5,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 9V74 80-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 580703.2,
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
          "id": "6a0a317eb4b27627a837076e4d21dc1dc17e3ab0",
          "message": "Merge pull request #521 from LoveDaisy/feat/axis-chip-pose-and-reset-row\n\nfeat(gui): chip switch re-seats preview pose; Reset All row (task-712)",
          "timestamp": "2026-10-20T01:55:48+08:00",
          "tree_id": "2b54e7d0492fe7227f1448ac86528fb92d531110",
          "url": "https://github.com/LoveDaisy/Lumice/commit/6a0a317eb4b27627a837076e4d21dc1dc17e3ab0"
        },
        "date": 1792432778069,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 813288.9,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1010571.9,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 799045,
            "unit": "rays/sec",
            "extra": "CPU: INTEL(R) XEON(R) PLATINUM 8573C\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 589760.1,
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
          "id": "4dfe2ac445b752957a1a13cbb722bb0f12a53d12",
          "message": "Merge pull request #522 from LoveDaisy/feat/gui-visual-declutter\n\nfeat(gui): right-panel visual declutter — Continue dim, Resolution de-rust, aspect combo merge (task-713)",
          "timestamp": "2026-10-20T02:27:18+08:00",
          "tree_id": "ff8a85581ce9896080bb1822c5962ddbef7910ee",
          "url": "https://github.com/LoveDaisy/Lumice/commit/4dfe2ac445b752957a1a13cbb722bb0f12a53d12"
        },
        "date": 1792434791160,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 834574.6,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1010160,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 722442.8,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 590376.4,
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
          "id": "6997a5a16a019e98c1819cb1b23b8a6c42f5fc33",
          "message": "Merge pull request #523 from LoveDaisy/feat/raypath-survey-c17-c06-c09\n\nfeat(survey): C17/C06/C09 wiring — witness, dual-mechanism chroma, support set (task-714)",
          "timestamp": "2026-10-20T06:23:22+08:00",
          "tree_id": "aa3e3ba9cc15c5e4714f8d861ef924a19ad59994",
          "url": "https://github.com/LoveDaisy/Lumice/commit/6997a5a16a019e98c1819cb1b23b8a6c42f5fc33"
        },
        "date": 1792448821129,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 943325.7,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1011068.3,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 721915.7,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 581489.9,
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
          "id": "d36f13688d6b3d03758b35e63d5a5936f8558fac",
          "message": "Merge pull request #524 from LoveDaisy/chore/fullsky-bound-comment\n\nchore(test): rewrite the full-sky identity pin's tolerance comment to the real mechanism",
          "timestamp": "2026-10-20T10:17:44+08:00",
          "tree_id": "2a6d01b61c4d30d7ae2cb4e9ef9897254515605b",
          "url": "https://github.com/LoveDaisy/Lumice/commit/d36f13688d6b3d03758b35e63d5a5936f8558fac"
        },
        "date": 1792462887115,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 924942.7,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1012277.8,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 728069,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 585625.5,
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
          "id": "471ce7726c6d62522bd87ff000726cbcd454d649",
          "message": "Merge pull request #525 from LoveDaisy/chore/comdat-flake-doc-note\n\ndocs(testing): record the COMDAT x -march local-red/CI-green flake mechanism (§4.6.1)",
          "timestamp": "2026-10-20T10:42:04+08:00",
          "tree_id": "3331d03171eadc032df781091ed4a7668619d608",
          "url": "https://github.com/LoveDaisy/Lumice/commit/471ce7726c6d62522bd87ff000726cbcd454d649"
        },
        "date": 1792464323219,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 1190251.5,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1014133.6,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 724364.3,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 608281,
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
          "id": "c10466366a3f9d19d42a890a092389cebaade1b0",
          "message": "Merge pull request #526 from LoveDaisy/feat/cl-exe-shared-ipo\n\nfeat(cmake): enable IPO for cl.exe shared Release trees",
          "timestamp": "2026-10-20T12:57:03+08:00",
          "tree_id": "38f1949b834ef1d5cd955ecb0db8f7a0c5e04fb2",
          "url": "https://github.com/LoveDaisy/Lumice/commit/c10466366a3f9d19d42a890a092389cebaade1b0"
        },
        "date": 1792472448964,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 807103.7,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1011119.9,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 1100212,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 9V45 96-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 576412.6,
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
          "id": "0133b0e9825ce175d55d5cfcb43ed65fd904b519",
          "message": "Merge pull request #527 from LoveDaisy/feat/core-json-grid-import-notice\n\nfeat(gui): notice render[].grid refusal on core JSON import",
          "timestamp": "2026-10-20T15:49:47+08:00",
          "tree_id": "d94d4836689f7f76be563603c87a73e502fbd55c",
          "url": "https://github.com/LoveDaisy/Lumice/commit/0133b0e9825ce175d55d5cfcb43ed65fd904b519"
        },
        "date": 1792482787660,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 877317.5,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1014866,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 1115403.1,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 9V45 96-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 582552.2,
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
          "id": "cb6c6b6a30094b1868d511ae523a9a244055462f",
          "message": "Merge pull request #528 from LoveDaisy/chore/rng-cross-stdlib-contract-doc\n\ndocs: state the cross-stdlib RNG contract instead of implying bit-exact replay",
          "timestamp": "2026-10-20T16:27:35+08:00",
          "tree_id": "6402f39a5ce3737c1d821ff1782754258edaf12f",
          "url": "https://github.com/LoveDaisy/Lumice/commit/cb6c6b6a30094b1868d511ae523a9a244055462f"
        },
        "date": 1792485064060,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 878151.9,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1012536.4,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 1084191.6,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 9V45 96-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 671275.3,
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
          "id": "d1b929b7a581fc70342002de9711cfb3760b2e62",
          "message": "Merge pull request #529 from LoveDaisy/feat/lens-proj-recalibration\n\ntest(gui): recalibrate the lens_proj reference group (drift +1.2~+2.5 dB)",
          "timestamp": "2026-10-20T20:41:06+08:00",
          "tree_id": "8ec07448bbb5d138099669297f4ba99267172564",
          "url": "https://github.com/LoveDaisy/Lumice/commit/d1b929b7a581fc70342002de9711cfb3760b2e62"
        },
        "date": 1792500270185,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 996241.2,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1013210.5,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 811459.9,
            "unit": "rays/sec",
            "extra": "CPU: Intel(R) Xeon(R) Platinum 8370C CPU @ 2.80GHz\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 584312.2,
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
          "id": "a4d56f0943b8fe3e078f72c8b136692f9692b114",
          "message": "Merge pull request #530 from LoveDaisy/feat/gap1-family-routing\n\nfeat(raypath): gap-1 measure-to-family routing (first wave 9/9)",
          "timestamp": "2026-10-20T22:27:14+08:00",
          "tree_id": "77c386a99074b5e97dfb8278a6d65fcc30ed649b",
          "url": "https://github.com/LoveDaisy/Lumice/commit/a4d56f0943b8fe3e078f72c8b136692f9692b114"
        },
        "date": 1792506646786,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 961248.3,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1012351.9,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 839785.4,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 9V74 80-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 587004.3,
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
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "edebcf3766408a5da0e9750157314d7325aaa469",
          "message": "Merge pull request #483 from LoveDaisy/feat/look-at-target-lock\n\nfeat(gui): add Look At target locking with manual takeover",
          "timestamp": "2026-10-11T21:06:44+08:00",
          "tree_id": "fb7774db9dcb6b30ed6ab81c83175faa1687100b",
          "url": "https://github.com/LoveDaisy/Lumice/commit/edebcf3766408a5da0e9750157314d7325aaa469"
        },
        "date": 1791724202846,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 102.7,
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
          "id": "edebcf3766408a5da0e9750157314d7325aaa469",
          "message": "Merge pull request #483 from LoveDaisy/feat/look-at-target-lock\n\nfeat(gui): add Look At target locking with manual takeover",
          "timestamp": "2026-10-11T21:06:44+08:00",
          "tree_id": "fb7774db9dcb6b30ed6ab81c83175faa1687100b",
          "url": "https://github.com/LoveDaisy/Lumice/commit/edebcf3766408a5da0e9750157314d7325aaa469"
        },
        "date": 1791725232639,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 89.9,
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
          "id": "87a9d3bd2b85c0f0373e5ecf32c77bd163a39cb6",
          "message": "Merge pull request #484 from LoveDaisy/chore/release-4.7.3\n\nchore(release): cut 4.7.3",
          "timestamp": "2026-10-12T13:03:05+08:00",
          "tree_id": "dc8ccddd040a6828e7b0f0bd8e0d347da506521e",
          "url": "https://github.com/LoveDaisy/Lumice/commit/87a9d3bd2b85c0f0373e5ecf32c77bd163a39cb6"
        },
        "date": 1791781595244,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 91.9,
            "unit": "%"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 100,
            "unit": "%"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 99.7,
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
          "id": "46671819d8f4c160a624710d644cfca2c5892361",
          "message": "Merge pull request #485 from LoveDaisy/chore/migrate-repo-name-lumice\n\ndocs: migrate repo name ice_halo_sim to Lumice",
          "timestamp": "2026-10-12T13:34:20+08:00",
          "tree_id": "aec7c127f5820de64118e9f6a204ec6cb003c8ff",
          "url": "https://github.com/LoveDaisy/Lumice/commit/46671819d8f4c160a624710d644cfca2c5892361"
        },
        "date": 1791783427775,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 89.6,
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
          "id": "e1ac9076ac5ba61fb78e51027bd35f89f6be05d7",
          "message": "Merge pull request #486 from LoveDaisy/fix/sun-diameter-zero-min\n\nfix(gui): allow sun diameter 0 (point source) in the GUI domain",
          "timestamp": "2026-10-12T20:22:38+08:00",
          "tree_id": "306c8309b25fc330b0d3e620e8b4ba4b488264b3",
          "url": "https://github.com/LoveDaisy/Lumice/commit/e1ac9076ac5ba61fb78e51027bd35f89f6be05d7"
        },
        "date": 1791807954044,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 86.6,
            "unit": "%"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 99.8,
            "unit": "%"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 102.4,
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
          "id": "cdb4c383c1637dc4de908ea64859de306a7ec22c",
          "message": "Merge pull request #487 from LoveDaisy/fix/look-at-row-alignment\n\nfix(gui): align the View Look At row to the panel's control grid",
          "timestamp": "2026-10-12T22:17:52+08:00",
          "tree_id": "684e0d405dc952e70143ce6cf8d78cd922de7278",
          "url": "https://github.com/LoveDaisy/Lumice/commit/cdb4c383c1637dc4de908ea64859de306a7ec22c"
        },
        "date": 1791815061923,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 98.6,
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
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "962a92222a309a30e539b51a23aaafe5da021605",
          "message": "Merge pull request #488 from LoveDaisy/fix/ci-seed-audit-false-positive\n\nfix(ci): fix ccache seed audit false positive (one write per miss)",
          "timestamp": "2026-10-13T00:11:07+08:00",
          "tree_id": "b8ad04d108363e75b26db167440a99bf58627e1e",
          "url": "https://github.com/LoveDaisy/Lumice/commit/962a92222a309a30e539b51a23aaafe5da021605"
        },
        "date": 1791821674878,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 79.4,
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
          "id": "b45848cea4b9c09ffeec37e4f3f442ae13cd62e6",
          "message": "Merge pull request #489 from LoveDaisy/fix/look-at-row-grid-polish\n\nFix: revert right panel to 300px and end the Look At row on the Lock square",
          "timestamp": "2026-10-13T07:07:41+08:00",
          "tree_id": "5b2702f367d17c7dd47f5ddeb66a544d1cd3c8e1",
          "url": "https://github.com/LoveDaisy/Lumice/commit/b45848cea4b9c09ffeec37e4f3f442ae13cd62e6"
        },
        "date": 1791846643819,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 79.6,
            "unit": "%"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 100.2,
            "unit": "%"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 97.8,
            "unit": "%"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 95.2,
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
          "id": "5b267889368334c2aed943d7bae310becb382cba",
          "message": "Merge pull request #491 from LoveDaisy/chore/release-4.7.3-recut\n\nchore(release): recut 4.7.3",
          "timestamp": "2026-10-13T07:28:24+08:00",
          "tree_id": "b91244d1d5185b200796d088072f6fd80ca186d1",
          "url": "https://github.com/LoveDaisy/Lumice/commit/5b267889368334c2aed943d7bae310becb382cba"
        },
        "date": 1791847905236,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 97.4,
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
          "id": "7ce6259807c4d988af8af1a1bee70b276ca2173e",
          "message": "Merge pull request #492 from LoveDaisy/chore/raypath-docs-current-state\n\ndocs(raypath): align analysis docs with the current product state",
          "timestamp": "2026-10-14T18:27:14+08:00",
          "tree_id": "b5ab7d4c031478152f3808435eb5a75fa97f6fbf",
          "url": "https://github.com/LoveDaisy/Lumice/commit/7ce6259807c4d988af8af1a1bee70b276ca2173e"
        },
        "date": 1791973847340,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 94.4,
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
          "id": "d732ea5937ccbe41714459898520e6607d066b12",
          "message": "Merge pull request #493 from LoveDaisy/fix/gui-test-window-size-leak\n\ntest(gui): restore shared window basis at case entry with a leak sentinel",
          "timestamp": "2026-10-14T20:16:19+08:00",
          "tree_id": "18a408014d7f5b0bdfc2b2667c2dd6fa2d21835e",
          "url": "https://github.com/LoveDaisy/Lumice/commit/d732ea5937ccbe41714459898520e6607d066b12"
        },
        "date": 1791980366663,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 95.7,
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
          "id": "92c54af058536b731f3f2a7bc50a444704146b7f",
          "message": "Merge pull request #494 from LoveDaisy/feat/analysis-crystal-id-mismatch\n\nfix(gui): align analysis list crystal numbers with card IDs",
          "timestamp": "2026-10-15T01:51:21+08:00",
          "tree_id": "d43c91f95f5daa0bd5c9b3c56b99635d451c6af2",
          "url": "https://github.com/LoveDaisy/Lumice/commit/92c54af058536b731f3f2a7bc50a444704146b7f"
        },
        "date": 1792000477226,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 80.4,
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
          "id": "e24525033315c4fa88cce6199342f7f62cee94d3",
          "message": "Merge pull request #495 from LoveDaisy/feat/axis-sticky-label\n\nfeat(gui): sticky user-chosen axis label, decoupled from preset apply",
          "timestamp": "2026-10-16T09:27:12+08:00",
          "tree_id": "839d54c4e434bf7ae6b44932a72e5fcd24c3bdac",
          "url": "https://github.com/LoveDaisy/Lumice/commit/e24525033315c4fa88cce6199342f7f62cee94d3"
        },
        "date": 1792114225259,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 89.7,
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
          "id": "d3b5b77505182ea5e0fff0873d106cc506c38f0c",
          "message": "Merge pull request #496 from LoveDaisy/feat/axis-apply-decouple\n\nfeat(gui): Apply preset is a pure parameter shortcut, label and view untouched",
          "timestamp": "2026-10-16T21:21:19+08:00",
          "tree_id": "4d4ba181a95d82bf92b42afdd4e29619e4d7710c",
          "url": "https://github.com/LoveDaisy/Lumice/commit/d3b5b77505182ea5e0fff0873d106cc506c38f0c"
        },
        "date": 1792157055844,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 95.3,
            "unit": "%"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 100.3,
            "unit": "%"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 99.1,
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
          "id": "c3372ae22b354c4456f0c52e19980930451b511a",
          "message": "Merge pull request #497 from LoveDaisy/feat/entry-edit-weight-toggle\n\nfeat(gui): entry bar in Edit Entry — weight and participation toggle in the modal",
          "timestamp": "2026-10-16T22:18:44+08:00",
          "tree_id": "1b15038814229189d3747ad68a70aad46c55ed88",
          "url": "https://github.com/LoveDaisy/Lumice/commit/c3372ae22b354c4456f0c52e19980930451b511a"
        },
        "date": 1792160498429,
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
          "id": "661f6814adafc3ae410096f9f67ed0cbcfe37403",
          "message": "Merge pull request #498 from LoveDaisy/chore/doc-topic-organization\n\ndocs: organize documentation into topic directories",
          "timestamp": "2026-10-17T01:50:13+08:00",
          "tree_id": "c1cf0a07d8d95b945e5279c062b26efaf6c926b0",
          "url": "https://github.com/LoveDaisy/Lumice/commit/661f6814adafc3ae410096f9f67ed0cbcfe37403"
        },
        "date": 1792173206116,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 100.1,
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
          "id": "a8a3d13f7e30f924bc710a8e6693a2e9a3cfc20c",
          "message": "Merge pull request #500 from LoveDaisy/docs/doc-code-consistency\n\ndocs: reconcile guides and architecture with current implementation",
          "timestamp": "2026-10-17T12:16:41+08:00",
          "tree_id": "0c9364b710adfa1f3ffbca4ad5afb4d536b131ab",
          "url": "https://github.com/LoveDaisy/Lumice/commit/a8a3d13f7e30f924bc710a8e6693a2e9a3cfc20c"
        },
        "date": 1792210808010,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 90.7,
            "unit": "%"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 100,
            "unit": "%"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 96.8,
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
          "id": "7578ffac709fd28e350dc9de11f7cd14c68c6bd0",
          "message": "Merge pull request #501 from LoveDaisy/fix/xyz-stats-current-capi\n\nFix dump_xyz_stats.py: consume current C API contract via CLI npy export",
          "timestamp": "2026-10-17T15:52:48+08:00",
          "tree_id": "b76c99c2e7759bc60276b944ee897ddb60ef96a4",
          "url": "https://github.com/LoveDaisy/Lumice/commit/7578ffac709fd28e350dc9de11f7cd14c68c6bd0"
        },
        "date": 1792223724257,
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
            "value": 98.1,
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
          "id": "2d8b602f597c2077eef981ad5ed97922620caecc",
          "message": "Merge pull request #502 from LoveDaisy/fix/cli-verbose-help-level\n\nfix(cli): describe -v as verbose level in help text",
          "timestamp": "2026-10-17T16:38:00+08:00",
          "tree_id": "ee41ffe66d42cacfc9fedf0ceb7e26dc02a0ea97",
          "url": "https://github.com/LoveDaisy/Lumice/commit/2d8b602f597c2077eef981ad5ed97922620caecc"
        },
        "date": 1792226466095,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 98.7,
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
          "id": "d18ef073971c4a72f49515f970489327eb2d6464",
          "message": "Merge pull request #503 from LoveDaisy/fix/cli-verbose-help-level\n\nfix(cli): describe -v as verbose level in help text, pin -v/-d level mapping",
          "timestamp": "2026-10-17T16:55:44+08:00",
          "tree_id": "721ce317bc2328c82f6a85758e3c0e11ecfee029",
          "url": "https://github.com/LoveDaisy/Lumice/commit/d18ef073971c4a72f49515f970489327eb2d6464"
        },
        "date": 1792227543977,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 88.7,
            "unit": "%"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 99.8,
            "unit": "%"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 99,
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
          "id": "35bde7ee5db9a00e9d59df65f6488a4e873fd9df",
          "message": "Merge pull request #504 from LoveDaisy/fix/lens-border-print-colour-gate\n\nfix(gui): gate lens border colour under print mode (join NotUnderPrintMode family)",
          "timestamp": "2026-10-17T17:14:39+08:00",
          "tree_id": "f863f15f651274c6567920c6d747669634cf2c78",
          "url": "https://github.com/LoveDaisy/Lumice/commit/35bde7ee5db9a00e9d59df65f6488a4e873fd9df"
        },
        "date": 1792228679209,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 89.8,
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
          "id": "43a72d655ad2a9f3ab50650b80bc0bafeddeb49a",
          "message": "Merge pull request #505 from LoveDaisy/chore/source-comment-consistency\n\nchore(source-comment-consistency): rewrite five drift groups of src/ comments",
          "timestamp": "2026-10-17T17:35:59+08:00",
          "tree_id": "d4e5b5d64f6ec519bee36522169fafe1ef1e335a",
          "url": "https://github.com/LoveDaisy/Lumice/commit/43a72d655ad2a9f3ab50650b80bc0bafeddeb49a"
        },
        "date": 1792230027544,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 101.4,
            "unit": "%"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 100.1,
            "unit": "%"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 103.3,
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
          "id": "9f6df4d7e70c2cd552b73298382577b9399e17d1",
          "message": "Merge pull request #506 from LoveDaisy/fix/verbose-witness-dead-site\n\nfeat(cli): make -v/-d observable — drop dead VERBOSE site, add parse-time level lines",
          "timestamp": "2026-10-17T18:54:32+08:00",
          "tree_id": "4ad751e4775236fa65d6490bf1d0dd4ec37524c1",
          "url": "https://github.com/LoveDaisy/Lumice/commit/9f6df4d7e70c2cd552b73298382577b9399e17d1"
        },
        "date": 1792234613625,
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
            "value": 98.4,
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
          "id": "e74a6fad65c33336e274a3929c5f9121f4339a44",
          "message": "Merge pull request #507 from LoveDaisy/chore/e2e-step-budget-runner-variance\n\ntest/e2e cost trims + shadow bench probe (697 PR1: restore work headroom, instrument the budget)",
          "timestamp": "2026-10-17T23:21:07+08:00",
          "tree_id": "b3064b0619f8ecd539d6992a56acf002c7bac678",
          "url": "https://github.com/LoveDaisy/Lumice/commit/e74a6fad65c33336e274a3929c5f9121f4339a44"
        },
        "date": 1792250653943,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 96.8,
            "unit": "%"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 100.3,
            "unit": "%"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 99.2,
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
          "id": "b5112a34db028a9f3ad0ae392c2a1769e42ea6f7",
          "message": "Merge pull request #509 from LoveDaisy/feat/api-version-narrow-header\n\nrefactor(capi): move LUMICE_API_VERSION to a leaf header",
          "timestamp": "2026-10-18T10:16:11+08:00",
          "tree_id": "9bdfa4c447ed393ed682a9709fdfa31a03e7e980",
          "url": "https://github.com/LoveDaisy/Lumice/commit/b5112a34db028a9f3ad0ae392c2a1769e42ea6f7"
        },
        "date": 1792289982479,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 85.9,
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
          "id": "b5112a34db028a9f3ad0ae392c2a1769e42ea6f7",
          "message": "Merge pull request #509 from LoveDaisy/feat/api-version-narrow-header\n\nrefactor(capi): move LUMICE_API_VERSION to a leaf header",
          "timestamp": "2026-10-18T10:16:11+08:00",
          "tree_id": "9bdfa4c447ed393ed682a9709fdfa31a03e7e980",
          "url": "https://github.com/LoveDaisy/Lumice/commit/b5112a34db028a9f3ad0ae392c2a1769e42ea6f7"
        },
        "date": 1792291254387,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 92,
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
          "id": "01ed3796d7ecf82462ec49259ea9a23a1b3a2820",
          "message": "Merge pull request #508 from LoveDaisy/feat/raypath-survey-c14-smoke\n\nfeat(raypath): survey CLI skeleton + C14 smoke slice (schema4)",
          "timestamp": "2026-10-18T11:57:21+08:00",
          "tree_id": "19d2870c77a0856c4cf48a574aea58c803bf7f4b",
          "url": "https://github.com/LoveDaisy/Lumice/commit/01ed3796d7ecf82462ec49259ea9a23a1b3a2820"
        },
        "date": 1792296087223,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 93.5,
            "unit": "%"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 100.1,
            "unit": "%"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 96.3,
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
          "id": "1ef8e6d0040a4eaf101f1b281d41bcf7cfc036a4",
          "message": "Merge pull request #510 from LoveDaisy/chore/restore-bisection-registration\n\nchore(test): restore the endpoint-bisection registry entry dropped in #507",
          "timestamp": "2026-10-18T13:00:24+08:00",
          "tree_id": "124086edda2c319809900322ef073b4ba1c6808c",
          "url": "https://github.com/LoveDaisy/Lumice/commit/1ef8e6d0040a4eaf101f1b281d41bcf7cfc036a4"
        },
        "date": 1792299791734,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 89.5,
            "unit": "%"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 99.9,
            "unit": "%"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 99.8,
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
          "id": "872baa366d78c2065a37645eb3bd9ac33ef78cdb",
          "message": "Merge pull request #511 from LoveDaisy/feat/raypath-survey-c01-delta\n\nfeat(raypath): survey C01 full-dimensional delta machine, radial probe and chromatic layer v1",
          "timestamp": "2026-10-18T16:00:38+08:00",
          "tree_id": "d06f27173c45451f8acbcd7f8ee8d892a299bbea",
          "url": "https://github.com/LoveDaisy/Lumice/commit/872baa366d78c2065a37645eb3bd9ac33ef78cdb"
        },
        "date": 1792310641906,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 83.9,
            "unit": "%"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 99.6,
            "unit": "%"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 100.5,
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
          "id": "be2288de2d204daa735f6abd653f3303a0dda03e",
          "message": "Merge pull request #512 from LoveDaisy/feat/raypath-survey-c15-tint\n\nfeat(raypath): survey C15 slice — weight_tint chromatic leg (smoke trilogy finale)",
          "timestamp": "2026-10-18T20:59:11+08:00",
          "tree_id": "15694e43ea5a96e16d4ddd53b45ecd9d8fdb14d6",
          "url": "https://github.com/LoveDaisy/Lumice/commit/be2288de2d204daa735f6abd653f3303a0dda03e"
        },
        "date": 1792328624212,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 94.2,
            "unit": "%"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 99.9,
            "unit": "%"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 97.1,
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
          "id": "6edfd93e4b15b31fb7103c58cbe8e08d64ea0770",
          "message": "Merge pull request #513 from LoveDaisy/feat/axis-preset-memory\n\nGUI: axis chips switch label and parameters, Reset row replaces apply combo",
          "timestamp": "2026-10-19T08:29:50+08:00",
          "tree_id": "3b226eb5638e474f8fba92b298af79680e608bb3",
          "url": "https://github.com/LoveDaisy/Lumice/commit/6edfd93e4b15b31fb7103c58cbe8e08d64ea0770"
        },
        "date": 1792370006874,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 88.5,
            "unit": "%"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 99.6,
            "unit": "%"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 98.2,
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
          "id": "a0bce739786ae19df76253a07e518ea470b5f422",
          "message": "Merge pull request #514 from LoveDaisy/chore/ci-build-budget-per-miss\n\nci: per-miss build budget — adaptive limit + absolute backstop (registry v3)",
          "timestamp": "2026-10-19T09:55:27+08:00",
          "tree_id": "44c0290ab73f18304b5552f748f3c0a49390d2dd",
          "url": "https://github.com/LoveDaisy/Lumice/commit/a0bce739786ae19df76253a07e518ea470b5f422"
        },
        "date": 1792375123690,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 93.4,
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
          "id": "31fd56b1341b4ec0624cb56ae875d26f0d037089",
          "message": "docs: slim root AGENTS.md to constitution + route map (983→396 lines) (#515)\n\n* docs(test): sink test-tree operating rules into test/AGENTS.md\n\nStep 2 (M1) of the AGENTS.md slimming: the Testing section's operational\ndetail (three-platform matrix, placement gates, isolation discipline,\nshared-library plumbing, E2E layout, the full reference-regeneration\nworkflow) moves to test/AGENTS.md; src/raypath/AGENTS.md states the module\nsurface. Root keeps the test-scope table, judgment discipline, gate\nsummaries and the duration-registry paragraph with the cold-build contract\nstrings verbatim. Pointers that named AGENTS.md sections now name the new\nhomes (checkers, CONTRIBUTING, e2e README, testing-architecture en/zh).\n\nCo-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>\n\n* docs(index): recycle AGENTS.md doc-index summaries into the docs themselves\n\nStep 3 (M2): the Documentation Index's 38 entries collapse to one/two-line\ntrigger hooks — the docs are the sole content authority. Claim-diff against\nthe doc bodies found only three real gaps, now ported: the v4.7.0\nfilter-symmetry baseline anchor (raypath-symmetry en+zh), the CUDA TU\ncompile guard script and its gtest-zero-match false-green rationale\n(gpu-remote-cuda-build-testing), and the GPU implementation-discipline\nblock with the legacy PropagateSlab reference (seam-design header; the\n#295 incident narrative stays in gpu-route-history). The index also drops\nstale claims the docs had already superseded (overlay blueprint→as-built,\nparity 10/10→30/30).\n\nCo-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>\n\n* docs(root): compress AGENTS.md to the constitution + route-map shape\n\nStep 4 (M3): root drops from 983 to 395 lines. Code Structure one-liners\nper directory (raypath/test rules point at the sub-AGENTS files); Style\nkeeps every rule statement and checker-referenced sentence, compressing\nonly narrative blocks (launcher history, env incident, naming sub-bullets);\nthe KB section keeps the retrieval discipline and the completeness-claim\nrule with the PR #261 evidence, compressing the story; the Testing\nsection's scope table and judgment discipline stay verbatim. Line count is\nthe target, not the judge: no checker-referenced or route-bearing sentence\nwas removed to hit it.\n\nCo-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>\n\n* docs(test): reconcile duration-registry summaries with registry v3 and fix the PR-261 pointer\n\nRebase onto main (a0bce739, PR #514) moved the duration registry to v3: the\nroot AGENTS.md summary still said \"v2 / two quantities\", which no checker\nwould have caught after merge. Root side now shrinks to the pytest-pinned\nstrings plus pointers (operating rules single-sourced in test/AGENTS.md,\nrewritten for v3 semantics: per-miss adaptive ruler + absolute backstop).\nThe KB completeness-rule paragraph no longer claims \"PR #261 has the full\nnarrative\" (falsified: the PR description does not carry it) and inlines the\nfour supporting measurements instead. The windows physical-desktop line moves\nout of root (test/AGENTS.md already holds it).\n\nCo-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>\n\n---------\n\nCo-authored-by: Claude Sonnet 5 <noreply@anthropic.com>",
          "timestamp": "2026-10-19T11:25:46+08:00",
          "tree_id": "8c68466a2ee7859b6f3cbfb33be0938673d9e9c4",
          "url": "https://github.com/LoveDaisy/Lumice/commit/31fd56b1341b4ec0624cb56ae875d26f0d037089"
        },
        "date": 1792380555223,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 89.8,
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
          "id": "5d28c6f252fce9b6b55e845b47eaf5cc77815f6e",
          "message": "Merge pull request #517 from LoveDaisy/chore/li-parity-fixture-lfs\n\nchore(test): track li-parity fixtures in Git LFS",
          "timestamp": "2026-10-19T16:00:23+08:00",
          "tree_id": "457c05fb6b6b02b293a736c7b48a89a1eb9f6353",
          "url": "https://github.com/LoveDaisy/Lumice/commit/5d28c6f252fce9b6b55e845b47eaf5cc77815f6e"
        },
        "date": 1792397030054,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 91.3,
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
          "id": "0c999555a069ca1ba6716ec874872bf1cfd785ac",
          "message": "Merge pull request #518 from LoveDaisy/chore/per-miss-all-legs\n\nci: per-miss budget on all four legs + envelope-derived outer bounds",
          "timestamp": "2026-10-19T17:39:48+08:00",
          "tree_id": "b435ae70681f441518035f9c2c7980f4c4eeef3b",
          "url": "https://github.com/LoveDaisy/Lumice/commit/0c999555a069ca1ba6716ec874872bf1cfd785ac"
        },
        "date": 1792403155540,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 85.1,
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
          "id": "3d1db8784fde656bdfd16735733b0763fef6523e",
          "message": "Merge pull request #516 from LoveDaisy/refactor/math-constants-consolidation\n\nrefactor: single authority for the pi/vec3 families (census ledger item 2)",
          "timestamp": "2026-10-19T18:09:03+08:00",
          "tree_id": "a99a1ce01dce24594bbd4c6e47ccb2033cfae345",
          "url": "https://github.com/LoveDaisy/Lumice/commit/3d1db8784fde656bdfd16735733b0763fef6523e"
        },
        "date": 1792404753331,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 101.7,
            "unit": "%"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 100.1,
            "unit": "%"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 97.7,
            "unit": "%"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 92.3,
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
          "id": "4f5f873d869a4e582eaad4d8fef0dcfcf48e1efe",
          "message": "Merge pull request #519 from LoveDaisy/chore/test-tu-cost-gate\n\nfeat(ci): test-TU cost declaration gate — fifth diff-scoped checker",
          "timestamp": "2026-10-19T18:39:25+08:00",
          "tree_id": "8ae83eaccc75164a1d8908cf61bbcecc506e8da4",
          "url": "https://github.com/LoveDaisy/Lumice/commit/4f5f873d869a4e582eaad4d8fef0dcfcf48e1efe"
        },
        "date": 1792406534339,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 89.6,
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
          "id": "943184db19db97281bc7d892e26c1e0a4a459c76",
          "message": "Merge pull request #520 from LoveDaisy/chore/release-4.7.4\n\nchore(release): cut 4.7.4",
          "timestamp": "2026-10-19T19:23:08+08:00",
          "tree_id": "267e85e972a21e8c9dd6b409a36f43e5ef8c9b61",
          "url": "https://github.com/LoveDaisy/Lumice/commit/943184db19db97281bc7d892e26c1e0a4a459c76"
        },
        "date": 1792409191858,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 97.3,
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
          "id": "6a0a317eb4b27627a837076e4d21dc1dc17e3ab0",
          "message": "Merge pull request #521 from LoveDaisy/feat/axis-chip-pose-and-reset-row\n\nfeat(gui): chip switch re-seats preview pose; Reset All row (task-712)",
          "timestamp": "2026-10-20T01:55:48+08:00",
          "tree_id": "2b54e7d0492fe7227f1448ac86528fb92d531110",
          "url": "https://github.com/LoveDaisy/Lumice/commit/6a0a317eb4b27627a837076e4d21dc1dc17e3ab0"
        },
        "date": 1792432780796,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 85.8,
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
          "id": "4dfe2ac445b752957a1a13cbb722bb0f12a53d12",
          "message": "Merge pull request #522 from LoveDaisy/feat/gui-visual-declutter\n\nfeat(gui): right-panel visual declutter — Continue dim, Resolution de-rust, aspect combo merge (task-713)",
          "timestamp": "2026-10-20T02:27:18+08:00",
          "tree_id": "ff8a85581ce9896080bb1822c5962ddbef7910ee",
          "url": "https://github.com/LoveDaisy/Lumice/commit/4dfe2ac445b752957a1a13cbb722bb0f12a53d12"
        },
        "date": 1792434793442,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 96.9,
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
          "id": "6997a5a16a019e98c1819cb1b23b8a6c42f5fc33",
          "message": "Merge pull request #523 from LoveDaisy/feat/raypath-survey-c17-c06-c09\n\nfeat(survey): C17/C06/C09 wiring — witness, dual-mechanism chroma, support set (task-714)",
          "timestamp": "2026-10-20T06:23:22+08:00",
          "tree_id": "aa3e3ba9cc15c5e4714f8d861ef924a19ad59994",
          "url": "https://github.com/LoveDaisy/Lumice/commit/6997a5a16a019e98c1819cb1b23b8a6c42f5fc33"
        },
        "date": 1792448823097,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 83.5,
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
          "id": "d36f13688d6b3d03758b35e63d5a5936f8558fac",
          "message": "Merge pull request #524 from LoveDaisy/chore/fullsky-bound-comment\n\nchore(test): rewrite the full-sky identity pin's tolerance comment to the real mechanism",
          "timestamp": "2026-10-20T10:17:44+08:00",
          "tree_id": "2a6d01b61c4d30d7ae2cb4e9ef9897254515605b",
          "url": "https://github.com/LoveDaisy/Lumice/commit/d36f13688d6b3d03758b35e63d5a5936f8558fac"
        },
        "date": 1792462889411,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 93.4,
            "unit": "%"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 100.3,
            "unit": "%"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 98.9,
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
          "id": "471ce7726c6d62522bd87ff000726cbcd454d649",
          "message": "Merge pull request #525 from LoveDaisy/chore/comdat-flake-doc-note\n\ndocs(testing): record the COMDAT x -march local-red/CI-green flake mechanism (§4.6.1)",
          "timestamp": "2026-10-20T10:42:04+08:00",
          "tree_id": "3331d03171eadc032df781091ed4a7668619d608",
          "url": "https://github.com/LoveDaisy/Lumice/commit/471ce7726c6d62522bd87ff000726cbcd454d649"
        },
        "date": 1792464326470,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 98.7,
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
          "id": "c10466366a3f9d19d42a890a092389cebaade1b0",
          "message": "Merge pull request #526 from LoveDaisy/feat/cl-exe-shared-ipo\n\nfeat(cmake): enable IPO for cl.exe shared Release trees",
          "timestamp": "2026-10-20T12:57:03+08:00",
          "tree_id": "38f1949b834ef1d5cd955ecb0db8f7a0c5e04fb2",
          "url": "https://github.com/LoveDaisy/Lumice/commit/c10466366a3f9d19d42a890a092389cebaade1b0"
        },
        "date": 1792472451084,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 102.8,
            "unit": "%"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 100.3,
            "unit": "%"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 99.3,
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
          "id": "0133b0e9825ce175d55d5cfcb43ed65fd904b519",
          "message": "Merge pull request #527 from LoveDaisy/feat/core-json-grid-import-notice\n\nfeat(gui): notice render[].grid refusal on core JSON import",
          "timestamp": "2026-10-20T15:49:47+08:00",
          "tree_id": "d94d4836689f7f76be563603c87a73e502fbd55c",
          "url": "https://github.com/LoveDaisy/Lumice/commit/0133b0e9825ce175d55d5cfcb43ed65fd904b519"
        },
        "date": 1792482789467,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 86.7,
            "unit": "%"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 100.1,
            "unit": "%"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 97.6,
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
          "id": "cb6c6b6a30094b1868d511ae523a9a244055462f",
          "message": "Merge pull request #528 from LoveDaisy/chore/rng-cross-stdlib-contract-doc\n\ndocs: state the cross-stdlib RNG contract instead of implying bit-exact replay",
          "timestamp": "2026-10-20T16:27:35+08:00",
          "tree_id": "6402f39a5ce3737c1d821ff1782754258edaf12f",
          "url": "https://github.com/LoveDaisy/Lumice/commit/cb6c6b6a30094b1868d511ae523a9a244055462f"
        },
        "date": 1792485066737,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 95.7,
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
          "id": "d1b929b7a581fc70342002de9711cfb3760b2e62",
          "message": "Merge pull request #529 from LoveDaisy/feat/lens-proj-recalibration\n\ntest(gui): recalibrate the lens_proj reference group (drift +1.2~+2.5 dB)",
          "timestamp": "2026-10-20T20:41:06+08:00",
          "tree_id": "8ec07448bbb5d138099669297f4ba99267172564",
          "url": "https://github.com/LoveDaisy/Lumice/commit/d1b929b7a581fc70342002de9711cfb3760b2e62"
        },
        "date": 1792500272639,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 83,
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
      }
    ]
  }
}