/* Copyright (c) 2010 by Michael J. Roberts.  All Rights Reserved. */
/*
 *   TADS.as - Flash add-in for the TADS Web UI.  This provides access to
 *   some extended functionality that's not available in the standard
 *   Javascript API in Web browsers, but which Flash provides.
 *   
 *   Note that this Flash object is purely for programmatic use from
 *   Javascript, and is designed to be hidden.  This object has no visual
 *   presence or UI interaction.  
 *   
 *   Flash is available almost everywhere there's a browser, and it has
 *   extremely high uptake where it's available - it's almost as ubiquitous
 *   as the browser itself, which is why we use it for these extensions.
 *   However, it's important to keep in mind that Flash isn't available
 *   *everywhere*.  It's notably missing on Apple handheld platforms (iPhone,
 *   iPod, iPad), and on platforms even where it's available, a small
 *   percentage of users choose not to install it.  We therefore have to
 *   degrade gracefully when Flash isn't present.
 *   
 *   Our graceful degradation strategy has three prongs.  First, the object
 *   is designed to be hidden, and has no UI; this means there's be nothing
 *   visually missing when Flash isn't there.  Second, the functionality we
 *   implement through Flash has to be non-essential, so that the rest of the
 *   application will continue to work when this object is inactive.
 *   Javascript callers that invoke our API have to take care to handle
 *   errors when the object isn't active, and to provide reasonable defaults
 *   or alternative behavior.  Third, in cases where proprietary browser
 *   extensions or other means exist to access the same functionality in the
 *   absense of Flash, we'll consider implementing those alternatives.  
 */

package
{
    import flash.display.Sprite;
    import flash.external.ExternalInterface;
    import flash.text.Font;
    import flash.text.FontStyle;
    import flash.system.Security;

    /* 
     *   The main TADS class.  We're based on Sprite because we're designed
     *   to be embedded in an HTML page (via <OBJECT> or <EMBED>), but note
     *   that our embedded object is meant to be hidden, since it exists only
     *   for its programmatic interface.  
     */
    public class TADS extends Sprite
    {
        /* constructor */
        public function TADS()
        {
            /* 
             *   Set up our Javascript API.  Javascript can only call
             *   entrypoints that we explicitly export via addCallback().  
             */
            var app:TADS = this;
            ExternalInterface.addCallback(
                "getFonts", function():Array { return app.getFonts() });

            /* 
             *   Notify the host page that we've finished loading.  To do
             *   this, look for a parameter called "onload" - this contains
             *   the name of a host page function to call when loading is
             *   complete.  If the parameter is there, treat the string as a
             *   Javascript function name in the host page, and invoke it via
             *   the external host interface.  
             */
            var onload:String = this.loaderInfo.parameters["onload"];
            if (onload != null)
                ExternalInterface.call(onload);
        }

        /*
         *   Javascript callback: get a list of fonts installed on the
         *   system.  The browser javascript API has no way of getting this
         *   information, but Flash can.
         */
        public function getFonts():Array
        {
            /* 
             *   enumerate all fonts (embedded fonts and device fonts -
             *   device fonts are the system fonts that can be shown on the
             *   display, so they're really the ones we're after here) 
             */
            return Font.enumerateFonts(true);
        }
    }
}
