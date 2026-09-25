window.BENCHMARK_DATA = {
  "lastUpdate": 1790358575987,
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
          "id": "c04ad137d42e4a3ed51cd137b75d2d97092b7864",
          "message": "Merge pull request #316 from LoveDaisy/fix/msvc-string-literal-limit\n\nfix(gui,ci): 拆开超 MSVC 上限的 shader 字面量 + 立静态门禁 + CI 触发去重",
          "timestamp": "2026-09-06T23:22:44+08:00",
          "tree_id": "6cd1e5d94251d724205eb0763696031080e86c9d",
          "url": "https://github.com/LoveDaisy/ice_halo_sim/commit/c04ad137d42e4a3ed51cd137b75d2d97092b7864"
        },
        "date": 1788708803885,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 357379.6,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 610991.9,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 440025.9,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 540653.7,
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
          "id": "349656ac94b6b2fd27ecbf4ed35812bea7646a2a",
          "message": "Merge pull request #317 from LoveDaisy/ci/windows-release-image-unify\n\nci: build Windows on the image we actually release from",
          "timestamp": "2026-09-07T01:04:20+08:00",
          "tree_id": "d7e945127aac502e4a16adeca4de66c2fca33ff5",
          "url": "https://github.com/LoveDaisy/ice_halo_sim/commit/349656ac94b6b2fd27ecbf4ed35812bea7646a2a"
        },
        "date": 1788714897840,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 442237.2,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 611721,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 438189.1,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 339521.3,
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
          "id": "59208e341a7a63e1f22366ec0fbc47211fd93950",
          "message": "Merge pull request #318 from LoveDaisy/test/e2e-cost-and-oracle-audit\n\ntest(e2e): 按「每个测试为自己的开销举证」审计套件成本，恢复预算余量",
          "timestamp": "2026-09-07T04:18:31+08:00",
          "tree_id": "a180e65114ec4dbebe9febd562ebcb9d7dcb6dcd",
          "url": "https://github.com/LoveDaisy/ice_halo_sim/commit/59208e341a7a63e1f22366ec0fbc47211fd93950"
        },
        "date": 1788726574327,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 454985.2,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 610617,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 508564.3,
            "unit": "rays/sec",
            "extra": "CPU: Intel(R) Xeon(R) Platinum 8370C CPU @ 2.80GHz\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 374070.1,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor                \\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "e707b15d31a6676c8f4147a0b0cfe62dfc452995",
          "message": "Merge pull request #319 from LoveDaisy/fix/gui-entry-delete-vs-open-editor\n\nfix(gui): keep the edit modal bound to its entry across a delete",
          "timestamp": "2026-09-08T11:14:03+08:00",
          "tree_id": "493abe69703c95a59432e4a6f6623947ade2f407",
          "url": "https://github.com/LoveDaisy/ice_halo_sim/commit/e707b15d31a6676c8f4147a0b0cfe62dfc452995"
        },
        "date": 1788837905475,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 366604,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 610382.8,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 393819.9,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 9V74 80-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 339860.6,
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
          "id": "da9e5533acc8c01c61877b6613ddf40bdce9a8b4",
          "message": "Merge pull request #320 from LoveDaisy/fix/cuda-zero-ray-batch-poisons-backend\n\nfix(cuda): stop a zero-ray layer from poisoning the CUDA backend",
          "timestamp": "2026-09-08T17:13:37+08:00",
          "tree_id": "915905aba2ebcbe1abe8eebe327b96458d905c22",
          "url": "https://github.com/LoveDaisy/ice_halo_sim/commit/da9e5533acc8c01c61877b6613ddf40bdce9a8b4"
        },
        "date": 1788859460604,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 431169.5,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 608896.9,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 436299.4,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 335202.4,
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
          "id": "d66985dfc19d7e0a2ad278bfb29e8adec87f3adc",
          "message": "Merge pull request #322 from LoveDaisy/test/random-source-exact-assertion-audit\n\ntest: audit random sources behind exact assertions, and refill the lost closed-form fuzz",
          "timestamp": "2026-09-08T19:05:43+08:00",
          "tree_id": "559a5d866b5f56b5751d9d1be56655c2412ea1a2",
          "url": "https://github.com/LoveDaisy/ice_halo_sim/commit/d66985dfc19d7e0a2ad278bfb29e8adec87f3adc"
        },
        "date": 1788866199114,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 380491.5,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 611147.2,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 433950.1,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 436283.4,
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
          "id": "f5d738bc07fade93832384583fd5655dda496ae4",
          "message": "Merge pull request #321 from LoveDaisy/ci/organization-and-windows-testing\n\nci(windows): route MSVC compilation through sccache",
          "timestamp": "2026-09-08T20:34:51+08:00",
          "tree_id": "8cbe8c8f0a06164c7e6f448e8e1b9b8f2b5aeff3",
          "url": "https://github.com/LoveDaisy/ice_halo_sim/commit/f5d738bc07fade93832384583fd5655dda496ae4"
        },
        "date": 1788871599160,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 361674,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 611795.7,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 425085.8,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 428972,
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
          "id": "5dab8ac5279acc30e74281d28ec9852f7091260f",
          "message": "Merge pull request #323 from LoveDaisy/feat/annotation-label-line-independence\n\nfeat(config): give the three grid families a line switch of their own",
          "timestamp": "2026-09-08T21:51:50+08:00",
          "tree_id": "17e102e9476ace17c070d7edc5d9863355c01883",
          "url": "https://github.com/LoveDaisy/ice_halo_sim/commit/5dab8ac5279acc30e74281d28ec9852f7091260f"
        },
        "date": 1788876183459,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 363514,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 610430.4,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 395620.5,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 9V74 80-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 340964.1,
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
          "id": "e06d6f8ee003f53159f4265e0a60478c3912298f",
          "message": "Merge pull request #324 from LoveDaisy/perf/cli-render-poll-floor\n\nperf(cli): poll completion before sleeping, so a render is not floored at 1s",
          "timestamp": "2026-09-08T22:52:18+08:00",
          "tree_id": "0e0c25fa9ce2a27fc0554ad989a39b2c64786891",
          "url": "https://github.com/LoveDaisy/ice_halo_sim/commit/e06d6f8ee003f53159f4265e0a60478c3912298f"
        },
        "date": 1788879778701,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 434610.5,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 609291.4,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 438619.2,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 361601,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor                \\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "dfb3f72cde1303813ed87b0403da0bf2d5264b86",
          "message": "Merge pull request #325 from LoveDaisy/fix/user-run-vs-backpressure-gate\n\nfix(gui): exempt a user-initiated Run from the commit backpressure gate",
          "timestamp": "2026-09-08T23:07:48+08:00",
          "tree_id": "3e55872359ee24f0c6224f4e627e5dd6964a1d7d",
          "url": "https://github.com/LoveDaisy/ice_halo_sim/commit/dfb3f72cde1303813ed87b0403da0bf2d5264b86"
        },
        "date": 1788880723618,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 393982.2,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 609608.5,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 435199.7,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 337894.2,
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
          "id": "61afdda60a81f3800fe9d39d0f1790efbed2eb82",
          "message": "Merge pull request #326 from LoveDaisy/ci/cuda-test-tu-compile-coverage\n\nci: compile the CUDA test TUs (close the CUDA×BUILD_TEST empty intersection)",
          "timestamp": "2026-09-09T09:01:05+08:00",
          "tree_id": "d8015a4a11ce1b395d085be4867bade1cd75f056",
          "url": "https://github.com/LoveDaisy/ice_halo_sim/commit/61afdda60a81f3800fe9d39d0f1790efbed2eb82"
        },
        "date": 1788916295931,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 398191.9,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 611456.6,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 439095.7,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 432172.6,
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
          "id": "5d04ed2b510cf8da1331d69d8a0ec9d064e8688a",
          "message": "Merge pull request #328 from LoveDaisy/feat/gui-import-capability-boundary\n\nfeat(gui): warn on intentionally unsupported capabilities when importing core/CLI configs",
          "timestamp": "2026-09-09T11:28:47+08:00",
          "tree_id": "5fb7bd048fbeeb0720b37f54dc406c24b4f696f7",
          "url": "https://github.com/LoveDaisy/ice_halo_sim/commit/5d04ed2b510cf8da1331d69d8a0ec9d064e8688a"
        },
        "date": 1788925157511,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 392728,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 610925.5,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 437873.1,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 362684.3,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor                \\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "fe4c0778ad67356b7107c49b9f2d1758751cbab0",
          "message": "Merge pull request #329 from LoveDaisy/fix/raypath-load-path-syntax-gate\n\nfix(gui): reject malformed raypath summand rows on the .lmc load path",
          "timestamp": "2026-09-09T11:50:12+08:00",
          "tree_id": "85ae4abe8324f97a506977a0b6b5e2f1ee1a6194",
          "url": "https://github.com/LoveDaisy/ice_halo_sim/commit/fe4c0778ad67356b7107c49b9f2d1758751cbab0"
        },
        "date": 1788926440664,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 456080.6,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 611254.2,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 436450.1,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 430115.9,
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
          "id": "61199dc71bb8afa824f182ecb361c1765212e2ba",
          "message": "Merge pull request #330 from LoveDaisy/build/cpm-cache-shared-default\n\nbuild(cpm): default the dependency-source cache to a machine-level directory",
          "timestamp": "2026-09-09T12:28:59+08:00",
          "tree_id": "401ce6afc69f30c5242ddcb0e9b1c84c265275ad",
          "url": "https://github.com/LoveDaisy/ice_halo_sim/commit/61199dc71bb8afa824f182ecb361c1765212e2ba"
        },
        "date": 1788928782668,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 372360.3,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 611889.9,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 437338.4,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 434947.7,
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
          "id": "52b769f8831d3826607f24929ab7130e9dc62d1e",
          "message": "Merge pull request #331 from LoveDaisy/refactor/field-set-sentinel-proxy\n\nrefactor(config): guard RenderConfig's field set by member count, not sizeof",
          "timestamp": "2026-09-09T13:13:40+08:00",
          "tree_id": "ea00f972f5073c2d5a34b75fbe200d19d12f07b6",
          "url": "https://github.com/LoveDaisy/ice_halo_sim/commit/52b769f8831d3826607f24929ab7130e9dc62d1e"
        },
        "date": 1788931507521,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 457275.6,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 610916,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 439743.4,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 377284,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor                \\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "96d644a21248a0968f4866679a3de372c1610833",
          "message": "Merge pull request #334 from LoveDaisy/feat/bg-image-color-picker\n\nfeat(gui): sample Sky Color off the background photo with an eyedropper",
          "timestamp": "2026-09-10T01:16:27+08:00",
          "tree_id": "dacea434bc3720c060ce99ea1e1f7083321478c0",
          "url": "https://github.com/LoveDaisy/ice_halo_sim/commit/96d644a21248a0968f4866679a3de372c1610833"
        },
        "date": 1788974899057,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 409623.3,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 610248.4,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 428039.1,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 439436.3,
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
          "id": "258d9d34fde41b6c3c29d2818e91a4a20c13f2af",
          "message": "Merge pull request #335 from LoveDaisy/ci/drop-unused-vendor-apt-source\n\nci: stop depending on a vendor apt source nothing here installs from",
          "timestamp": "2026-09-10T02:19:42+08:00",
          "tree_id": "0ec8ef1453363cf2a4e9bac9403be312d6a3d461",
          "url": "https://github.com/LoveDaisy/ice_halo_sim/commit/258d9d34fde41b6c3c29d2818e91a4a20c13f2af"
        },
        "date": 1788978629071,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 360916.5,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 611670.6,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 439932.3,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 364142.1,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor                \\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "712eb886076cecd28b4dedc683f8255351558cb5",
          "message": "Merge pull request #333 from LoveDaisy/ci/cache-budget\n\nci(cache): budget the actions/cache quota — fix three prefix-shadowed keys, add ccache to the critical-path leg",
          "timestamp": "2026-09-10T02:35:58+08:00",
          "tree_id": "c53075d893ac829084799f406bdd8c280a195292",
          "url": "https://github.com/LoveDaisy/ice_halo_sim/commit/712eb886076cecd28b4dedc683f8255351558cb5"
        },
        "date": 1788979613946,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 473801.9,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 611061.6,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 483844.3,
            "unit": "rays/sec",
            "extra": "CPU: INTEL(R) XEON(R) PLATINUM 8573C\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 340987,
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
          "id": "491b117b9a07cdf85de8099529f7811e686abf1e",
          "message": "Merge pull request #336 from LoveDaisy/feat/miller-index-and-wedge-presets\n\nfix(gui,core): give the Miller-index wedge conversion one owner, and correct the presets it was never checked against",
          "timestamp": "2026-09-10T04:26:14+08:00",
          "tree_id": "4e290141061128a452482994544759c4c4475a08",
          "url": "https://github.com/LoveDaisy/ice_halo_sim/commit/491b117b9a07cdf85de8099529f7811e686abf1e"
        },
        "date": 1788986309576,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 413207.2,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 609913.3,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 421022.6,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 369755,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor                \\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "7a4c526050e74287deacd4473631926deabf13a9",
          "message": "Merge pull request #337 from LoveDaisy/feat/print-mode-subtractive-ink\n\nfeat(render,gui): add a print tone that lays ink on paper instead of adding light to sky",
          "timestamp": "2026-09-10T09:06:10+08:00",
          "tree_id": "7d1fa0a3596ea0279c8d776fa7d4f0baaa8feab6",
          "url": "https://github.com/LoveDaisy/ice_halo_sim/commit/7a4c526050e74287deacd4473631926deabf13a9"
        },
        "date": 1789003109399,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 364368.5,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 610311,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 433708.1,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 366244,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor                \\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "ae46f7c283910d4fbb4e7a0f2949aef02e5f87df",
          "message": "Merge pull request #338 from LoveDaisy/feat/gui-display-rendering-regroup\n\nfix(gui): regroup the Display Rendering rows and pair the ground swatch with the mode",
          "timestamp": "2026-09-10T14:08:18+08:00",
          "tree_id": "eeaec146eaa0226974bb83e95e4e1b36f34970a9",
          "url": "https://github.com/LoveDaisy/ice_halo_sim/commit/ae46f7c283910d4fbb4e7a0f2949aef02e5f87df"
        },
        "date": 1789021100736,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 362919.2,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 611994.1,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 433174,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 368764.6,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor                \\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "3ad57411dc16f516b6785967efaba5266c88e7b8",
          "message": "Merge pull request #339 from LoveDaisy/feat/test-capi-lib\n\ntest: liblumice_testapi, a test-only export surface beside the product C API",
          "timestamp": "2026-09-10T16:59:47+08:00",
          "tree_id": "91bfa648700adc1c02137e2bab552ca4271f3417",
          "url": "https://github.com/LoveDaisy/ice_halo_sim/commit/3ad57411dc16f516b6785967efaba5266c88e7b8"
        },
        "date": 1789031371463,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 333294.7,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 611939.9,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 426046.3,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 373294.8,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor                \\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "724fa7cff5ff0cc832f33d45b08e1bf3d4536f40",
          "message": "Merge pull request #342 from LoveDaisy/feat/annotation-lines-shader-anchors-api\n\ngui: auxiliary lines track the camera every frame again; anchors-only annotation API (v4.28)",
          "timestamp": "2026-09-10T17:18:09+08:00",
          "tree_id": "7f9537581734b6e612b1d500271e44df92102186",
          "url": "https://github.com/LoveDaisy/ice_halo_sim/commit/724fa7cff5ff0cc832f33d45b08e1bf3d4536f40"
        },
        "date": 1789032507905,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 285739.7,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 609018.5,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 391682.2,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 9V74 80-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 447223.6,
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
          "id": "d57f132dc2f546ad001a215fb28d6c917bddabf4",
          "message": "Merge pull request #340 from LoveDaisy/docs/working-discipline-hardening\n\ndocs+hooks: harden two working-discipline rules into criteria and a commit gate",
          "timestamp": "2026-09-10T18:06:39+08:00",
          "tree_id": "1e03b6ab5fd5688bd565295581883fc4c9690d85",
          "url": "https://github.com/LoveDaisy/ice_halo_sim/commit/d57f132dc2f546ad001a215fb28d6c917bddabf4"
        },
        "date": 1789035351539,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 344873.1,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 605823.8,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 434453.3,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 348984.7,
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
          "id": "37b141504a40cc0be937f0d5bf071fef24759171",
          "message": "Merge pull request #341 from LoveDaisy/test/defaults-panel-refs-reshoot\n\ntest(gui): pin the wedge add row in every preset scene, and re-shoot the two that were not",
          "timestamp": "2026-09-10T18:49:54+08:00",
          "tree_id": "42fe951d730ad0cd31b0d30b5c6fc14fa0ec2dc1",
          "url": "https://github.com/LoveDaisy/ice_halo_sim/commit/37b141504a40cc0be937f0d5bf071fef24759171"
        },
        "date": 1789038025783,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 385061.6,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 607319.5,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 436205.8,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 449794,
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
          "id": "f860acc46471dee85057842a8611895cea64b88d",
          "message": "Merge pull request #343 from LoveDaisy/feat/gui-print-mode-label-ink\n\nfix(gui): draw overlay label text as ink under the print tone",
          "timestamp": "2026-09-10T20:57:53+08:00",
          "tree_id": "f3daf403c69c5adec882772328d1a77ec96a9215",
          "url": "https://github.com/LoveDaisy/ice_halo_sim/commit/f860acc46471dee85057842a8611895cea64b88d"
        },
        "date": 1789045689806,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 361400.6,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 607430.3,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 499679.9,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 9V74 80-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 380774.8,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor                \\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "dc76b64939e2b7e7bb319dee15905da1c73ee7fa",
          "message": "Merge pull request #344 from LoveDaisy/feat/image-comparison-metric-by-layer\n\ntest: give each image comparison a ruler that matches its layer (pixel ruler, lines-only parity, block-mean PSNR)",
          "timestamp": "2026-09-11T01:37:19+08:00",
          "tree_id": "41b16f85c1532f746a24d73f00d8388af7f2060a",
          "url": "https://github.com/LoveDaisy/ice_halo_sim/commit/dc76b64939e2b7e7bb319dee15905da1c73ee7fa"
        },
        "date": 1789062625915,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 346766.6,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 608036.5,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 437509.2,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 401895.2,
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
          "id": "38aff9c6a97f3fdcca9801ffeb6e1dcecf4be998",
          "message": "Merge pull request #345 from LoveDaisy/chore/release-4.5.1\n\nrelease: cut 4.5.1, and make the release a per-version backfill chore",
          "timestamp": "2026-09-11T08:06:39+08:00",
          "tree_id": "c20bb077289e78ac31076d801efb94c50cab02ad",
          "url": "https://github.com/LoveDaisy/ice_halo_sim/commit/38aff9c6a97f3fdcca9801ffeb6e1dcecf4be998"
        },
        "date": 1789085815841,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 307557.1,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 606764.1,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 434462.9,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 448238.3,
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
          "id": "142e29e615d7a573006eabf610b33948e5584c98",
          "message": "Merge pull request #346 from LoveDaisy/feat/hardware-perf-distribution\n\nbuild/release: ship ISA- and GPU-matched binaries behind CPUID launchers (x86-64-v4 Linux, x86-64-v3 clang-cl Windows, sm_120 fatbin)",
          "timestamp": "2026-09-11T20:51:32+08:00",
          "tree_id": "f05bd3485353b55d626d7d9fe93091774693bb97",
          "url": "https://github.com/LoveDaisy/ice_halo_sim/commit/142e29e615d7a573006eabf610b33948e5584c98"
        },
        "date": 1789131609120,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 352688.7,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 605073.5,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 389515.3,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 9V74 80-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 370417.8,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor                \\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "4fdfe61cc4b1326f60a924a669133f7c6311023d",
          "message": "Merge pull request #347 from LoveDaisy/feat/raypath-analysis-panel\n\nfeat: raypath analysis panel — dedicated non-rendering pass, ranked by chain",
          "timestamp": "2026-09-12T16:27:12+08:00",
          "tree_id": "071aa48f973504cccddec5a5f2199596e22adfc9",
          "url": "https://github.com/LoveDaisy/ice_halo_sim/commit/4fdfe61cc4b1326f60a924a669133f7c6311023d"
        },
        "date": 1789202418498,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 454449.6,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 597641.8,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 383992.6,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 9V74 80-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 370443.8,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor                \\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "70eb8f5fad44336cf2b57da4d314a9aced4c8224",
          "message": "Merge pull request #349 from LoveDaisy/feat/raypath-analysis-followups\n\nRaypath analysis follow-ups: fixed-seed reproducibility, session-kind rebuild predicate, joiner glyphs, debt sweep",
          "timestamp": "2026-09-12T23:46:53+08:00",
          "tree_id": "1570515bb91fdd1a4610a3ac00dc77a6ae1f7cca",
          "url": "https://github.com/LoveDaisy/ice_halo_sim/commit/70eb8f5fad44336cf2b57da4d314a9aced4c8224"
        },
        "date": 1789228782888,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 427219.5,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 593937.2,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 637628.9,
            "unit": "rays/sec",
            "extra": "CPU: Intel(R) Xeon(R) 6973P-C\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 376566.3,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor                \\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "c106292be4122b60092a9666131dde843f639b15",
          "message": "Merge pull request #348 from LoveDaisy/feat/crystal-ray-allocation\n\nfeat(core): adaptive ray allocation across crystal entries (scene.ray_allocation)",
          "timestamp": "2026-09-13T04:19:28+08:00",
          "tree_id": "82448d3731df653e760db492e5a8794a13dc6f0c",
          "url": "https://github.com/LoveDaisy/ice_halo_sim/commit/c106292be4122b60092a9666131dde843f639b15"
        },
        "date": 1789245352047,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 438166.5,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 595074.3,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 431818.5,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 337191.8,
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
          "id": "88e0fbf6864b1d95ba7e19c4d6660fb8c15c1f4c",
          "message": "Merge pull request #350 from LoveDaisy/chore/install-manual-refresh-and-review-minors\n\nchore: refresh the install manual, land the metric-by-layer review minors, report wrong-size anchor planes once",
          "timestamp": "2026-09-13T04:52:15+08:00",
          "tree_id": "94bf3da366cd365b6946ce53b5309cd6df95c36e",
          "url": "https://github.com/LoveDaisy/ice_halo_sim/commit/88e0fbf6864b1d95ba7e19c4d6660fb8c15c1f4c"
        },
        "date": 1789247019730,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 460521.5,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 592155.7,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 422287.1,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 338599.7,
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
          "id": "d5c230764f43779ffb32bc75e454ec07f2159a03",
          "message": "Merge pull request #351 from LoveDaisy/fix/exposure-mode-combo-fixed-separation\n\ntest(gui): prove exposure-mode separation with an intensity probe, not a seed-dependent gap",
          "timestamp": "2026-09-13T05:13:06+08:00",
          "tree_id": "8da3f1ccd7d22ef20d6fff6dff9a5c615f8e013d",
          "url": "https://github.com/LoveDaisy/ice_halo_sim/commit/d5c230764f43779ffb32bc75e454ec07f2159a03"
        },
        "date": 1789248083352,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 474199.5,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 592907.9,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 384547.7,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 9V74 80-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 550630,
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
          "id": "7a68880e398137ef91895edbb6d4ff0c997923e4",
          "message": "Merge pull request #352 from LoveDaisy/chore/regen-refs-deterministic-single-shot\n\nchore(regen-refs): shoot deterministic groups once, share runs across groups, refuse stale-base reshoots",
          "timestamp": "2026-09-13T05:31:09+08:00",
          "tree_id": "5e9ceaec07873c2b9154175e6d957784999b8820",
          "url": "https://github.com/LoveDaisy/ice_halo_sim/commit/7a68880e398137ef91895edbb6d4ff0c997923e4"
        },
        "date": 1789249338363,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 389497.6,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 592514.4,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 425177.3,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 372608.7,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor                \\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "365409776ad9761a5ebf3402cf9cde48f573e9d8",
          "message": "Merge pull request #353 from LoveDaisy/fix/render-consumer-label-flake-root-cause\n\nfix(test): root-cause the RenderConsumerLabel flake — an uninitialized SunParam azimuth",
          "timestamp": "2026-09-13T05:47:24+08:00",
          "tree_id": "2a620f7705b88d7f56d29a1cf923e190e358fd9e",
          "url": "https://github.com/LoveDaisy/ice_halo_sim/commit/365409776ad9761a5ebf3402cf9cde48f573e9d8"
        },
        "date": 1789250195815,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 473770.6,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 594577.8,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 468123.6,
            "unit": "rays/sec",
            "extra": "CPU: INTEL(R) XEON(R) PLATINUM 8573C\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 436391.2,
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
          "id": "35235a01c6962917a905abe105b458ee0ba444ab",
          "message": "Merge pull request #354 from LoveDaisy/feat/ray-num-slider-100b-log-scale\n\nfeat(gui): Rays(M) slider spans 0.1..100 000 M on a kLog track, one domain for both rows",
          "timestamp": "2026-09-13T06:17:09+08:00",
          "tree_id": "8c31257305277f134a3473abfb64eca3bdbdbc3a",
          "url": "https://github.com/LoveDaisy/ice_halo_sim/commit/35235a01c6962917a905abe105b458ee0ba444ab"
        },
        "date": 1789252142656,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 359420.6,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 593463,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 432483.7,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 571161.9,
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
          "id": "f0add0b4a676a2e20ab27c782e9e9b5a182aac5b",
          "message": "Merge pull request #355 from LoveDaisy/feat/cli-lens-and-grid-contract\n\nfeat(lens): the CLI/GUI lens contract — short-edge fov, defaults, focal length import, annotations at intensity 0",
          "timestamp": "2026-09-13T07:31:34+08:00",
          "tree_id": "14ed237826bf2e9217fc60dc8c0bd508aee5669a",
          "url": "https://github.com/LoveDaisy/ice_halo_sim/commit/f0add0b4a676a2e20ab27c782e9e9b5a182aac5b"
        },
        "date": 1789256490985,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 404296.1,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 594911.1,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 501402.1,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 9V74 80-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 378549.8,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor                \\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "51fa59e29850545fd09b7f6041faaed4a4bea4cd",
          "message": "Merge pull request #356 from LoveDaisy/feat/cuda-hostgen-black-and-energy-accounting\n\nfix(cuda): host root-gen fallback renders again; landed weight reduced per warp so the energy ledger matches legacy",
          "timestamp": "2026-09-13T08:09:28+08:00",
          "tree_id": "5310d81311d369114cdde3eb97a22ec84b3b27c2",
          "url": "https://github.com/LoveDaisy/ice_halo_sim/commit/51fa59e29850545fd09b7f6041faaed4a4bea4cd"
        },
        "date": 1789258760432,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 444666.8,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 594342.9,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 428027.6,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 443613.7,
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
          "id": "22166140295c68e58e6375028394d4587a351c11",
          "message": "Merge pull request #357 from LoveDaisy/feat/view-center-angular-dist-grid\n\nfeat(annotation): view_dist — circles of constant angular distance from the optical axis, config → core → C API → GUI",
          "timestamp": "2026-09-13T12:02:01+08:00",
          "tree_id": "6875aee1957344381ca66a902bb0dc2ba20f8f02",
          "url": "https://github.com/LoveDaisy/ice_halo_sim/commit/22166140295c68e58e6375028394d4587a351c11"
        },
        "date": 1789273859093,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 495013.5,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 593779,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 423887.8,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 576401,
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
          "id": "682bf9aadbd77a61c2d7697ccd4b353bb70a06be",
          "message": "Merge pull request #358 from LoveDaisy/fix/equidistant-focal-length-factor-two\n\nfix(config): equidistant lens f→fov conversion was half the documented value",
          "timestamp": "2026-09-13T12:42:20+08:00",
          "tree_id": "ad66fbc16fa5d96511d2e7667a3348c6c69b86d5",
          "url": "https://github.com/LoveDaisy/ice_halo_sim/commit/682bf9aadbd77a61c2d7697ccd4b353bb70a06be"
        },
        "date": 1789275324609,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 337608.6,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 590600.4,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 424936.9,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 378988.5,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor                \\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "b10d1bbc134afb64423fa842b7740de5a59934f2",
          "message": "Merge pull request #359 from LoveDaisy/feat/analysis-panel-polish\n\nfeat(gui): raypath analysis panel polish — first-picture gate, draw layer, geometry, thousands grouping, Export CSV",
          "timestamp": "2026-09-13T14:55:21+08:00",
          "tree_id": "ffbfcccfae868fd3b9504d3d98acd998d41a1933",
          "url": "https://github.com/LoveDaisy/ice_halo_sim/commit/b10d1bbc134afb64423fa842b7740de5a59934f2"
        },
        "date": 1789283174402,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 466913.7,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 592792.1,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 494210.1,
            "unit": "rays/sec",
            "extra": "CPU: Intel(R) Xeon(R) Platinum 8370C CPU @ 2.80GHz\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 345133.9,
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
          "id": "90d23cafd492e0f65663df5a681c439b0fa09f35",
          "message": "Merge pull request #360 from LoveDaisy/feat/analysis-standing-cpu-pool\n\nfeat(server): standing CPU analysis pool on the GPU route, woken by session kind",
          "timestamp": "2026-09-13T15:15:21+08:00",
          "tree_id": "0e81a696990d2128a156faf657717d6b2838d130",
          "url": "https://github.com/LoveDaisy/ice_halo_sim/commit/90d23cafd492e0f65663df5a681c439b0fa09f35"
        },
        "date": 1789284487911,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 455987,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 594636.6,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 498534.4,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 9V74 80-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 379542.6,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor                \\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "0eba76808ec4e34d0e76e5c741347a0779db27c2",
          "message": "Merge pull request #361 from LoveDaisy/feat/panel-state-round-trip\n\nfeat(gui): panel-derived state round trip — analysis list freshness predicate, colour-ref layer re-indexing",
          "timestamp": "2026-09-13T15:37:56+08:00",
          "tree_id": "e7548f5a60a46f37b724f835b81f368a1b59d85a",
          "url": "https://github.com/LoveDaisy/ice_halo_sim/commit/0eba76808ec4e34d0e76e5c741347a0779db27c2"
        },
        "date": 1789285824168,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 369745.6,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 593579.4,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 426670.2,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 379213.1,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor                \\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "673e55308cc451c53b25f590d5d48bf83f9ecf17",
          "message": "Merge pull request #362 from LoveDaisy/feat/angular-distance-section-merge\n\nfeat(gui): merge both angular-distance ring families into one collapsed Angular Distance section",
          "timestamp": "2026-09-13T16:21:39+08:00",
          "tree_id": "5b13f138562a66f363f9e17a9a4111ed6049f9d4",
          "url": "https://github.com/LoveDaisy/ice_halo_sim/commit/673e55308cc451c53b25f590d5d48bf83f9ecf17"
        },
        "date": 1789288413671,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 346877.7,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 593964.6,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 410176.4,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 375460.2,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor                \\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "1a6c43be3f9fa9d8d89a3115d1fdf2f994e3a984",
          "message": "Merge pull request #363 from LoveDaisy/chore/angular-distance-from-naming\n\nfeat(gui): name the Angular Distance section \"from...\" and its rows Sun / Lens Center",
          "timestamp": "2026-09-13T20:15:08+08:00",
          "tree_id": "042b5e14f5fbae521e6d1e53edc4d0889e44bcf5",
          "url": "https://github.com/LoveDaisy/ice_halo_sim/commit/1a6c43be3f9fa9d8d89a3115d1fdf2f994e3a984"
        },
        "date": 1789302337481,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 393873.7,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 590875.4,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 494099.3,
            "unit": "rays/sec",
            "extra": "CPU: Intel(R) Xeon(R) Platinum 8370C CPU @ 2.80GHz\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 377487.5,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor                \\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "6a6ae79565932a61955ad281237b6445b8a5d48f",
          "message": "Merge pull request #364 from LoveDaisy/chore/hide-ray-allocation-checkbox\n\nchore(gui): hide Adaptive ray allocation checkbox from the main panel",
          "timestamp": "2026-09-13T22:02:17+08:00",
          "tree_id": "53eedb96fe96c777dbf6f5fd7d8737f2e45e20f6",
          "url": "https://github.com/LoveDaisy/ice_halo_sim/commit/6a6ae79565932a61955ad281237b6445b8a5d48f"
        },
        "date": 1789308811645,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 364112,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 593775.1,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 541896.8,
            "unit": "rays/sec",
            "extra": "CPU: INTEL(R) XEON(R) PLATINUM 8573C\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 345735.7,
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
          "id": "19741f43a26b205cd9953af019cae4b44fd12984",
          "message": "Merge pull request #365 from LoveDaisy/feat/overlay-panel-ux\n\nfeat(gui): Overlay panel UX — Lens Center circle defaults and a Reference Points All row",
          "timestamp": "2026-09-14T00:15:21+08:00",
          "tree_id": "d43c7c2948629dec68903aa2f4de6ded9dcafbae",
          "url": "https://github.com/LoveDaisy/ice_halo_sim/commit/19741f43a26b205cd9953af019cae4b44fd12984"
        },
        "date": 1789316823021,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 360973,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 591105.8,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 420399.3,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 345762.9,
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
          "id": "c4288079e3f5670454e7cc8788194498a3f584f2",
          "message": "Merge pull request #366 from LoveDaisy/feat/analysis-session-ray-allocation\n\nfeat(analysis): analysis sessions follow scene.ray_allocation; drop the Rays column",
          "timestamp": "2026-09-14T00:38:07+08:00",
          "tree_id": "d76d462fb476b2cff0e3581ec03afa0c622be7a6",
          "url": "https://github.com/LoveDaisy/ice_halo_sim/commit/c4288079e3f5670454e7cc8788194498a3f584f2"
        },
        "date": 1789318078250,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 423638.5,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 592846,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 546286.1,
            "unit": "rays/sec",
            "extra": "CPU: INTEL(R) XEON(R) PLATINUM 8573C\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 379127.3,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor                \\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "fd1bb1cffe3edfbbf7c4de2a1eff5ff0ecb5ea1a",
          "message": "Merge pull request #367 from LoveDaisy/chore/release-4.6.0\n\nchore(release): cut 4.6.0",
          "timestamp": "2026-09-14T01:21:05+08:00",
          "tree_id": "bd318927bfaf54e9b78b5aacd9ba6b7628b02941",
          "url": "https://github.com/LoveDaisy/ice_halo_sim/commit/fd1bb1cffe3edfbbf7c4de2a1eff5ff0ecb5ea1a"
        },
        "date": 1789320637996,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 391374.4,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 593614.5,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 427748.6,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 422431.8,
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
          "id": "7a9430ef19ad7197b2befd279263c2e4e3474f45",
          "message": "Merge pull request #368 from LoveDaisy/chore/cli-subcommands\n\ncli: split the flat flag set into render / benchmark subcommands",
          "timestamp": "2026-09-14T18:29:48+08:00",
          "tree_id": "5979324f4b5810dffb657260e392b8fab205f97d",
          "url": "https://github.com/LoveDaisy/ice_halo_sim/commit/7a9430ef19ad7197b2befd279263c2e4e3474f45"
        },
        "date": 1789382388338,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 324246.3,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 591153.5,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 448285.8,
            "unit": "rays/sec",
            "extra": "CPU: INTEL(R) XEON(R) PLATINUM 8573C\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 378579.6,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor                \\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "9232bccb77567b58b5b9342c1049e69337b2fb44",
          "message": "Merge pull request #369 from LoveDaisy/feat/cli-raypath-analyze\n\ncli: add the `analyze` subcommand — raypath analysis from the command line",
          "timestamp": "2026-09-14T20:18:27+08:00",
          "tree_id": "aa2b67b0a92fe71b387a76367f305cdf54930982",
          "url": "https://github.com/LoveDaisy/ice_halo_sim/commit/9232bccb77567b58b5b9342c1049e69337b2fb44"
        },
        "date": 1789389132607,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 371699.1,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 595110.7,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 424310.9,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 378236.6,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor                \\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "f220e71f72ceab196f527085937d1268369d7e63",
          "message": "Merge pull request #370 from LoveDaisy/chore/cli-render-seed-and-small-fixes\n\nchore: render --seed, double emitted-energy accumulators, manual fixes, GUI log sink to stderr",
          "timestamp": "2026-09-16T08:42:53+08:00",
          "tree_id": "9ad81ba8b1b719f173f4198a5be2b649c860cde2",
          "url": "https://github.com/LoveDaisy/Lumice/commit/f220e71f72ceab196f527085937d1268369d7e63"
        },
        "date": 1789520226922,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 371676.5,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 592174.7,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 428606.8,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 370923.5,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor                \\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
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
        "date": 1789521261967,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 470443.3,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 592620.3,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 432851.5,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 372388.8,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor                \\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
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
          "id": "5cb81a9565ff0e1394a957d97ad2078d0d8f9310",
          "message": "Merge pull request #315 from LoveDaisy/scrum/changelog-backfill-and-release-notes\n\ndocs(release): 回填 v4.1.4 起 31 个版本的 CHANGELOG，并把它接进发版链路",
          "timestamp": "2026-09-06T16:24:48+08:00",
          "tree_id": "079d5a016ac9ac51339233ba0779369c19e64745",
          "url": "https://github.com/LoveDaisy/ice_halo_sim/commit/5cb81a9565ff0e1394a957d97ad2078d0d8f9310"
        },
        "date": 1788683897234,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 837040.1,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1217120.4,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 854169.1,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 686534.5,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor                \\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "c04ad137d42e4a3ed51cd137b75d2d97092b7864",
          "message": "Merge pull request #316 from LoveDaisy/fix/msvc-string-literal-limit\n\nfix(gui,ci): 拆开超 MSVC 上限的 shader 字面量 + 立静态门禁 + CI 触发去重",
          "timestamp": "2026-09-06T23:22:44+08:00",
          "tree_id": "6cd1e5d94251d724205eb0763696031080e86c9d",
          "url": "https://github.com/LoveDaisy/ice_halo_sim/commit/c04ad137d42e4a3ed51cd137b75d2d97092b7864"
        },
        "date": 1788708808149,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 835865.8,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1218279.2,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 829130,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 989983.4,
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
          "id": "349656ac94b6b2fd27ecbf4ed35812bea7646a2a",
          "message": "Merge pull request #317 from LoveDaisy/ci/windows-release-image-unify\n\nci: build Windows on the image we actually release from",
          "timestamp": "2026-09-07T01:04:20+08:00",
          "tree_id": "d7e945127aac502e4a16adeca4de66c2fca33ff5",
          "url": "https://github.com/LoveDaisy/ice_halo_sim/commit/349656ac94b6b2fd27ecbf4ed35812bea7646a2a"
        },
        "date": 1788714901687,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 1146045.3,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1217253.7,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 829440.5,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 629547.5,
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
          "id": "59208e341a7a63e1f22366ec0fbc47211fd93950",
          "message": "Merge pull request #318 from LoveDaisy/test/e2e-cost-and-oracle-audit\n\ntest(e2e): 按「每个测试为自己的开销举证」审计套件成本，恢复预算余量",
          "timestamp": "2026-09-07T04:18:31+08:00",
          "tree_id": "a180e65114ec4dbebe9febd562ebcb9d7dcb6dcd",
          "url": "https://github.com/LoveDaisy/ice_halo_sim/commit/59208e341a7a63e1f22366ec0fbc47211fd93950"
        },
        "date": 1788726578238,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 1096881.1,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1214833.6,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 902823.8,
            "unit": "rays/sec",
            "extra": "CPU: Intel(R) Xeon(R) Platinum 8370C CPU @ 2.80GHz\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 673985,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor                \\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "e707b15d31a6676c8f4147a0b0cfe62dfc452995",
          "message": "Merge pull request #319 from LoveDaisy/fix/gui-entry-delete-vs-open-editor\n\nfix(gui): keep the edit modal bound to its entry across a delete",
          "timestamp": "2026-09-08T11:14:03+08:00",
          "tree_id": "493abe69703c95a59432e4a6f6623947ade2f407",
          "url": "https://github.com/LoveDaisy/ice_halo_sim/commit/e707b15d31a6676c8f4147a0b0cfe62dfc452995"
        },
        "date": 1788837910236,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 1168034,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1219054.3,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 762441.6,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 9V74 80-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 627060.3,
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
          "id": "da9e5533acc8c01c61877b6613ddf40bdce9a8b4",
          "message": "Merge pull request #320 from LoveDaisy/fix/cuda-zero-ray-batch-poisons-backend\n\nfix(cuda): stop a zero-ray layer from poisoning the CUDA backend",
          "timestamp": "2026-09-08T17:13:37+08:00",
          "tree_id": "915905aba2ebcbe1abe8eebe327b96458d905c22",
          "url": "https://github.com/LoveDaisy/ice_halo_sim/commit/da9e5533acc8c01c61877b6613ddf40bdce9a8b4"
        },
        "date": 1788859466565,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 954973.7,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1217338.4,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 818302.1,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 625175,
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
          "id": "d66985dfc19d7e0a2ad278bfb29e8adec87f3adc",
          "message": "Merge pull request #322 from LoveDaisy/test/random-source-exact-assertion-audit\n\ntest: audit random sources behind exact assertions, and refill the lost closed-form fuzz",
          "timestamp": "2026-09-08T19:05:43+08:00",
          "tree_id": "559a5d866b5f56b5751d9d1be56655c2412ea1a2",
          "url": "https://github.com/LoveDaisy/ice_halo_sim/commit/d66985dfc19d7e0a2ad278bfb29e8adec87f3adc"
        },
        "date": 1788866202924,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 871865.5,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1219755.4,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 820621,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 806711.2,
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
          "id": "f5d738bc07fade93832384583fd5655dda496ae4",
          "message": "Merge pull request #321 from LoveDaisy/ci/organization-and-windows-testing\n\nci(windows): route MSVC compilation through sccache",
          "timestamp": "2026-09-08T20:34:51+08:00",
          "tree_id": "8cbe8c8f0a06164c7e6f448e8e1b9b8f2b5aeff3",
          "url": "https://github.com/LoveDaisy/ice_halo_sim/commit/f5d738bc07fade93832384583fd5655dda496ae4"
        },
        "date": 1788871602788,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 853838.7,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1217164.2,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 821670.4,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 702479.9,
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
          "id": "5dab8ac5279acc30e74281d28ec9852f7091260f",
          "message": "Merge pull request #323 from LoveDaisy/feat/annotation-label-line-independence\n\nfeat(config): give the three grid families a line switch of their own",
          "timestamp": "2026-09-08T21:51:50+08:00",
          "tree_id": "17e102e9476ace17c070d7edc5d9863355c01883",
          "url": "https://github.com/LoveDaisy/ice_halo_sim/commit/5dab8ac5279acc30e74281d28ec9852f7091260f"
        },
        "date": 1788876188885,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 933334.3,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1211581.3,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 759839.1,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 9V74 80-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 626731.9,
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
          "id": "e06d6f8ee003f53159f4265e0a60478c3912298f",
          "message": "Merge pull request #324 from LoveDaisy/perf/cli-render-poll-floor\n\nperf(cli): poll completion before sleeping, so a render is not floored at 1s",
          "timestamp": "2026-09-08T22:52:18+08:00",
          "tree_id": "0e0c25fa9ce2a27fc0554ad989a39b2c64786891",
          "url": "https://github.com/LoveDaisy/ice_halo_sim/commit/e06d6f8ee003f53159f4265e0a60478c3912298f"
        },
        "date": 1788879785389,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 1048533.7,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1216159.6,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 823560.9,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 667733.3,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor                \\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "dfb3f72cde1303813ed87b0403da0bf2d5264b86",
          "message": "Merge pull request #325 from LoveDaisy/fix/user-run-vs-backpressure-gate\n\nfix(gui): exempt a user-initiated Run from the commit backpressure gate",
          "timestamp": "2026-09-08T23:07:48+08:00",
          "tree_id": "3e55872359ee24f0c6224f4e627e5dd6964a1d7d",
          "url": "https://github.com/LoveDaisy/ice_halo_sim/commit/dfb3f72cde1303813ed87b0403da0bf2d5264b86"
        },
        "date": 1788880729931,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 968328.2,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1214975.3,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 826837.5,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 626662.7,
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
          "id": "61afdda60a81f3800fe9d39d0f1790efbed2eb82",
          "message": "Merge pull request #326 from LoveDaisy/ci/cuda-test-tu-compile-coverage\n\nci: compile the CUDA test TUs (close the CUDA×BUILD_TEST empty intersection)",
          "timestamp": "2026-09-09T09:01:05+08:00",
          "tree_id": "d8015a4a11ce1b395d085be4867bade1cd75f056",
          "url": "https://github.com/LoveDaisy/ice_halo_sim/commit/61afdda60a81f3800fe9d39d0f1790efbed2eb82"
        },
        "date": 1788916300005,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 1000537.7,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1221757.8,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 820672.5,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 809410.3,
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
          "id": "5d04ed2b510cf8da1331d69d8a0ec9d064e8688a",
          "message": "Merge pull request #328 from LoveDaisy/feat/gui-import-capability-boundary\n\nfeat(gui): warn on intentionally unsupported capabilities when importing core/CLI configs",
          "timestamp": "2026-09-09T11:28:47+08:00",
          "tree_id": "5fb7bd048fbeeb0720b37f54dc406c24b4f696f7",
          "url": "https://github.com/LoveDaisy/ice_halo_sim/commit/5d04ed2b510cf8da1331d69d8a0ec9d064e8688a"
        },
        "date": 1788925161823,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 929318.6,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1216021.6,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 822254.3,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 667280.4,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor                \\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "fe4c0778ad67356b7107c49b9f2d1758751cbab0",
          "message": "Merge pull request #329 from LoveDaisy/fix/raypath-load-path-syntax-gate\n\nfix(gui): reject malformed raypath summand rows on the .lmc load path",
          "timestamp": "2026-09-09T11:50:12+08:00",
          "tree_id": "85ae4abe8324f97a506977a0b6b5e2f1ee1a6194",
          "url": "https://github.com/LoveDaisy/ice_halo_sim/commit/fe4c0778ad67356b7107c49b9f2d1758751cbab0"
        },
        "date": 1788926445499,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 925137.3,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1220731.7,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 827201.9,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 699231.4,
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
          "id": "61199dc71bb8afa824f182ecb361c1765212e2ba",
          "message": "Merge pull request #330 from LoveDaisy/build/cpm-cache-shared-default\n\nbuild(cpm): default the dependency-source cache to a machine-level directory",
          "timestamp": "2026-09-09T12:28:59+08:00",
          "tree_id": "401ce6afc69f30c5242ddcb0e9b1c84c265275ad",
          "url": "https://github.com/LoveDaisy/ice_halo_sim/commit/61199dc71bb8afa824f182ecb361c1765212e2ba"
        },
        "date": 1788928787906,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 847754.4,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1219430.6,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 826846.1,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 806459.1,
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
          "id": "52b769f8831d3826607f24929ab7130e9dc62d1e",
          "message": "Merge pull request #331 from LoveDaisy/refactor/field-set-sentinel-proxy\n\nrefactor(config): guard RenderConfig's field set by member count, not sizeof",
          "timestamp": "2026-09-09T13:13:40+08:00",
          "tree_id": "ea00f972f5073c2d5a34b75fbe200d19d12f07b6",
          "url": "https://github.com/LoveDaisy/ice_halo_sim/commit/52b769f8831d3826607f24929ab7130e9dc62d1e"
        },
        "date": 1788931513181,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 1082517.3,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1217969.4,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 818696.2,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 673174.2,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor                \\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "96d644a21248a0968f4866679a3de372c1610833",
          "message": "Merge pull request #334 from LoveDaisy/feat/bg-image-color-picker\n\nfeat(gui): sample Sky Color off the background photo with an eyedropper",
          "timestamp": "2026-09-10T01:16:27+08:00",
          "tree_id": "dacea434bc3720c060ce99ea1e1f7083321478c0",
          "url": "https://github.com/LoveDaisy/ice_halo_sim/commit/96d644a21248a0968f4866679a3de372c1610833"
        },
        "date": 1788974904755,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 1014215.1,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1215012.3,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 814779.6,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 812055.9,
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
          "id": "258d9d34fde41b6c3c29d2818e91a4a20c13f2af",
          "message": "Merge pull request #335 from LoveDaisy/ci/drop-unused-vendor-apt-source\n\nci: stop depending on a vendor apt source nothing here installs from",
          "timestamp": "2026-09-10T02:19:42+08:00",
          "tree_id": "0ec8ef1453363cf2a4e9bac9403be312d6a3d461",
          "url": "https://github.com/LoveDaisy/ice_halo_sim/commit/258d9d34fde41b6c3c29d2818e91a4a20c13f2af"
        },
        "date": 1788978634096,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 771436.7,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1220570.7,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 826659,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 674290.2,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor                \\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "712eb886076cecd28b4dedc683f8255351558cb5",
          "message": "Merge pull request #333 from LoveDaisy/ci/cache-budget\n\nci(cache): budget the actions/cache quota — fix three prefix-shadowed keys, add ccache to the critical-path leg",
          "timestamp": "2026-09-10T02:35:58+08:00",
          "tree_id": "c53075d893ac829084799f406bdd8c280a195292",
          "url": "https://github.com/LoveDaisy/ice_halo_sim/commit/712eb886076cecd28b4dedc683f8255351558cb5"
        },
        "date": 1788979617888,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 1057995.4,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1217168.8,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 895353,
            "unit": "rays/sec",
            "extra": "CPU: INTEL(R) XEON(R) PLATINUM 8573C\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 627227.7,
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
          "id": "491b117b9a07cdf85de8099529f7811e686abf1e",
          "message": "Merge pull request #336 from LoveDaisy/feat/miller-index-and-wedge-presets\n\nfix(gui,core): give the Miller-index wedge conversion one owner, and correct the presets it was never checked against",
          "timestamp": "2026-09-10T04:26:14+08:00",
          "tree_id": "4e290141061128a452482994544759c4c4475a08",
          "url": "https://github.com/LoveDaisy/ice_halo_sim/commit/491b117b9a07cdf85de8099529f7811e686abf1e"
        },
        "date": 1788986313728,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 1000101.9,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1217212.5,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 784590.7,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 666161.5,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor                \\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "7a4c526050e74287deacd4473631926deabf13a9",
          "message": "Merge pull request #337 from LoveDaisy/feat/print-mode-subtractive-ink\n\nfeat(render,gui): add a print tone that lays ink on paper instead of adding light to sky",
          "timestamp": "2026-09-10T09:06:10+08:00",
          "tree_id": "7d1fa0a3596ea0279c8d776fa7d4f0baaa8feab6",
          "url": "https://github.com/LoveDaisy/ice_halo_sim/commit/7a4c526050e74287deacd4473631926deabf13a9"
        },
        "date": 1789003113626,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 839561.6,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1217551.3,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 819537.5,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 671185.8,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor                \\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "ae46f7c283910d4fbb4e7a0f2949aef02e5f87df",
          "message": "Merge pull request #338 from LoveDaisy/feat/gui-display-rendering-regroup\n\nfix(gui): regroup the Display Rendering rows and pair the ground swatch with the mode",
          "timestamp": "2026-09-10T14:08:18+08:00",
          "tree_id": "eeaec146eaa0226974bb83e95e4e1b36f34970a9",
          "url": "https://github.com/LoveDaisy/ice_halo_sim/commit/ae46f7c283910d4fbb4e7a0f2949aef02e5f87df"
        },
        "date": 1789021106000,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 1016691.3,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1220236.7,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 817266.3,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 660118.1,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor                \\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "3ad57411dc16f516b6785967efaba5266c88e7b8",
          "message": "Merge pull request #339 from LoveDaisy/feat/test-capi-lib\n\ntest: liblumice_testapi, a test-only export surface beside the product C API",
          "timestamp": "2026-09-10T16:59:47+08:00",
          "tree_id": "91bfa648700adc1c02137e2bab552ca4271f3417",
          "url": "https://github.com/LoveDaisy/ice_halo_sim/commit/3ad57411dc16f516b6785967efaba5266c88e7b8"
        },
        "date": 1789031375132,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 798856.7,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1219764.9,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 817944.8,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 666878.2,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor                \\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "724fa7cff5ff0cc832f33d45b08e1bf3d4536f40",
          "message": "Merge pull request #342 from LoveDaisy/feat/annotation-lines-shader-anchors-api\n\ngui: auxiliary lines track the camera every frame again; anchors-only annotation API (v4.28)",
          "timestamp": "2026-09-10T17:18:09+08:00",
          "tree_id": "7f9537581734b6e612b1d500271e44df92102186",
          "url": "https://github.com/LoveDaisy/ice_halo_sim/commit/724fa7cff5ff0cc832f33d45b08e1bf3d4536f40"
        },
        "date": 1789032511943,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 856421.4,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1215198,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 756353.4,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 9V74 80-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 808847.8,
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
          "id": "d57f132dc2f546ad001a215fb28d6c917bddabf4",
          "message": "Merge pull request #340 from LoveDaisy/docs/working-discipline-hardening\n\ndocs+hooks: harden two working-discipline rules into criteria and a commit gate",
          "timestamp": "2026-09-10T18:06:39+08:00",
          "tree_id": "1e03b6ab5fd5688bd565295581883fc4c9690d85",
          "url": "https://github.com/LoveDaisy/ice_halo_sim/commit/d57f132dc2f546ad001a215fb28d6c917bddabf4"
        },
        "date": 1789035355617,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 896997.6,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1211723.7,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 822315.2,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 633852.2,
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
          "id": "37b141504a40cc0be937f0d5bf071fef24759171",
          "message": "Merge pull request #341 from LoveDaisy/test/defaults-panel-refs-reshoot\n\ntest(gui): pin the wedge add row in every preset scene, and re-shoot the two that were not",
          "timestamp": "2026-09-10T18:49:54+08:00",
          "tree_id": "42fe951d730ad0cd31b0d30b5c6fc14fa0ec2dc1",
          "url": "https://github.com/LoveDaisy/ice_halo_sim/commit/37b141504a40cc0be937f0d5bf071fef24759171"
        },
        "date": 1789038031511,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 877212.8,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1210583.8,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 819254.1,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 810913.6,
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
          "id": "f860acc46471dee85057842a8611895cea64b88d",
          "message": "Merge pull request #343 from LoveDaisy/feat/gui-print-mode-label-ink\n\nfix(gui): draw overlay label text as ink under the print tone",
          "timestamp": "2026-09-10T20:57:53+08:00",
          "tree_id": "f3daf403c69c5adec882772328d1a77ec96a9215",
          "url": "https://github.com/LoveDaisy/ice_halo_sim/commit/f860acc46471dee85057842a8611895cea64b88d"
        },
        "date": 1789045695822,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 763603.8,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1212271.2,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 955404.3,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 9V74 80-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 674721.6,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor                \\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "dc76b64939e2b7e7bb319dee15905da1c73ee7fa",
          "message": "Merge pull request #344 from LoveDaisy/feat/image-comparison-metric-by-layer\n\ntest: give each image comparison a ruler that matches its layer (pixel ruler, lines-only parity, block-mean PSNR)",
          "timestamp": "2026-09-11T01:37:19+08:00",
          "tree_id": "41b16f85c1532f746a24d73f00d8388af7f2060a",
          "url": "https://github.com/LoveDaisy/ice_halo_sim/commit/dc76b64939e2b7e7bb319dee15905da1c73ee7fa"
        },
        "date": 1789062631306,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 814269.9,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1213530.8,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 817358.8,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 692646.5,
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
          "id": "38aff9c6a97f3fdcca9801ffeb6e1dcecf4be998",
          "message": "Merge pull request #345 from LoveDaisy/chore/release-4.5.1\n\nrelease: cut 4.5.1, and make the release a per-version backfill chore",
          "timestamp": "2026-09-11T08:06:39+08:00",
          "tree_id": "c20bb077289e78ac31076d801efb94c50cab02ad",
          "url": "https://github.com/LoveDaisy/ice_halo_sim/commit/38aff9c6a97f3fdcca9801ffeb6e1dcecf4be998"
        },
        "date": 1789085819488,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 776743.4,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1209815.5,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 819284.4,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 801751.1,
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
          "id": "142e29e615d7a573006eabf610b33948e5584c98",
          "message": "Merge pull request #346 from LoveDaisy/feat/hardware-perf-distribution\n\nbuild/release: ship ISA- and GPU-matched binaries behind CPUID launchers (x86-64-v4 Linux, x86-64-v3 clang-cl Windows, sm_120 fatbin)",
          "timestamp": "2026-09-11T20:51:32+08:00",
          "tree_id": "f05bd3485353b55d626d7d9fe93091774693bb97",
          "url": "https://github.com/LoveDaisy/ice_halo_sim/commit/142e29e615d7a573006eabf610b33948e5584c98"
        },
        "date": 1789131614038,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 998804.1,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1214310.8,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 751824.9,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 9V74 80-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 667423.5,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor                \\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "4fdfe61cc4b1326f60a924a669133f7c6311023d",
          "message": "Merge pull request #347 from LoveDaisy/feat/raypath-analysis-panel\n\nfeat: raypath analysis panel — dedicated non-rendering pass, ranked by chain",
          "timestamp": "2026-09-12T16:27:12+08:00",
          "tree_id": "071aa48f973504cccddec5a5f2199596e22adfc9",
          "url": "https://github.com/LoveDaisy/ice_halo_sim/commit/4fdfe61cc4b1326f60a924a669133f7c6311023d"
        },
        "date": 1789202421547,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 1030197.9,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1189140.5,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 738088.5,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 9V74 80-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 667082.2,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor                \\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "70eb8f5fad44336cf2b57da4d314a9aced4c8224",
          "message": "Merge pull request #349 from LoveDaisy/feat/raypath-analysis-followups\n\nRaypath analysis follow-ups: fixed-seed reproducibility, session-kind rebuild predicate, joiner glyphs, debt sweep",
          "timestamp": "2026-09-12T23:46:53+08:00",
          "tree_id": "1570515bb91fdd1a4610a3ac00dc77a6ae1f7cca",
          "url": "https://github.com/LoveDaisy/ice_halo_sim/commit/70eb8f5fad44336cf2b57da4d314a9aced4c8224"
        },
        "date": 1789228786544,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 1083377.5,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1180853.3,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 1232767.7,
            "unit": "rays/sec",
            "extra": "CPU: Intel(R) Xeon(R) 6973P-C\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 663385.7,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor                \\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "c106292be4122b60092a9666131dde843f639b15",
          "message": "Merge pull request #348 from LoveDaisy/feat/crystal-ray-allocation\n\nfeat(core): adaptive ray allocation across crystal entries (scene.ray_allocation)",
          "timestamp": "2026-09-13T04:19:28+08:00",
          "tree_id": "82448d3731df653e760db492e5a8794a13dc6f0c",
          "url": "https://github.com/LoveDaisy/ice_halo_sim/commit/c106292be4122b60092a9666131dde843f639b15"
        },
        "date": 1789245355985,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 1070986.5,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1188828.2,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 801067.3,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 626023.9,
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
          "id": "88e0fbf6864b1d95ba7e19c4d6660fb8c15c1f4c",
          "message": "Merge pull request #350 from LoveDaisy/chore/install-manual-refresh-and-review-minors\n\nchore: refresh the install manual, land the metric-by-layer review minors, report wrong-size anchor planes once",
          "timestamp": "2026-09-13T04:52:15+08:00",
          "tree_id": "94bf3da366cd365b6946ce53b5309cd6df95c36e",
          "url": "https://github.com/LoveDaisy/ice_halo_sim/commit/88e0fbf6864b1d95ba7e19c4d6660fb8c15c1f4c"
        },
        "date": 1789247023205,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 1116608.1,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1181067.4,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 801885.5,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 626387.9,
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
          "id": "d5c230764f43779ffb32bc75e454ec07f2159a03",
          "message": "Merge pull request #351 from LoveDaisy/fix/exposure-mode-combo-fixed-separation\n\ntest(gui): prove exposure-mode separation with an intensity probe, not a seed-dependent gap",
          "timestamp": "2026-09-13T05:13:06+08:00",
          "tree_id": "8da3f1ccd7d22ef20d6fff6dff9a5c615f8e013d",
          "url": "https://github.com/LoveDaisy/ice_halo_sim/commit/d5c230764f43779ffb32bc75e454ec07f2159a03"
        },
        "date": 1789248086988,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 1101595.8,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1181361.7,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 742006.3,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 9V74 80-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 997131.7,
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
          "id": "7a68880e398137ef91895edbb6d4ff0c997923e4",
          "message": "Merge pull request #352 from LoveDaisy/chore/regen-refs-deterministic-single-shot\n\nchore(regen-refs): shoot deterministic groups once, share runs across groups, refuse stale-base reshoots",
          "timestamp": "2026-09-13T05:31:09+08:00",
          "tree_id": "5e9ceaec07873c2b9154175e6d957784999b8820",
          "url": "https://github.com/LoveDaisy/ice_halo_sim/commit/7a68880e398137ef91895edbb6d4ff0c997923e4"
        },
        "date": 1789249342472,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 999683.6,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1181956.6,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 802031.1,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 675892.3,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor                \\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "365409776ad9761a5ebf3402cf9cde48f573e9d8",
          "message": "Merge pull request #353 from LoveDaisy/fix/render-consumer-label-flake-root-cause\n\nfix(test): root-cause the RenderConsumerLabel flake — an uninitialized SunParam azimuth",
          "timestamp": "2026-09-13T05:47:24+08:00",
          "tree_id": "2a620f7705b88d7f56d29a1cf923e190e358fd9e",
          "url": "https://github.com/LoveDaisy/ice_halo_sim/commit/365409776ad9761a5ebf3402cf9cde48f573e9d8"
        },
        "date": 1789250200819,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 1119652.5,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1187489.7,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 865896.2,
            "unit": "rays/sec",
            "extra": "CPU: INTEL(R) XEON(R) PLATINUM 8573C\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 812746.2,
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
          "id": "35235a01c6962917a905abe105b458ee0ba444ab",
          "message": "Merge pull request #354 from LoveDaisy/feat/ray-num-slider-100b-log-scale\n\nfeat(gui): Rays(M) slider spans 0.1..100 000 M on a kLog track, one domain for both rows",
          "timestamp": "2026-09-13T06:17:09+08:00",
          "tree_id": "8c31257305277f134a3473abfb64eca3bdbdbc3a",
          "url": "https://github.com/LoveDaisy/ice_halo_sim/commit/35235a01c6962917a905abe105b458ee0ba444ab"
        },
        "date": 1789252145956,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 859107.6,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1188364,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 808237,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 1013551.4,
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
          "id": "f0add0b4a676a2e20ab27c782e9e9b5a182aac5b",
          "message": "Merge pull request #355 from LoveDaisy/feat/cli-lens-and-grid-contract\n\nfeat(lens): the CLI/GUI lens contract — short-edge fov, defaults, focal length import, annotations at intensity 0",
          "timestamp": "2026-09-13T07:31:34+08:00",
          "tree_id": "14ed237826bf2e9217fc60dc8c0bd508aee5669a",
          "url": "https://github.com/LoveDaisy/ice_halo_sim/commit/f0add0b4a676a2e20ab27c782e9e9b5a182aac5b"
        },
        "date": 1789256495974,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 929375.9,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1186105,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 958150.1,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 9V74 80-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 661972.4,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor                \\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "51fa59e29850545fd09b7f6041faaed4a4bea4cd",
          "message": "Merge pull request #356 from LoveDaisy/feat/cuda-hostgen-black-and-energy-accounting\n\nfix(cuda): host root-gen fallback renders again; landed weight reduced per warp so the energy ledger matches legacy",
          "timestamp": "2026-09-13T08:09:28+08:00",
          "tree_id": "5310d81311d369114cdde3eb97a22ec84b3b27c2",
          "url": "https://github.com/LoveDaisy/ice_halo_sim/commit/51fa59e29850545fd09b7f6041faaed4a4bea4cd"
        },
        "date": 1789258763979,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 1045324.7,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1180404.9,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 813138.5,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 812113,
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
          "id": "22166140295c68e58e6375028394d4587a351c11",
          "message": "Merge pull request #357 from LoveDaisy/feat/view-center-angular-dist-grid\n\nfeat(annotation): view_dist — circles of constant angular distance from the optical axis, config → core → C API → GUI",
          "timestamp": "2026-09-13T12:02:01+08:00",
          "tree_id": "6875aee1957344381ca66a902bb0dc2ba20f8f02",
          "url": "https://github.com/LoveDaisy/ice_halo_sim/commit/22166140295c68e58e6375028394d4587a351c11"
        },
        "date": 1789273862949,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 1240350.2,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1182444.9,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 802146.7,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 1016951.5,
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
          "id": "682bf9aadbd77a61c2d7697ccd4b353bb70a06be",
          "message": "Merge pull request #358 from LoveDaisy/fix/equidistant-focal-length-factor-two\n\nfix(config): equidistant lens f→fov conversion was half the documented value",
          "timestamp": "2026-09-13T12:42:20+08:00",
          "tree_id": "ad66fbc16fa5d96511d2e7667a3348c6c69b86d5",
          "url": "https://github.com/LoveDaisy/ice_halo_sim/commit/682bf9aadbd77a61c2d7697ccd4b353bb70a06be"
        },
        "date": 1789275329406,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 808629,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1180008.8,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 803160.1,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 665843.4,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor                \\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "b10d1bbc134afb64423fa842b7740de5a59934f2",
          "message": "Merge pull request #359 from LoveDaisy/feat/analysis-panel-polish\n\nfeat(gui): raypath analysis panel polish — first-picture gate, draw layer, geometry, thousands grouping, Export CSV",
          "timestamp": "2026-09-13T14:55:21+08:00",
          "tree_id": "ffbfcccfae868fd3b9504d3d98acd998d41a1933",
          "url": "https://github.com/LoveDaisy/ice_halo_sim/commit/b10d1bbc134afb64423fa842b7740de5a59934f2"
        },
        "date": 1789283178026,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 1239098.3,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1180647.3,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 880752.2,
            "unit": "rays/sec",
            "extra": "CPU: Intel(R) Xeon(R) Platinum 8370C CPU @ 2.80GHz\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 623845.9,
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
          "id": "90d23cafd492e0f65663df5a681c439b0fa09f35",
          "message": "Merge pull request #360 from LoveDaisy/feat/analysis-standing-cpu-pool\n\nfeat(server): standing CPU analysis pool on the GPU route, woken by session kind",
          "timestamp": "2026-09-13T15:15:21+08:00",
          "tree_id": "0e81a696990d2128a156faf657717d6b2838d130",
          "url": "https://github.com/LoveDaisy/ice_halo_sim/commit/90d23cafd492e0f65663df5a681c439b0fa09f35"
        },
        "date": 1789284492874,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 865014,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1184775.3,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 949906.9,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 9V74 80-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 673336.5,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor                \\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "0eba76808ec4e34d0e76e5c741347a0779db27c2",
          "message": "Merge pull request #361 from LoveDaisy/feat/panel-state-round-trip\n\nfeat(gui): panel-derived state round trip — analysis list freshness predicate, colour-ref layer re-indexing",
          "timestamp": "2026-09-13T15:37:56+08:00",
          "tree_id": "e7548f5a60a46f37b724f835b81f368a1b59d85a",
          "url": "https://github.com/LoveDaisy/ice_halo_sim/commit/0eba76808ec4e34d0e76e5c741347a0779db27c2"
        },
        "date": 1789285829347,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 785131.5,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1182613.8,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 807535.4,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 666985.5,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor                \\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "673e55308cc451c53b25f590d5d48bf83f9ecf17",
          "message": "Merge pull request #362 from LoveDaisy/feat/angular-distance-section-merge\n\nfeat(gui): merge both angular-distance ring families into one collapsed Angular Distance section",
          "timestamp": "2026-09-13T16:21:39+08:00",
          "tree_id": "5b13f138562a66f363f9e17a9a4111ed6049f9d4",
          "url": "https://github.com/LoveDaisy/ice_halo_sim/commit/673e55308cc451c53b25f590d5d48bf83f9ecf17"
        },
        "date": 1789288418055,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 843028.5,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1184766.5,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 810535.7,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 671131.4,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor                \\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "1a6c43be3f9fa9d8d89a3115d1fdf2f994e3a984",
          "message": "Merge pull request #363 from LoveDaisy/chore/angular-distance-from-naming\n\nfeat(gui): name the Angular Distance section \"from...\" and its rows Sun / Lens Center",
          "timestamp": "2026-09-13T20:15:08+08:00",
          "tree_id": "042b5e14f5fbae521e6d1e53edc4d0889e44bcf5",
          "url": "https://github.com/LoveDaisy/ice_halo_sim/commit/1a6c43be3f9fa9d8d89a3115d1fdf2f994e3a984"
        },
        "date": 1789302342472,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 1015912.7,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1181335.2,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 886752.4,
            "unit": "rays/sec",
            "extra": "CPU: Intel(R) Xeon(R) Platinum 8370C CPU @ 2.80GHz\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 668539,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor                \\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "6a6ae79565932a61955ad281237b6445b8a5d48f",
          "message": "Merge pull request #364 from LoveDaisy/chore/hide-ray-allocation-checkbox\n\nchore(gui): hide Adaptive ray allocation checkbox from the main panel",
          "timestamp": "2026-09-13T22:02:17+08:00",
          "tree_id": "53eedb96fe96c777dbf6f5fd7d8737f2e45e20f6",
          "url": "https://github.com/LoveDaisy/ice_halo_sim/commit/6a6ae79565932a61955ad281237b6445b8a5d48f"
        },
        "date": 1789308816966,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 867115.6,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1182789.4,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 1005747.3,
            "unit": "rays/sec",
            "extra": "CPU: INTEL(R) XEON(R) PLATINUM 8573C\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 624763.3,
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
          "id": "19741f43a26b205cd9953af019cae4b44fd12984",
          "message": "Merge pull request #365 from LoveDaisy/feat/overlay-panel-ux\n\nfeat(gui): Overlay panel UX — Lens Center circle defaults and a Reference Points All row",
          "timestamp": "2026-09-14T00:15:21+08:00",
          "tree_id": "d43c7c2948629dec68903aa2f4de6ded9dcafbae",
          "url": "https://github.com/LoveDaisy/ice_halo_sim/commit/19741f43a26b205cd9953af019cae4b44fd12984"
        },
        "date": 1789316828384,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 823023.2,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1181066.6,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 809130.8,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 628147.9,
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
          "id": "c4288079e3f5670454e7cc8788194498a3f584f2",
          "message": "Merge pull request #366 from LoveDaisy/feat/analysis-session-ray-allocation\n\nfeat(analysis): analysis sessions follow scene.ray_allocation; drop the Rays column",
          "timestamp": "2026-09-14T00:38:07+08:00",
          "tree_id": "d76d462fb476b2cff0e3581ec03afa0c622be7a6",
          "url": "https://github.com/LoveDaisy/ice_halo_sim/commit/c4288079e3f5670454e7cc8788194498a3f584f2"
        },
        "date": 1789318083794,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 976919.8,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1179451.3,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 1021485.3,
            "unit": "rays/sec",
            "extra": "CPU: INTEL(R) XEON(R) PLATINUM 8573C\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 669756.5,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor                \\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "fd1bb1cffe3edfbbf7c4de2a1eff5ff0ecb5ea1a",
          "message": "Merge pull request #367 from LoveDaisy/chore/release-4.6.0\n\nchore(release): cut 4.6.0",
          "timestamp": "2026-09-14T01:21:05+08:00",
          "tree_id": "bd318927bfaf54e9b78b5aacd9ba6b7628b02941",
          "url": "https://github.com/LoveDaisy/ice_halo_sim/commit/fd1bb1cffe3edfbbf7c4de2a1eff5ff0ecb5ea1a"
        },
        "date": 1789320643258,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 858850.7,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1184727.4,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 807455.3,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 705498.1,
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
          "id": "7a9430ef19ad7197b2befd279263c2e4e3474f45",
          "message": "Merge pull request #368 from LoveDaisy/chore/cli-subcommands\n\ncli: split the flat flag set into render / benchmark subcommands",
          "timestamp": "2026-09-14T18:29:48+08:00",
          "tree_id": "5979324f4b5810dffb657260e392b8fab205f97d",
          "url": "https://github.com/LoveDaisy/ice_halo_sim/commit/7a9430ef19ad7197b2befd279263c2e4e3474f45"
        },
        "date": 1789382391970,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 709955.6,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1180195.1,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 827120.6,
            "unit": "rays/sec",
            "extra": "CPU: INTEL(R) XEON(R) PLATINUM 8573C\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 659320.6,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor                \\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "9232bccb77567b58b5b9342c1049e69337b2fb44",
          "message": "Merge pull request #369 from LoveDaisy/feat/cli-raypath-analyze\n\ncli: add the `analyze` subcommand — raypath analysis from the command line",
          "timestamp": "2026-09-14T20:18:27+08:00",
          "tree_id": "aa2b67b0a92fe71b387a76367f305cdf54930982",
          "url": "https://github.com/LoveDaisy/ice_halo_sim/commit/9232bccb77567b58b5b9342c1049e69337b2fb44"
        },
        "date": 1789389138582,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 842600.9,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1185218.4,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 799761.5,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 673096.9,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor                \\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "f220e71f72ceab196f527085937d1268369d7e63",
          "message": "Merge pull request #370 from LoveDaisy/chore/cli-render-seed-and-small-fixes\n\nchore: render --seed, double emitted-energy accumulators, manual fixes, GUI log sink to stderr",
          "timestamp": "2026-09-16T08:42:53+08:00",
          "tree_id": "9ad81ba8b1b719f173f4198a5be2b649c860cde2",
          "url": "https://github.com/LoveDaisy/Lumice/commit/f220e71f72ceab196f527085937d1268369d7e63"
        },
        "date": 1789520231854,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 852366.8,
            "unit": "rays/sec",
            "extra": "CPU: Apple M1 (Virtual)\\nCores: 3"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 1182436.2,
            "unit": "rays/sec",
            "extra": "CPU: Neoverse-N2\\nCores: 4"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 811953.8,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor\\nCores: 4"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 669535.6,
            "unit": "rays/sec",
            "extra": "CPU: AMD EPYC 7763 64-Core Processor                \\nCores: 4"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
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
          "id": "5cb81a9565ff0e1394a957d97ad2078d0d8f9310",
          "message": "Merge pull request #315 from LoveDaisy/scrum/changelog-backfill-and-release-notes\n\ndocs(release): 回填 v4.1.4 起 31 个版本的 CHANGELOG，并把它接进发版链路",
          "timestamp": "2026-09-06T16:24:48+08:00",
          "tree_id": "079d5a016ac9ac51339233ba0779369c19e64745",
          "url": "https://github.com/LoveDaisy/ice_halo_sim/commit/5cb81a9565ff0e1394a957d97ad2078d0d8f9310"
        },
        "date": 1788683899356,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 76.2,
            "unit": "%"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 99.7,
            "unit": "%"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 94.9,
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
          "id": "c04ad137d42e4a3ed51cd137b75d2d97092b7864",
          "message": "Merge pull request #316 from LoveDaisy/fix/msvc-string-literal-limit\n\nfix(gui,ci): 拆开超 MSVC 上限的 shader 字面量 + 立静态门禁 + CI 触发去重",
          "timestamp": "2026-09-06T23:22:44+08:00",
          "tree_id": "6cd1e5d94251d724205eb0763696031080e86c9d",
          "url": "https://github.com/LoveDaisy/ice_halo_sim/commit/c04ad137d42e4a3ed51cd137b75d2d97092b7864"
        },
        "date": 1788708810363,
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
            "value": 94.2,
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
          "id": "349656ac94b6b2fd27ecbf4ed35812bea7646a2a",
          "message": "Merge pull request #317 from LoveDaisy/ci/windows-release-image-unify\n\nci: build Windows on the image we actually release from",
          "timestamp": "2026-09-07T01:04:20+08:00",
          "tree_id": "d7e945127aac502e4a16adeca4de66c2fca33ff5",
          "url": "https://github.com/LoveDaisy/ice_halo_sim/commit/349656ac94b6b2fd27ecbf4ed35812bea7646a2a"
        },
        "date": 1788714903512,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 86.4,
            "unit": "%"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 99.5,
            "unit": "%"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 94.6,
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
          "id": "59208e341a7a63e1f22366ec0fbc47211fd93950",
          "message": "Merge pull request #318 from LoveDaisy/test/e2e-cost-and-oracle-audit\n\ntest(e2e): 按「每个测试为自己的开销举证」审计套件成本，恢复预算余量",
          "timestamp": "2026-09-07T04:18:31+08:00",
          "tree_id": "a180e65114ec4dbebe9febd562ebcb9d7dcb6dcd",
          "url": "https://github.com/LoveDaisy/ice_halo_sim/commit/59208e341a7a63e1f22366ec0fbc47211fd93950"
        },
        "date": 1788726580118,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 80.4,
            "unit": "%"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 99.5,
            "unit": "%"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 88.8,
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
          "id": "e707b15d31a6676c8f4147a0b0cfe62dfc452995",
          "message": "Merge pull request #319 from LoveDaisy/fix/gui-entry-delete-vs-open-editor\n\nfix(gui): keep the edit modal bound to its entry across a delete",
          "timestamp": "2026-09-08T11:14:03+08:00",
          "tree_id": "493abe69703c95a59432e4a6f6623947ade2f407",
          "url": "https://github.com/LoveDaisy/ice_halo_sim/commit/e707b15d31a6676c8f4147a0b0cfe62dfc452995"
        },
        "date": 1788837912651,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 106.2,
            "unit": "%"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 99.9,
            "unit": "%"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 96.8,
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
          "id": "da9e5533acc8c01c61877b6613ddf40bdce9a8b4",
          "message": "Merge pull request #320 from LoveDaisy/fix/cuda-zero-ray-batch-poisons-backend\n\nfix(cuda): stop a zero-ray layer from poisoning the CUDA backend",
          "timestamp": "2026-09-08T17:13:37+08:00",
          "tree_id": "915905aba2ebcbe1abe8eebe327b96458d905c22",
          "url": "https://github.com/LoveDaisy/ice_halo_sim/commit/da9e5533acc8c01c61877b6613ddf40bdce9a8b4"
        },
        "date": 1788859469241,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 73.8,
            "unit": "%"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 100,
            "unit": "%"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 93.8,
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
          "id": "d66985dfc19d7e0a2ad278bfb29e8adec87f3adc",
          "message": "Merge pull request #322 from LoveDaisy/test/random-source-exact-assertion-audit\n\ntest: audit random sources behind exact assertions, and refill the lost closed-form fuzz",
          "timestamp": "2026-09-08T19:05:43+08:00",
          "tree_id": "559a5d866b5f56b5751d9d1be56655c2412ea1a2",
          "url": "https://github.com/LoveDaisy/ice_halo_sim/commit/d66985dfc19d7e0a2ad278bfb29e8adec87f3adc"
        },
        "date": 1788866204883,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 76.4,
            "unit": "%"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 99.8,
            "unit": "%"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 94.6,
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
          "id": "f5d738bc07fade93832384583fd5655dda496ae4",
          "message": "Merge pull request #321 from LoveDaisy/ci/organization-and-windows-testing\n\nci(windows): route MSVC compilation through sccache",
          "timestamp": "2026-09-08T20:34:51+08:00",
          "tree_id": "8cbe8c8f0a06164c7e6f448e8e1b9b8f2b5aeff3",
          "url": "https://github.com/LoveDaisy/ice_halo_sim/commit/f5d738bc07fade93832384583fd5655dda496ae4"
        },
        "date": 1788871604718,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 78.7,
            "unit": "%"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 99.5,
            "unit": "%"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 96.6,
            "unit": "%"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 81.9,
            "unit": "%"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "5dab8ac5279acc30e74281d28ec9852f7091260f",
          "message": "Merge pull request #323 from LoveDaisy/feat/annotation-label-line-independence\n\nfeat(config): give the three grid families a line switch of their own",
          "timestamp": "2026-09-08T21:51:50+08:00",
          "tree_id": "17e102e9476ace17c070d7edc5d9863355c01883",
          "url": "https://github.com/LoveDaisy/ice_halo_sim/commit/5dab8ac5279acc30e74281d28ec9852f7091260f"
        },
        "date": 1788876191569,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 85.6,
            "unit": "%"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 99.2,
            "unit": "%"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 96,
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
          "id": "e06d6f8ee003f53159f4265e0a60478c3912298f",
          "message": "Merge pull request #324 from LoveDaisy/perf/cli-render-poll-floor\n\nperf(cli): poll completion before sleeping, so a render is not floored at 1s",
          "timestamp": "2026-09-08T22:52:18+08:00",
          "tree_id": "0e0c25fa9ce2a27fc0554ad989a39b2c64786891",
          "url": "https://github.com/LoveDaisy/ice_halo_sim/commit/e06d6f8ee003f53159f4265e0a60478c3912298f"
        },
        "date": 1788879788663,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 80.4,
            "unit": "%"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 99.8,
            "unit": "%"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 93.9,
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
          "id": "dfb3f72cde1303813ed87b0403da0bf2d5264b86",
          "message": "Merge pull request #325 from LoveDaisy/fix/user-run-vs-backpressure-gate\n\nfix(gui): exempt a user-initiated Run from the commit backpressure gate",
          "timestamp": "2026-09-08T23:07:48+08:00",
          "tree_id": "3e55872359ee24f0c6224f4e627e5dd6964a1d7d",
          "url": "https://github.com/LoveDaisy/ice_halo_sim/commit/dfb3f72cde1303813ed87b0403da0bf2d5264b86"
        },
        "date": 1788880732587,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 81.9,
            "unit": "%"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 99.7,
            "unit": "%"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 95,
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
          "id": "61afdda60a81f3800fe9d39d0f1790efbed2eb82",
          "message": "Merge pull request #326 from LoveDaisy/ci/cuda-test-tu-compile-coverage\n\nci: compile the CUDA test TUs (close the CUDA×BUILD_TEST empty intersection)",
          "timestamp": "2026-09-09T09:01:05+08:00",
          "tree_id": "d8015a4a11ce1b395d085be4867bade1cd75f056",
          "url": "https://github.com/LoveDaisy/ice_halo_sim/commit/61afdda60a81f3800fe9d39d0f1790efbed2eb82"
        },
        "date": 1788916301959,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 83.8,
            "unit": "%"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 99.9,
            "unit": "%"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 93.5,
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
          "id": "5d04ed2b510cf8da1331d69d8a0ec9d064e8688a",
          "message": "Merge pull request #328 from LoveDaisy/feat/gui-import-capability-boundary\n\nfeat(gui): warn on intentionally unsupported capabilities when importing core/CLI configs",
          "timestamp": "2026-09-09T11:28:47+08:00",
          "tree_id": "5fb7bd048fbeeb0720b37f54dc406c24b4f696f7",
          "url": "https://github.com/LoveDaisy/ice_halo_sim/commit/5d04ed2b510cf8da1331d69d8a0ec9d064e8688a"
        },
        "date": 1788925163996,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 78.9,
            "unit": "%"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 99.5,
            "unit": "%"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 93.9,
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
          "id": "fe4c0778ad67356b7107c49b9f2d1758751cbab0",
          "message": "Merge pull request #329 from LoveDaisy/fix/raypath-load-path-syntax-gate\n\nfix(gui): reject malformed raypath summand rows on the .lmc load path",
          "timestamp": "2026-09-09T11:50:12+08:00",
          "tree_id": "85ae4abe8324f97a506977a0b6b5e2f1ee1a6194",
          "url": "https://github.com/LoveDaisy/ice_halo_sim/commit/fe4c0778ad67356b7107c49b9f2d1758751cbab0"
        },
        "date": 1788926447909,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 67.6,
            "unit": "%"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 99.9,
            "unit": "%"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 94.8,
            "unit": "%"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 81.3,
            "unit": "%"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "61199dc71bb8afa824f182ecb361c1765212e2ba",
          "message": "Merge pull request #330 from LoveDaisy/build/cpm-cache-shared-default\n\nbuild(cpm): default the dependency-source cache to a machine-level directory",
          "timestamp": "2026-09-09T12:28:59+08:00",
          "tree_id": "401ce6afc69f30c5242ddcb0e9b1c84c265275ad",
          "url": "https://github.com/LoveDaisy/ice_halo_sim/commit/61199dc71bb8afa824f182ecb361c1765212e2ba"
        },
        "date": 1788928790150,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 75.9,
            "unit": "%"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 99.6,
            "unit": "%"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 94.5,
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
          "id": "52b769f8831d3826607f24929ab7130e9dc62d1e",
          "message": "Merge pull request #331 from LoveDaisy/refactor/field-set-sentinel-proxy\n\nrefactor(config): guard RenderConfig's field set by member count, not sizeof",
          "timestamp": "2026-09-09T13:13:40+08:00",
          "tree_id": "ea00f972f5073c2d5a34b75fbe200d19d12f07b6",
          "url": "https://github.com/LoveDaisy/ice_halo_sim/commit/52b769f8831d3826607f24929ab7130e9dc62d1e"
        },
        "date": 1788931515598,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 78.9,
            "unit": "%"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 99.7,
            "unit": "%"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 93.1,
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
          "id": "96d644a21248a0968f4866679a3de372c1610833",
          "message": "Merge pull request #334 from LoveDaisy/feat/bg-image-color-picker\n\nfeat(gui): sample Sky Color off the background photo with an eyedropper",
          "timestamp": "2026-09-10T01:16:27+08:00",
          "tree_id": "dacea434bc3720c060ce99ea1e1f7083321478c0",
          "url": "https://github.com/LoveDaisy/ice_halo_sim/commit/96d644a21248a0968f4866679a3de372c1610833"
        },
        "date": 1788974906928,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 82.5,
            "unit": "%"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 99.6,
            "unit": "%"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 95.2,
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
          "id": "258d9d34fde41b6c3c29d2818e91a4a20c13f2af",
          "message": "Merge pull request #335 from LoveDaisy/ci/drop-unused-vendor-apt-source\n\nci: stop depending on a vendor apt source nothing here installs from",
          "timestamp": "2026-09-10T02:19:42+08:00",
          "tree_id": "0ec8ef1453363cf2a4e9bac9403be312d6a3d461",
          "url": "https://github.com/LoveDaisy/ice_halo_sim/commit/258d9d34fde41b6c3c29d2818e91a4a20c13f2af"
        },
        "date": 1788978637042,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 71.2,
            "unit": "%"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 99.8,
            "unit": "%"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 94,
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
          "id": "712eb886076cecd28b4dedc683f8255351558cb5",
          "message": "Merge pull request #333 from LoveDaisy/ci/cache-budget\n\nci(cache): budget the actions/cache quota — fix three prefix-shadowed keys, add ccache to the critical-path leg",
          "timestamp": "2026-09-10T02:35:58+08:00",
          "tree_id": "c53075d893ac829084799f406bdd8c280a195292",
          "url": "https://github.com/LoveDaisy/ice_halo_sim/commit/712eb886076cecd28b4dedc683f8255351558cb5"
        },
        "date": 1788979619952,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 74.4,
            "unit": "%"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 99.6,
            "unit": "%"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 92.5,
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
          "id": "491b117b9a07cdf85de8099529f7811e686abf1e",
          "message": "Merge pull request #336 from LoveDaisy/feat/miller-index-and-wedge-presets\n\nfix(gui,core): give the Miller-index wedge conversion one owner, and correct the presets it was never checked against",
          "timestamp": "2026-09-10T04:26:14+08:00",
          "tree_id": "4e290141061128a452482994544759c4c4475a08",
          "url": "https://github.com/LoveDaisy/ice_halo_sim/commit/491b117b9a07cdf85de8099529f7811e686abf1e"
        },
        "date": 1788986315645,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 80.7,
            "unit": "%"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 99.8,
            "unit": "%"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 93.2,
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
          "id": "7a4c526050e74287deacd4473631926deabf13a9",
          "message": "Merge pull request #337 from LoveDaisy/feat/print-mode-subtractive-ink\n\nfeat(render,gui): add a print tone that lays ink on paper instead of adding light to sky",
          "timestamp": "2026-09-10T09:06:10+08:00",
          "tree_id": "7d1fa0a3596ea0279c8d776fa7d4f0baaa8feab6",
          "url": "https://github.com/LoveDaisy/ice_halo_sim/commit/7a4c526050e74287deacd4473631926deabf13a9"
        },
        "date": 1789003115561,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 76.8,
            "unit": "%"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 99.7,
            "unit": "%"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 94.5,
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
          "id": "ae46f7c283910d4fbb4e7a0f2949aef02e5f87df",
          "message": "Merge pull request #338 from LoveDaisy/feat/gui-display-rendering-regroup\n\nfix(gui): regroup the Display Rendering rows and pair the ground swatch with the mode",
          "timestamp": "2026-09-10T14:08:18+08:00",
          "tree_id": "eeaec146eaa0226974bb83e95e4e1b36f34970a9",
          "url": "https://github.com/LoveDaisy/ice_halo_sim/commit/ae46f7c283910d4fbb4e7a0f2949aef02e5f87df"
        },
        "date": 1789021108360,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 93.4,
            "unit": "%"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 99.7,
            "unit": "%"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 94.3,
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
          "id": "3ad57411dc16f516b6785967efaba5266c88e7b8",
          "message": "Merge pull request #339 from LoveDaisy/feat/test-capi-lib\n\ntest: liblumice_testapi, a test-only export surface beside the product C API",
          "timestamp": "2026-09-10T16:59:47+08:00",
          "tree_id": "91bfa648700adc1c02137e2bab552ca4271f3417",
          "url": "https://github.com/LoveDaisy/ice_halo_sim/commit/3ad57411dc16f516b6785967efaba5266c88e7b8"
        },
        "date": 1789031377316,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 79.9,
            "unit": "%"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 99.7,
            "unit": "%"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 96,
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
          "id": "724fa7cff5ff0cc832f33d45b08e1bf3d4536f40",
          "message": "Merge pull request #342 from LoveDaisy/feat/annotation-lines-shader-anchors-api\n\ngui: auxiliary lines track the camera every frame again; anchors-only annotation API (v4.28)",
          "timestamp": "2026-09-10T17:18:09+08:00",
          "tree_id": "7f9537581734b6e612b1d500271e44df92102186",
          "url": "https://github.com/LoveDaisy/ice_halo_sim/commit/724fa7cff5ff0cc832f33d45b08e1bf3d4536f40"
        },
        "date": 1789032513828,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 99.9,
            "unit": "%"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 99.8,
            "unit": "%"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 96.6,
            "unit": "%"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 90.4,
            "unit": "%"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "d57f132dc2f546ad001a215fb28d6c917bddabf4",
          "message": "Merge pull request #340 from LoveDaisy/docs/working-discipline-hardening\n\ndocs+hooks: harden two working-discipline rules into criteria and a commit gate",
          "timestamp": "2026-09-10T18:06:39+08:00",
          "tree_id": "1e03b6ab5fd5688bd565295581883fc4c9690d85",
          "url": "https://github.com/LoveDaisy/ice_halo_sim/commit/d57f132dc2f546ad001a215fb28d6c917bddabf4"
        },
        "date": 1789035357696,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 86.7,
            "unit": "%"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 100,
            "unit": "%"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 94.6,
            "unit": "%"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 90.8,
            "unit": "%"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "37b141504a40cc0be937f0d5bf071fef24759171",
          "message": "Merge pull request #341 from LoveDaisy/test/defaults-panel-refs-reshoot\n\ntest(gui): pin the wedge add row in every preset scene, and re-shoot the two that were not",
          "timestamp": "2026-09-10T18:49:54+08:00",
          "tree_id": "42fe951d730ad0cd31b0d30b5c6fc14fa0ec2dc1",
          "url": "https://github.com/LoveDaisy/ice_halo_sim/commit/37b141504a40cc0be937f0d5bf071fef24759171"
        },
        "date": 1789038034008,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 75.9,
            "unit": "%"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 99.7,
            "unit": "%"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 93.9,
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
          "id": "f860acc46471dee85057842a8611895cea64b88d",
          "message": "Merge pull request #343 from LoveDaisy/feat/gui-print-mode-label-ink\n\nfix(gui): draw overlay label text as ink under the print tone",
          "timestamp": "2026-09-10T20:57:53+08:00",
          "tree_id": "f3daf403c69c5adec882772328d1a77ec96a9215",
          "url": "https://github.com/LoveDaisy/ice_halo_sim/commit/f860acc46471dee85057842a8611895cea64b88d"
        },
        "date": 1789045698466,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 70.4,
            "unit": "%"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 99.8,
            "unit": "%"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 95.6,
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
          "id": "dc76b64939e2b7e7bb319dee15905da1c73ee7fa",
          "message": "Merge pull request #344 from LoveDaisy/feat/image-comparison-metric-by-layer\n\ntest: give each image comparison a ruler that matches its layer (pixel ruler, lines-only parity, block-mean PSNR)",
          "timestamp": "2026-09-11T01:37:19+08:00",
          "tree_id": "41b16f85c1532f746a24d73f00d8388af7f2060a",
          "url": "https://github.com/LoveDaisy/ice_halo_sim/commit/dc76b64939e2b7e7bb319dee15905da1c73ee7fa"
        },
        "date": 1789062633837,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 78.3,
            "unit": "%"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 99.8,
            "unit": "%"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 93.4,
            "unit": "%"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 86.2,
            "unit": "%"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "38aff9c6a97f3fdcca9801ffeb6e1dcecf4be998",
          "message": "Merge pull request #345 from LoveDaisy/chore/release-4.5.1\n\nrelease: cut 4.5.1, and make the release a per-version backfill chore",
          "timestamp": "2026-09-11T08:06:39+08:00",
          "tree_id": "c20bb077289e78ac31076d801efb94c50cab02ad",
          "url": "https://github.com/LoveDaisy/ice_halo_sim/commit/38aff9c6a97f3fdcca9801ffeb6e1dcecf4be998"
        },
        "date": 1789085821398,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 84.2,
            "unit": "%"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 99.7,
            "unit": "%"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 94.3,
            "unit": "%"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 89.4,
            "unit": "%"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "142e29e615d7a573006eabf610b33948e5584c98",
          "message": "Merge pull request #346 from LoveDaisy/feat/hardware-perf-distribution\n\nbuild/release: ship ISA- and GPU-matched binaries behind CPUID launchers (x86-64-v4 Linux, x86-64-v3 clang-cl Windows, sm_120 fatbin)",
          "timestamp": "2026-09-11T20:51:32+08:00",
          "tree_id": "f05bd3485353b55d626d7d9fe93091774693bb97",
          "url": "https://github.com/LoveDaisy/ice_halo_sim/commit/142e29e615d7a573006eabf610b33948e5584c98"
        },
        "date": 1789131616296,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 94.4,
            "unit": "%"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 100.3,
            "unit": "%"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 96.5,
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
          "id": "4fdfe61cc4b1326f60a924a669133f7c6311023d",
          "message": "Merge pull request #347 from LoveDaisy/feat/raypath-analysis-panel\n\nfeat: raypath analysis panel — dedicated non-rendering pass, ranked by chain",
          "timestamp": "2026-09-12T16:27:12+08:00",
          "tree_id": "071aa48f973504cccddec5a5f2199596e22adfc9",
          "url": "https://github.com/LoveDaisy/ice_halo_sim/commit/4fdfe61cc4b1326f60a924a669133f7c6311023d"
        },
        "date": 1789202423219,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 75.6,
            "unit": "%"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 99.5,
            "unit": "%"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 96.1,
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
          "id": "70eb8f5fad44336cf2b57da4d314a9aced4c8224",
          "message": "Merge pull request #349 from LoveDaisy/feat/raypath-analysis-followups\n\nRaypath analysis follow-ups: fixed-seed reproducibility, session-kind rebuild predicate, joiner glyphs, debt sweep",
          "timestamp": "2026-09-12T23:46:53+08:00",
          "tree_id": "1570515bb91fdd1a4610a3ac00dc77a6ae1f7cca",
          "url": "https://github.com/LoveDaisy/ice_halo_sim/commit/70eb8f5fad44336cf2b57da4d314a9aced4c8224"
        },
        "date": 1789228788442,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 84.5,
            "unit": "%"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 99.4,
            "unit": "%"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 96.7,
            "unit": "%"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 88.1,
            "unit": "%"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "c106292be4122b60092a9666131dde843f639b15",
          "message": "Merge pull request #348 from LoveDaisy/feat/crystal-ray-allocation\n\nfeat(core): adaptive ray allocation across crystal entries (scene.ray_allocation)",
          "timestamp": "2026-09-13T04:19:28+08:00",
          "tree_id": "82448d3731df653e760db492e5a8794a13dc6f0c",
          "url": "https://github.com/LoveDaisy/ice_halo_sim/commit/c106292be4122b60092a9666131dde843f639b15"
        },
        "date": 1789245358053,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 81.5,
            "unit": "%"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 99.9,
            "unit": "%"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 92.8,
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
          "id": "88e0fbf6864b1d95ba7e19c4d6660fb8c15c1f4c",
          "message": "Merge pull request #350 from LoveDaisy/chore/install-manual-refresh-and-review-minors\n\nchore: refresh the install manual, land the metric-by-layer review minors, report wrong-size anchor planes once",
          "timestamp": "2026-09-13T04:52:15+08:00",
          "tree_id": "94bf3da366cd365b6946ce53b5309cd6df95c36e",
          "url": "https://github.com/LoveDaisy/ice_halo_sim/commit/88e0fbf6864b1d95ba7e19c4d6660fb8c15c1f4c"
        },
        "date": 1789247025108,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 80.8,
            "unit": "%"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 99.7,
            "unit": "%"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 94.9,
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
          "id": "d5c230764f43779ffb32bc75e454ec07f2159a03",
          "message": "Merge pull request #351 from LoveDaisy/fix/exposure-mode-combo-fixed-separation\n\ntest(gui): prove exposure-mode separation with an intensity probe, not a seed-dependent gap",
          "timestamp": "2026-09-13T05:13:06+08:00",
          "tree_id": "8da3f1ccd7d22ef20d6fff6dff9a5c615f8e013d",
          "url": "https://github.com/LoveDaisy/ice_halo_sim/commit/d5c230764f43779ffb32bc75e454ec07f2159a03"
        },
        "date": 1789248088818,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 77.4,
            "unit": "%"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 99.6,
            "unit": "%"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 96.5,
            "unit": "%"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 90.5,
            "unit": "%"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "7a68880e398137ef91895edbb6d4ff0c997923e4",
          "message": "Merge pull request #352 from LoveDaisy/chore/regen-refs-deterministic-single-shot\n\nchore(regen-refs): shoot deterministic groups once, share runs across groups, refuse stale-base reshoots",
          "timestamp": "2026-09-13T05:31:09+08:00",
          "tree_id": "5e9ceaec07873c2b9154175e6d957784999b8820",
          "url": "https://github.com/LoveDaisy/ice_halo_sim/commit/7a68880e398137ef91895edbb6d4ff0c997923e4"
        },
        "date": 1789249344475,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 85.6,
            "unit": "%"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 99.7,
            "unit": "%"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 94.3,
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
          "id": "365409776ad9761a5ebf3402cf9cde48f573e9d8",
          "message": "Merge pull request #353 from LoveDaisy/fix/render-consumer-label-flake-root-cause\n\nfix(test): root-cause the RenderConsumerLabel flake — an uninitialized SunParam azimuth",
          "timestamp": "2026-09-13T05:47:24+08:00",
          "tree_id": "2a620f7705b88d7f56d29a1cf923e190e358fd9e",
          "url": "https://github.com/LoveDaisy/ice_halo_sim/commit/365409776ad9761a5ebf3402cf9cde48f573e9d8"
        },
        "date": 1789250203207,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 78.8,
            "unit": "%"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 99.9,
            "unit": "%"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 92.5,
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
          "id": "35235a01c6962917a905abe105b458ee0ba444ab",
          "message": "Merge pull request #354 from LoveDaisy/feat/ray-num-slider-100b-log-scale\n\nfeat(gui): Rays(M) slider spans 0.1..100 000 M on a kLog track, one domain for both rows",
          "timestamp": "2026-09-13T06:17:09+08:00",
          "tree_id": "8c31257305277f134a3473abfb64eca3bdbdbc3a",
          "url": "https://github.com/LoveDaisy/ice_halo_sim/commit/35235a01c6962917a905abe105b458ee0ba444ab"
        },
        "date": 1789252147861,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 79.7,
            "unit": "%"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 100.1,
            "unit": "%"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 93.4,
            "unit": "%"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 88.7,
            "unit": "%"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "f0add0b4a676a2e20ab27c782e9e9b5a182aac5b",
          "message": "Merge pull request #355 from LoveDaisy/feat/cli-lens-and-grid-contract\n\nfeat(lens): the CLI/GUI lens contract — short-edge fov, defaults, focal length import, annotations at intensity 0",
          "timestamp": "2026-09-13T07:31:34+08:00",
          "tree_id": "14ed237826bf2e9217fc60dc8c0bd508aee5669a",
          "url": "https://github.com/LoveDaisy/ice_halo_sim/commit/f0add0b4a676a2e20ab27c782e9e9b5a182aac5b"
        },
        "date": 1789256498533,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 76.6,
            "unit": "%"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 99.7,
            "unit": "%"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 95.5,
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
          "id": "51fa59e29850545fd09b7f6041faaed4a4bea4cd",
          "message": "Merge pull request #356 from LoveDaisy/feat/cuda-hostgen-black-and-energy-accounting\n\nfix(cuda): host root-gen fallback renders again; landed weight reduced per warp so the energy ledger matches legacy",
          "timestamp": "2026-09-13T08:09:28+08:00",
          "tree_id": "5310d81311d369114cdde3eb97a22ec84b3b27c2",
          "url": "https://github.com/LoveDaisy/ice_halo_sim/commit/51fa59e29850545fd09b7f6041faaed4a4bea4cd"
        },
        "date": 1789258765905,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 78.4,
            "unit": "%"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 99.3,
            "unit": "%"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 95,
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
          "id": "22166140295c68e58e6375028394d4587a351c11",
          "message": "Merge pull request #357 from LoveDaisy/feat/view-center-angular-dist-grid\n\nfeat(annotation): view_dist — circles of constant angular distance from the optical axis, config → core → C API → GUI",
          "timestamp": "2026-09-13T12:02:01+08:00",
          "tree_id": "6875aee1957344381ca66a902bb0dc2ba20f8f02",
          "url": "https://github.com/LoveDaisy/ice_halo_sim/commit/22166140295c68e58e6375028394d4587a351c11"
        },
        "date": 1789273864943,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 83.5,
            "unit": "%"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 99.6,
            "unit": "%"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 94.6,
            "unit": "%"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 88.2,
            "unit": "%"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "682bf9aadbd77a61c2d7697ccd4b353bb70a06be",
          "message": "Merge pull request #358 from LoveDaisy/fix/equidistant-focal-length-factor-two\n\nfix(config): equidistant lens f→fov conversion was half the documented value",
          "timestamp": "2026-09-13T12:42:20+08:00",
          "tree_id": "ad66fbc16fa5d96511d2e7667a3348c6c69b86d5",
          "url": "https://github.com/LoveDaisy/ice_halo_sim/commit/682bf9aadbd77a61c2d7697ccd4b353bb70a06be"
        },
        "date": 1789275331756,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 79.8,
            "unit": "%"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 99.9,
            "unit": "%"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 94.5,
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
          "id": "b10d1bbc134afb64423fa842b7740de5a59934f2",
          "message": "Merge pull request #359 from LoveDaisy/feat/analysis-panel-polish\n\nfeat(gui): raypath analysis panel polish — first-picture gate, draw layer, geometry, thousands grouping, Export CSV",
          "timestamp": "2026-09-13T14:55:21+08:00",
          "tree_id": "ffbfcccfae868fd3b9504d3d98acd998d41a1933",
          "url": "https://github.com/LoveDaisy/ice_halo_sim/commit/b10d1bbc134afb64423fa842b7740de5a59934f2"
        },
        "date": 1789283180008,
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
            "value": 89.1,
            "unit": "%"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 90.4,
            "unit": "%"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "90d23cafd492e0f65663df5a681c439b0fa09f35",
          "message": "Merge pull request #360 from LoveDaisy/feat/analysis-standing-cpu-pool\n\nfeat(server): standing CPU analysis pool on the GPU route, woken by session kind",
          "timestamp": "2026-09-13T15:15:21+08:00",
          "tree_id": "0e81a696990d2128a156faf657717d6b2838d130",
          "url": "https://github.com/LoveDaisy/ice_halo_sim/commit/90d23cafd492e0f65663df5a681c439b0fa09f35"
        },
        "date": 1789284495186,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 63.2,
            "unit": "%"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 99.6,
            "unit": "%"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 95.3,
            "unit": "%"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 88.7,
            "unit": "%"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "0eba76808ec4e34d0e76e5c741347a0779db27c2",
          "message": "Merge pull request #361 from LoveDaisy/feat/panel-state-round-trip\n\nfeat(gui): panel-derived state round trip — analysis list freshness predicate, colour-ref layer re-indexing",
          "timestamp": "2026-09-13T15:37:56+08:00",
          "tree_id": "e7548f5a60a46f37b724f835b81f368a1b59d85a",
          "url": "https://github.com/LoveDaisy/ice_halo_sim/commit/0eba76808ec4e34d0e76e5c741347a0779db27c2"
        },
        "date": 1789285836317,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 70.8,
            "unit": "%"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 99.6,
            "unit": "%"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 94.6,
            "unit": "%"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 87.9,
            "unit": "%"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "673e55308cc451c53b25f590d5d48bf83f9ecf17",
          "message": "Merge pull request #362 from LoveDaisy/feat/angular-distance-section-merge\n\nfeat(gui): merge both angular-distance ring families into one collapsed Angular Distance section",
          "timestamp": "2026-09-13T16:21:39+08:00",
          "tree_id": "5b13f138562a66f363f9e17a9a4111ed6049f9d4",
          "url": "https://github.com/LoveDaisy/ice_halo_sim/commit/673e55308cc451c53b25f590d5d48bf83f9ecf17"
        },
        "date": 1789288420585,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 81,
            "unit": "%"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 99.7,
            "unit": "%"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 98.8,
            "unit": "%"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 89.4,
            "unit": "%"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "1a6c43be3f9fa9d8d89a3115d1fdf2f994e3a984",
          "message": "Merge pull request #363 from LoveDaisy/chore/angular-distance-from-naming\n\nfeat(gui): name the Angular Distance section \"from...\" and its rows Sun / Lens Center",
          "timestamp": "2026-09-13T20:15:08+08:00",
          "tree_id": "042b5e14f5fbae521e6d1e53edc4d0889e44bcf5",
          "url": "https://github.com/LoveDaisy/ice_halo_sim/commit/1a6c43be3f9fa9d8d89a3115d1fdf2f994e3a984"
        },
        "date": 1789302344891,
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
            "value": 89.7,
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
          "id": "6a6ae79565932a61955ad281237b6445b8a5d48f",
          "message": "Merge pull request #364 from LoveDaisy/chore/hide-ray-allocation-checkbox\n\nchore(gui): hide Adaptive ray allocation checkbox from the main panel",
          "timestamp": "2026-09-13T22:02:17+08:00",
          "tree_id": "53eedb96fe96c777dbf6f5fd7d8737f2e45e20f6",
          "url": "https://github.com/LoveDaisy/ice_halo_sim/commit/6a6ae79565932a61955ad281237b6445b8a5d48f"
        },
        "date": 1789308819353,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 79.4,
            "unit": "%"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 99.6,
            "unit": "%"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 92.8,
            "unit": "%"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 90.4,
            "unit": "%"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "19741f43a26b205cd9953af019cae4b44fd12984",
          "message": "Merge pull request #365 from LoveDaisy/feat/overlay-panel-ux\n\nfeat(gui): Overlay panel UX — Lens Center circle defaults and a Reference Points All row",
          "timestamp": "2026-09-14T00:15:21+08:00",
          "tree_id": "d43c7c2948629dec68903aa2f4de6ded9dcafbae",
          "url": "https://github.com/LoveDaisy/ice_halo_sim/commit/19741f43a26b205cd9953af019cae4b44fd12984"
        },
        "date": 1789316830599,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 76,
            "unit": "%"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 99.9,
            "unit": "%"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 96.2,
            "unit": "%"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 90.8,
            "unit": "%"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "c4288079e3f5670454e7cc8788194498a3f584f2",
          "message": "Merge pull request #366 from LoveDaisy/feat/analysis-session-ray-allocation\n\nfeat(analysis): analysis sessions follow scene.ray_allocation; drop the Rays column",
          "timestamp": "2026-09-14T00:38:07+08:00",
          "tree_id": "d76d462fb476b2cff0e3581ec03afa0c622be7a6",
          "url": "https://github.com/LoveDaisy/ice_halo_sim/commit/c4288079e3f5670454e7cc8788194498a3f584f2"
        },
        "date": 1789318086117,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 76.9,
            "unit": "%"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 99.5,
            "unit": "%"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 93.5,
            "unit": "%"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 88.3,
            "unit": "%"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "fd1bb1cffe3edfbbf7c4de2a1eff5ff0ecb5ea1a",
          "message": "Merge pull request #367 from LoveDaisy/chore/release-4.6.0\n\nchore(release): cut 4.6.0",
          "timestamp": "2026-09-14T01:21:05+08:00",
          "tree_id": "bd318927bfaf54e9b78b5aacd9ba6b7628b02941",
          "url": "https://github.com/LoveDaisy/ice_halo_sim/commit/fd1bb1cffe3edfbbf7c4de2a1eff5ff0ecb5ea1a"
        },
        "date": 1789320645631,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 73.1,
            "unit": "%"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 99.8,
            "unit": "%"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 94.4,
            "unit": "%"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 83.5,
            "unit": "%"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "7a9430ef19ad7197b2befd279263c2e4e3474f45",
          "message": "Merge pull request #368 from LoveDaisy/chore/cli-subcommands\n\ncli: split the flat flag set into render / benchmark subcommands",
          "timestamp": "2026-09-14T18:29:48+08:00",
          "tree_id": "5979324f4b5810dffb657260e392b8fab205f97d",
          "url": "https://github.com/LoveDaisy/ice_halo_sim/commit/7a9430ef19ad7197b2befd279263c2e4e3474f45"
        },
        "date": 1789382394087,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 73,
            "unit": "%"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 99.8,
            "unit": "%"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 92.3,
            "unit": "%"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 87.1,
            "unit": "%"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "9232bccb77567b58b5b9342c1049e69337b2fb44",
          "message": "Merge pull request #369 from LoveDaisy/feat/cli-raypath-analyze\n\ncli: add the `analyze` subcommand — raypath analysis from the command line",
          "timestamp": "2026-09-14T20:18:27+08:00",
          "tree_id": "aa2b67b0a92fe71b387a76367f305cdf54930982",
          "url": "https://github.com/LoveDaisy/ice_halo_sim/commit/9232bccb77567b58b5b9342c1049e69337b2fb44"
        },
        "date": 1789389141341,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 75.6,
            "unit": "%"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 99.6,
            "unit": "%"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 94.2,
            "unit": "%"
          },
          {
            "name": "Windows MSVC x86_64",
            "value": 89,
            "unit": "%"
          }
        ]
      },
      {
        "commit": {
          "author": {
            "email": "zhangjiajie043@gmail.com",
            "name": "Jiajie Zhang",
            "username": "LoveDaisy"
          },
          "committer": {
            "email": "noreply@github.com",
            "name": "GitHub",
            "username": "web-flow"
          },
          "distinct": true,
          "id": "f220e71f72ceab196f527085937d1268369d7e63",
          "message": "Merge pull request #370 from LoveDaisy/chore/cli-render-seed-and-small-fixes\n\nchore: render --seed, double emitted-energy accumulators, manual fixes, GUI log sink to stderr",
          "timestamp": "2026-09-16T08:42:53+08:00",
          "tree_id": "9ad81ba8b1b719f173f4198a5be2b649c860cde2",
          "url": "https://github.com/LoveDaisy/Lumice/commit/f220e71f72ceab196f527085937d1268369d7e63"
        },
        "date": 1789520234254,
        "tool": "customBiggerIsBetter",
        "benches": [
          {
            "name": "macOS ARM64",
            "value": 76.4,
            "unit": "%"
          },
          {
            "name": "Ubuntu ARM64",
            "value": 99.8,
            "unit": "%"
          },
          {
            "name": "Ubuntu x86_64",
            "value": 94.7,
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
      }
    ]
  }
}