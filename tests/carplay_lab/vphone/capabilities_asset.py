"""Read-only ASCII model of the recovered CarPlay asset selector.

This models the checked 23G90 and 24A5418b selection blocks, including their
per-property score reset. It reports possible winners instead of inventing an
NSDictionary/NSSet enumeration order. It does not emulate Foundation locale
rules or the manager's later preference/country reconciliation.
"""
from collections import defaultdict


def string_matches(left, right):
    if not isinstance(left, str) or not isinstance(right, str):
        return False
    if not left.isascii() or not right.isascii():
        raise ValueError("localized non-ASCII comparison is outside this model")
    if left.lower() == right.lower():
        return True
    wildcard = lambda s: len(s) >= 3 and s.startswith("*") and s.endswith("*")
    a, b = wildcard(left), wildcard(right)
    if a == b:
        return False
    pattern, value = (left, right) if a else (right, left)
    return pattern[1:-1].lower() in value.lower()


def audit_selection(asset, configuration):
    """Return all fully matching rows and all order-dependent possible winners.

    The native value index is shared across property names. Each search-hit
    bucket filters its candidates against the entire configuration, chooses
    its own highest score, then overwrites the shared selected row. A later
    bucket may therefore replace a more specific row from an earlier bucket.
    """
    properties, vehicles = asset["properties"], asset["vehicles"]
    index = defaultdict(set)
    for i, row in enumerate(vehicles):
        for key in properties:
            value = row.get(key)
            if value is not None:
                if not isinstance(value, str) or not value.isascii():
                    raise ValueError("asset contains an unsupported match value")
                index[value].add(i)

    predicates = [{k: v for k, v in row.items() if k != "features"}
                  for row in vehicles]
    matches = {i for i, terms in enumerate(predicates) if terms and all(
        key in configuration and string_matches(value, configuration[key])
        for key, value in terms.items())}
    buckets, possible = {}, set()
    for key in properties:
        if key not in configuration:
            continue
        candidates = set()
        for value, rows in index.items():
            if string_matches(value, configuration[key]):
                candidates.update(rows)
        satisfied = candidates & matches
        if not satisfied:
            continue
        highest = max(len(predicates[i]) for i in satisfied)
        winners = {i for i in satisfied if len(predicates[i]) == highest}
        buckets[key] = {"satisfied_rows": sorted(satisfied),
                        "local_score": highest, "possible_winners": sorted(winners)}
        possible.update(winners)
    return {"matching_rows": sorted(matches), "buckets": buckets,
            "possible_winners": sorted(possible),
            "default_features_used": not possible,
            "ambiguous_selection": len(possible) > 1,
            "scope": "ASCII selector model; native collection order unspecified"}
