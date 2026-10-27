"""Compare src/detile.h (tests/detile_test.c output) with galhook/detile/qcom_detile.py on a real frame.
   python detile_check.py frame.raw c_full.yuv c_band.yuv   (c_band = rows 296..752 only)"""
import sys, numpy as np
sys.path.insert(0, __import__('os').environ.get('QCOM_DETILE_DIR', '.'))
from qcom_detile import detile
raw, full, band = sys.argv[1:4]
W, H = 1920, 1080
Y, UV = detile(open(raw, 'rb').read(), W, H)
U, V = UV[:, 0::2], UV[:, 1::2]
c = np.frombuffer(open(full, 'rb').read(), np.uint8)
cY = c[:W*H].reshape(H, W); cU = c[W*H:W*H+W*H//4].reshape(H//2, W//2); cV = c[W*H+W*H//4:].reshape(H//2, W//2)
assert (cY == Y).all(), 'luma differs'
assert (cU == U).all() and (cV == V).all(), 'chroma differs'
b = np.frombuffer(open(band, 'rb').read(), np.uint8)
bY = b[:W*H].reshape(H, W); bU = b[W*H:W*H+W*H//4].reshape(H//2, W//2)
assert (bY[296:752] == Y[296:752]).all() and (bU[148:376] == U[148:376]).all(), 'row range differs'
assert not bY[:280].any() and not bY[768:].any(), 'rows outside the range written'
# run 135: the shown window only (rows 296..752, columns 240..1680 -> whole tiles 192..1728)
if len(sys.argv) > 4:
    v = np.frombuffer(open(sys.argv[4], 'rb').read(), np.uint8)
    vY = v[:W*H].reshape(H, W); vU = v[W*H:W*H+W*H//4].reshape(H//2, W//2); vV = v[W*H+W*H//4:].reshape(H//2, W//2)
    assert (vY[296:752, 240:1680] == Y[296:752, 240:1680]).all(), 'window luma differs'
    assert (vU[148:376, 120:840] == U[148:376, 120:840]).all() and (vV[148:376, 120:840] == V[148:376, 120:840]).all(), 'window chroma differs'
    assert not vY[296:752, :192].any() and not vY[296:752, 1728:].any(), 'columns outside the window written'
    print('PASS: window-only de-tile (rows 296..752, columns 240..1680) exact, nothing outside written')
print(f'PASS: detile.h == qcom_detile.py on the car frame (luma mean {Y.mean():.1f}); row range 296..752 exact')
