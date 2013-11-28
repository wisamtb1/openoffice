/**************************************************************
 * 
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 * 
 *   http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 * 
 *************************************************************/



// MARKER(update_precomp.py): autogen include statement, do not remove
#include "precompiled_cui.hxx"

#include <linkdlg.hxx>
#include <vcl/svapp.hxx>
#include "helpid.hrc"

#include <tools/urlobj.hxx>
#include <svtools/svmedit.hxx>
#include <svtools/filedlg.hxx>
#include <vcl/dialog.hxx>
#include <vcl/button.hxx>
#include <vcl/fixed.hxx>
#include <vcl/group.hxx>
#include <vcl/lstbox.hxx>
#include <vcl/msgbox.hxx>
#include <vcl/timer.hxx>
#include <svtools/svtabbx.hxx>

#include <svuidlg.hrc>
#include <sfx2/linkmgr.hxx>
#include <sfx2/linksrc.hxx>
#include <svtools/soerr.hxx>
#include <sfx2/lnkbase.hxx>
#include <sfx2/objsh.hxx>

#include <dialmgr.hxx>

#define _SVSTDARR_USHORTS
#include <svl/svstdarr.hxx>

#define MAX_FILENAME	18
#define MAX_LINKNAME	18
#define MAX_TYPENAME	15
#define MAX_UPDATENAME	10

#define FILEOBJECT ( OBJECT_CLIENT_FILE & ~OBJECT_CLIENT_SO )

using namespace sfx2;

SV_DECL_IMPL_REF_LIST(SvBaseLink,SvBaseLink*)

// Achtung im Code wird dieses Array direkt (0, 1, ...) indiziert
static long nTabs[] =
	{	4, // Number of Tabs
		0, 77, 144, 209
	};


SvBaseLinksDlg::SvBaseLinksDlg( Window * pParent, LinkManager* pMgr, sal_Bool bHtml )
    : ModalDialog( pParent, CUI_RES( MD_UPDATE_BASELINKS ) ),
    aFtFiles( this, CUI_RES( FT_FILES ) ),
    aFtLinks( this, CUI_RES( FT_LINKS ) ),
    aFtType( this, CUI_RES( FT_TYPE ) ),
    aFtStatus( this, CUI_RES( FT_STATUS ) ),
    aTbLinks( this, CUI_RES(TB_LINKS ) ),
    aFtFiles2( this, CUI_RES( FT_FILES2 ) ),
    aFtFullFileName( this, CUI_RES( FT_FULL_FILE_NAME ) ),
    aFtSource2( this, CUI_RES( FT_SOURCE2 ) ),
    aFtFullSourceName( this, CUI_RES( FT_FULL_SOURCE_NAME ) ),
    aFtType2( this, CUI_RES( FT_TYPE2 ) ),
    aFtFullTypeName( this, CUI_RES( FT_FULL_TYPE_NAME ) ),
    aFtUpdate( this, CUI_RES( FT_UPDATE ) ),
    aRbAutomatic( this, CUI_RES( RB_AUTOMATIC ) ),
    aRbManual( this, CUI_RES( RB_MANUAL ) ),
    aCancelButton1( this, CUI_RES( 1 ) ),
    aHelpButton1( this, CUI_RES( 1 ) ),
    aPbUpdateNow( this, CUI_RES( PB_UPDATE_NOW ) ),
    aPbOpenSource( this, CUI_RES( PB_OPEN_SOURCE ) ),
    aPbChangeSource( this, CUI_RES( PB_CHANGE_SOURCE ) ),
    aPbBreakLink( this, CUI_RES( PB_BREAK_LINK ) ),
    aStrAutolink( CUI_RES( STR_AUTOLINK ) ),
    aStrManuallink( CUI_RES( STR_MANUALLINK ) ),
    aStrBrokenlink( CUI_RES( STR_BROKENLINK ) ),
    aStrGraphiclink( CUI_RES( STR_GRAPHICLINK ) ),
    aStrButtonclose( CUI_RES( STR_BUTTONCLOSE ) ),
    aStrCloselinkmsg( CUI_RES( STR_CLOSELINKMSG ) ),
    aStrCloselinkmsgMulti( CUI_RES( STR_CLOSELINKMSG_MULTI ) ),
    aStrWaitinglink( CUI_RES( STR_WAITINGLINK ) ),
    pLinkMgr( NULL ),
	bHtmlMode(bHtml)
{
    FreeResource();

    aTbLinks.SetHelpId(HID_LINKDLG_TABLB);
    aTbLinks.SetSelectionMode( MULTIPLE_SELECTION );
    aTbLinks.SetTabs( &nTabs[0], MAP_APPFONT );
    aTbLinks.Resize();  // OS: Hack fuer richtige Selektion

    //JP 24.02.99: UpdateTimer fuer DDE-/Grf-Links, auf die gewarted wird
    aUpdateTimer.SetTimeoutHdl( LINK( this, SvBaseLinksDlg, UpdateWaitingHdl ) );
    aUpdateTimer.SetTimeout( 1000 );
	// Set the ZOrder, and accessible name to the dialog's title	
	aTbLinks.SetZOrder(0, WINDOW_ZORDER_FIRST);
	aTbLinks.SetAccessibleName(this->GetText());
	aTbLinks.SetAccessibleRelationLabeledBy(&aFtFiles);

    OpenSource().Hide();

    Links().SetSelectHdl( LINK( this, SvBaseLinksDlg, LinksSelectHdl ) );
    Links().SetDoubleClickHdl( LINK( this, SvBaseLinksDlg, LinksDoubleClickHdl ) );
    Automatic().SetClickHdl( LINK( this, SvBaseLinksDlg, AutomaticClickHdl ) );
    Manual().SetClickHdl( LINK( this, SvBaseLinksDlg, ManualClickHdl ) );
    UpdateNow().SetClickHdl( LINK( this, SvBaseLinksDlg, UpdateNowClickHdl ) );
//  OpenSource().SetClickHdl( LINK( this, SvBaseLinksDlg, OpenSourceClickHdl ) );
    ChangeSource().SetClickHdl( LINK( this, SvBaseLinksDlg, ChangeSourceClickHdl ) );
	if(!bHtmlMode)
        BreakLink().SetClickHdl( LINK( this, SvBaseLinksDlg, BreakLinkClickHdl ) );
	else
        BreakLink().Hide();

	SetManager( pMgr );
}

SvBaseLinksDlg::~SvBaseLinksDlg()
{
}

/*************************************************************************
|*    SvBaseLinksDlg::Handler()
|*
|*	  Beschreibung
|*	  Ersterstellung	MM 14.06.94
|*	  Letzte Aenderung	JP 30.05.95
*************************************************************************/
IMPL_LINK( SvBaseLinksDlg, LinksSelectHdl, SvTabListBox *, pSvTabListBox )
{
	sal_uInt16 nSelectionCount = pSvTabListBox ?
		(sal_uInt16)pSvTabListBox->GetSelectionCount() : 0;
	if(nSelectionCount > 1)
	{
		//bei Mehrfachselektion ggf. alte Eintraege deselektieren
		SvLBoxEntry* pEntry = 0;
		SvBaseLink* pLink = 0;
		pEntry = pSvTabListBox->GetHdlEntry();
		pLink = (SvBaseLink*)pEntry->GetUserData();
		sal_uInt16 nObjectType = pLink->GetObjType();
		if((OBJECT_CLIENT_FILE & nObjectType) != OBJECT_CLIENT_FILE)
		{
			pSvTabListBox->SelectAll(sal_False);
			pSvTabListBox->Select(pEntry);
			nSelectionCount = 1;
		}
		else
		{
			for( sal_uInt16 i = 0; i < nSelectionCount; i++)
			{
				pEntry = i == 0 ? pSvTabListBox->FirstSelected() :
									pSvTabListBox->NextSelected(pEntry);
				DBG_ASSERT(pEntry, "Wo ist der Entry?");
				pLink = (SvBaseLink*)pEntry->GetUserData();
				DBG_ASSERT(pLink, "Wo ist der Link?");
				if( (OBJECT_CLIENT_FILE & pLink->GetObjType()) != OBJECT_CLIENT_FILE )
					pSvTabListBox->Select( pEntry, sal_False );

			}
		}

        UpdateNow().Enable();

        Automatic().Disable();
        Manual().Check();
        Manual().Disable();
	}
	else
	{
		sal_uInt16 nPos;
		SvBaseLink* pLink = GetSelEntry( &nPos );
		if( !pLink )
			return 0;

        UpdateNow().Enable();

		String sType, sLink;
		String *pLinkNm = &sLink, *pFilter = 0;

		if( FILEOBJECT & pLink->GetObjType() )
		{
            Automatic().Disable();
            Manual().Check();
            Manual().Disable();
			if( OBJECT_CLIENT_GRF == pLink->GetObjType() )
				pLinkNm = 0, pFilter = &sLink;
		}
		else
		{
            Automatic().Enable();
            Manual().Enable();

			if( LINKUPDATE_ALWAYS == pLink->GetUpdateMode() )
                Automatic().Check();
			else
                Manual().Check();
		}

		String aFileName;
		pLinkMgr->GetDisplayNames( pLink, &sType, &aFileName, pLinkNm, pFilter );
        aFileName = INetURLObject::decode(aFileName, INET_HEX_ESCAPE, INetURLObject::DECODE_UNAMBIGUOUS);
        FileName().SetText( aFileName );
        SourceName().SetText( sLink );
        TypeName().SetText( sType );
	}
	return 0;
}

IMPL_LINK_INLINE_START( SvBaseLinksDlg, LinksDoubleClickHdl, SvTabListBox *, pSvTabListBox )
{
	(void)pSvTabListBox;

	ChangeSourceClickHdl( 0 );
	return 0;
}
IMPL_LINK_INLINE_END( SvBaseLinksDlg, LinksDoubleClickHdl, SvTabListBox *, pSvTabListBox )

IMPL_LINK_INLINE_START( SvBaseLinksDlg, AutomaticClickHdl, RadioButton *, pRadioButton )
{
	(void)pRadioButton;

	sal_uInt16 nPos;
	SvBaseLink* pLink = GetSelEntry( &nPos );
	if( pLink && !( FILEOBJECT & pLink->GetObjType() ) &&
		LINKUPDATE_ALWAYS != pLink->GetUpdateMode() )
		SetType( *pLink, nPos, LINKUPDATE_ALWAYS );
	return 0;
}
IMPL_LINK_INLINE_END( SvBaseLinksDlg, AutomaticClickHdl, RadioButton *, pRadioButton )

IMPL_LINK_INLINE_START( SvBaseLinksDlg, ManualClickHdl, RadioButton *, pRadioButton )
{
	(void)pRadioButton;

	sal_uInt16 nPos;
	SvBaseLink* pLink = GetSelEntry( &nPos );
	if( pLink && !( FILEOBJECT & pLink->GetObjType() ) &&
		LINKUPDATE_ONCALL != pLink->GetUpdateMode())
		SetType( *pLink, nPos, LINKUPDATE_ONCALL );
	return 0;
}
IMPL_LINK_INLINE_END( SvBaseLinksDlg, ManualClickHdl, RadioButton *, pRadioButton )

IMPL_LINK( SvBaseLinksDlg, UpdateNowClickHdl, PushButton *, EMPTYARG )
{
    SvTabListBox& rListBox = Links();
	sal_uInt16 nSelCnt = (sal_uInt16)rListBox.GetSelectionCount();
	if( 255 < nSelCnt )
		nSelCnt = 255;

	std::vector< SvBaseLink* > aLnkArr;
	std::vector< sal_uInt16 > aPosArr;

	SvLBoxEntry* pE = rListBox.FirstSelected();
	while( pE )
	{
		sal_uInt16 nFndPos = (sal_uInt16)rListBox.GetModel()->GetAbsPos( pE );
		if( LISTBOX_ENTRY_NOTFOUND != nFndPos )
		{
			aLnkArr.push_back( static_cast< SvBaseLink* >( pE->GetUserData() ) );
			aPosArr.push_back( nFndPos );
		}
		pE = rListBox.NextSelected( pE );
	}

	if( !aLnkArr.empty() )
	{
		for( sal_uInt16 n = 0; n < aLnkArr.size(); ++n )
		{
			SvBaseLinkRef xLink = aLnkArr[ n ];

			// suche erstmal im Array nach dem Eintrag
			for( sal_uInt16 i = 0; i < pLinkMgr->GetLinks().Count(); ++i )
				if( &xLink == *pLinkMgr->GetLinks()[ i ] )
				{
					xLink->SetUseCache( sal_False );
					SetType( *xLink, aPosArr[ n ], xLink->GetUpdateMode() );
					xLink->SetUseCache( sal_True );
					break;
				}
		}

		// falls jemand der Meinung ist, seine Links auszutauschen (SD)
		LinkManager* pNewMgr = pLinkMgr;
		pLinkMgr = 0;
		SetManager( pNewMgr );


		if( 0 == (pE = rListBox.GetEntry( aPosArr[ 0 ] )) ||
			pE->GetUserData() != aLnkArr[ 0 ] )
		{
			// suche mal den Link
			pE = rListBox.First();
			while( pE )
			{
				if( pE->GetUserData() == aLnkArr[ 0 ] )
					break;
				pE = rListBox.Next( pE );
			}

			if( !pE )
				pE = rListBox.FirstSelected();
		}

		if( pE )
		{
			SvLBoxEntry* pSelEntry = rListBox.FirstSelected();
			if( pE != pSelEntry )
				rListBox.Select( pSelEntry, sal_False );
			rListBox.Select( pE );
			rListBox.MakeVisible( pE );
		}
	}
	return 0;
}

/*
IMPL_LINK_INLINE_START( SvBaseLinksDlg, OpenSourceClickHdl, PushButton *, pPushButton )
{
	DBG_ASSERT( !this, "Open noch nicht impl." );
	return 0;
}
IMPL_LINK_INLINE_END( SvBaseLinksDlg, OpenSourceClickHdl, PushButton *, pPushButton )
*/

IMPL_LINK( SvBaseLinksDlg, ChangeSourceClickHdl, PushButton *, pPushButton )
{
	(void)pPushButton;

    sal_uInt16 nSelectionCount = (sal_uInt16)Links().GetSelectionCount();
	if(nSelectionCount > 1)
	{
        PathDialog aPathDlg( this );
		String sType, sFile, sLinkName;
		String  sFilter;
        SvLBoxEntry* pEntry = Links().FirstSelected();
		SvBaseLink* pLink = (SvBaseLink*)pEntry->GetUserData();
		pLinkMgr->GetDisplayNames( pLink, &sType, &sFile, 0, 0 );
		INetURLObject aUrl(sFile);
		if(aUrl.GetProtocol() == INET_PROT_FILE)
		{
			rtl::OUString sOldPath(aUrl.PathToFileName());
			sal_Int32 nLen = aUrl.GetName().getLength();
			sOldPath = sOldPath.copy(0, sOldPath.getLength() - nLen);
			aPathDlg.SetPath(sOldPath);
		}
		if(aPathDlg.Execute() == RET_OK)
		{
			String aPath = aPathDlg.GetPath();

			for( sal_uInt16 i = 0; i < nSelectionCount; i++)
			{
				pEntry = i==0 ?
                        Links().FirstSelected() :
                            Links().NextSelected( pEntry );
				DBG_ASSERT(pEntry,"Wo ist der Entry");
				pLink = (SvBaseLink*)pEntry->GetUserData();
				DBG_ASSERT(pLink,"Wo ist der Link");
				pLinkMgr->GetDisplayNames( pLink, &sType, &sFile, &sLinkName, &sFilter );
				INetURLObject aUrl_(sFile);
				INetURLObject aUrl2(aPath, INET_PROT_FILE);
				aUrl2.insertName( aUrl_.getName() );
				String sNewLinkName;
                MakeLnkName( sNewLinkName, 0 ,
						aUrl2.GetMainURL(INetURLObject::DECODE_TO_IURI), sLinkName, &sFilter);
				pLink->SetLinkSourceName( sNewLinkName );
				pLink->Update();
			}
			if( pLinkMgr->GetPersist() )
				pLinkMgr->GetPersist()->SetModified();
			LinkManager* pNewMgr = pLinkMgr;
			pLinkMgr = 0;
			SetManager( pNewMgr );
		}
	}
	else
	{
		sal_uInt16 nPos;
		SvBaseLink* pLink = GetSelEntry( &nPos );
        if ( pLink && (pLink->GetLinkSourceName().Len() != 0) )
            pLink->Edit( this, LINK( this, SvBaseLinksDlg, EndEditHdl ) );
	}
	return 0;
}

IMPL_LINK( SvBaseLinksDlg, BreakLinkClickHdl, PushButton *, pPushButton )
{
	(void)pPushButton;

	sal_Bool bModified = sal_False;
    if(Links().GetSelectionCount() <= 1)
	{
		sal_uInt16 nPos;
		SvBaseLinkRef xLink = GetSelEntry( &nPos );
		if( !xLink.Is() )
			return 0;

        QueryBox aBox( this, WB_YES_NO | WB_DEF_YES, Closelinkmsg() );

		if( RET_YES == aBox.Execute() )
		{
            Links().GetModel()->Remove( Links().GetEntry( nPos ) );

			// falls Object noch vorhanden, dann das schliessen
			sal_Bool bNewLnkMgr = OBJECT_CLIENT_FILE == xLink->GetObjType();

			// dem Link sagen, das er aufgeloest wird!
			xLink->Closed();

			// falls einer vergessen hat sich auszutragen
			if( xLink.Is() )
				pLinkMgr->Remove( &xLink );

			if( bNewLnkMgr )
			{
				LinkManager* pNewMgr = pLinkMgr;
				pLinkMgr = 0;
				SetManager( pNewMgr );

                SvLBoxEntry* pEntry = Links().GetEntry( nPos ? --nPos : 0 );
				if( pEntry )
                    Links().SetCurEntry( pEntry );
			}
			bModified = sal_True;
		}
	}
	else
	{
        QueryBox aBox( this, WB_YES_NO | WB_DEF_YES, CloselinkmsgMulti() );

		if( RET_YES == aBox.Execute() )
		{

			SvBaseLinkMemberList aLinkList;
            SvLBoxEntry* pEntry = Links().FirstSelected();
			while ( pEntry )
			{
				void * pUD = pEntry->GetUserData();
				if( pUD )
					aLinkList.Append( (SvBaseLink*)pUD );
                pEntry = Links().NextSelected(pEntry);
			}
            Links().RemoveSelection();
			for( sal_uLong i = 0; i < aLinkList.Count(); i++ )
			{
				SvBaseLinkRef xLink = aLinkList.GetObject( i );
				// dem Link sagen, das er aufgeloest wird!
				xLink->Closed();

				// falls einer vergessen hat sich auszutragen
				pLinkMgr->Remove( &xLink );
				bModified = sal_True;
			}
			//Danach alle selektierten Eintraege entfernen
		}
	}
	if(bModified)
	{
        if( !Links().GetEntryCount() )
		{
			// Der letzte macht das Licht aus
            Automatic().Disable();
            Manual().Disable();
            UpdateNow().Disable();
//            OpenSource().Disable();
            ChangeSource().Disable();
            BreakLink().Disable();

			String aEmpty;
            SourceName().SetText( aEmpty );
            TypeName().SetText( aEmpty );
		}
		if( pLinkMgr->GetPersist() )
			pLinkMgr->GetPersist()->SetModified();
	}
	return 0;
}

IMPL_LINK( SvBaseLinksDlg, UpdateWaitingHdl, Timer*, pTimer )
{
	(void)pTimer;
//    for( SvLBoxEntry* pBox = Links().First(); pBox;
//          pBox = Links().Next( pBox ))

    Links().SetUpdateMode(sal_False);
    for( sal_uLong nPos = Links().GetEntryCount(); nPos; )
	{
        SvLBoxEntry* pBox = Links().GetEntry( --nPos );
		SvBaseLinkRef xLink( (SvBaseLink*)pBox->GetUserData() );
		if( xLink.Is() )
		{
			String sCur( ImplGetStateStr( *xLink ) ),
                    sOld( Links().GetEntryText( pBox, 3 ) );
			if( sCur != sOld )
                Links().SetEntryText( sCur, pBox, 3 );
		}
	}
    Links().SetUpdateMode(sal_True);
	return 0;
}

IMPL_LINK( SvBaseLinksDlg, EndEditHdl, sfx2::SvBaseLink*, _pLink )
{
    sal_uInt16 nPos;
    GetSelEntry( &nPos );

    if( _pLink && _pLink->WasLastEditOK() )
    {
        // JP 09.01.98:
        // StarImpress/Draw tauschen die LinkObjecte selbst aus!
        // also suche den Link im Manager, wenn der nicht mehr existiert,
        // dann setze fuelle die Liste komplett neu. Ansonsten braucht
        // nur der editierte Linkt aktualisiert werden.
        sal_Bool bLinkFnd = sal_False;
        for( sal_uInt16 n = pLinkMgr->GetLinks().Count(); n;  )
            if( _pLink == &(*pLinkMgr->GetLinks()[ --n ]) )
            {
                bLinkFnd = sal_True;
                break;
            }

        if( bLinkFnd )
        {
            Links().SetUpdateMode(sal_False);
            Links().GetModel()->Remove( Links().GetEntry( nPos ) );
            SvLBoxEntry* pToUnselect = Links().FirstSelected();
            InsertEntry( *_pLink, nPos, sal_True );
            if(pToUnselect)
                Links().Select(pToUnselect, sal_False);
            Links().SetUpdateMode(sal_True);
        }
        else
        {
            LinkManager* pNewMgr = pLinkMgr;
            pLinkMgr = 0;
            SetManager( pNewMgr );
        }
        if( pLinkMgr->GetPersist() )
            pLinkMgr->GetPersist()->SetModified();
    }
    return 0;
}

String SvBaseLinksDlg::ImplGetStateStr( const SvBaseLink& rLnk )
{
	String sRet;
	if( !rLnk.GetObj() )
        sRet = Brokenlink();
	else if( rLnk.GetObj()->IsPending() )
	{
        sRet = Waitinglink();
        StartUpdateTimer();
	}
	else if( LINKUPDATE_ALWAYS == rLnk.GetUpdateMode() )
        sRet = Autolink();
	else
        sRet = Manuallink();

	return sRet;
}

void SvBaseLinksDlg::SetManager( LinkManager* pNewMgr )
{
	if( pLinkMgr == pNewMgr )
		return;

	if( pNewMgr )
		// Update muss vor Clear gestoppt werden
        Links().SetUpdateMode( sal_False );

    Links().Clear();
	pLinkMgr = pNewMgr;

	if( pLinkMgr )
	{
		SvBaseLinks& rLnks = (SvBaseLinks&)pLinkMgr->GetLinks();
		for( sal_uInt16 n = 0; n < rLnks.Count(); ++n )
		{
			SvBaseLinkRef* pLinkRef = rLnks[ n ];
			if( !pLinkRef->Is() )
			{
				rLnks.Remove( n, 1 );
				--n;
				continue;
			}
			if( (*pLinkRef)->IsVisible() )
				InsertEntry( **pLinkRef );
		}

		if( rLnks.Count() )
		{
            SvLBoxEntry* pEntry = Links().GetEntry( 0 );
            Links().SetCurEntry( pEntry );
            Links().Select( pEntry );
			LinksSelectHdl( 0 );
		}
        Links().SetUpdateMode( sal_True );
        Links().Invalidate();
	}
}


void SvBaseLinksDlg::InsertEntry( const SvBaseLink& rLink, sal_uInt16 nPos, sal_Bool bSelect )
{
	String aEntry, sFileNm, sLinkNm, sTypeNm, sFilter;

	pLinkMgr->GetDisplayNames( (SvBaseLink*)&rLink, &sTypeNm, &sFileNm, &sLinkNm, &sFilter );

	// GetTab(0) gibt die Position der von der TabListBox automatisch eingefuegten
	// Bitmap. Die Breite der ersten Textspalte ergibt sich deshalb aus Tab(2)-Tab(1)
    long nWidthPixel = Links().GetLogicTab( 2 ) - Links().GetLogicTab( 1 );
	nWidthPixel -= SV_TAB_BORDER;
    XubString aTxt = Links().GetEllipsisString( sFileNm, nWidthPixel, TEXT_DRAW_PATHELLIPSIS );
	INetURLObject aPath( sFileNm, INET_PROT_FILE );
	String aFileName = aPath.getName();
    aFileName = INetURLObject::decode(aFileName, INET_HEX_ESCAPE, INetURLObject::DECODE_UNAMBIGUOUS);
    
    if( aFileName.Len() > aTxt.Len() )
		aTxt = aFileName;
	else if( aTxt.Search( aFileName, aTxt.Len() - aFileName.Len() ) == STRING_NOTFOUND )
		// filename not in string
		aTxt = aFileName;

	aEntry = aTxt;
	aEntry += '\t';
	if( OBJECT_CLIENT_GRF == rLink.GetObjType() )
		aEntry += sFilter;
	else
		aEntry += sLinkNm;
	aEntry += '\t';
	aEntry += sTypeNm;
	aEntry += '\t';
	aEntry += ImplGetStateStr( rLink );

    SvLBoxEntry * pE = Links().InsertEntryToColumn( aEntry, nPos );
	pE->SetUserData( (void*)&rLink );
    if(bSelect)
        Links().Select(pE);
}

SvBaseLink* SvBaseLinksDlg::GetSelEntry( sal_uInt16* pPos )
{
    SvLBoxEntry* pE = Links().FirstSelected();
	sal_uInt16 nPos;
	if( pE && LISTBOX_ENTRY_NOTFOUND !=
        ( nPos = (sal_uInt16)Links().GetModel()->GetAbsPos( pE ) ) )
	{
		DBG_ASSERT( pE, "wo kommt der leere Eintrag her?" );

		if( pPos )
			*pPos = nPos;
		return (SvBaseLink*)pE->GetUserData();
	}
	return 0;
}

void SvBaseLinksDlg::SetType( SvBaseLink& rLink,
									sal_uInt16 nSelPos,
									sal_uInt16 nType )
{
	rLink.SetUpdateMode( nType );
	rLink.Update();
    SvLBoxEntry* pBox = Links().GetEntry( nSelPos );
    Links().SetEntryText( ImplGetStateStr( rLink ), pBox, 3 );
	if( pLinkMgr->GetPersist() )
		pLinkMgr->GetPersist()->SetModified();
}

void SvBaseLinksDlg::SetActLink( SvBaseLink * pLink )
{
	if( pLinkMgr )
	{
		const SvBaseLinks& rLnks = pLinkMgr->GetLinks();
        sal_uInt16 nSelect = 0;
        for( sal_uInt16 n = 0; n < rLnks.Count(); ++n )
        {
            SvBaseLinkRef* pLinkRef = rLnks[ n ];
            // #109573# only visible links have been inserted into the TreeListBox,
            // invisible ones have to be skipped here
            if( (*pLinkRef)->IsVisible() )
            {
                if( pLink == *pLinkRef )
                {
                    Links().Select( Links().GetEntry( nSelect ) );
                    LinksSelectHdl( 0 );
                    return ;
                }
                nSelect++;
            }
        }
	}
}

