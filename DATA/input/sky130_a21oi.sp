.SUBCKT a21oi_1 A1 A2 B1 VDD VSS Y
MMPA0 pndA A1 VDD VSS pfet_01v8_hvt m=1 w=1.0 l=0.15
MMPA1 pndA A2 VDD VSS pfet_01v8_hvt m=1 w=1.0 l=0.15
MMPB0 Y B1 pndA VSS pfet_01v8_hvt m=1 w=1.0 l=0.15
MMNA0 Y A1 sndA1 VSS nfet_01v8 m=1 w=0.65 l=0.15
MMNA1 sndA1 A2 VSS VSS nfet_01v8 m=1 w=0.65 l=0.15
MMNB0 Y B1 VSS VSS nfet_01v8 m=1 w=0.65 l=0.15
.ENDS
