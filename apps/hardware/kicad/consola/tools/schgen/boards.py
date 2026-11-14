"""Board registry shared by the generator and verifier; PCB-M remains the default."""

from .pcb_m import pcb_m
from .pcb_k import pcb_k_left, pcb_k_right
from .passive_boards import pcb_f, pcb_p

BOARDS = {'pcb-m': pcb_m, 'pcb-k-left': pcb_k_left, 'pcb-k-right': pcb_k_right,
          'pcb-f': pcb_f, 'pcb-p': pcb_p}
NEW_BOARDS = ('pcb-k-left', 'pcb-k-right', 'pcb-f', 'pcb-p')
