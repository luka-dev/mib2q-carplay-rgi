"""Static first-B callback/control reference, NOT an event scheduler.

124F56 callback,124D64 alternate handler and the successful12DE3B transition
path. Calls are explicit events. No on-time callback, current handler/readiness,
audio-bank selection or undocumented library reset is inferred.
"""
import argparse
from dataclasses import dataclass
import hashlib
import json
from pathlib import Path
import struct
import sys

import numpy as np

from amp_b_wrapper import LOADER,ROOT,_array


def _word(value,name):
    if type(value) is not int or not 0<=value<=0xffffffff:
        raise ValueError(f'explicit uint32 {name} required')
    return value


def callback_error_status(status):
    """124F8B/91 etc: BTGL result zero compares the ENTIRE return word."""
    return _word(status,'callee status') in (0x4000,0x8000)


@dataclass
class WrapperCallbackControl:
    """One synchronous invocation of124F56; caller decides WHEN it occurs.

External init/parameter/process return words must be supplied only for the
calls reached. State/busy/error correspond toB95E2/B95E3/B95E4. busy=1 on
entry and0 on every represented return. A clear action means4096 words at
the currently published BC9EB, not both banks or already copied graph audio.
"""
    state:int
    busy:int
    error:int

    def __post_init__(self):
        for name in ('state','busy','error'):_word(getattr(self,name),name)

    def invoke(self, *, inhibit_word, process_status=None, initialize_status=None,
               parameter_status=None):
        inhibit_word=_word(inhibit_word,'BC9EC inhibit word')
        reached=[];clear=False;new_state=self.state;new_error=self.error
        if self.state==1:
            if process_status is not None:raise ValueError('process is not reached in initialization state')
            failed=callback_error_status(initialize_status)
            reached=['clear4096','initialize_12DD7D']
            clear=True
            if failed:
                if parameter_status is not None:raise ValueError('parameter call not reached after init failure')
                new_state,new_error=1,2
            else:
                reached.append('parameter_131A65_0800AFFE')
                if callback_error_status(parameter_status):new_state,new_error=1,1
                else:new_state,new_error=3,0
        elif self.state==3 and inhibit_word!=1:
            if initialize_status is not None or parameter_status is not None:
                raise ValueError('only processing reached in state3 without exact inhibit1')
            reached=['process_12DE3B']
            if callback_error_status(process_status):
                new_error=3;clear=True;reached.append('clear4096')
            else:new_error=0
        else:
            if any(v is not None for v in (process_status,initialize_status,parameter_status)):
                raise ValueError('no library call reached on this clearing branch')
            clear=True;reached=['clear4096']
        previous={'state':self.state,'busy':self.busy,'error':self.error}
        # Validation is complete before mutating reference state.
        self.busy=1
        self.state,self.error=new_state,new_error
        self.busy=0
        return {'before':previous,'after':{'state':self.state,'busy':self.busy,'error':self.error},
                'actions':['busy1',*reached,'busy0'],
                'output_action':'clear_published_bank' if clear else 'keep_library_result',
                'clear_output_words':4096 if clear else 0,
                'input_cleared_by_callback':False,
                'no_scheduler_or_live_state_inferred':True}


def alternate_stereo_fanout(input5):
    """124D64: input0/1 copied to FOUR stereo pairs; ignores other3 inputs."""
    x=np.asarray(input5)
    if x.ndim!=2 or x.shape[1]!=5:raise ValueError('frames x5 input required')
    x=_array(x,x.shape)
    return np.tile(x[:,:2],(1,4))


class SuccessfulLibraryTransition:
    """Successful12DE3B path, fixed512-frame calls and explicit transition state.

    The supplied algorithm represents pre-conversion→dispatcher→post-conversion.
    All these calls must complete successfully;
no concurrent parameter/state writes are modelled. This contract intentionally
does not invent pending-control1326C7 behavior, reset internals, or recovery.
Algorithm gets ZERO inputs during exactB909B==1 but MUST STILL RUN, advancing
    its own supplied history. Output is cleared afterward, at the12DF07 boundary
    BEFORE optional object+1176 output-mode processing/reset. End bookkeeping
follows only after successful processing. Counter comparison is==10, NOT>=10.
"""
    def __init__(self, *, transition_word, counter, ready_word,
                 assume_successful_calls_and_no_intervening_writes):
        self.transition=_word(transition_word,'B909B transition')
        self.counter=_word(counter,'B89BF counter')
        if _word(ready_word,'B89BA ready')!=1:
            raise ValueError('successful-library reference requires ready word exactly1')
        if assume_successful_calls_and_no_intervening_writes is not True:
            raise ValueError('explicit successful-calls/no-intervening-writes assumption required')
        self.calls=0

    def process_bank(self, input5, algorithm):
        if not callable(algorithm):raise ValueError('explicit stateful library algorithm required')
        work=_array(input5,(512,5));active=self.transition==1
        events=[]
        if active:
            self.counter=(self.counter+1)&0xffffffff
            work.fill(0);events+=['increment_counter','clear5_inputs']
        events.append('run_supplied_algorithm')
        # Exceptions/invalid outputs are outside the successful-path contract;
        # the preceding input clear/counter increment is intentionally not undone.
        output=_array(algorithm(work.copy()),(512,8))
        if active:
            output.fill(0);events.append('clear8_outputs')
            if self.counter==10:
                self.transition=0;self.counter=0
                events+=['clear_transition_1319FE','clear_counter']
        self.calls+=1
        return {'audio_at_12DF07':output,'library_input':work,'actions':events,
                'transition_word':self.transition,'counter':self.counter,
                'algorithm_state_reset_inferred':False,
                'boundary':'before optional object+1176 output-mode processing and reset; NOT final callback audio'}


def control_evidence(loader=LOADER):
    sys.path.insert(0,str(ROOT/'tools'))
    from extract_amp_sharc_app import BITREV_SHA256
    from inspect_amp_visa import load_shortwords,decode,listing
    raw=Path(loader).read_bytes()
    if hashlib.sha256(raw).hexdigest()!=BITREV_SHA256:raise ValueError('audited loader required')
    memory=load_shortwords(raw)
    ranges={}
    for name,start,end in [('alternate',0x124D64,0x124DF3),('callback',0x124F56,0x12504D),
                           ('library_process',0x12DE3B,0x12DFA8),('transition_access',0x1319FE,0x131A10)]:
        lines=list(listing(memory,start,end))
        if any('UNKNOWN' in line for line in lines):raise ValueError('unknown control instruction')
        ranges[name]={'start_shortword':hex(start),'end_exclusive_shortword':hex(end),
                      'instructions':len(lines),'raw_shortword_sha256':hashlib.sha256(
                          struct.pack(f'<{end-start}H',*[memory[i] for i in range(start,end)])).hexdigest()}
    pins={0x124F61:'DM(0xb95e3)=M6',0x124F73:'R2=DM(0xbc9ec)',
          0x124F76:'R2=BTGL R2 BY 0',0x124F8B:'R2=BTGL R0 BY 14',
          0x124F91:'R2=BTGL R0 BY 15',0x124F9A:'DM(0xb95e3)=M5',
          0x124FAA:'R4=DM(0xbc9eb)',0x124FAD:'R12=0x1000',
          0x125025:'IF NOT SZ JUMP 0x125031',
          0x125034:'JUMP 0x124f9a (DB)',0x125037:'I4=0x3',
          0x125039:'DM(0xb95e2)=I4',0x12DE98:'R2=BTGL R13 BY 0',
          0x12DF79:'R2=0xa',0x12DF7E:'COMP(R1,R2)',
          0x12DF8C:'DM(0xb89bf)=M5',0x1319FE:'DM(0xb909b)=M5'}
    for pc,wanted in pins.items():
        if decode(memory,pc)[1]!=wanted:raise ValueError(f'control pin differs at{pc:x}')
    return {'loader_sha256':BITREV_SHA256,'ranges':ranges,
            'pins':{hex(pc):v for pc,v in pins.items()},
            'scope':'explicit callback events and successful-library transition algebra; not scheduler/live-state proof'}


def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--output',type=Path)
    args=parser.parse_args();report=control_evidence()
    report['callback_cases']=[]
    for label,state,error,inhibit,statuses in [
        ('inactive',0,7,0,{}),('inhibit_exact1',3,7,1,{}),
        ('word3_is_not_inhibit',3,7,3,{'process_status':0}),
        ('process_error4000',3,7,0,{'process_status':0x4000}),
        ('statusC000_is_not_exact4000_or8000',3,7,0,{'process_status':0xc000}),
        ('init_error',1,7,0,{'initialize_status':0x8000}),
        ('post_init_parameter_error',1,7,0,{'initialize_status':0,'parameter_status':0x4000}),
        ('init_success',1,7,0,{'initialize_status':0,'parameter_status':0})]:
        report['callback_cases'].append({'label':label,**WrapperCallbackControl(state,0,error).invoke(
            inhibit_word=inhibit,**statuses)})
    report['transition_trace']=[]
    transition=SuccessfulLibraryTransition(transition_word=1,counter=0,ready_word=1,
        assume_successful_calls_and_no_intervening_writes=True)
    for i in range(11):
        r=transition.process_bank(np.ones((512,5)),lambda _:np.ones((512,8)))
        report['transition_trace'].append({'call':i+1,'counter':r['counter'],'transition_word':r['transition_word'],
                                           'input_peak':float(abs(r['library_input']).max()),
                                           'output_peak_at_12DF07':float(abs(r['audio_at_12DF07']).max()),'actions':r['actions']})
    encoded=json.dumps(report,indent=2)+'\n'
    if args.output:
        args.output.parent.mkdir(parents=True,exist_ok=True);args.output.write_text(encoded);print(args.output)
    else:print(encoded,end='')


if __name__=='__main__':main()
