#import "GlkStream.h"
#include "glk.h" /* This comes with the IosGlk library. */
#include "glulxe.h"
#include "iosglk_startup.h" /* This comes with the IosGlk library. */

void iosglk_startup_code()
{
	NSBundle *bundle = [NSBundle mainBundle];
	NSString *pathname = [bundle pathForResource:@"Game" ofType:@"ulx"];
	
	gamefile = [[GlkStreamFile alloc] initWithMode:filemode_Read rock:1 unicode:NO textmode:NO dirname:@"." pathname:pathname];
}
