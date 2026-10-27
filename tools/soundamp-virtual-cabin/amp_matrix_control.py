"""Explicit V850 coefficient targets; no device I/O or inferred live preset."""


def pair_row_step(rows, *, level, processing_flag, b01_flag, state, current):
    """One AA644 byte-state update, not wall-clock ramp timing."""
    if len(rows)!=5 or any(len(r)!=20 for r in rows) or len(current)!=20:
        raise ValueError('five20-entry rows and prior20-entry state required')
    if any(type(v) is not int or not -100<=v<=20 for r in [*rows,current] for v in r):
        raise ValueError('audited -100..20 dB byte domain required')
    if type(level) is not int or level not in range(4):raise ValueError('level0..3 required')
    if any(type(v) is not int or not 0<=v<=255 for v in (processing_flag,b01_flag,state)):
        raise ValueError('explicit flag bytes required')
    row=level+1 if processing_flag==b01_flag==0xA5 else 0
    target=rows[row];out=list(current)
    if state==0x5A:out=list(target)
    elif state==0xAA:
        out=[v+(t>v)-(t<v) for v,t in zip(current,target)]
        if out==list(current):state=0x5A
    elif state==0xFF:out=[-100 if i%2 else v for i,v in enumerate(target)]
    return {'row':row,'state':state,'current':out}


def coefficient_targets(binding, *, matrix_codes, pair_codes, row, asad, processing_flag, coordinate):
    """After explicit row-state update: exact table lookup, ordered SHARC words.

    matrix_codes are raw RAM rows, now proven OUTPUT rows/INPUT columns.
    No implicit profile, matrix bank, UI label, event timing or audio B value.
    """
    if len(matrix_codes)!=8 or any(len(r)!=8 for r in matrix_codes) or len(pair_codes)!=20:
        raise ValueError('8x8 RAM matrix and20 current pair codes required')
    if any(type(v) is not int or not -100<=v<=20 for r in [*matrix_codes,pair_codes] for v in r):
        raise ValueError('audited signed gain codes -100..20 required')
    if type(coordinate) is not int or not -9<=coordinate<=9:raise ValueError('stored coordinate -9..9 required')
    if type(row) is not int or row not in range(5):raise ValueError('explicit row0..4 required')
    if any(type(v) is not int or not 0<=v<=255 for v in (asad,processing_flag)):
        raise ValueError('explicit control bytes required')
    table=lambda name:binding['tables'][name]['tenths_db'][9+coordinate]
    clamp=lambda x:max(-990,min(200,x))
    lookup=binding['gain_lookup']
    gain=lambda x:lookup['linear'][x-lookup['min_tenths']]
    matrix=[[0]*8 for _ in range(8)]
    for j in range(8):
        for i in range(8):
            correction=(table('matrix_input') if asad&15 and j>=4 else
                        table('matrix_output') if asad==0xC0 and processing_flag==0x5A and i>=4 else 0)
            matrix[i][j]=clamp(10*matrix_codes[i][j]+correction)
    pair=[clamp(10*v+table('pair_odd')) if row>0 and asad==0xC0 and k%2 else 10*v
          for k,v in enumerate(pair_codes)]
    return {'matrix_tenths_db':matrix,'matrix_linear':[[gain(v) for v in r] for r in matrix],
        'matrix_words':[gain(matrix[i][j]) for j in range(8) for i in range(8)],
        'pair_tenths_db':pair,'pair_words':[gain(v) for v in pair],
        'actual_vehicle_state_proven':False}
