.SUBCKT nand3_1 A B C VDD VSS Y
MMP0 Y A VDD VDD pfet_01v8_hvt m=1 w=1.0 l=0.15
MMP1 Y B VDD VDD pfet_01v8_hvt m=1 w=1.0 l=0.15
MMP2 Y C VDD VDD pfet_01v8_hvt m=1 w=1.0 l=0.15
MMN0 Y A sndA VSS nfet_01v8 m=1 w=0.65 l=0.15
MMN1 sndA B sndB VSS nfet_01v8 m=1 w=0.65 l=0.15
MMN2 sndB C VSS VSS nfet_01v8 m=1 w=0.65 l=0.15
.ENDS
