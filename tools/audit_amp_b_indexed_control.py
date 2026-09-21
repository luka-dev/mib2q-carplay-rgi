"""Static extended B command FIFO. No command is sent to a device."""
import hashlib
import struct
from inspect_amp_visa import decode

PROOFS=[
 ('first_b_extended_queue_append',0x131A10,0x131A3E,'c70677f42c3167ac68b87d81e6037df9dedaacd383ed4a1b7a369a690ed5aed8'),
 ('first_b_extended_read_dispatch',0x131ACC,0x131CCA,'b4cbcebc12cfcc54eccc44e44e275306b664cae51507bc4c108c10a98d2b811c'),
 ('first_b_extended_write_validation',0x131D22,0x131E0E,'961a0c18282ba32d5f8ffbbcbd4352a77330dc2c3a9936bacad7fc28c3095ed7'),
 ('first_b_extended_special_validation',0x132034,0x1320CE,'f9759b2755ecf2d39f1ce4cede4d882e2224aea7ef0af06476d245a1e1f530de'),
 ('first_b_extended_queue_apply',0x1326C7,0x1328EA,'ea07551b37231a2e51db8b60a1c6555d1f8b3d3700c79029df56aaddcc65333e'),
 ('first_b_effective_config_store',0x132AEF,0x132B2B,'d3eab73249a205e096bc0f28e3209642566509fee85e4e06ed6759764a886dcd'),
 ('first_b_ready_core_getters',0x12DD67,0x12DD7D,'e52c735cfc2fcb9b8e644a655eeade444c8c76036c41cee9d5d2f994b93e80ab'),
]
RANGES={name:(a,b) for name,a,b,_ in PROOFS}
CALLER_PROOFS=[
 (0x131A65,0x131ACC,'8a728026f703b4afaebf54ea3017893b3d036d373a4e0106bde1ad94a8454a89'),
 (0x131CCA,0x131D22,'6028300513cf9fcc2bb92b0160a69eea5781bbe776dbd58e450687a5859ecfc6'),
 (0x12DE3B,0x12DE8F,'40ec64cc5108195c763ce662ebdacb194143071aeaa8cf048635271a0ea045b4'),
 (0x12DDE9,0x12DDEC,'a993d66f78683ddbc71d9053f5304d01444da75fb6939ad9b592537a91bc4078'),
]


def indexed_control(data,dm,mem):
    from audit_amp_output_bindings import SOURCE_SHA256,SOURCE_SIZE
    if len(data)!=SOURCE_SIZE or hashlib.sha256(data).hexdigest()!=SOURCE_SHA256:
        raise ValueError('wrong CodeFlash')
    for name,a,b,h in PROOFS:
        if hashlib.sha256(struct.pack(f'<{b-a}H',*[mem[p] for p in range(a,b)])).hexdigest()!=h:
            raise ValueError(f'indexed proof changed: {name}')
    for a,b,h in CALLER_PROOFS:
        if hashlib.sha256(struct.pack(f'<{b-a}H',*[mem[p] for p in range(a,b)])).hexdigest()!=h:
            raise ValueError(f'indexed caller proof changed: {a:x}')
    for pc,insn in {0x12DE66:'CJUMP 0x1326c7 (DB); R2=I6, I6=I7',
        0x131CD1:'IF NE JUMP 0x131d22',0x131A92:'R11=R9&R2',
        0x1327C6:'IF LE JUMP 0x132870 (DB)',0x1327CB:'DM(M4,I4)=R2',
        0x1327E2:'R2=DM(+1132,I5)',0x1327FD:'R2=DM(+1132,I5)',
        0x12DDE9:'R12=0xb89c0'}.items():
        if decode(mem,pc)[1]!=insn:raise ValueError(f'indexed caller changed: {pc:x}')
    if dm[0xB912F]!=0x132072:raise ValueError('source-store validator changed')
    return {
        'source_sha256':SOURCE_SHA256,'proofs':[{ 'name':n,'start':hex(a),'end':hex(b),'sha256':h} for n,a,b,h in PROOFS],
        'queue':{'base':'0xb909c','count':'0xb911c','normal_pending_flag':'0xb911d',
            'capacity':64,'record_words':2,'append':'131A19 copies command and raw payload word; caller retains no payload pointer',
            'validation':'canonical extended writes require ready1, nonnull payload pointer, count<64; high command bits distinguish extended dispatch, not queue empty/nonempty',
            'apply_order':'12DE66 before audio: normal pending16..29 first; then extended FIFO only if pending result is0; finally clear count and pending flag even when skipped',
            'boundary':'invalid/overflow logging return policy and concurrent producer timing not fully emulated'},
        'canonical_writes':{
            '0x14000000|index':{'index_min':1,'index_max':842,'payload':'raw32, no per-parameter float validator',
                'writes':['object+299+index','DM(object+1180)+index'],
                'index_le_829':'rebase descriptor44..58 bases only; no replay, mode helper or publication here',
                'index_gt_829':'replay132168; helper12B348(api20,value=object1132); helper12B348(api16,value=object1132 AGAIN); bank replace132346(value=object1129)',
                'boundary_store':'second store1327CB is a delay slot of1327C6 and runs for BOTH branches',
                'defer':'does not consult object1178; does not update shadow controls',
                'overlap':[{'index':829+i,'object_offset':1128+i,'api':16+i,
                    'target_address_in_normal_init':hex(0xB89C0+829+i)} for i in range(14)]},
            '0x12000000|index':{'index_min':4,'index_max':206,'writes':['object+92+index'],
                'effect':'raw core parameter write only; no rebase/replay/publish/defer check'},
            '0x10000000':{'payload_raw':[0,1,2],'writes':['object1178'],
                'effect':'sets future normal-control defer behavior; no automatic shadow replay here'},
            '0x20000003':{'payload_raw':[1,2,3,4],'effect':'CRC effective config299 words0..827 into828; copy829 words to source1179+(bank-1)*829',
                'destination':'SHARC source RAM; no flash write, target publish or bank-selector change here'}},
        'read_domains':{'0x0c000000|index':[0,857],'0x0a000000|index':[0,206],'0x09000000|index':[0,32],
            'bases':'object+299, object+92, object+59 respectively; reads are immediate, not FIFO writes',
            'pointer_boundary':'widest read ends atobject1156; indexed writes end1141, below pointer table1156..1173'},
        'important_boundaries':['normal target B89C0 +829 is B8CFD, start of interpolation-step array; do not truncate or silently clamp extended writes to829 words',
            'API16 helper really receives effective API20 word, not effective API16; stored mode words are not changed by pointer helper',
            'normal retained API20 values1/3 select API16 default pointer mapping in this branch; this is not a proof that the branch executes in the vehicle',
            'no normal V850 sender of these extended command headers has been bound; no live-state or bass-cause inference'],
    }
