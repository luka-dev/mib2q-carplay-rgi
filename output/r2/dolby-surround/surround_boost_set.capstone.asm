0x180004120: push      rbx
0x180004121: push      rsi
0x180004122: push      rbp
0x180004123: sub       rsp, 0x20
0x180004127: mov       rsi, rcx
0x18000412a: mov       ebx, edx
0x18000412c: lea       rbp, [rsi + 0x38]
0x180004130: mov       rcx, rbp
0x180004133: call      0x180010920
0x180004138: mov       eax, ebx
0x18000413a: mov       edx, 0x60
0x18000413f: sar       eax, 0x1f
0x180004142: pxor      xmm1, xmm1
0x180004146: not       eax
0x180004148: and       eax, ebx
0x18000414a: cmp       eax, 0x60
0x18000414d: movss     xmm0, dword ptr [rsi + 0x7ac]
0x180004155: cmovl     edx, eax
0x180004158: cvtsi2ss  xmm1, edx
0x18000415c: mulss     xmm1, dword ptr [rip + 0x2e45dc]
0x180004164: ucomiss   xmm0, xmm1
0x180004167: jp        0x18000416b
0x180004169: je        0x18000417d
0x18000416b: movss     dword ptr [rsi + 0x7ac], xmm1
0x180004173: mov       dword ptr [rsi + 0x13b0], 1
0x18000417d: mov       rcx, rbp
0x180004180: add       rsp, 0x20
0x180004184: pop       rbp
0x180004185: pop       rsi
0x180004186: pop       rbx
0x180004187: jmp       0x180010960
