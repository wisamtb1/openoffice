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
#include "precompiled_basctl.hxx"

#include <ide_pch.hxx>


#include <basidesh.hrc>
#include <basidesh.hxx>
#include <basobj.hxx>

#include <basicbox.hxx>
#include <iderid.hxx>
#include <iderdll.hxx>
#include <bastypes.hxx>
#include "bastype2.hxx"
#include "basdoc.hxx"

#include "localizationmgr.hxx"
#include "managelang.hxx"
#include "dlgresid.hrc"
#include <editeng/unolingu.hxx>

#include <svtools/langtab.hxx>


using namespace ::com::sun::star;
using namespace ::com::sun::star::lang;
using namespace ::com::sun::star::uno;

SFX_IMPL_TOOLBOX_CONTROL( LibBoxControl, SfxStringItem );

LibBoxControl::LibBoxControl( sal_uInt16 nSlotId, sal_uInt16 nId, ToolBox& rTbx )
	: SfxToolBoxControl( nSlotId, nId, rTbx )
{
}



LibBoxControl::~LibBoxControl()
{
}



void LibBoxControl::StateChanged( sal_uInt16, SfxItemState eState, const SfxPoolItem* pState )
{
	BasicLibBox* pBox = (BasicLibBox*) GetToolBox().GetItemWindow( GetId() );

	DBG_ASSERT( pBox, "Box not found" );
	if ( !pBox )
		return;

	if ( eState != SFX_ITEM_AVAILABLE )
		pBox->Disable();
	else
	{
		pBox->Enable();

		if ( pState->ISA(SfxStringItem) )
			pBox->Update( (const SfxStringItem*)pState );
		else
			pBox->Update( NULL );
	}
}



Window* LibBoxControl::CreateItemWindow( Window *pParent )
{
	return new BasicLibBox( pParent, m_xFrame );
}

//=============================================================================
//= DocListenerBox
//=============================================================================

DocListenerBox::DocListenerBox( Window* pParent )
    :ListBox( pParent, WinBits( WB_BORDER | WB_DROPDOWN ) )
    ,m_aNotifier( *this )
{
}

DocListenerBox::~DocListenerBox()
{
    m_aNotifier.dispose();
}

void DocListenerBox::onDocumentCreated( const ScriptDocument& /*_rDocument*/ )
{
    FillBox();
}

void DocListenerBox::onDocumentOpened( const ScriptDocument& /*_rDocument*/ )
{
    FillBox();
}

void DocListenerBox::onDocumentSave( const ScriptDocument& /*_rDocument*/ )
{
    // not interested in
}

void DocListenerBox::onDocumentSaveDone( const ScriptDocument& /*_rDocument*/ )
{
    // not interested in
}

void DocListenerBox::onDocumentSaveAs( const ScriptDocument& /*_rDocument*/ )
{
    // not interested in
}

void DocListenerBox::onDocumentSaveAsDone( const ScriptDocument& /*_rDocument*/ )
{
    FillBox();
}

void DocListenerBox::onDocumentClosed( const ScriptDocument& /*_rDocument*/ )
{
    FillBox();
}

void DocListenerBox::onDocumentTitleChanged( const ScriptDocument& /*_rDocument*/ )
{
    // not interested in
}

void DocListenerBox::onDocumentModeChanged( const ScriptDocument& /*_rDocument*/ )
{
    // not interested in
}

//=============================================================================
//= BasicLibBox
//=============================================================================
BasicLibBox::BasicLibBox( Window* pParent, const uno::Reference< frame::XFrame >& rFrame ) :
    DocListenerBox( pParent ),
    m_xFrame( rFrame )
{
	FillBox();
	bIgnoreSelect = sal_True;	// Select von 0 noch nicht weiterleiten
	bFillBox = sal_True;
	SelectEntryPos( 0 );
	aCurText = GetEntry( 0 );
	SetSizePixel( Size( 250, 200 ) );
    bIgnoreSelect = sal_False;
}



__EXPORT BasicLibBox::~BasicLibBox()
{
    ClearBox();
}

void __EXPORT BasicLibBox::Update( const SfxStringItem* pItem )
{
	// Immer auf dem laufenden sein...
//	if ( !pItem  || !pItem->GetValue().Len() )
		FillBox();

	if ( pItem )
	{
		aCurText = pItem->GetValue();
		if ( aCurText.Len() == 0 )
			aCurText = String( IDEResId( RID_STR_ALL ) );
	}

	if ( GetSelectEntry() != aCurText )
		SelectEntry( aCurText );
}

void __EXPORT BasicLibBox::ReleaseFocus()
{
	SfxViewShell* pCurSh = SfxViewShell::Current();
	DBG_ASSERT( pCurSh, "Current ViewShell not found!" );

	if ( pCurSh )
	{
		Window* pShellWin = pCurSh->GetWindow();
		if ( !pShellWin )		// sonst werde ich ihn nicht los
			pShellWin = Application::GetDefDialogParent();

		pShellWin->GrabFocus();
	}
}

void BasicLibBox::FillBox()
{
	SetUpdateMode( sal_False );
	bIgnoreSelect = sal_True;

	aCurText = GetSelectEntry();

	SelectEntryPos( 0 );
	ClearBox();

    // create list box entries
    sal_uInt16 nPos = InsertEntry( String( IDEResId( RID_STR_ALL ) ), LISTBOX_APPEND );
    SetEntryData( nPos, new BasicLibEntry( ScriptDocument::getApplicationScriptDocument(), LIBRARY_LOCATION_UNKNOWN, String() ) );
    InsertEntries( ScriptDocument::getApplicationScriptDocument(), LIBRARY_LOCATION_USER );
    InsertEntries( ScriptDocument::getApplicationScriptDocument(), LIBRARY_LOCATION_SHARE );

    ScriptDocuments aDocuments( ScriptDocument::getAllScriptDocuments( ScriptDocument::DocumentsSorted ) );
    for (   ScriptDocuments::const_iterator doc = aDocuments.begin();
            doc != aDocuments.end();
            ++doc
        )
    {
        InsertEntries( *doc, LIBRARY_LOCATION_DOCUMENT );
    }

	SetUpdateMode( sal_True );

	SelectEntry( aCurText );
	if ( !GetSelectEntryCount() )
	{
		SelectEntryPos( GetEntryCount() );	// gibst es nicht => leer?
		aCurText = GetSelectEntry();
	}
	bIgnoreSelect = sal_False;
}

void BasicLibBox::InsertEntries( const ScriptDocument& rDocument, LibraryLocation eLocation )
{
    // get a sorted list of library names
    Sequence< ::rtl::OUString > aLibNames = rDocument.getLibraryNames();
    sal_Int32 nLibCount = aLibNames.getLength();
    const ::rtl::OUString* pLibNames = aLibNames.getConstArray();

    for ( sal_Int32 i = 0 ; i < nLibCount ; ++i )
    {
        String aLibName = pLibNames[ i ];
        if ( eLocation == rDocument.getLibraryLocation( aLibName ) )
        {
            String aName( rDocument.getTitle( eLocation ) );
            String aEntryText( CreateMgrAndLibStr( aName, aLibName ) );
            sal_uInt16 nPos = InsertEntry( aEntryText, LISTBOX_APPEND );
            SetEntryData( nPos, new BasicLibEntry( rDocument, eLocation, aLibName ) );
        }
    }
}

long BasicLibBox::PreNotify( NotifyEvent& rNEvt )
{
	long nDone = 0;
	if( rNEvt.GetType() == EVENT_KEYINPUT )
	{
		KeyEvent aKeyEvt = *rNEvt.GetKeyEvent();
		sal_uInt16 nKeyCode = aKeyEvt.GetKeyCode().GetCode();
		switch( nKeyCode )
		{
			case KEY_RETURN:
			{
				NotifyIDE();
				nDone = 1;
			}
			break;

			case KEY_ESCAPE:
			{
				SelectEntry( aCurText );
				ReleaseFocus();
				nDone = 1;
			}
			break;
		}
	}
	else if( rNEvt.GetType() == EVENT_GETFOCUS )
	{
		if ( bFillBox )
		{
			FillBox();
			bFillBox = sal_False;
		}
	}
	else if( rNEvt.GetType() == EVENT_LOSEFOCUS )
	{
		if ( !HasChildPathFocus( sal_True ) )
		{
			bIgnoreSelect = sal_True;
			bFillBox = sal_True;
		}
	}

	return nDone ? nDone : ListBox::PreNotify( rNEvt );
}

void __EXPORT BasicLibBox::Select()
{
	if ( !IsTravelSelect() )
	{
		if ( !bIgnoreSelect )
			NotifyIDE();
		else
			SelectEntry( aCurText );	// Seit 306... (Select nach Escape)
	}
}

void BasicLibBox::NotifyIDE()
{
    sal_uInt16 nSelPos = GetSelectEntryPos();
    BasicLibEntry* pEntry = (BasicLibEntry*)GetEntryData( nSelPos );
    if ( pEntry )
    {
        ScriptDocument aDocument( pEntry->GetDocument() );
        SfxUsrAnyItem aDocumentItem( SID_BASICIDE_ARG_DOCUMENT_MODEL, uno::makeAny( aDocument.getDocumentOrNull() ) );
        String aLibName = pEntry->GetLibName();
        SfxStringItem aLibNameItem( SID_BASICIDE_ARG_LIBNAME, aLibName );
        BasicIDEShell* pIDEShell = IDE_DLL()->GetShell();
        SfxViewFrame* pViewFrame = pIDEShell ? pIDEShell->GetViewFrame() : NULL;
        SfxDispatcher* pDispatcher = pViewFrame ? pViewFrame->GetDispatcher() : NULL;
        if ( pDispatcher )
        {
            pDispatcher->Execute( SID_BASICIDE_LIBSELECTED,
                                  SFX_CALLMODE_SYNCHRON, &aDocumentItem, &aLibNameItem, 0L );
        }
    }
    ReleaseFocus();
}

void BasicLibBox::ClearBox()
{
    sal_uInt16 nCount = GetEntryCount();
    for ( sal_uInt16 i = 0; i < nCount; ++i )
    {
        BasicLibEntry* pEntry = (BasicLibEntry*)GetEntryData( i );
        delete pEntry;
    }
    ListBox::Clear();
}

// class LanguageBoxControl ----------------------------------------------

SFX_IMPL_TOOLBOX_CONTROL( LanguageBoxControl, SfxStringItem );

LanguageBoxControl::LanguageBoxControl( sal_uInt16 nSlotId, sal_uInt16 nId, ToolBox& rTbx )
	: SfxToolBoxControl( nSlotId, nId, rTbx )
{
}

LanguageBoxControl::~LanguageBoxControl()
{
}

void LanguageBoxControl::StateChanged( sal_uInt16 _nID, SfxItemState _eState, const SfxPoolItem* _pItem )
{
	(void)_nID;

	BasicLanguageBox* pBox = (BasicLanguageBox*)( GetToolBox().GetItemWindow( GetId() ) );

	if ( pBox )
	{
		if ( _eState != SFX_ITEM_AVAILABLE )
			pBox->Disable();
		else
		{
			pBox->Enable();
			if ( _pItem->ISA(SfxStringItem) )
				pBox->Update( (const SfxStringItem*)_pItem );
			else
				pBox->Update( NULL );
		}
	}
}

Window* LanguageBoxControl::CreateItemWindow( Window *pParent )
{
	return new BasicLanguageBox( pParent );
}

// class BasicLanguageBox ------------------------------------------------

BasicLanguageBox::BasicLanguageBox( Window* pParent ) :

	DocListenerBox( pParent ),

	m_sNotLocalizedStr( IDEResId( RID_STR_TRANSLATION_NOTLOCALIZED ) ),
	m_sDefaultLanguageStr( IDEResId( RID_STR_TRANSLATION_DEFAULT ) ),

	m_bIgnoreSelect( false )

{
	SetSizePixel( Size( 210, 200 ) );

	FillBox();
}

BasicLanguageBox::~BasicLanguageBox()
{
	ClearBox();
}

void BasicLanguageBox::FillBox()
{
	SetUpdateMode( sal_False );
	m_bIgnoreSelect = true;
	m_sCurrentText = GetSelectEntry();
	ClearBox();

	LocalizationMgr* pCurMgr = IDE_DLL()->GetShell()->GetCurLocalizationMgr();
	if ( pCurMgr->isLibraryLocalized() )
	{
		Enable();
		SvtLanguageTable aLangTable;
		Locale aDefaultLocale = pCurMgr->getStringResourceManager()->getDefaultLocale();
		Locale aCurrentLocale = pCurMgr->getStringResourceManager()->getCurrentLocale();
		Sequence< Locale > aLocaleSeq = pCurMgr->getStringResourceManager()->getLocales();
		const Locale* pLocale = aLocaleSeq.getConstArray();
		sal_Int32 i, nCount = aLocaleSeq.getLength();
		sal_uInt16 nSelPos = LISTBOX_ENTRY_NOTFOUND;
		for ( i = 0;  i < nCount;  ++i )
		{
			bool bIsDefault = localesAreEqual( aDefaultLocale, pLocale[i] );
			bool bIsCurrent = localesAreEqual( aCurrentLocale, pLocale[i] );
			LanguageType eLangType = SvxLocaleToLanguage( pLocale[i] );
			String sLanguage = aLangTable.GetString( eLangType );
			if ( bIsDefault )
			{
				sLanguage += ' ';
				sLanguage += m_sDefaultLanguageStr;
			}
			sal_uInt16 nPos = InsertEntry( sLanguage );
			SetEntryData( nPos, new LanguageEntry( sLanguage, pLocale[i], bIsDefault ) );

			if ( bIsCurrent )
				nSelPos = nPos;
		}

		if ( nSelPos != LISTBOX_ENTRY_NOTFOUND )
		{
			SelectEntryPos( nSelPos );
			m_sCurrentText = GetSelectEntry();
		}
	}
	else
	{
		InsertEntry( m_sNotLocalizedStr );
		SelectEntryPos(0);
		Disable();
	}

	SetUpdateMode( sal_True );
	m_bIgnoreSelect = false;
}

void BasicLanguageBox::ClearBox()
{
    sal_uInt16 nCount = GetEntryCount();
    for ( sal_uInt16 i = 0; i < nCount; ++i )
    {
        LanguageEntry* pEntry = (LanguageEntry*)GetEntryData(i);
        delete pEntry;
    }
    ListBox::Clear();
}

void BasicLanguageBox::SetLanguage()
{
    LanguageEntry* pEntry = (LanguageEntry*)GetEntryData( GetSelectEntryPos() );
	if ( pEntry )
		IDE_DLL()->GetShell()->GetCurLocalizationMgr()->handleSetCurrentLocale( pEntry->m_aLocale );
}

void BasicLanguageBox::Select()
{
	if ( !m_bIgnoreSelect )
		SetLanguage();
	else
		SelectEntry( m_sCurrentText );	// Select after Escape
}

long BasicLanguageBox::PreNotify( NotifyEvent& rNEvt )
{
	long nDone = 0;
	if( rNEvt.GetType() == EVENT_KEYINPUT )
	{
		sal_uInt16 nKeyCode = rNEvt.GetKeyEvent()->GetKeyCode().GetCode();
		switch( nKeyCode )
		{
			case KEY_RETURN:
			{
				SetLanguage();
				nDone = 1;
			}
			break;

			case KEY_ESCAPE:
			{
				SelectEntry( m_sCurrentText );
				nDone = 1;
			}
			break;
		}
	}
	else if( rNEvt.GetType() == EVENT_GETFOCUS )
	{
	}
	else if( rNEvt.GetType() == EVENT_LOSEFOCUS )
	{
	}

	return nDone ? nDone : ListBox::PreNotify( rNEvt );
}

void BasicLanguageBox::Update( const SfxStringItem* pItem )
{
	FillBox();

	if ( pItem && pItem->GetValue().Len() > 0 )
	{
		m_sCurrentText = pItem->GetValue();
		if ( GetSelectEntry() != m_sCurrentText )
			SelectEntry( m_sCurrentText );
	}
}

