#include "fusb30x_global.h"

struct fusb30x_chip* fg_chip = NULL;  

struct fusb30x_chip* fusb30x_GetChip(void)
{
    return fg_chip;      
}

void fusb30x_SetChip(struct fusb30x_chip* newChip)
{
    fg_chip = newChip;   
    fg_chip->client->addr = fg_chip->client->addr >> 1;
	printk("FUSB [%s] chip->client addr = 0x%x\n", __func__, fg_chip->client->addr);
}
