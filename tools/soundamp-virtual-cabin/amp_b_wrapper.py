"""124DF3 double-buffer reference under explicit on-time callback assumptions.

No queue/interrupt timing, live gate, ready state or algorithm is invented.
Completed-bank callback is supplied; its returned samples are already the
library's eight outputs (including whatever algorithm delay it implements).
"""
import copy
import hashlib
from pathlib import Path
import struct
import sys

import numpy as np

ROOT = Path(__file__).resolve().parents[2]
LOADER = ROOT/'output/r2/soundamp/J525_8W0035465_SW0420_SHARC_SPI_bitrev.ldr.bin'
RANGE = (0x124DF3, 0x124ED8)
RANGE_SHA256 = '0f527209f2f8f16396f935f00f56fd425c43da5dc1cc1437724de4ef8c8a56f4'


def wrapper_evidence(loader=LOADER):
    sys.path.insert(0, str(ROOT/'tools'))
    from extract_amp_sharc_app import BITREV_SHA256
    from inspect_amp_visa import load_shortwords, decode
    raw = Path(loader).read_bytes()
    if hashlib.sha256(raw).hexdigest() != BITREV_SHA256:
        raise ValueError('audited loader required')
    memory = load_shortwords(raw)
    a,b = RANGE
    sha = hashlib.sha256(struct.pack(f'<{b-a}H',*[memory[i] for i in range(a,b)])).hexdigest()
    if sha != RANGE_SHA256:
        raise ValueError('wrapper instructions changed')
    pins = {0x124E0C:'R1=0xa00', 0x124E0E:'R14=DM(+7,I5)',
            0x124E15:'R2=0xb95ea', 0x124E18:'R15=DM(+6,I5)',
            0x124E60:'I3=MODIFY(I3,+512)', 0x124E63:'R1=0xba9ea',
            0x124E69:'R0=LSHIFT R14 BY 12', 0x124EA5:'I4=MODIFY(I4,+512)',
            0x124EBC:'R13+=R15', 0x124EBD:'R2=LSHIFT R13 BY -9',
            0x124EC3:'DM(0xbc9ea)=R11', 0x124EC6:'DM(0xbc9eb)=R10',
            0x124EC9:'R2=R14+1 || R13=M5', 0x124ED3:'R2=FEXT R2 BY 0:1',
            0x124ED6:'DM(+7,I5)=R2'}
    for pc,wanted in pins.items():
        if decode(memory,pc)[1] != wanted:
            raise ValueError(f'wrapper binding changed at{pc:x}')
    return {'loader_sha256':BITREV_SHA256,'range_sha256':sha,
            'shortword_range':[hex(a),hex(b)],
            'instruction_pins':{hex(k):v for k,v in pins.items()},
            'input_base':'0xb95ea','output_base':'0xba9ea',
            'input_bank_words':2560,'output_bank_words':4096,'frames_per_bank':512,
            'banks':2,'steady_wrapper_delay_samples':1024,
            'boundary':'copy output before callback, alternate banks; on-time successful callbacks and explicit initial banks only; no live-state/queue proof'}


def _array(value, shape):
    if np.iscomplexobj(value):
        raise ValueError('real buffer required')
    x=np.array(value,dtype=float,copy=True)
    if x.shape!=shape or not np.isfinite(x).all():
        raise ValueError(f'explicit finite buffer{shape} required')
    return x


class BWrapperDoubleBufferReference:
    """Steady process-handler bank algebra, not full callback lifecycle.

    Both input/output banks and initial selector/offset must be supplied.
    Every call must fit within the current512-frame bank, as the firmware
    block copy does. Callback executes after output copying at each completed
    bank. It must be modelled as completed on time before the next boundary;
    overdue events, shared-pointer races and transition/error clearing are
    outside this reference and must not be silently assigned a fixed delay.
    """
    def __init__(self, *, callback, input_banks, output_banks, selector, offset,
                 assume_on_time_successful_callbacks):
        if not callable(callback) or assume_on_time_successful_callbacks is not True:
            raise ValueError('explicit callback and on-time successful-callback assumption required')
        if type(selector) is not int or selector not in (0,1) or type(offset) is not int or not 0<=offset<512:
            raise ValueError('explicit selector0/1 and bank offset0..511 required')
        self.callback=copy.deepcopy(callback)
        self.input_banks=_array(input_banks,(2,512,5))
        self.output_banks=_array(output_banks,(2,512,8))
        self.selector,self.offset=selector,offset
        self.completed_banks=0

    def process_block(self, input5):
        x=np.asarray(input5)
        if x.ndim!=2 or x.shape[1]!=5 or not 0<len(x)<=512-self.offset:
            raise ValueError('five lanes, nonempty block wholly within current bank required')
        x=_array(x,x.shape)
        bank,start=self.selector,self.offset;end=start+len(x)
        self.input_banks[bank,start:end]=x
        # This read precedes the completed-bank callback in124DF3.
        out=self.output_banks[bank,start:end].copy()
        self.offset=end
        if end==512:
            result=self.callback(self.input_banks[bank].copy())
            self.output_banks[bank]=_array(result,(512,8))
            self.selector^=1;self.offset=0;self.completed_banks+=1
        return out

    def metadata(self):
        return {'kind':'explicit_steady_first_B_double_buffer',
                'steady_wrapper_delay_samples':1024,
                'library_algorithm_delay_included':False,
                'live_state_proven':False,'on_time_callbacks_assumed':True,
                'boundary':'no interrupt/queue, readiness, transition or late-callback model'}
