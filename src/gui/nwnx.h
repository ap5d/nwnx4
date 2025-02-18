/***************************************************************************
    NWNX GUI - This class defines the wxWidgets application
    Copyright (C) 2007 Ingmar Stieger (Papillon, papillon@nwnx.org)

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

***************************************************************************/

#ifndef NWNX_H
#define NWNX_H


enum
{ GUI_EXIT	= wxID_HIGHEST + 1,
  GUI_OPEN,
  GUI_CONNECTIONS,
  GUI_ABOUT,
};

#define LABELSIZE 100

class NWNXGUI : public wxApp
{
  public:
	virtual bool OnInit();

  private:
	wxLocale m_locale;
};

DECLARE_APP(NWNXGUI)

#endif

