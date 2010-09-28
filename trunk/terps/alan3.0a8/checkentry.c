#include "checkentry.h"

#include "inter.h"
#include "lists.h"
#include "memory.h"


/*======================================================================*/
Bool checksFailed(Aaddr adr, Bool execute)
{
	CheckEntry *chk = (CheckEntry *) pointerTo(adr);
	if (chk->exp == 0) {
		if (execute == EXECUTE_CHECK_BODY_ON_FAIL)
			interpret(chk->stms);
		return TRUE;
	} else {
		while (!isEndOfArray(chk)) {
			if (!evaluate(chk->exp)) {
				if (execute == EXECUTE_CHECK_BODY_ON_FAIL)
					interpret(chk->stms);
				return TRUE;
			}
			chk++;
		}
		return FALSE;
	}
}
