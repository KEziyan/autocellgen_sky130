.SUBCKT nand2_1 A B VDD VSS Y
MMP0 Y A VDD VSS pfet_01v8_hvt m=1 w=1.0 l=0.15
MMP1 Y B VDD VSS pfet_01v8_hvt m=1 w=1.0 l=0.15
MMN0 Y A sndA VSS nfet_01v8 m=1 w=0.65 l=0.15
MMN1 sndA B VSS VSS nfet_01v8 m=1 w=0.65 l=0.15
.ENDS
