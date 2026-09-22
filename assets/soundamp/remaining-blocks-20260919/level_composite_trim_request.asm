0x000c53a0      63370100       st.w r6, 0[sp]
0x000c53a4      23ef0100       ld.w 0[sp], r29
0x000c53a8      633f0500       st.w r7, 4[sp]                          ; 4
0x000c53ac      23e70500       ld.w 4[sp], r28                         ; 4
0x000c53b0      0332           mov 3, r6
0x000c53b2      203e3b00       movea 59, r0, r7                        ; ';'
0x000c53b6      0040           mov r0, r8
0x000c53b8      0048           mov r0, r9
0x000c53ba      82ff364f       jarl 0xea2f0, lp
0x000c53be      3c570100       ld.w 0[r28], r10
0x000c53c2      0a7f0000       ld.b 0[r10], r15
0x000c53c6      e079           cmp r0, r15
0x000c53c8      aa15           bne 0xc53ec
0x000c53ca      3c870400       ld.h 4[r28], r16                        ; 4
0x000c53ce      200e1700       movea 23, r0, r1
0x000c53d2      e181           cmp r1, r16
0x000c53d4      920d           be 0xc53e6
0x000c53d6      1d30           mov r29, r6
0x000c53d8      053a           mov 5, r7
0x000c53da      bdff0e4a       jarl 0x99de8, lp
0x000c53de      0a58           mov r10, r11
0x000c53e0      0f5a           mov 15, r11
0x000c53e2      80072c03       jr 0xc570e
0x000c53e6      0038           mov r0, r7
0x000c53e8      0772           mov 7, r14
0x000c53ea      c54d           br 0xc5482
0x000c53ec      3c8f0100       ld.w 0[r28], r17
0x000c53f0      91970100       ld.bu 0[r17], r18
0x000c53f4      f491           cmp r20, r18
0x000c53f6      8a15           bne 0xc5416
0x000c53f8      3c9f0400       ld.h 4[r28], r19                        ; 4
0x000c53fc      659a           cmp 5, r19
0x000c53fe      920d           be 0xc5410
0x000c5400      1d30           mov r29, r6
0x000c5402      053a           mov 5, r7
0x000c5404      bdffe449       jarl 0x99de8, lp
0x000c5408      0a58           mov r10, r11
0x000c540a      0f5a           mov 15, r11
0x000c540c      80070203       jr 0xc570e
0x000c5410      0038           mov r0, r7
0x000c5412      0772           mov 7, r14
0x000c5414      f535           br 0xc5482
0x000c5416      3c7f0400       ld.h 4[r28], r15                        ; 4
0x000c541a      657a           cmp 5, r15
0x000c541c      920d           be 0xc542e
0x000c541e      1d30           mov r29, r6
0x000c5420      053a           mov 5, r7
0x000c5422      bdffc649       jarl 0x99de8, lp
0x000c5426      0a58           mov r10, r11
0x000c5428      0f5a           mov 15, r11
0x000c542a      8007e402       jr 0xc570e
0x000c542e      0060           mov r0, r12
0x000c5430      3c870100       ld.w 0[r28], r16
0x000c5434      908f0100       ld.bu 0[r16], r17
0x000c5438      400e0900       movhi 9, r0, r1
0x000c543c      81972d6e       ld.bu 28204[r1], r18                    ; 0x6e2c
0x000c5440      f289           cmp r18, r17
0x000c5442      e215           be 0xc546e
0x000c5444      6762           cmp 7, r12
0x000c5446      ce15           bge 0xc546e
0x000c5448      2b062c6e0900   mov 0x96e2c, r11
0x000c544e      0c9e0100       addi 1, r12, r19
0x000c5452      1360           mov r19, r12
0x000c5454      5461           and r20, r12
0x000c5456      0b78           mov r11, r15
0x000c5458      cc79           add r12, r15
0x000c545a      8f570100       ld.bu 0[r15], r10
0x000c545e      3c870100       ld.w 0[r28], r16
0x000c5462      908f0100       ld.bu 0[r16], r17
0x000c5466      f151           cmp r17, r10
0x000c5468      b205           be 0xc546e
0x000c546a      6762           cmp 7, r12
0x000c546c      96f5           blt 0xc544e
0x000c546e      6762           cmp 7, r12
0x000c5470      c605           blt 0xc5478
0x000c5472      0058           mov r0, r11
0x000c5474      80079a02       jr 0xc570e
0x000c5478      0c38           mov r12, r7
0x000c547a      07960100       addi 1, r7, r18
0x000c547e      1270           mov r18, r14
0x000c5480      5471           and r20, r14
0x000c5482      0242           mov 2, r8
0x000c5484      0858           mov r8, r11
0x000c5486      0768           mov r7, r13
0x000c5488      ee39           cmp r14, r7
0x000c548a      b105           bl 0xc5490
0x000c548c      80071401       jr 0xc55a0
0x000c5490      3c9f0100       ld.w 0[r28], r19
0x000c5494      937f0100       ld.bu 0[r19], r15
0x000c5498      f479           cmp r20, r15
0x000c549a      aa05           bne 0xc549e
0x000c549c      0858           mov r8, r11
0x000c549e      3c870100       ld.w 0[r28], r16
0x000c54a2      cb81           add r11, r16
0x000c54a4      90670100       ld.bu 0[r16], r12
0x000c54a8      ac00           sxb r12
0x000c54aa      200ee0ff       movea -32, r0, r1
0x000c54ae      e161           cmp r1, r12
0x000c54b0      d605           blt 0xc54ba
0x000c54b2      200e2000       movea 32, r0, r1
0x000c54b6      e161           cmp r1, r12
0x000c54b8      a71d           ble 0xc54ec
0x000c54ba      3c570100       ld.w 0[r28], r10
0x000c54be      cb51           add r11, r10
0x000c54c0      8a8f0100       ld.bu 0[r10], r17
0x000c54c4      200e8000       movea 128, r0, r1
0x000c54c8      e189           cmp r1, r17
0x000c54ca      9215           be 0xc54ec
0x000c54cc      1d30           mov r29, r6
0x000c54ce      0b960100       addi 1, r11, r18
0x000c54d2      5491           and r20, r18
0x000c54d4      1238           mov r18, r7
0x000c54d6      3c9f0100       ld.w 0[r28], r19
0x000c54da      cb99           add r11, r19
0x000c54dc      1340           mov r19, r8
0x000c54de      014a           mov 1, r9
0x000c54e0      bdffdc49       jarl 0x99ebc, lp
0x000c54e4      0a58           mov r10, r11
0x000c54e6      0f5a           mov 15, r11
0x000c54e8      80072602       jr 0xc570e
0x000c54ec      0b7e0100       addi 1, r11, r15
0x000c54f0      0f58           mov r15, r11
0x000c54f2      5459           and r20, r11
0x000c54f4      3c870100       ld.w 0[r28], r16
0x000c54f8      cb81           add r11, r16
0x000c54fa      90670100       ld.bu 0[r16], r12
0x000c54fe      ac00           sxb r12
0x000c5500      7762           cmp -9, r12
0x000c5502      b605           blt 0xc5508
0x000c5504      6962           cmp 9, r12
0x000c5506      a71d           ble 0xc553a
0x000c5508      3c8f0100       ld.w 0[r28], r17
0x000c550c      cb89           add r11, r17
0x000c550e      91970100       ld.bu 0[r17], r18
0x000c5512      200e8000       movea 128, r0, r1
0x000c5516      e191           cmp r1, r18
0x000c5518      9215           be 0xc553a
0x000c551a      1d30           mov r29, r6
0x000c551c      0b9e0100       addi 1, r11, r19
0x000c5520      5499           and r20, r19
0x000c5522      1338           mov r19, r7
0x000c5524      3c570100       ld.w 0[r28], r10
0x000c5528      cb51           add r11, r10
0x000c552a      0a40           mov r10, r8
0x000c552c      014a           mov 1, r9
0x000c552e      bdff8e49       jarl 0x99ebc, lp
0x000c5532      0a58           mov r10, r11
0x000c5534      0f5a           mov 15, r11
0x000c5536      8007d801       jr 0xc570e
0x000c553a      0b7e0100       addi 1, r11, r15
0x000c553e      0f58           mov r15, r11
0x000c5540      5459           and r20, r11
0x000c5542      3c870100       ld.w 0[r28], r16
0x000c5546      cb81           add r11, r16
0x000c5548      90670100       ld.bu 0[r16], r12
0x000c554c      ac00           sxb r12
0x000c554e      7762           cmp -9, r12
0x000c5550      b605           blt 0xc5556
0x000c5552      6962           cmp 9, r12
0x000c5554      a71d           ble 0xc5588
0x000c5556      3c8f0100       ld.w 0[r28], r17
0x000c555a      cb89           add r11, r17
0x000c555c      91970100       ld.bu 0[r17], r18
0x000c5560      200e8000       movea 128, r0, r1
0x000c5564      e191           cmp r1, r18
0x000c5566      9215           be 0xc5588
0x000c5568      1d30           mov r29, r6
0x000c556a      0b9e0100       addi 1, r11, r19
0x000c556e      5499           and r20, r19
0x000c5570      1338           mov r19, r7
0x000c5572      3c570100       ld.w 0[r28], r10
0x000c5576      cb51           add r11, r10
0x000c5578      0a40           mov r10, r8
0x000c557a      014a           mov 1, r9
0x000c557c      bdff4049       jarl 0x99ebc, lp
0x000c5580      0a58           mov r10, r11
0x000c5582      0f5a           mov 15, r11
0x000c5584      80078a01       jr 0xc570e
0x000c5588      0b7e0100       addi 1, r11, r15
0x000c558c      0f58           mov r15, r11
0x000c558e      5459           and r20, r11
0x000c5590      0d860100       addi 1, r13, r16
0x000c5594      1068           mov r16, r13
0x000c5596      5469           and r20, r13
0x000c5598      ee69           cmp r14, r13
0x000c559a      b905           bnl 0xc55a0
0x000c559c      bf07f4fe       jr 0xc5490
0x000c55a0      0858           mov r8, r11
0x000c55a2      20365a00       movea 90, r0, r6                        ; 'Z'
0x000c55a6      0768           mov r7, r13
0x000c55a8      ee39           cmp r14, r7
0x000c55aa      b105           bl 0xc55b0
0x000c55ac      80072e01       jr 0xc56da
0x000c55b0      3c8f0100       ld.w 0[r28], r17
0x000c55b4      91970100       ld.bu 0[r17], r18
0x000c55b8      f491           cmp r20, r18
0x000c55ba      aa05           bne 0xc55be
0x000c55bc      0858           mov r8, r11
0x000c55be      204e8000       movea 128, r0, r9
0x000c55c2      33062c6e0900   mov 0x96e2c, r19
0x000c55c8      cd99           add r13, r19
0x000c55ca      13570000       ld.b 0[r19], r10
0x000c55ce      0a56dfff       addi -33, r10, r10
0x000c55d2      0a60           mov r10, r12
0x000c55d4      5461           and r20, r12
0x000c55d6      3c7f0100       ld.w 0[r28], r15
0x000c55da      cb79           add r11, r15
0x000c55dc      8f870100       ld.bu 0[r15], r16
0x000c55e0      e981           cmp r9, r16
0x000c55e2      e215           be 0xc560e
0x000c55e4      24ee70c5       movea -14992, gp, r29
0x000c55e8      3c8f0100       ld.w 0[r28], r17
0x000c55ec      cb89           add r11, r17
0x000c55ee      91970100       ld.bu 0[r17], r18
0x000c55f2      b200           sxb r18
0x000c55f4      1238           mov r18, r7
0x000c55f6      1d98           mov r29, r19
0x000c55f8      cc99           add r12, r19
0x000c55fa      13570000       ld.b 0[r19], r10
0x000c55fe      ea39           cmp r10, r7
0x000c5600      b205           be 0xc5606
0x000c5602      2036a500       movea 165, r0, r6
0x000c5606      1d78           mov r29, r15
0x000c5608      cc79           add r12, r15
0x000c560a      4f3f0000       st.b r7, 0[r15]
0x000c560e      0b860100       addi 1, r11, r16
0x000c5612      1058           mov r16, r11
0x000c5614      5459           and r20, r11
0x000c5616      3c8f0100       ld.w 0[r28], r17
0x000c561a      cb89           add r11, r17
0x000c561c      91970100       ld.bu 0[r17], r18
0x000c5620      e991           cmp r9, r18
0x000c5622      e215           be 0xc564e
0x000c5624      24ee8dc5       movea -14963, gp, r29
0x000c5628      3c9f0100       ld.w 0[r28], r19
0x000c562c      cb99           add r11, r19
0x000c562e      93570100       ld.bu 0[r19], r10
0x000c5632      aa00           sxb r10
0x000c5634      0a38           mov r10, r7
0x000c5636      1d78           mov r29, r15
0x000c5638      cc79           add r12, r15
0x000c563a      0f870000       ld.b 0[r15], r16
0x000c563e      f039           cmp r16, r7
0x000c5640      b205           be 0xc5646
0x000c5642      2036a500       movea 165, r0, r6
0x000c5646      1d88           mov r29, r17
0x000c5648      cc89           add r12, r17
0x000c564a      513f0000       st.b r7, 0[r17]
0x000c564e      0b960100       addi 1, r11, r18
0x000c5652      1258           mov r18, r11
0x000c5654      5459           and r20, r11
0x000c5656      3c9f0100       ld.w 0[r28], r19
0x000c565a      cb99           add r11, r19
0x000c565c      93570100       ld.bu 0[r19], r10
0x000c5660      e951           cmp r9, r10
0x000c5662      e215           be 0xc568e
0x000c5664      24ee9fc5       movea -14945, gp, r29
0x000c5668      3c7f0100       ld.w 0[r28], r15
0x000c566c      cb79           add r11, r15
0x000c566e      8f870100       ld.bu 0[r15], r16
0x000c5672      b000           sxb r16
0x000c5674      1038           mov r16, r7
0x000c5676      1d88           mov r29, r17
0x000c5678      cc89           add r12, r17
0x000c567a      11970000       ld.b 0[r17], r18
0x000c567e      f239           cmp r18, r7
0x000c5680      b205           be 0xc5686
0x000c5682      2036a500       movea 165, r0, r6
0x000c5686      1d98           mov r29, r19
0x000c5688      cc99           add r12, r19
0x000c568a      533f0000       st.b r7, 0[r19]
0x000c568e      0b560100       addi 1, r11, r10
0x000c5692      0a58           mov r10, r11
0x000c5694      5459           and r20, r11
0x000c5696      a47f8dba       ld.bu -17779[gp], r15
0x000c569a      200ea500       movea 165, r0, r1
0x000c569e      e179           cmp r1, r15
0x000c56a0      d215           be 0xc56ca
0x000c56a2      248685c2       movea -15739, gp, r16
0x000c56a6      cc81           add r12, r16
0x000c56a8      248e8dc5       movea -14963, gp, r17
0x000c56ac      cc89           add r12, r17
0x000c56ae      11970000       ld.b 0[r17], r18
0x000c56b2      50970000       st.b r18, 0[r16]
0x000c56b6      249e72c2       movea -15758, gp, r19
0x000c56ba      cc99           add r12, r19
0x000c56bc      24569fc5       movea -14945, gp, r10
0x000c56c0      cc51           add r12, r10
0x000c56c2      0a7f0000       ld.b 0[r10], r15
0x000c56c6      537f0000       st.b r15, 0[r19]
0x000c56ca      0d8e0100       addi 1, r13, r17
0x000c56ce      1168           mov r17, r13
0x000c56d0      5469           and r20, r13
0x000c56d2      ee69           cmp r14, r13
0x000c56d4      b905           bnl 0xc56da
0x000c56d6      bf07dafe       jr 0xc55b0
0x000c56da      200ea500       movea 165, r0, r1
0x000c56de      e131           cmp r1, r6
0x000c56e0      ca0d           bne 0xc56f8
0x000c56e2      3c870100       ld.w 0[r28], r16
0x000c56e6      10970000       ld.b 0[r16], r18
0x000c56ea      449762f2       st.b r18, -3486[gp]
0x000c56ee      243694f1       movea -3692, gp, r6
0x000c56f2      013a           mov 1, r7
0x000c56f4      81ff1867       jarl 0xdbe0c, lp
0x000c56f8      80ffec11       jarl 0xc68e4, lp
0x000c56fc      0132           mov 1, r6
0x000c56fe      0b3a           mov 11, r7
0x000c5700      81ffd47b       jarl 0xdd2d4, lp
0x000c5704      20363100       movea 49, r0, r6                        ; '1'
0x000c5708      bfff88e2       jarl 0xc3990, lp
0x000c570c      0058           mov r0, r11
0x000c570e      0b50           mov r11, r10
0x000c5710      4406ff00       dispose 2, {r28, r29, lp}, lp
0x000c5714      8407e100       prepare {r28, r29, lp}, 2
0x000c5718      bf0788fc       jr 0xc53a0
