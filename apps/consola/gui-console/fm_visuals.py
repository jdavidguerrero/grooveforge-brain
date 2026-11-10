"""Control-domain DX7 curves, checked against the compiled engine in test_dx7_wiring."""
_EXP_SCALE=(0,1,2,3,4,5,6,7,8,9,11,14,16,19,23,27,33,39,47,56,66,80,94,110,126,142,158,174,190,206,222,238,250)


def keyboard_scaling(note, breakpoint, left_depth, right_depth, left_curve, right_curve):
    offset=note-breakpoint-17
    depth,curve=(right_depth,right_curve) if offset>=0 else (left_depth,left_curve)
    group=abs(offset)//3
    scale=(group*depth*329)>>12 if curve in (0,3) else (_EXP_SCALE[min(group,32)]*depth*329)>>15
    return -scale if curve<2 else scale
