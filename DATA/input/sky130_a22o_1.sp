.SUBCKT a22o_1 A1 A2 B1 B2 VDD VSS X
MMPA0 pndA A1 VDD VDD pfet_01v8_hvt m=1 w=1.0 l=0.15
MMPA1 pndA A2 VDD VDD pfet_01v8_hvt m=1 w=1.0 l=0.15
MMPB0 y B1 pndA VDD pfet_01v8_hvt m=1 w=1.0 l=0.15
MMPB1 y B2 pndA VDD pfet_01v8_hvt m=1 w=1.0 l=0.15
MMIPX X y VDD VDD pfet_01v8_hvt m=1 w=1.0 l=0.15
MMNA0 y A1 sndA1 VSS nfet_01v8 m=1 w=0.65 l=0.15
MMNA1 sndA1 A2 VSS VSS nfet_01v8 m=1 w=0.65 l=0.15
MMNB0 y B1 sndB1 VSS nfet_01v8 m=1 w=0.65 l=0.15
MMNB1 sndB1 B2 VSS VSS nfet_01v8 m=1 w=0.65 l=0.15
MMINX X y VSS VSS nfet_01v8 m=1 w=0.65 l=0.15
.ENDS
