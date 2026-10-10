.SUBCKT nor2_1 A B VDD VSS Y
MMP0 sndPA A VDD VDD pfet_01v8_hvt w=1.0 l=0.15
MMP1 Y B sndPA VDD pfet_01v8_hvt w=1.0 l=0.15
MMN0 Y A VSS VSS nfet_01v8 w=0.65 l=0.15
MMN1 Y B VSS VSS nfet_01v8 w=0.65 l=0.15
.ENDS nor2_1
