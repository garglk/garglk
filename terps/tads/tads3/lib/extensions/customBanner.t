#charset "us-ascii"
#include <adv3.h>
#include <en_us.h>

  /*
   *    Custom Banner version 1.2
   *     by Eric Eve
   *  
   *     Version date: 13-Sep-06
   *
   *     This file implements a CustomBannerWindow class that vastly eases
   *     the process of setting up banners and displaying material in them.
   *     e.g. to set up a graphics banner to display pictures, starting with
   *     pic1.jpg at startup, but not appearing at all on an interpreter that
   *     can't display JPEGs you could define:
   *
   *     pictureWindow: CustomBanner
   *       canDisplay = (systemInfo(SysInfoJpeg))
   *       bannerArgs = [nil, BannerAfter,  statuslineBanner, BannerTypeText, 
   *             BannerAlignTop, 10, BannerSizeAbsolute, BannerStyleBorder]
   *       currentContents = '<img src="pic1.jpg">'
   *    ;
   *
   *    Then to change the picture dislayed at a later point, call:
   *
   *       pictureWindow.updateContents('<img src="pic2.jpg">');
   *
   *    And everything else, including getting everything right on RESTART, UNDO
   *    and RESTORE should be taken care of.
   */

  /* ------------------------------------------------------------------------ */
  /* 
   *  A CustomBannerWindow, like a BannerWindow, corrsponds to an on-screen
   *  banner. The purpose of CustomBannerWindow is to eliminate most of the
   *  busy-work that a game author would otherwise have to take care of in
   *  displaying and manipulating banners.
   *
   *  As with BannerWinnow, merely creating a CustomBannerWindow does not
   *  display the banner. However, any CustomBannerWindows in existence at
   *  the start of the game will be added to the screen display, unless the
   *  condition specified in their shouldDisplay() method prevents initialization.
   *
   *  The one property that must be defined on each instance of a CustomBannerWindow
   *  is bannerArgs, which takes the form:
   *
   *     bannerArgs = [parent, where, other, windowType, align, 
   *                     size, sizeUnits, styleFlags]
   *
   *  where each list element has the same meaning at the corresponding argument
   *  to BannerWindow.showBanner()
   *
   *  This merely ensures that the CustomBannerWindow is added to the screen's
   *  banner window layout. To have the CustomBannerWindow display some content
   *  when first added to the screen layout, override its current contents property:
   *
   *     currentContents = 'My initial contents'
   *
   *  To change what's displayed in a CustomBannerWindow from game code, call its
   *  updateContents() method, e.g.:
   *
   *     pictureWindow.updateContents('<img src="pics/troll.jpg">');
   *
   *  To redisplay the current contents, call the showCurrentContents() method.
   *  By default a call to updateContents() or showCurrentContents() clears the
   *  window before displaying the new content. To have the additional content
   *  added to the existing content, change the clearBeforeUpdate property to nil.
   *
   *  You can control whether the game uses this banner at all by overriding
   *  the canDisplay property. The main purpose of this property is to easily allow
   *  a game to run on different types of interpreter. For example, if your banner is
   *  meant to display pictures in the JPEG format, there's no point having it included
   *  in the screen layout of an interpreter that can't display JPEGs, and attempts to
   *  update its contents with a new picture should do nothing. In which case we could
   *  define:
   *
   *     canDisplay = (systemInfo(SysInfoJpeg))
   *
   *  Calls to CustomBannerWindow methods like updateContents() and clearWindow()
   *  should be safe on an interpreter for which shouldDisplay returns nil, since
   *  by default these will do nothing beyond updating the banner's currentContents
   *  property. This makes it easier to write game code that is suitable for all 
   *  classes of interpreter
   *
   *  To have a CustomBannerWindow resize to contents each time its contents are
   *  displayed, set its autoSize property to true.   
   *
   *  If you do not want a CustomBannerWindow you have defined not to be dispayed
   *  at game startup, set its isActive property to nil. Call the activate()
   *  method to add it to the screen layout and the deactivate() method to remove
   *  it, or any other CustomBannerWindow, from the screen display.    
   *    
   *  Obviously, it is then the game author's responsibility to ensure that no
   *  other banner window that's meant to persist after pictureWindow is deactivated
   *  depends on pictureWindow for its existence; i.e. that we're not deactivating
   *  the parent of some other banner window(s) we want to keep or subsequently
   *  activate, or the sibling of any banner window that's subsequently going to
   *  defined in relation to us.    
   */

class CustomBannerWindow: BannerWindow

 /*
  *  The list of any banner windows that must be set up before me,
  *  either one of them is my parent, or because I'm going
  *  to be placed before or after them with BannerBefore or BannerAfter.
  *
  *  If bannerArgs has been set up with the list of showBanner arguments, 
  *  then we can derive this information automatically
  */
  initBeforeMe()
  {
    /*
     *  If our bannerArgs property contains a list of the right length, i.e. 8
     *  elements, then the first and third elements of the list (our parent, and
     *  the sibling we're to be placed before or after) must be initialized before
     *  we are. If either of these is nil, no harm is done, since initBannerWindow()
     *  will simply skip the nil value.     
     *
     *  Moreover, if our sibling is in the list, we don't need our parent as well,
     *  since either our sibling or one of its siblings will initialize our parent.
     */
     
    local lst = [];
    
    if (propType(&bannerArgs) == TypeList && bannerArgs.length() == 8) 
      lst = bannerArgs[3] ? [bannerArgs[3]] : [bannerArgs[1]];
        
    initBeforeMe = lst;
    
    return lst;    
  }   

  
  /*
   * A condition to test whether this banner window should actually display.
   * Normally this would test for the interpreter class if this would
   * affect whether we wanted this banner to be created. For example, if
   * we were going to use this banner window to display a JPEG picture, we
   * might not this window to display at all if the interpreter we're running
   * on can't display JPEGS, so we might write:
   *
   *    canDisplay = (systemInfo(SysInfoJpeg))
   *
   *   If your complete system of CustomBanners depends on the same condition
   *   (e.g. you don't want any CustomBanners if the interpreter we're running
   *   on can't display JPEGs, then it's probably easiest to modify CustomBanner
   *   and override scanDisplay on the modified class.
   *
   *  By default, we simply check that the interpreter we're running on
   *  can display banners.
   */
  canDisplay = (systemInfo(SysInfoBanners))  
  
  shouldDisplay = (canDisplay && isActive)
   
   /*
    *  The standard use of initBannerWindow is first to ensure that any
    *  banner windows whose existence we presuppose have themselves been
    *  initialized, and then to set up our own window on screen.
    *  This function should be used for initializing banner window *layout*,
    *  not content.
    */
   
   
  initBannerWindow()
  {
     /*
      *  If we shouldn't display on this class of interpreter, don't
      *  initialize us.
      */
     if(!shouldDisplay)
       return nil;
       
     /*
      *  If we've already been initialized, there's nothing left to do.
      */
        
     if(inited_)
       return true;  
       
     /*  
      *  Initialize all the bannner windows on whose existence our own
      *  depends. If one of them can't be initialized, neither can we,
      *  in which case return nil to show that our initialization failed. 
      *  If, however, the parent or sibling banner window we want initialized
      *  before us is not a CustomBannerWindow, then its initBannerWindow()
      *  won't have a return value, in which case we ignore the fact that
      *  it returns nil 
      */
       
     foreach(local ban in initBeforeMe)
       if(ban && !ban.initBannerWindow() && ban.ofKind(CustomBannerWindow))
         return nil;
        
     /*  
      * Create my banner window on screen; if this fails return nil
      * to indicate that the window could not be created
      */       
      
      return (inited_ = initBannerLayout());

  }
  
  /*
   *  Initialize my onscreen layout, normally through a call to showBanner(),
   *  whose return value this method should return, e.g.:
   *
   *     initBannerLayout()
   *     {
   *         return showBanner(nil, BannerAfter,  statuslineBanner,
   *           BannerTypeText, BannerAlignTop, 1, BannerSizeAbsolute,
   *           BannerStyleBorder); 
   *     }
   *
   *    By default we simply call initBannerLayout() using our bannerArgs.
   */
  
  initBannerLayout()
  {
     return showBanner(bannerArgs...);
  }
  
   
  
  /* 
   * The list of args used to define our screen layout, as they would be passed
   * to showBanner. This is used both by initBannerLayout and initBeforeMe.
   *
   *  The args should be listed in the form
   *
   *  bannerArgs = [parent, where, other, windowType, align, size, sizeUnits, styleFlags]
   *
   *  e.g.
   *    bannerArgs = [nil, BannerAfter,  statuslineBanner,
   *           BannerTypeText, BannerAlignTop, 1, BannerSizeAbsolute,
   *           BannerStyleBorder]
   *   
   */
    
  bannerArgs = nil
  
  /*
   *  The current contents to be displayed in this window, which could be 
   *  a string of text, or the HTML string to display a picture.
   *
   *  currentContents can be overridden to hold the initial contents
   *  we want this banner to display, but it should not otherwise be
   *  directly written to in game code. To display new contents in the
   *  banner, use updateContents() instead.  
   */
  currentContents = ''
  
  /*
   *  Is this banner currently active? Set to nil if you don't want to this
   *  CustomBannerWindow to be active at startup; thereafter use the deactivate()
   *  and activate() methods   
   */
   
  isActive = true
  
  /*
   *  deactivate a currently active banner; this removes it from the screen
   *  and prevents writing anything further to it. Be careful to respect the
   *  dependency order of banner windows when activating and deactivating
   *
   *  The argument is optional. If it is the constant true then the currentContents
   *  will be set to an empty string (''). If it is a string, then the currentContents
   *  will be set to that string (ready to be displayed when the banner is reactivated).
   */
   
  deactivate([args])  
  {     
     removeBanner();
     isActive = nil;
     
     if(args.length > 0)
     {
        local arg = args[1];
        switch(dataType(arg))
        {
           case TypeTrue:
             currentContents = '';
             break;
           case TypeSString:
             currentContents = arg;
             break;
        }
     }
     
  }
   
  /* 
   *  Activate a currently inactive banner; this restores it to the screen. 
   *  The argument is optional; if present and true then activate(true)
   *  displays the current contents of the banner window after activating it.
   *  If the first argument is a string then the string is displayed in the banner.   
   */ 
  activate([args])
  {
      if(isActive)
         return;
         
      isActive = true;
      initBannerWindow();
      
      if(args.length() > 0 && args[1] != nil)
      {
         if(dataType(args[1]) == TypeSString)
            updateContents(args...);
         else  
            showCurrentContents();
      }
  }
   
  removeBanner()
  {
    /*
     *  If I'm removed I can't be inited_ any more, and I'll need to be regarded
     *  as not inited_ in the event of being redisplayed in the future.
     */
  
    inited_ = nil;
    
    inherited;
  } 
   
   
  /*
   * Set this flag to true to clear the contents of the window before displaying
   * the new contents, e.g. to display a new picture that replaces the old one.
   */
  clearBeforeUpdate =  true
  
  /* 
   *  Set this to true to have this banner size to contents each time its
   *  contents are displayed. Note that not all interpreters support the size to
   *  contents so you should still set an appropriate initial size, and, where
   *  appropriate, call setSize() with the isAdvisory flag set.
   */
    
  autoSize = nil
  
  
  /*
   *  Update the contents of this banner window. This is the method to
   *  call to change what a banner displays.   
   *
   *  The second argument is optional. If present it overrides the 
   *  setting of clearBeforeUpdate: updateContents(cont, true) will
   *  clear the banner before the update, whereas updateContents(cont, nil)
   *  will not, whatever the value of clearBeforeUpdate. 
   */
   
  updateContents(cont, [args])
  {
    /*
     *  Update our current contents. Note that this takes place even if
     *  shouldDisplay is nil, so that if, for example, we are updated on
     *  a text-only interpreter on which this banner is not displayed, 
     *  and the game is saved there and subsequently restored on a full HTML 
     *  interpreter in which we are displayed, the HTML interpreter will know 
     *  what contents it needs to display in us.
     */
    currentContents = cont;
    
    showCurrentContents(args...);
  }
  
  /* Show the current contents of this banner window */  
  
  showCurrentContents([args])
  { 
    local clr;
    if(args.length > 0)
      clr = (args[1] != nil);
    else
      clr = clearBeforeUpdate;
     
    if(clr)    
       clearWindow();
          
     writeToBanner(currentContents);      
     
     if(autoSize)
       sizeToContents();
  }
  
  
   /*  This is called on each CustomBannerWindow after a Restore. */   
   
  restoreBannerDisplay()
  {
        /* 
         * It's possible a game was saved in a text-mode terp and
         * restored in an HTML one. In which case we need to initialize
         * this banner before attempting to display anything
         */
         
         if(shouldDisplay && handle_ == nil)
         {
            if(!initBannerWindow())
              return;
         }
                  
        
        /* redisplay my contents after a restore */        
        showCurrentContents(); 
  }  
  
  /*
   *  Alternatively we might have been saved in a terp that does
   *  use this banner and restored in one that doesn't, in which
   *  case we should remove ourselves. This is called on each BannerWindow
   *  after a restore, but before bannerTracker.restoreDisplayState().
   */
     
  restoreRemove()
  {
     if(!shouldDisplay)        
        removeBanner();     
  }
  
  
  /* show my initial contents on startup */
       
  initBannerDisplay()  {  showCurrentContents();   }
  
  /*
   *   We provide overrides for all the various banner manipulation methods
   *   that game code might call, in order to make it safe to call them even
   *   our shouldDisplay method returns nil and we don't - or shouldn't - exist.
   *   For each of these xxxYyy methods we provide an altXxxyyy method that is
   *   called when shouldDisplay is nil (e.g. because we're using a window to
   *   display graphics on an interpreter that doesn't have graphics capablities).
   *   By default these altXxxYyy methods do nothing, which in many cases will
   *   be fine, but if you do want something else to happen you can override
   *   the appropriate altXxxYyy method accordingly (e.g. to show a message in
   *   the main game window instead of this banner). This should make it easier
   *   to structure the rest of your game code without needing to worry about
   *   what happens on interpreters which don't display your banners.
   */
   
  clearWindow()
  {
    if(shouldDisplay)
      inherited();
    else
      altClearWindow();
  }
  
  altClearWindow() { }
    
  /* write to me, but only if I should display */ 
   
  writeToBanner(txt)
  {  
    if(shouldDisplay)
      inherited(txt);
    else
      altWriteToBanner(txt);
  }
  
  /*
   *  altWriteToBanner(txt) is called when our game code tries to display
   *  something in this banner, but our shouldDisplay method has ruled out
   *  displaying this banner. In this case we might want to write something
   *  to the main display instead. By default we do nothing here, but
   *  individual instances and/or subclasses can override this method as
   *  required.
   */
  
  altWriteToBanner(txt) { }
  
  
  /* 
   *  We don't provide alternative methods for the setSize and sizeToContents
   *  methods, since there would almost certainly be nothing for them to do.
   *  We simply do nothing if shouldDisplay is nil.
   */
  
  setSize(size, sizeUnits, isAdvisory)
  {
       if(shouldDisplay)
         inherited(size, sizeUnits, isAdvisory);
  }
     
  sizeToContents()
  {
        /* size our system-level window to our contents */
       if(shouldDisplay)  
         bannerSizeToContents(handle_);
  }

  captureOutput(func)
  {
     if(shouldDisplay)
       inherited(func);
     else
       altCaptureOutput(func);  
  }
  
  /* Simply execute the callback without changing the output stream */
  
  altCaptureOutput(func) { (func)(); }
           
  setOutputStream()
     {
        if(shouldDisplay)
           /* set my stream as the default */
           return outputManager.setOutputStream(outputStream_);
        else
           return altSetOutputStream();          
     }
     
  /* 
   *  Our caller, or rather our caller's caller, will expect us to return
   *  the current output stream, which means we must be sure to do this
   *  whatever else we do.  
   */
         
  altSetOutputStream() { return outputManager.curOutputStream; }         
  
  flushBanner() 
  { 
    if(shouldDisplay)
      inherited();
    else  
      altFlushBanner();
  }
         
  altFlushBanner() { }       
           
  setTextColor(fg, bg)
  {
    if(shouldDisplay)
      inherited(fg, bg);
    else
      altSetTextColor(fg, bg);
  }         
          
  altSetTextColor(fg, bg) { }
  
  setScreenColor(color)
  {
    if(shouldDisplay)
      inherited(color);
    else
      altSetScreenColor(color);
  }
  
  altSetScreenColor(color) { }
  
  cursorTo(row, col)
  {
    if(shouldDisplay)
      inherited(row, col);
    else
      altCursorTo(row, col);
  }             
           
  /* 
   *  If this banner isn't displaying we can't do anything directly comparable
   *  to setting the cursot to a particular column and row in it, but we might
   *  want to do something else instead, like inserting so many blank lines in 
   *  the main window.            
   */       
  altCursorTo(row, col) { }         
;



 /*
  *  Initialize or reinitialize what all CustomBanners display at startup or
  *  after an UNDO
  */

customBannerInit: InitObject, PostUndoObject
  execBeforeMe =  [bannerInit]
  
  execute()
  {
     /* first ensure that all banner windows that need to exist do exist */
     
//     forEachInstance(CustomBannerWindow, new function(win) {
//        if(win.shouldDisplay && win.handle_ == nil)
//          win.initBannerWindow(); 
//     } );
  
     /* then show the current contents of every active banner */
  
     forEachInstance(CustomBannerWindow, {win: win.showCurrentContents() } );
  }
;

  /*
   *  Reinitialize what all the CustomBanners display on restoring. This requires
   *  a different procedure since we can't be sure that we're being restored on
   *  the same class of interpreter as we were saved on.
   */

customBannerRestore: PostRestoreObject
   execBeforeMe = [bannerTracker] 

   execute()
   {
   
     /*
      *  If we save in one terp, restore in the second terp, save in the second
      *  terp, then restore in the first terp, when different rules apply about
      *  displaying banners in the two terps, then windows removed in the second
      *  terp could still be marked as inited_ in the restore file that comes
      *  back to the first terp. To get round this, on restoration we ensure
      *  that each CustomBanner's inited_ property in fact corresponds to whether
      *  it has an active handle_, otherwise the attempt to reinitialize missing
      *  banners might fail.
      */
     forEachInstance(CustomBannerWindow, {win: win.inited_ = (win.handle_ != nil) } );
       
     forEachInstance(CustomBannerWindow, {win: win.restoreBannerDisplay() } );
   }

;

customBannerRestoreRemove: PostRestoreObject
  execAfterMe = [bannerTracker]

  execute()
  {
      forEachInstance(CustomBannerWindow, {win: win.restoreRemove() } );
  }
;

 /*
  *  If we display a menu then we need to remove any active banners from the
  *  screen before the menu displays and restore them to the screen on exiting
  *  from the menu
  */
modify MenuItem
  display()
  {
    /*
     *  First we store a list of all the banners that are currently
     *  displaying
     */
    local vec = new Vector(10);
     
    forEachInstance(CustomBannerWindow, new function(ban) {
        if(ban.shouldDisplay)
          vec.append(ban);  } );    
          
    /* deactive all active banners */      
          
    foreach(local ban in vec)
       ban.deactivate();
        
    try
    {
       /* carry out the inherited menu display */
       inherited();
    }  
      
   /* 
    *  Restore all the banners in our list of banners that were previously
    *  displayed. To ensure that they are activated in the right order
    *  we make what may be several passes through the list. On each pass
    *  we activate only those banners that don't depend on any inactive
    *  banners for their activation. Each time we activate a banner, we
    *  remove it from the list. On the next pass through the list any
    *  banners that depended on banners we have just activated may now themselves
    *  be activated, so we can carry on until every banner has been activated
    *  and removed from the list.  
    */  
   finally
   {
     while(vec.length())
     {
       local bannerRemoved = nil;
       
       foreach(local ban in vec)
       {

       
         if(ban.bannerArgs[1] != nil && ban.bannerArgs[1].handle_ == nil)
            continue;
           
         if(ban.bannerArgs[3] != nil && ban.bannerArgs[3].handle_ == nil)
            continue; 
             
         ban.activate(true);
         vec.removeElement(ban);
         bannerRemoved = true;      
       }  
       
       /*
        *  If we didn't remove any banners on this pass through, we're
        *  potentially in an infinite loop, so we'd better break out
        *  of it.
        */
       if(!bannerRemoved)
         break;
     }
      
    }            
  }
; 

