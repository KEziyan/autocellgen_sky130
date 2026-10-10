.SUBCKT xor2_1 A B VDD VSS X
MMNnor0 inor A VSS VSS nfet_01v8 w=0.65 l=0.15
MMNnor1 inor B VSS VSS nfet_01v8 w=0.65 l=0.15
MMNaoi10 VSS A sndNA VSS nfet_01v8 w=0.65 l=0.15
MMNaoi11 sndNA B X VSS nfet_01v8 w=0.65 l=0.15
MMNaoi20 X inor VSS VSS nfet_01v8 w=0.65 l=0.15
MMPnor0 VDD A sndPA VDD pfet_01v8_hvt w=1.0 l=0.15
MMPnor1 sndPA B inor VDD pfet_01v8_hvt w=1.0 l=0.15
MMPaoi10 pmid A VDD VDD pfet_01v8_hvt w=1.0 l=0.15
MMPaoi11 pmid B VDD VDD pfet_01v8_hvt w=1.0 l=0.15
MMPaoi20 X inor pmid VDD pfet_01v8_hvt w=1.0 l=0.15
.ENDS xor2_1
