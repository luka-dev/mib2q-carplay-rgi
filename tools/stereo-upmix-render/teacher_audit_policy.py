"""Conservative review triage; numerical consistency is NOT semantic accuracy."""

def triage(record):
    flags=[]
    stems=record.get('stems',{});qa=record.get('qa',{})
    lead=stems.get('lead',{}).get('rms',0)
    back=stems.get('backing',{}).get('rms',0)
    if back>.005 and lead<back*.03:
        flags.append('Оценка лид-вокала очень тихая относительно вокального остатка. Есть ли здесь лид-партия? Вокальный сэмпл или текстура в остатке сами по себе не означают ошибку.')
    if qa.get('demucs_relative_residual_db',-100)>-18:
        flags.append('Заметный остаток Demucs: проверь пропавшие звуки и примеси.')
    if qa.get('drumsep_relative_residual_db',-100)>-18:
        flags.append('Заметный остаток DrumSep: проверь хэт, малый и тарелки отдельно.')
    if qa.get('vocal_teacher_disagreement_db',-100)>-10:
        flags.append('Учителя вокала расходятся: сравни обе вокальные дорожки.')
    return flags

def coverage_report(manifest):
    sources={};counts={};frames=0
    for row in manifest['items']:
        source=row['source_sha256'];sources.setdefault(source,[]).append(row)
        counts[row['partition']]=counts.get(row['partition'],0)+1
        if row.get('training_eligible') is not False:raise ValueError('unreviewed data must be quarantined')
    for source,rows in sources.items():
        if len({r['partition'] for r in rows})!=1:raise ValueError('source leaks between partitions')
        rows.sort(key=lambda r:r['core_start_seconds'])
        cursor=0
        for row in rows:
            start=round(row['core_start_seconds']*48000)
            if start!=cursor:raise ValueError('gap or overlap in source coverage')
            if row['core_frames']<=0 or row['core_offset_frames']+row['core_frames']>row['frames']:
                raise ValueError('invalid contextual crop')
            cursor+=row['core_frames']
        if cursor!=round(rows[0]['source_duration_seconds']*48000):raise ValueError('incomplete source coverage')
        frames+=cursor
    return dict(songs=len(sources),clips=sum(counts.values()),partition_clips=counts,
                unique_minutes=frames/48000/60,semantic_validation='pending',training_eligible=False)

def reviewed_candidates(items,annotations,required_stems):
    """Only exact audio/revision GOOD reviews qualify, and only per reviewed stem.

    These are candidates for a separate training admission decision, not a cache
    manifest and not automatically approved examples. Heldout never qualifies.
    """
    latest={(r['id'],r['stem'],r['revision']):r for r in annotations}
    selected=[]
    for item in items:
        if item['partition'] not in ('train','validation'):continue
        reviews=[latest.get((item['id'],stem,item['revision'])) for stem in required_stems]
        if not required_stems or not all(r and r.get('reviewer')=='human-local' and
            r.get('decision')=='good' and r.get('stem_sha256') and r.get('source_sha256') for r in reviews):continue
        selected.append(dict(id=item['id'],partition=item['partition'],revision=item['revision'],
            reviews=reviews,training_approved=False))
    return selected
