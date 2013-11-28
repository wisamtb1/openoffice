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


#ifndef SW_VIEWIMP_HXX
#define SW_VIEWIMP_HXX

#include <vector>

#include <vcl/timer.hxx>
#include <tools/color.hxx>

// OD 25.06.2003 #108784#
#include <svx/svdtypes.hxx>

#include <tools/string.hxx>

#include <swtypes.hxx>
#include <swrect.hxx>

class ViewShell;
class SwFlyFrm;
class SwViewOption;
class SwRegionRects;
class SwFrm;
class SwLayAction;
class SwLayIdle;
class SwDrawView;
class SdrPageView;
class SwPageFrm;
class SwRegionRects;
struct SdrPaintProcRec;
class SwAccessibleMap;
class SdrObject;
class Fraction;
class SwPrintData;
class SwPagePreviewLayout;
struct PrevwPage;
class SwTxtFrm;
// --> OD #i76669#
namespace sdr { namespace contact {
        class ViewObjectContactRedirector;
} }
// <--

class SwViewImp
{
	friend class ViewShell;

	friend class SwLayAction;	//Lay- und IdleAction tragen sich ein und aus.
	friend class SwLayIdle;

    // OD 12.12.2002 #103492# - for paint of page preview
    friend class SwPagePreviewLayout;

	ViewShell *pSh;				//Falls jemand einen Imp durchreicht und doch
								//mal eine ViewShell braucht hier die
								//Rueckwaertsverkettung.

	SwDrawView  *pDrawView;		//Unsere DrawView
	SdrPageView *pSdrPageView;	//Genau eine Seite fuer unsere DrawView

	SwPageFrm	  *pFirstVisPage;//Zeigt immer auf die erste sichtbare Seite.
    SwRegionRects *pRegion;      //Sammler fuer Paintrects aus der LayAction.

	SwLayAction	  *pLayAct;		 //Ist gesetzt wenn ein Action-Objekt existiert
								 //Wird vom SwLayAction-CTor ein- und vom DTor
								 //ausgetragen.
	SwLayIdle	  *pIdleAct;	 //Analog zur SwLayAction fuer SwLayIdle.

	SwAccessibleMap	*pAccMap;		// Accessible Wrappers

    mutable const SdrObject * pSdrObjCached;
    mutable String sSdrObjCachedComment;

	sal_Bool bFirstPageInvalid	:1;	//Pointer auf erste Seite ungueltig?

	//sal_Bool bResetXorVisibility:1; //StartAction/EndAction
	//HMHBOOL bShowHdlPaint		:1; //LockPaint/UnlockPaint
	sal_Bool bResetHdlHiddenPaint:1;//  -- "" --

	sal_Bool bSmoothUpdate		:1;	//Meber fuer SmoothScroll
	sal_Bool bStopSmooth		:1;
	sal_Bool bStopPrt           :1; // Stop Printing

    sal_uInt16 nRestoreActions  ; //Die Anzahl der zu restaurierenden Actions (UNO)
	SwRect aSmoothRect;

    // OD 12.12.2002 #103492#
    SwPagePreviewLayout* mpPgPrevwLayout;

    /**
		Signal whether to stop printing.

		@param _useless just to fit macro
	*/
	DECL_LINK(SetStopPrt, void * _useless = NULL);

	/**
	   Returns if printer shall be stopped.

	   @retval sal_True The printer shall be stopped.
	   @retval sal_False else
	*/
	sal_Bool IsStopPrt() { return bStopPrt; }

	/**
	   Resets signal for stopping printing.

	*/
	void ResetStopPrt() { bStopPrt = sal_False; }

	void SetFirstVisPage();		//Neue Ermittlung der ersten sichtbaren Seite

	void StartAction();			//Henkel Anzeigen und verstecken.
	void EndAction();			//gerufen von ViewShell::ImplXXXAction
	void LockPaint();			//dito, gerufen von ViewShell::ImplLockPaint
	void UnlockPaint();

private:

	SwAccessibleMap *CreateAccessibleMap();

    /** invalidate CONTENT_FLOWS_FROM/_TO relation for paragraphs

        OD 2005-12-01 #i27138#
        implementation for wrapper method
        <ViewShell::InvalidateAccessibleParaFlowRelation(..)>

        @author OD

        @param _pFromTxtFrm
        input parameter - paragraph frame, for which the relation CONTENT_FLOWS_FROM
        has to be invalidated.
        If NULL, no CONTENT_FLOWS_FROM relation has to be invalidated

        @param _pToTxtFrm
        input parameter - paragraph frame, for which the relation CONTENT_FLOWS_TO
        has to be invalidated.
        If NULL, no CONTENT_FLOWS_TO relation has to be invalidated
    */
    void _InvalidateAccessibleParaFlowRelation( const SwTxtFrm* _pFromTxtFrm,
                                                const SwTxtFrm* _pToTxtFrm );

    /** invalidate text selection for paragraphs

        OD 2005-12-12 #i27301#
        implementation for wrapper method
        <ViewShell::InvalidateAccessibleParaTextSelection(..)>

        @author OD
    */
    void _InvalidateAccessibleParaTextSelection();

    /** invalidate attributes for paragraphs and paragraph's characters

        OD 2009-01-06 #i88069#
        implementation for wrapper method
        <ViewShell::InvalidateAccessibleParaAttrs(..)>

        @author OD
    */
    void _InvalidateAccessibleParaAttrs( const SwTxtFrm& rTxtFrm );

public:
    SwViewImp( ViewShell * );
	~SwViewImp();
	void Init( const SwViewOption * );			//nur fuer ViewShell::Init()

	const ViewShell *GetShell() const { return pSh; }
		  ViewShell *GetShell()		  { return pSh; }

	Color GetRetoucheColor() const;

	//Verwaltung zur ersten sichtbaren Seite
	inline const SwPageFrm *GetFirstVisPage() const;
	inline		 SwPageFrm *GetFirstVisPage();
	void SetFirstVisPageInvalid() { bFirstPageInvalid = sal_True; }

	sal_Bool AddPaintRect( const SwRect &rRect );
	SwRegionRects *GetRegion()		{ return pRegion; }
    void DelRegion();

	// neues Interface fuer StarView Drawing
	inline sal_Bool HasDrawView()       const { return 0 != pDrawView; }
		  SwDrawView* GetDrawView() 	  { return pDrawView; }
	const SwDrawView* GetDrawView() const { return pDrawView; }
		  SdrPageView*GetPageView() 	  { return pSdrPageView; }
	const SdrPageView*GetPageView() const { return pSdrPageView; }
	void MakeDrawView();

    // OD 29.08.2002 #102450#
    // add 3rd parameter <const Color* pPageBackgrdColor> for setting this
    // color as the background color at the outliner of the draw view
    // for painting layers <hell> and <heaven>
    // OD 09.12.2002 #103045# - add 4th parameter for the horizontal text
    // direction of the page in order to set the default horizontal text
    // direction at the outliner of the draw view for painting layers <hell>
    // and <heaven>.
    // OD 25.06.2003 #108784# - correct type of 1st parameter
    // OD #i76669# - added parameter <pRedirector>
    void   PaintLayer( const SdrLayerID _nLayerID,
                       SwPrintData const*const pPrintData,
                       const SwRect& _rRect,
                       const Color* _pPageBackgrdColor = 0,
                       const bool _bIsPageRightToLeft = false,
                       sdr::contact::ViewObjectContactRedirector* pRedirector = 0 ) const;

	//wird als Link an die DrawEngine uebergeben, entscheidet was wie
	//gepaintet wird oder nicht.
	//#110094#-3
    //DECL_LINK( PaintDispatcher, SdrPaintProcRec * );

	// Interface Drawing
	sal_Bool IsDragPossible( const Point &rPoint );
	void NotifySizeChg( const Size &rNewSz );

	//SS Fuer die Lay- bzw. IdleAction und verwandtes
	sal_Bool  IsAction() const 					 { return pLayAct  != 0; }
	sal_Bool  IsIdleAction() const				 { return pIdleAct != 0; }
		  SwLayAction &GetLayAction()		 { return *pLayAct; }
	const SwLayAction &GetLayAction() const  { return *pLayAct; }
		  SwLayIdle	  &GetIdleAction()		 { return *pIdleAct;}
	const SwLayIdle   &GetIdleAction() const { return *pIdleAct;}

	//Wenn eine Aktion laueft wird diese gebeten zu pruefen ob es
	//an der zeit ist den WaitCrsr einzuschalten.
	void CheckWaitCrsr();
	sal_Bool IsCalcLayoutProgress() const;	//Fragt die LayAction wenn vorhanden.
	//sal_True wenn eine LayAction laeuft, dort wird dann auch das Flag fuer
	//ExpressionFields gesetzt.
	sal_Bool IsUpdateExpFlds();

	void	SetRestoreActions(sal_uInt16 nSet){nRestoreActions = nSet;}
	sal_uInt16 	GetRestoreActions() const{return nRestoreActions;}

    // OD 12.12.2002 #103492#
    void InitPagePreviewLayout();

    // OD 12.12.2002 #103492#
    inline SwPagePreviewLayout* PagePreviewLayout()
    {
        return mpPgPrevwLayout;
    }

	// Is this view accessible?
	sal_Bool IsAccessible() const { return pAccMap != 0; }

	inline SwAccessibleMap& GetAccessibleMap();

	// Update (this) accessible view
	void UpdateAccessible();

	// Remove a frame from the accessible view
	void DisposeAccessible( const SwFrm *pFrm, const SdrObject *pObj,
							sal_Bool bRecursive );
	inline void DisposeAccessibleFrm( const SwFrm *pFrm,
							   sal_Bool bRecursive=sal_False );
	inline void DisposeAccessibleObj( const SdrObject *pObj );

	// Move a frame's position in the accessible view
	void MoveAccessible( const SwFrm *pFrm, const SdrObject *pObj,
						 const SwRect& rOldFrm );
	inline void MoveAccessibleFrm( const SwFrm *pFrm, const SwRect& rOldFrm );

	// Add a frame in the accessible view
	inline void AddAccessibleFrm( const SwFrm *pFrm );

	inline void AddAccessibleObj( const SdrObject *pObj );

	void FirePageChangeEvent(sal_uInt16 nOldPage, sal_uInt16 nNewPage);
	void FireSectionChangeEvent(sal_uInt16 nOldSection, sal_uInt16 nNewSection);
	void FireColumnChangeEvent(sal_uInt16 nOldColumn, sal_uInt16 nNewColumn);

	// Invalidate accessible frame's frame's content
	void InvalidateAccessibleFrmContent( const SwFrm *pFrm );

	// Invalidate accessible frame's cursor position
	void InvalidateAccessibleCursorPosition( const SwFrm *pFrm );

	// Invalidate editable state for all accessible frames
	void InvalidateAccessibleEditableState( sal_Bool bAllShells=sal_True,
		   									const SwFrm *pFrm=0 );

	// Invalidate frame's relation set (for chained frames)
	void InvalidateAccessibleRelationSet( const SwFlyFrm *pMaster,
                                          const SwFlyFrm *pFollow );

    // update data for accessible preview
    // OD 15.01.2003 #103492# - change method signature due to new page preview
    // functionality
    void UpdateAccessiblePreview( const std::vector<PrevwPage*>& _rPrevwPages,
                                  const Fraction&  _rScale,
                                  const SwPageFrm* _pSelectedPageFrm,
                                  const Size&      _rPrevwWinSize );

    void InvalidateAccessiblePreViewSelection( sal_uInt16 nSelPage );

	// Fire all accessible events that have been collected so far
	void FireAccessibleEvents();
};

inline SwPageFrm *SwViewImp::GetFirstVisPage()
{
	if ( bFirstPageInvalid )
		SetFirstVisPage();
	return pFirstVisPage;
}

inline const SwPageFrm *SwViewImp::GetFirstVisPage() const
{
	if ( bFirstPageInvalid )
		((SwViewImp*)this)->SetFirstVisPage();
	return pFirstVisPage;
}

inline SwAccessibleMap& SwViewImp::GetAccessibleMap()
{
	if( !pAccMap )
		CreateAccessibleMap();

	return *pAccMap;
}

inline void SwViewImp::DisposeAccessibleFrm( const SwFrm *pFrm,
							   sal_Bool bRecursive )
{
	DisposeAccessible( pFrm, 0, bRecursive );
}

inline void SwViewImp::DisposeAccessibleObj( const SdrObject *pObj )
{
	DisposeAccessible( 0, pObj, sal_False );
}

inline void SwViewImp::MoveAccessibleFrm( const SwFrm *pFrm,
										  const SwRect& rOldFrm )
{
	MoveAccessible( pFrm, 0, rOldFrm );
}

inline void SwViewImp::AddAccessibleFrm( const SwFrm *pFrm )
{
	SwRect aEmptyRect;
	MoveAccessible( pFrm, 0, aEmptyRect );
}

inline void SwViewImp::AddAccessibleObj( const SdrObject *pObj )
{
	SwRect aEmptyRect;
	MoveAccessible( 0, pObj, aEmptyRect );
}
#endif // SW_VIEWIMP_HXX

