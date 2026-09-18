"""Read actual host-C input features through pipes, without a disk cache."""
import subprocess
import numpy as np


def c_features(oracle, model, pcm):
    if pcm.dtype!=np.dtype('<i2') or pcm.ndim!=2 or pcm.shape[1]!=2:
        raise ValueError('expected little-endian S16 stereo')
    result=subprocess.run([str(oracle.resolve()),str(model.resolve()),'--pcm','/dev/stdin','/dev/stdout'],
        input=pcm.tobytes(),stdout=subprocess.PIPE,stderr=subprocess.PIPE,check=True)
    features=np.frombuffer(result.stdout,dtype='<f4').reshape(-1,368)
    expected=max(0,(len(pcm)//128-3)//4)
    if len(features)!=expected or not np.isfinite(features).all():raise ValueError('invalid C feature stream')
    return features
