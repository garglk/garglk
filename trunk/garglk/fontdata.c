/* include hex-dumped font files */

#include "lmr.hex"
#include "lmb.hex"
#include "lmi.hex"
#include "lmz.hex"

#include "cbr.hex"
#include "cbb.hex"
#include "cbi.hex"
#include "cbz.hex"

void gli_get_builtin_font(int idx, unsigned char **ptr, unsigned int *len)
{
	switch (idx)
	{
	case 0:
		*ptr = LuxiMonoRegular_pfb;
		*len = LuxiMonoRegular_pfb_len;
		break;
	case 1:
		*ptr = LuxiMonoBold_pfb;
		*len = LuxiMonoBold_pfb_len;
		break;
	case 2:
		*ptr = LuxiMonoOblique_pfb;
		*len = LuxiMonoOblique_pfb_len;
		break;
	case 3:
		*ptr = LuxiMonoBoldOblique_pfb;
		*len = LuxiMonoBoldOblique_pfb_len;
		break;

	case 4:
		*ptr = CharterBT_Roman_ttf;
		*len = CharterBT_Roman_ttf_len;
		break;
	case 5:
		*ptr = CharterBT_Bold_ttf;
		*len = CharterBT_Bold_ttf_len;
		break;
	case 6:
		*ptr = CharterBT_Italic_ttf;
		*len = CharterBT_Italic_ttf_len;
		break;
	case 7:
		*ptr = CharterBT_BoldItalic_ttf;
		*len = CharterBT_BoldItalic_ttf_len;
		break;

	default:
		*ptr = 0;
		len = 0;
	}
}

