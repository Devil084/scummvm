/* ScummVM - Graphic Adventure Engine
 *
 * ScummVM is the legal property of its developers, whose names
 * are too numerous to list here. Please refer to the COPYRIGHT
 * file distributed with this source distribution.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */

/*
 * This file is based on WME Lite.
 * http://dead-code.org/redir.php?target=wmelite
 * Copyright (c) 2011 Jan Nedoma
 */

#include "dcgf.h"
#include "BTransitionMgr.h"
#include "BGame.h"
#include "PlatformSDL.h"

namespace WinterMute {

//////////////////////////////////////////////////////////////////////////
CBTransitionMgr::CBTransitionMgr(CBGame *inGame): CBBase(inGame) {
	m_State = TRANS_MGR_READY;
	m_Type = TRANSITION_NONE;
	m_OrigInteractive = false;
	m_PreserveInteractive = false;
	m_LastTime = 0;
	m_Started = false;
}



//////////////////////////////////////////////////////////////////////////
CBTransitionMgr::~CBTransitionMgr() {

}


//////////////////////////////////////////////////////////////////////////
bool CBTransitionMgr::IsReady() {
	return (m_State == TRANS_MGR_READY);
}


//////////////////////////////////////////////////////////////////////////
HRESULT CBTransitionMgr::Start(TTransitionType Type, bool NonInteractive) {
	if (m_State != TRANS_MGR_READY) return S_OK;

	if (Type == TRANSITION_NONE || Type >= NUM_TRANSITION_TYPES) {
		m_State = TRANS_MGR_READY;
		return S_OK;
	}

	if (NonInteractive) {
		m_PreserveInteractive = true;
		m_OrigInteractive = Game->m_Interactive;
		Game->m_Interactive = false;
	} else m_PreserveInteractive;

	m_Type = Type;
	m_State = TRANS_MGR_RUNNING;
	m_Started = false;

	return S_OK;
}

#define FADE_DURATION 200

//////////////////////////////////////////////////////////////////////////
HRESULT CBTransitionMgr::Update() {
	if (IsReady()) return S_OK;

	if (!m_Started) {
		m_Started = true;
		m_LastTime = CBPlatform::GetTime();
	}

	switch (m_Type) {
	case TRANSITION_NONE:
		m_State = TRANS_MGR_READY;
		break;

	case TRANSITION_FADE_OUT: {
		uint32 time = CBPlatform::GetTime() - m_LastTime;
		int Alpha = 255 - (float)time / (float)FADE_DURATION * 255;
		Alpha = MIN(255, MAX(Alpha, 0));
		Game->m_Renderer->Fade((WORD)Alpha);

		if (time > FADE_DURATION) m_State = TRANS_MGR_READY;
	}
	break;

	case TRANSITION_FADE_IN: {
		uint32 time = CBPlatform::GetTime() - m_LastTime;
		int Alpha = (float)time / (float)FADE_DURATION * 255;
		Alpha = MIN(255, MAX(Alpha, 0));
		Game->m_Renderer->Fade((WORD)Alpha);

		if (time > FADE_DURATION) m_State = TRANS_MGR_READY;
	}
	break;

	}

	if (IsReady()) {
		if (m_PreserveInteractive) Game->m_Interactive = m_OrigInteractive;
	}
	return S_OK;
}

} // end of namespace WinterMute
