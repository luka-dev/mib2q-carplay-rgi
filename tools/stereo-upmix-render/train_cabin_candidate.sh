#!/usr/bin/env bash
# Local-only V11 warm-start pilot. No vehicle access or audio upload.
set -euo pipefail
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"
cd "$ROOT"
OUT="${1:-build/hssr-cabin-v12-a}"
PROFILE="${2:-balanced}"
RATE="${3:-0.00003}"
[[ ! -e "$OUT" ]] || { echo "Refusing to overwrite candidate: $OUT" >&2; exit 1; }
SOURCE=build/reference-audio/v12-training-source/manifest.json
TEACHER=build/reference-audio/v12-training-teacher
CACHE=build/reference-audio/v12-separator-cache
[[ -f "$SOURCE" ]] || { echo "Prepare the 43-song training manifest first" >&2; exit 1; }
TQDM_DISABLE=1 build/hssr-teacher-env/bin/python "$HERE/prepare_hssr_roformer_teacher.py" \
  --manifest "$SOURCE" --teacher-root "$TEACHER" --model-dir build/hssr-teacher-models
python3 "$HERE/prepare_hssr_separator_supervision.py" \
  --manifest "$SOURCE" --teacher-root "$TEACHER" --output "$CACHE" \
  --target-policy selection-v11 --include-vocal-negatives \
  --include-instrumental-positives --counterfactual-mixes --input-gains-db 0 -6 -12
python3 "$HERE/train_hssr.py" --cache-only --extra-cache-root "$CACHE" \
  --init-model models/hssr-selection-v11/hssr_guard.bin --output "$OUT" \
  --routing-target universal-v5 --loss-profile "$PROFILE" \
  --epochs 6 --batch-size 32 --learning-rate "$RATE"
python3 "$HERE/evaluate_hssr_level_validation.py" --cache-root "$CACHE" \
  --models models/hssr-selection-v11/hssr_guard.bin "$OUT/hssr_model.bin" \
  --output "$OUT/level-validation.json"
echo "Candidate trained; C-render/heldout/promotion review still required: $OUT"
