"""Differentiable emulation of HSSR's custom C GRU, not generic int8 GRU.

Forward quantizes weights/activations; straight-through gradients are a training
surrogate. CPU tests compare exported weights against the actual host C oracle.
"""
import torch
from torch.nn import functional as F

def ste(actual, surrogate):
    return surrogate+(actual-surrogate).detach()

def activation(x, multiplier):
    clipped=x.clamp(-127/multiplier,127/multiplier)
    rounded=torch.sign(clipped)*torch.floor(clipped.abs()*multiplier+.5)
    return ste(rounded/multiplier,clipped)

def weight(x):
    scale=(x.detach().abs().amax(dim=1,keepdim=True)/127).clamp_min(1e-8)
    return ste(torch.round(x/scale).clamp(-127,127)*scale,x)

def tanh(x):
    x=x.clamp(-3,3)
    return x*(27+x*x)/(27+9*x*x)

def sigmoid(x):
    return .5*(tanh(.5*x)+1)

def forward(model,x,state=None,return_embeddings=False):
    batch=x.shape[0];h=model.gru1.hidden_size
    h1,h2=(x.new_zeros(batch,h),x.new_zeros(batch,h)) if state is None else state
    layers=[model.gru1,model.gru2]
    weights=[(weight(g.weight_ih_l0),weight(g.weight_hh_l0)) for g in layers]
    head=weight(model.head.weight);outputs=[]
    for t in range(x.shape[1]):
        v=activation(x[:,t],127/6)
        states=[]
        for g,(wi,wh),hidden in zip(layers,weights,(h1,h2)):
            ir,iz,inn=F.linear(v,wi,g.bias_ih_l0).chunk(3,-1)
            hr,hz,hn=F.linear(activation(hidden,127),wh,g.bias_hh_l0).chunk(3,-1)
            reset=sigmoid(ir+hr);update=sigmoid(iz+hz)
            n=tanh(inn+reset*hn)
            hidden=(1-update)*n+update*hidden
            states.append(hidden);v=activation(hidden,127)
        h1,h2=states
        outputs.append(v if return_embeddings else F.linear(v,head,model.head.bias))
    return torch.stack(outputs,dim=1),(h1,h2)

def supervised_prediction(model,features,targets,burn_in=0,delay=0):
    state=None
    if burn_in:
        with torch.no_grad():
            _,state=model.forward_state(features[:,:burn_in])
        state=tuple(s.detach() for s in state)
    prediction,_=model.forward_state(features[:,burn_in:],state)
    target=targets[:,burn_in:]
    if delay:
        prediction=prediction[:,:-delay];target=target[:,delay:]
    return prediction,target
