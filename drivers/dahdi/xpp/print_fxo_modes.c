#include <stdio.h>

int main(int argc, char *argv[])
{
	size_t i;
	
	for (i=0; i<(sizeof(fxo_modes)/sizeof(struct fxo_mode)); i++) {
		if (fxo_modes[i].name == NULL) break;
		int reg16=0, reg26=0, reg30=0, reg31=0x20;
		char ring_osc[BUFSIZ]="", ring_x[BUFSIZ] = "";
		
		reg16 |= (fxo_modes[i].ohs << 6);
		reg16 |= (fxo_modes[i].rz << 1);
		reg16 |= (fxo_modes[i].rt);
		
		reg26 |= (fxo_modes[i].dcv << 6);
		reg26 |= (fxo_modes[i].mini << 4);
		reg26 |= (fxo_modes[i].ilim << 1);
		
		reg30 = (fxo_modes[i].acim);
		
		reg31 |= (fxo_modes[i].ohs2 << 3);

		if (fxo_modes[i].ring_osc)
			snprintf(ring_osc, BUFSIZ, "ring_osc=%04X", fxo_modes[i].ring_osc);
		if (fxo_modes[i].ring_x)
			snprintf(ring_x, BUFSIZ, "ring_x=%04X", fxo_modes[i].ring_x);
		printf("%-15s\treg16=%02X\treg26=%02X\treg30=%02X\treg31=%02X\t%s\t%s\n",
		       fxo_modes[i].name, reg16, reg26, reg30, reg31, ring_osc, ring_x);
	}
	return 0;
}
