/************************************************************************ 
 * This file is part of PDCurses. PDCurses is public domain software;	*
 * you may use it for any purpose. This software is provided AS IS with	*
 * NO WARRANTY whatsoever.						*
 *									*
 * If you use PDCurses in an application, an acknowledgement would be	*
 * appreciated, but is not mandatory. If you make corrections or	*
 * enhancements to PDCurses, please forward them to the current		*
 * maintainer for the benefit of other users.				*
 *									*
 * No distribution of modified PDCurses code may be made under the name	*
 * "PDCurses", except by the current maintainer. (Although PDCurses is	*
 * public domain, the name is a trademark.)				*
 *									*
 * See the file maintain.er for details of the current maintainer.	*
 ************************************************************************/

#define CURSES_LIBRARY 1
#include <curses.h>
#include <stdlib.h>
#include <string.h>

/* undefine any macros for functions defined in this module */
#undef refresh
#undef wrefresh
#undef wnoutrefresh
#undef doupdate
#undef redrawwin
#undef wredrawln

/* undefine any macros for functions called by this module if in debug mode */
#ifdef PDCDEBUG
# undef wattrset
# undef mvwprintw
# undef wmove
# undef wattrset
# undef touchwin
# undef reset_prog_mode
#endif

#ifdef PDCDEBUG
const char *rcsid_refresh =
	"$Id: refresh.c,v 1.26 2006/03/27 14:07:21 wmcbrine Exp $";
#endif

/*man-start**************************************************************

  Name:                                                       refresh

  Synopsis:
	int refresh(void);
	int wrefresh(WINDOW *win);
	int wnoutrefresh(WINDOW *win);
	int doupdate(void);
	int redrawwin(WINDOW *win);
	int wredrawln(WINDOW *win, int beg_line, int num_lines);

  X/Open Description:
	The routine wrefresh() copies the named window to the physical 
	terminal screen, taking into account what is already there in 
	order to optimize cursor movement. The routine refresh() does 
	the same, using stdscr as a default screen. These routines must 
	be called to get any output on the terminal, as other routines 
	only manipulate data structures. Unless leaveok has been 
	enabled, the physical cursor of the terminal is left at the 
	location of the window's cursor.

	The wnoutrefresh() and doupdate() routines allow multiple 
	updates with more efficiency than wrefresh() alone.  In addition 
	to all of the window structures representing the terminal 
	screen: a physical screen, describing what is actually on the 
	screen and a virtual screen, describing what the programmer 
	wants to have on the screen.

	The wrefresh() function works by first calling wnoutrefresh(), 
	which copies the named window to the virtual screen.  It then 
	calls doupdate(), which compares the virtual screen to the 
	physical screen and does the actual update.  If the programmer 
	wishes to output several windows at once, a series of cals to 
	wrefresh() will result in alternating calls to wnoutrefresh() 
	and doupdate(), causing several bursts of output to the screen.  
	By first calling wnoutrefresh() for each window, it is then 
	possible to call doupdate() once.  This results in only one 
	burst of output, with probably fewer total characters 
	transmitted and certainly less CPU time used.

  X/Open Return Value:
	All functions return OK on success and ERR on error.

  X/Open Errors:
	No errors are defined for this function.

  Portability				     X/Open    BSD    SYS V
					     Dec '88
	refresh					Y	Y	Y
	wrefresh				Y	Y	Y
	wnoutrefresh				Y	Y	Y
	doupdate				Y	Y	Y
	redrawwin				-	-      4.0
	wredrawln				-	-      4.0

**man-end****************************************************************/

int refresh(void)
{
	PDC_LOG(("refresh() - called\n"));

	return wrefresh(stdscr);
}

int wrefresh(WINDOW *win)
{
	bool save_clear;

	PDC_LOG(("wrefresh() - called\n"));

	if ( (win == (WINDOW *)NULL) || (win->_flags & (_PAD|_SUBPAD)) )
		return ERR;

	save_clear = win->_clear;

	if (win == curscr)
		curscr->_clear = TRUE;
	else
		wnoutrefresh(win);

	if (save_clear && win->_maxy == SP->lines && win->_maxx == SP->cols)
		curscr->_clear = TRUE;

	doupdate();
	return OK;
}

int wnoutrefresh(WINDOW *win)
{
	int first;		/* first changed char on line */
	int last;		/* last changed char on line  */
	int begy, begx;		/* window's place on screen   */
	int i, j;

	PDC_LOG(("wnoutrefresh() - called: win=%x\n", win));

	if ( (win == (WINDOW *)NULL) || (win->_flags & (_PAD|_SUBPAD)) )
		return ERR;

	begy = win->_begy;
	begx = win->_begx;

	for (i = 0, j = begy; i < win->_maxy; i++, j++)
	{
		if (win->_firstch[i] != _NO_CHANGE)
		{
			first = win->_firstch[i];
			last = win->_lastch[i];

			memcpy(&(curscr->_y[j][begx + first]),
				&(win->_y[i][first]),
				(last - first + 1) * sizeof(chtype));

			first += begx; 
			last += begx;

			if (curscr->_firstch[j] != _NO_CHANGE)
				curscr->_firstch[j] =
					min(curscr->_firstch[j], first);
			else
				curscr->_firstch[j] = first;

			curscr->_lastch[j] = max(curscr->_lastch[j], last);

			win->_firstch[i] = _NO_CHANGE;	/* updated now */
		}
		win->_lastch[i] = _NO_CHANGE;		/* updated now */
	}

	if (win->_clear)
		win->_clear = FALSE;

	if (!win->_leaveit)
	{
		curscr->_cury = win->_cury + begy;
		curscr->_curx = win->_curx + begx;
	}

	return OK;
}

int doupdate(void)
{
	int i;

	PDC_LOG(("doupdate() - called\n"));

	if (isendwin())			/* coming back after endwin() called */
	{
		reset_prog_mode();
		curscr->_clear = TRUE;
		SP->alive = TRUE;	/* so isendwin() result is correct */
	}

	if (SP->shell)
		reset_prog_mode();

	if (curscr == (WINDOW *)NULL)
		return ERR;

	if (curscr->_clear)
		PDC_clr_update();
	else
		for (i = 0; i < SP->lines; i++)
		{
		    PDC_LOG(("doupdate() - Transforming line %d of %d: %s\n",
			i, SP->lines, (curscr->_firstch[i] != _NO_CHANGE) ?
			"Yes" : "No"));

		    if ((curscr->_firstch[i] != _NO_CHANGE) &&
			 PDC_transform_line(i))		/* if test new */
				break;
		}

#ifdef XCURSES
	XCursesInstructAndWait(CURSES_REFRESH);
#endif
	if (SP->cursrow != curscr->_cury || SP->curscol != curscr->_curx)
	{
		PDC_gotoxy(curscr->_cury, curscr->_curx);
		SP->cursrow = curscr->_cury;
		SP->curscol = curscr->_curx;
	}

	return OK;
}

int redrawwin(WINDOW *win)
{
	PDC_LOG(("redrawwin() - called: win=%x\n", win));

	if (win == (WINDOW *)NULL)
		return ERR;

	return wredrawln(win, 0, win->_maxy);
}

int wredrawln(WINDOW *win, int start, int num)
{
	int i;

	PDC_LOG(("wredrawln() - called: win=%x start=%d num=%d\n",
		win, start, num));

	if ((win == (WINDOW *)NULL) ||
	    (start > win->_maxy || start + num > win->_maxy))
		return ERR;

	for (i = start; i < start + num; i++)
	{
		win->_firstch[i] = 0;
		win->_lastch[i] = win->_maxx - 1;
	}

	return OK;
}
